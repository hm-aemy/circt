##===- YosysCompilerOptions.cmake ------------------------------*- cmake -*-===//
##
## Compiler settings for CIRCT libraries that include the Yosys headers.
## Include it before `add_circt_library()`, as it sets directory-scoped
## variables the LLVM helpers read when creating the target.
##
## Exceptions are not enabled here: `LLVM_REQUIRES_EH` also forces RTTI, which
## breaks linking against `mlir::Pass`. See `lib/Dialect/RTLIL/Transforms` for
## the one file that needs them.
##
##===----------------------------------------------------------------------===//

# The Yosys headers require C++20. The `obj.*` library does not inherit the
# `cxx_std_20` feature from `Yosys::libyosys`, so set it at directory scope.
set(CMAKE_CXX_STANDARD 20)

# Precompiled headers would be built for C++17 and break under C++20.
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
