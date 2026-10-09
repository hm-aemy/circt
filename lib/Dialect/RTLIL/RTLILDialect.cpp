//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Dialect/RTLIL/RTLILDialect.h"
#include "circt/Dialect/RTLIL/RTLILAttributes.h"
#include "circt/Dialect/RTLIL/RTLILEnums.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "mlir/IR/DialectImplementation.h"

using namespace circt;
using namespace circt::rtlil;

void RTLILDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "circt/Dialect/RTLIL/RTLIL.cpp.inc"
      >();
  registerAttributes();
  registerTypes();
}

#include "circt/Dialect/RTLIL/RTLILDialect.cpp.inc"

#include "circt/Dialect/RTLIL/RTLILEnums.cpp.inc"
