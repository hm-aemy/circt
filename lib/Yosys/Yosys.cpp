//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Owns Yosys' global state: setup, data paths and log streams. Callers use
// these helpers instead of including the C++20 Yosys headers.
//
//===----------------------------------------------------------------------===//

#include "circt/Yosys/Yosys.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <sstream>

// Yosys headers last; everything stays explicitly `Yosys::`-qualified.
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

/// Directory of the running executable, or empty if unknown.
static std::string getExecutableDir() {
  // A library has no `argv0`, so pass an address inside it for the fallback.
  void *addr = (void *)(intptr_t)getExecutableDir;
  std::string exe = llvm::sys::fs::getMainExecutable("", addr);
  if (exe.empty())
    return {};
  return std::string(llvm::sys::path::parent_path(exe));
}

/// Locate Yosys' data directory, read by its passes at runtime.
static std::string resolveDataDir() {
  // Prefer `../share/yosys`, so an installed binary does not use the build
  // tree.
  if (std::string exeDir = getExecutableDir(); !exeDir.empty()) {
    llvm::SmallString<128> candidate(exeDir);
    llvm::sys::path::append(candidate, "..", "share", "yosys");
    llvm::sys::path::remove_dots(candidate, /*remove_dot_dot=*/true);
    if (llvm::sys::fs::is_directory(candidate))
      return std::string(candidate);
  }

  // Fall back to the build-time directory, e.g. for unit tests.
  if (llvm::sys::fs::is_directory(CIRCT_YOSYS_DATADIR))
    return CIRCT_YOSYS_DATADIR;

  return {};
}

/// Locate `yosys-abc` next to the executable or at its build-time path.
static std::string resolveAbcExecutable() {
  if (std::string exeDir = getExecutableDir(); !exeDir.empty()) {
    llvm::SmallString<128> candidate(exeDir);
    llvm::sys::path::append(candidate, "yosys-abc");
    if (llvm::sys::fs::can_execute(candidate))
      return std::string(candidate);
  }

  // Empty when ABC is integrated or disabled.
  if (llvm::sys::fs::can_execute(CIRCT_YOSYS_ABC_EXECUTABLE))
    return CIRCT_YOSYS_ABC_EXECUTABLE;

  return {};
}

llvm::Error yosys::initialize() {
  auto &state = getState();
  if (state.initialized)
    return llvm::Error::success();

  std::string dataDir = resolveDataDir();
  if (dataDir.empty())
    return llvm::createStringError(
        llvm::inconvertibleErrorCode(),
        "cannot find the Yosys data directory; looked in '../share/yosys' "
        "next to the executable and in '%s'",
        CIRCT_YOSYS_DATADIR);
  std::string abcExecutable = resolveAbcExecutable();

  // Set before `yosys_setup()`, which keeps them if set. The share dir is used
  // as a prefix and needs the trailing separator.
  Yosys::yosys_share_dirname = dataDir + "/";
  Yosys::yosys_abc_executable = abcExecutable;

  Yosys::yosys_setup();

  // Send Yosys' log to stderr; without a stream only fatal errors get through.
  Yosys::log_streams.push_back(&std::cerr);
  Yosys::log_error_stderr = true;

  // `log_error()` ends in `_Exit(1)`, so report the error from the only hook
  // that runs first. Flush explicitly, as `_Exit` skips destructors.
  Yosys::log_error_atexit = []() {
    llvm::errs() << "error: Yosys aborted: " << Yosys::log_last_error << "\n"
                 << "note: Yosys exits the process on fatal errors\n";
    llvm::errs().flush();
  };

  state.initialized = true;
  state.dataDir = std::move(dataDir);
  state.abcExecutable = std::move(abcExecutable);
  return llvm::Error::success();
}

void yosys::shutdown() {
  auto &state = getState();
  if (!state.initialized)
    return;

  Yosys::yosys_shutdown();
  Yosys::log_streams.clear();
  state = YosysState{};
}

std::string yosys::getDataDir() { return getState().dataDir; }

std::string yosys::getAbcExecutable() { return getState().abcExecutable; }

// Here rather than in each caller: it swaps the global `log_streams`, and its
// state stays hidden from code built without Yosys headers.
struct yosys::LogCapture::Impl {
  std::ostringstream buffer;
  std::vector<std::ostream *> saved;
};

yosys::LogCapture::LogCapture(bool active) {
  if (!active)
    return;
  impl = std::make_unique<Impl>();
  impl->saved = Yosys::log_streams;
  Yosys::log_streams.clear();
  Yosys::log_streams.push_back(&impl->buffer);
}

yosys::LogCapture::~LogCapture() {
  if (impl)
    Yosys::log_streams = impl->saved;
}

std::string yosys::LogCapture::str() const {
  return impl ? impl->buffer.str() : std::string();
}
