//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILOPS_TD
#define CIRCT_DIALECT_RTLIL_RTLILOPS_TD

include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/OpBase.td"
include "mlir/IR/OpAsmInterface.td"
include "mlir/IR/RegionKindInterface.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/IR/EnumAttr.td"
include "mlir/IR/BuiltinAttributeInterfaces.td"
include "mlir/IR/BuiltinTypes.td"
include "mlir/IR/CommonAttrConstraints.td"

include "circt/Dialect/RTLIL/RTLILDialect.td"
include "circt/Dialect/RTLIL/RTLILEnums.td"
include "circt/Dialect/RTLIL/RTLILTypes.td"
include "circt/Dialect/RTLIL/RTLILAttributes.td"
include "circt/Dialect/RTLIL/RTLILOpInterfaces.td"

//===----------------------------------------------------------------------===//
// Base class definitions
//===----------------------------------------------------------------------===//

class RTLILOp<string mnemonic, list<Trait> traits = []> :
        Op<RTLILDialect, mnemonic, traits>;


//===----------------------------------------------------------------------===//
// Wire related operations and primitives
//===----------------------------------------------------------------------===//

def ConstOp : MValueExpander, RTLILOp<"const",
  [Pure, AllMatch<
    ["llvm::cast<mlir::TypedValue<" # typeName # ">>($result).getType().getWidth()", "$value.size()"],
    "bitwidth matches">
  ]> {
  let summary = "A constant run of bits";
  let arguments = (ins ConstAttr:$value);
  let results = (outs MValueType:$result);
}

//===----------------------------------------------------------------------===//
// Signal spec
//===----------------------------------------------------------------------===//
//
// An `RTLIL::SigSpec` is a bit vector assembled from chunks, where each chunk is
// either a slice of a wire or a run of constant bits. A design that has been
// through `opt`, `techmap` or `abc` is full of them -- a `$_AND_` cell driving
// one bit of a wide wire, say -- so the import cannot represent cell ports as
// whole wires alone.
//
// These two ops are that representation. A chunk that happens to cover a whole
// wire, or a spec of exactly one chunk, stays bare: `rtlil.slice` and
// `rtlil.concat` appear only where a SigSpec genuinely needs them.

def SliceOp : RTLILOp<"slice", [Pure]> {
  let summary = "A contiguous run of bits from a value";
  let description = [{
    `$offset` counts from bit 0 of `$input`, *not* from the wire's
    `start_offset` -- that attribute is Verilog declaration cosmetics and is
    carried on `rtlil.wire` separately. The number of bits taken is the width of
    the result.
  }];
  let arguments = (ins MValueType:$input, I32Attr:$offset);
  let results = (outs MValueType:$result);
  let hasVerifier = 1;
}

def ConcatOp : RTLILOp<"concat", [Pure]> {
  let summary = "Several values joined into a wider one";
  let description = [{
    Operands are ordered **least significant first**, matching
    `RTLIL::SigSpec::chunks()` and the bit order of `rtlil.const`. Note this is
    the opposite of RTLIL's textual `{ msb ... lsb }` syntax.
  }];
  let arguments = (ins Variadic<MValueType>:$inputs);
  let results = (outs MValueType:$result);
  let hasVerifier = 1;
}

// `port_id` and `start_offset` are `I32Attr` because `RTLIL::Wire` declares
// both as plain `int`.
def WireOp : RTLILOp<"wire"> {
  let summary = "A named RTLIL wire, and the value its bits carry";
  let arguments = (ins StrAttr:$name
                       ,BoolAttr:$is_signed
                       ,DefaultValuedAttr<I32Attr, "0">:$port_id
                       ,DefaultValuedAttr<I32Attr, "0">:$start_offset
                       ,DefaultValuedAttr<BoolAttr, "false">:$port_input
                       ,DefaultValuedAttr<BoolAttr, "false">:$port_output
                       ,DefaultValuedAttr<BoolAttr, "false">:$upto
                       // The RTLIL `attributes` dict. `src` is not stored here:
                       // it becomes the op's Location on import and is derived
                       // back from it on export, so keeping it would duplicate.
                       ,DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes
  );
  DerivedAttr width = DerivedAttr<"::mlir::IntegerAttr", [{
        return mlir::IntegerAttr::get(mlir::IntegerType::get(getContext(), 32), llvm::cast<rtlil::MValueType>(this->getResult().getType()).getWidth());
                          }], "getWidth()">;
  let results = (outs MValueType:$result);
}

//===----------------------------------------------------------------------===//
// Structure
//===----------------------------------------------------------------------===//

// Defined after `WireOp` so that `getPortWires()` can name it by value.
def ModuleOp : RTLILOp<"module", [
    HasParent<"::mlir::ModuleOp">,
    Symbol,
    IsolatedFromAbove,
    SingleBlock,
    NoTerminator,
    NoRegionArguments,
    RegionKindInterface,
    OpAsmOpInterface
  ]> {
  let summary = "A named RTLIL netlist of wires and cells";
  let description = [{
    Models one `RTLIL::Module`. The symbol name is the RTLIL identifier
    including its sigil, so a module Yosys calls `\demo` is `rtlil.module
    @"\\demo"` here -- see `rtlil.wire` for why the sigil is part of the name.

    Ports are not listed separately: in RTLIL a port *is* a wire with a non-zero
    `port_id`, and `RTLIL::Module::fixup_ports()` derives the port list from
    those flags. Keeping a second list here would be a second source of truth
    that any Yosys pass could desynchronise.

    The body is a *graph* region. RTLIL is a netlist rather than straight-line
    code, so a cell may drive a wire declared below it.
  }];

  let arguments = (ins SymbolNameAttr:$sym_name,
                       DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes,
                       DefaultValuedAttr<StrArrayAttr, "{}">:$avail_parameters);
  let regions = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    $sym_name (`avail_parameters` $avail_parameters^)?
    (`attributes` $rtlil_attributes^)? attr-dict-with-keyword $body
  }];

  // The auto-generated builders leave the region blockless, which every
  // `getBodyBlock()` would then walk off the end of. Replace them with builders
  // that create the single block.
  let skipDefaultBuilders = 1;
  let builders = [
    OpBuilder<(ins "::mlir::StringAttr":$symName,
                   CArg<"::mlir::ArrayAttr", "{}">:$rtlilAttributes,
                   CArg<"::mlir::ArrayAttr", "{}">:$availParameters)>,
    OpBuilder<(ins "::llvm::StringRef":$symName), [{
      build($_builder, $_state, $_builder.getStringAttr(symName));
    }]>
  ];

  let extraClassDeclaration = [{
    /// Implement RegionKindInterface.
    static ::mlir::RegionKind getRegionKind(unsigned index) {
      return ::mlir::RegionKind::Graph;
    }

    /// Implement OpAsmOpInterface: print the body without an `rtlil.` prefix on
    /// every op.
    static ::llvm::StringRef getDefaultDialect() { return "rtlil"; }

    ::mlir::Block *getBodyBlock() { return &getBody().front(); }

    // The port wires are reached through the free function
    // `circt::rtlil::getPortWires(ModuleOp)` rather than a member: TableGen
    // emits op classes in alphabetical order, so `WireOp` is still incomplete
    // here and `SmallVector<WireOp>` cannot be named.
  }];

  let hasVerifier = 1;
}

