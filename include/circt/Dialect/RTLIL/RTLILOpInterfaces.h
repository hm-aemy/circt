//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILOPINTERFACES_H
#define CIRCT_DIALECT_RTLIL_RTLILOPINTERFACES_H

#include "circt/Support/LLVM.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/ValueRange.h"
#include "llvm/ADT/StringRef.h"

namespace circt::rtlil {

/// The `CellOpInterface` verifier, called from the generated code below.
mlir::LogicalResult verifyCellOpInterface(mlir::Operation *op);

} // namespace circt::rtlil

#include "circt/Dialect/RTLIL/RTLILOpInterfaces.h.inc"

#endif // CIRCT_DIALECT_RTLIL_RTLILOPINTERFACES_H
