//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Walks the RTLIL dialect into a `Yosys::RTLIL::Design`. Yosys reports bad
// names with `log_error`, which ends the process, so `validate()` checks the
// input before any Yosys object is created.
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/ExportRTLIL.h"
#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/Debug/DebugDialect.h"
#include "circt/Dialect/Emit/EmitDialect.h"
#include "circt/Dialect/HW/HWDialect.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/LTL/LTLDialect.h"
#include "circt/Dialect/OM/OMDialect.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "circt/Dialect/SV/SVDialect.h"
#include "circt/Dialect/Seq/SeqDialect.h"
#include "circt/Dialect/Sim/SimDialect.h"
#include "circt/Dialect/Verif/VerifDialect.h"
#include "circt/Yosys/Yosys.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/Tools/mlir-translate/Translation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "circt/Yosys/RTLILUtils.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"

using namespace circt;
using namespace mlir;
using yosys::circtLocAttrName;
using yosys::toIdString;

//===----------------------------------------------------------------------===//
// Small helpers
//===----------------------------------------------------------------------===//

static Yosys::RTLIL::State toState(rtlil::StateEnum state) {
  switch (state) {
  case rtlil::StateEnum::S0:
    return Yosys::RTLIL::State::S0;
  case rtlil::StateEnum::S1:
    return Yosys::RTLIL::State::S1;
  case rtlil::StateEnum::Sx:
    return Yosys::RTLIL::State::Sx;
  case rtlil::StateEnum::Sz:
    return Yosys::RTLIL::State::Sz;
  case rtlil::StateEnum::Sa:
    return Yosys::RTLIL::State::Sa;
  }
  llvm_unreachable("unhandled RTLIL state");
}

static std::vector<Yosys::RTLIL::State> toBits(rtlil::ConstAttr value) {
  std::vector<Yosys::RTLIL::State> bits;
  bits.reserve(value.size());
  // Least significant bit first, as `RTLIL::Const` stores it.
  for (rtlil::StateEnum bit : value.getBits())
    bits.push_back(toState(bit));
  return bits;
}

/// An `RTLIL::Const` for one `#rtlil.param` value.
static Yosys::RTLIL::Const toConst(Attribute value,
                                   std::optional<uint16_t> flags) {
  Yosys::RTLIL::Const result;
  if (auto str = dyn_cast<StringAttr>(value)) {
    result = Yosys::RTLIL::Const(str.getValue().str());
  } else if (auto bitVector = dyn_cast<rtlil::ConstAttr>(value)) {
    result = Yosys::RTLIL::Const(toBits(bitVector));
  } else {
    // Iterate the APInt, since `getInt()` asserts above 64 bits.
    const APInt &intVal = cast<IntegerAttr>(value).getValue();
    std::vector<Yosys::RTLIL::State> bits;
    bits.reserve(intVal.getBitWidth());
    for (unsigned i = 0, e = intVal.getBitWidth(); i != e; ++i)
      bits.push_back(intVal[i] ? Yosys::RTLIL::State::S1
                               : Yosys::RTLIL::State::S0);
    result = Yosys::RTLIL::Const(bits);
  }
  if (flags)
    result.flags = *flags;
  return result;
}

/// Render `loc` as a Yosys `src` attribute: `file:line.col-line.col`, joined
/// with `|` for a `FusedLoc`. Empty when there is no file location.
static std::string getSrcAttribute(Location loc) {
  SmallVector<std::string> pieces;
  std::function<void(Location)> collect = [&](Location current) {
    // A `FileLineColLoc` is a point range, so this matches both.
    if (auto fileLoc = dyn_cast<FileLineColRange>(current)) {
      pieces.push_back((fileLoc.getFilename().getValue() + ":" +
                        Twine(fileLoc.getStartLine()) + "." +
                        Twine(fileLoc.getStartColumn()) + "-" +
                        Twine(fileLoc.getEndLine()) + "." +
                        Twine(fileLoc.getEndColumn()))
                           .str());
      return;
    }
    if (auto fused = dyn_cast<FusedLoc>(current)) {
      for (Location nested : fused.getLocations())
        collect(nested);
      return;
    }
    if (auto named = dyn_cast<NameLoc>(current)) {
      collect(named.getChildLoc());
      return;
    }
    // A flat `src` cannot hold a stack; `circt.loc` keeps the full location.
    if (auto callsite = dyn_cast<CallSiteLoc>(current))
      collect(callsite.getCallee());
  };
  collect(loc);
  return llvm::join(pieces, "|");
}

