//===- ImportRTLIL.cpp - A Yosys design to the RTLIL dialect --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Walks a `Yosys::RTLIL::Design` into `rtlil.module` ops.
//
// Two things make this harder than the export direction. First, a design that
// has been through `opt`, `techmap` or `abc` connects cell ports to arbitrary
// `SigSpec`s -- slices of wires concatenated with constant bits -- which become
// `rtlil.slice` and `rtlil.concat` here. Second, cells arrive with types the
// dialect has no op for (`$_AND_`, `$_DFF_P_`, `$lut`, ...), so every cell
// becomes a generic `rtlil.cell` carrying its ports and parameters explicitly.
// Only a cell whose type names a module in the design becomes `rtlil.instance`.
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/ImportRTLIL.h"
#include "circt/Dialect/RTLIL/RTLIL.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "circt/Yosys/Yosys.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/Tools/mlir-translate/Translation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SourceMgr.h"

#include <sstream>
#include <string>

// Yosys headers last; everything stays explicitly `Yosys::`-qualified.
#include "kernel/rtlil.h"
#include "kernel/yosys.h"

using namespace circt;
using namespace mlir;

//===----------------------------------------------------------------------===//
// Small helpers
//===----------------------------------------------------------------------===//

/// An `RTLIL::IdString` as a `StringRef`. RTLIL names keep their `\`/`$` sigil
/// in the dialect, so nothing is stripped.
static StringRef toStringRef(const Yosys::RTLIL::IdString &name) {
  const char *cstr = name.c_str();
  return StringRef(cstr);
}

namespace {
class Importer {
public:
  Importer(mlir::ModuleOp module)
      : context(module.getContext()), module(module),
        builder(OpBuilder::atBlockEnd(module.getBody())) {}

  LogicalResult importDesign(Yosys::RTLIL::Design *design);

private:
  LogicalResult importModule(Yosys::RTLIL::Module *source);
  LogicalResult importCell(Yosys::RTLIL::Cell *cell, Location loc);

  /// A value carrying the bits of `spec`, built from the wires already
  /// imported plus `rtlil.const`, `rtlil.slice` and `rtlil.concat` as needed.
  Value importSigSpec(const Yosys::RTLIL::SigSpec &spec, Location loc,
                      Operation *diagnosticOp);

  /// The dialect attribute for one `RTLIL::Const`.
  FailureOr<Attribute> importConst(const Yosys::RTLIL::Const &value,
                                   Location loc);

  /// An attribute dict, minus `src` which becomes the Location instead.
  FailureOr<ArrayAttr> importAttributes(const Yosys::RTLIL::AttrObject &object,
                                        Location loc);

  /// The Location for an object, from its `src` attribute.
  Location importLocation(const Yosys::RTLIL::AttrObject &object);

  rtlil::MValueType getType(unsigned width) {
    return rtlil::MValueType::get(context, width);
  }

  MLIRContext *context;
  mlir::ModuleOp module;
  OpBuilder builder;

  /// Wires of the module currently being imported.
  llvm::DenseMap<Yosys::RTLIL::Wire *, Value> wireValues;
  /// Module names in the design, so a cell can be recognised as an instance.
  llvm::DenseSet<StringRef> moduleNames;
  /// The module currently being built, for diagnostics.
  rtlil::ModuleOp currentModule;
};
} // namespace

//===----------------------------------------------------------------------===//
// Locations and attributes
//===----------------------------------------------------------------------===//

/// Parse one `file:line.col-line.col` piece of an RTLIL `src` attribute.
static std::optional<std::pair<StringRef, std::pair<unsigned, unsigned>>>
parseSrcPiece(StringRef piece) {
  // Split at the *last* colon so that a Windows drive letter or a path
  // containing a colon does not confuse the range.
  size_t colon = piece.rfind(':');
  if (colon == StringRef::npos)
    return std::nullopt;
  StringRef file = piece.substr(0, colon);
  StringRef range = piece.substr(colon + 1);

  StringRef begin = range.split('-').first;
  StringRef lineStr = begin.split('.').first;
  StringRef colStr = begin.split('.').second;
  unsigned line = 0, col = 0;
  if (lineStr.getAsInteger(10, line))
    return std::nullopt;
  // A piece may be just `file:line`.
  if (!colStr.empty() && colStr.getAsInteger(10, col))
    col = 0;
  return std::make_pair(file, std::make_pair(line, col));
}

