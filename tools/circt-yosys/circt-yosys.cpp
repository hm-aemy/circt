//===- circt-yosys.cpp - Driving Yosys from CIRCT -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A proof of concept for the in-process Yosys library: start from a CIRCT
// `hw.module`, lower it to the RTLIL dialect with `convert-hw-to-rtlil`, hand
// the result to Yosys as a real `RTLIL::Design`, run Yosys passes over it, and
// read the result back -- all in one process, without writing a file or
// spawning the `yosys` binary.
//
// The point is that both halves live in the same translation unit: MLIR/CIRCT
// headers and Yosys headers coexist, and an `mlir::Value` can be used as the
// key of a map to an `RTLIL::SigSpec`. What this file does *not* contain is any
// hardware-specific knowledge: that all sits in the conversion pass, and what
// is left here is a mechanical walk over the RTLIL dialect. `rtlil.wire`
// becomes an `RTLIL::Wire`, `rtlil.const` an `RTLIL::Const`, and every op
// implementing `rtlil::CellOpInterface` an `RTLIL::Cell` -- the dialect already
// carries the cell type, port names, and parameters Yosys wants, so the walk
// does not grow a case per Comb/HW operation.
//
//===----------------------------------------------------------------------===//

#include "circt/Conversion/HWToRTLIL.h"
#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/HW/HWDialect.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/RTLIL/RTLIL.h"
#include "circt/Dialect/RTLIL/RTLILOps.h"
#include "circt/Dialect/RTLIL/RTLILTypes.h"
#include "circt/Dialect/Seq/SeqDialect.h"
#include "circt/Support/Version.h"
#include "circt/Yosys/Yosys.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <vector>

// The Yosys headers come last. Everything below stays explicitly qualified with
// `Yosys::`; Yosys' own sources use the `USING_YOSYS_NAMESPACE` macro instead,
// but an embedder does not want that namespace pulled into its translation
// units. Beyond C++20 these headers need nothing special from the build any
// more -- see `cmake/modules/YosysCompilerOptions.cmake`.
#include "kernel/rtlil.h"
#include "kernel/yosys.h"

using namespace circt;
using namespace mlir;
namespace cl = llvm::cl;

/// The CIRCT-side input: an 8-bit `(a & b) | c`.
static constexpr llvm::StringRef demoIR = R"MLIR(
  hw.module @demo(in %a: i8, in %b: i8, in %c: i8, out y: i8) {
    %0 = comb.and %a, %b : i8
    %1 = comb.or %0, %c : i8
    hw.output %1 : i8
  }
)MLIR";

/// The passes to run over the design once it has been built. `techmap` reads a
/// technology library out of Yosys' data directory and `abc` execs the
/// `yosys-abc` binary, so getting through this list also proves both runtime
/// paths were resolved correctly. `-auto-top` because the conversion renames
/// modules -- `@demo` comes out as `\demo_<n>` -- so there is no name to
/// hardcode here.
static constexpr llvm::StringRef passPipeline[] = {
    "hierarchy -check -auto-top", "opt", "techmap", "abc -g AND,OR,XOR",
    "opt_clean"};

/// RTLIL identifiers carry their own escaping: the dialect already emits names
/// with the leading `\` (public) or `$` (auto-generated) that Yosys expects, so
/// they go through unchanged.
static Yosys::RTLIL::IdString id(llvm::StringRef name) {
  return Yosys::RTLIL::IdString(std::string_view(name.data(), name.size()));
}

static Yosys::RTLIL::State toState(rtlil::StateEnum state) {
  switch (state) {
  case rtlil::StateEnum::S0:
    return Yosys::RTLIL::State::S0;
  case rtlil::StateEnum::S1:
    return Yosys::RTLIL::State::S1;
  case rtlil::StateEnum::Sx:
    return Yosys::RTLIL::State::Sx;
  case rtlil::StateEnum::Sz:
    return Yosys::RTLIL::State::Sz;
  case rtlil::StateEnum::Sa:
    return Yosys::RTLIL::State::Sa;
  }
  llvm_unreachable("unhandled RTLIL state");
}

namespace {
/// Translates the RTLIL-dialect ops of one `builtin.module` into a Yosys
/// module. Every value in that body is defined by an `rtlil.wire` or an
/// `rtlil.const`, so a map from `mlir::Value` to `RTLIL::SigSpec` is the whole
/// of the translation state.
class ModuleEmitter {
public:
  ModuleEmitter(Yosys::RTLIL::Design *design) : design(design) {}

