# Must precede the target: sets C++20 for the Yosys headers.
include(YosysCompilerOptions)

add_circt_translation_library(CIRCTExportRTLIL
  ExportRTLIL.cpp

  # `DEPENDS` also reaches the `obj.*` library, so Yosys headers exist first.
  DEPENDS
  yosys_external

  LINK_COMPONENTS
  Support

  LINK_LIBS PUBLIC
  # The dialects the translation registers, so mixed-dialect input parses.
  CIRCTComb
  CIRCTDebug
  CIRCTEmit
  CIRCTHW
  CIRCTLTL
  CIRCTOM
  CIRCTRTLIL
  CIRCTSV
  CIRCTSeq
  CIRCTSim
  CIRCTSupport
  CIRCTVerif
  MLIRIR
  MLIRSupport
  MLIRTranslateLib

  PRIVATE
  CIRCTYosys
  Yosys::libyosys
)
