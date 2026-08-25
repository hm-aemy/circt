//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILTYPES_TD
#define CIRCT_DIALECT_RTLIL_RTLILTYPES_TD

include "mlir/IR/AttrTypeBase.td"
include "circt/Dialect/RTLIL/RTLILDialect.td"

/// Base type for all RTLIL objects
class RTLILType<string name, list<Trait> traits = []>
  : TypeDef<RTLILDialect, name, traits> { }

/// All Values in RTLIL must pass of MValueType check
def RTLILValue : DialectType<
  RTLILDialect,
  CPred<"::circt::rtlil::isMValueType($_self)">,
  "value"
>;

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

class MValueExpander {
  string typeName = MValueTypeImpl.cppType;
}

#endif // CIRCT_DIALECT_RTLIL_RTLILTYPES_TD
