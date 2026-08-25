#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/MLIRContext.h"
#include "llvm/ADT/StringRef.h"

using namespace mlir;

namespace circt::rtlil {
bool isMValueType(mlir::Type type) { return isa<MValueType>(type); }

unsigned MValueType::getBitWidth() {
  return cast<IntegerAttr>(getWidth()).getValue().getZExtValue();
}

MValueType MValueType::get(mlir::MLIRContext *context, unsigned width) {
  return get(context, IntegerAttr::get(IntegerType::get(context, 32), width));
}

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
