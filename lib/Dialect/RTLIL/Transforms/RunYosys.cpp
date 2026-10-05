//===- RunYosys.cpp - Drive Yosys over the RTLIL dialect ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The payoff of linking libyosys: export the RTLIL dialect to a real
// `RTLIL::Design`, run a Yosys script over it, and read the result back, all in
// one address space with no file in between.
//
// Most of the work here is defending against how Yosys reports failure. In
// descending order of how much it buys:
//
//   1. `exportRTLIL` validates the IR before creating any Yosys object, so the
//      common failures are ordinary MLIR diagnostics.
//   2. `log_cmd_error`, from an unknown command or a bad argument, can be
//      caught, but only if `log_cmd_error_throw` is set first. That is what
//      `YosysScript.cpp` does, and why it is compiled with exceptions.
//   3. Everything else goes through `log_error`, which ends in `_Exit(1)`.
//      `circt::yosys::initialize()` installs a `log_error_atexit` hook so that
//      at least a message gets out.
//   4. Yosys' log is redirected while the script runs, so a passing test does
//      not drown in banner text and `--verify-diagnostics` stays usable.
//
// Without libyosys the pass is still registered, but only reports an error.
//
//===----------------------------------------------------------------------===//

#include "circt/Dialect/RTLIL/RTLILPasses.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#ifdef CIRCT_YOSYS_LIB_ENABLED
#include "circt/Conversion/ExportRTLIL.h"
#include "circt/Conversion/ImportRTLIL.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Yosys/Yosys.h"

#include "YosysScript.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include <mutex>
#include <sstream>
#include <string>

// Yosys headers last; everything stays explicitly `Yosys::`-qualified.
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#endif

namespace circt {
namespace rtlil {
#define GEN_PASS_DEF_RUNYOSYS
#include "circt/Dialect/RTLIL/RTLILPasses.h.inc"
} // namespace rtlil
} // namespace circt

using namespace circt;
using namespace mlir;

namespace {
struct RunYosysPass : public circt::rtlil::impl::RunYosysBase<RunYosysPass> {
  using circt::rtlil::impl::RunYosysBase<RunYosysPass>::RunYosysBase;
  void runOnOperation() override;
};
} // namespace

#ifndef CIRCT_YOSYS_LIB_ENABLED

void RunYosysPass::runOnOperation() {
  getOperation().emitError(
      "rtlil-run-yosys is not available: CIRCT was built without libyosys");
  signalPassFailure();
}

#else

/// Split a script into individual commands. Yosys' own `run_pass` takes one
/// command at a time, and splitting here means a failing command can be named
/// in the diagnostic.
///
/// A `;` inside a `+`-prefixed argument is not a separator: that is how `abc`
/// spells a sub-script, as in `abc -script +strash;dc2`, and Yosys hands the
/// whole token to `abc` untouched. Newlines separate commands unconditionally.
static SmallVector<std::string> splitScript(StringRef script) {
  SmallVector<std::string> commands;

  auto push = [&](StringRef piece) {
    if (StringRef trimmed = piece.trim(); !trimmed.empty())
      commands.push_back(trimmed.str());
  };

  size_t start = 0;
  // Whether the token being scanned started with `+`, in which case a `;`
  // belongs to it rather than ending the command.
  bool inPlusToken = false;
  bool atTokenStart = true;
  for (size_t i = 0, e = script.size(); i != e; ++i) {
    char c = script[i];
    if (c == '\n' || (c == ';' && !inPlusToken)) {
      push(script.slice(start, i));
      start = i + 1;
      inPlusToken = false;
      atTokenStart = true;
      continue;
    }
    if (c == ' ' || c == '\t') {
      inPlusToken = false;
      atTokenStart = true;
      continue;
    }
    if (atTokenStart) {
      inPlusToken = c == '+';
      atTokenStart = false;
    }
  }
  push(script.substr(start));
  return commands;
}

void RunYosysPass::runOnOperation() {
  mlir::ModuleOp module = getOperation();

  // Yosys is a single global design database: `yosys_design`, `autoidx`, the
  // pass registry and the `IdString` intern table are all globals, and
  // `IdString::insert` asserts that no other thread is running. Anchoring at
  // the top-level module already means the pass manager runs this serially, but
  // the mutex also covers nested pipelines and in-process embedders, where the
  // failure mode would be an abort rather than a diagnostic.
  static std::mutex yosysMutex;
  std::lock_guard<std::mutex> guard(yosysMutex);

  // Idempotent, but not itself thread safe, hence inside the lock.
  if (auto error = circt::yosys::initialize()) {
    module.emitError("failed to initialize Yosys: ")
        << llvm::toString(std::move(error));
    return signalPassFailure();
  }

  // Only the `rtlil.module`s go to Yosys, which is what lets this pass run on a
  // partly converted design: an `hw.module` `convert-hw-to-rtlil` could not
  // handle stays in the file and is still there when the import writes back.
  Yosys::RTLIL::Design design;
  auto exported = llvm::to_vector(module.getOps<circt::rtlil::ModuleOp>());
  if (failed(circt::rtlil::exportRTLIL(exported, &design)))
    return signalPassFailure();

  std::vector<std::string> commands;
  if (runHierarchy)
    commands.push_back(topModule.empty()
                           ? "hierarchy -check -auto-top"
                           : "hierarchy -check -top " + topModule);
  for (const std::string &command : splitScript(script))
    commands.push_back(command);

  // The catch lives in `YosysScript.cpp`, which is the only file here compiled
  // with exceptions; see `YosysScript.h`. Silencing the log is this side's job,
  // so that a failure can still quote what Yosys had to say.
  circt::yosys::LogCapture capture(quiet);
  std::string failedCommand, error;
  if (!circt::rtlil::detail::runYosysScript(commands, &design, failedCommand,
                                            error)) {
    auto diag = module.emitError("yosys command '")
                << failedCommand << "' failed: " << error;
    if (std::string log = capture.str(); !log.empty())
      diag.attachNote() << "yosys log:\n" << log;
    return signalPassFailure();
  }

  // Replace rather than merge: after a script there is no correspondence left
  // between the modules that went in and the ones that came out. Only the
  // `rtlil.module`s are erased, so anything else in the top-level module
  // survives, an unconverted `hw.module` for example.
  for (auto stale :
       llvm::make_early_inc_range(module.getOps<circt::rtlil::ModuleOp>()))
    stale.erase();

  if (failed(circt::rtlil::importRTLIL(&design, module)))
    return signalPassFailure();
}

#endif // CIRCT_YOSYS_LIB_ENABLED
