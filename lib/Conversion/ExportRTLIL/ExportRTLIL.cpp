//===- ExportRTLIL.cpp - RTLIL dialect to a Yosys design ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Walks the RTLIL dialect into a `Yosys::RTLIL::Design`. The walk is
// deliberately mechanical: the dialect already carries cell types, port names
// and parameters in the shape Yosys wants, so there is no case per Comb or HW
// operation here -- that knowledge lives in `convert-hw-to-rtlil`.
//
// The one thing this file does that its proof-of-concept predecessor did not is
// check its input first. Yosys signals a duplicate name or a malformed
// identifier with `log_error`, which ends the process; there is no way to catch
// it and no diagnostic to show the user. So `validate()` runs to completion
// before a single Yosys object is created, and everything it can reject becomes
// an ordinary MLIR error.
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/ExportRTLIL.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "circt/Yosys/Yosys.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/Tools/mlir-translate/Translation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>
#include <string>
#include <vector>

// The Yosys headers come last, and everything below stays explicitly qualified
// with `Yosys::`. Yosys' own sources use `USING_YOSYS_NAMESPACE`; an embedder
// does not want that namespace pulled into its translation units.
#include "kernel/rtlil.h"
#include "kernel/yosys.h"

using namespace circt;
using namespace mlir;

//===----------------------------------------------------------------------===//
// Small helpers
//===----------------------------------------------------------------------===//

/// RTLIL identifiers carry their own escaping: the dialect stores names with
/// the leading `\` (public) or `$` (auto-generated) that Yosys expects, so they
/// go through unchanged. Only ever called on names `validate()` has accepted.
static Yosys::RTLIL::IdString id(StringRef name) {
  return Yosys::RTLIL::IdString(std::string_view(name.data(), name.size()));
}

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
  // Least significant bit first, the order both `#rtlil.const` and
  // `RTLIL::Const` store.
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
    // Expand the APInt bit by bit rather than going through `getInt()`, which
    // truncates -- and asserts -- above 64 bits. `$lut` masks are routinely
    // wider than that.
    const llvm::APInt &intVal = cast<IntegerAttr>(value).getValue();
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

/// Render `loc` in the `file:line.col-line.col` form Yosys uses for its `src`
/// attribute, or the empty string when there is nothing to say. A `FusedLoc`
/// becomes the `|`-joined form Yosys itself produces when a pass merges cells.
///
/// This is the only channel by which locations survive a Yosys script: between
/// `exportRTLIL` and `importRTLIL` the design belongs to Yosys, and `src` is
/// the one place it will carry them.
static std::string getSrcAttribute(Location loc) {
  llvm::SmallVector<std::string> pieces;
  std::function<void(Location)> collect = [&](Location current) {
    if (auto fileLoc = dyn_cast<FileLineColLoc>(current)) {
      pieces.push_back((fileLoc.getFilename().getValue() + ":" +
                        Twine(fileLoc.getLine()) + "." +
                        Twine(fileLoc.getColumn()) + "-" +
                        Twine(fileLoc.getLine()) + "." +
                        Twine(fileLoc.getColumn()))
                           .str());
      return;
    }
    if (auto fused = dyn_cast<FusedLoc>(current)) {
      for (Location nested : fused.getLocations())
        collect(nested);
      return;
    }
    if (auto named = dyn_cast<NameLoc>(current))
      collect(named.getChildLoc());
  };
  collect(loc);
  return llvm::join(pieces, "|");
}

namespace {
/// Translates the ops of one `rtlil.module` into a Yosys module.
class ModuleEmitter {
public:
  ModuleEmitter(Yosys::RTLIL::Design *design) : design(design) {}

  /// Check everything Yosys would react to fatally. Runs to completion before
  /// any Yosys object exists.
  LogicalResult validate(rtlil::ModuleOp op);

  /// Emit `op` into the design. Only valid after a successful `validate()`.
  LogicalResult emit(rtlil::ModuleOp op);

private:
  /// The signal a value stands for, building slices and concatenations on the
  /// way. Memoized, so a shared sub-expression is walked once.
  ///
  /// Returns by value rather than by reference into `signals`: a nested lookup
  /// can insert and so rehash the map, which would dangle a reference handed
  /// out earlier in the same expression.
  std::optional<Yosys::RTLIL::SigSpec> lookup(Value value, Operation *user);

