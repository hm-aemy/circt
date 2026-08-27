//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/MLIRContext.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;

#define GET_TYPEDEF_CLASSES
#include "circt/Dialect/RTLIL/RTLILTypes.cpp.inc"

void circt::rtlil::RTLILDialect::registerTypes() {
  addTypes<
#define GET_TYPEDEF_LIST
#include "circt/Dialect/RTLIL/RTLILTypes.cpp.inc"
      >();
}

namespace circt::rtlil {
bool isValidIdentifier(llvm::StringRef name) {
  if (name.empty() || (name.front() != '\\' && name.front() != '$'))
    return false;
  // Yosys rejects any byte at or below a space, which covers both control
  // characters and the space itself.
  return llvm::none_of(name, [](char c) {
    return static_cast<unsigned char>(c) <= static_cast<unsigned char>(' ');
  });
}
} // namespace circt::rtlil
