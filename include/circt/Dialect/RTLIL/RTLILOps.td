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
// Structure
//===----------------------------------------------------------------------===//

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
  let summary = "An RTLIL netlist of wires and cells";
  let description = [{
    Equivalent of `RTLIL::Module`.

    Collection of all elements as a graph region.
    Ports are not defined as arguments, but are marked by wires with a
    non-zero `port_id`.
  }];

  let arguments = (ins SymbolNameAttr:$sym_name,
                       DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes,
                       DefaultValuedAttr<StrArrayAttr, "{}">:$avail_parameters);
  let regions = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    $sym_name (`avail_parameters` $avail_parameters^)?
    (`attributes` $rtlil_attributes^)? attr-dict-with-keyword $body
  }];

  // Custom builders which create a single block.
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

    /// Fills `ports` with the wires flagged as ports, ordered by `port_id`.
    void getPortWires(::llvm::SmallVectorImpl<WireOp> &ports);
  }];

  // The body checks read the nested ops' attributes, so they run in
  // `verifyRegions()`, after those ops have passed their own verifiers.
  let hasVerifier = 1;
  let hasRegionVerifier = 1;
}

//===----------------------------------------------------------------------===//
// Wires and connections
//===----------------------------------------------------------------------===//

def WireOp : RTLILOp<"wire"> {
  let summary = "A named RTLIL wire, and the value its bits carry";
  let description = [{
    Equivalent of `RTLIL::Wire`.
    Keeps the convention of Yosys for the names: `\a` for explicitly named and
    `$1` for constructed wires.

    `rtlil.module` checks that the `port_id`s are exactly 1..N, which
    `fixup_ports()` would otherwise silently impose. `start_offset` and `upto`
    are carried to the backends for convenience (`wire [7:4]`, `wire [0:7]`);
    they do not affect bit numbering here.
  }];
  // `direction` stands for `port_input` and `port_output`; `inout` sets both.
  // `port_id` and `start_offset` are `I32Attr` because `RTLIL::Wire` declares
  // both as plain `int`. `src` is not stored, as the export derives it from the
  // location.
  let arguments = (ins StrAttr:$name,
                       OptionalAttr<PortDirection>:$direction,
                       DefaultValuedAttr<I32Attr, "0">:$port_id,
                       DefaultValuedAttr<Signedness,
                         "::circt::rtlil::Signedness::Unsigned">:$signedness,
                       DefaultValuedAttr<I32Attr, "0">:$start_offset,
                       UnitAttr:$upto,
                       // The RTLIL `attributes` dict.
                       DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes);
  DerivedAttr width = DerivedAttr<"::mlir::IntegerAttr", [{
    return ::mlir::IntegerAttr::get(
        ::mlir::IntegerType::get(getContext(), 32),
        ::llvm::cast<::circt::rtlil::MValueType>(getResult().getType())
            .getWidth());
  }], "getWidth()">;
  let results = (outs MValueType:$result);
  let extraClassDeclaration = [{
    /// `RTLIL::Wire::port_input`: an `input` or `inout` port.
    bool isPortInput() {
      auto direction = getDirection();
      return direction && *direction != ::circt::rtlil::PortDirection::Output;
    }
    /// `RTLIL::Wire::port_output`: an `output` or `inout` port.
    bool isPortOutput() {
      auto direction = getDirection();
      return direction && *direction != ::circt::rtlil::PortDirection::Input;
    }
  }];
  let assemblyFormat = [{
    $name ($direction^)? (`port` $port_id^)?
    ($signedness^)? (`offset` $start_offset^)? (`upto` $upto^)?
    (`attrs` $rtlil_attributes^)? attr-dict `:` qualified(type($result))
  }];
}

def WConnectionOp : RTLILOp<"wconnection", [AllTypesMatch<["lhs", "rhs"]>]> {
  let summary = "One entry of a module's connection list";
  let description = [{
    Represents one `RTLIL::SigSig` in `RTLIL::Module::connections()`.
    `$lhs` is driven by `$rhs` and they must have the same width.
  }];
  let arguments = (ins MValueType:$lhs, MValueType:$rhs);
  let assemblyFormat = "$lhs `,` $rhs attr-dict `:` qualified(type($lhs))";
}

//===----------------------------------------------------------------------===//
// Signal spec
//===----------------------------------------------------------------------===//
//
// An `RTLIL::SigSpec` is a bit vector of chunks (slice of a wire or
// constants).

def ConstOp : RTLILOp<"const", [Pure, AllMatch<[
    "::llvm::cast<::mlir::TypedValue<::circt::rtlil::MValueType>>($result)"
        # ".getType().getWidth()",
    "$value.size()"], "bitwidth matches">]> {
  let summary = "A constant run of bits";
  let description = [{
    Equivalent of `RTLIL::Const`. Bits are written most significant first, so
    `rtlil.const <"1010"> : !rtlil<val[4]>` is 10.
  }];
  let arguments = (ins ConstAttr:$value);
  let results = (outs MValueType:$result);
  let assemblyFormat = "$value attr-dict `:` qualified(type($result))";
}