Location Importer::importLocation(const Yosys::RTLIL::AttrObject &object) {
  auto it = object.attributes.find(Yosys::ID::src);
  if (it == object.attributes.end())
    return builder.getUnknownLoc();

  std::string src = it->second.decode_string();
  SmallVector<Location> locations;
  SmallVector<StringRef> pieces;
  StringRef(src).split(pieces, '|', /*MaxSplit=*/-1, /*KeepEmpty=*/false);
  for (StringRef piece : pieces)
    if (auto parsed = parseSrcPiece(piece))
      locations.push_back(FileLineColLoc::get(
          builder.getStringAttr(parsed->first), parsed->second.first,
          parsed->second.second));

  if (locations.empty())
    return builder.getUnknownLoc();
  if (locations.size() == 1)
    return locations.front();
  return builder.getFusedLoc(locations);
}

FailureOr<Attribute> Importer::importConst(const Yosys::RTLIL::Const &value,
                                           Location loc) {
  // The only sound discriminator is the flag. `Const::is_str()` is private, and
  // `Const(long long, int)` uses the string backing internally for byte-multiple
  // widths, so the backing tag says nothing about intent. `decode_string()`
  // also drops NUL bytes, which makes it lossy on anything that is not really a
  // string.
  if (value.flags & Yosys::RTLIL::CONST_FLAG_STRING)
    return Attribute(builder.getStringAttr(value.decode_string()));

  SmallVector<Attribute> bits;
  bits.reserve(value.size());
  for (Yosys::RTLIL::State bit : value.to_bits()) {
    rtlil::StateEnum state;
    switch (bit) {
    case Yosys::RTLIL::State::S0:
      state = rtlil::StateEnum::S0;
      break;
    case Yosys::RTLIL::State::S1:
      state = rtlil::StateEnum::S1;
      break;
    case Yosys::RTLIL::State::Sx:
      state = rtlil::StateEnum::Sx;
      break;
    case Yosys::RTLIL::State::Sz:
      state = rtlil::StateEnum::Sz;
      break;
    case Yosys::RTLIL::State::Sa:
      state = rtlil::StateEnum::Sa;
      break;
    default:
      // `Sm` is a marker a few Yosys passes use internally and has no
      // StateEnumAttr case. Range-check rather than casting an out-of-range
      // value, which would be undefined behaviour rather than a diagnostic.
      return mlir::emitError(loc)
             << "constant contains RTLIL state " << int(bit)
             << ", which the rtlil dialect cannot represent";
    }
    bits.push_back(rtlil::StateEnumAttr::get(context, state));
  }
  return Attribute(builder.getArrayAttr(bits));
}

FailureOr<ArrayAttr>
Importer::importAttributes(const Yosys::RTLIL::AttrObject &object,
                           Location loc) {
  SmallVector<Attribute> entries;
  for (const auto &[name, value] : object.attributes) {
    // `src` becomes the op's Location. Keeping it here as well would duplicate
    // it on every Path B round trip, since export re-derives it from the loc.
    if (name == Yosys::ID::src)
      continue;
    auto imported = importConst(value, loc);
    if (failed(imported))
      return failure();
    entries.push_back(rtlil::ParameterAttr::get(
        context, builder.getStringAttr(toStringRef(name)), *imported,
        std::optional<uint16_t>(value.flags)));
  }
  return builder.getArrayAttr(entries);
}

//===----------------------------------------------------------------------===//
// SigSpec
//===----------------------------------------------------------------------===//

