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

/// All Values in RTLIL must pass of MValueType check
def RTLILValue : DialectType<
  RTLILDialect,
  CPred<"::circt::rtlil::isMValueType($_self)">,
  "value"
>;

// A fixed-width sequence of `RTLIL::State`. Declares `rtlil::MValueType` in C++.
def MValueTypeImpl : TypeDef<RTLILDialect, "MValue"> {
  let mnemonic = "val";
  let summary = "fixed-width RTLIL::State sequence";
  let description = [{
    The value carried by every RTLIL wire and cell port: `$width` bits, each of
    which is one of the five `RTLIL::State` cases rather than a plain 0/1.

    The width is a plain `unsigned`, mirroring `RTLIL::Wire::width` and
    `RTLIL::SigChunk::width`, which are both `int`. Unlike `hw::IntType` there
    is no parameterized case to represent: Yosys keeps parameters in the
    module's `avail_parameters` and the cell's parameter dict, and `derive()`
    has substituted them by the time any wire exists.
  }];
  let parameters = (ins "unsigned":$width);
  let typeName = "rtlil.val";
  let assemblyFormat = "`[` $width `]`";
}

class MValueExpander {
  string typeName = MValueTypeImpl.cppType;
}

#endif // CIRCT_DIALECT_RTLIL_RTLILTYPES_TD
