add_mlir_dialect(RTLILOps rtlil)
# add_mlir_doc(RTLILDialect RTLILDialect RTLIL/ -gen-dialect-doc)
# add_mlir_doc(RTLILOps RTLILOps RTLIL/ -gen-op-doc)

set(LLVM_TARGET_DEFINITIONS RTLILOps.td)
mlir_tablegen(RTLILAttrDefs.h.inc -gen-attrdef-decls
-attrdefs-dialect=rtlil)
mlir_tablegen(RTLILAttrDefs.cpp.inc -gen-attrdef-defs
-attrdefs-dialect=rtlil)
add_public_tablegen_target(MLIRRTLILAttrDefsIncGen)

set(LLVM_TARGET_DEFINITIONS RTLILOps.td)
mlir_tablegen(RTLILEnums.h.inc -gen-enum-decls)
mlir_tablegen(RTLILEnums.cpp.inc -gen-enum-defs)
add_public_tablegen_target(MLIRRTLILEnumDefsIncGen)

set(LLVM_TARGET_DEFINITIONS RTLILOps.td)
mlir_tablegen(RTLILInterfaces.h.inc -gen-op-interface-decls)
mlir_tablegen(RTLILInterfaces.cpp.inc -gen-op-interface-defs)
add_public_tablegen_target(MLIRRTLILInterfaceDefsIncGen)
# The RTLIL passes need libyosys, so they are declared here rather than in
# `circt/Conversion/Passes.td` -- see RTLILPasses.td for why. The tablegen
# itself is unconditional; only the library that implements the passes is
# gated on CIRCT_YOSYS_LIB_ENABLED.
set(LLVM_TARGET_DEFINITIONS RTLILPasses.td)
mlir_tablegen(RTLILPasses.h.inc -gen-pass-decls)
add_public_tablegen_target(CIRCTRTLILTransformsIncGen)
