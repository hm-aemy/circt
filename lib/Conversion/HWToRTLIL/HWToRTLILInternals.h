//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CONVERSION_HWTORTLIL_HWTORTLILINTERNALS_H
#define CONVERSION_HWTORTLIL_HWTORTLILINTERNALS_H

#include "circt/Dialect/HW/HWTypes.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/Seq/SeqTypes.h"
#include "circt/Support/LLVM.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/IR/Value.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/Support/FormatVariadic.h"

namespace circt::HWToRTLIL {

template <typename Sym>
inline static Operation *lookupSymbolWalkTables(Operation *from,
                                                const Sym &sym) {
  auto *op = from;
  Operation *result = nullptr;
  while (op) {
    if ((result = SymbolTable::lookupNearestSymbolFrom(op, sym)))
      break;
    op = op->getParentOp();
  }
  return result;
}

template <typename OpType, typename Sym>
inline static OpType lookupSymbolWalkTables(Operation *from, const Sym &sym) {
  auto *op = from;
  OpType result = nullptr;
  while (op) {
    if ((result = SymbolTable::lookupNearestSymbolFrom<OpType>(op, sym)))
      break;
    op = op->getParentOp();
  }
  return result;
}

struct ConversionPatternContext {
  /// Counter for auto-generated `$<n>` names, global across all modules.
  unsigned nameCtr = 0;
};

class RTLILTypeConverter : public TypeConverter {

  static std::optional<Type> convertInteger(IntegerType type);

  static std::optional<Type> convertInt(hw::IntType type);

  static std::optional<Type> convertClock(seq::ClockType type);

public:
  RTLILTypeConverter();
};

/// Create a wire named `name`. Without a `direction` it is an internal wire.
inline rtlil::WireOp createWire(OpBuilder &builder, Location loc,
                                rtlil::MValueType type, StringRef name,
                                rtlil::PortDirectionAttr direction = {},
                                uint32_t portId = 0) {
  return rtlil::WireOp::create(builder, loc, type, name,
                               rtlil::Signedness::Unsigned, portId,
                               /*start_offset=*/0, direction);
}

template <typename T>
struct ConversionPatternBase : public OpConversionPattern<T> {
private:
  using Super = OpConversionPattern<T>;

protected:
  ConversionPatternContext &rtlilContext;

public:
  ConversionPatternBase(const TypeConverter &typeConverter,
                        ConversionPatternContext &rtlilContext,
                        MLIRContext *context)
      : Super(typeConverter, context), rtlilContext(rtlilContext) {}

  /// The public RTLIL name: `\` plus the name verbatim. No uniquing suffix:
  /// names are scoped per module, Yosys keeps `\` names intact, and the
  /// `rtlil.module` verifier catches real collisions.
  template <typename NameT>
  StringAttr makeGlobal(ConversionPatternRewriter &rewriter, NameT name) const {
    return rewriter.getStringAttr(llvm::formatv("\\{0}", name));
  }

  StringAttr genUniqueLocalName(ConversionPatternRewriter &rewriter) const {
    unsigned id = ++rtlilContext.nameCtr;
    return rewriter.getStringAttr(llvm::formatv("${0}", id));
  }

  /// A new internal wire with a unique `$<n>` name and the converted type of
  /// `value`, or null if that type has no RTLIL form.
  rtlil::WireOp genLocalWire(Location loc, Value value,
                             ConversionPatternRewriter &rewriter) const {
    auto type =
        Super::getTypeConverter()->template convertType<rtlil::MValueType>(
            value.getType());
    if (!type)
      return {};
    return createWire(rewriter, loc, type,
                      genUniqueLocalName(rewriter).getValue());
  }
};
} // namespace circt::HWToRTLIL

#endif // CONVERSION_HWTORTLIL_HWTORTLILINTERNALS_H
