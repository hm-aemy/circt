//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/OpImplementation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

using namespace circt;
using namespace rtlil;

//===----------------------------------------------------------------------===//
// Custom directives
//===----------------------------------------------------------------------===//

/// The connections of a cell, each next to the port it attaches to:
/// `["\\A" = %a, "\\Y" = %y]`. `$ports` and `$connections` are parallel
/// arrays, which the generic form leaves the reader to line up by position.
static ParseResult
parseCellPorts(OpAsmParser &parser,
               SmallVectorImpl<OpAsmParser::UnresolvedOperand> &connections,
               ArrayAttr &ports) {
  SmallVector<Attribute> names;
  auto parsePort = [&]() -> ParseResult {
    std::string name;
    if (parser.parseString(&name) || parser.parseEqual() ||
        parser.parseOperand(connections.emplace_back()))
      return failure();
    names.push_back(parser.getBuilder().getStringAttr(name));
    return success();
  };
  if (parser.parseCommaSeparatedList(OpAsmParser::Delimiter::Square, parsePort))
    return failure();
  ports = parser.getBuilder().getArrayAttr(names);
  return success();
}

/// `CellOpInterface` verifies that `$ports` and `$connections` have the same
/// length, and the printer only sees verified ops.
static void printCellPorts(OpAsmPrinter &printer, Operation *,
                           OperandRange connections, ArrayAttr ports) {
  printer << '[';
  llvm::interleaveComma(llvm::zip_equal(ports, connections), printer,
                        [&](auto port) {
                          printer.printAttribute(std::get<0>(port));
                          printer << " = " << std::get<1>(port);
                        });
  printer << ']';
}

#define GET_OP_CLASSES
#include "circt/Dialect/RTLIL/RTLIL.cpp.inc"

//===----------------------------------------------------------------------===//
// ModuleOp
//===----------------------------------------------------------------------===//

void rtlil::ModuleOp::build(OpBuilder &builder, OperationState &result,
                            StringAttr symName, ArrayAttr rtlilAttributes,
                            ArrayAttr availParameters) {
  result.addAttribute(SymbolTable::getSymbolAttrName(), symName);
  if (!rtlilAttributes)
    rtlilAttributes = builder.getArrayAttr({});
  if (!availParameters)
    availParameters = builder.getArrayAttr({});
  result.addAttribute(getRtlilAttributesAttrName(result.name), rtlilAttributes);
  result.addAttribute(getAvailParametersAttrName(result.name), availParameters);
  result.addRegion()->emplaceBlock();
}

/// The RTLIL name an op inside a module body claims, or nullopt for ops that
/// claim none. Wires and cells share one namespace in `RTLIL::Module`, so they
/// are collected together.
static std::optional<StringRef> getDeclaredName(Operation *op) {
  if (auto wire = dyn_cast<WireOp>(op))
    return wire.getName();
  if (auto cell = dyn_cast<CellOpInterface>(op))
    return cell.getCellName();
  return std::nullopt;
}

LogicalResult rtlil::ModuleOp::verify() {
  if (!isValidIdentifier(getSymName()))
    return emitOpError("name ")
           << getSymName()
           << " is not a valid RTLIL identifier; it must start with '\\' or "
              "'$' and contain no spaces or control characters";
  return success();
}

LogicalResult rtlil::ModuleOp::verifyRegions() {
  // Wires and cells share one Yosys namespace; a duplicate corrupts the design.
  DenseMap<StringRef, Operation *> declared;
  // `port_id` is 1-based and must be dense: `fixup_ports()` silently renumbers
  // otherwise, and export/import stop round-tripping.
  DenseMap<uint32_t, Operation *> portIds;
  uint32_t maxPortId = 0;

  for (Operation &op : getBodyBlock()->getOperations()) {
    if (op.getDialect() != (*this)->getDialect())
      return op.emitOpError("is not an RTLIL operation, so it cannot appear in "
                            "an rtlil.module body");

    if (auto name = getDeclaredName(&op)) {
      if (!isValidIdentifier(*name))
        return op.emitOpError("name ")
               << *name
               << " is not a valid RTLIL identifier; it must start with '\\' "
                  "or '$' and contain no spaces or control characters";
      auto [it, inserted] = declared.try_emplace(*name, &op);
      if (!inserted)
        return op.emitOpError("redeclares the RTLIL name ")
                   .append(*name)
                   .attachNote(it->second->getLoc())
               << "previously declared here; wires and cells share one "
                  "namespace";
    }

    auto wire = dyn_cast<WireOp>(op);
    if (!wire)
      continue;
    uint32_t portId = wire.getPortId();
    // An unassigned `port_id` is not a port yet: `fixup_ports()` numbers it.
    if (portId == 0)
      continue;
    // A port with neither flag is one `fixup_ports()` cannot classify.
    if (!wire.getPortInput() && !wire.getPortOutput())
      return op.emitOpError("port_id ")
             << portId << " has neither an input nor an output designation";
    auto [it, inserted] = portIds.try_emplace(portId, &op);
    if (!inserted)
      return op.emitOpError("reuses port_id ")
                 .append(portId)
                 .attachNote(it->second->getLoc())
             << "already used here";
    maxPortId = std::max(maxPortId, portId);
  }

  if (maxPortId != portIds.size())
    return emitOpError("port_ids must be exactly 1..")
           << portIds.size() << ", but the largest is " << maxPortId;

  return success();
}

