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

/// The ports of each `rtlil.module`, as the `hw.module` it becomes declares
/// them. Collected before the conversion, so instances can refer to modules
/// that are already converted.
using ModulePortMap = DenseMap<StringAttr, hw::ModulePortInfo>;

/// Returns whether `use` connects its value to a port the user drives.
static bool isDriven(OpOperand &use, const ModulePortMap &modulePorts) {
  if (isa<WConnectionOp>(use.getOwner()))
    return use.getOperandNumber() == 0;
  // The callee of an instance may already be converted, so its port wires
  // cannot be consulted. Use the collected ports instead.
  if (auto instance = dyn_cast<rtlil::InstanceOp>(use.getOwner())) {
    auto it = modulePorts.find(instance.getTypeAttr().getAttr());
    if (it == modulePorts.end())
      return false;
    auto name =
        makeLocal(instance.getContext(),
                  cast<StringAttr>(instance.getPorts()[use.getOperandNumber()])
                      .getValue());
    return llvm::any_of(it->second.getOutputs(),
                        [&](const auto &port) { return port.name == name; });
  }
  if (auto cell = dyn_cast<CellOpInterface>(use.getOwner()))
    return cell.getCellPortDirection(use.getOperandNumber()) ==
           PortDirection::Output;
  return false;
}

//===----------------------------------------------------------------------===//
// Structure
//===----------------------------------------------------------------------===//

/// Collects the ports of all modules, in `port_id` order.
static LogicalResult collectModulePorts(mlir::ModuleOp root,
                                        ModulePortMap &modulePorts) {
  for (auto module : root.getOps<rtlil::ModuleOp>()) {
    SmallVector<WireOp> portWires;
    module.getPortWires(portWires);
    SmallVector<hw::PortInfo> ports;
    for (auto wire : portWires) {
      if (wire.isPortInput() && wire.isPortOutput())
        return wire.emitOpError("inout ports are not supported");
      hw::PortInfo port;
      port.name = makeLocal(root.getContext(), wire.getName());
      port.type = getIntType(wire.getResult());
      port.dir = wire.isPortInput() ? hw::ModulePort::Direction::Input
                                    : hw::ModulePort::Direction::Output;
      port.loc = wire.getLoc();
      ports.push_back(port);
    }
    modulePorts.try_emplace(module.getSymNameAttr(), ports);
  }
  return success();
}

