#ifndef CIRCT_DIALECT_RTLIL_RTLILTYPES_H
#define CIRCT_DIALECT_RTLIL_RTLILTYPES_H

#include "circt/Dialect/RTLIL/RTLILDialect.h"
#include "circt/Support/LLVM.h"
#include "mlir/IR/BuiltinAttributeInterfaces.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Types.h"
#include "llvm/ADT/StringRef.h"

#define GET_TYPEDEF_CLASSES
#include "circt/Dialect/RTLIL/RTLILTypes.h.inc"

namespace circt::rtlil {

bool isMValueType(mlir::Type type);

/// Whether `name` is a legal RTLIL identifier: non-empty, starting with `\`
/// (public) or `$` (auto-generated), and containing no control character or
/// space.
///
/// This mirrors what `RTLIL::IdString` asserts when Yosys interns a name
/// (`kernel/rtlil.cc`, `really_insert`). Yosys reacts to a violation by ending
/// the process, so the dialect has to be the one that reports it: anything that
/// reaches the exporter must already be known good.
bool isValidIdentifier(llvm::StringRef name);

} // namespace circt::rtlil

#endif // CIRCT_DIALECT_RTLIL_RTLILTYPES_H
