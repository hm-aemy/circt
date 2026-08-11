#ifndef CIRCT_CONVERSION_RTLILCOMMON_H
#define CIRCT_CONVERSION_RTLILCOMMON_H

#include "circt/Dialect/HW/HWTypes.h"
#include "circt/Dialect/RTLIL/RTLIL.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "circt/Dialect/Seq/SeqTypes.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/IR/Value.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"
#include <atomic>
#include <string>

namespace circt::rtlil {

inline static std::string asOperandRaw(const mlir::Value v) {
  std::string result;
  llvm::raw_string_ostream os(result);
  v.printAsOperand(os, {});
  return result;
}

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
  /// Supplies the number in auto-generated `$<n>` RTLIL names. Monotonic across
  /// the whole conversion, which is stronger than RTLIL needs -- names are
  /// unique per module -- but costs nothing and keeps the patterns independent
  /// of the order the conversion driver happens to visit modules in.
  std::atomic<unsigned int> nameCtr = 0;
  auto lock() { return std::lock_guard<std::recursive_mutex>(l); }

private:
  std::recursive_mutex l;
};

class RTLILTypeConverter : public mlir::TypeConverter {

  class RTLILSignatureConversion : public SignatureConversion {

  public:
    RTLILSignatureConversion(int n);
  };

  static std::optional<mlir::Type> convertInteger(mlir::IntegerType t);

  static std::optional<mlir::Type> convertInt(circt::hw::IntType t);

  static std::optional<mlir::Type> convertClock(circt::seq::ClockType t);

public:
  /// Takes the shared context so that materialized wires can be given a unique
  /// auto-generated name up front. Naming them from `printAsOperand` instead --
  /// as this used to -- restarts numbering on a detached value, and two wires
  /// that end up with the same name make Yosys abort the process when the
  /// design is exported.
  explicit RTLILTypeConverter(ConversionPatternContext &rtlilContext);
  void convertSignature() {}
};

template <typename T>
struct ConversionPatternBase : public OpConversionPattern<T> {
private:
  using Super = OpConversionPattern<T>;

protected:
  rtlil::ConversionPatternContext &rtlilContext;

public:
  ConversionPatternBase(const TypeConverter &typeConverter,
                        rtlil::ConversionPatternContext &rtlilContext,
                        mlir::MLIRContext *context)
      : Super(typeConverter, context), rtlilContext(rtlilContext) {}

  template <typename S>
  mlir::StringAttr getStr(S &&s) const {
    return mlir::StringAttr::get(Super::getContext(), s);
  }

  mlir::IntegerAttr getInt(int32_t i) const {
    return mlir::IntegerAttr::get(
        mlir::IntegerType::get(Super::getContext(), 32), i);
  }

  template <typename S>
  rtlil::ParameterAttr getParameter(S &&key, int32_t val) const {
    return rtlil::ParameterAttr::get(Super::getContext(), getStr(key),
                                     getInt(val));
  }

  template <typename S>
  rtlil::ParameterAttr getParameter(S &&key, mlir::IntegerAttr val) const {
    return rtlil::ParameterAttr::get(Super::getContext(), getStr(key), val);
  }

  /// The RTLIL name for something the user named: the `\` sigil (RTLIL's
  /// "public") plus the name verbatim.
  ///
  /// Deliberately no uniquing suffix. RTLIL identifiers are scoped per module,
  /// so two modules may each have a `\x`, and Yosys carries `\`-prefixed names
  /// through `opt`, `techmap` and `abc` untouched while rewriting `$`-prefixed
  /// ones freely. A suffix would make every name in a design coming back from
  /// Yosys unrecognizable and would force `hierarchy -auto-top`, in exchange
  /// for nothing: a real collision is caught by the `rtlil.module` verifier,
  /// which is a better outcome than mangling every name to avoid it.
  template <typename S>
  mlir::StringAttr makeGlobal(mlir::ConversionPatternRewriter &r, S s) const {
    return r.getStringAttr(llvm::formatv("\\{0}", s));
  }

  template <typename S>
  mlir::StringAttr makeLocal(mlir::ConversionPatternRewriter &r, S s) const {
    auto res = llvm::formatv("${0}", s);
    return r.getStringAttr(res);
  }

  mlir::StringAttr
  genUniqueLocalName(mlir::ConversionPatternRewriter &r) const {
    auto v = ++rtlilContext.nameCtr;
    return r.getStringAttr(llvm::formatv("${0}", v));
  }

  mlir::StringAttr asOperand(mlir::ConversionPatternRewriter &r,
                             Value v) const {
    return r.getStringAttr(rtlil::asOperandRaw(v));
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
} // namespace circt::rtlil

#endif
