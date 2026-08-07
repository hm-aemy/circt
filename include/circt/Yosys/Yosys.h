//===- Yosys.h - Embedding the Yosys synthesis library ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Setup and teardown for the in-process Yosys library. Yosys is linked into
// CIRCT rather than driven as a subprocess, so a design can be handed to Yosys
// passes and read back without ever touching a file.
//
// This header deliberately exposes none of Yosys' own API, so that including it
// costs nothing. Code that does want the Yosys API includes `kernel/rtlil.h` and
// friends directly, as `tools/circt-yosys` does; the only thing those headers
// still ask of the build is C++20, which
// `cmake/modules/YosysCompilerOptions.cmake` arranges.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_YOSYS_YOSYS_H
#define CIRCT_YOSYS_YOSYS_H

#include "llvm/Support/Error.h"

#include <string>

namespace circt {
namespace yosys {

/// Initialize the Yosys library: register its built-in passes, point it at its
/// data directory and `yosys-abc`, and attach a log stream. Idempotent, and
/// safe to call from any binary regardless of where it lives.
///
/// Yosys resolves both its data directory and `yosys-abc` relative to the
/// running executable, which holds for a CIRCT tool in `bin/` but not for a
/// binary elsewhere in the build tree. Both paths are therefore resolved here
/// and assigned before `yosys_setup()`, which leaves them alone when they are
/// already set.
///
/// Returns an error if the data directory cannot be located.
llvm::Error initialize();

/// Tear down the Yosys library. Calling this without a preceding successful
/// `initialize()` is a no-op.
void shutdown();

/// The data directory Yosys is using, valid after `initialize()`. Mainly useful
/// for diagnostics and tests.
std::string getDataDir();

/// The `yosys-abc` binary Yosys is using, valid after `initialize()`. Empty
/// when Yosys was built with an integrated or disabled ABC.
std::string getAbcExecutable();

} // namespace yosys
} // namespace circt

#endif // CIRCT_YOSYS_YOSYS_H
