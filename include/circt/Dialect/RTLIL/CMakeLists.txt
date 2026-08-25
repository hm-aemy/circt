add_circt_dialect(RTLIL rtlil)
add_circt_dialect_doc(RTLIL rtlil)
add_circt_interface(RTLILOpInterfaces)

set(LLVM_TARGET_DEFINITIONS RTLIL.td)

mlir_tablegen(RTLILEnums.h.inc -gen-enum-decls)
mlir_tablegen(RTLILEnums.cpp.inc -gen-enum-defs)
add_public_tablegen_target(CIRCTRTLILEnumsIncGen)
add_dependencies(circt-headers CIRCTRTLILEnumsIncGen)

mlir_tablegen(RTLILAttrDefs.h.inc -gen-attrdef-decls -attrdefs-dialect=rtlil)
mlir_tablegen(RTLILAttrDefs.cpp.inc -gen-attrdef-defs -attrdefs-dialect=rtlil)
add_public_tablegen_target(CIRCTRTLILAttrDefsIncGen)
add_dependencies(circt-headers CIRCTRTLILAttrDefsIncGen)

# The RTLIL passes need libyosys, so they are declared here rather than in
# `circt/Conversion/Passes.td` -- see RTLILPasses.td for why. The tablegen
# itself is unconditional; only the library that implements the passes is
# gated on CIRCT_YOSYS_LIB_ENABLED.
set(LLVM_TARGET_DEFINITIONS RTLILPasses.td)
mlir_tablegen(RTLILPasses.h.inc -gen-pass-decls)
add_public_tablegen_target(CIRCTRTLILTransformsIncGen)
add_dependencies(circt-headers CIRCTRTLILTransformsIncGen)
add_circt_doc(RTLILPasses RTLILPasses -gen-pass-doc)
