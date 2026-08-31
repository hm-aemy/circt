//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILATTRIBUTES_TD
#define CIRCT_DIALECT_RTLIL_RTLILATTRIBUTES_TD

include "mlir/IR/BuiltinAttributeInterfaces.td"
include "mlir/IR/BuiltinTypes.td"
include "mlir/IR/AttrTypeBase.td"

include "circt/Dialect/RTLIL/RTLILDialect.td"
include "circt/Dialect/RTLIL/RTLILEnums.td"

//===----------------------------------------------------------------------===//
// Parameter Attributes
//===----------------------------------------------------------------------===//

// Mirror `dict<RTLIL::IdString, RTLIL::Const>` used in `RTLIL::AttrObject` for
// attributes and in `RTLIL::Cell` for parameters.
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
def ParamAttr : AttrDef<RTLILDialect, "Parameter"> {
  let mnemonic = "param";
  let summary = "Connect `RTLIL::Const` to `RTLIL::IdString`";
  let parameters = (ins
                    "::mlir::StringAttr":$name,
                    "::mlir::Attribute":$value,
                    OptionalParameter<"std::optional<uint16_t>">:$flags);
  // Explicitly delimited: without the angle brackets the printed form is
  // `#rtlil.param "\NAME" 4 : i32`, which is ambiguous inside the parameter
  // array.
  let assemblyFormat = "`<` $name $value (`flags` $flags^)? `>`";
  let builders = [
    AttrBuilder<(ins
      "llvm::StringRef":$name, "unsigned":$width,"uint64_t":$val), [{
        auto intattr = mlir::IntegerAttr::get(mlir::IntegerType::get($_ctxt, width), val);
        auto strattr = mlir::StringAttr::get($_ctxt, name);
        return Base::get($_ctxt, strattr, intattr, std::nullopt);
      }]
    >
  ];
  let genVerifyDecl = 1;
}

def ParamArrayAttr
  : TypedArrayAttrBase<ParamAttr, "cell parameters"> {
  let constBuilderCall = "::circt::rtlil::createParamArrayAttr($_builder.getContext(), $0)";
  let summary = "Dict of `RTLIL::Const` and `RTLIL::IdString`";
}

//===----------------------------------------------------------------------===//
// Connectivity Attributes
//===----------------------------------------------------------------------===//

def ConstAttr : AttrDef<RTLILDialect, "Const"> {
  let mnemonic = "const";
  let summary = "constant multi-valued bitvec";
  let description = [{
    The general form of an `RTLIL::Const`, written as a bit string over the
    alphabet of `RTLIL::Const::as_string()`: `0`, `1`, `x`, `z` and `-` (`Sa`).

    The string is most significant bit first, as RTLIL's own `4'1010` reads.
    `$bits` is stored the other way round, matching `RTLIL::Const::to_bits()`
    and the operand order of `rtlil.concat`.
  }];
  let parameters = (ins ArrayRefParameter<"::circt::rtlil::StateEnum">:$bits);
  let hasCustomAssemblyFormat = 1;
  let extraClassDeclaration = [{
    /// Named to match `RTLIL::Const::size()`, and used by the `rtlil.const`
    /// bitwidth trait.
    size_t size() const { return getBits().size(); }

    /// The printed form of the bits, most significant first.
    std::string getBitString() const;

    /// The attribute for such a bit string, or a null attribute if any
    /// character is not one of `01xz-`.
    static ConstAttr getFromBitString(::mlir::MLIRContext *context,
                                      ::llvm::StringRef bits);
  }];
}


#endif // CIRCT_DIALECT_RTLIL_RTLILATTRIBUTES_TD
