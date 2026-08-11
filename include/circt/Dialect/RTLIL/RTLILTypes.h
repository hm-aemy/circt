#ifndef CIRCT_DIALECT_RTLIL_RTLILTYPES_H
#define CIRCT_DIALECT_RTLIL_RTLILTYPES_H

#include "RTLIL.h"

namespace circt::rtlil {
bool isMValueType(mlir::Type type);
mlir::ArrayAttr createParamArrayAttr(
    mlir::MLIRContext *context,
    llvm::ArrayRef<std::tuple<llvm::StringRef, unsigned, uint64_t>> &&r);

/// Whether `name` is a legal RTLIL identifier: non-empty, starting with `\`
/// (public) or `$` (auto-generated), and containing no control character or
/// space.
///
/// This mirrors what `RTLIL::IdString` asserts when Yosys interns a name
/// (`kernel/rtlil.cc`, `really_insert`). Yosys reacts to a violation by ending
/// the process, so the dialect has to be the one that reports it: anything that
/// reaches the exporter must already be known good.
bool isValidIdentifier(llvm::StringRef name);

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
}; // namespace circt::rtlil

#endif
