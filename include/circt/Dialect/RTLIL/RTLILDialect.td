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
        Follows `RTLIL::Design` closely enough that a design can be handed to
        Yosys in process and read back without losing names, attributes or cell
        parameters. That is what makes Yosys passes such as `opt`, `techmap`
        and `abc` usable from a CIRCT pipeline; see `rtlil-run-yosys`.

        Because the ops track RTLIL's structures rather than CIRCT's, reaching
        the dialect is an explicit conversion (`convert-hw-to-rtlil`) rather
        than something the other hardware dialects do implicitly. For the
        format itself, see the [RTLIL
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
