//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/RTLILToCore.h"
#include "circt/Dialect/Comb/CombOps.h"
#include "circt/Dialect/Debug/DebugOps.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/HW/HWTypes.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/Verif/VerifOps.h"
#include "circt/Support/ConversionPatternSet.h"
#include "circt/Transforms/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMTypes.h"
#include "mlir/IR/Iterators.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/RegionUtils.h"
#include "llvm/IR/DerivedTypes.h"

namespace circt {
#define GEN_PASS_DEF_CONVERTRTLILTOCORE
#include "circt/Conversion/Passes.h.inc"
} // namespace circt

using namespace mlir;
using namespace circt;
using namespace rtlil;

//===----------------------------------------------------------------------===//
// RTLIL to Core Conversion Pass
//===----------------------------------------------------------------------===//

namespace {
struct RTLILToCorePass
    : public circt::impl::ConvertRTLILToCoreBase<RTLILToCorePass> {
  void runOnOperation() override;
};
} // namespace

/// This is the main entry point for the RTLIL to Core conversion pass.
void RTLILToCorePass::runOnOperation() {
  getOperation().emitError("RTLIL to Core conversion not implemented yet");
  signalPassFailure();
}
