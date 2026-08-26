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
    let summary = "Types and operations for RTLIL dialect";
    let description = [{
        This dialect defines the `rtlil` dialect, which is used to translate
        between CIRCT and Yosys. For more information, see the [RTLIL
        description](https://yosyshq.readthedocs.io/projects/yosys/en/latest/yosys_internals/formats/rtlil_rep.html).
    }];
    let cppNamespace = "::circt::rtlil";
    // let hasConstantMaterializer = 1;
    let useDefaultTypePrinterParser = 1;
    let useDefaultAttributePrinterParser = 1;

    let extraClassDeclaration = [{
      /// Register all RTLIL types. Defined in `RTLILTypes.cpp`, next to the
      /// generated storage classes registration needs.
      void registerTypes();
      /// Register all RTLIL attributes. Defined in `RTLILAttributes.cpp`, for
      /// the same reason.
      void registerAttributes();
    }];
}

#endif // CIRCT_DIALECT_RTLIL_RTLILDIALECT_TD