  /// Emit `op` into the design, or return null after reporting why not.
  Yosys::RTLIL::Module *emit(mlir::ModuleOp op);

private:
  /// The signal a value stands for, or null if it was never defined. The
  /// conversion pass leaves the body topologically sorted, so a definition is
  /// always seen before its uses.
  const Yosys::RTLIL::SigSpec *lookup(Value value, Operation *user);

  bool emitWire(rtlil::WireOp op);
  bool emitConst(rtlil::ConstOp op);
  bool emitConnection(rtlil::WConnectionOp op);
  bool emitCell(rtlil::CellOpInterface op);

  Yosys::RTLIL::Design *design;
  Yosys::RTLIL::Module *module = nullptr;
  llvm::DenseMap<Value, Yosys::RTLIL::SigSpec> signals;
};
} // namespace

Yosys::RTLIL::Module *ModuleEmitter::emit(mlir::ModuleOp op) {
  auto name = op.getSymName();
  if (!name) {
    op.emitError("module has no name to give the RTLIL module");
    return nullptr;
  }
  if (design->has(id(*name))) {
    op.emitError("design already contains a module named ") << *name;
    return nullptr;
  }

  module = design->addModule(id(*name));
  signals.clear();

  for (Operation &nested : op.getBody()->getOperations()) {
    bool ok = false;
    if (auto wire = dyn_cast<rtlil::WireOp>(nested))
      ok = emitWire(wire);
    else if (auto constant = dyn_cast<rtlil::ConstOp>(nested))
      ok = emitConst(constant);
    else if (auto connection = dyn_cast<rtlil::WConnectionOp>(nested))
      ok = emitConnection(connection);
    else if (auto cell = dyn_cast<rtlil::CellOpInterface>(nested))
      ok = emitCell(cell);
    else
      nested.emitError("no RTLIL mapping for this operation");
    if (!ok)
      return nullptr;
  }

  // Collects the wires flagged as ports into `module->ports` and renumbers
  // their `port_id`s; the dialect only records the flags and the ordering.
  module->fixup_ports();
  return module;
}

const Yosys::RTLIL::SigSpec *ModuleEmitter::lookup(Value value,
                                                   Operation *user) {
  auto it = signals.find(value);
  if (it == signals.end()) {
    user->emitError("operand has no RTLIL signal; expected it to be defined by "
                    "an rtlil.wire or rtlil.const");
    return nullptr;
  }
  return &it->second;
}

bool ModuleEmitter::emitWire(rtlil::WireOp op) {
  auto *wire = module->addWire(id(op.getName()), op.getWidth().getInt());
  wire->port_id = op.getPortId();
  wire->port_input = op.getPortInput();
  wire->port_output = op.getPortOutput();
  wire->start_offset = op.getStartOffset();
  wire->upto = op.getUpto();
  wire->is_signed = op.getIsSigned();
  signals.try_emplace(op.getResult(), wire);
  return true;
}

bool ModuleEmitter::emitConst(rtlil::ConstOp op) {
  std::vector<Yosys::RTLIL::State> bits;
  bits.reserve(op.getValue().size());
  // Least significant bit first, the order both the dialect attribute and
  // `RTLIL::Const` use.
  for (Attribute bit : op.getValue())
    bits.push_back(toState(cast<rtlil::StateEnumAttr>(bit).getValue()));
  signals.try_emplace(op.getResult(),
                      Yosys::RTLIL::Const(std::move(bits)));
  return true;
}

bool ModuleEmitter::emitConnection(rtlil::WConnectionOp op) {
  const auto *lhs = lookup(op.getLhs(), op);
  const auto *rhs = lookup(op.getRhs(), op);
  if (!lhs || !rhs)
    return false;
  module->connect(*lhs, *rhs);
  return true;
}

bool ModuleEmitter::emitCell(rtlil::CellOpInterface op) {
  ArrayAttr ports = op.getCellPorts();
  OperandRange connections = op.getCellConnections();
  if (ports.size() != connections.size()) {
    op->emitError("cell has ")
        << ports.size() << " port names but " << connections.size()
        << " connections";
    return false;
  }

  auto *cell = module->addCell(id(op.getCellName()), id(op.getCellType()));
  for (auto [port, value] : llvm::zip(ports, connections)) {
    const auto *signal = lookup(value, op);
    if (!signal)
      return false;
    cell->setPort(id(cast<StringAttr>(port).getValue()), *signal);
  }
  for (Attribute parameter : op.getCellParameters()) {
    auto param = cast<rtlil::ParameterAttr>(parameter);
    IntegerAttr value = param.getValue();
    cell->setParam(id(param.getName().getValue()),
                   Yosys::RTLIL::Const(value.getInt(),
                                       value.getType().getIntOrFloatBitWidth()));
  }
  return true;
}

