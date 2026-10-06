//===- YosysTest.cpp - Yosys library integration tests --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Runs real Yosys passes on an in-memory design. `techmap` needs the data
// directory and `abc` needs `yosys-abc`, so this checks the build-time path
// fallbacks, as the test binary is not in `bin/`.
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

/// Initialize once per binary, as Yosys cannot be set up again after shutdown.
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

/// Build an 8-bit adder as RTLIL.
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
  // Used as a prefix, so it needs the trailing separator.
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

  Yosys::run_pass("hierarchy -check -top adder", &design);
  Yosys::run_pass("techmap", &design);
  Yosys::run_pass("opt_clean", &design);

  // `techmap` replaces the `$add` with gates.
  EXPECT_GT(module->cells().size(), 1u);
  for (auto *cell : module->cells())
    EXPECT_NE(cell->type.str(), "$add");
}

TEST(YosysTest, RunsAbc) {
  if (yosys::getAbcExecutable().empty())
    GTEST_SKIP() << "Yosys was built without an external yosys-abc";

  Yosys::RTLIL::Design design;
  auto *module = buildAdder(&design);

  Yosys::run_pass("hierarchy -check -top adder", &design);
  Yosys::run_pass("techmap", &design);
  Yosys::run_pass("abc -g AND,OR,XOR", &design);
  Yosys::run_pass("opt_clean", &design);

  // Only the requested gates plus inverters remain.
  EXPECT_GT(module->cells().size(), 0u);
  for (auto *cell : module->cells()) {
    auto type = cell->type.str();
    EXPECT_TRUE(type == "$_AND_" || type == "$_OR_" || type == "$_XOR_" ||
                type == "$_NOT_")
        << "unexpected cell type " << type;
  }
}

} // namespace
