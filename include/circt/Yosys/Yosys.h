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
// This header deliberately exposes none of Yosys' own API. The Yosys headers
// require exceptions, RTTI, and C++20, and their `_YOSYS_` namespace macros do
// not mix well with LLVM headers, so they stay confined to `Yosys.cpp`.
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
/// running executable, which does not hold for a CIRCT binary in general, and
/// it fails *silently* when they are wrong: setup still succeeds and the
/// process dies with exit code 1 and no diagnostic as soon as a pass reads a
/// technology library. Both paths are therefore set explicitly here.
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