void rtlil::ModuleOp::getPortWires(SmallVectorImpl<WireOp> &ports) {
  ports.clear();
  for (auto wire : getBodyBlock()->getOps<WireOp>())
    if (wire.getPortId() != 0)
      ports.push_back(wire);
  llvm::sort(ports, [](WireOp lhs, WireOp rhs) {
    return lhs.getPortId() < rhs.getPortId();
  });
}

//===----------------------------------------------------------------------===//
// SliceOp / ConcatOp
//===----------------------------------------------------------------------===//

static unsigned getBitWidth(Value value) {
  return cast<MValueType>(value.getType()).getWidth();
}

LogicalResult SliceOp::verify() {
  unsigned inputWidth = getBitWidth(getInput());
  unsigned resultWidth = getBitWidth(getResult());
  // Add rather than subtract: both are unsigned, and the sum cannot wrap.
  if (uint64_t(getOffset()) + resultWidth > inputWidth)
    return emitOpError("slice of ")
           << resultWidth << " bits at offset " << getOffset()
           << " runs past the end of a " << inputWidth << "-bit value";
  return success();
}

LogicalResult ConcatOp::verify() {
  uint64_t total = 0;
  for (Value input : getInputs())
    total += getBitWidth(input);
  if (total != getBitWidth(getResult()))
    return emitOpError("operands total ")
           << total << " bits but the result is " << getBitWidth(getResult());
  return success();
}

//===----------------------------------------------------------------------===//
// WConnectionOp
//===----------------------------------------------------------------------===//

LogicalResult WConnectionOp::verify() {
  unsigned lhsWidth = getBitWidth(getLhs());
  unsigned rhsWidth = getBitWidth(getRhs());
  if (lhsWidth != rhsWidth)
    return emitOpError("left-hand side is ")
           << lhsWidth << " bits but the right-hand side is " << rhsWidth;
  return success();
}

//===----------------------------------------------------------------------===//
// InstanceOp
//===----------------------------------------------------------------------===//

LogicalResult InstanceOp::verifySymbolUses(SymbolTableCollection &symbolTable) {
  // `rtlil.module` is deliberately not a SymbolTable, so the nearest one is the
  // enclosing `builtin.module`: the op that stands in for the RTLIL design, and
  // the scope in which module names are unique.
  auto callee = symbolTable.lookupNearestSymbolFrom<rtlil::ModuleOp>(
      *this, getTypeAttr());
  if (!callee)
    return emitOpError("references unknown module ") << getType();

  // Only the ports this instance names: a port left out is undriven, which
  // RTLIL allows. `CellOpInterface` has checked that `$ports` and
  // `$connections` line up, so the zip is safe.
  SmallVector<WireOp> calleePortWires;
  callee.getPortWires(calleePortWires);
  llvm::SmallDenseMap<StringRef, WireOp> calleePorts;
  for (WireOp port : calleePortWires)
    calleePorts.try_emplace(port.getName(), port);

  for (auto [port, connection] : llvm::zip(getPorts(), getConnections())) {
    StringRef name = cast<StringAttr>(port).getValue();
    auto it = calleePorts.find(name);
    if (it == calleePorts.end())
      return emitOpError("connects port ")
                 .append(name)
                 .append(", which module ")
                 .append(getType())
                 .append(" does not declare")
                 .attachNote(callee.getLoc())
             << "module declared here";

    unsigned portWidth = getBitWidth(it->second.getResult());
    unsigned connectionWidth = getBitWidth(connection);
    if (portWidth != connectionWidth)
      return emitOpError("connects ")
                 .append(connectionWidth)
                 .append(" bits to port ")
                 .append(name)
                 .append(", which is ")
                 .append(portWidth)
                 .append(" bits wide")
                 .attachNote(it->second.getLoc())
             << "port declared here";
  }
  return success();
}