Value Importer::importSigSpec(const Yosys::RTLIL::SigSpec &spec, Location loc,
                              Operation *diagnosticOp) {
  SmallVector<Value> pieces;
  // `chunks()` is ordered least significant first, which is also the operand
  // order `rtlil.concat` uses.
  for (const auto &chunk : spec.chunks()) {
    if (!chunk.wire) {
      // A run of constant bits.
      auto value = importConst(Yosys::RTLIL::Const(chunk.data), loc);
      if (failed(value))
        return {};
      pieces.push_back(builder.create<rtlil::ConstOp>(
          loc, getType(chunk.width), cast<ArrayAttr>(*value)));
      continue;
    }

    auto it = wireValues.find(chunk.wire);
    if (it == wireValues.end()) {
      mlir::emitError(loc) << "signal refers to wire "
                           << toStringRef(chunk.wire->name)
                           << ", which is not in this module";
      return {};
    }
    Value wire = it->second;
    // A chunk covering the whole wire stays bare; `rtlil.slice` appears only
    // where the SigSpec genuinely takes a part.
    if (chunk.offset == 0 && chunk.width == chunk.wire->width) {
      pieces.push_back(wire);
      continue;
    }
    pieces.push_back(builder.create<rtlil::SliceOp>(
        loc, getType(chunk.width), wire, builder.getI32IntegerAttr(
                                              chunk.offset)));
  }

  if (pieces.empty()) {
    mlir::emitError(loc) << "zero-width signal is not representable";
    return {};
  }
  if (pieces.size() == 1)
    return pieces.front();
  return builder.create<rtlil::ConcatOp>(loc, getType(spec.size()), pieces);
}

//===----------------------------------------------------------------------===//
// Cells and modules
//===----------------------------------------------------------------------===//

LogicalResult Importer::importCell(Yosys::RTLIL::Cell *cell, Location loc) {
  SmallVector<Attribute> portNames;
  SmallVector<Value> connections;
  // `connections()` is a sorted dict, so the two arrays stay index-parallel and
  // the order is stable across runs.
  for (const auto &[port, signal] : cell->connections()) {
    Value value = importSigSpec(signal, loc, nullptr);
    if (!value)
      return failure();
    portNames.push_back(builder.getStringAttr(toStringRef(port)));
    connections.push_back(value);
  }

  SmallVector<Attribute> parameters;
  for (const auto &[name, value] : cell->parameters) {
    auto imported = importConst(value, loc);
    if (failed(imported))
      return failure();
    parameters.push_back(rtlil::ParameterAttr::get(
        context, builder.getStringAttr(toStringRef(name)), *imported,
        std::optional<uint16_t>(value.flags)));
  }

  auto attributes = importAttributes(*cell, loc);
  if (failed(attributes))
    return failure();

  StringRef type = toStringRef(cell->type);
  StringAttr name = builder.getStringAttr(toStringRef(cell->name));
  if (moduleNames.contains(type)) {
    builder.create<rtlil::InstanceOp>(
        loc, name, FlatSymbolRefAttr::get(context, type), connections,
        builder.getArrayAttr(portNames), builder.getArrayAttr(parameters),
        *attributes);
    return success();
  }
  builder.create<rtlil::CellOp>(loc, name, builder.getStringAttr(type),
                                connections, builder.getArrayAttr(portNames),
                                builder.getArrayAttr(parameters), *attributes);
  return success();
}

