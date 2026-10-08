//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Compiled with `-fexceptions` (see CMakeLists.txt) and deliberately free of
// MLIR and LLVM types.
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
  // Without this global, `log_cmd_error` exits the process instead of throwing.
  // Restore it afterwards, as Yosys' own `shell()` does.
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
