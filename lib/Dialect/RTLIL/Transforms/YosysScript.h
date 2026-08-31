//===- YosysScript.h - Running a Yosys script, catchably --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The one part of the run-yosys pass that needs exceptions.
//
// Yosys reports a recoverable command error, an unknown command or a bad
// argument, by throwing `log_cmd_error_exception`, and catching it is the only
// way to turn a bad script into a diagnostic rather than a process exit. But
// enabling exceptions on a translation unit also enables RTTI (`AddLLVM.cmake`
// couples them), and a TU with RTTI that derives from the non-RTTI
// `mlir::Pass` does not link.
//
// So the `catch` lives in `YosysScript.cpp`, which gets `-fexceptions` on its
// own and mentions no MLIR type at all; the pass keeps CIRCT's normal flags and
// calls through this header.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_TRANSFORMS_YOSYSSCRIPT_H
#define CIRCT_DIALECT_RTLIL_TRANSFORMS_YOSYSSCRIPT_H

#include <string>
#include <vector>

namespace Yosys {
namespace RTLIL {
struct Design;
} // namespace RTLIL
} // namespace Yosys

namespace circt {
namespace rtlil {
namespace detail {

/// Run `commands` over `design` in order.
///
/// Returns true on success. On a recoverable Yosys error returns false and sets
/// `failedCommand` and `error`. An *unrecoverable* Yosys error cannot be
/// reported here at all: `log_error()` ends in `_Exit(1)`, and the only thing
/// that gets a message out is the `log_error_atexit` hook that
/// `circt::yosys::initialize()` installs.
///
/// `captureLog` redirects Yosys' log output for the duration and returns it in
/// `log`, so an ordinary run stays quiet and a failure can still be explained.
bool runYosysScript(const std::vector<std::string> &commands,
                    Yosys::RTLIL::Design *design, bool captureLog,
                    std::string &failedCommand, std::string &error,
                    std::string &log);

} // namespace detail
} // namespace rtlil
} // namespace circt

#endif // CIRCT_DIALECT_RTLIL_TRANSFORMS_YOSYSSCRIPT_H