def SliceOp : RTLILOp<"slice", [Pure]> {
  let summary = "A contiguous run of bits from a value";
  let description = [{
    A slice from `$input` starting at `$offset` with the length taken from the
    width of `$result`.
  }];
  let arguments = (ins MValueType:$input, I32Attr:$offset);
  let results = (outs MValueType:$result);
  let hasVerifier = 1;
  let assemblyFormat = "$input `offset` $offset attr-dict `:` functional-type($input, $result)";
}

def ConcatOp : RTLILOp<"concat", [Pure]> {
  let summary = "Several values joined into a wider one";
  let description = [{
    Concatenation of `$inputs` ordered least significant first, following
    `RTLIL::SigSpec::chunks()`.
    Note this is the opposite of RTLIL's textual `{ msb ... lsb }` syntax.
  }];
  let arguments = (ins Variadic<MValueType>:$inputs);
  let results = (outs MValueType:$result);
  let hasVerifier = 1;
  let assemblyFormat = "$inputs attr-dict `:` functional-type($inputs, $result)";
}

//===----------------------------------------------------------------------===//
// Generic cells
//===----------------------------------------------------------------------===//

def CellOp : RTLILOp<"cell", [DeclareOpInterfaceMethods<CellOpInterface>]> {
  let summary = "A cell of any type, with its ports and parameters";
  let description = [{
    Equivalent of `RTLIL::Cell`. Can represent any cell type, if no dedicated
    op is defined.
    `$ports` names the port each `$connections` operand attaches to, in order.
    `$parameters` is the parameter dict verbatim.

    The dialect defines specialized ops, which should be preferred.
  }];
  let arguments = (ins StrAttr:$name,
                       StrAttr:$type,
                       Variadic<MValueType>:$connections,
                       StrArrayAttr:$ports,
                       ParamArrayAttr:$parameters,
                       DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes);
  let assemblyFormat = [{
    $name $type custom<CellPorts>($connections, $ports)
    `parameters` $parameters (`attributes` $rtlil_attributes^)? attr-dict
    (`:` type($connections)^)?
  }];
}

def InstanceOp : RTLILOp<"instance", [
    DeclareOpInterfaceMethods<CellOpInterface>,
    DeclareOpInterfaceMethods<SymbolUserOpInterface>]> {
  let summary = "A cell pointing to another module";
  let description = [{
    Equivalent of `rtlil.cell` for the case where the cell type resolves to an
    `rtlil.module`.
    The reference can be an MLIR symbol. Invalid references result in a
    verifier error.
  }];
  let arguments = (ins StrAttr:$name,
                       FlatSymbolRefAttr:$type,
                       Variadic<MValueType>:$connections,
                       StrArrayAttr:$ports,
                       ParamArrayAttr:$parameters,
                       DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes);
  let assemblyFormat = [{
    $name $type custom<CellPorts>($connections, $ports)
    `parameters` $parameters (`attributes` $rtlil_attributes^)? attr-dict
    (`:` type($connections)^)?
  }];
}

//===----------------------------------------------------------------------===//
// Typed cells
//===----------------------------------------------------------------------===//

