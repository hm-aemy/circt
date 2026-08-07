//===- YosysTest.cpp - Yosys library integration tests --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// End-to-end check that the embedded Yosys library actually works: a design is
// built in memory, real passes run over it, and the result is read back --
// without writing a file or spawning the `yosys` binary.
//
// This is deliberately more than a smoke test of `initialize()`. `techmap`
// reads a technology library from Yosys' data directory and `abc` execs
// `yosys-abc`, so the passes below fail unless *both* paths resolved. That is
// the failure worth catching: Yosys locates them relative to the running
// executable, and this test binary does not sit in `bin/`, so it exercises the
// compiled-in fallback rather than the layout.
//
//===----------------------------------------------------------------------===//

#include "circt/Yosys/Yosys.h"

#include "llvm/Support/Error.h"
#include "gtest/gtest.h"

// Yosys headers stay after the LLVM ones; see `lib/Yosys/Yosys.cpp`.
#include "kernel/rtlil.h"
#include "kernel/yosys.h"

using namespace circt;

namespace {

/// Bring the library up once for the whole test binary. `yosys_setup()` is
/// global state, and Yosys does not support tearing it down and back up.
class YosysEnvironment : public ::testing::Environment {
public:
  void SetUp() override {
    ASSERT_FALSE(bool(yosys::initialize()))
        << "failed to initialize the Yosys library";
  }
  void TearDown() override { yosys::shutdown(); }
};

const auto *const environment =
    ::testing::AddGlobalTestEnvironment(new YosysEnvironment);

/// Build an 8-bit adder as RTLIL, the way a frontend would. Identifiers are
/// `IdString`s: a leading backslash marks a public name, a leading dollar sign
/// an internal one.
Yosys::RTLIL::Module *buildAdder(Yosys::RTLIL::Design *design) {
  auto *module = design->addModule("\\adder");

  auto *a = module->addWire("\\a", 8);
  a->port_input = true;
  auto *b = module->addWire("\\b", 8);
  b->port_input = true;
  auto *y = module->addWire("\\y", 8);
  y->port_output = true;
  module->fixup_ports();

  module->addAdd("$add", a, b, y);
  return module;
}

TEST(YosysTest, ResolvesDataDirectory) {
  EXPECT_FALSE(yosys::getDataDir().empty());
  // Yosys appends to this, so the separator has to be there.
  EXPECT_EQ(Yosys::yosys_share_dirname, yosys::getDataDir() + "/");
}

TEST(YosysTest, BuildsDesignInMemory) {
  Yosys::RTLIL::Design design;
  auto *module = buildAdder(&design);

  EXPECT_EQ(module->name.str(), "\\adder");
  EXPECT_EQ(module->cells().size(), 1u);
  EXPECT_EQ(design.modules().size(), 1u);
}

TEST(YosysTest, RunsPassesOverDesign) {
  Yosys::RTLIL::Design design;
  auto *module = buildAdder(&design);

  // Addressed by the same command strings a script would use.
  Yosys::run_pass("hierarchy -check -top adder", &design);
  Yosys::run_pass("techmap", &design);
  Yosys::run_pass("opt_clean", &design);

  // `techmap` replaces the single `$add` with a gate-level netlist, so the
  // design is both larger and free of the original cell.
  EXPECT_GT(module->cells().size(), 1u);
  for (auto *cell : module->cells())
    EXPECT_NE(cell->type.str(), "$add");
}

TEST(YosysTest, RunsAbc) {
  // Nothing to run against when Yosys was built with an integrated or disabled
  // ABC; the library reports no executable in that case.
  if (yosys::getAbcExecutable().empty())
    GTEST_SKIP() << "Yosys was built without an external yosys-abc";

  Yosys::RTLIL::Design design;
  auto *module = buildAdder(&design);

  Yosys::run_pass("hierarchy -check -top adder", &design);
  Yosys::run_pass("techmap", &design);
  Yosys::run_pass("abc -g AND,OR,XOR", &design);
  Yosys::run_pass("opt_clean", &design);

  // ABC maps everything onto the gates it was restricted to, plus inverters.
  EXPECT_GT(module->cells().size(), 0u);
  for (auto *cell : module->cells()) {
    auto type = cell->type.str();
    EXPECT_TRUE(type == "$_AND_" || type == "$_OR_" || type == "$_XOR_" ||
                type == "$_NOT_")
        << "unexpected cell type " << type;
  }
}

} // namespace
