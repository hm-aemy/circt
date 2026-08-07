# The test includes the Yosys headers directly, so it needs the same relaxed
# compiler settings as the library itself.
include(YosysCompilerOptions)

add_circt_unittest(CIRCTYosysTests
  YosysTest.cpp
)

target_link_libraries(CIRCTYosysTests
  PRIVATE
  CIRCTYosys
  Yosys::libyosys
)
