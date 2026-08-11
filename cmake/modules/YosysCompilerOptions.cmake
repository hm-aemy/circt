##===- YosysCompilerOptions.cmake ------------------------------*- cmake -*-===//
##
## Compiler settings for CIRCT libraries that include the Yosys headers.
## Include this module at the top of such a library's CMakeLists.txt, the same
## way `SlangCompilerOptions.cmake` is used for the slang frontend. It has to
## come *before* `add_circt_library()`, because it works through
## directory-scoped variables that the LLVM helpers read while creating the
## target.
##
## The Yosys headers used to demand `-D_YOSYS_`, exceptions, and RTTI on top of
## C++20; they now self-configure from the installed `kernel/yosys_config.h` and
## contain no `dynamic_cast` at all.
##
## Note that `YS_THROW` (`kernel/hashlib.h`) is *not* how Yosys reports ordinary
## errors: it covers hashlib's internal assertions only, and falls back to
## `abort()` when exceptions are off. `log_error()` ends in `_Exit(1)` and is not
## catchable at all. The one recoverable path is `log_cmd_error`, which throws
## `log_cmd_error_exception` when `log_cmd_error_throw` is set -- catching that
## is the only way to turn a bad Yosys script into a diagnostic instead of a
## process exit, and it needs exceptions in the catching translation unit.
##
## That is deliberately *not* arranged here. `LLVM_REQUIRES_EH` force-enables
## RTTI as well (`AddLLVM.cmake`, "Exception handling requires RTTI"), and a
## translation unit compiled with RTTI that derives from a class built without
## it -- `mlir::Pass`, `llvm::cl::opt` -- fails to link on missing typeinfo. The
## one file that catches gets `-fexceptions` on its own, via
## `set_source_files_properties()`; see `lib/Dialect/RTLIL/Transforms`.
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
