# Compile/link options and the library-under-test, as interface targets.
#
# This file exists to reproduce test/Makefile exactly, not to be idiomatic CMake.
# The two build systems run side by side for a release so their results can be
# compared, and a comparison is only meaningful if the compiler sees the same
# flags. Every deviation from the Makefile below is deliberate and commented.

include_guard(GLOBAL)

# --------------------------------------------------------------- build mode
#
# Mirrors test/Makefile's MODE rather than CMAKE_BUILD_TYPE. The Makefile's modes
# are not build types: `sanitizer` and `tsan` are -O1 -g with NDEBUG *absent*,
# which no standard CMake configuration produces. Naming them after the Makefile
# keeps the mapping one-to-one and reviewable; CMAKE_BUILD_TYPE is forced empty
# below so CMake contributes no optimization or debug flags of its own.
set(IOV2_TEST_MODE "release" CACHE STRING
    "Build mode, mirroring test/Makefile MODE")
set_property(CACHE IOV2_TEST_MODE PROPERTY STRINGS
    release debug sanitizer tsan coverage)

if(NOT IOV2_TEST_MODE MATCHES "^(release|debug|sanitizer|tsan|coverage)$")
  message(FATAL_ERROR "IOV2_TEST_MODE must be one of: release debug sanitizer tsan coverage")
endif()

if(CMAKE_BUILD_TYPE)
  message(WARNING
      "CMAKE_BUILD_TYPE='${CMAKE_BUILD_TYPE}' is ignored; IOv2's test build is driven by "
      "IOV2_TEST_MODE (currently '${IOV2_TEST_MODE}'). Clearing it so the per-config flags "
      "CMake would otherwise append cannot diverge from test/Makefile.")
endif()
set(CMAKE_BUILD_TYPE "" CACHE STRING "" FORCE)

# --------------------------------------------------------- consumer of IOv2
#
# Which copy of the library the tests compile against. ci.yml exercises all
# three: the repo headers, and both installed layouts reached through pkg-config.
set(IOV2_TEST_CONSUMER "source" CACHE STRING
    "Where the library under test comes from")
set_property(CACHE IOV2_TEST_CONSUMER PROPERTY STRINGS
    source installed-header-only installed-shared)

# ------------------------------------------------------------------ options
add_library(iov2_test_options INTERFACE)

# Keep the compiler-specific spelling explicit. GCC/Clang need -std=c++23 rather
# than the GNU dialect; current MSVC and clang-cl expose C++23 through
# /std:c++latest.
set_target_properties(iov2_test_options PROPERTIES INTERFACE_COMPILE_FEATURES "")
if(MSVC)
  # /Zc:u8EscapeEncoding: by default cl encodes \xFF in a u8"" literal as the
  # UTF-8 of U+00FF, two bytes, where the standard wants the one code unit. Tests
  # spell raw bytes that way. clang-cl already conforms and rejects the flag.
  target_compile_options(iov2_test_options INTERFACE
      /std:c++latest /permissive- /Zc:__cplusplus /utf-8 /W4 /WX
      $<$<CXX_COMPILER_ID:MSVC>:/Zc:u8EscapeEncoding>)
else()
  target_compile_options(iov2_test_options INTERFACE
      -std=c++23 -Wall -Werror -Wshadow)
endif()

# POSIX.1-2008: locale_t, newlocale, uselocale and the *_l ctype helpers. The
# POSIX implementation does not compile without it. It must not leak into the
# Windows CRT implementation.
if(NOT WIN32)
  target_compile_definitions(iov2_test_options INTERFACE _POSIX_C_SOURCE=200809L)
endif()

# Per-mode flags, transcribed from test/Makefile. Sanitizer flags must reach the
# link line as well as the compile line, hence the matching target_link_options.
if(IOV2_TEST_MODE STREQUAL "release")
  if(MSVC)
    target_compile_options(iov2_test_options INTERFACE /O2 /Z7)
  else()
    target_compile_options(iov2_test_options INTERFACE -O3 -g)
  endif()
  target_compile_definitions(iov2_test_options INTERFACE NDEBUG)
elseif(IOV2_TEST_MODE STREQUAL "debug")
  if(MSVC)
    target_compile_options(iov2_test_options INTERFACE /Od /Z7)
    if(NOT CMAKE_MSVC_RUNTIME_LIBRARY OR CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL$")
      set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebugDLL)
    else()
      set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebug)
    endif()
  else()
    target_compile_options(iov2_test_options INTERFACE -g -O0)
  endif()
