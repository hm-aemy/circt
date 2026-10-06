//===--------------------------------------------------------------------===//
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
#include "mlir/IR/Operation.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/IR/Value.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/Support/FormatVariadic.h"
#include <atomic>

namespace circt::HWToRTLIL {

template <typename Sym>
inline static mlir::Operation *lookupSymbolWalkTables(mlir::Operation *from,
                                                      const Sym &sym) {
  auto *op = from;
  mlir::Operation *result = nullptr;
  while (op) {
    if ((result = mlir::SymbolTable::lookupNearestSymbolFrom(op, sym))) {
      break;
    };
    op = op->getParentOp();
  }
  return result;
}

template <typename OpType, typename Sym>
inline static OpType lookupSymbolWalkTables(mlir::Operation *from,
                                            const Sym &sym) {
  auto *op = from;
  OpType result = nullptr;
  while (op) {
    if ((result =
             mlir::SymbolTable::lookupNearestSymbolFrom<OpType>(op, sym))) {
      break;
    };
    op = op->getParentOp();
  }
  return result;
}

struct ConversionPatternContext {
  /// Counter for auto-generated `$<n>` names, global across all modules.
  std::atomic<unsigned int> nameCtr = 0;
};

class RTLILTypeConverter : public mlir::TypeConverter {

  static std::optional<mlir::Type> convertInteger(mlir::IntegerType t);

  static std::optional<mlir::Type> convertInt(circt::hw::IntType t);

  static std::optional<mlir::Type> convertClock(circt::seq::ClockType t);

public:
  /// Uses the shared context to give each materialized wire a unique name.
  explicit RTLILTypeConverter(ConversionPatternContext &rtlilContext);
};

template <typename T>
struct ConversionPatternBase : public OpConversionPattern<T> {
private:
  using Super = OpConversionPattern<T>;

protected:
  ConversionPatternContext &rtlilContext;

public:
  ConversionPatternBase(const TypeConverter &typeConverter,
                        ConversionPatternContext &rtlilContext,
                        mlir::MLIRContext *context)
      : Super(typeConverter, context), rtlilContext(rtlilContext) {}

  /// The public RTLIL name: `\` plus the name verbatim. No uniquing suffix:
  /// names are scoped per module, Yosys keeps `\` names intact, and the
  /// `rtlil.module` verifier catches real collisions.
  template <typename S>
  mlir::StringAttr makeGlobal(mlir::ConversionPatternRewriter &r, S s) const {
    return r.getStringAttr(llvm::formatv("\\{0}", s));
  }

  mlir::StringAttr
  genUniqueLocalName(mlir::ConversionPatternRewriter &r) const {
    auto v = ++rtlilContext.nameCtr;
    return r.getStringAttr(llvm::formatv("${0}", v));
  }

  rtlil::WireOp genLocalWire(Location l, Value v,
                             mlir::ConversionPatternRewriter &rewriter) const {
    auto t = Super::getTypeConverter()->convertType(v.getType());
    if (!t)
      return {};
    rtlil::WireOp result = Super::getTypeConverter()
                               ->materializeTargetConversion(rewriter, l, t, v)
                               .template getDefiningOp<rtlil::WireOp>();
    if (result)
      rewriter.modifyOpInPlace(
          result, [&] { result.setNameAttr(genUniqueLocalName(rewriter)); });
    return result;
  }
};
} // namespace circt::HWToRTLIL

#endif
