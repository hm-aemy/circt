//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Dialect/RTLIL/RTLILAttributes.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace circt;
using namespace circt::rtlil;
using namespace mlir;

#define GET_ATTRDEF_CLASSES
#include "circt/Dialect/RTLIL/RTLILAttributes.cpp.inc"

void RTLILDialect::registerAttributes() {
  addAttributes<
#define GET_ATTRDEF_LIST
#include "circt/Dialect/RTLIL/RTLILAttributes.cpp.inc"
      >();
}

ArrayAttr circt::rtlil::createParamArrayAttr(
    mlir::MLIRContext *context,
    llvm::ArrayRef<std::tuple<llvm::StringRef, unsigned, uint64_t>> &&r) {
  llvm::SmallVector<Attribute, 5> v;
  for (auto &&[name, width, val] : r) {
    v.emplace_back(ParameterAttr::get(context, name, width, val));
  }
  return ArrayAttr::get(context, v);
}

//===----------------------------------------------------------------------===//
// ParameterAttr
//===----------------------------------------------------------------------===//

LogicalResult
ParameterAttr::verify(llvm::function_ref<InFlightDiagnostic()> emitError,
                      StringAttr name, Attribute value,
                      std::optional<uint16_t> flags) {
  if (!name || !isValidIdentifier(name.getValue()))
    return emitError() << "parameter name '" << (name ? name.getValue() : "")
                       << "' is not a valid RTLIL identifier";

  // An RTLIL::Const is a string or a bit vector. `IntegerAttr` is the
  // shorthand for a bit vector that fits in 64 bits and is fully defined;
  // `ConstAttr` (an array of StateEnumAttr) is the general form.
  if (isa<StringAttr, IntegerAttr>(value))
    return success();
  if (auto array = dyn_cast<ArrayAttr>(value)) {
    if (llvm::all_of(array, llvm::IsaPred<StateEnumAttr>))
      return success();
    return emitError() << "parameter '" << name.getValue()
                       << "' has an array value that is not a bit vector";
  }
  return emitError() << "parameter '" << name.getValue()
                     << "' must be an integer, a bit vector or a string";
}
