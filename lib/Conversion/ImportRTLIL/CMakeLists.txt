# Must precede the target: sets C++20 for the Yosys headers.
include(YosysCompilerOptions)

add_circt_translation_library(CIRCTImportRTLIL
  ImportRTLIL.cpp

  # `DEPENDS` also reaches the `obj.*` library, so Yosys headers exist first.
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
