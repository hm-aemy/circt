//===- ImportRTLIL.cpp - A Yosys design to the RTLIL dialect --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Walks a `Yosys::RTLIL::Design` into `rtlil.module` ops. `SigSpec`s become
// `rtlil.slice` and `rtlil.concat`. Cells become generic `rtlil.cell`s, or
// `rtlil.instance` when their type names a module in the design.
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/ImportRTLIL.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "circt/Yosys/Yosys.h"

#include "mlir/AsmParser/AsmParser.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Diagnostics.h"
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

#include "kernel/rtlil.h"
#include "kernel/yosys.h"

using namespace circt;
using namespace mlir;

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

/// An `RTLIL::IdString` as a `StringRef`, sigil included.
static StringRef toStringRef(const Yosys::RTLIL::IdString &name) {
  const char *cstr = name.c_str();
  return StringRef(cstr);
}

static Yosys::RTLIL::IdString id(StringRef name) {
  return Yosys::RTLIL::IdString(std::string_view(name.data(), name.size()));
}

/// Must match `circtLocAttrName` in ExportRTLIL.cpp, the only writer.
static constexpr StringRef circtLocAttrName = "\\circt.loc";

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

  /// A value carrying the bits of `spec`. `context` names the signal in
  /// diagnostics, since a `SigSpec` has no location of its own.
  Value importSigSpec(const Yosys::RTLIL::SigSpec &spec, Location loc,
                      const Twine &context);

  /// The bit vector for one `RTLIL::Const`, as `rtlil.const` takes it.
  FailureOr<rtlil::ConstAttr> importBits(const Yosys::RTLIL::Const &value,
                                         Location loc);

  /// A parameter or attribute value, as an integer or string when it fits.
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
  /// Module names in the design, so a cell can be recognized as an instance.
  llvm::DenseSet<StringRef> moduleNames;
};
} // namespace

//===----------------------------------------------------------------------===//
// Locations and attributes
//===----------------------------------------------------------------------===//

namespace {
/// One `file:line.col-line.col` piece of an RTLIL `src` attribute.
struct SrcPiece {
  StringRef file;
  unsigned startLine, startColumn, endLine, endColumn;
};
} // namespace

/// Parse `line.col`, where the column is optional.
static std::optional<std::pair<unsigned, unsigned>>
parseLineCol(StringRef text) {
  auto [lineStr, colStr] = text.split('.');
  unsigned line = 0, col = 0;
  if (lineStr.getAsInteger(10, line))
    return std::nullopt;
  if (!colStr.empty() && colStr.getAsInteger(10, col))
    col = 0;
  return std::make_pair(line, col);
}

/// Parse one `file:line.col-line.col` piece of an RTLIL `src`.
static std::optional<SrcPiece> parseSrcPiece(StringRef piece) {
  // Split at the last colon, as the path may contain colons.
  size_t colon = piece.rfind(':');
  if (colon == StringRef::npos)
    return std::nullopt;
  StringRef file = piece.substr(0, colon);
  auto [beginStr, endStr] = piece.substr(colon + 1).split('-');

  auto begin = parseLineCol(beginStr);
  if (!begin)
    return std::nullopt;
  auto end = endStr.empty() ? begin : parseLineCol(endStr);
  if (!end)
    end = begin;
  return SrcPiece{file, begin->first, begin->second, end->first, end->second};
}

Location Importer::importLocation(const Yosys::RTLIL::AttrObject &object) {
  // Prefer `\circt.loc`; cells Yosys created only have `src`.
  auto exact = object.attributes.find(id(circtLocAttrName));
  if (exact != object.attributes.end()) {
    std::string text = exact->second.decode_string();
    mlir::ScopedDiagnosticHandler quiet(context,
                                        [](Diagnostic &) { return success(); });
    if (auto attr = dyn_cast_or_null<LocationAttr>(
            mlir::parseAttribute(text, context, /*type=*/nullptr)))
      return Location(attr);
  }

  auto it = object.attributes.find(Yosys::ID::src);
  if (it == object.attributes.end())
    return builder.getUnknownLoc();

  std::string src = it->second.decode_string();
  SmallVector<Location> locations;
  SmallVector<StringRef> pieces;
  StringRef(src).split(pieces, '|', /*MaxSplit=*/-1, /*KeepEmpty=*/false);
  for (StringRef piece : pieces) {
    auto parsed = parseSrcPiece(piece);
    if (!parsed)
      continue;
    auto file = builder.getStringAttr(parsed->file);
    // `FileLineColRange::get` does not canonicalize a point range.
    if (parsed->startLine == parsed->endLine &&
        parsed->startColumn == parsed->endColumn)
      locations.push_back(
          FileLineColLoc::get(file, parsed->startLine, parsed->startColumn));
    else
      locations.push_back(
          FileLineColRange::get(file, parsed->startLine, parsed->startColumn,
                                parsed->endLine, parsed->endColumn));
  }

  if (locations.empty())
    return builder.getUnknownLoc();
  if (locations.size() == 1)
    return locations.front();
  return builder.getFusedLoc(locations);
}