//===----------------------------------------------------------------------===//
// Cell related operations
//===----------------------------------------------------------------------===//


def CellOp : RTLILOp<"cell", [DeclareOpInterfaceMethods<CellOpInterface>]> {
  let summary = "A cell of any type, with its ports and parameters spelled out";
  let arguments = (ins StrAttr:$name
                       ,StrAttr:$type
                       ,Variadic<MValueType>:$connections
                       ,StrArrayAttr:$ports
                       ,ParamArrayAttr:$parameters
                       ,DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes
  );
}

class FixedOperands<int n> : AllMatch<["getNumOperands()", "" # n], "has " # n # " connections">;


class FixedWidths<list<string> widths> : MValueExpander, TraitList<!foreach(idx, !range(widths), AllMatch<["::llvm::cast<"# typeName #">(getOperand(" # idx # ").getType()).getWidth()", "" # widths[idx]], "input " # idx # " width is " # widths[idx]>)>;

class BinOp<string t> : RTLILOp<t, [DeclareOpInterfaceMethods<CellOpInterface>, FixedOperands<3>, FixedWidths<["$width", "$width", "$width"]>]> {
  let summary = "The RTLIL `" # t # "` cell, with equally wide A, B and Y ports";
  let arguments = (ins StrAttr:$name,
                      Variadic<MValueType>:$connections,
                      I32Attr:$width,
                      I32Attr:$opsSigned,
                      DefaultValuedAttr<StrAttr, [{"$$}] # t # [{"}]>:$type,
                      DefaultValuedAttr<StrArrayAttr, [{ {"\\A", "\\B", "\\Y"} }]>:$ports);
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
                            return ::circt::rtlil::createParamArrayAttr(getContext(), {
                              {"\\A_SIGNED", 32, getOpsSigned()},
                              {"\\B_SIGNED", 32, getOpsSigned()},
                              {"\\A_WIDTH", 32, getWidth()},
                              {"\\B_WIDTH", 32, getWidth()},
                              {"\\Y_WIDTH", 32, getWidth()},
                            });
                          }], "getParameters()">;
}

def AndOp : BinOp<"and">;
def OrOp  : BinOp<"or">;
def AddOp : BinOp<"add">;
def SubOp : BinOp<"sub">;

def MuxOp : RTLILOp<"mux", [DeclareOpInterfaceMethods<CellOpInterface>, FixedOperands<4>, FixedWidths<["$width", "$width", "1", "$width"]>]> {
  let summary = "The RTLIL `mux` cell: Y is B when S is 1, else A";
  let arguments = (ins StrAttr:$name,
                      Variadic<MValueType>:$connections,
                      I32Attr:$width,
                      DefaultValuedAttr<StrAttr, [{"$$mux"}]>:$type,
                      DefaultValuedAttr<StrArrayAttr, [{ {"\\A", "\\B", "\\S", "\\Y"} }]>:$ports);
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
                            return ::circt::rtlil::createParamArrayAttr(getContext(), {
                              {"\\WIDTH", 32, getWidth()},
                            });
                          }], "getParameters()">;
}

