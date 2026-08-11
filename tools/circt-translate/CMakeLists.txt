get_property(dialect_libs GLOBAL PROPERTY CIRCT_DIALECT_LIBS)
get_property(translation_libs GLOBAL PROPERTY CIRCT_TRANSLATION_LIBS)

if(CIRCT_SLANG_FRONTEND_ENABLED)
  add_compile_definitions(CIRCT_SLANG_FRONTEND_ENABLED)
endif()

# The ImportRTLIL/ExportRTLIL headers name `RTLIL::Design` through a forward
# declaration and pull in no Yosys header, so this directory needs the define
# but not `YosysCompilerOptions`.
if(CIRCT_YOSYS_LIB_ENABLED)
  add_compile_definitions(CIRCT_YOSYS_LIB_ENABLED)
endif()

set(LLVM_LINK_COMPONENTS
  Support
)

add_circt_tool(circt-translate
  circt-translate.cpp
)

llvm_update_compile_flags(circt-translate)

target_link_libraries(circt-translate
  PRIVATE
  ${dialect_libs}
  ${translation_libs}
  MLIRExportSMTLIB
  MLIRIR
  MLIRSupport
  MLIRTranslateLib
)
