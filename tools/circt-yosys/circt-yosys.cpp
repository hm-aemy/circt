//===- circt-yosys.cpp - Driving Yosys from CIRCT -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A proof of concept for the in-process Yosys library: start from a CIRCT
// `hw.module`, hand-build the equivalent Yosys design, run real Yosys passes
// over it, and read the result back -- all in one process, without writing a
// file or spawning the `yosys` binary.
//
// The point is that both halves live in the same translation unit: MLIR/CIRCT
// headers and Yosys headers coexist, and an `mlir::Value` can be used as the key
// of a map to an `RTLIL::Wire`. This is deliberately *not* a conversion pass.
// The input is a hardcoded module and the translation handles exactly the two
// operations it contains; a real conversion has to deal with the whole of Comb
// and HW, multi-operand ops, non-integer types, instances, and registers.
//
//===----------------------------------------------------------------------===//

#include "circt/Dialect/Comb/CombOps.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/HW/HWTypes.h"
#include "circt/Support/Version.h"
#include "circt/Yosys/Yosys.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Parser/Parser.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

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
/// paths were resolved correctly.
static constexpr llvm::StringRef passPipeline[] = {
    "hierarchy -check -top demo", "opt", "techmap", "abc -g AND,OR,XOR",
    "opt_clean"};

/// Translate `hwModule` into a fresh RTLIL module inside `design`.
///
/// Only what the hardcoded input above needs: integer-typed ports, two-operand
/// `comb.and`/`comb.or`, and `hw.output`. Anything else is reported rather than
/// silently ignored, because a demo that quietly drops operations would be worse
/// than one that stops.
static Yosys::RTLIL::Module *buildRtlilFrom(Yosys::RTLIL::Design *design,
                                            hw::HWModuleOp hwModule) {
  auto *module = design->addModule("\\" + hwModule.getModuleName().str());
  Block *body = hwModule.getBodyBlock();

  // Ports first. `mlir::Value` is a handle, so it works as a DenseMap key
  // directly -- this map is the whole of the translation state.
  llvm::DenseMap<Value, Yosys::RTLIL::Wire *> wires;
  llvm::SmallVector<Yosys::RTLIL::Wire *> outputWires;
  for (const hw::PortInfo &port : hwModule.getPortList()) {
    auto *wire = module->addWire("\\" + port.getName().str(),
                                 hw::getBitWidth(port.type));
    if (port.isInput()) {
      wire->port_input = true;
      wires[body->getArgument(port.argNum)] = wire;
    } else {
      wire->port_output = true;
      outputWires.push_back(wire);
    }
  }
  module->fixup_ports();

  // Then the body, in order. Every op defines at most one result, and operands
  // are always already in the map because an `hw.module` body is a graph region
  // that this demo keeps in topological order.
  unsigned cellIndex = 0;
  for (Operation &op : body->getOperations()) {
    if (auto outputOp = dyn_cast<hw::OutputOp>(op)) {
      for (auto [wire, value] : llvm::zip(outputWires, outputOp.getOperands()))
        module->connect(wire, wires.lookup(value));
      continue;
    }

    // `comb.and`/`comb.or` are variadic; the RTLIL cells are strictly binary,
    // so a real conversion would decompose. Here we only accept what we emit.
    if (op.getNumOperands() != 2 || op.getNumResults() != 1) {
      llvm::WithColor::error(llvm::errs(), "circt-yosys")
          << "unsupported operation: " << op << "\n";
      return nullptr;
    }
    auto *lhs = wires.lookup(op.getOperand(0));
    auto *rhs = wires.lookup(op.getOperand(1));
    auto *result =
        module->addWire("$" + std::to_string(cellIndex),
                        hw::getBitWidth(op.getResult(0).getType()));
    wires[op.getResult(0)] = result;

    auto name = "$cell_" + std::to_string(cellIndex++);
    if (isa<comb::AndOp>(op))
      module->addAnd(name, lhs, rhs, result);
    else if (isa<comb::OrOp>(op))
      module->addOr(name, lhs, rhs, result);
    else {
      llvm::WithColor::error(llvm::errs(), "circt-yosys")
          << "no RTLIL mapping for: " << op << "\n";
      return nullptr;
    }
  }

  return module;
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
      "  Builds a hardcoded hw.module, translates it to a Yosys design, runs\n"
      "  Yosys passes over it, and prints the resulting cells. Yosys' own log\n"
      "  goes to stderr; this tool's output goes to stdout.\n");

  // The CIRCT half.
  MLIRContext context;
  context.loadDialect<hw::HWDialect, comb::CombDialect>();
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
  auto *module = buildRtlilFrom(&design, hwModule);
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