  LogicalResult emitWire(rtlil::WireOp op);
  LogicalResult emitConnection(rtlil::WConnectionOp op);
  LogicalResult emitCell(rtlil::CellOpInterface op);

  /// Copy an `rtlil_attributes` dict onto a Yosys object, adding `src` from the
  /// op's location.
  void setAttributes(Yosys::RTLIL::AttrObject *object, ArrayAttr attributes,
                     Location loc);

  Yosys::RTLIL::Design *design;
  Yosys::RTLIL::Module *module = nullptr;
  llvm::DenseMap<Value, Yosys::RTLIL::SigSpec> signals;
};
} // namespace

//===----------------------------------------------------------------------===//
// Validation
//===----------------------------------------------------------------------===//

LogicalResult ModuleEmitter::validate(rtlil::ModuleOp op) {
  StringRef name = op.getSymName();
  if (!rtlil::isValidIdentifier(name))
    return op.emitError("module name '")
           << name
           << "' is not a valid RTLIL identifier; it must start with '\\' or "
              "'$' and contain no spaces or control characters";
  if (design->has(id(name)))
    return op.emitError("design already contains a module named ") << name;

  // Wires and cells share one namespace inside an `RTLIL::Module`. The dialect
  // verifier checks this too, but the exporter must not depend on having been
  // handed verified IR -- `circt-translate` can be given anything that parses.
  llvm::DenseMap<StringRef, Operation *> declared;
  for (Operation &nested : op.getBodyBlock()->getOperations()) {
    StringRef declaredName;
    if (auto wire = dyn_cast<rtlil::WireOp>(nested))
      declaredName = wire.getName();
    else if (auto cell = dyn_cast<rtlil::CellOpInterface>(nested))
      declaredName = cell.getCellName();
    else
      continue;

    if (!rtlil::isValidIdentifier(declaredName))
      return nested.emitError("name '")
             << declaredName << "' is not a valid RTLIL identifier";
    auto [it, inserted] = declared.try_emplace(declaredName, &nested);
    if (!inserted)
      return nested.emitError("redeclares the RTLIL name ")
                 .append(declaredName)
                 .attachNote(it->second->getLoc())
             << "previously declared here; wires and cells share one namespace";
  }

  // A cell's port names and connections are index-parallel arrays.
  for (auto cell : op.getBodyBlock()->getOps<rtlil::CellOpInterface>()) {
    ArrayAttr ports = cell.getCellPorts();
    if (ports.size() != cell.getCellConnections().size())
      return cell->emitError("cell has ")
             << ports.size() << " port names but "
             << cell.getCellConnections().size() << " connections";
    for (Attribute port : ports)
      if (!rtlil::isValidIdentifier(cast<StringAttr>(port).getValue()))
        return cell->emitError("port name '")
               << cast<StringAttr>(port).getValue()
               << "' is not a valid RTLIL identifier";
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
    object->attributes[id(parameter.getName().getValue())] =
        toConst(parameter.getValue(), parameter.getFlags());
  }
  std::string src = getSrcAttribute(loc);
  if (!src.empty())
    object->set_src_attribute(src);
}

LogicalResult ModuleEmitter::emit(rtlil::ModuleOp op) {
  module = design->addModule(id(op.getSymName()));
  signals.clear();
  setAttributes(module, op.getRtlilAttributes(), op.getLoc());
  for (Attribute parameter : op.getAvailParameters())
    module->avail_parameters(id(cast<StringAttr>(parameter).getValue()));

  // Two passes over the body. The region is a graph region, so a cell may
  // precede the wire it drives; and even in a topologically sorted body a
  // connection may name a wire declared later.
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
      return nested.emitError("no RTLIL mapping for this operation");
    }
  }

  // Collects the wires flagged as ports into `module->ports` and renumbers
  // their `port_id`s. The dialect verifier guarantees the ids are already
  // exactly 1..N, so this only builds the list.
  module->fixup_ports();
  return success();
}

