#include "circt/Conversion/RTLILCommon.h"
#include "circt/Dialect/RTLIL/RTLIL.h"
#include "circt/Support/LLVM.h"
#include "llvm/Support/FormatVariadic.h"

namespace circt::rtlil {

std::optional<mlir::Type>
RTLILTypeConverter::convertInteger(mlir::IntegerType t) {
  auto val = t.getWidth();
  if (val >= INT32_MAX) {
    return std::nullopt;
  }
  return rtlil::MValueType::get(
      t.getContext(),
      mlir::IntegerAttr::get(mlir::IntegerType::get(t.getContext(), 32), val));
}
std::optional<mlir::Type> RTLILTypeConverter::convertInt(circt::hw::IntType t) {
  auto width = cast<mlir::IntegerAttr>(t.getWidth());
  auto val = width.getInt();
  if (val >= INT32_MAX) {
    return std::nullopt;
  }
  return rtlil::MValueType::get(
      t.getContext(),
      mlir::IntegerAttr::get(mlir::IntegerType::get(t.getContext(), 32), val));
}

std::optional<mlir::Type>
RTLILTypeConverter::convertClock(circt::seq::ClockType t) {
  return rtlil::MValueType::get(
      t.getContext(),
      mlir::IntegerAttr::get(mlir::IntegerType::get(t.getContext(), 32), 1));
}

RTLILTypeConverter::RTLILTypeConverter(ConversionPatternContext &rtlilContext)
    : mlir::TypeConverter() {
  addConversion(convertInt);
  addConversion(convertInteger);
  addConversion(convertClock);
  // Materializing with no input value means "this stands for a module input",
  // which is the one case that produces a port wire. Everything else is an
  // ordinary internal wire.
  //
  // Every wire gets a unique `$<n>` name here rather than a placeholder;
  // patterns that know a better name overwrite it. `InstanceConversion` does
  // not, so without this its result wires would all share one name.
  addTargetMaterialization([&rtlilContext](mlir::OpBuilder &builder,
                                           circt::rtlil::MValueType t,
                                           mlir::ValueRange vals,
                                           mlir::Location pos) -> mlir::Value {
    bool isInput = vals.empty();
    if (vals.size() > 1)
      return {};
    auto name =
        builder.getStringAttr(llvm::formatv("${0}", ++rtlilContext.nameCtr));
    return rtlil::WireOp::create(builder, pos, t, name, 0, 0, 0, isInput, 0, 0);
  });
}

} // namespace circt::rtlil
