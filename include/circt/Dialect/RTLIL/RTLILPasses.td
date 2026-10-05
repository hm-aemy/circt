//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILPASSES_TD
#define CIRCT_DIALECT_RTLIL_RTLILPASSES_TD

include "mlir/Pass/PassBase.td"

def RunYosys : Pass<"rtlil-run-yosys", "mlir::ModuleOp"> {
  let summary = "Run a Yosys script over the `rtlil.module`s";
  let description = [{
    Exports every `rtlil.module` to an in-process `RTLIL::Design`, runs `script`
    on it, and replaces the `rtlil.module`s with the result. No files or
    `yosys` processes are involved.

    The modules are replaced rather than updated, since a script can add,
    remove and rename them. Other ops in the top-level module are left alone,
    so a partially converted design still works.

    Yosys is not thread safe, so all instances of this pass run serially.

    Requires CIRCT to be built with libyosys (`CIRCT_YOSYS_LIB_ENABLED`);
    otherwise the pass fails with an error.
  }];

  let options = [
    Option<"script", "script", "std::string", "\"\"",
           "Yosys commands, separated by ';' or newlines">,
    Option<"topModule", "top", "std::string", "\"\"",
           "Module for 'hierarchy -top'; defaults to '-auto-top'">,
    Option<"runHierarchy", "hierarchy", "bool", "true",
           "Run 'hierarchy -check' before the script">,
    Option<"quiet", "quiet", "bool", "true",
           "Suppress Yosys' log output unless the script fails">,
  ];

  let dependentDialects = ["circt::rtlil::RTLILDialect"];
}

#endif // CIRCT_DIALECT_RTLIL_RTLILPASSES_TD
