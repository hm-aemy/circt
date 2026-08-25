//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILOPS_H
#define CIRCT_DIALECT_RTLIL_RTLILOPS_H

#include "circt/Dialect/RTLIL/RTLILAttributes.h"
#include "circt/Dialect/RTLIL/RTLILDialect.h"
#include "circt/Dialect/RTLIL/RTLILOpInterfaces.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"

#include "circt/Support/LLVM.h"
#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/RegionKindInterface.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Interfaces/DerivedAttributeOpInterface.h"
#include "mlir/Interfaces/FunctionInterfaces.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/SmallVector.h"

#define GET_OP_CLASSES
#include "circt/Dialect/RTLIL/RTLIL.h.inc"

namespace circt::rtlil {

/// The wires of `module` flagged as ports, ordered by `port_id`.
///
/// RTLIL keeps no separate port list on a module -- a port is a wire with a
/// non-zero `port_id`, and `RTLIL::Module::fixup_ports()` derives the list from
/// those flags -- so neither does `rtlil.module`. The module verifier
/// guarantees the non-zero ids are exactly `1..N`, so the result is dense.
///
/// A free function rather than a method on `ModuleOp` because TableGen emits op
/// classes alphabetically, leaving `WireOp` incomplete inside `ModuleOp`.
llvm::SmallVector<WireOp> getPortWires(ModuleOp module);

} // namespace circt::rtlil

#endif // CIRCT_DIALECT_RTLIL_RTLILOPS_H
