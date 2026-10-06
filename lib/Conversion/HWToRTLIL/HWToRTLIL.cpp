//===--------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/HWToRTLIL.h"
#include "HWToRTLILInternals.h"
#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/Comb/CombOps.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/HW/HWTypes.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/Seq/SeqDialect.h"
#include "circt/Dialect/Seq/SeqOps.h"
#include "circt/Support/LLVM.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/LogicalResult.h"
#include <cstdint>
#include <optional>
#include <type_traits>

namespace circt {
#define GEN_PASS_DEF_CONVERTHWTORTLIL
#include "circt/Conversion/Passes.h.inc"
} // namespace circt

using namespace circt;
using namespace comb;

// TODO: Add a proper scoping mechanism to map symbols to global RTLIL names,
// likely a symbol table walk with prefixes.

//===----------------------------------------------------------------------===//
// Type conversion
//===----------------------------------------------------------------------===//

namespace circt::HWToRTLIL {

/// Widths an `rtlil.wire` can hold: `RTLIL::Wire::width` is an `int`, and
/// Yosys drops zero-width signals, so they would not survive a round trip.
static bool isRepresentableWidth(int64_t width) {
  return width > 0 && width < INT32_MAX;
}

std::optional<mlir::Type>
RTLILTypeConverter::convertInteger(mlir::IntegerType t) {
  auto val = t.getWidth();
  if (!isRepresentableWidth(val))
    return std::nullopt;
  return rtlil::MValueType::get(t.getContext(), val);
}

std::optional<mlir::Type> RTLILTypeConverter::convertInt(circt::hw::IntType t) {
  // A parameterized width has no RTLIL form.
  auto width = dyn_cast<mlir::IntegerAttr>(t.getWidth());
  if (!width)
    return std::nullopt;
  auto val = width.getInt();
  if (!isRepresentableWidth(val))
    return std::nullopt;
  return rtlil::MValueType::get(t.getContext(), val);
}

std::optional<mlir::Type>
RTLILTypeConverter::convertClock(circt::seq::ClockType t) {
  return rtlil::MValueType::get(t.getContext(), 1);
}

// No target materialization: a value that is still unconverted at the end
// would otherwise become an undriven wire. Patterns create wires explicitly.
RTLILTypeConverter::RTLILTypeConverter() : mlir::TypeConverter() {
  addConversion(convertInt);
  addConversion(convertInteger);
  addConversion(convertClock);
}

} // namespace circt::HWToRTLIL

//===----------------------------------------------------------------------===//
// Conversion patterns
//===----------------------------------------------------------------------===//

