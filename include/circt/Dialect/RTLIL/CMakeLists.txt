add_mlir_dialect(RTLILOps rtlil)
add_circt_doc(RTLILOps Dialects/RTLILOps -gen-op-doc -dialect rtlil)

set(LLVM_TARGET_DEFINITIONS RTLILEnums.td)
mlir_tablegen(RTLILEnums.h.inc -gen-enum-decls)
mlir_tablegen(RTLILEnums.cpp.inc -gen-enum-defs)
add_public_tablegen_target(CIRCTRTLILEnumsIncGen)
add_dependencies(circt-headers CIRCTRTLILEnumsIncGen)

set(LLVM_TARGET_DEFINITIONS RTLILOps.td)
mlir_tablegen(RTLILAttrDefs.h.inc -gen-attrdef-decls
-attrdefs-dialect=rtlil)
mlir_tablegen(RTLILAttrDefs.cpp.inc -gen-attrdef-defs
-attrdefs-dialect=rtlil)
add_public_tablegen_target(CIRCTRTLILAttrDefsIncGen)

# set(LLVM_TARGET_DEFINITIONS RTLILOps.td)
# mlir_tablegen(RTLILEnums.h.inc -gen-enum-decls)
# mlir_tablegen(RTLILEnums.cpp.inc -gen-enum-defs)
# add_public_tablegen_target(CIRCTRTLILEnumDefsIncGen)


set(LLVM_TARGET_DEFINITIONS RTLILOps.td)
mlir_tablegen(RTLILInterfaces.h.inc -gen-op-interface-decls)
mlir_tablegen(RTLILInterfaces.cpp.inc -gen-op-interface-defs)
add_public_tablegen_target(CIRCTRTLILInterfaceDefsIncGen)

# The RTLIL passes need libyosys, so they are declared here rather than in
# `circt/Conversion/Passes.td` -- see RTLILPasses.td for why. The tablegen
# itself is unconditional; only the library that implements the passes is
# gated on CIRCT_YOSYS_LIB_ENABLED.
set(LLVM_TARGET_DEFINITIONS RTLILPasses.td)
mlir_tablegen(RTLILPasses.h.inc -gen-pass-decls)
add_public_tablegen_target(CIRCTRTLILTransformsIncGen)
