add_circt_dialect_library(CIRCTRTLIL
  RTLILAttributes.cpp
  RTLILDialect.cpp
  RTLILOpInterfaces.cpp
  RTLILOps.cpp
  RTLILTypes.cpp

  ADDITIONAL_HEADER_DIRS
  ${CIRCT_MAIN_INCLUDE_DIR}/circt/Dialect/RTLIL

  DEPENDS
  MLIRRTLILIncGen
  CIRCTRTLILAttributesIncGen
  CIRCTRTLILEnumsIncGen
  MLIRRTLILOpInterfacesIncGen

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRDerivedAttributeOpInterface
)

add_subdirectory(Transforms)