LogicalResult Importer::importModule(Yosys::RTLIL::Module *source) {
  Location loc = importLocation(*source);
  StringRef name = toStringRef(source->name);

  // Reject rather than drop: silently losing a process or a memory changes what
  // the design means.
  if (!source->processes.empty())
    return mlir::emitError(loc)
           << "module " << name
           << " contains processes, which the rtlil dialect cannot represent; "
              "run 'proc' before importing";
  if (!source->memories.empty())
    return mlir::emitError(loc)
           << "module " << name
           << " contains memories, which the rtlil dialect cannot represent; "
              "run 'memory_collect' or 'memory_map' before importing";

  auto attributes = importAttributes(*source, loc);
  if (failed(attributes))
    return failure();

  SmallVector<Attribute> availParameters;
  for (const auto &parameter : source->avail_parameters)
    availParameters.push_back(builder.getStringAttr(toStringRef(parameter)));

  auto moduleOp = builder.create<rtlil::ModuleOp>(
      loc, builder.getStringAttr(name), *attributes,
      builder.getArrayAttr(availParameters));
  currentModule = moduleOp;
  wireValues.clear();

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToEnd(moduleOp.getBodyBlock());

  // Wires first: every SigSpec below refers to them. The body is a graph
  // region, so nothing forces cells to follow, but keeping the order makes the
  // output readable.
  for (auto *wire : source->wires()) {
    Location wireLoc = importLocation(*wire);
    auto wireAttributes = importAttributes(*wire, wireLoc);
    if (failed(wireAttributes))
      return failure();
    auto wireOp = builder.create<rtlil::WireOp>(
        wireLoc, getType(wire->width), builder.getStringAttr(toStringRef(wire->name)),
        builder.getBoolAttr(wire->is_signed),
        builder.getI32IntegerAttr(wire->port_id),
        builder.getI32IntegerAttr(wire->start_offset),
        builder.getBoolAttr(wire->port_input),
        builder.getBoolAttr(wire->port_output),
        builder.getBoolAttr(wire->upto), *wireAttributes);
    wireValues.try_emplace(wire, wireOp.getResult());
  }

  for (auto *cell : source->cells())
    if (failed(importCell(cell, importLocation(*cell))))
      return failure();

  for (const auto &[lhs, rhs] : source->connections()) {
    Value lhsValue = importSigSpec(lhs, loc, nullptr);
    Value rhsValue = importSigSpec(rhs, loc, nullptr);
    if (!lhsValue || !rhsValue)
      return failure();
    builder.create<rtlil::WConnectionOp>(loc, lhsValue, rhsValue);
  }

  return success();
}

LogicalResult Importer::importDesign(Yosys::RTLIL::Design *design) {
  for (auto *source : design->modules())
    moduleNames.insert(toStringRef(source->name));

  for (auto *source : design->modules())
    if (failed(importModule(source)))
      return failure();
  return success();
}

//===----------------------------------------------------------------------===//
// Entry point
//===----------------------------------------------------------------------===//

LogicalResult circt::rtlil::importRTLIL(Yosys::RTLIL::Design *design,
                                        mlir::ModuleOp module) {
  module.getContext()->loadDialect<rtlil::RTLILDialect>();
  Importer importer(module);
  return importer.importDesign(design);
}

//===----------------------------------------------------------------------===//
// Translation Registration
//===----------------------------------------------------------------------===//

void circt::rtlil::registerImportRTLILTranslation() {
  static mlir::TranslateToMLIRRegistration fromRTLIL(
      "import-rtlil", "import an RTLIL (.il) file",
      [](llvm::SourceMgr &sourceMgr,
         MLIRContext *context) -> OwningOpRef<mlir::ModuleOp> {
        if (auto error = circt::yosys::initialize()) {
          mlir::emitError(UnknownLoc::get(context))
              << "failed to initialize Yosys: "
              << llvm::toString(std::move(error));
          return {};
        }

        // Yosys' own RTLIL frontend does the parsing -- writing a second `.il`
        // parser on the MLIR side is exactly what linking the library avoids.
        // Passing a non-null stream makes `frontend_call` ignore the filename,
        // so the buffer lit hands us never has to reach the filesystem.
        const auto *buffer =
            sourceMgr.getMemoryBuffer(sourceMgr.getMainFileID());
        std::istringstream stream(buffer->getBuffer().str());
        std::istream *streamPtr = &stream;

        Yosys::RTLIL::Design design;
        // A malformed `.il` reaches `log_error` and ends the process; the hook
        // installed by `yosys::initialize()` is the only thing that gets a
        // message out first.
        Yosys::Frontend::frontend_call(&design, streamPtr,
                                       buffer->getBufferIdentifier().str(),
                                       "rtlil");

        OwningOpRef<mlir::ModuleOp> module(
            mlir::ModuleOp::create(UnknownLoc::get(context)));
        if (failed(importRTLIL(&design, module.get())))
          return {};
        return module;
      },
      [](DialectRegistry &registry) {
        registry.insert<circt::rtlil::RTLILDialect>();
      });
}
