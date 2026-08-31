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
//===----------------------------------------------------------------------===//

#include "circt/Conversion/ExportRTLIL.h"
#include "circt/Conversion/ImportRTLIL.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLILPasses.h"
#include "circt/Yosys/Yosys.h"

#include "YosysScript.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
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

namespace circt {
namespace rtlil {
#define GEN_PASS_DEF_RUNYOSYS
#include "circt/Dialect/RTLIL/RTLILPasses.h.inc"
} // namespace rtlil
} // namespace circt

using namespace circt;
using namespace mlir;

/// Split a script into individual commands. Yosys' own `run_pass` takes one
/// command at a time, and splitting here means a failing command can be named
/// in the diagnostic.
static SmallVector<std::string> splitScript(StringRef script) {
  SmallVector<std::string> commands;
  SmallVector<StringRef> pieces;
  script.split(pieces, ';', /*MaxSplit=*/-1, /*KeepEmpty=*/false);
  for (StringRef piece : pieces) {
    SmallVector<StringRef> lines;
    piece.split(lines, '\n', /*MaxSplit=*/-1, /*KeepEmpty=*/false);
    for (StringRef line : lines)
      if (StringRef trimmed = line.trim(); !trimmed.empty())
        commands.push_back(trimmed.str());
  }
  return commands;
}

namespace {
struct RunYosysPass : public circt::rtlil::impl::RunYosysBase<RunYosysPass> {
  using circt::rtlil::impl::RunYosysBase<RunYosysPass>::RunYosysBase;
  void runOnOperation() override;
};
} // namespace

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

  Yosys::RTLIL::Design design;
  if (failed(circt::rtlil::exportRTLIL(module, &design)))
    return signalPassFailure();

  std::vector<std::string> commands;
  if (runHierarchy)
    commands.push_back(topModule.empty()
                           ? "hierarchy -check -auto-top"
                           : "hierarchy -check -top " + topModule);
  for (const std::string &command : splitScript(script))
    commands.push_back(command);

  // The catch lives in `YosysScript.cpp`, which is the only file here compiled
  // with exceptions; see `YosysScript.h`.
  std::string failedCommand, error, log;
  if (!circt::rtlil::detail::runYosysScript(commands, &design, quiet,
                                            failedCommand, error, log)) {
    auto diag = module.emitError("yosys command '")
                << failedCommand << "' failed: " << error;
    if (!log.empty())
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
