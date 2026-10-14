//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Convert RTLIL to Core dialects. Uses LLHD to represent the wires. This
// requires LLHD passes to follow up for a Core only representation.
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/RTLILToCore.h"
#include "circt/Dialect/Comb/CombOps.h"
#include "circt/Dialect/Debug/DebugOps.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/HW/HWTypes.h"
#include "circt/Dialect/LLHD/LLHDOps.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/Seq/SeqOps.h"
#include "circt/Dialect/Verif/VerifOps.h"
#include "circt/Support/ConversionPatternSet.h"
#include "circt/Transforms/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/Iterators.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/IR/DerivedTypes.h"

namespace circt {
#define GEN_PASS_DEF_CONVERTRTLILTOCORE
#include "circt/Conversion/Passes.h.inc"
} // namespace circt

using namespace mlir;
using namespace circt;
using namespace rtlil;

//===----------------------------------------------------------------------===//
// Utilities
//===----------------------------------------------------------------------===//

/// Reverse `makeGlobal` of HWToRTLIL
static StringAttr makeLocal(MLIRContext *context, StringRef name) {
  name.consume_front("\\");
  return StringAttr::get(context, name);
}

/// Get signal name converted without global prefix and empty for anonymous.
static StringAttr getSignalName(MLIRContext *context, StringRef name) {
  if (!name.starts_with("\\"))
    return {};
  return makeLocal(context, name);
}

static unsigned getWidth(Value value) {
  return cast<MValueType>(value.getType()).getWidth();
}

static IntegerType getIntType(Value value) {
  return IntegerType::get(value.getContext(), getWidth(value));
}

/// Drives `value` onto `signal` without delay, like a continuous assignment.
/// `llhd-sig2reg` only promotes drives with this delay.
static void drive(OpBuilder &builder, Location loc, Value signal, Value value) {
  auto time = llhd::ConstantTimeOp::create(builder, loc, 0, "ns", 0, 1);
  llhd::DriveOp::create(builder, loc, signal, value, time, Value{});
}

/// Returns whether `use` connects its value to a port the user drives.
static bool isDriven(OpOperand &use) {
  if (isa<WConnectionOp>(use.getOwner()))
    return use.getOperandNumber() == 0;
  if (auto cell = dyn_cast<CellOpInterface>(use.getOwner()))
    return cell.getCellPortDirection(use.getOperandNumber()) ==
           PortDirection::Output;
  return false;
}

//===----------------------------------------------------------------------===//
// Structure
//===----------------------------------------------------------------------===//

/// Create the module by collecting and processing the ports
static LogicalResult convert(rtlil::ModuleOp op, rtlil::ModuleOp::Adaptor,
                             ConversionPatternRewriter &rewriter,
                             const TypeConverter &typeConverter) {
  auto *context = op.getContext();

  SmallVector<WireOp> portWires;
  op.getPortWires(portWires);
  SmallVector<hw::PortInfo> ports;
  // Gathering input and output ports
  for (auto wire : portWires) {
    if (wire.isPortInput() && wire.isPortOutput())
      return wire.emitOpError("inout ports are not supported");
    hw::PortInfo port;
    port.name = makeLocal(context, wire.getName());
    port.type = getIntType(wire.getResult());
    port.dir = wire.isPortInput() ? hw::ModulePort::Direction::Input
                                  : hw::ModulePort::Direction::Output;
    port.loc = wire.getLoc();
    ports.push_back(port);
  }

  auto hwModule = hw::HWModuleOp::create(
      rewriter, op.getLoc(), makeLocal(context, op.getSymName()), ports);
  auto *body = hwModule.getBodyBlock();
  auto *terminator = body->getTerminator();
  rewriter.inlineBlockBefore(op.getBodyBlock(), terminator);

  // Create ops accessing the ports
  unsigned numInputs = 0;
  SmallVector<Value> outputs;
  for (auto wire : portWires) {
    rewriter.setInsertionPoint(wire);
    auto signal = llhd::SignalOp::create(
        rewriter, wire.getLoc(), typeConverter.convertType(wire.getType()),
        getSignalName(context, wire.getName()), Value{});
    if (wire.isPortInput()) {
      drive(rewriter, wire.getLoc(), signal, body->getArgument(numInputs++));
    } else {
      rewriter.setInsertionPoint(terminator);
      outputs.push_back(llhd::ProbeOp::create(rewriter, wire.getLoc(), signal));
    }
    rewriter.replaceOp(wire, signal);
  }
  rewriter.modifyOpInPlace(terminator,
                           [&] { terminator->setOperands(outputs); });

  rewriter.eraseOp(op);
  return success();
}

/// Convert internal wires to LLHD signals.
static LogicalResult convert(WireOp op, WireOp::Adaptor,
                             ConversionPatternRewriter &rewriter,
                             const TypeConverter &typeConverter) {
  if (op.getDirection())
    return failure();
  rewriter.replaceOpWithNewOp<llhd::SignalOp>(
      op, typeConverter.convertType(op.getType()),
      getSignalName(op.getContext(), op.getName()), Value{});
  return success();
}

