# Must come before `add_circt_translation_library()`: it works through
# directory-scoped variables that the LLVM helpers read while creating the
# target. Lifts this directory to C++20, which the Yosys headers require.
include(YosysCompilerOptions)

add_circt_translation_library(CIRCTExportRTLIL
  ExportRTLIL.cpp

  # `add_dependencies()` on the imported target only reaches targets that link
  # it, which does not include the `obj.*` object library `llvm_add_library()`
  # creates (it would try to compile against headers Yosys has not installed
  # yet). `DEPENDS` is applied to both.
  DEPENDS
  yosys_external

  LINK_COMPONENTS
  Support

  LINK_LIBS PUBLIC
  # Besides CIRCTRTLIL, the dialects the translation registers so that the file
  # it is handed parses at all. See the `DialectRegistry` callback in
  # `ExportRTLIL.cpp`.
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
