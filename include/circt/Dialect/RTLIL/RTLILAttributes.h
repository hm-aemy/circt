//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILATTRIBUTES_H
#define CIRCT_DIALECT_RTLIL_RTLILATTRIBUTES_H

#include "circt/Dialect/RTLIL/RTLILDialect.h"
#include "circt/Dialect/RTLIL/RTLILEnums.h"
#include "circt/Support/LLVM.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

#include <optional>
#include <tuple>

#define GET_ATTRDEF_CLASSES
#include "circt/Dialect/RTLIL/RTLILAttributes.h.inc"

namespace circt::rtlil {

/// Builds a `#rtlil.param` array from `(name, width, value)` triples. Used by
/// the generated code of `ParamArrayAttr` and the typed cell ops.
mlir::ArrayAttr createParamArrayAttr(
    mlir::MLIRContext *context,
    llvm::ArrayRef<std::tuple<llvm::StringRef, unsigned, uint64_t>> &&r);

} // namespace circt::rtlil

#endif // CIRCT_DIALECT_RTLIL_RTLILATTRIBUTES_H
