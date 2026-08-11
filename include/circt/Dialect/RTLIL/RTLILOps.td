#ifndef RTLIL_OPS
#define RTLIL_OPS

include "circt/Dialect/RTLIL/RTLIL.td"
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

//===----------------------------------------------------------------------===//
// RTLIL dialect definition.
//===----------------------------------------------------------------------===//

def RTLILDialect : Dialect {
    let name = "rtlil";
    let summary = "Types and operations for RTLIL dialect";
    let description = [{
        This dialect deifnes the `rtlil` dialect, which is used to translate
        between CIRCT and Yosys. For more information, see the [RTLIL
        description](https://yosyshq.readthedocs.io/projects/yosys/en/latest/yosys_internals/formats/rtlil_rep.html).
    }];
    let cppNamespace = "::circt::rtlil";
    // let hasConstantMaterializer = 1;
    let useDefaultTypePrinterParser = 1;
    let useDefaultAttributePrinterParser = 1;
}

//===----------------------------------------------------------------------===//
// Base class definitions
//===----------------------------------------------------------------------===//

class RTLILOp<string mnemonic, list<Trait> traits = []> :
        Op<RTLILDialect, mnemonic, traits>;

class RTLILAttr<string attrName, string attrMnemonic, list<Trait> traits = []>
    : AttrDef<RTLILDialect, attrName, traits> {
  let mnemonic = attrMnemonic;
}

class RTLILType<string name, list<Trait> traits = []>
  : TypeDef<RTLILDialect, name, traits> { }

//===----------------------------------------------------------------------===//
// Operation helper definitions
//===----------------------------------------------------------------------===//

// Parameters

// One entry of an `RTLIL::Cell`'s parameter dict, or of any RTLIL object's
// attribute dict -- both map an identifier to an `RTLIL::Const`, so both use
// this.
//
// `$value` is deliberately a bare `Attribute` rather than an `IntegerAttr`: an
// `RTLIL::Const` is either a string or a bit vector of arbitrary width that may
// contain `x` and `z`, and Yosys hands back plenty of both ($lut masks, $mem
// INIT, string parameters). It holds one of:
//
//   * `IntegerAttr` -- the convenient case, and what the derived `parameters`
//     attributes below construct;
//   * `ConstAttr`   -- an explicit bit vector, for widths over 64 or any value
//     containing `x`/`z`;
//   * `StringAttr`  -- a string parameter.
//
// `$flags` carries `RTLIL::Const::flags` verbatim so that CONST_FLAG_STRING,
// _SIGNED and _REAL survive a round trip.
def ParamAttr : RTLILAttr<"Parameter", "param"> {
  let parameters = (ins
                    "::mlir::StringAttr":$name,
                    "::mlir::Attribute":$value,
                    OptionalParameter<"std::optional<uint16_t>">:$flags);
  // Explicitly delimited: without the angle brackets the printed form is
  // `#rtlil.param "\NAME" 4 : i32`, which is ambiguous inside the parameter
  // array.
  let assemblyFormat = "`<` $name $value (`flags` $flags^)? `>`";
  let builders = [
    AttrBuilder<(ins "llvm::StringRef":$name, "unsigned":$width,"uint64_t":$val), [{
      auto intattr = mlir::IntegerAttr::get(mlir::IntegerType::get($_ctxt, width), val);
      auto strattr = mlir::StringAttr::get($_ctxt, name);
      return Base::get($_ctxt, strattr, intattr, std::nullopt);
    }]>
  ];
  let genVerifyDecl = 1;
}

def ParamArrayAttr
  : TypedArrayAttrBase<ParamAttr, "cell parameters"> {
  let constBuilderCall = "::circt::rtlil::createParamArrayAttr($_builder.getContext(), $0)";
}

// Connectivity

class I8EnumAttrCase<string sym, int val, string str = sym>
    : IntEnumAttrCaseBase<I8, sym, str, val>;
def StateS0: I8EnumAttrCase<"S0", 0>;
def StateS1: I8EnumAttrCase<"S1", 1>;
def StateSx: I8EnumAttrCase<"Sx", 2>;
def StateSz: I8EnumAttrCase<"Sz", 3>;
def StateSa: I8EnumAttrCase<"Sa", 4>;

class I8EnumAttr<string name, string summary, list<I8EnumAttrCase> cases> :
    IntEnumAttr<I8, name, summary, cases> {
  let underlyingType = "uint8_t";
}

def StateEnumAttr: I8EnumAttr<"StateEnum", "RTLIL::State model",
    [StateS0, StateS1, StateSx, StateSz, StateSa]> {
  let stringToSymbolFnName = "ConvertToEnum";
  let symbolToStringFnName = "ConvertToString";
  let cppNamespace = "::circt::rtlil";
}

// A parameterized integer type. Declares the hw::IntType in C++.
def MValueTypeImpl : RTLILType<"MValue"> {
  let summary = "parameterized-width RTLIL::State sequence";
  let parameters = (ins "::mlir::TypedAttr":$width);
  let typeName = "rtlil.val";
  let assemblyFormat = "`[` $width `]`";
  let mnemonic = "val";
  let extraClassDeclaration = [{
    /// The width of this value, in bits.
    ///
    /// The width is stored as a `TypedAttr` so that it can print as
    /// `!rtlil<val[32 : i32]>`, which means every reader would otherwise spell
    /// `cast<IntegerAttr>(cast<MValueType>(t).getWidth()).getInt()`. The
    /// import/export converters do this on every wire, chunk and slice.
    unsigned getBitWidth();

    /// Build an `!rtlil<val[$width : i32]>` from a plain bit width.
    static MValueType get(::mlir::MLIRContext *context, unsigned width);
  }];
}

class MValueExpander  {
  string typeName = MValueTypeImpl.cppType;
}

def RTLILValue : DialectType<RTLILDialect, CPred<"::circt::rtlil::isMValueType($_self)">, "value">;

def ConstAttr
  : TypedArrayAttrBase<StateEnumAttr, "constant multi-valued bitvec">;

def ConstOp : MValueExpander, RTLILOp<"const", [AllMatch<["llvm::cast<mlir::IntegerAttr>(llvm::cast<mlir::TypedValue<" # typeName # ">>($result).getType().getWidth()).getValue()", "$value.size()"], "bitwidth matches">]> {
  let arguments = (ins ConstAttr:$value);
  let results = (outs RTLILValue:$result);
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
  let arguments = (ins RTLILValue:$input, I32Attr:$offset);
  let results = (outs RTLILValue:$result);
  let hasVerifier = 1;
}

def ConcatOp : RTLILOp<"concat", [Pure]> {
  let summary = "Several values joined into a wider one";
  let description = [{
    Operands are ordered **least significant first**, matching
    `RTLIL::SigSpec::chunks()` and the bit order of `rtlil.const`. Note this is
    the opposite of RTLIL's textual `{ msb ... lsb }` syntax.
  }];
  let arguments = (ins Variadic<RTLILValue>:$inputs);
  let results = (outs RTLILValue:$result);
  let hasVerifier = 1;
}

// TODO Is I32 correct?
def WireOp : RTLILOp<"wire"> {
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
        return mlir::IntegerAttr::get(mlir::IntegerType::get(getContext(), 32), llvm::cast<mlir::IntegerAttr>(llvm::cast<rtlil::MValueType>(this->getResult().getType()).getWidth()).getValue());
                          }], "getWidth()">;
  let results = (outs RTLILValue:$result);
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

def CellOpInterface : OpInterface<"CellOpInterface"> {
  let description = [{
    All cell types implement this interface.
  }];
  let cppNamespace = "::circt::rtlil";
  let methods = [
    InterfaceMethod<[{
      Gets the name of the cell op.
    }], "::llvm::StringRef", "getCellName", (ins),
    /*methodBody*/ "",
    /*defaultImpl*/ [{
      return $_op.getName();
    }]>,
    InterfaceMethod<[{
      Gets the type of the cell op.
    }], "::llvm::StringRef", "getCellType", (ins),
    "", [{
      return $_op.getType();
    }]>,
    InterfaceMethod<[{
      Gets the connections of the cell op.
    }], "::mlir::OperandRange", "getCellConnections", (ins),
    "", [{
      return $_op.getConnections();
    }]>,
    InterfaceMethod<[{
      Gets the Ports of the cell op.
    }], "::mlir::ArrayAttr", "getCellPorts", (ins),
    "", [{
      return $_op.getPorts();
    }]>,
    InterfaceMethod<[{
      Gets the parameters of the cell op.
    }], "::mlir::ArrayAttr", "getCellParameters", (ins),
    "", [{
      return $_op.getParameters();
    }]>
  ];
}

def CellOp : RTLILOp<"cell", [DeclareOpInterfaceMethods<CellOpInterface>]> {
  let arguments = (ins StrAttr:$name
                       ,StrAttr:$type
                       ,Variadic<RTLILValue>:$connections
                       ,StrArrayAttr:$ports
                       ,ParamArrayAttr:$parameters
                       ,DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes
  );
}

class FixedOperands<int n> : AllMatch<["getNumOperands()", "" # n], "has " # n # " connections">;


class FixedWidths<list<string> widths> : MValueExpander, TraitList<!foreach(idx, !range(widths), AllMatch<["::llvm::cast<::mlir::IntegerAttr>(::llvm::cast<"# typeName #">(getOperand(" # idx # ").getType()).getWidth()).getInt()", "" # widths[idx]], "input " # idx # " width is " # widths[idx]>)>;

class BinOp<string t> : RTLILOp<t, [DeclareOpInterfaceMethods<CellOpInterface>, FixedOperands<3>, FixedWidths<["$width", "$width", "$width"]>]> {
  let arguments = (ins StrAttr:$name,
                      Variadic<RTLILValue>:$connections,
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
  let arguments = (ins StrAttr:$name,
                      Variadic<RTLILValue>:$connections,
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
  let arguments = (ins StrAttr:$name,
                      Variadic<RTLILValue>:$connections,
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
  let arguments = (ins StrAttr:$name,
                      Variadic<RTLILValue>:$connections,
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
  let arguments = (ins StrAttr:$name,
                      Variadic<RTLILValue>:$connections,
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
                       Variadic<RTLILValue>:$connections,
                       StrArrayAttr:$ports,
                       ParamArrayAttr:$parameters,
                       DefaultValuedAttr<ParamArrayAttr, "{}">:$rtlil_attributes
  );
}

def WConnectionOp : RTLILOp<"wconnection"> {
  let arguments = (ins RTLILValue:$lhs, RTLILValue:$rhs);
}
                      //  , // TODO back to FlatSymbolRefAttr?
  // let results = (outs Variadic<AnyType>:$results);

#endif // RTLIL_OPS
