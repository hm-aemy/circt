add_circt_conversion_library(CIRCTRTLILToCore
  RTLILToCore.cpp

  DEPENDS
  CIRCTConversionPassIncGen

  LINK_COMPONENTS
  Core

  LINK_LIBS PUBLIC
  CIRCTComb
  CIRCTDebug
  CIRCTHW
  CIRCTRTLIL
  CIRCTSeq
  MLIRPass
  MLIRSupport
  CIRCTTransforms
  CIRCTVerif
  MLIRTransforms
)
