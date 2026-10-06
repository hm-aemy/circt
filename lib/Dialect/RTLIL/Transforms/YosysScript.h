//===- YosysScript.h - Running a Yosys script, catchably --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Yosys reports command errors by throwing `log_cmd_error_exception`. Catching
// it needs exceptions, which `AddLLVM.cmake` couples with RTTI, and an RTTI TU
// deriving from `mlir::Pass` does not link. So the `catch` lives in
// `YosysScript.cpp`, built with `-fexceptions` and free of MLIR types.
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

/// Run `commands` over `design` in order. Returns false on a command error and
/// sets `failedCommand` and `error`. Fatal errors (`log_error`) exit the
/// process. The caller silences the log with `circt::yosys::LogCapture`.
bool runYosysScript(const std::vector<std::string> &commands,
                    Yosys::RTLIL::Design *design, std::string &failedCommand,
                    std::string &error);

} // namespace detail
} // namespace rtlil
} // namespace circt

#endif // CIRCT_DIALECT_RTLIL_TRANSFORMS_YOSYSSCRIPT_H
