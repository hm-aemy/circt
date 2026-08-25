add_circt_dialect_library(CIRCTRTLIL
  RTLIL.cpp
  RTLILOps.cpp
  RTLILTypes.cpp

  ADDITIONAL_HEADER_DIRS
  ${CIRCT_MAIN_INCLUDE_DIR}/circt/Dialect/RTLIL

  DEPENDS
  MLIRRTLILIncGen
  CIRCTRTLILAttrDefsIncGen
  CIRCTRTLILEnumsIncGen
  MLIRRTLILOpInterfacesIncGen

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRDerivedAttributeOpInterface
)

if(CIRCT_YOSYS_LIB_ENABLED)
  add_subdirectory(Transforms)
endif()