class FixedOperands<int n>
    : AllMatch<["getNumOperands()", "" # n], "has " # n # " connections">;

// A cell type with a dedicated op.
// `ports` and `widths` line up with `$connections`. A width is either a number
// or `$width`, the op's `width` attribute; the verifier checks each port.
// `extraArgs` for arguments between `$width` and `$type`, such as a signedness
// flag.
// `$type` and `$ports` are fixed by the op.
class TypedCellOp<string t, list<string> ports, list<string> widths,
                  dag extraArgs = (ins)>
    : RTLILOp<t, [DeclareOpInterfaceMethods<CellOpInterface>,
                  FixedOperands<!size(widths)>]> {
  let arguments = !con(
      (ins StrAttr:$name,
           Variadic<MValueType>:$connections,
           I32Attr:$width),
      extraArgs,
      (ins DefaultValuedAttr<StrAttr, [{"$$}] # t # [{"}]>:$type,
           DefaultValuedAttr<StrArrayAttr,
             "{" # !interleave(!foreach(p, ports, [{"\\}] # p # [{"}]), ", ")
                 # "}">:$ports));
  let assemblyFormat = [{
    $name `(` $connections `)` attr-dict `:` type($connections)
  }];
  let hasVerifier = 1;
  let extraClassDefinition = [{
    ::mlir::LogicalResult $cppClass::verify() {
      return verifyPortWidths(*this, {}] #
        !interleave(!foreach(p, ports, "\"" # p # "\""), ", ") # [{}, {}] #
        !interleave(!foreach(w, widths, !subst("$width", "getWidth()", w)),
                    ", ") # [{});
    }
  }];
}

// Binary arithmetic and bitwise cells

class BinOp<string t>
    : TypedCellOp<t, ["A", "B", "Y"], ["$width", "$width", "$width"],
                  (ins I32Attr:$opsSigned)> {
  let summary =
      "The RTLIL `" # t # "` cell, with equally wide A, B and Y ports";
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
def XorOp : BinOp<"xor">;
def AddOp : BinOp<"add">;
def SubOp : BinOp<"sub">;
def MulOp : BinOp<"mul">;
// Truncating ops for Verilog's `/` and `%`. Not Yosys' `$divfloor` and
// `$modfloor`.
def DivOp : BinOp<"div">;
def ModOp : BinOp<"mod">;

// Shift cells

class ShiftOp<string t>
    : TypedCellOp<t, ["A", "B", "Y"], ["$width", "$width", "$width"],
                  (ins I32Attr:$aSigned)> {
  let summary =
      "The RTLIL `" # t # "` cell, shifting A by an unsigned B into Y";
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
    return ::circt::rtlil::createParamArrayAttr(getContext(), {
      {"\\A_SIGNED", 32, getASigned()},
      {"\\B_SIGNED", 32, 0},
      {"\\A_WIDTH", 32, getWidth()},
      {"\\B_WIDTH", 32, getWidth()},
      {"\\Y_WIDTH", 32, getWidth()},
    });
  }], "getParameters()">;
}

def ShlOp : ShiftOp<"shl">;
def ShrOp : ShiftOp<"shr">;
// Shift sign bit with A_SIGNED set.
def SShrOp : ShiftOp<"sshr">;

// Comparison cells

class CmpOp<string t>
    : TypedCellOp<t, ["A", "B", "Y"], ["$width", "$width", "1"],
                  (ins I32Attr:$opsSigned)> {
  let summary =
      "The RTLIL `" # t # "` cell, comparing A and B into a one-bit Y";
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

def LTOp : CmpOp<"lt">;
def LEOp : CmpOp<"le">;
def EQOp : CmpOp<"eq">;
def NEOp : CmpOp<"ne">;
// `$eqx` and `$nex` are SystemVerilog's `===` and `!==`.
def EQXOp : CmpOp<"eqx">;
def NEXOp : CmpOp<"nex">;
def GEOp : CmpOp<"ge">;
def GTOp : CmpOp<"gt">;

// Reduction cells

class ReduceOp<string t>
    : TypedCellOp<t, ["A", "Y"], ["$width", "1"], (ins I32Attr:$aSigned)> {
  let summary = "The RTLIL `" # t # "` cell, reducing A to a one-bit Y";
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
    return ::circt::rtlil::createParamArrayAttr(getContext(), {
      {"\\A_SIGNED", 32, getASigned()},
      {"\\A_WIDTH", 32, getWidth()},
      {"\\Y_WIDTH", 32, 1},
    });
  }], "getParameters()">;
}

def ReduceXorOp : ReduceOp<"reduce_xor">;

// Multiplexer cells

def MuxOp : TypedCellOp<"mux", ["A", "B", "S", "Y"],
                        ["$width", "$width", "1", "$width"]> {
  let summary = "The RTLIL `mux` cell: Y is B when S is 1, else A";
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
    return ::circt::rtlil::createParamArrayAttr(getContext(), {
      {"\\WIDTH", 32, getWidth()},
    });
  }], "getParameters()">;
}

// Flip-flop cells

def DFFOp : TypedCellOp<"dff", ["CLK", "D", "Q"], ["1", "$width", "$width"]> {
  let summary = "The RTLIL `dff` cell: Q takes D on the rising CLK edge";
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
    return ::circt::rtlil::createParamArrayAttr(getContext(), {
      {"\\WIDTH", 32, getWidth()},
      {"\\CLK_POLARITY", 32, 1}
    });
  }], "getParameters()">;
}

def ALDFFOp : TypedCellOp<"aldff", ["CLK", "D", "ALOAD", "AD", "Q"],
                          ["1", "$width", "1", "$width", "$width"]> {
  let summary =
      "The RTLIL `aldff` cell: a `dff` that loads AD while ALOAD is 1";
  DerivedAttr parameters = DerivedAttr<"::mlir::ArrayAttr", [{
    return ::circt::rtlil::createParamArrayAttr(getContext(), {
      {"\\WIDTH", 32, getWidth()},
      {"\\CLK_POLARITY", 32, 1},
      {"\\ALOAD_POLARITY", 32, 1},
    });
  }], "getParameters()">;
}

#endif // CIRCT_DIALECT_RTLIL_RTLILOPS_TD