/// Translate every module of `mlirModule` -- the conversion pass turns each
/// `hw.module` into a named `builtin.module` of RTLIL ops -- into `design`.
/// Returns the module that was emitted last, for reporting.
static Yosys::RTLIL::Module *buildDesign(Yosys::RTLIL::Design *design,
                                         mlir::ModuleOp mlirModule) {
  Yosys::RTLIL::Module *last = nullptr;
  for (auto nested : mlirModule.getOps<mlir::ModuleOp>()) {
    ModuleEmitter emitter(design);
    last = emitter.emit(nested);
    if (!last)
      return nullptr;
  }
  if (!last)
    mlirModule.emitError("no modules to hand to Yosys");
  return last;
}

/// Print how many cells of each type the module contains, in a stable order.
/// This is the half that matters for a real integration: after the passes run
/// the design is still an ordinary C++ object, and walking it is where a
/// translation back into CIRCT's IR would happen.
static void printCellHistogram(llvm::raw_ostream &os,
                               Yosys::RTLIL::Module *module) {
  std::map<std::string, unsigned> counts;
  for (auto *cell : module->cells())
    ++counts[cell->type.str()];

  os << "cells in " << module->name.str() << ":\n";
  for (const auto &[type, count] : counts)
    os << "  " << count << "x " << type << "\n";
}

int main(int argc, char **argv) {
  llvm::InitLLVM y(argc, argv);

  cl::AddExtraVersionPrinter([](llvm::raw_ostream &os) {
    os << getCirctVersion() << '\n';
    os << "yosys version " << Yosys::yosys_version_str << '\n';
  });
  cl::ParseCommandLineOptions(
      argc, argv,
      "circt-yosys - drive Yosys as a library from CIRCT\n\n"
      "  Builds a hardcoded hw.module, lowers it to the RTLIL dialect,\n"
      "  translates that to a Yosys design, runs Yosys passes over it, and\n"
      "  prints the resulting cells. Yosys' own log goes to stderr; this\n"
      "  tool's output goes to stdout.\n");

  // The CIRCT half.
  MLIRContext context;
  context.loadDialect<hw::HWDialect, comb::CombDialect, seq::SeqDialect,
                      rtlil::RTLILDialect>();
  OwningOpRef<ModuleOp> mlirModule = parseSourceString<ModuleOp>(demoIR,
                                                                 &context);
  if (!mlirModule) {
    llvm::WithColor::error(llvm::errs(), "circt-yosys")
        << "failed to parse the demo module\n";
    return 1;
  }
  auto hwModule = SymbolTable(mlirModule.get()).lookup<hw::HWModuleOp>("demo");
  assert(hwModule && "demo module must exist");

  llvm::outs() << "input hw.module:\n";
  hwModule.print(llvm::outs());
  llvm::outs() << "\n\n";

  PassManager pm(&context);
  pm.addPass(createConvertHWToRTLIL());
  if (failed(pm.run(*mlirModule))) {
    llvm::WithColor::error(llvm::errs(), "circt-yosys")
        << "failed to lower to the RTLIL dialect\n";
    return 1;
  }

  llvm::outs() << "after convert-hw-to-rtlil:\n";
  mlirModule->print(llvm::outs());
  llvm::outs() << "\n\n";

  // The Yosys half. Registers Yosys' built-in passes and points it at its data
  // directory and `yosys-abc`, both located relative to this executable when
  // possible.
  if (auto error = yosys::initialize()) {
    llvm::logAllUnhandledErrors(
        std::move(error), llvm::WithColor::error(llvm::errs(), "circt-yosys"));
    return 1;
  }

  llvm::outs() << "yosys data directory: " << yosys::getDataDir() << "\n";
  llvm::outs() << "yosys-abc: "
               << (yosys::getAbcExecutable().empty()
                       ? "<none>"
                       : yosys::getAbcExecutable())
               << "\n";

  Yosys::RTLIL::Design design;
  auto *module = buildDesign(&design, *mlirModule);
  if (!module) {
    yosys::shutdown();
    return 1;
  }
  llvm::outs() << "built " << module->name.str() << " with "
               << module->cells().size() << " cells\n";

  // Passes are addressed by the same command strings a script would use.
  for (llvm::StringRef pass : passPipeline) {
    llvm::outs() << "running: " << pass << "\n";
    Yosys::run_pass(pass.str(), &design);
  }

  printCellHistogram(llvm::outs(), module);

  yosys::shutdown();
  return 0;
}
