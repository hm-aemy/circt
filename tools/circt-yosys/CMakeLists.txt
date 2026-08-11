# Lifts this directory to C++20, which the Yosys headers require, and turns off
# precompiled headers, which CIRCT builds for C++17.
#
# Unlike `lib/Yosys`, nothing here has to work around `llvm_add_library()`'s
# `obj.*` object libraries: `add_circt_tool()` ends up at `add_llvm_executable()`,
# which calls plain `add_executable()`. So the usage requirements of
# `Yosys::libyosys` -- and the dependency on the external Yosys build attached to
# it -- propagate normally.
include(YosysCompilerOptions)

set(LLVM_LINK_COMPONENTS Support)

add_circt_tool(circt-yosys circt-yosys.cpp)
target_link_libraries(circt-yosys
  PRIVATE
  CIRCTComb
  CIRCTHW
  CIRCTHWToRTLIL
  CIRCTRTLIL
  CIRCTSeq
  CIRCTSupport
  CIRCTYosys
  MLIRIR
  MLIRParser
  MLIRPass
  Yosys::libyosys
)

llvm_update_compile_flags(circt-yosys)