static std::string getCirctLocAttribute(Location loc) {
  if (isa<UnknownLoc>(loc))
    return {};
  std::string result;
  llvm::raw_string_ostream os(result);
  loc.print(os);
  return result;
}

namespace {
/// Translates the ops of one `rtlil.module` into a Yosys module.
class ModuleEmitter {
public:
  explicit ModuleEmitter(Yosys::RTLIL::Design *design) : design(design) {}

  /// Check everything Yosys would treat as fatal.
  LogicalResult validate(rtlil::ModuleOp op);

  /// Emit `op` into the design. Only valid after a successful `validate()`.
  LogicalResult emit(rtlil::ModuleOp op);

private:
  /// The memoized signal for `value`. Returns by value, as nested lookups may
  /// rehash `signals`.
  std::optional<Yosys::RTLIL::SigSpec> lookup(Value value, Operation *user);

  LogicalResult emitWire(rtlil::WireOp op);
  LogicalResult emitConnection(rtlil::WConnectionOp op);
  LogicalResult emitCell(rtlil::CellOpInterface op);

  /// Copy `attributes` onto `object` and add the location attributes.
  void setAttributes(Yosys::RTLIL::AttrObject *object, ArrayAttr attributes,
                     Location loc);

  Yosys::RTLIL::Design *design;
  Yosys::RTLIL::Module *module = nullptr;
  DenseMap<Value, Yosys::RTLIL::SigSpec> signals;
  /// Values `lookup` is visiting, to detect cyclic slice/concat chains.
  DenseSet<Value> visiting;
};
} // namespace

//===----------------------------------------------------------------------===//
// Validation
//===----------------------------------------------------------------------===//

LogicalResult ModuleEmitter::validate(rtlil::ModuleOp op) {
  StringRef name = op.getSymName();
  if (failed(rtlil::verifyIdentifier(op, "module name", name)))
    return failure();
  if (design->has(toIdString(name)))
    return op.emitOpError("redefines module '") << name << "'";

  // Repeats the dialect verifiers on purpose: callers of `exportRTLIL()` may
  // pass unverified IR, and Yosys ends the process on a bad or duplicate name.
  DenseMap<StringRef, Operation *> declared;
  for (Operation &nested : op.getBodyBlock()->getOperations()) {
    StringRef declaredName;
    if (auto wire = dyn_cast<rtlil::WireOp>(nested))
      declaredName = wire.getName();
    else if (auto cell = dyn_cast<rtlil::CellOpInterface>(nested))
      declaredName = cell.getCellName();
    else
      continue;

    if (failed(rtlil::verifyIdentifier(&nested, "name", declaredName)))
      return failure();
    auto [it, inserted] = declared.try_emplace(declaredName, &nested);
    if (!inserted) {
      auto diag = nested.emitOpError("redeclares name '")
                  << declaredName << "'";
      diag.attachNote(it->second->getLoc())
          << "previously declared here; wires and cells share one namespace";
      return diag;
    }
  }

  // A cell's port names and connections are index-parallel arrays.
  for (auto cell : op.getBodyBlock()->getOps<rtlil::CellOpInterface>()) {
    ArrayAttr ports = cell.getCellPorts();
    if (ports.size() != cell.getCellConnections().size())
      return cell->emitOpError("has ")
             << ports.size() << " port names but "
             << cell.getCellConnections().size() << " connections";
    for (Attribute port : ports)
      if (failed(rtlil::verifyIdentifier(cell, "port name",
                                         cast<StringAttr>(port).getValue())))
        return failure();
  }

  return success();
}

//===----------------------------------------------------------------------===//
// Emission
//===----------------------------------------------------------------------===//

void ModuleEmitter::setAttributes(Yosys::RTLIL::AttrObject *object,
                                  ArrayAttr attributes, Location loc) {
  for (Attribute entry : attributes) {
    auto parameter = cast<rtlil::ParameterAttr>(entry);
    object->attributes[toIdString(parameter.getName().getValue())] =
        toConst(parameter.getValue(), parameter.getFlags());
  }
  std::string src = getSrcAttribute(loc);
  if (!src.empty())
    object->set_src_attribute(src);
  std::string exact = getCirctLocAttribute(loc);
  if (!exact.empty())
    object->set_string_attribute(toIdString(circtLocAttrName), exact);
}