static LogicalResult convert(WConnectionOp op, WConnectionOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  drive(rewriter, op.getLoc(), adaptor.getLhs(),
        llhd::ProbeOp::create(rewriter, op.getLoc(), adaptor.getRhs()));
  rewriter.eraseOp(op);
  return success();
}

//===----------------------------------------------------------------------===//
// Signal spec
//===----------------------------------------------------------------------===//

/// Constant
static LogicalResult convert(ConstOp op, ConstOp::Adaptor,
                             ConversionPatternRewriter &rewriter,
                             const TypeConverter &typeConverter) {
  // `$bits` is least significant first, like `APInt`.
  auto bits = op.getValue().getBits();
  APInt intVal(bits.size(), 0);
  for (auto [idx, bit] : llvm::enumerate(bits)) {
    if (bit == StateEnum::S1)
      intVal.setBit(idx);
    else if (bit != StateEnum::S0)
      return op.emitOpError("with bits other than 0 and 1 is not supported");
  }
  auto value = hw::ConstantOp::create(rewriter, op.getLoc(), intVal);
  rewriter.replaceOpWithNewOp<llhd::SignalOp>(
      op, typeConverter.convertType(op.getType()), StringAttr{}, value);
  return success();
}

static LogicalResult convert(SliceOp op, SliceOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter,
                             const TypeConverter &typeConverter) {
  auto lowBit = hw::ConstantOp::create(
      rewriter, op.getLoc(),
      rewriter.getIntegerType(llvm::Log2_64_Ceil(getWidth(op.getInput()))),
      op.getOffset());
  rewriter.replaceOpWithNewOp<llhd::SigExtractOp>(
      op, typeConverter.convertType(op.getType()), adaptor.getInput(), lowBit);
  return success();
}

/// LLHD has no concatenation of signals. A concat becomes a signal of its own,
/// connected to its inputs in the direction it is used in.
static LogicalResult convert(ConcatOp op, ConcatOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter,
                             const TypeConverter &typeConverter) {
  auto uses = op.getResult().getUses();
  bool driven = llvm::any_of(uses, isDriven);
  if (driven && !llvm::all_of(uses, isDriven))
    return op.emitOpError("is both driven and read");

  auto loc = op.getLoc();
  Value signal = llhd::SignalOp::create(rewriter, loc,
                                        typeConverter.convertType(op.getType()),
                                        StringAttr{}, Value{});
  if (driven) {
    Value value = llhd::ProbeOp::create(rewriter, loc, signal);
    unsigned offset = 0;
    for (auto [input, inputSignal] :
         llvm::zip(op.getInputs(), adaptor.getInputs())) {
      unsigned width = getWidth(input);
      drive(rewriter, loc, inputSignal,
            comb::ExtractOp::create(rewriter, loc, value, offset, width));
      offset += width;
    }
  } else {
    // `rtlil.concat` is least significant first, `comb.concat` most.
    SmallVector<Value> inputs;
    for (auto input : llvm::reverse(adaptor.getInputs()))
      inputs.push_back(llhd::ProbeOp::create(rewriter, loc, input));
    drive(rewriter, loc, signal, comb::ConcatOp::create(rewriter, loc, inputs));
  }
  rewriter.replaceOp(op, signal);
  return success();
}

//===----------------------------------------------------------------------===//
// Cells
//===----------------------------------------------------------------------===//

/// The TypedCellOp with a fixed number of connections for Comb.
template <typename CellOp, typename CombOp>
static LogicalResult convertFixedOperands(CellOp op,
                                          typename CellOp::Adaptor adaptor,
                                          ConversionPatternRewriter &rewriter) {
  auto connections = adaptor.getConnections();
  auto output = connections.drop_front(CellOp::getNumCellInputs());
  assert(output.size() == 1 && "Comb ops have a single result");
  SmallVector<Value> inputs;
  for (auto connection : connections.take_front(CellOp::getNumCellInputs()))
    inputs.push_back(llhd::ProbeOp::create(rewriter, op.getLoc(), connection));
  auto result = CombOp::create(rewriter, op.getLoc(),
                               getIntType(op.getConnections().back()), inputs);
  drive(rewriter, op.getLoc(), output.front(), result);
  rewriter.eraseOp(op);
  return success();
}

/// Comparison cells become `comb.icmp`, with the predicate picked by the
/// signedness of the operands.
template <typename CellOp, comb::ICmpPredicate unsignedPred,
          comb::ICmpPredicate signedPred = unsignedPred>
static LogicalResult convertCmp(CellOp op, typename CellOp::Adaptor adaptor,
                                ConversionPatternRewriter &rewriter) {
  auto loc = op.getLoc();
  auto connections = adaptor.getConnections();
  Value lhs = llhd::ProbeOp::create(rewriter, loc, connections[0]);
  Value rhs = llhd::ProbeOp::create(rewriter, loc, connections[1]);
  auto pred = op.getOpsSigned() ? signedPred : unsignedPred;
  drive(rewriter, loc, connections[2],
        comb::ICmpOp::create(rewriter, loc, pred, lhs, rhs));
  rewriter.eraseOp(op);
  return success();
}