namespace {

using HWToRTLIL::ConversionPatternBase;

/// Build a synchronously reset register.
/// Combination of `rtlil.dff` and `rtlil.mux` for the reset value.
template <typename Pattern>
static rtlil::WireOp genSyncResetReg(const Pattern &pattern, Location loc,
                                     mlir::ConversionPatternRewriter &rewriter,
                                     StringAttr name, Value data, Value clk,
                                     Value next, Value reset,
                                     Value resetValue) {
  rtlil::WireOp resultWire = pattern.genLocalWire(loc, data, rewriter);
  if (!resultWire)
    return {};
  rtlil::WireOp muxWire = pattern.genLocalWire(loc, data, rewriter);
  if (!muxWire)
    return {};

  // `rtlil.mux` drives Y with B while S is 1, so the reset value goes on B.
  Value muxConnections[4] = {next, resetValue, reset, muxWire};
  rtlil::MuxOp::create(rewriter, loc, pattern.genUniqueLocalName(rewriter),
                       muxConnections, resultWire.getWidth());

  Value dffConnections[3] = {clk, muxWire, resultWire};
  rtlil::DFFOp::create(rewriter, loc, name, dffConnections,
                       resultWire.getWidth());
  return resultWire;
}

struct CompRegOpResetConversion : ConversionPatternBase<seq::CompRegOp> {
  using ConversionPatternBase<seq::CompRegOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(seq::CompRegOp op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    if (!op.getReset() || op.getInitialValue())
      return failure();
    auto name = op.getInnerSym()
                    ? makeGlobal(rewriter, op.getInnerSymAttr().getSymName())
                    : genUniqueLocalName(rewriter);
    // Follow SeqToSV and use synchronous reset style.
    rtlil::WireOp resultWire = genSyncResetReg(
        *this, op->getLoc(), rewriter, name, op.getData(), adaptor.getClk(),
        adaptor.getInput(), adaptor.getReset(), adaptor.getResetValue());
    if (!resultWire)
      return failure();
    rewriter.replaceOp(op, resultWire);
    return success();
  }
};

struct CompRegOpConversion : ConversionPatternBase<seq::CompRegOp> {
  using ConversionPatternBase<seq::CompRegOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(seq::CompRegOp op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    if (op.getReset() || op.getInitialValue())
      return failure();
    rtlil::WireOp resultWire =
        genLocalWire(op->getLoc(), op.getData(), rewriter);
    if (!resultWire)
      return failure();
    auto name = op.getInnerSym()
                    ? makeGlobal(rewriter, op.getInnerSymAttr().getSymName())
                    : genUniqueLocalName(rewriter);
    Value connections[3] = {adaptor.getClk(), adaptor.getInput(), resultWire};
    rtlil::DFFOp::create(rewriter, op.getLoc(), name, connections,
                         resultWire.getWidth());
    rewriter.replaceOp(op, resultWire);
    return success();
  }
};

struct FirRegOpConversion : ConversionPatternBase<seq::FirRegOp> {
  using ConversionPatternBase<seq::FirRegOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(seq::FirRegOp op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    if (op.getReset() || op.getPreset())
      return failure();
    rtlil::WireOp resultWire =
        genLocalWire(op->getLoc(), op.getData(), rewriter);
    if (!resultWire)
      return failure();
    auto name = op.getInnerSym()
                    ? makeGlobal(rewriter, op.getInnerSymAttr().getSymName())
                    : genUniqueLocalName(rewriter);
    Value connections[3] = {adaptor.getClk(), adaptor.getNext(), resultWire};
    rtlil::DFFOp::create(rewriter, op.getLoc(), name, connections,
                         resultWire.getWidth());
    rewriter.replaceOp(op, resultWire);
    return success();
  }
};

struct FirRegOpResetConversion : ConversionPatternBase<seq::FirRegOp> {
  using ConversionPatternBase<seq::FirRegOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(seq::FirRegOp op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    if (!op.getReset() || op.getPreset())
      return failure();
    auto name = op.getInnerSym()
                    ? makeGlobal(rewriter, op.getInnerSymAttr().getSymName())
                    : genUniqueLocalName(rewriter);
    rtlil::WireOp resultWire;
    if (op.getIsAsync()) {
      resultWire = genLocalWire(op->getLoc(), op.getData(), rewriter);
      if (!resultWire)
        return failure();
      // For async reset use `$aldff`.
      Value connections[5] = {adaptor.getClk(), adaptor.getNext(),
                              adaptor.getReset(), adaptor.getResetValue(),
                              resultWire};
      rtlil::ALDFFOp::create(rewriter, op->getLoc(), name, connections,
                             resultWire.getWidth());
    } else {
      resultWire = genSyncResetReg(
          *this, op->getLoc(), rewriter, name, op.getData(), adaptor.getClk(),
          adaptor.getNext(), adaptor.getReset(), adaptor.getResetValue());
      if (!resultWire)
        return failure();
    }
    rewriter.replaceOp(op, resultWire);
    return success();
  }
};

template <typename BinOp, typename ResultOp, bool OpsSigned = false,
          typename = void>
struct BinOpConversion;

/// The variadic `comb` ops, lowered to a left-leaning chain of two-input cells.
template <typename BinOp, typename ResultOp, bool OpsSigned>
struct BinOpConversion<BinOp, ResultOp, OpsSigned,
                       std::enable_if_t<std::is_member_function_pointer_v<
                           decltype(&BinOp::getInputs)>>>
    : ConversionPatternBase<BinOp> {
  using Super = ConversionPatternBase<BinOp>;
  using Super::ConversionPatternBase;
  using typename Super::OpAdaptor;

  LogicalResult matchAndRewrite(BinOp op, OpAdaptor adaptor,
                                ConversionPatternRewriter &r) const override {
    auto inputs = adaptor.getInputs();
    if (inputs.size() < 2)
      return failure();

    auto width = op.getInputs()[0].getType().getIntOrFloatBitWidth();
    Value lhs = inputs.front();
    rtlil::WireOp resultWire;
    for (Value rhs : inputs.drop_front()) {
      resultWire = Super::genLocalWire(op->getLoc(), op->getResult(0), r);
      if (!resultWire)
        return failure();
      Value connections[3] = {lhs, rhs, resultWire};
      ResultOp::create(r, op->getLoc(), Super::genUniqueLocalName(r),
                       connections, width, OpsSigned);
      lhs = resultWire;
    }
    r.replaceOp(op, resultWire);
    return success();
  }
};

/// The two-operand `comb` ops.
template <typename BinOp, typename ResultOp, bool OpsSigned>
struct BinOpConversion<BinOp, ResultOp, OpsSigned,
                       std::enable_if_t<std::is_member_function_pointer_v<
                           decltype(&BinOp::getLhs)>>>
    : ConversionPatternBase<BinOp> {
  using Super = ConversionPatternBase<BinOp>;
  using Super::ConversionPatternBase;
  using typename Super::OpAdaptor;

  LogicalResult matchAndRewrite(BinOp op, OpAdaptor adaptor,
                                ConversionPatternRewriter &r) const override {
    auto resultWire = Super::genLocalWire(op->getLoc(), op->getResult(0), r);
    if (!resultWire)
      return failure();
    Value connections[3] = {adaptor.getLhs(), adaptor.getRhs(), resultWire};
    ResultOp::create(r, op->getLoc(), Super::genUniqueLocalName(r), connections,
                     op.getLhs().getType().getIntOrFloatBitWidth(), OpsSigned);
    r.replaceOp(op, resultWire);
    return success();
  }
};

struct MuxOpConversion : ConversionPatternBase<MuxOp> {
  using ConversionPatternBase<MuxOp>::ConversionPatternBase;

