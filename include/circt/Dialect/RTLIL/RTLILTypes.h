//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILTYPES_H
#define CIRCT_DIALECT_RTLIL_RTLILTYPES_H

#include "circt/Dialect/RTLIL/RTLILDialect.h"
#include "circt/Support/LLVM.h"
#include "mlir/IR/BuiltinAttributeInterfaces.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Types.h"
#include "llvm/ADT/StringRef.h"

#define GET_TYPEDEF_CLASSES
#include "circt/Dialect/RTLIL/RTLILTypes.h.inc"

namespace circt::rtlil {

/// Whether `name` is a valid RTLIL identifier: non-empty, starting with `\`
/// (public) or `$` (generated), and without spaces or control characters.
/// Yosys aborts on an invalid name, so the dialect has to reject it first.
bool isValidIdentifier(llvm::StringRef name);

} // namespace circt::rtlil

#endif // CIRCT_DIALECT_RTLIL_RTLILTYPES_H