FailureOr<rtlil::ConstAttr>
Importer::importBits(const Yosys::RTLIL::Const &value, Location loc) {
  SmallVector<rtlil::StateEnum> bits;
  bits.reserve(value.size());
  // `to_bits()` works for either Const backing.
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
      // `Sm` is internal to Yosys and has no `StateEnum` case.
      return mlir::emitError(loc)
             << "unsupported RTLIL state " << int(bit) << " in constant";
    }
    bits.push_back(state);
  }
  return rtlil::ConstAttr::get(context, bits);
}

FailureOr<Attribute> Importer::importConst(const Yosys::RTLIL::Const &value,
                                           Location loc) {
  // Only the flag marks a real string; the backing does not.
  if (value.flags & Yosys::RTLIL::CONST_FLAG_STRING)
    return Attribute(builder.getStringAttr(value.decode_string()));

  // A fully defined value of up to 64 bits becomes an `IntegerAttr`, which
  // export expands back to the same bits.
  std::vector<Yosys::RTLIL::State> rawBits = value.to_bits();
  if (!rawBits.empty() && rawBits.size() <= 64 &&
      llvm::all_of(rawBits, [](Yosys::RTLIL::State bit) {
        return bit == Yosys::RTLIL::State::S0 || bit == Yosys::RTLIL::State::S1;
      })) {
    llvm::APInt intVal(rawBits.size(), 0);
    for (unsigned i = 0, e = rawBits.size(); i != e; ++i)
      if (rawBits[i] == Yosys::RTLIL::State::S1)
        intVal.setBit(i);
    return Attribute(
        builder.getIntegerAttr(builder.getIntegerType(rawBits.size()), intVal));
  }

  auto bits = importBits(value, loc);
  if (failed(bits))
    return failure();
  return Attribute(*bits);
}

/// `flags`, unless they are the default.
static std::optional<uint16_t> nonDefaultFlags(const Yosys::RTLIL::Const &v) {
  if (v.flags == Yosys::RTLIL::CONST_FLAG_NONE)
    return std::nullopt;
  return v.flags;
}

FailureOr<ArrayAttr>
Importer::importAttributes(const Yosys::RTLIL::AttrObject &object,
                           Location loc) {
  // Sort by name, as hash dict order is not stable.
  SmallVector<std::pair<StringRef, const Yosys::RTLIL::Const *>> sorted;
  for (const auto &[name, value] : object.attributes) {
    // Both become the op's Location, and export re-derives them from it.
    if (name == Yosys::ID::src || name == id(circtLocAttrName))
      continue;
    sorted.emplace_back(toStringRef(name), &value);
  }
  llvm::sort(sorted, llvm::less_first());

  SmallVector<Attribute> entries;
  for (auto [name, value] : sorted) {
    auto imported = importConst(*value, loc);
    if (failed(imported))
      return failure();
    entries.push_back(
        rtlil::ParameterAttr::get(context, builder.getStringAttr(name),
                                  *imported, nonDefaultFlags(*value)));
  }
  return builder.getArrayAttr(entries);
}

//===----------------------------------------------------------------------===//
// SigSpec
//===----------------------------------------------------------------------===//

Value Importer::importSigSpec(const Yosys::RTLIL::SigSpec &spec, Location loc,
                              const Twine &context) {
  SmallVector<Value> pieces;
  // `chunks()` and `rtlil.concat` are both least significant first.
  for (const auto &chunk : spec.chunks()) {
    if (!chunk.wire) {
      // A run of constant bits.
      auto value = importBits(Yosys::RTLIL::Const(chunk.data), loc);
      if (failed(value))
        return {};
      pieces.push_back(
          rtlil::ConstOp::create(builder, loc, getType(chunk.width), *value));
      continue;
    }

    auto it = wireValues.find(chunk.wire);
    if (it == wireValues.end()) {
      mlir::emitError(loc) << context << " references unknown wire '"
                           << toStringRef(chunk.wire->name) << "'";
      return {};
    }
    Value wire = it->second;
    // Only slice when the chunk takes part of the wire.
    if (chunk.offset == 0 && chunk.width == chunk.wire->width) {
      pieces.push_back(wire);
      continue;
    }
    pieces.push_back(
        rtlil::SliceOp::create(builder, loc, getType(chunk.width), wire,
                               builder.getI32IntegerAttr(chunk.offset)));
  }

  if (pieces.empty()) {
    mlir::emitError(loc) << context << " has zero width";
    return {};
  }
  if (pieces.size() == 1)
    return pieces.front();
  return rtlil::ConcatOp::create(builder, loc, getType(spec.size()), pieces);
}

//===----------------------------------------------------------------------===//
// Cells and modules
//===----------------------------------------------------------------------===//