  LogicalResult matchAndRewrite(MuxOp op, OpAdaptor adaptor,
                                ConversionPatternRewriter &r) const override {
    if (op.getTrueValue().getType() != op.getFalseValue().getType())
      return failure();

    auto resultWire = genLocalWire(op->getLoc(), op->getResult(0), r);
    if (!resultWire)
      return failure();
    Value connections[4] = {adaptor.getFalseValue(), adaptor.getTrueValue(),
                            adaptor.getCond(), resultWire};

    rtlil::MuxOp::create(r, op->getLoc(), genUniqueLocalName(r), connections,
                         op.getTrueValue().getType().getIntOrFloatBitWidth());
    r.replaceOp(op, resultWire);

    return success();
  }
};

struct ConstantConversion : ConversionPatternBase<hw::ConstantOp> {
  using ConversionPatternBase<hw::ConstantOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(hw::ConstantOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto outType = getTypeConverter()->convertType<rtlil::MValueType>(
        op->getResultTypes()[0]);
    if (!outType)
      return failure();
    // Iterate the APInt, since `getInt()` asserts above 64 bits.
    const llvm::APInt &intVal = adaptor.getValueAttr().getValue();
    unsigned width = intVal.getBitWidth();

    llvm::SmallVector<rtlil::StateEnum> bits;
    bits.reserve(width);
    // Least significant bit first, as `#rtlil.const` stores it.
    for (unsigned idx = 0; idx < width; idx++)
      bits.emplace_back(intVal[idx] ? rtlil::StateEnum::S1
                                    : rtlil::StateEnum::S0);

    rewriter.replaceOpWithNewOp<rtlil::ConstOp>(
        op, outType, rtlil::ConstAttr::get(getContext(), bits));
    return success();
  }
};

struct ModuleConversion : ConversionPatternBase<hw::HWModuleOp> {
  using ConversionPatternBase<hw::HWModuleOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(hw::HWModuleOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    if (!op.getBody().hasOneBlock())
      return failure();
    // `prepareForConversion` has already reported unconvertible ports.
    if (llvm::any_of(op.getPortTypes(), [&](Type type) {
          return !getTypeConverter()->convertType(type);
        }))
      return rewriter.notifyMatchFailure(op, "port type has no RTLIL form");
    auto moduleOp = rtlil::ModuleOp::create(
        rewriter, op.getLoc(), makeGlobal(rewriter, op.getSymName()));
    mlir::TypeConverter::SignatureConversion converter(op.getNumInputPorts());
    auto createPortWire = [&](Type type, size_t portId,
                              rtlil::PortDirection direction) {
      return HWToRTLIL::createWire(
          rewriter, op->getLoc(),
          getTypeConverter()->convertType<rtlil::MValueType>(type),
          makeGlobal(rewriter, op.getPortName(portId)).getValue(),
          rtlil::PortDirectionAttr::get(getContext(), direction), portId + 1);
    };
    for (size_t input = 0; input < op.getNumInputPorts(); input++) {
      rewriter.setInsertionPoint(moduleOp.getBodyBlock(),
                                 moduleOp.getBodyBlock()->begin());
      auto wire = createPortWire(op.getInputTypes()[input],
                                 op.getPortIdForInputId(input),
                                 rtlil::PortDirection::Input);
      converter.remapInput(input, wire.getResult());
    }
    // Create output port wires here, where the port names are still known.
    // `OutputConversion` finds them again through `getPortWires()`.
    for (size_t output = 0; output < op.getNumOutputPorts(); output++) {
      rewriter.setInsertionPoint(moduleOp.getBodyBlock(),
                                 moduleOp.getBodyBlock()->end());
      createPortWire(op.getOutputTypes()[output],
                     op.getPortIdForOutputId(output),
                     rtlil::PortDirection::Output);
    }
    // Convert the block signature and inline the body into the module.
    rewriter.applySignatureConversion(op.getBodyBlock(), converter,
                                      getTypeConverter());
    rewriter.inlineBlockBefore(&op.getBody().getBlocks().front(),
                               moduleOp.getBodyBlock(),
                               moduleOp.getBodyBlock()->end());
    rewriter.replaceOp(op, moduleOp);
    return success();
  }
};

struct OutputConversion : ConversionPatternBase<hw::OutputOp> {
  using ConversionPatternBase<hw::OutputOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(hw::OutputOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto module = op->getParentOfType<rtlil::ModuleOp>();
    if (!module)
      return failure();

    // Ordered by `port_id`, so the output port wires come in output order.
    llvm::SmallVector<rtlil::WireOp> outputPorts;
    module.getPortWires(outputPorts);
    llvm::erase_if(outputPorts,
                   [](rtlil::WireOp wire) { return !wire.isPortOutput(); });

    auto outputs = adaptor.getOutputs();
    if (outputPorts.size() != outputs.size())
      return failure();

    // Connect rather than rename the defining wire: the value may be a const,
    // an input port, or feed several outputs.
    for (auto [wire, value] : llvm::zip(outputPorts, outputs))
      rtlil::WConnectionOp::create(rewriter, op.getLoc(), wire.getResult(),
                                   value);
    rewriter.eraseOp(op);
    return success();
  }
};

struct InstanceConversion : ConversionPatternBase<hw::InstanceOp> {
  using ConversionPatternBase<hw::InstanceOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(hw::InstanceOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    llvm::SmallVector<mlir::Attribute> ports;
    auto definingOp = HWToRTLIL::lookupSymbolWalkTables<hw::HWModuleOp>(
        op, op.getModuleNameAttr());
    if (!definingOp)
      return failure();
    // Callee port wires are named `\` + the port name.
    for (auto &in : op.getArgNames())
      ports.emplace_back(
          makeGlobal(rewriter, cast<mlir::StringAttr>(in).strref()));
    for (auto &out : op.getResultNames())
      ports.emplace_back(
          makeGlobal(rewriter, cast<mlir::StringAttr>(out).strref()));
    llvm::SmallVector<Value> resultWires(adaptor.getInputs());
    for (auto res : op->getResults()) {
      auto wire = genLocalWire(res.getLoc(), res, rewriter);
      if (!wire)
        return failure();
      resultWires.emplace_back(wire);
    }

    rtlil::InstanceOp::create(
        rewriter, op->getLoc(), makeGlobal(rewriter, op.getInstanceNameAttr()),
        mlir::FlatSymbolRefAttr::get(makeGlobal(rewriter, op.getModuleName())),
        resultWires, rewriter.getArrayAttr(ports), rewriter.getArrayAttr({}));
    resultWires.erase(resultWires.begin(),
                      resultWires.begin() + op.getNumInputPorts());
    rewriter.replaceOp(op, mlir::ValueRange(resultWires));
    return success();
  }
};

struct ICMPConversion : ConversionPatternBase<comb::ICmpOp> {
  using ConversionPatternBase<comb::ICmpOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(comb::ICmpOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto pred = adaptor.getPredicate();
    // RTLIL has no wildcard-compare cell for `==?`/`!=?`.
    if (pred == ICmpPredicate::weq || pred == ICmpPredicate::wne)
      return failure();

    auto resultWire = genLocalWire(op->getLoc(), op.getResult(), rewriter);
    if (!resultWire)
      return failure();
    mlir::Value connections[3] = {adaptor.getLhs(), adaptor.getRhs(),
                                  resultWire};
    auto name = genUniqueLocalName(rewriter);
    auto width = op.getLhs().getType().getIntOrFloatBitWidth();
    bool isSigned = comb::ICmpOp::isPredicateSigned(pred);
    switch (pred) {
    case ICmpPredicate::eq:
      rtlil::EQOp::create(rewriter, op->getLoc(), name, connections, width,
                          isSigned);
      break;
    case ICmpPredicate::ne:
      rtlil::NEOp::create(rewriter, op->getLoc(), name, connections, width,
                          isSigned);
      break;
    // `$eqx`/`$nex` compare x and z as values instead of propagating them,
    // matching `===`/`!==`. Under `bin` this coincides with `$eq`/`$ne`.
    case ICmpPredicate::ceq:
      rtlil::EQXOp::create(rewriter, op->getLoc(), name, connections, width,
                           isSigned);
      break;
    case ICmpPredicate::cne:
      rtlil::NEXOp::create(rewriter, op->getLoc(), name, connections, width,
                           isSigned);
      break;
    case ICmpPredicate::ugt:
    case ICmpPredicate::sgt:
      rtlil::GTOp::create(rewriter, op->getLoc(), name, connections, width,
                          isSigned);
      break;
    case ICmpPredicate::ult:
    case ICmpPredicate::slt:
      rtlil::LTOp::create(rewriter, op->getLoc(), name, connections, width,
                          isSigned);
      break;
    case ICmpPredicate::ule:
    case ICmpPredicate::sle:
      rtlil::LEOp::create(rewriter, op->getLoc(), name, connections, width,
                          isSigned);
      break;
    case ICmpPredicate::uge:
    case ICmpPredicate::sge:
      rtlil::GEOp::create(rewriter, op->getLoc(), name, connections, width,
                          isSigned);
      break;
    case ICmpPredicate::weq:
    case ICmpPredicate::wne:
      llvm_unreachable("wildcard predicates rejected above");
    }
    rewriter.replaceOp(op, resultWire);
    return success();
  }
};

// `comb.concat` orders its operands most significant first, `rtlil.concat`
// least significant first.
struct ConcatConversion : ConversionPatternBase<comb::ConcatOp> {
  using ConversionPatternBase<comb::ConcatOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(comb::ConcatOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto resultType =
        getTypeConverter()->convertType<rtlil::MValueType>(op.getType());
    if (!resultType)
      return failure();

    llvm::SmallVector<Value> inputs(llvm::reverse(adaptor.getInputs()));
    rewriter.replaceOpWithNewOp<rtlil::ConcatOp>(op, resultType, inputs);
    return success();
  }
};

// Both `lowBit` and `offset` count from bit 0 of the input, and the width taken
// is the width of the result.
struct ExtractConversion : ConversionPatternBase<comb::ExtractOp> {
  using ConversionPatternBase<comb::ExtractOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(comb::ExtractOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto resultType =
        getTypeConverter()->convertType<rtlil::MValueType>(op.getType());
    if (!resultType)
      return failure();

    rewriter.replaceOpWithNewOp<rtlil::SliceOp>(
        op, resultType, adaptor.getInput(), op.getLowBitAttr());
    return success();
  }
};

struct ParityConversion : ConversionPatternBase<comb::ParityOp> {
  using ConversionPatternBase<comb::ParityOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(comb::ParityOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto resultWire = genLocalWire(op->getLoc(), op.getResult(), rewriter);
    if (!resultWire)
      return failure();

    Value connections[2] = {adaptor.getInput(), resultWire};
    rtlil::ReduceXorOp::create(
        rewriter, op->getLoc(), genUniqueLocalName(rewriter), connections,
        op.getInput().getType().getIntOrFloatBitWidth(), false);
    rewriter.replaceOp(op, resultWire);
    return success();
  }
};

// Use concat of SigSpec.
struct ReplicateConversion : ConversionPatternBase<comb::ReplicateOp> {
  using ConversionPatternBase<comb::ReplicateOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(comb::ReplicateOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto resultType =
        getTypeConverter()->convertType<rtlil::MValueType>(op.getType());
    if (!resultType)
      return failure();

    size_t multiple = op.getMultiple();
    if (multiple == 1) {
      rewriter.replaceOp(op, adaptor.getInput());
      return success();
    }
    llvm::SmallVector<Value> inputs(multiple, adaptor.getInput());
    rewriter.replaceOpWithNewOp<rtlil::ConcatOp>(op, resultType, inputs);
    return success();
  }
};

// SigSpec of one-bit chunks, read back to front.
struct ReverseConversion : ConversionPatternBase<comb::ReverseOp> {
  using ConversionPatternBase<comb::ReverseOp>::ConversionPatternBase;

