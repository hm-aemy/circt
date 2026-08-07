##===- YosysCompilerOptions.cmake ------------------------------*- cmake -*-===//
##
## Compiler settings for CIRCT libraries that include the Yosys headers.
## Include this module at the top of such a library's CMakeLists.txt, the same
## way `SlangCompilerOptions.cmake` is used for the slang frontend. It has to
## come *before* `add_circt_library()`, because it works through
## directory-scoped variables that the LLVM helpers read while creating the
## target.
##
## `Yosys::libyosys` carries the include directory, `_YOSYS_`, and `cxx_std_20`
## as usage requirements, so a target that links it needs none of them spelled
## out. What is left here is what a link cannot express -- exceptions and RTTI
## -- plus a repeat of the two requirements that `llvm_add_library()` drops on
## the way to its `obj.*` object libraries.
##
##===----------------------------------------------------------------------===//

# The Yosys headers reject anything older than C++20. The linked target gets
# that from `cxx_std_20` on `Yosys::libyosys`; `obj.*` does not (see below), and
# `CMAKE_CXX_STANDARD` is what initializes its `CXX_STANDARD` property.
set(CMAKE_CXX_STANDARD 20)

# Yosys throws (`log_error`, `hashlib`) and uses `dynamic_cast` in inline code,
# so exceptions and RTTI have to be enabled for the compilation units that
# include its headers. That takes two steps.
#
# First, stop `llvm_add_library()` (called by `add_circt_library()` and friends)
# from adding `-fno-exceptions`/`-fno-rtti` per target. These variables are read
# from the surrounding directory scope.
set(LLVM_REQUIRES_EH ON)
set(LLVM_REQUIRES_RTTI ON)

# Second, drop the same flags from `CMAKE_CXX_FLAGS` and enable exceptions and
# RTTI explicitly. This is necessary because on GCC/Clang the variables above
# merely suppress the disabling flags -- LLVM has no flags to enable EH/RTTI
# there -- while LLVM and CIRCT put `-fno-exceptions -fno-rtti` into the global
# `CMAKE_CXX_FLAGS`. Assigning to that variable here only affects this
# directory and its subdirectories.
foreach(flag -fno-exceptions -fno-rtti /EHs-c- /GR-)
  string(REPLACE "${flag}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endforeach()
if(MSVC)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHsc> $<$<COMPILE_LANGUAGE:CXX>:/GR>)
else()
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexceptions>
                      $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
endif()

# HACK: When `llvm_add_library()` creates an `obj.*` object library -- for an
# aggregated library, or whenever both SHARED and STATIC are built -- it
# forwards only `INCLUDE_DIRECTORIES` to it (`AddLLVM.cmake`), never the link
# interface. So the usage requirements of `Yosys::libyosys` reach the library
# target but not its object library, which then fails to compile the very same
# sources: `kernel/yosys_common.h` rejects both a missing `_YOSYS_` and a
# pre-C++20 standard. Restore the definition at directory scope, reading it off
# the target so the two cannot drift apart; `CMAKE_CXX_STANDARD` above covers
# the other half.
if(TARGET Yosys::libyosys)
  add_compile_definitions(
    $<TARGET_PROPERTY:Yosys::libyosys,INTERFACE_COMPILE_DEFINITIONS>)
endif()

# Fix an issue with CMake's precompiled headers mechanism. Headers would be
# precompiled for CIRCT's language setting (C++17), which then breaks here
# where targets are lifted to C++20.
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
