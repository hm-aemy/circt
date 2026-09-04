//===- HWToRTLIL.cpp ----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/HWToRTLIL.h"
#include "circt/Conversion/RTLILCommon.h"
#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/Comb/CombOps.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/HW/HWTypes.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/Seq/SeqDialect.h"
#include "circt/Dialect/Seq/SeqOps.h"
#include "circt/Support/LLVM.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/DialectConversion.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/LogicalResult.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>

namespace circt {
#define GEN_PASS_DEF_CONVERTHWTORTLIL
#include "circt/Conversion/Passes.h.inc"
} // namespace circt

using namespace circt;
using namespace comb;

// TODO proper scoping mechanism for symbols --> global rtlil names
// likely symbol table walk with prefixes

//===----------------------------------------------------------------------===//
// Conversion patterns
//===----------------------------------------------------------------------===//

namespace {

using rtlil::ConversionPatternBase;

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
    if (!op.getReset() || op.getInitialValue()) {
      return failure();
    }
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
    if (op.getReset() || op.getInitialValue()) {
      return failure();
    }
    rtlil::WireOp resultWire =
        genLocalWire(op->getLoc(), op.getData(), rewriter);
    auto name =
        op.getInnerSym()
            ? makeGlobal(
                  rewriter,
                  op.getInnerSymAttr().getSymName()) // this should be prefixed
                                                     // by the module probably
            : genUniqueLocalName(rewriter);
    std::vector<Value> connections(
        {adaptor.getClk(), adaptor.getInput(), resultWire});
    rtlil::DFFOp::create(rewriter, op.getLoc(), name, std::move(connections),
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
    if (op.getReset() || op.getPreset()) {
      return failure();
    }
    rtlil::WireOp resultWire =
        genLocalWire(op->getLoc(), op.getData(), rewriter);
    auto name =
        op.getInnerSym()
            ? makeGlobal(
                  rewriter,
                  op.getInnerSymAttr().getSymName()) // this should be prefixed
                                                     // by the module probably
            : genUniqueLocalName(rewriter);
    std::vector<Value> connections(
        {adaptor.getClk(), adaptor.getNext(), resultWire});
    rtlil::DFFOp::create(rewriter, op.getLoc(), name, std::move(connections),
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
    if (!op.getReset() || op.getPreset()) {
      return failure();
    }
    auto name = op.getInnerSym()
                    ? makeGlobal(rewriter, op.getInnerSymAttr().getSymName())
                    : genUniqueLocalName(rewriter);
    rtlil::WireOp resultWire;
    if (op.getIsAsync()) {
      resultWire = genLocalWire(op->getLoc(), op.getData(), rewriter);
      if (!resultWire)
        return failure();
      // For async reset use `$aldff`.
      std::vector<Value> connections({adaptor.getClk(), adaptor.getNext(),
                                      adaptor.getReset(),
                                      adaptor.getResetValue(), resultWire});
      rtlil::ALDFFOp::create(rewriter, op->getLoc(), name,
                             std::move(connections), resultWire.getWidth());
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

/// The variadic `comb` ops. RTLIL cell takes two inputs, spread back out over a
/// left-leaning chain of
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
    if (op.getTrueValue().getType() != op.getFalseValue().getType()) {
      return failure();
    }

    auto resultWire = genLocalWire(op->getLoc(), op->getResult(0), r);
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
    // Iterate the APInt rather than going through `getInt()`, which asserts
    // above 64 bits. Constants that wide are ordinary in gate-level designs.
    const llvm::APInt &intVal = adaptor.getValueAttr().getValue();
    unsigned width = intVal.getBitWidth();

    llvm::SmallVector<rtlil::StateEnum> bits;
    bits.reserve(width);
    // Least significant bit first, the order both `#rtlil.const` and
    // `RTLIL::Const` store.
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
    if (!op.getBody().hasOneBlock()) {
      return failure();
    }
    auto moduleOp = rtlil::ModuleOp::create(
        rewriter, op.getLoc(), makeGlobal(rewriter, op.getSymName()));
    mlir::TypeConverter::SignatureConversion converter(op.getNumInputPorts());
    for (size_t input = 0; input < op.getNumInputPorts(); input++) {
      rewriter.setInsertionPoint(moduleOp.getBodyBlock(),
                                 moduleOp.getBodyBlock()->begin());
      Value replacement = getTypeConverter()->materializeTargetConversion(
          rewriter, op->getLoc(),
          getTypeConverter()->convertType(op.getInputTypes()[input]), {});
      auto wire = replacement.getDefiningOp<rtlil::WireOp>();
      rewriter.modifyOpInPlace(wire, [&]() {
        wire.setPortId(op.getPortIdForInputId(input) + 1);
        wire.setName(makeGlobal(rewriter,
                                op.getPortName(op.getPortIdForInputId(input))));
      });
      converter.remapInput(input, replacement);
    }
    // Output port wires are created here rather than by `OutputConversion`,
    // which by the time it runs has been reparented into `moduleOp` and can no
    // longer see this `hw.module` to get the port names from. It finds these
    // wires through `getPortWires()` instead, so nothing has to be carried on
    // the shared context between the two patterns -- and the conversion driver
    // does not promise to finish one module before starting the next.
    for (size_t output = 0; output < op.getNumOutputPorts(); output++) {
      rewriter.setInsertionPoint(moduleOp.getBodyBlock(),
                                 moduleOp.getBodyBlock()->end());
      Value portWire = getTypeConverter()->materializeTargetConversion(
          rewriter, op->getLoc(),
          getTypeConverter()->convertType(op.getOutputTypes()[output]), {});
      auto wire = portWire.getDefiningOp<rtlil::WireOp>();
      rewriter.modifyOpInPlace(wire, [&]() {
        wire.setPortInput(false);
        wire.setPortOutput(true);
        wire.setPortId(op.getPortIdForOutputId(output) + 1);
        wire.setName(makeGlobal(
            rewriter, op.getPortName(op.getPortIdForOutputId(output))));
      });
    }
    // Apply type conversion to block signature, then inline the converted block
    // into the module.
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

    // `getPortWires()` is ordered by `port_id`, and `getPortIdForOutputId` is
    // monotonic in the output index, so the output port wires appear here in
    // output order.
    llvm::SmallVector<rtlil::WireOp> outputPorts;
    for (auto wire : rtlil::getPortWires(module))
      if (wire.getPortOutput())
        outputPorts.push_back(wire);

    auto outputs = adaptor.getOutputs();
    if (outputPorts.size() != outputs.size())
      return failure();

    // Drive each port wire with a connection rather than relabelling whatever
    // wire happens to define the value. The value may be an `rtlil.const`, an
    // input port, or the same wire feeding two outputs -- none of which can be
    // turned into this output port in place.
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
    auto definingOp = rtlil::lookupSymbolWalkTables<hw::HWModuleOp>(
        op, op.getModuleNameAttr());
    if (!definingOp)
      return failure();
    // The callee's port wires are named `\` + the port name, so the caller can
    // spell them without looking the callee up. That only holds because
    // `makeGlobal` no longer appends a uniquing suffix -- when it did, the
    // callee's identity had to be smuggled in as its Location.
    for (auto &in : op.getArgNames())
      ports.emplace_back(
          makeGlobal(rewriter, cast<mlir::StringAttr>(in).strref()));
    for (auto &out : op.getResultNames())
      ports.emplace_back(
          makeGlobal(rewriter, cast<mlir::StringAttr>(out).strref()));
    llvm::SmallVector<Value> resultWires(adaptor.getInputs());
    for (auto res : op->getResults()) {
      auto type = getTypeConverter()->convertType(res.getType());
      auto wire = getTypeConverter()->materializeTargetConversion(
          rewriter, res.getLoc(), type, res);
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
    if (static_cast<int>(pred) > 10 || !adaptor.getTwoState()) {
      return failure(); // currently not supported
    }
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
    default:
      return failure();
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
    // `rtlil.concat` takes its operands least significant first, and result bit
    // 0 is input bit `width - 1`.
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
    TypeConverter &converter, rtlil::ConversionPatternContext &rtlilContext,
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
static LogicalResult prepareForConversion(mlir::ModuleOp module) {
  auto walk = module.walk([](hw::InstanceOp op) -> mlir::WalkResult {
    // Not supported `hw.module.extern` and `hw.module.generated`.
    auto *callee = rtlil::lookupSymbolWalkTables(op, op.getModuleNameAttr());
    if (callee && !isa<hw::HWModuleOp>(callee)) {
      op.emitOpError("instantiates ")
              .append(op.getModuleName())
              .append(", which has no body; the rtlil dialect cannot represent "
                      "extern or generated modules yet")
              .attachNote(callee->getLoc())
          << "module declared here";
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
  return success();
}

void ConvertHWToRTLILPass::runOnOperation() {
  if (failed(prepareForConversion(getOperation())))
    return signalPassFailure();

  ConversionTarget target(getContext());
  mlir::ConversionConfig config;
  target.addLegalDialect<rtlil::RTLILDialect>();
  target.addIllegalDialect<comb::CombDialect>();
  target.addIllegalDialect<seq::SeqDialect>();
  target.addLegalOp<mlir::ModuleOp>();
  rtlil::ConversionPatternContext context;

  RewritePatternSet patterns(&getContext());
  rtlil::RTLILTypeConverter converter(context);
  populateHWToRTLILConversionPatterns(converter, context, patterns);
  // No topological sort afterwards: an `rtlil.module` body is a graph region,
  // so a cell may precede the `rtlil.wire` it drives.
  if (failed(mlir::applyPartialConversion(getOperation(), target,
                                          std::move(patterns))))
    return signalPassFailure();
}
