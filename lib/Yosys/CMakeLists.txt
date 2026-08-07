# Must come before `add_circt_library()`: it works through directory-scoped
# variables that the LLVM helpers read while creating the target. Restores
# exceptions and RTTI, which the Yosys headers need and CIRCT disables globally.
include(YosysCompilerOptions)

# Yosys resolves its data directory and `yosys-abc` relative to the running
# executable, which does not hold for a CIRCT binary in general. `Yosys.cpp`
# prefers the executable-relative layout and falls back to these.
#
# Set at directory scope rather than with `target_compile_definitions()`: the
# `obj.*` object library `llvm_add_library()` may create does not inherit a
# target's compile definitions, and it is what actually compiles the sources.
add_compile_definitions(
  CIRCT_YOSYS_DATADIR="${CIRCT_YOSYS_DATADIR}"
  CIRCT_YOSYS_ABC_EXECUTABLE="${CIRCT_YOSYS_ABC_EXECUTABLE}"
)

add_circt_library(CIRCTYosys
  Yosys.cpp

  # `add_dependencies()` on the imported target only reaches targets that link
  # it, which does not include the `obj.*` object library `llvm_add_library()`
  # may create -- it would try to compile against headers Yosys has not
  # installed yet. `DEPENDS` is applied to both.
  DEPENDS
  yosys_external

  LINK_LIBS PUBLIC
  CIRCTSupport
  PRIVATE
  Yosys::libyosys
)
