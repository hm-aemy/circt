# Must come before `add_circt_translation_library()`: it works through
# directory-scoped variables that the LLVM helpers read while creating the
# target. Lifts this directory to C++20, which the Yosys headers require.
include(YosysCompilerOptions)

add_circt_translation_library(CIRCTImportRTLIL
  ImportRTLIL.cpp

  # See `lib/Conversion/ExportRTLIL/CMakeLists.txt`: `add_dependencies()` on
  # the imported target does not reach the `obj.*` object library that
  # actually compiles this.
  DEPENDS
  yosys_external

  LINK_COMPONENTS
  Support

  LINK_LIBS PUBLIC
  CIRCTRTLIL
  CIRCTSupport
  MLIRIR
  MLIRSupport
  MLIRTranslateLib

  PRIVATE
  CIRCTYosys
  Yosys::libyosys
)
