//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Dialect/RTLIL/RTLILOpInterfaces.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "llvm/ADT/DenseMap.h"

#include "circt/Dialect/RTLIL/RTLILOpInterfaces.cpp.inc"

using namespace circt;
using namespace mlir;

LogicalResult rtlil::verifyCellOpInterface(Operation *op) {
  auto cell = cast<CellOpInterface>(op);
  ArrayAttr ports = cell.getCellPorts();
  OperandRange connections = cell.getCellConnections();

  if (ports.size() != connections.size())
    return op->emitOpError("has ")
           << ports.size() << " port names but " << connections.size()
           << " connections; the two are index-parallel";

  // A cell may leave a port unconnected, but it may not name one twice:
  // `RTLIL::Cell::connections_` is a dict, so the second `setPort` silently
  // replaces the first and a connection disappears.
  DenseMap<StringRef, unsigned> seen;
  for (auto [index, port] : llvm::enumerate(ports)) {
    StringRef name = cast<StringAttr>(port).getValue();
    if (failed(verifyIdentifier(op, "port name", name)))
      return failure();
    auto [it, inserted] = seen.try_emplace(name, index);
    if (!inserted)
      return op->emitOpError("connects port ")
             << name << " twice, at operands " << it->second << " and "
             << index;
  }
  return success();
}
