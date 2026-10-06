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

/// Cells have `$ports` which are attached to `$connections`.
/// Each is an array where the position maps both together.
/// For example: `["\\A" = %a, "\\Y" = %y]`.
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

/// Print the combined `$ports` and `$connections`.
/// Verifier of `CellOpInterface` ensure equal length.
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

/// Check that each connection of a typed cell has the width its port requires.
/// `FixedOperands` has already checked the number of connections.
static LogicalResult verifyPortWidths(Operation *op, ArrayRef<StringRef> ports,
                                      ArrayRef<unsigned> widths) {
  for (auto [port, width, connection] :
       llvm::zip_equal(ports, widths, op->getOperands())) {
    unsigned actual = cast<MValueType>(connection.getType()).getWidth();
    if (actual != width)
      return op->emitOpError("port ")
             << port << " is " << actual << " bits wide, but must be " << width;
  }
  return success();
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

/// Wire and Cell operations names share the namespace.
static std::optional<StringRef> getDeclaredName(Operation *op) {
  if (auto wire = dyn_cast<WireOp>(op))
    return wire.getName();
  if (auto cell = dyn_cast<CellOpInterface>(op))
    return cell.getCellName();
  return std::nullopt;
}

LogicalResult rtlil::ModuleOp::verify() {
  return verifyIdentifier(*this, "module name", getSymName());
}

LogicalResult rtlil::ModuleOp::verifyRegions() {
  // Store every wire and cell opertions to reject duplicated names.
  DenseMap<StringRef, Operation *> declared;
  // Renumber `port_id`. Start at 1 and consecutively increment for each port.
  DenseMap<uint32_t, Operation *> portIds;
  uint32_t maxPortId = 0;

  for (Operation &op : getBodyBlock()->getOperations()) {
    if (op.getDialect() != (*this)->getDialect())
      return op.emitOpError("is not allowed in an RTLIL module");

    if (auto name = getDeclaredName(&op)) {
      if (failed(verifyIdentifier(&op, "name", *name)))
        return failure();
      auto [it, inserted] = declared.try_emplace(*name, &op);
      if (!inserted) {
        auto diag = op.emitOpError("redeclares name '") << *name << "'";
        diag.attachNote(it->second->getLoc())
            << "previously declared here; wires and cells share one namespace";
        return diag;
      }
    }

    auto wire = dyn_cast<WireOp>(op);
    if (!wire)
      continue;
    uint32_t portId = wire.getPortId();
    // `fixup_ports()` will number the port.
    if (portId == 0)
      continue;
    if (!wire.getDirection())
      return op.emitOpError("port_id ") << portId << " without a direction";
    auto [it, inserted] = portIds.try_emplace(portId, &op);
    if (!inserted) {
      auto diag = op.emitOpError("reuses port_id ") << portId;
      diag.attachNote(it->second->getLoc()) << "already used here";
      return diag;
    }
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
  if (uint64_t(getOffset()) + resultWidth > inputWidth)
    return emitOpError("slice of ")
           << resultWidth << " bits at offset " << getOffset()
           << " is out of bounds for the " << inputWidth << "-bit input";
  return success();
}

LogicalResult ConcatOp::verify() {
  uint64_t total = 0;
  for (Value input : getInputs())
    total += getBitWidth(input);
  if (total != getBitWidth(getResult()))
    return emitOpError("result width ")
           << getBitWidth(getResult()) << " does not match total operand width "
           << total;
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
  // `rtlil.module` is not a SymbolTable, so the lookup resolves in the
  // enclosing `builtin.module`, which represents the RTLIL design.
  auto callee = symbolTable.lookupNearestSymbolFrom<rtlil::ModuleOp>(
      *this, getTypeAttr());
  if (!callee)
    return emitOpError("references unknown module '") << getType() << "'";

  SmallVector<WireOp> calleePortWires;
  callee.getPortWires(calleePortWires);
  llvm::SmallDenseMap<StringRef, WireOp> calleePorts;
  for (WireOp port : calleePortWires)
    calleePorts.try_emplace(port.getName(), port);

  // Only check the ports this instance connects: a port left out is undriven,
  // which RTLIL allows.
  for (auto [port, connection] :
       llvm::zip_equal(getPorts(), getConnections())) {
    StringRef name = cast<StringAttr>(port).getValue();
    auto it = calleePorts.find(name);
    if (it == calleePorts.end()) {
      auto diag = emitOpError("connects port '")
                  << name << "', which module '" << getType()
                  << "' does not declare";
      diag.attachNote(callee.getLoc()) << "module declared here";
      return diag;
    }

    unsigned portWidth = getBitWidth(it->second.getResult());
    unsigned connectionWidth = getBitWidth(connection);
    if (portWidth != connectionWidth) {
      auto diag = emitOpError("connects ")
                  << connectionWidth << " bits to port '" << name
                  << "', which is " << portWidth << " bits wide";
      diag.attachNote(it->second.getLoc()) << "port declared here";
      return diag;
    }
  }
  return success();
}