  LogicalResult
  matchAndRewrite(comb::ReverseOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    auto resultType =
        getTypeConverter()->convertType<rtlil::MValueType>(op.getType());
    if (!resultType)
      return failure();

    unsigned width = resultType.getWidth();
    auto bitType = rtlil::MValueType::get(getContext(), 1);
    llvm::SmallVector<Value> bits;
    bits.reserve(width);
    // `rtlil.concat` is least significant first: result bit 0 is input bit
    // `width - 1`.
    for (unsigned bit = 0; bit < width; bit++)
      bits.push_back(rtlil::SliceOp::create(
          rewriter, op->getLoc(), bitType, adaptor.getInput(),
          rewriter.getI32IntegerAttr(width - 1 - bit)));
    rewriter.replaceOpWithNewOp<rtlil::ConcatOp>(op, resultType, bits);
    return success();
  }
};

} // namespace

//===----------------------------------------------------------------------===//
// Convert HW to RTLIL pass
//===----------------------------------------------------------------------===//

namespace {
struct ConvertHWToRTLILPass
    : public circt::impl::ConvertHWToRTLILBase<ConvertHWToRTLILPass> {
  void runOnOperation() override;
};
} // namespace

static void populateHWToRTLILConversionPatterns(
    TypeConverter &converter, HWToRTLIL::ConversionPatternContext &rtlilContext,
    RewritePatternSet &patterns) {
  patterns
      .add<ModuleConversion, OutputConversion,
           BinOpConversion<comb::AndOp, rtlil::AndOp>,
           BinOpConversion<comb::OrOp, rtlil::OrOp>,
           BinOpConversion<comb::XorOp, rtlil::XorOp>,
           BinOpConversion<comb::AddOp, rtlil::AddOp>,
           BinOpConversion<comb::SubOp, rtlil::SubOp>,
           BinOpConversion<comb::MulOp, rtlil::MulOp>,
           BinOpConversion<comb::DivUOp, rtlil::DivOp>,
           BinOpConversion<comb::DivSOp, rtlil::DivOp, true>,
           BinOpConversion<comb::ModUOp, rtlil::ModOp>,
           BinOpConversion<comb::ModSOp, rtlil::ModOp, true>,
           BinOpConversion<comb::ShlOp, rtlil::ShlOp>,
           BinOpConversion<comb::ShrUOp, rtlil::ShrOp>,
           BinOpConversion<comb::ShrSOp, rtlil::SShrOp, true>, MuxOpConversion,
           InstanceConversion, CompRegOpResetConversion, CompRegOpConversion,
           FirRegOpResetConversion, FirRegOpConversion, ConstantConversion,
           ICMPConversion, ConcatConversion, ExtractConversion,
           ParityConversion, ReplicateConversion, ReverseConversion>(
          converter, rtlilContext, patterns.getContext());
}