LogicalResult ModuleEmitter::emitWire(rtlil::WireOp op) {
  auto *wire = module->addWire(
      id(op.getName()),
      cast<rtlil::MValueType>(op.getResult().getType()).getWidth());
  wire->port_id = op.getPortId();
  wire->port_input = op.getPortInput();
  wire->port_output = op.getPortOutput();
  wire->start_offset = op.getStartOffset();
  wire->upto = op.getUpto();
  wire->is_signed = op.getIsSigned();
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
    user->emitError("operand has no defining operation");
    return std::nullopt;
  }

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
    // `SigSpec::append` adds at the most significant end, and the operands are
    // already ordered least significant first.
    for (Value input : concat.getInputs()) {
      auto piece = lookup(input, concat);
      if (!piece)
        return std::nullopt;
      spec.append(*piece);
    }
  } else {
    user->emitError("operand has no RTLIL signal; expected it to be defined by "
                    "an rtlil.wire, rtlil.const, rtlil.slice or rtlil.concat");
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

  auto *cell = module->addCell(id(op.getCellName()), id(op.getCellType()));
  for (auto [port, value] : llvm::zip(ports, connections)) {
    auto signal = lookup(value, op);
    if (!signal)
      return failure();
    cell->setPort(id(cast<StringAttr>(port).getValue()), *signal);
  }
  for (Attribute parameter : op.getCellParameters()) {
    auto param = cast<rtlil::ParameterAttr>(parameter);
    cell->setParam(id(param.getName().getValue()),
                   toConst(param.getValue(), param.getFlags()));
  }
  // Only `rtlil.cell` and `rtlil.instance` carry an attribute dict; the
  // fixed-shape cell ops (`rtlil.and`, `rtlil.dff`, ...) have none, and still
  // want `src` set from their location.
  auto attributes = op->getAttrOfType<ArrayAttr>("rtlil_attributes");
  setAttributes(cell, attributes ? attributes : ArrayAttr::get(op->getContext(), {}),
                op->getLoc());
  return success();
}

//===----------------------------------------------------------------------===//
// Entry points
//===----------------------------------------------------------------------===//

LogicalResult circt::rtlil::exportRTLILModule(rtlil::ModuleOp module,
                                              Yosys::RTLIL::Design *design) {
  ModuleEmitter emitter(design);
  if (failed(emitter.validate(module)))
    return failure();
  return emitter.emit(module);
}

LogicalResult circt::rtlil::exportRTLIL(mlir::ModuleOp module,
                                        Yosys::RTLIL::Design *design) {
  auto modules = llvm::to_vector(module.getOps<rtlil::ModuleOp>());

  // Validate everything first: after the first `addModule()` a diagnostic can
  // no longer leave `design` untouched.
  ModuleEmitter validator(design);
  llvm::DenseMap<StringRef, Operation *> seen;
  for (auto nested : modules) {
    if (failed(validator.validate(nested)))
      return failure();
    // `validate` only sees what is already in `design`, so duplicates *among*
    // the modules being exported have to be caught here.
    auto [it, inserted] = seen.try_emplace(nested.getSymName(), nested);
    if (!inserted)
      return nested.emitError("design already contains a module named ")
                 .append(nested.getSymName())
                 .attachNote(it->second->getLoc())
             << "previously defined here";
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

void circt::rtlil::registerExportRTLILTranslation() {
  static mlir::TranslateFromMLIRRegistration toRTLIL(
      "export-rtlil", "export the RTLIL dialect as an RTLIL (.il) file",
      [](mlir::ModuleOp module, llvm::raw_ostream &os) -> LogicalResult {
        if (auto error = circt::yosys::initialize())
          return module.emitError("failed to initialize Yosys: ")
                 << llvm::toString(std::move(error));

        Yosys::RTLIL::Design design;
        if (failed(exportRTLIL(module, &design)))
          return failure();

        // Yosys' own RTLIL backend does the printing. It writes to a
        // `std::ostream`, so the result is buffered and copied across rather
        // than streamed.
        std::ostringstream stream;
        std::ostream *streamPtr = &stream;
        Yosys::Backend::backend_call(&design, streamPtr, "<stdout>", "rtlil");
        os << stream.str();
        return success();
      },
      [](DialectRegistry &registry) {
        registry.insert<circt::rtlil::RTLILDialect>();
      });
}
