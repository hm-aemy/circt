# Must precede the target: sets C++20 for the Yosys headers.
include(YosysCompilerOptions)

# Build-time fallbacks for `Yosys.cpp`. Directory scope, as the `obj.*`
# library ignores target definitions.
add_compile_definitions(
  CIRCT_YOSYS_DATADIR="${CIRCT_YOSYS_DATADIR}"
  CIRCT_YOSYS_ABC_EXECUTABLE="${CIRCT_YOSYS_ABC_EXECUTABLE}"
)

add_circt_library(CIRCTYosys
  Yosys.cpp

  # `DEPENDS` also reaches the `obj.*` library, so Yosys headers exist first.
  DEPENDS
  yosys_external

  LINK_LIBS PUBLIC
  CIRCTSupport
  PRIVATE
  Yosys::libyosys
)
