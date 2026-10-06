set(LLVM_OPTIONAL_SOURCES
  RunYosys.cpp
  YosysScript.cpp
  )

set(RTLIL_TRANSFORMS_SOURCES
  RunYosys.cpp
  )

set(RTLIL_TRANSFORMS_DEPENDS
  CIRCTRTLILTransformsIncGen
  )

set(RTLIL_TRANSFORMS_PRIVATE_LIBS)

# The pass is always registered; without libyosys it only reports an error.
if(CIRCT_YOSYS_LIB_ENABLED)
  # Must precede the target: sets C++20 for the Yosys headers.
  include(YosysCompilerOptions)

  # Directory scope, as the `obj.*` library ignores target definitions.
  add_compile_definitions(CIRCT_YOSYS_LIB_ENABLED)

  list(APPEND RTLIL_TRANSFORMS_SOURCES YosysScript.cpp)
  list(APPEND RTLIL_TRANSFORMS_DEPENDS yosys_external)
  list(APPEND RTLIL_TRANSFORMS_PRIVATE_LIBS
    CIRCTExportRTLIL
    CIRCTImportRTLIL
    CIRCTYosys
    Yosys::libyosys
    )

  # Only this file needs exceptions.
  set_source_files_properties(YosysScript.cpp PROPERTIES
    COMPILE_OPTIONS "-fexceptions")
endif()

add_circt_dialect_library(CIRCTRTLILTransforms
  ${RTLIL_TRANSFORMS_SOURCES}

  DEPENDS
  ${RTLIL_TRANSFORMS_DEPENDS}

  LINK_LIBS PUBLIC
  CIRCTRTLIL
  CIRCTSupport
  MLIRIR
  MLIRPass

  PRIVATE
  ${RTLIL_TRANSFORMS_PRIVATE_LIBS}
  )
