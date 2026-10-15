# Cross-compiles the tests from Linux for x86_64 Windows (the MSVC ABI) with
# clang-cl and lld-link, and runs them under Wine through
# CMAKE_CROSSCOMPILING_EMULATOR. A local stand-in for a Windows machine: it
# catches what does not compile against MSVC's STL and CRT, and most of what
# behaves differently there; the Windows CI jobs remain the authority.
#
# The MSVC CRT/STL and the Windows SDK come from xwin, splatted once with
#
#   xwin --accept-license --arch x86_64 splat --include-debug-libs \
#        --use-winsysroot-style --output ~/.cache/xwin
#
# XWIN_DIR overrides that location. Needs clang-cl, lld-link, llvm-lib, llvm-rc,
# llvm-mt (Fedora: clang, lld, llvm) and wine.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# C as well: GoogleTest's project() enables it, and would otherwise pick up the
# host's cc.
set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)

if(DEFINED ENV{XWIN_DIR})
  set(_iov2_xwin "$ENV{XWIN_DIR}")
else()
  set(_iov2_xwin "$ENV{HOME}/.cache/xwin")
endif()
if(NOT EXISTS "${_iov2_xwin}/VC/Tools/MSVC")
  message(FATAL_ERROR
      "No xwin sysroot at '${_iov2_xwin}'; splat one (see the top of this file) "
      "or point XWIN_DIR at it")
endif()

# clang-cl finds headers through /winsysroot. lld-link would find libraries the
# same way, but it looks for lib/x64 where xwin writes lib/x86_64, so the three
# library directories are named explicitly.
file(GLOB _iov2_msvc_dirs LIST_DIRECTORIES true "${_iov2_xwin}/VC/Tools/MSVC/*")
list(GET _iov2_msvc_dirs 0 _iov2_msvc)
file(GLOB _iov2_sdk_dirs LIST_DIRECTORIES true "${_iov2_xwin}/Windows Kits/10/Lib/*")
list(GET _iov2_sdk_dirs 0 _iov2_sdk)

set(CMAKE_C_FLAGS_INIT "/winsysroot \"${_iov2_xwin}\"")
set(CMAKE_CXX_FLAGS_INIT "/winsysroot \"${_iov2_xwin}\"")
set(CMAKE_RC_FLAGS_INIT "/I \"${_iov2_xwin}/Windows Kits/10/Include\"")
foreach(_kind IN ITEMS EXE SHARED MODULE)
  set(CMAKE_${_kind}_LINKER_FLAGS_INIT
      "/libpath:\"${_iov2_msvc}/lib/x86_64\" /libpath:\"${_iov2_sdk}/ucrt/x86_64\" /libpath:\"${_iov2_sdk}/um/x86_64\"")
endforeach()

# The static CRT, so that the CRT under test is Microsoft's own code. With /MD
# the program would load Wine's reimplementation of ucrtbase.dll instead, and
# the two differ (measured: a 1-byte _read of CRLF from a pipe).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# IOV2_TEST_UNDER_WINE lets a test skip what only Wine gets wrong (Wine passes
# the host environment through); real Windows never sets it.
set(CMAKE_CROSSCOMPILING_EMULATOR "env;WINEDEBUG=-all;IOV2_TEST_UNDER_WINE=1;wine")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