/// Mux output Y is B while S is 1, else A.
static LogicalResult convert(rtlil::MuxOp op, rtlil::MuxOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  auto loc = op.getLoc();
  auto connections = adaptor.getConnections();
  Value a = llhd::ProbeOp::create(rewriter, loc, connections[0]);
  Value b = llhd::ProbeOp::create(rewriter, loc, connections[1]);
  Value s = llhd::ProbeOp::create(rewriter, loc, connections[2]);
  drive(rewriter, loc, connections[3],
        comb::MuxOp::create(rewriter, loc, s, b, a));
  rewriter.eraseOp(op);
  return success();
}

/// Rising edge `$dff` is a `seq.compreg` without reset.
static LogicalResult convert(DFFOp op, DFFOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  auto loc = op.getLoc();
  auto connections = adaptor.getConnections();
  Value clk = seq::ToClockOp::create(
      rewriter, loc, llhd::ProbeOp::create(rewriter, loc, connections[0]));
  Value input = llhd::ProbeOp::create(rewriter, loc, connections[1]);
  seq::CompRegOp reg;
  if (auto name = getSignalName(op.getContext(), op.getName()))
    reg = seq::CompRegOp::create(rewriter, loc, input, clk, name);
  else
    reg = seq::CompRegOp::create(rewriter, loc, input, clk);
  drive(rewriter, loc, connections[2], reg);
  rewriter.eraseOp(op);
  return success();
}

//===----------------------------------------------------------------------===//
// RTLIL to Core Conversion Pass
//===----------------------------------------------------------------------===//

namespace {
struct RTLILToCorePass
    : public circt::impl::ConvertRTLILToCoreBase<RTLILToCorePass> {
  void runOnOperation() override;
};
} // namespace

void RTLILToCorePass::runOnOperation() {
  auto *context = &getContext();

  TypeConverter typeConverter;
  typeConverter.addConversion([](MValueType type) {
    return llhd::RefType::get(
        IntegerType::get(type.getContext(), type.getWidth()));
  });

  ConversionTarget target(*context);
  target.addIllegalDialect<RTLILDialect>();
  target.addLegalDialect<comb::CombDialect, hw::HWDialect, llhd::LLHDDialect,
                         seq::SeqDialect>();

  ConversionPatternSet patterns(context, typeConverter);
  patterns.add<rtlil::ModuleOp>(convert, typeConverter);
  patterns.add<WireOp>(convert, typeConverter);
  patterns.add<WConnectionOp>(convert);
  patterns.add<ConstOp>(convert, typeConverter);
  patterns.add<SliceOp>(convert, typeConverter);
  patterns.add<ConcatOp>(convert, typeConverter);
  patterns.add<rtlil::AndOp>(convertFixedOperands<rtlil::AndOp, comb::AndOp>);
  patterns.add<rtlil::OrOp>(convertFixedOperands<rtlil::OrOp, comb::OrOp>);
  patterns.add<rtlil::XorOp>(convertFixedOperands<rtlil::XorOp, comb::XorOp>);
  patterns.add<rtlil::AddOp>(convertFixedOperands<rtlil::AddOp, comb::AddOp>);
  patterns.add<rtlil::SubOp>(convertFixedOperands<rtlil::SubOp, comb::SubOp>);
  patterns.add<rtlil::MulOp>(convertFixedOperands<rtlil::MulOp, comb::MulOp>);
  patterns.add<rtlil::ShlOp>(convertFixedOperands<rtlil::ShlOp, comb::ShlOp>);
  patterns.add<rtlil::ShrOp>(convertFixedOperands<rtlil::ShrOp, comb::ShrUOp>);
  patterns.add<rtlil::ReduceXorOp>(
      convertFixedOperands<rtlil::ReduceXorOp, comb::ParityOp>);
  using comb::ICmpPredicate;
  patterns.add<LTOp>(convertCmp<LTOp, ICmpPredicate::ult, ICmpPredicate::slt>);
  patterns.add<LEOp>(convertCmp<LEOp, ICmpPredicate::ule, ICmpPredicate::sle>);
  patterns.add<GTOp>(convertCmp<GTOp, ICmpPredicate::ugt, ICmpPredicate::sgt>);
  patterns.add<GEOp>(convertCmp<GEOp, ICmpPredicate::uge, ICmpPredicate::sge>);
  patterns.add<EQOp>(convertCmp<EQOp, ICmpPredicate::eq>);
  patterns.add<NEOp>(convertCmp<NEOp, ICmpPredicate::ne>);
  patterns.add<EQXOp>(convertCmp<EQXOp, ICmpPredicate::ceq>);
  patterns.add<NEXOp>(convertCmp<NEXOp, ICmpPredicate::cne>);
  patterns.add<rtlil::MuxOp>(convert);
  patterns.add<DFFOp>(convert);

  if (failed(
          applyPartialConversion(getOperation(), target, std::move(patterns))))
    signalPassFailure();
}
