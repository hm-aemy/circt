//===- ExportVerilog.h - Verilog Exporter -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the interface to the Verilog emitter.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_TRANSLATION_EXPORTVERILOG_H
#define CIRCT_TRANSLATION_EXPORTVERILOG_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/IR/Location.h"
#include "circt/Support/LoweringOptions.h"

namespace circt {

#define GEN_PASS_DECL_TESTAPPLYLOWERINGOPTION
#define GEN_PASS_DECL_HWLOWERINSTANCECHOICES
#define GEN_PASS_DECL_PREPAREFOREMISSION
#define GEN_PASS_DECL_LEGALIZEANONENUMS
#define GEN_PASS_DECL_EXPORTSPLITVERILOG
#define GEN_PASS_DECL_EXPORTVERILOG
#include "circt/Conversion/Passes.h.inc"
namespace hw {
class HWModuleLike;
class HWEmittableModuleLike;
} // namespace hw

std::unique_ptr<mlir::Pass>
createTestApplyLoweringOptionPass(llvm::StringRef options);
std::unique_ptr<mlir::Pass> createTestApplyLoweringOptionPass();

std::unique_ptr<mlir::Pass> createHWLowerInstanceChoicesPass();
std::unique_ptr<mlir::Pass> createPrepareForEmissionPass();
std::unique_ptr<mlir::Pass> createLegalizeAnonEnumsPass();

std::unique_ptr<mlir::Pass>
createExportVerilogPass(std::unique_ptr<llvm::raw_ostream> os);
std::unique_ptr<mlir::Pass> createExportVerilogPass(llvm::raw_ostream &os);
std::unique_ptr<mlir::Pass> createExportVerilogPass();

std::unique_ptr<mlir::Pass>
createExportSplitVerilogPass(llvm::StringRef directory = "./");

/// Export a module containing HW, and SV dialect code. Requires that the SV
/// dialect is loaded in to the context.
mlir::LogicalResult exportVerilog(mlir::ModuleOp module, llvm::raw_ostream &os);

/// Export a module containing HW, and SV dialect code, as one file per SV
/// module. Requires that the SV dialect is loaded in to the context.
///
/// Files are created in the directory indicated by \p dirname.
mlir::LogicalResult exportSplitVerilog(mlir::ModuleOp module,
                                       llvm::StringRef dirname);

/// An auxiliary class to emit a user-friendly string representation of a
/// location or a set of locations. The string can be emitted in various
/// styles.
class LocationEmitter {
public:
  /// Generates location info for a single location in the specified style.
  LocationEmitter(LoweringOptions::LocationInfoStyle style, mlir::Location loc);

  /// Generates location info for a set of operations in the specified style.
  LocationEmitter(LoweringOptions::LocationInfoStyle style,
                  const llvm::SmallPtrSetImpl<mlir::Operation *> &ops);

  /// Returns the generated location string.
  llvm::StringRef strref() { return output; }

private:
  /// Emits the location information for a given set of attributes to a stream.
  void emitLocationSetInfo(llvm::raw_string_ostream &os, LoweringOptions::LocationInfoStyle style,
                           const llvm::SmallPtrSetImpl<mlir::Attribute> &locationSet);

  std::string output;

  /// A private implementation struct to encapsulate the recursive emission logic.
  struct Impl;
};

} // namespace circt

#endif // CIRCT_TRANSLATION_EXPORTVERILOG_H
