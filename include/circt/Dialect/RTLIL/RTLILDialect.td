//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILDIALECT_TD
#define CIRCT_DIALECT_RTLIL_RTLILDIALECT_TD

include "mlir/IR/DialectBase.td"

def RTLILDialect : Dialect {
  let name = "rtlil";
  let summary = "An in-memory mirror of Yosys' RTLIL netlist";
  let description = [{
    Represents a Yosys `RTLIL::Design` with MLIR operations, so that converting
    between the two representations does not lose information.

    `import-rtlil` and `export-rtlil` translate from and to RTLIL files, and
    `convert-hw-to-rtlil` lowers the core dialects to RTLIL.

    For the format itself, see the [RTLIL
    description](https://yosyshq.readthedocs.io/projects/yosys/en/latest/yosys_internals/formats/rtlil_rep.html).
  }];
  let cppNamespace = "::circt::rtlil";
  let useDefaultTypePrinterParser = 1;
  let useDefaultAttributePrinterParser = 1;

  let extraClassDeclaration = [{
    /// Register all RTLIL types.
    void registerTypes();
    /// Register all RTLIL attributes.
    void registerAttributes();
  }];
}

#endif // CIRCT_DIALECT_RTLIL_RTLILDIALECT_TD
