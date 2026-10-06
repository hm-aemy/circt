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

  static std::optional<Type> convertInteger(IntegerType t);

  static std::optional<Type> convertInt(hw::IntType t);

  static std::optional<Type> convertClock(seq::ClockType t);

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
  template <typename S>
  StringAttr makeGlobal(ConversionPatternRewriter &r, S s) const {
    return r.getStringAttr(llvm::formatv("\\{0}", s));
  }

  StringAttr genUniqueLocalName(ConversionPatternRewriter &r) const {
    auto v = ++rtlilContext.nameCtr;
    return r.getStringAttr(llvm::formatv("${0}", v));
  }

  /// A new internal wire with a unique `$<n>` name and the converted type of
  /// `v`, or null if that type has no RTLIL form.
  rtlil::WireOp genLocalWire(Location l, Value v,
                             ConversionPatternRewriter &rewriter) const {
    auto t = Super::getTypeConverter()->template convertType<rtlil::MValueType>(
        v.getType());
    if (!t)
      return {};
    return createWire(rewriter, l, t, genUniqueLocalName(rewriter).getValue());
  }
};
} // namespace circt::HWToRTLIL

#endif // CONVERSION_HWTORTLIL_HWTORTLILINTERNALS_H
