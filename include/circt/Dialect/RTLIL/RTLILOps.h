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
