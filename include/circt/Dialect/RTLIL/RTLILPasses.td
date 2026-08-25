//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// These passes need libyosys, so they live in their own tablegen file rather
// than in `circt/Conversion/Passes.td`: the generated
// `registerCIRCTConversionPasses()` names every pass unconditionally, which
// would not link in a build configured without `CIRCT_YOSYS_LIB_ENABLED`.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILPASSES_TD
#define CIRCT_DIALECT_RTLIL_RTLILPASSES_TD

include "mlir/Pass/PassBase.td"

def RunYosys : Pass<"rtlil-run-yosys", "mlir::ModuleOp"> {
  let summary = "Hand the rtlil.modules to Yosys, run a script, read the result back";
  let description = [{
    Translates every `rtlil.module` in the top-level module into an in-process
    `RTLIL::Design`, runs `script` over it with Yosys' own pass manager, and
    replaces the `rtlil.module`s with the result. No file is written and the
    `yosys` binary is never spawned.

    The design is replaced rather than updated: a Yosys script adds, deletes,
    renames and re-parameterises modules, so there is no meaningful incremental
    mapping back onto the ops that went in. Names and source locations are what
    survive -- `\`-prefixed identifiers are carried through `opt`, `techmap` and
    `abc` untouched, and locations travel as RTLIL `src` attributes.

    Anything in the top-level module that is not an `rtlil.module` is left
    alone, so a partially converted design still works.

    Yosys keeps global state and is not thread safe, so this pass serialises
    itself against any other instance.
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
