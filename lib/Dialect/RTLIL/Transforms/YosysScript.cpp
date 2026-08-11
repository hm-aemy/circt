//===- YosysScript.cpp - Running a Yosys script, catchably ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Compiled with `-fexceptions` (see this directory's CMakeLists.txt) and
// deliberately free of MLIR and LLVM types -- see `YosysScript.h` for why.
//
//===----------------------------------------------------------------------===//

#include "YosysScript.h"

#include <ostream>
#include <sstream>
#include <vector>

#include "kernel/rtlil.h"
#include "kernel/yosys.h"

namespace {
/// Redirects Yosys' log for the duration of a scope, keeping what it wrote so a
/// failure can quote it. Without this every run dumps Yosys' banner and per-pass
/// headers onto stderr, which makes `--verify-diagnostics` tests unreadable.
class LogCapture {
public:
  explicit LogCapture(bool active) : active(active) {
    if (!active)
      return;
    saved = Yosys::log_streams;
    Yosys::log_streams.clear();
    Yosys::log_streams.push_back(&buffer);
  }
  ~LogCapture() {
    if (active)
      Yosys::log_streams = saved;
  }

  std::string str() const { return buffer.str(); }

private:
  bool active;
  std::ostringstream buffer;
  std::vector<std::ostream *> saved;
};
} // namespace

bool circt::rtlil::detail::runYosysScript(
    const std::vector<std::string> &commands, Yosys::RTLIL::Design *design,
    bool captureLog, std::string &failedCommand, std::string &error,
    std::string &log) {
  LogCapture capture(captureLog);

  // `log_cmd_error` only throws when this is set; otherwise it falls through to
  // `log_error` and the process ends. It is a global, and Yosys' own `shell()`
  // sets and clears it exactly this way, so restore it on the way out.
  bool savedThrow = Yosys::log_cmd_error_throw;
  Yosys::log_cmd_error_throw = true;

  bool ok = true;
  for (const std::string &command : commands) {
    try {
      Yosys::run_pass(command, design);
    } catch (const Yosys::log_cmd_error_exception &) {
      failedCommand = command;
      error = Yosys::log_last_error;
      ok = false;
      break;
    }
  }

  Yosys::log_cmd_error_throw = savedThrow;
  if (captureLog)
    log = capture.str();
  return ok;
}
