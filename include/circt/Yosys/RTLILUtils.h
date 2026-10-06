//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Helpers shared by the RTLIL import and export. Unlike `Yosys.h`, this header
// includes the Yosys headers, so includers need C++20.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_YOSYS_RTLILUTILS_H
#define CIRCT_YOSYS_RTLILUTILS_H

#include "llvm/ADT/StringRef.h"

#include <string_view>

#include "kernel/rtlil.h"

namespace circt {
namespace yosys {

/// The attribute holding an object's full MLIR location, written by the export
/// next to `src` and preferred by the import.
inline constexpr llvm::StringLiteral circtLocAttrName = "\\circt.loc";

/// An `RTLIL::IdString` for `name`, which keeps its `\` or `$` sigil.
inline Yosys::RTLIL::IdString toIdString(llvm::StringRef name) {
  return Yosys::RTLIL::IdString(std::string_view(name.data(), name.size()));
}

/// An `RTLIL::IdString` as a `StringRef`, sigil included. Valid while the
/// interned string is alive, so only use it for names the design holds.
inline llvm::StringRef toStringRef(const Yosys::RTLIL::IdString &name) {
  return llvm::StringRef(name.c_str());
}

} // namespace yosys
} // namespace circt

#endif // CIRCT_YOSYS_RTLILUTILS_H