/// Preparation of conversion by erroring on unsupported constructs and removing
/// dead modules.
static LogicalResult prepareForConversion(mlir::ModuleOp module,
                                          const TypeConverter &converter) {
  auto walk = module.walk([](hw::InstanceOp op) -> mlir::WalkResult {
    // Not supported `hw.module.extern` and `hw.module.generated`.
    auto *callee =
        HWToRTLIL::lookupSymbolWalkTables(op, op.getModuleNameAttr());
    if (callee && !isa<hw::HWModuleOp>(callee)) {
      auto diag = op.emitOpError("instantiates '")
                  << op.getModuleName()
                  << "', which has no body; the rtlil dialect cannot represent "
                     "extern or generated modules yet";
      diag.attachNote(callee->getLoc()) << "module declared here";
      return mlir::WalkResult::interrupt();
    }

    // A cell with parameters sends `hierarchy` into `RTLIL::Module::derive()`,
    // which only modules carrying a Verilog AST implement.
    if (!op.getParameters().empty()) {
      op.emitOpError("has parameters, which the rtlil dialect cannot represent "
                     "on a module with a body; run 'hw-specialize' first");
      return mlir::WalkResult::interrupt();
    }
    return mlir::WalkResult::advance();
  });
  if (walk.wasInterrupted())
    return failure();

  // Remove parametric modules that nothing references any more, which is what
  // `hw-specialize` leaves behind and `symbol-dce` will not collect.
  for (auto moduleOp :
       llvm::make_early_inc_range(module.getOps<hw::HWModuleOp>())) {
    if (moduleOp.getParameters().empty())
      continue;
    if (!mlir::SymbolTable::symbolKnownUseEmpty(moduleOp, module))
      return moduleOp.emitOpError(
          "is parametric and still used; run 'hw-specialize' first");
    moduleOp.erase();
  }

  // Reject ports and values without an RTLIL type up front: `hw.module` is not
  // illegal, so a failed `ModuleConversion` would not fail the pass, and comb
  // or seq ops would only get a generic legalization error. This runs after
  // the parametric templates are erased, as their port types are parametric.
  for (auto moduleOp : module.getOps<hw::HWModuleOp>()) {
    for (auto &port : moduleOp.getPortList()) {
      if (converter.convertType(port.type))
        continue;
      return moduleOp.emitOpError("port '")
             << port.getName() << "' has type " << port.type
             << ", which has no RTLIL representation";
    }

    // Catch integer values whose width RTLIL cannot hold, such as `i0`. Only
    // ops this pass converts are checked, and only integer types: any other
    // type means the op itself is unsupported, which legalization reports.
    auto walk = moduleOp.walk([&](Operation *op) -> mlir::WalkResult {
      if (!isa<comb::CombDialect, seq::SeqDialect>(op->getDialect()) &&
          !isa<hw::ConstantOp>(op))
        return mlir::WalkResult::advance();
      for (Type type : op->getResultTypes()) {
        if (!isa<IntegerType, hw::IntType>(type) || converter.convertType(type))
          continue;
        op->emitOpError("result has type ")
            << type << ", which has no RTLIL representation";
        return mlir::WalkResult::interrupt();
      }
      return mlir::WalkResult::advance();
    });
    if (walk.wasInterrupted())
      return failure();
  }
  return success();
}

void ConvertHWToRTLILPass::runOnOperation() {
  HWToRTLIL::ConversionPatternContext context;
  HWToRTLIL::RTLILTypeConverter converter;
  if (failed(prepareForConversion(getOperation(), converter)))
    return signalPassFailure();

  ConversionTarget target(getContext());
  target.addLegalDialect<rtlil::RTLILDialect>();
  target.addIllegalDialect<comb::CombDialect>();
  target.addIllegalDialect<seq::SeqDialect>();
  target.addLegalOp<mlir::ModuleOp>();

  RewritePatternSet patterns(&getContext());
  populateHWToRTLILConversionPatterns(converter, context, patterns);
  // No topological sort afterwards: an `rtlil.module` body is a graph region,
  // so a cell may precede the `rtlil.wire` it drives.
  if (failed(mlir::applyPartialConversion(getOperation(), target,
                                          std::move(patterns))))
    return signalPassFailure();
}
