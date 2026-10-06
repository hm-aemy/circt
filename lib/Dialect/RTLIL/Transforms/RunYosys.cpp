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
//
// Yosys' log is redirected while the script runs, so a passing test does not
// drown in banner text and `--verify-diagnostics` stays usable.
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
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include <algorithm>
#include <mutex>
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

/// Split the supplied script into commands and run each one.
/// This follows the loop Yosys uses and reuses `next_token`.
///
///   - Commands end with newline or a word ending with `;`. When the `;`
///   is within a word, it does not split. Needed for `abc -script +strash;dc2`.
///   - `;;` and `;;;` as shorthand for `clean` and `clean -purge`.
///   - `#` starts a comment that runs to the end of the line.
///   - A double-quoted string is one word, even if it contains a `;`.
///   - A script starting with `!` goes to the shell whole.
static SmallVector<std::string> splitScript(StringRef script) {
  SmallVector<std::string> commands;
  SmallVector<std::string> words;
  auto finish = [&] {
    if (!words.empty())
      commands.push_back(llvm::join(words, " "));
    words.clear();
  };

  std::string rest = script.str();
  std::string word = Yosys::next_token(rest, " \t\r\n", true);
  if (!word.empty() && word.front() == '!') {
    commands.push_back(script.trim().str());
    return commands;
  }

  while (!word.empty()) {
    if (word.front() == '#') {
      rest.erase(0, std::min(rest.find_first_of("\r\n"), rest.size()));
    } else if (word.back() == ';') {
      size_t semicolons = 0;
      while (!word.empty() && word.back() == ';') {
        word.pop_back();
        ++semicolons;
      }
      if (!word.empty())
        words.push_back(word);
      finish();
      if (semicolons == 2)
        commands.push_back("clean");
      else if (semicolons == 3)
        commands.push_back("clean -purge");
    } else {
      words.push_back(word);
    }

    size_t next = rest.find_first_not_of(" \t");
    if (next != std::string::npos && (rest[next] == '\r' || rest[next] == '\n'))
      finish();
    word = Yosys::next_token(rest, " \t\r\n", true);
  }
  finish();
  return commands;
}

void RunYosysPass::runOnOperation() {
  mlir::ModuleOp module = getOperation();

  // Ensure that all invocations run serially.
  static std::mutex yosysMutex;
  std::lock_guard<std::mutex> guard(yosysMutex);

  if (auto error = circt::yosys::initialize()) {
    module.emitError("failed to initialize Yosys: ")
        << llvm::toString(std::move(error));
    return signalPassFailure();
  }

  // Only process `rtlil.module`s.
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

  // Collect error messages from caught failures in `YosysScript.cpp`.
  circt::yosys::LogCapture capture(quiet);
  std::string failedCommand, error;
  if (!circt::rtlil::detail::runYosysScript(commands, &design, failedCommand,
                                            error)) {
    auto diag = module.emitError("Yosys command '")
                << failedCommand << "' failed: " << error;
    if (std::string log = capture.str(); !log.empty())
      diag.attachNote() << "Yosys log:\n" << log;
    return signalPassFailure();
  }

  // The design is now the source of truth. Yosys may have renamed, removed
  // or added modules, so drop the exported ones.
  for (auto stale : exported)
    stale.erase();

  // Import all modules from the design. A failure leaves the IR without them,
  // which is fine since the pass fails and the IR is discarded.
  if (failed(circt::rtlil::importRTLIL(&design, module)))
    return signalPassFailure();
}

#endif // CIRCT_YOSYS_LIB_ENABLED
