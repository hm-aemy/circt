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

def ParamAttr : AttrDef<RTLILDialect, "Parameter"> {
  let mnemonic = "param";
  let summary = "`RTLIL::IdString` naming an `RTLIL::Const`";
  let description = [{
    One entry of the `dict<RTLIL::IdString, RTLIL::Const>` that
    `RTLIL::AttrObject` uses for attributes and `RTLIL::Cell` for parameters.

    `$value` is a bare `Attribute` so it can hold strings as well as bit vectors
    of any width that may contain `x` and `z`. It holds one of:

      * `IntegerAttr`: the convenient case, and what the derived `parameters`
        attributes below construct;
      * `ConstAttr`: an explicit bit vector, for widths over 64 or any value
        containing `x`/`z`;
      * `StringAttr`: a string parameter.

    `$flags` carries `RTLIL::Const::flags` verbatim so that `CONST_FLAG_STRING`,
    `_SIGNED` and `_REAL` survive a round trip.
  }];
  let parameters = (ins
                    "::mlir::StringAttr":$name,
                    "::mlir::Attribute":$value,
                    OptionalParameter<"std::optional<uint16_t>">:$flags);
  // Angle brackets make it unambiguous inside the parameter array.
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
  let summary = "A dict of `RTLIL::Const`, keyed by `RTLIL::IdString`";
  let description = [{
    Storage for the parameters of a cell or the attributes of any object that
    carries them.
  }];
}

//===----------------------------------------------------------------------===//
// Connectivity Attributes
//===----------------------------------------------------------------------===//

def ConstAttr : AttrDef<RTLILDialect, "Const"> {
  let mnemonic = "const";
  let summary = "A constant multi-valued bit vector";
  let description = [{
    An `RTLIL::Const` as a bit string of `RTLIL::State` values.

    The string is most significant bit first. `$bits` is stored the other way
    round, matching `RTLIL::Const::to_bits()` and the operand order of
    `rtlil.concat`.
  }];
  let parameters = (ins ArrayRefParameter<"::circt::rtlil::StateEnum">:$bits);
  let hasCustomAssemblyFormat = 1;
  let extraClassDeclaration = [{
    /// Matches `RTLIL::Const::size()`.
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
