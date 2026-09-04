//===- YosysScript.cpp - Running a Yosys script, catchably ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Compiled with `-fexceptions` (see this directory's CMakeLists.txt) and
// deliberately free of MLIR and LLVM types; see `YosysScript.h` for why.
//
//===----------------------------------------------------------------------===//

#include "YosysScript.h"

#include <string>
#include <vector>

#include "kernel/rtlil.h"
#include "kernel/yosys.h"

bool circt::rtlil::detail::runYosysScript(
    const std::vector<std::string> &commands, Yosys::RTLIL::Design *design,
    std::string &failedCommand, std::string &error) {
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
  return ok;
}