/// Create the module from its collected ports.
static LogicalResult convert(rtlil::ModuleOp op, rtlil::ModuleOp::Adaptor,
                             ConversionPatternRewriter &rewriter,
                             const TypeConverter &typeConverter,
                             const ModulePortMap &modulePorts) {
  auto *context = op.getContext();

  SmallVector<WireOp> portWires;
  op.getPortWires(portWires);
  const auto &ports = modulePorts.at(op.getSymNameAttr());

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
                             const TypeConverter &typeConverter,
                             const ModulePortMap &modulePorts) {
  auto uses = op.getResult().getUses();
  auto isUseDriven = [&](OpOperand &use) { return isDriven(use, modulePorts); };
  bool driven = llvm::any_of(uses, isUseDriven);
  if (driven && !llvm::all_of(uses, isUseDriven))
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

/// Drives Y of a unary cell with `fn` applied to A.
template <typename CellOp>
static LogicalResult convertUnary(CellOp op, typename CellOp::Adaptor adaptor,
                                  ConversionPatternRewriter &rewriter,
                                  llvm::function_ref<Value(Value)> fn) {
  auto loc = op.getLoc();
  auto connections = adaptor.getConnections();
  Value input = llhd::ProbeOp::create(rewriter, loc, connections[0]);
  drive(rewriter, loc, connections[1], fn(input));
  rewriter.eraseOp(op);
  return success();
}

/// `$not` is an XOR with all ones, as in MooreToCore.
static LogicalResult convert(NotOp op, NotOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  return convertUnary(op, adaptor, rewriter, [&](Value input) {
    return comb::createOrFoldNot(rewriter, op.getLoc(), input);
  });
}

/// `$neg` is a subtraction from zero.
static LogicalResult convert(NegOp op, NegOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  return convertUnary(op, adaptor, rewriter, [&](Value input) -> Value {
    auto zero =
        hw::ConstantOp::create(rewriter, op.getLoc(), input.getType(), 0);
    return comb::SubOp::create(rewriter, op.getLoc(), zero, input);
  });
}

/// `$pos` passes A through.
static LogicalResult convert(PosOp op, PosOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  return convertUnary(op, adaptor, rewriter, [](Value input) { return input; });
}

/// Compares `value` against the constant `bits`, sign-extended to its width.
static Value compareTo(OpBuilder &builder, Location loc,
                       comb::ICmpPredicate pred, Value value, int64_t bits) {
  auto constant = hw::ConstantOp::create(builder, loc, value.getType(), bits);
  return comb::ICmpOp::create(builder, loc, pred, value, constant);
}

/// Whether any bit of `value` is set, the truth value of the logic cells.
static Value isTrue(OpBuilder &builder, Location loc, Value value) {
  return compareTo(builder, loc, comb::ICmpPredicate::ne, value, 0);
}

static LogicalResult convert(ReduceAndOp op, ReduceAndOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  return convertUnary(op, adaptor, rewriter, [&](Value input) {
    return compareTo(rewriter, op.getLoc(), comb::ICmpPredicate::eq, input, -1);
  });
}

/// `$reduce_or` and `$reduce_bool` are the same function.
template <typename CellOp>
static LogicalResult convertReduceOr(CellOp op,
                                     typename CellOp::Adaptor adaptor,
                                     ConversionPatternRewriter &rewriter) {
  return convertUnary(op, adaptor, rewriter, [&](Value input) {
    return isTrue(rewriter, op.getLoc(), input);
  });
}

static LogicalResult convert(ReduceXnorOp op, ReduceXnorOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  return convertUnary(op, adaptor, rewriter, [&](Value input) {
    auto parity = comb::ParityOp::create(rewriter, op.getLoc(), input);
    return comb::createOrFoldNot(rewriter, op.getLoc(), parity);
  });
}

static LogicalResult convert(LogicNotOp op, LogicNotOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  return convertUnary(op, adaptor, rewriter, [&](Value input) {
    return compareTo(rewriter, op.getLoc(), comb::ICmpPredicate::eq, input, 0);
  });
}

/// `$logic_and` and `$logic_or` combine the truth values of A and B.
template <typename CellOp, typename CombOp>
static LogicalResult convertLogic(CellOp op, typename CellOp::Adaptor adaptor,
                                  ConversionPatternRewriter &rewriter) {
  auto loc = op.getLoc();
  auto connections = adaptor.getConnections();
  Value lhs = isTrue(rewriter, loc,
                     llhd::ProbeOp::create(rewriter, loc, connections[0]));
  Value rhs = isTrue(rewriter, loc,
                     llhd::ProbeOp::create(rewriter, loc, connections[1]));
  drive(rewriter, loc, connections[2], CombOp::create(rewriter, loc, lhs, rhs));
  rewriter.eraseOp(op);
  return success();
}

/// Cells whose Comb op depends on the signedness of the operands.
template <typename CellOp, typename UnsignedOp, typename SignedOp>
static LogicalResult convertSigned(CellOp op, typename CellOp::Adaptor adaptor,
                                   ConversionPatternRewriter &rewriter) {
  if (op.getOpsSigned())
    return convertFixedOperands<CellOp, SignedOp>(op, adaptor, rewriter);
  return convertFixedOperands<CellOp, UnsignedOp>(op, adaptor, rewriter);
}

/// `$sshr` only shifts in the sign bit if A is signed.
static LogicalResult convert(SShrOp op, SShrOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter) {
  if (op.getASigned())
    return convertFixedOperands<SShrOp, comb::ShrSOp>(op, adaptor, rewriter);
  return convertFixedOperands<SShrOp, comb::ShrUOp>(op, adaptor, rewriter);
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

/// An instance becomes `hw.instance`. Connections to inputs are probed and
/// connections to outputs driven, following the ports of the callee.
static LogicalResult convert(rtlil::InstanceOp op,
                             rtlil::InstanceOp::Adaptor adaptor,
                             ConversionPatternRewriter &rewriter,
                             const ModulePortMap &modulePorts) {
  auto *context = op.getContext();
  auto loc = op.getLoc();
  if (!op.getParameters().empty())
    return op.emitOpError("with parameters is not supported");
  auto it = modulePorts.find(op.getTypeAttr().getAttr());
  if (it == modulePorts.end())
    return failure();
  const auto &ports = it->second;

  DenseMap<StringAttr, Value> connections;
  for (auto [port, connection] :
       llvm::zip(op.getPorts(), adaptor.getConnections()))
    connections.try_emplace(
        makeLocal(context, cast<StringAttr>(port).getValue()), connection);

  SmallVector<Value> inputs;
  SmallVector<Attribute> argNames;
  for (const auto &port : ports.getInputs()) {
    Value connection = connections.lookup(port.name);
    if (!connection)
      return op.emitOpError("leaves input port '")
             << port.name.getValue() << "' unconnected";
    inputs.push_back(llhd::ProbeOp::create(rewriter, loc, connection));
    argNames.push_back(port.name);
  }
  SmallVector<Type> resultTypes;
  SmallVector<Attribute> resultNames;
  for (const auto &port : ports.getOutputs()) {
    resultTypes.push_back(port.type);
    resultNames.push_back(port.name);
  }

  auto instance = hw::InstanceOp::create(
      rewriter, loc, resultTypes, makeLocal(context, op.getName()),
      FlatSymbolRefAttr::get(makeLocal(context, op.getType())), inputs,
      rewriter.getArrayAttr(argNames), rewriter.getArrayAttr(resultNames),
      rewriter.getArrayAttr({}), hw::InnerSymAttr{});
  // An unconnected output is left unused.
  for (auto [port, result] :
       llvm::zip(ports.getOutputs(), instance.getResults()))
    if (Value connection = connections.lookup(port.name))
      drive(rewriter, loc, connection, result);
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

  ModulePortMap modulePorts;
  if (failed(collectModulePorts(getOperation(), modulePorts)))
    return signalPassFailure();

  ConversionPatternSet patterns(context, typeConverter);
  patterns.add<rtlil::ModuleOp>(convert, modulePorts);
  patterns.add<rtlil::InstanceOp>(convert, modulePorts);
  patterns.add<WireOp>(convert, typeConverter);
  patterns.add<WConnectionOp>(convert);
  patterns.add<ConstOp>(convert, typeConverter);
  patterns.add<SliceOp>(convert, typeConverter);
  patterns.add<ConcatOp>(convert, modulePorts);
  patterns.add<rtlil::AndOp>(convertFixedOperands<rtlil::AndOp, comb::AndOp>);
  patterns.add<rtlil::OrOp>(convertFixedOperands<rtlil::OrOp, comb::OrOp>);
  patterns.add<rtlil::XorOp>(convertFixedOperands<rtlil::XorOp, comb::XorOp>);
  patterns.add<rtlil::AddOp>(convertFixedOperands<rtlil::AddOp, comb::AddOp>);
  patterns.add<rtlil::SubOp>(convertFixedOperands<rtlil::SubOp, comb::SubOp>);
  patterns.add<rtlil::MulOp>(convertFixedOperands<rtlil::MulOp, comb::MulOp>);
  patterns.add<rtlil::DivOp>(
      convertSigned<rtlil::DivOp, comb::DivUOp, comb::DivSOp>);
  patterns.add<rtlil::ModOp>(
      convertSigned<rtlil::ModOp, comb::ModUOp, comb::ModSOp>);
  patterns.add<rtlil::ShlOp>(convertFixedOperands<rtlil::ShlOp, comb::ShlOp>);
  patterns.add<rtlil::ShrOp>(convertFixedOperands<rtlil::ShrOp, comb::ShrUOp>);
  patterns.add<SShrOp>(convert);
  patterns.add<NotOp>(convert);
  patterns.add<NegOp>(convert);
  patterns.add<PosOp>(convert);
  patterns.add<ReduceAndOp>(convert);
  patterns.add<ReduceOrOp>(convertReduceOr<ReduceOrOp>);
  patterns.add<ReduceBoolOp>(convertReduceOr<ReduceBoolOp>);
  patterns.add<ReduceXnorOp>(convert);
  patterns.add<LogicNotOp>(convert);
  patterns.add<LogicAndOp>(convertLogic<LogicAndOp, comb::AndOp>);
  patterns.add<LogicOrOp>(convertLogic<LogicOrOp, comb::OrOp>);
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
