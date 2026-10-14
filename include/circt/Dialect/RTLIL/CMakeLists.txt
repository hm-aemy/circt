add_circt_dialect(RTLIL rtlil)
add_circt_dialect_doc(RTLIL rtlil)
add_circt_interface(RTLILOpInterfaces)

set(LLVM_TARGET_DEFINITIONS RTLIL.td)

mlir_tablegen(RTLILEnums.h.inc -gen-enum-decls)
mlir_tablegen(RTLILEnums.cpp.inc -gen-enum-defs)
add_public_tablegen_target(CIRCTRTLILEnumsIncGen)
add_dependencies(circt-headers CIRCTRTLILEnumsIncGen)

mlir_tablegen(RTLILAttributes.h.inc -gen-attrdef-decls -attrdefs-dialect=rtlil)
mlir_tablegen(RTLILAttributes.cpp.inc -gen-attrdef-defs -attrdefs-dialect=rtlil)
add_public_tablegen_target(CIRCTRTLILAttributesIncGen)
add_dependencies(circt-headers CIRCTRTLILAttributesIncGen)
