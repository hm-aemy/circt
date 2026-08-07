//===- circt-yosys.cpp - Driving Yosys from CIRCT -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A proof of concept for the in-process Yosys library: build a small design
// with the Yosys API, run real Yosys passes over it, and read the result back
// -- without writing a file or spawning the `yosys` binary.
//
// The design is constructed as RTLIL directly rather than converted from CIRCT
// IR. Translating between the two is the interesting problem and is not
// attempted here; what this shows is that the machinery underneath it works.
//
//===----------------------------------------------------------------------===//

#include "circt/Support/Version.h"
#include "circt/Yosys/Yosys.h"

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
namespace cl = llvm::cl;

/// The passes to run over the design. `techmap` reads a technology library out
/// of Yosys' data directory and `abc` execs the `yosys-abc` binary, so getting
/// through this list also proves both runtime paths were resolved correctly.
static constexpr llvm::StringRef passPipeline[] = {
    "hierarchy -check -top demo", "opt", "techmap", "abc -g AND,OR,XOR",
    "opt_clean"};

/// Build the design a frontend would otherwise produce:
///
///     input a, b, c;  output y;
///     $and: a & b -> tmp
///     $or:  tmp | c -> y
///
/// Two cells, joined by the internal wire `\tmp`. Identifiers are `IdString`s,
/// where a leading backslash marks a public name -- one that came from the
/// source -- and a leading dollar sign an internal one.
static Yosys::RTLIL::Module *buildDemoDesign(Yosys::RTLIL::Design *design) {
  auto *module = design->addModule("\\demo");

  auto *a = module->addWire("\\a", 8);
  a->port_input = true;
  auto *b = module->addWire("\\b", 8);
  b->port_input = true;
  auto *c = module->addWire("\\c", 8);
  c->port_input = true;
  auto *y = module->addWire("\\y", 8);
  y->port_output = true;
  module->fixup_ports();

  // Not a port: purely the connection between the two cells.
  auto *tmp = module->addWire("\\tmp", 8);

  module->addAnd("$and", a, b, tmp);
  module->addOr("$or", tmp, c, y);
  return module;
}

/// Print how many cells of each type the module contains, in a stable order.
/// This is the half that matters for a real integration: after the passes run,
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
      "  Builds a small design with the Yosys API, runs Yosys passes over it,\n"
      "  and prints the resulting cells. Yosys' own log goes to stderr; this\n"
      "  tool's output goes to stdout.\n");

  // Registers Yosys' built-in passes and points it at its data directory and
  // `yosys-abc`. Both are located relative to this executable when possible,
  // which is why running from the build tree's `bin/` exercises a different
  // code path than the unit tests do.
  if (auto error = yosys::initialize()) {
    llvm::logAllUnhandledErrors(std::move(error),
                                llvm::WithColor::error(llvm::errs(), "circt-yosys"));
    return 1;
  }

  llvm::outs() << "yosys data directory: " << yosys::getDataDir() << "\n";
  llvm::outs() << "yosys-abc: "
               << (yosys::getAbcExecutable().empty()
                       ? "<none>"
                       : yosys::getAbcExecutable())
               << "\n";

  Yosys::RTLIL::Design design;
  auto *module = buildDemoDesign(&design);
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
