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
  CIRCTLLHD
  CIRCTRTLIL
  CIRCTSeq
  MLIRPass
  MLIRSupport
  MLIRTransformUtils
  CIRCTTransforms
  CIRCTVerif
  MLIRTransforms
)