LogicalResult ModuleEmitter::emit(rtlil::ModuleOp op) {
  module = design->addModule(toIdString(op.getSymName()));
  signals.clear();
  setAttributes(module, op.getRtlilAttributes(), op.getLoc());
  for (Attribute parameter : op.getAvailParameters())
    module->avail_parameters(
        toIdString(cast<StringAttr>(parameter).getValue()));

  // Wires first, as cells and connections may precede the wires they use.
  for (Operation &nested : op.getBodyBlock()->getOperations())
    if (auto wire = dyn_cast<rtlil::WireOp>(nested))
      if (failed(emitWire(wire)))
        return failure();

  for (Operation &nested : op.getBodyBlock()->getOperations()) {
    if (isa<rtlil::WireOp, rtlil::ConstOp, rtlil::SliceOp, rtlil::ConcatOp>(
            nested))
      continue; // Values, materialized on demand by `lookup`.
    if (auto connection = dyn_cast<rtlil::WConnectionOp>(nested)) {
      if (failed(emitConnection(connection)))
        return failure();
    } else if (auto cell = dyn_cast<rtlil::CellOpInterface>(nested)) {
      if (failed(emitCell(cell)))
        return failure();
    } else {
      return nested.emitOpError("is not allowed in an RTLIL module");
    }
  }

  // Builds `module->ports` from the port wires.
  module->fixup_ports();
  return success();
}

LogicalResult ModuleEmitter::emitWire(rtlil::WireOp op) {
  auto *wire = module->addWire(
      toIdString(op.getName()),
      cast<rtlil::MValueType>(op.getResult().getType()).getWidth());
  wire->port_id = op.getPortId();
  wire->port_input = op.isPortInput();
  wire->port_output = op.isPortOutput();
  wire->start_offset = op.getStartOffset();
  wire->upto = op.getUpto();
  wire->is_signed = op.getSignedness() == rtlil::Signedness::Signed;
  setAttributes(wire, op.getRtlilAttributes(), op.getLoc());
  signals.try_emplace(op.getResult(), wire);
  return success();
}

std::optional<Yosys::RTLIL::SigSpec> ModuleEmitter::lookup(Value value,
                                                           Operation *user) {
  if (auto it = signals.find(value); it != signals.end())
    return it->second;

  Operation *definingOp = value.getDefiningOp();
  if (!definingOp) {
    user->emitOpError("has an operand without a defining operation");
    return std::nullopt;
  }

  if (!visiting.insert(value).second) {
    auto diag =
        definingOp->emitOpError("is part of a cyclic slice/concat chain");
    diag.attachNote(user->getLoc()) << "cycle reached again from here";
    return std::nullopt;
  }
  llvm::scope_exit leaveScope([&] { visiting.erase(value); });

  Yosys::RTLIL::SigSpec spec;
  if (auto constant = dyn_cast<rtlil::ConstOp>(definingOp)) {
    spec = Yosys::RTLIL::Const(toBits(constant.getValue()));
  } else if (auto slice = dyn_cast<rtlil::SliceOp>(definingOp)) {
    auto input = lookup(slice.getInput(), slice);
    if (!input)
      return std::nullopt;
    spec = input->extract(
        slice.getOffset(),
        cast<rtlil::MValueType>(slice.getResult().getType()).getWidth());
  } else if (auto concat = dyn_cast<rtlil::ConcatOp>(definingOp)) {
    // Both are least significant first.
    for (Value input : concat.getInputs()) {
      auto piece = lookup(input, concat);
      if (!piece)
        return std::nullopt;
      spec.append(*piece);
    }
  } else {
    user->emitOpError("has an operand not defined by 'rtlil.wire', "
                      "'rtlil.const', 'rtlil.slice' or 'rtlil.concat'");
    return std::nullopt;
  }

  signals.try_emplace(value, spec);
  return spec;
}

LogicalResult ModuleEmitter::emitConnection(rtlil::WConnectionOp op) {
  auto lhs = lookup(op.getLhs(), op);
  auto rhs = lookup(op.getRhs(), op);
  if (!lhs || !rhs)
    return failure();
  module->connect(*lhs, *rhs);
  return success();
}