elseif(IOV2_TEST_MODE STREQUAL "sanitizer")
  # No NDEBUG here, matching the Makefile.
  if(MSVC)
    # The Windows MSVC ABI supports AddressSanitizer but not UBSan.
    target_compile_options(iov2_test_options INTERFACE /O1 /Z7)
    # Directory-wide, unlike every other flag here, so that GoogleTest (included
    # after this file) is instrumented too: the MSVC STL stamps each object with
    # detect_mismatch("annotate_string"/"annotate_vector", "0" or "1") by whether
    # ASan is on, and an instrumented test linked against an uninstrumented
    # gtest.lib fails with LNK2038.
    add_compile_options(/fsanitize=address)
  else()
    target_compile_options(iov2_test_options INTERFACE
        -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(iov2_test_options INTERFACE -fsanitize=address,undefined)
  endif()
elseif(IOV2_TEST_MODE STREQUAL "tsan")
  if(MSVC)
    message(FATAL_ERROR "IOV2_TEST_MODE=tsan is not supported by the MSVC ABI")
  else()
    target_compile_options(iov2_test_options INTERFACE
        -g -O1 -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(iov2_test_options INTERFACE -fsanitize=thread)
  endif()
elseif(IOV2_TEST_MODE STREQUAL "coverage")
  if(MSVC)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      message(FATAL_ERROR
          "Windows coverage requires clang-cl so llvm-cov can export an LCOV report")
    endif()
    target_compile_options(iov2_test_options INTERFACE
        /Od /Z7 -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(iov2_test_options INTERFACE -fprofile-instr-generate)
  else()
    target_compile_options(iov2_test_options INTERFACE -g -O0 --coverage)
    target_link_options(iov2_test_options INTERFACE --coverage)
  endif()
endif()

if(IOV2_TEST_MODE MATCHES "^(debug|sanitizer)$" AND NOT MSVC)
  target_compile_definitions(iov2_test_options INTERFACE
      _GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)
endif()

# ------------------------------------------------- library under test
add_library(iov2_test_subject INTERFACE)

# Exported rather than attached to the target because the order matters: the
# Makefile passes -I../include before -I., so a header present in both trees
# resolves to the library's copy. CMake would otherwise put a target's own
# directories ahead of any it inherits through linking, silently reversing that.
# The order remains explicit so source-tree and installed consumers resolve the
# same public headers even if a future test support directory uses the IOv2 name.
set(IOV2_TEST_SUBJECT_INCLUDE_DIRS "")

if(IOV2_TEST_CONSUMER STREQUAL "source")
  set(IOV2_TEST_SUBJECT_INCLUDE_DIRS "${IOV2_REPO_ROOT}/include")
else()
  find_package(PkgConfig REQUIRED)
  if(IOV2_TEST_CONSUMER STREQUAL "installed-shared")
    set(_iov2_pc iov2-shared)
  else()
    set(_iov2_pc iov2)
  endif()
  pkg_check_modules(IOV2_INSTALLED REQUIRED IMPORTED_TARGET "${_iov2_pc}")
  target_link_libraries(iov2_test_subject INTERFACE PkgConfig::IOV2_INSTALLED)
  set(IOV2_TEST_SUBJECT_INCLUDE_DIRS ${IOV2_INSTALLED_INCLUDE_DIRS})

  # The whole point of these two modes is that the repo headers are NOT used, so
  # a stray source-tree path here would make the job silently test nothing new.
  # ci.yml checks the generated compile commands for the same thing. The whole
  # repository, not this project's test/: the headers to keep out are in include/.
  foreach(dir IN LISTS IOV2_INSTALLED_INCLUDE_DIRS)
    cmake_path(IS_PREFIX IOV2_REPO_ROOT "${dir}" NORMALIZE _inside_repo)
    if(_inside_repo)
      message(FATAL_ERROR
          "pkg-config ${_iov2_pc} resolved to '${dir}', inside the source tree. "
          "Consumer '${IOV2_TEST_CONSUMER}' must use installed headers only.")
    endif()
  endforeach()
endif()

# --------------------------------------------- optional third-party libraries
#
# Botan 3 installs a CMake package of its own, and so do the distribution
# packages and vcpkg's port, so it is found the same way everywhere; Windows has
# no pkg-config to lean on. The package names its target Botan::Botan when there
# is a shared library and Botan::Botan-static when there is only a static one.
# It also adds -fstack-protector -m64 -pthread to the link, which is harmless:
# at link time they generate no code.
#
# Required whatever the selection: every platform the tests run on has both
# (Windows through vcpkg, see vcpkg.json; macOS through Homebrew), and one rule
# is simpler than tracking which suites use them.
find_package(Botan 3 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

if(TARGET Botan::Botan)
  set(_iov2_botan Botan::Botan)
else()
  set(_iov2_botan Botan::Botan-static)
endif()
# Not SYSTEM, which an imported target is by default: -isystem would suppress
# warnings from Botan's headers, and with -Werror those should fail the build.
# The property goes on the target behind the alias.
get_target_property(_iov2_botan_real ${_iov2_botan} ALIASED_TARGET)
if(NOT _iov2_botan_real)
  set(_iov2_botan_real ${_iov2_botan})
endif()
set_target_properties(${_iov2_botan_real} PROPERTIES SYSTEM FALSE)

# vcpkg builds a debug Botan as well, but Botan's package names a single library
# for every configuration, and this build has no configuration anyway (the debug
# CRT comes from IOV2_TEST_MODE, above). So the debug mode would link the release
# DLL, whose STL has another layout: the first std::string to cross the DLL
# boundary crashes (hash_cvt's hex_encode). Swap in vcpkg's debug copies, which
# have the same file names.
if(MSVC AND IOV2_TEST_MODE STREQUAL "debug" AND DEFINED VCPKG_INSTALLED_DIR)
  set(_iov2_vcpkg_debug "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug")
  foreach(_iov2_prop IN ITEMS IMPORTED_LOCATION IMPORTED_IMPLIB
                              IMPORTED_LOCATION_NOCONFIG IMPORTED_IMPLIB_NOCONFIG)
    get_target_property(_iov2_release_file ${_iov2_botan_real} ${_iov2_prop})
    if(NOT _iov2_release_file OR NOT EXISTS "${_iov2_release_file}")
      continue()
    endif()
    cmake_path(GET _iov2_release_file FILENAME _iov2_name)
    cmake_path(GET _iov2_release_file PARENT_PATH _iov2_dir)
    cmake_path(GET _iov2_dir FILENAME _iov2_subdir)       # bin or lib
    set(_iov2_debug_file "${_iov2_vcpkg_debug}/${_iov2_subdir}/${_iov2_name}")
    if(NOT EXISTS "${_iov2_debug_file}")
      message(FATAL_ERROR "No debug Botan at ${_iov2_debug_file}, for ${_iov2_release_file}")
    endif()
    set_target_properties(${_iov2_botan_real} PROPERTIES ${_iov2_prop} "${_iov2_debug_file}")
  endforeach()

  # FindZLIB does know both of vcpkg's zlibs, as the RELEASE and DEBUG
  # configurations of ZLIB::ZLIB, but with no configuration CMake takes RELEASE,
  # zlib1.dll, absent from debug/bin (the debug one is zlibd1.dll). Windows then
  # stops each such test on a dialog that nobody dismisses.
  get_target_property(_iov2_zlib_configs ZLIB::ZLIB IMPORTED_CONFIGURATIONS)
  if(NOT "DEBUG" IN_LIST _iov2_zlib_configs)
    message(FATAL_ERROR "ZLIB::ZLIB has no DEBUG configuration (only '${_iov2_zlib_configs}')")
  endif()
  set_target_properties(ZLIB::ZLIB PROPERTIES MAP_IMPORTED_CONFIG_NOCONFIG DEBUG)
endif()

# The DLLs the tests load, for CTest to put first on PATH. vcpkg's own copying
# next to each executable is turned off in the presets: it picks bin/ or
# debug/bin/ by configuration, which this build lacks, and the debug and
# release Botan DLLs share a name, so the debug mode got the release one.
if(DEFINED VCPKG_INSTALLED_DIR)
  if(MSVC AND IOV2_TEST_MODE STREQUAL "debug")
    set(_iov2_dll_dir "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
  else()
    set(_iov2_dll_dir "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
  endif()
  cmake_path(NATIVE_PATH _iov2_dll_dir IOV2_TEST_DLL_DIR)
endif()

add_library(iov2_test_thirdparty INTERFACE)
target_link_libraries(iov2_test_thirdparty INTERFACE ${_iov2_botan} ZLIB::ZLIB)
