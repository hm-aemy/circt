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

  // Wires and cells share a single namespace inside an `RTLIL::Module`, which
  // asserts on a duplicate rather than reporting one. Catch it here instead.
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
    if (portId == 0)
      continue;
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

//===----------------------------------------------------------------------===//
// SliceOp / ConcatOp
//===----------------------------------------------------------------------===//

static unsigned getBitWidth(Value value) {
  return cast<MValueType>(value.getType()).getWidth();
}

LogicalResult SliceOp::verify() {
  unsigned inputWidth = getBitWidth(getInput());
  unsigned resultWidth = getBitWidth(getResult());
  // Both are unsigned, so add rather than subtract: `offset + resultWidth`
  // cannot wrap for any width the type can express.
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
// InstanceOp
//===----------------------------------------------------------------------===//

LogicalResult InstanceOp::verifySymbolUses(SymbolTableCollection &symbolTable) {
  // `rtlil.module` is deliberately not a SymbolTable, so the nearest one is the
  // enclosing `builtin.module` -- the op that stands in for the RTLIL design,
  // and the scope in which module names are unique.
  auto callee = symbolTable.lookupNearestSymbolFrom<rtlil::ModuleOp>(
      *this, getTypeAttr());
  if (!callee)
    return emitOpError("references unknown module ") << getType();
  return success();
}

SmallVector<WireOp> rtlil::getPortWires(rtlil::ModuleOp module) {
  SmallVector<WireOp> ports;
  for (auto wire : module.getBodyBlock()->getOps<WireOp>())
    if (wire.getPortId() != 0)
      ports.push_back(wire);
  llvm::sort(ports, [](WireOp lhs, WireOp rhs) {
    return lhs.getPortId() < rhs.getPortId();
  });
  return ports;
}
