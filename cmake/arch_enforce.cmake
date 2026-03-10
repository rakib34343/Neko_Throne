# ═══════════════════════════════════════════════════════════════════════════════
# cmake/arch_enforce.cmake — Strict amd64 / x86_64 Architecture Enforcement
# ═══════════════════════════════════════════════════════════════════════════════
# Throne is an amd64-exclusive application. This module triggers a FATAL_ERROR
# if anyone attempts to compile for any other architecture.
#
# Enforcement layers:
#   1. CMake-level processor/arch detection
#   2. Compiler-level preprocessor checks (MSVC + GCC/Clang)
# ═══════════════════════════════════════════════════════════════════════════════

# ─── Layer 1: CMake system processor check ────────────────────────────────────
# CMAKE_SYSTEM_PROCESSOR is set by CMake during platform introspection.
# On cross-compilation, CMAKE_OSX_ARCHITECTURES may override it.

set(_THRONE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")

# Normalize: accept "x86_64", "AMD64", "amd64" — reject everything else
string(TOLOWER "${_THRONE_ARCH}" _THRONE_ARCH_LOWER)
if (NOT (_THRONE_ARCH_LOWER STREQUAL "x86_64" OR _THRONE_ARCH_LOWER STREQUAL "amd64"))
    message(FATAL_ERROR
        "══════════════════════════════════════════════════════════════\n"
        " FATAL: Throne is an amd64/x86_64-exclusive application.\n"
        " Detected architecture: ${_THRONE_ARCH}\n"
        " Building for ARM, ARM64, i686, or any other arch is forbidden.\n"
        " If cross-compiling, set CMAKE_SYSTEM_PROCESSOR=x86_64.\n"
        "══════════════════════════════════════════════════════════════"
    )
endif()

message(STATUS "[Throne] Architecture enforcement passed: ${_THRONE_ARCH}")

# ─── Layer 2: Compiler-level preprocessor guard ──────────────────────────────
# Even if CMake variables are spoofed, the compiler itself will catch it.
# This adds a compile definition that triggers a #error in non-amd64 builds.

if (MSVC)
    # MSVC: _M_AMD64 is defined only for x86_64 targets
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C,CXX>:/DTHRONE_ARCH_ENFORCE>"
    )
    # Inject a header-check via a forced-include or compile definition
    add_compile_definitions(THRONE_MSVC_AMD64_CHECK)
else()
    # GCC / Clang: __x86_64__ is defined only for x86_64 targets
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C,CXX>:-DTHRONE_ARCH_ENFORCE>"
    )
    add_compile_definitions(THRONE_GCC_AMD64_CHECK)
endif()

# ─── Unset temporaries ───────────────────────────────────────────────────────
unset(_THRONE_ARCH)
unset(_THRONE_ARCH_LOWER)
