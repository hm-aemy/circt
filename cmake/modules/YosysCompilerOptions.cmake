##===- YosysCompilerOptions.cmake ------------------------------*- cmake -*-===//
##
## Compiler settings for CIRCT libraries that include the Yosys headers.
## Include this module at the top of such a library's CMakeLists.txt, the same
## way `SlangCompilerOptions.cmake` is used for the slang frontend. It has to
## come *before* `add_circt_library()`, because it works through
## directory-scoped variables that the LLVM helpers read while creating the
## target.
##
## There is very little left to do here. The Yosys headers used to demand
## `-D_YOSYS_`, exceptions, and RTTI on top of C++20; they now self-configure
## from the installed `kernel/yosys_config.h`, contain no `dynamic_cast` at all,
## and route their fatal-error paths through `YS_THROW`, which falls back to
## `abort()` when exceptions are disabled. So CIRCT's own `-fno-exceptions
## -fno-rtti` are left alone.
##
##===----------------------------------------------------------------------===//

# The Yosys headers reject anything older than C++20. A target that links
# `Yosys::libyosys` gets that from its `cxx_std_20` compile feature, but the
# `obj.*` object library `llvm_add_library()` may create does not -- it inherits
# only `INCLUDE_DIRECTORIES` (see `AddLLVM.cmake`) -- and it is what actually
# compiles the sources. `CMAKE_CXX_STANDARD` is what initializes its
# `CXX_STANDARD` property.
set(CMAKE_CXX_STANDARD 20)

# Fix an issue with CMake's precompiled headers mechanism. Headers would be
# precompiled for CIRCT's language setting (C++17), which then breaks here where
# targets are lifted to C++20.
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
