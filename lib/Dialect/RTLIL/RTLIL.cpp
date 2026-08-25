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

#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/OpImplementation.h"
#include "llvm/ADT/TypeSwitch.h"

#include "circt/Dialect/RTLIL/RTLIL.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"

using namespace mlir;
using namespace circt::rtlil;

//===----------------------------------------------------------------------===//
// RTLIL dialect.
//===----------------------------------------------------------------------===//

#include "circt/Dialect/RTLIL/RTLILEnums.cpp.inc"
#include "circt/Dialect/RTLIL/RTLILDialect.cpp.inc"

#include "circt/Dialect/RTLIL/RTLILOpInterfaces.cpp.inc"

#define GET_ATTRDEF_CLASSES
#include "circt/Dialect/RTLIL/RTLILAttrDefs.cpp.inc"
#undef GET_ATTRDEF_CLASSES

#define GET_TYPEDEF_CLASSES
#include "circt/Dialect/RTLIL/RTLILTypes.cpp.inc"
#undef GET_TYPEDEF_CLASSES

LogicalResult
ParameterAttr::verify(llvm::function_ref<InFlightDiagnostic()> emitError,
                      StringAttr name, Attribute value,
                      std::optional<uint16_t> flags) {
  if (!name || !isValidIdentifier(name.getValue()))
    return emitError() << "parameter name '" << (name ? name.getValue() : "")
                       << "' is not a valid RTLIL identifier";

  // An RTLIL::Const is a string or a bit vector. `IntegerAttr` is the
  // shorthand for a bit vector that fits in 64 bits and is fully defined;
  // `ConstAttr` (an array of StateEnumAttr) is the general form.
  if (isa<StringAttr, IntegerAttr>(value))
    return success();
  if (auto array = dyn_cast<ArrayAttr>(value)) {
    if (llvm::all_of(array, llvm::IsaPred<StateEnumAttr>))
      return success();
    return emitError() << "parameter '" << name.getValue()
                       << "' has an array value that is not a bit vector";
  }
  return emitError() << "parameter '" << name.getValue()
                     << "' must be an integer, a bit vector or a string";
}

void RTLILDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "circt/Dialect/RTLIL/RTLIL.cpp.inc"
      >();
  addAttributes<
#define GET_ATTRDEF_LIST
#include "circt/Dialect/RTLIL/RTLILAttrDefs.cpp.inc"
      >();
  addTypes<
#define GET_TYPEDEF_LIST
#include "circt/Dialect/RTLIL/RTLILTypes.cpp.inc"
      >();
}