class CmpOp<string t> : RTLILOp<t, [DeclareOpInterfaceMethods<CellOpInterface>, FixedOperands<3>, FixedWidths<["$width", "$width", "1"]>]> {
  let summary = "The RTLIL `" # t # "` cell, comparing A and B into a one-bit Y";
  let arguments = (ins StrAttr:$name,
                      Variadic<MValueType>:$connections,
                      I32Attr:$width,
                      I32Attr:$opsSigned,
                      DefaultValuedAttr<StrAttr, [{"$$}] # t # [{"}]>:$type,
                      DefaultValuedAttr<StrArrayAttr, [{ {"\\A", "\\B", "\\Y"} }]>:$ports);
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
                            return ::circt::rtlil::createParamArrayAttr(getContext(), {
                              {"\\A_SIGNED", 32, getOpsSigned()},
                              {"\\B_SIGNED", 32, getOpsSigned()},
                              {"\\A_WIDTH", 32, getWidth()},
                              {"\\B_WIDTH", 32, getWidth()},
                              {"\\Y_WIDTH", 32, 1},
                            });
                          }], "getParameters()">;
}

def GTOp : CmpOp<"gt">;
def EQOp : CmpOp<"eq">;
def NEOp : CmpOp<"ne">;
def GEOp : CmpOp<"ge">;
def LEOp : CmpOp<"le">;
def LTOp : CmpOp<"lt">;

def DFFOp : RTLILOp<"dff", [DeclareOpInterfaceMethods<CellOpInterface>, FixedOperands<3>, FixedWidths<["1", "$width", "$width"]>]> {
  let summary = "The RTLIL `dff` cell: Q takes D on the rising CLK edge";
  let arguments = (ins StrAttr:$name,
                      Variadic<MValueType>:$connections,
                      I32Attr:$width,
                      DefaultValuedAttr<StrAttr, [{"$$dff"}]>:$type,
                      DefaultValuedAttr<StrArrayAttr, [{ {"\\CLK", "\\D", "\\Q"} }]>:$ports);
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
                        return ::circt::rtlil::createParamArrayAttr(getContext(), {
                          {"\\WIDTH", 32, getWidth()},
                          {"\\CLK_POLARITY", 32, 1}
                        });
                      }], "getParameters()">;
}

def ALDFFOp : RTLILOp<"aldff", [DeclareOpInterfaceMethods<CellOpInterface>, FixedOperands<5>, FixedWidths<["1", "$width", "1", "$width", "$width"]>]> {
  let summary = "The RTLIL `aldff` cell: a `dff` that loads AD while ALOAD is 1";
  let arguments = (ins StrAttr:$name,
                      Variadic<MValueType>:$connections,
                      I32Attr:$width,
                      DefaultValuedAttr<StrAttr, [{"$$aldff"}]>:$type,
                      DefaultValuedAttr<StrArrayAttr, [{ {"\\CLK", "\\D", "\\ALOAD", "\\AD", "\\Q"} }]>:$ports);
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
                        return ::circt::rtlil::createParamArrayAttr(getContext(), {
                          {"\\WIDTH", 32, getWidth()},
                          {"\\CLK_POLARITY", 32, 1},
                          {"\\ALOAD_POLARITY", 32, 1},
                        });
                      }], "getParameters()">;
}

def InstanceOp : RTLILOp<"instance", [
    DeclareOpInterfaceMethods<CellOpInterface>,
    DeclareOpInterfaceMethods<SymbolUserOpInterface>]> {
  let summary = "A cell whose type names another module in the design";
  let description = [{
    The RTLIL-level equivalent of `rtlil.cell` for the case where the cell type
    resolves to an `rtlil.module`, so the reference can be a real MLIR symbol
    and a dangling callee is a verifier error rather than something the exporter
    discovers.
  }];
  let arguments = (ins StrAttr:$name,
                       FlatSymbolRefAttr:$type,
                       Variadic<MValueType>:$connections,
                       StrArrayAttr:$ports,
                       ParamArrayAttr:$parameters,
                       DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes
  );
}

def WConnectionOp : RTLILOp<"wconnection"> {
  let summary = "One entry of a module's connection list";
  let description = [{
    Represents one `RTLIL::SigSig` in `RTLIL::Module::connections()`.
    `$lhs` is driven by `$rhs` and they must have the same width.
  }];
  let arguments = (ins MValueType:$lhs, MValueType:$rhs);
  let hasVerifier = 1;
}

#endif // CIRCT_DIALECT_RTLIL_RTLILOPS_TD
