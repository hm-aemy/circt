//===- Yosys.cpp - Embedding the Yosys synthesis library ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Yosys/Yosys.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include <iostream>

// The Yosys headers come last and stay in this translation unit: they need
// exceptions, RTTI, and C++20 (see `YosysCompilerOptions.cmake`), and their
// `_YOSYS_` namespace macros do not mix well with LLVM headers. Everything
// below stays explicitly qualified with `Yosys::` rather than using Yosys' own
// `USING_YOSYS_NAMESPACE`.
#include "kernel/yosys.h"

using namespace circt;

namespace {
/// Whether the library has been set up, and what it was pointed at.
struct YosysState {
  bool initialized = false;
  std::string dataDir;
  std::string abcExecutable;
};
} // namespace

static YosysState &getState() {
  static YosysState state;
  return state;
}

/// Directory containing the running executable, or empty if it cannot be
/// determined.
static std::string getExecutableDir() {
  // `getMainExecutable()` queries the OS directly on the platforms that allow
  // it (`_NSGetExecutablePath`, `/proc/self/exe`, `GetModuleFileNameW`) and
  // only falls back to `argv0` and the symbol address elsewhere -- a library
  // has no `argv0`, so pass an address inside it.
  void *addr = (void *)(intptr_t)getExecutableDir;
  std::string exe = llvm::sys::fs::getMainExecutable("", addr);
  if (exe.empty())
    return {};
  return std::string(llvm::sys::path::parent_path(exe));
}

/// Locate Yosys' data directory: the technology libraries and headers its
/// passes read at runtime.
static std::string resolveDataDir() {
  // Prefer a data directory next to the running executable, which is how both
  // CIRCT's build tree and an installed CIRCT are laid out. Checking this first
  // is what keeps an installed binary from reaching back into the build tree it
  // was compiled in.
  if (std::string exeDir = getExecutableDir(); !exeDir.empty()) {
    llvm::SmallString<128> candidate(exeDir);
    llvm::sys::path::append(candidate, "..", "share", "yosys");
    llvm::sys::path::remove_dots(candidate, /*remove_dot_dot=*/true);
    if (llvm::sys::fs::is_directory(candidate))
      return std::string(candidate);
  }

  // Fall back to the directory recorded at build time. This covers binaries
  // that do not sit in `bin/` at all, such as unit tests.
  if (llvm::sys::fs::is_directory(CIRCT_YOSYS_DATADIR))
    return CIRCT_YOSYS_DATADIR;

  return {};
}

/// Locate `yosys-abc`. Yosys only ever looks for it in the running executable's
/// own directory, with no `../` fallback at all.
static std::string resolveAbcExecutable() {
  if (std::string exeDir = getExecutableDir(); !exeDir.empty()) {
    llvm::SmallString<128> candidate(exeDir);
    llvm::sys::path::append(candidate, "yosys-abc");
    if (llvm::sys::fs::can_execute(candidate))
      return std::string(candidate);
  }

  // Empty when Yosys was built with an integrated or disabled ABC, in which
  // case there is nothing to point at and `can_execute()` simply fails.
  if (llvm::sys::fs::can_execute(CIRCT_YOSYS_ABC_EXECUTABLE))
    return CIRCT_YOSYS_ABC_EXECUTABLE;

  return {};
}

llvm::Error circt::yosys::initialize() {
  auto &state = getState();
  if (state.initialized)
    return llvm::Error::success();

  std::string dataDir = resolveDataDir();
  if (dataDir.empty())
    return llvm::createStringError(
        llvm::inconvertibleErrorCode(),
        "unable to locate the Yosys data directory: looked for "
        "'../share/yosys' next to the running executable and at '%s'",
        CIRCT_YOSYS_DATADIR);
  std::string abcExecutable = resolveAbcExecutable();

  // `yosys_setup()` derives both of these from the location of the running
  // executable, which does not hold for a CIRCT binary, but it leaves them alone
  // when they are already set. `yosys_share_dirname` is used as a prefix and
  // needs the separator.
  Yosys::yosys_share_dirname = dataDir + "/";
  Yosys::yosys_abc_executable = abcExecutable;

  Yosys::yosys_setup();

  // Yosys writes its diagnostics to its own log streams. It falls back to stderr
  // for fatal errors when nothing is registered, but ordinary output would still
  // be discarded, so register a stream for the rest.
  Yosys::log_streams.push_back(&std::cerr);
  Yosys::log_error_stderr = true;

  state.initialized = true;
  state.dataDir = std::move(dataDir);
  state.abcExecutable = std::move(abcExecutable);
  return llvm::Error::success();
}

void circt::yosys::shutdown() {
  auto &state = getState();
  if (!state.initialized)
    return;

  Yosys::yosys_shutdown();
  Yosys::log_streams.clear();
  state = YosysState{};
}

std::string circt::yosys::getDataDir() { return getState().dataDir; }

std::string circt::yosys::getAbcExecutable() {
  return getState().abcExecutable;
}
