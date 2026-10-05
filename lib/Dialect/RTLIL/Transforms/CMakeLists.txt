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

# The pass is always registered. Without libyosys it only reports that it is
# unavailable.
if(CIRCT_YOSYS_LIB_ENABLED)
  # Must come first: works through directory-scoped variables the LLVM helpers
  # read while creating the target. Supplies C++20 for the Yosys headers.
  include(YosysCompilerOptions)

  # Directory scope, since the `obj.*` object library does not inherit a
  # target's compile definitions.
  add_compile_definitions(CIRCT_YOSYS_LIB_ENABLED)

  list(APPEND RTLIL_TRANSFORMS_SOURCES YosysScript.cpp)
  list(APPEND RTLIL_TRANSFORMS_DEPENDS yosys_external)
  list(APPEND RTLIL_TRANSFORMS_PRIVATE_LIBS
    CIRCTExportRTLIL
    CIRCTImportRTLIL
    CIRCTYosys
    Yosys::libyosys
    )

  # Only `YosysScript.cpp` catches `log_cmd_error_exception`, and only it gets
  # exceptions. Doing this at directory scope via `LLVM_REQUIRES_EH` would also
  # force RTTI on (`AddLLVM.cmake`), and `RunYosys.cpp` derives from
  # `mlir::Pass`, which is built without it, and the link then fails on missing
  # typeinfo. These come after CIRCT's global `-fno-exceptions`, so they win.
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
