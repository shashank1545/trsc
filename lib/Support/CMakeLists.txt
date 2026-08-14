if(NOT TRSC_GLIBC_COMPAT)
  return()
endif()

# OBJECT, never STATIC. The definitions in here are what satisfy references
# coming out of the LLVM and MLIR archives, and an archive member is only
# pulled in when something undefined already refers to it. As a static library
# this would be skipped depending on link order and the pinning would silently
# stop happening. An object library hands its objects to the link directly.
add_library(TrscGlibcSymbolVersions OBJECT GlibcSymbolVersions.c)

# -fno-builtin: this file defines names the compiler knows as builtins.
# -w: redeclaring them is the entire point, so the warnings are noise.
target_compile_options(TrscGlibcSymbolVersions PRIVATE -fno-builtin -w)
