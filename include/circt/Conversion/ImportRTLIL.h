//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Reads a `Yosys::RTLIL::Design` back into `rtlil.module` ops, so that a design
// handed to Yosys' passes can re-enter CIRCT without going through a file.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_CONVERSION_IMPORTRTLIL_H
#define CIRCT_CONVERSION_IMPORTRTLIL_H

#include "circt/Support/LLVM.h"
#include "mlir/IR/BuiltinOps.h"

// See `ExportRTLIL.h` for why this forward declaration is sound.
namespace Yosys {
namespace RTLIL {
struct Design;
} // namespace RTLIL
} // namespace Yosys

namespace circt {
namespace rtlil {

/// Import every module of `design` into `module` as an `rtlil.module`.
///
/// Takes an existing `mlir::ModuleOp` rather than returning a fresh one because
/// the run-yosys pass imports back into the module it already owns; a
/// translation wrapper can create an empty one first.
///
/// Constructs that the dialect cannot represent (processes, memories and
/// bindings) are reported as errors rather than dropped, since dropping them
/// would silently change the design's meaning.
LogicalResult importRTLIL(Yosys::RTLIL::Design *design, mlir::ModuleOp module);

/// Register the `import-rtlil` translation, which reads a `.il` file through
/// Yosys' own RTLIL frontend.
void registerImportRTLILTranslation();

} // namespace rtlil
} // namespace circt

#endif // CIRCT_CONVERSION_IMPORTRTLIL_H