LogicalResult Importer::importCell(Yosys::RTLIL::Cell *cell, Location loc) {
  // Sort by name, as hash dict order is not stable.
  SmallVector<std::pair<StringRef, const Yosys::RTLIL::SigSpec *>> sortedPorts;
  for (const auto &[port, signal] : cell->connections())
    sortedPorts.emplace_back(toStringRef(port), &signal);
  llvm::sort(sortedPorts, llvm::less_first());

  SmallVector<Attribute> portNames;
  SmallVector<Value> connections;
  for (auto [port, signal] : sortedPorts) {
    Value value = importSigSpec(*signal, loc,
                                "port '" + port + "' of cell '" +
                                    toStringRef(cell->name) + "'");
    if (!value)
      return failure();
    portNames.push_back(builder.getStringAttr(port));
    connections.push_back(value);
  }

  SmallVector<std::pair<StringRef, const Yosys::RTLIL::Const *>> sortedParams;
  for (const auto &[name, value] : cell->parameters)
    sortedParams.emplace_back(toStringRef(name), &value);
  llvm::sort(sortedParams, llvm::less_first());

  SmallVector<Attribute> parameters;
  for (auto [name, value] : sortedParams) {
    auto imported = importConst(*value, loc);
    if (failed(imported))
      return failure();
    parameters.push_back(
        rtlil::ParameterAttr::get(context, builder.getStringAttr(name),
                                  *imported, nonDefaultFlags(*value)));
  }

  auto attributes = importAttributes(*cell, loc);
  if (failed(attributes))
    return failure();

  StringRef type = toStringRef(cell->type);
  StringAttr name = builder.getStringAttr(toStringRef(cell->name));
  if (moduleNames.contains(type)) {
    rtlil::InstanceOp::create(builder, loc, name,
                              FlatSymbolRefAttr::get(context, type),
                              connections, builder.getArrayAttr(portNames),
                              builder.getArrayAttr(parameters), *attributes);
    return success();
  }
  rtlil::CellOp::create(builder, loc, name, builder.getStringAttr(type),
                        connections, builder.getArrayAttr(portNames),
                        builder.getArrayAttr(parameters), *attributes);
  return success();
}

LogicalResult Importer::importModule(Yosys::RTLIL::Module *source) {
  Location loc = importLocation(*source);
  StringRef name = toStringRef(source->name);

  // Reject rather than silently drop processes, memories and bindings.
  if (!source->processes.empty())
    return mlir::emitError(loc)
           << "module '" << name << "' contains processes; run 'proc' first";
  if (!source->memories.empty())
    return mlir::emitError(loc) << "module '" << name
                                << "' contains memories; run 'memory_collect' "
                                   "or 'memory_map' first";
  if (!source->bindings_.empty())
    return mlir::emitError(loc) << "module '" << name << "' contains bindings";

  auto attributes = importAttributes(*source, loc);
  if (failed(attributes))
    return failure();

  SmallVector<Attribute> availParameters;
  for (const auto &parameter : source->avail_parameters)
    availParameters.push_back(builder.getStringAttr(toStringRef(parameter)));

  auto moduleOp = rtlil::ModuleOp::create(
      builder, loc, builder.getStringAttr(name), *attributes,
      builder.getArrayAttr(availParameters));
  wireValues.clear();

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToEnd(moduleOp.getBodyBlock());

  // Wires first, as every SigSpec below refers to them.
  for (auto *wire : source->wires()) {
    Location wireLoc = importLocation(*wire);
    auto wireAttributes = importAttributes(*wire, wireLoc);
    if (failed(wireAttributes))
      return failure();
    auto wireOp = rtlil::WireOp::create(
        builder, wireLoc, getType(wire->width), toStringRef(wire->name),
        wire->is_signed, wire->port_id, wire->start_offset, wire->port_input,
        wire->port_output, wire->upto, *wireAttributes);
    wireValues.try_emplace(wire, wireOp.getResult());
  }

  for (auto *cell : source->cells())
    if (failed(importCell(cell, importLocation(*cell))))
      return failure();

  unsigned index = 0;
  for (const auto &[lhs, rhs] : source->connections()) {
    // Connections have no name or location, so identify them by index.
    std::string which =
        ("connection " + Twine(index++) + " of module '" + name + "'").str();
    Value lhsValue = importSigSpec(lhs, loc, "the left-hand side of " + which);
    Value rhsValue = importSigSpec(rhs, loc, "the right-hand side of " + which);
    if (!lhsValue || !rhsValue)
      return failure();
    rtlil::WConnectionOp::create(builder, loc, lhsValue, rhsValue);
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

        // Keep the frontend's log off stderr. Fatal errors still get through.
        circt::yosys::LogCapture capture;

        // Parse with Yosys' RTLIL frontend. With a stream, `frontend_call`
        // ignores the filename and does not touch the filesystem.
        const auto *buffer =
            sourceMgr.getMemoryBuffer(sourceMgr.getMainFileID());
        std::istringstream stream(buffer->getBuffer().str());
        std::istream *streamPtr = &stream;

        Yosys::RTLIL::Design design;
        // A malformed `.il` ends the process via `log_error`.
        Yosys::Frontend::frontend_call(
            &design, streamPtr, buffer->getBufferIdentifier().str(), "rtlil");

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
