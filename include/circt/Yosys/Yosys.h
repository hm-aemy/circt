//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Setup for the in-process Yosys library. Exposes no Yosys API, so includers
// need neither the Yosys headers nor their C++20 requirement.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_YOSYS_YOSYS_H
#define CIRCT_YOSYS_YOSYS_H

#include "llvm/Support/Error.h"

#include <memory>
#include <string>

namespace circt {
namespace yosys {

/// Register Yosys' passes, set its data directory and `yosys-abc`, and attach
/// a log stream. Idempotent. Fails if the data directory cannot be found.
llvm::Error initialize();

/// Tear down Yosys. A no-op without a successful `initialize()`.
void shutdown();

/// The data directory in use, valid after `initialize()`.
std::string getDataDir();

/// The `yosys-abc` in use, valid after `initialize()`. Empty when ABC is
/// integrated or disabled.
std::string getAbcExecutable();

/// Captures Yosys' log for its lifetime, so runs stay quiet but failures can
/// be explained. Fatal errors still reach stderr. Construct only after
/// `initialize()`, as the streams are restored on destruction.
class LogCapture {
public:
  /// With `active == false`, the log is left alone.
  explicit LogCapture(bool active = true);
  ~LogCapture();
  LogCapture(const LogCapture &) = delete;
  LogCapture &operator=(const LogCapture &) = delete;

  /// What Yosys has logged since construction. Empty when inactive.
  std::string str() const;

private:
  struct Impl;
  /// Null when inactive.
  std::unique_ptr<Impl> impl;
};

} // namespace yosys
} // namespace circt

#endif // CIRCT_YOSYS_YOSYS_H
