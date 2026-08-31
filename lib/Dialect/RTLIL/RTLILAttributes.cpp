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
#include "llvm/ADT/STLExtras.h"
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
// ConstAttr
//===----------------------------------------------------------------------===//

/// The character `RTLIL::Const::as_string()` uses for a state.
static char toChar(StateEnum state) {
  switch (state) {
  case StateEnum::S0:
    return '0';
  case StateEnum::S1:
    return '1';
  case StateEnum::Sx:
    return 'x';
  case StateEnum::Sz:
    return 'z';
  case StateEnum::Sa:
    return '-';
  }
  llvm_unreachable("unhandled RTLIL state");
}

/// The state such a character stands for, or nullopt if it is not one of them.
static std::optional<StateEnum> fromChar(char c) {
  switch (c) {
  case '0':
    return StateEnum::S0;
  case '1':
    return StateEnum::S1;
  case 'x':
    return StateEnum::Sx;
  case 'z':
    return StateEnum::Sz;
  case '-':
    return StateEnum::Sa;
  default:
    return std::nullopt;
  }
}

std::string ConstAttr::getBitString() const {
  std::string result;
  result.reserve(size());
  // Stored least significant first, printed most significant first.
  for (StateEnum bit : llvm::reverse(getBits()))
    result.push_back(toChar(bit));
  return result;
}

ConstAttr ConstAttr::getFromBitString(MLIRContext *context, StringRef bits) {
  SmallVector<StateEnum> states;
  states.reserve(bits.size());
  for (char c : llvm::reverse(bits)) {
    auto state = fromChar(c);
    if (!state)
      return {};
    states.push_back(*state);
  }
  return ConstAttr::get(context, states);
}

Attribute ConstAttr::parse(AsmParser &parser, Type) {
  std::string bits;
  llvm::SMLoc loc = parser.getCurrentLocation();
  if (parser.parseLess() || parser.parseString(&bits) || parser.parseGreater())
    return {};
  auto attr = ConstAttr::getFromBitString(parser.getContext(), bits);
  if (!attr)
    parser.emitError(loc,
                     "expected a bit string of '0', '1', 'x', 'z' and '-'");
  return attr;
}

void ConstAttr::print(AsmPrinter &printer) const {
  printer << "<\"" << getBitString() << "\">";
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
  // `ConstAttr` is the general form.
  if (isa<StringAttr, IntegerAttr, ConstAttr>(value))
    return success();
  return emitError() << "parameter '" << name.getValue()
                     << "' must be an integer, a bit vector or a string";
}