LogicalResult ModuleEmitter::emitCell(rtlil::CellOpInterface op) {
  ArrayAttr ports = op.getCellPorts();
  OperandRange connections = op.getCellConnections();

  auto *cell = module->addCell(toIdString(op.getCellName()),
                               toIdString(op.getCellType()));
  for (auto [port, value] : llvm::zip(ports, connections)) {
    auto signal = lookup(value, op);
    if (!signal)
      return failure();
    cell->setPort(toIdString(cast<StringAttr>(port).getValue()), *signal);
  }
  for (Attribute parameter : op.getCellParameters()) {
    auto param = cast<rtlil::ParameterAttr>(parameter);
    cell->setParam(toIdString(param.getName().getValue()),
                   toConst(param.getValue(), param.getFlags()));
  }
  // Only `rtlil.cell` and `rtlil.instance` carry attributes.
  ArrayAttr attributes = ArrayAttr::get(op->getContext(), {});
  if (auto generic = dyn_cast<rtlil::CellOp>(op.getOperation()))
    attributes = generic.getRtlilAttributes();
  else if (auto instance = dyn_cast<rtlil::InstanceOp>(op.getOperation()))
    attributes = instance.getRtlilAttributes();
  setAttributes(cell, attributes, op->getLoc());
  return success();
}

//===----------------------------------------------------------------------===//
// Entry points
//===----------------------------------------------------------------------===//

LogicalResult rtlil::exportRTLIL(ArrayRef<rtlil::ModuleOp> modules,
                                 Yosys::RTLIL::Design *design) {
  // Validate all modules first, so nothing Yosys treats as fatal reaches it.
  // Emission can still fail and leave partial modules in `design`.
  ModuleEmitter validator(design);
  DenseMap<StringRef, Operation *> seen;
  for (auto nested : modules) {
    if (failed(validator.validate(nested)))
      return failure();
    // Catch duplicates among the exported modules.
    auto [it, inserted] = seen.try_emplace(nested.getSymName(), nested);
    if (!inserted) {
      auto diag = nested.emitOpError("redefines module '")
                  << nested.getSymName() << "'";
      diag.attachNote(it->second->getLoc()) << "previously defined here";
      return diag;
    }
  }

  for (auto nested : modules) {
    ModuleEmitter emitter(design);
    if (failed(emitter.emit(nested)))
      return failure();
  }
  return success();
}

//===----------------------------------------------------------------------===//
// Translation Registration
//===----------------------------------------------------------------------===//

/// Reject unconverted hardware modules. Other ops, like `om.class`, are
/// dropped silently.
static LogicalResult checkNothingHardwareIsDropped(mlir::ModuleOp module) {
  auto leftovers = llvm::to_vector(module.getOps<hw::HWModuleLike>());
  if (leftovers.empty())
    return success();

  // Not `module.emitError()`, which would print the whole design.
  auto diag = mlir::emitError(module.getLoc())
              << "design contains " << leftovers.size()
              << " unconverted hardware module"
              << (leftovers.size() == 1 ? "" : "s")
              << "; run 'convert-hw-to-rtlil' first";
  for (auto leftover : leftovers)
    diag.attachNote(leftover.getLoc())
        << "'" << leftover.getModuleName() << "' is not an 'rtlil.module'";
  return failure();
}

void rtlil::registerExportRTLILTranslation() {
  static TranslateFromMLIRRegistration toRTLIL(
      "export-rtlil", "export the RTLIL dialect as an RTLIL (.il) file",
      [](mlir::ModuleOp module, raw_ostream &os) -> LogicalResult {
        if (failed(checkNothingHardwareIsDropped(module)))
          return failure();

        if (auto error = yosys::initialize())
          return module.emitError("failed to initialize Yosys: ")
                 << llvm::toString(std::move(error));

        // Keep the backend's log off stderr. Fatal errors still get through.
        yosys::LogCapture capture;

        Yosys::RTLIL::Design design;
        if (failed(exportRTLIL(
                llvm::to_vector(module.getOps<rtlil::ModuleOp>()), &design)))
          return failure();

        // Print with Yosys' RTLIL backend, buffered via `std::ostream`.
        std::ostringstream stream;
        std::ostream *streamPtr = &stream;
        Yosys::Backend::backend_call(&design, streamPtr, "<stdout>", "rtlil");
        os << stream.str();
        return success();
      },
      [](DialectRegistry &registry) {
        // Accept the other dialects a converted firtool output may contain.
        registry.insert<rtlil::RTLILDialect, hw::HWDialect, comb::CombDialect,
                        seq::SeqDialect, sv::SVDialect, sim::SimDialect,
                        verif::VerifDialect, ltl::LTLDialect, om::OMDialect,
                        emit::EmitDialect, debug::DebugDialect>();
      });
}
