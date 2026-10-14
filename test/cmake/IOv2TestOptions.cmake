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
  target_compile_options(iov2_test_options INTERFACE
      /std:c++latest /permissive- /Zc:__cplusplus /utf-8 /W4 /WX)
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
  else()
    target_compile_options(iov2_test_options INTERFACE -g -O0)
  endif()
elseif(IOV2_TEST_MODE STREQUAL "sanitizer")
  # No NDEBUG here, matching the Makefile.
  if(MSVC)
    # The Windows MSVC ABI supports AddressSanitizer but not UBSan.
    target_compile_options(iov2_test_options INTERFACE /O1 /Z7 /fsanitize=address)
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
# Only the include directory and the link library are taken from pkg-config.
# `pkg-config --cflags botan-2` also reports -fstack-protector -m64 -pthread,
# which test/Makefile does not pass; -fstack-protector in particular changes code
# generation, so adopting it would make the two builds incomparable. Dropping
# CFLAGS_OTHER is what keeps this an equivalence migration. The target always
# exists so suite construction stays uniform, but dependency-free component
# selections leave it empty and perform no discovery.
add_library(iov2_test_thirdparty INTERFACE)
if(IOV2_TEST_NEEDS_OPTIONAL_DEPS)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(BOTAN REQUIRED botan-2)
  find_package(ZLIB REQUIRED)

  # Plain INTERFACE, not SYSTEM: -isystem would suppress warnings from these
  # headers, and with -Werror that is the difference between a build the
  # Makefile fails and one CMake lets through.
  target_include_directories(iov2_test_thirdparty INTERFACE ${BOTAN_INCLUDE_DIRS})
  target_link_libraries(iov2_test_thirdparty INTERFACE ${BOTAN_LIBRARIES} ZLIB::ZLIB)
endif()
