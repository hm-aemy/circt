//===- ExportRTLIL.h - RTLIL dialect to a Yosys design ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Translates `rtlil.module` ops into a real `Yosys::RTLIL::Design`, in memory,
// so that the result can be handed straight to Yosys' passes.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_CONVERSION_EXPORTRTLIL_H
#define CIRCT_CONVERSION_EXPORTRTLIL_H

#include "circt/Support/LLVM.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/ArrayRef.h"

// Names `RTLIL::Design` without including a single Yosys header, which is what
// keeps this header cheap and keeps `-fexceptions`/C++20 out of every consumer's
// build. `YOSYS_NAMESPACE_BEGIN` expands to a plain `namespace Yosys {` -- no
// inline namespace and no version tag -- so this declares the same entity
// `kernel/rtlil.h` does. `ExportRTLIL.cpp` includes both, so a divergence would
// be a compile error there rather than a link error later.
namespace Yosys {
namespace RTLIL {
struct Design;
struct Module;
} // namespace RTLIL
} // namespace Yosys

namespace circt {
namespace rtlil {

class ModuleOp;

/// Translate `modules` into `design`.
///
/// Takes the modules rather than the `mlir::ModuleOp` holding them: which ones
/// to export is the caller's policy. `rtlil-run-yosys` passes every
/// `rtlil.module` and leaves the rest of the file standing, which is what lets
/// it run on a partly converted design; `export-rtlil` passes the same set but
/// first refuses a file that still holds an `hw.module`.
///
/// Validates all of `modules` before creating anything in `design`. That
/// ordering is not a nicety: Yosys reacts to a duplicate name or a malformed
/// identifier by ending the process, so any such problem has to become an MLIR
/// diagnostic before the first `addModule()` call. Returns failure after
/// emitting a diagnostic; on failure `design` may hold partially built modules.
mlir::LogicalResult exportRTLIL(llvm::ArrayRef<rtlil::ModuleOp> modules,
                                Yosys::RTLIL::Design *design);

/// Register the `export-rtlil` translation, which writes a `.il` file through
/// Yosys' own RTLIL backend.
void registerExportRTLILTranslation();

} // namespace rtlil
} // namespace circt

#endif // CIRCT_CONVERSION_EXPORTRTLIL_H
