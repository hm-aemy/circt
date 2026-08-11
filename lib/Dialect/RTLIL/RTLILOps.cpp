// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLIL.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/OpImplementation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;

#define GET_OP_CLASSES
#include "circt/Dialect/RTLIL/RTLILOps.cpp.inc"

//===----------------------------------------------------------------------===//
// ModuleOp
//===----------------------------------------------------------------------===//
//
// Note the deliberate absence of `using namespace circt::rtlil` in this file:
// with `using namespace mlir` also in scope, an unqualified `ModuleOp` would
// silently bind to `mlir::ModuleOp`. Everything below is qualified.
//
//===----------------------------------------------------------------------===//

namespace rtlil = circt::rtlil;

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
  if (auto wire = dyn_cast<rtlil::WireOp>(op))
    return wire.getName();
  if (auto cell = dyn_cast<rtlil::CellOpInterface>(op))
    return cell.getCellName();
  return std::nullopt;
}

LogicalResult rtlil::ModuleOp::verify() {
  if (!rtlil::isValidIdentifier(getSymName()))
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
      if (!rtlil::isValidIdentifier(*name))
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

    auto wire = dyn_cast<rtlil::WireOp>(op);
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
// InstanceOp
//===----------------------------------------------------------------------===//

LogicalResult
rtlil::InstanceOp::verifySymbolUses(SymbolTableCollection &symbolTable) {
  // `rtlil.module` is deliberately not a SymbolTable, so the nearest one is the
  // enclosing `builtin.module` -- the op that stands in for the RTLIL design,
  // and the scope in which module names are unique.
  auto callee =
      symbolTable.lookupNearestSymbolFrom<rtlil::ModuleOp>(*this, getTypeAttr());
  if (!callee)
    return emitOpError("references unknown module ") << getType();
  return success();
}

SmallVector<rtlil::WireOp> rtlil::getPortWires(rtlil::ModuleOp module) {
  SmallVector<rtlil::WireOp> ports;
  for (auto wire : module.getBodyBlock()->getOps<rtlil::WireOp>())
    if (wire.getPortId() != 0)
      ports.push_back(wire);
  llvm::sort(ports, [](rtlil::WireOp lhs, rtlil::WireOp rhs) {
    return lhs.getPortId() < rhs.getPortId();
  });
  return ports;
}