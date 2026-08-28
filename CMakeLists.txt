# IOv2 -- CMake entry point.
#
# The library itself is header-only and needs no build system; the top-level
# Makefile still owns the optional shared-library packaging (make shared /
# install / install-shared). This file exists for the test suite, which is being
# moved from test/Makefile to CMake/CTest.
#
# Both build systems are kept and run side by side for a release, so their
# results, coverage and timings can be compared before the Makefile is removed.
# Until that comparison is done, nothing here may change what the tests do.

cmake_minimum_required(VERSION 3.25)

project(IOv2
    VERSION 0.1.0
    DESCRIPTION "C++23 I/O and localization library"
    HOMEPAGE_URL "https://github.com/liwei-cpp/IOv2"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR
      "In-source builds are not supported: they would recreate the obj_*/bin_* clutter "
      "in the source tree that moving to CMake is meant to remove. Configure into a "
      "separate directory, e.g. cmake -S . -B build/release-gcc")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Compile commands are how the installed-* consumer modes are audited for
# source-tree header leakage, so they are always generated.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Must precede include(CTest): that is what writes these into
# DartConfiguration.tcl, where `ctest -T memcheck` reads them. The three options
# are the ones test/Makefile's valgrind target already passes.
#
# These apply to every test. The one option that must not is --fair-sched=yes,
# which only the concur suite needs; test/CMakeLists.txt passes it through
# VALGRIND_OPTS on that test alone.
find_program(MEMORYCHECK_COMMAND valgrind)
set(MEMORYCHECK_COMMAND_OPTIONS
    "--error-exitcode=1 --leak-check=full --max-stackframe=6000000")

include(CTest)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
