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

def MValueType : TypeDef<RTLILDialect, "MValue"> {
  let mnemonic = "val";
  let summary = "A fixed-width sequence of `RTLIL::State` bits";
  let description = [{
    The value of an RTLIL wire or cell port: `$width` bits, each one of the
    five `RTLIL::State` values. The width is always concrete, since Yosys
    resolves parameters before wires exist.
  }];
  let parameters = (ins "unsigned":$width);
  let assemblyFormat = "`[` $width `]`";
}

#endif // CIRCT_DIALECT_RTLIL_RTLILTYPES_TD
