//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILPASSES_H
#define CIRCT_DIALECT_RTLIL_RTLILPASSES_H

#include <memory>

#include "circt/Dialect/RTLIL/RTLILDialect.h"
#include "mlir/Pass/Pass.h"

namespace circt {
namespace rtlil {

/// Generate the code for registering passes.
#define GEN_PASS_DECL
#define GEN_PASS_REGISTRATION
#include "circt/Dialect/RTLIL/RTLILPasses.h.inc"

} // namespace rtlil
} // namespace circt

#endif // CIRCT_DIALECT_RTLIL_RTLILPASSES_H
