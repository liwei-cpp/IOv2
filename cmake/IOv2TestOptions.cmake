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

# -std=c++23, not -std=gnu++23. target_compile_features(cxx_std_23) would default
# to extensions on and emit the GNU dialect, which the Makefile does not use.
set_target_properties(iov2_test_options PROPERTIES INTERFACE_COMPILE_FEATURES "")
target_compile_options(iov2_test_options INTERFACE -std=c++23)

# POSIX.1-2008: locale_t, newlocale, uselocale and the *_l ctype helpers. The
# headers do not compile without it. Applied to every mode, as CPPFLAGS is in
# the Makefile.
target_compile_definitions(iov2_test_options INTERFACE _POSIX_C_SOURCE=200809L)

target_compile_options(iov2_test_options INTERFACE -Wall -Werror -Wshadow)

# Per-mode flags, transcribed from test/Makefile. Sanitizer flags must reach the
# link line as well as the compile line, hence the matching target_link_options.
if(IOV2_TEST_MODE STREQUAL "release")
  target_compile_options(iov2_test_options INTERFACE -O3 -g)
  target_compile_definitions(iov2_test_options INTERFACE NDEBUG)
elseif(IOV2_TEST_MODE STREQUAL "debug")
  target_compile_options(iov2_test_options INTERFACE -g -O0)
elseif(IOV2_TEST_MODE STREQUAL "sanitizer")
  # No NDEBUG here, matching the Makefile.
  target_compile_options(iov2_test_options INTERFACE
      -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(iov2_test_options INTERFACE -fsanitize=address,undefined)
elseif(IOV2_TEST_MODE STREQUAL "tsan")
  target_compile_options(iov2_test_options INTERFACE
      -g -O1 -fsanitize=thread -fno-omit-frame-pointer)
  target_link_options(iov2_test_options INTERFACE -fsanitize=thread)
elseif(IOV2_TEST_MODE STREQUAL "coverage")
  target_compile_options(iov2_test_options INTERFACE -g -O0 --coverage)
  target_link_options(iov2_test_options INTERFACE --coverage)
endif()

# ------------------------------------------------- library under test
add_library(iov2_test_subject INTERFACE)

# Exported rather than attached to the target because the order matters: the
# Makefile passes -I../include before -I., so a header present in both trees
# resolves to the library's copy. CMake would otherwise put a target's own
# directories ahead of any it inherits through linking, silently reversing that.
# No header collides today, but include/ and test/ share six directory names, so
# the first one added would diverge between the two build systems.
set(IOV2_TEST_SUBJECT_INCLUDE_DIRS "")

if(IOV2_TEST_CONSUMER STREQUAL "source")
  set(IOV2_TEST_SUBJECT_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include")
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
  # ci.yml checks the generated compile commands for the same thing.
  foreach(dir IN LISTS IOV2_INSTALLED_INCLUDE_DIRS)
    cmake_path(IS_PREFIX PROJECT_SOURCE_DIR "${dir}" NORMALIZE _inside_repo)
    if(_inside_repo)
      message(FATAL_ERROR
          "pkg-config ${_iov2_pc} resolved to '${dir}', inside the source tree. "
          "Consumer '${IOV2_TEST_CONSUMER}' must use installed headers only.")
    endif()
  endforeach()
endif()

# ---------------------------------------------------- third-party libraries
#
# Only the include directory and the link library are taken from pkg-config.
# `pkg-config --cflags botan-2` also reports -fstack-protector -m64 -pthread,
# which test/Makefile does not pass; -fstack-protector in particular changes code
# generation, so adopting it would make the two builds incomparable. Dropping
# CFLAGS_OTHER is what keeps this an equivalence migration.
find_package(PkgConfig REQUIRED)
pkg_check_modules(BOTAN REQUIRED botan-2)
find_package(ZLIB REQUIRED)

# Plain INTERFACE, not SYSTEM: -isystem would suppress warnings from these
# headers, and with -Werror that is the difference between a build the Makefile
# fails and one CMake lets through.
add_library(iov2_test_thirdparty INTERFACE)
target_include_directories(iov2_test_thirdparty INTERFACE ${BOTAN_INCLUDE_DIRS})
target_link_libraries(iov2_test_thirdparty INTERFACE ${BOTAN_LIBRARIES} ZLIB::ZLIB)
