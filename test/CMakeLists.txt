# IOv2 -- the test suite: one per-feature suite and CTest test per source-holding
# directory.
#
# The library itself is header-only and needs no build system; the top-level
# Makefile owns the optional shared-library packaging (make shared / install /
# install-shared) and nothing else. This project builds only the tests, which is
# why it is rooted here rather than at the repository root: configure it from
# this directory. Its build trees still land in the repository's build/; see
# CMakePresets.json.
#
# test/Makefile linked everything into seven executables, one per top-level
# directory, each driven by a hand-written main() that called its tests in a
# fixed order. That was too coarse to work with: a single failure meant
# re-running hundreds of unrelated tests, and there was no way to ask for just
# the monetary facet or just ostream::seek.
#
# The suites here are those same tests, regrouped by the directory that holds
# them. Every suite is GoogleTest now, so the cases register themselves and
# there is no order left to preserve; the partition is simply "one suite per
# directory that directly holds a .cpp", computed below rather than recorded in
# a manifest.

cmake_minimum_required(VERSION 3.25)

project(IOv2
    VERSION 1.0.0
    DESCRIPTION "C++23 I/O and localization library"
    HOMEPAGE_URL "https://github.com/liwei-cpp/IOv2"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR
      "In-source builds are not supported: they would recreate the obj_*/bin_* clutter "
      "in the source tree that moving to CMake removed. Configure into a separate "
      "directory, e.g. cmake --preset gcc-release")
endif()

# The repository, of which this project is the test/ directory: the library's
# include/ is found from here. Normalized, so compile commands carry no "/..".
cmake_path(GET PROJECT_SOURCE_DIR PARENT_PATH IOV2_REPO_ROOT)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Compile commands are how the installed-* consumer modes are audited for
# source-tree header leakage, so they are always generated.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Must precede include(CTest): that is what writes these into
# DartConfiguration.tcl, where `ctest -T memcheck` reads them. The three options
# are the ones test/Makefile's valgrind target passed.
#
# These apply to every test. The one option that must not is --fair-sched=yes,
# which only the concur suite needs; it is passed through VALGRIND_OPTS on that
# test alone, below.
find_program(MEMORYCHECK_COMMAND valgrind)
set(MEMORYCHECK_COMMAND_OPTIONS
    "--error-exitcode=1 --leak-check=full --max-stackframe=6000000")

include(CTest)

# Select top-level test components at configure time, not merely at CTest run
# time. This lets a platform bring up (say) device without first making every
# unrelated platform backend and optional dependency buildable.
set(IOV2_TEST_KNOWN_COMPONENTS common concur cvt device facet io locale)
set(IOV2_TEST_COMPONENTS "all" CACHE STRING
    "Semicolon-separated top-level test components to configure, or 'all'")
set_property(CACHE IOV2_TEST_COMPONENTS PROPERTY STRINGS
    all ${IOV2_TEST_KNOWN_COMPONENTS})

if(IOV2_TEST_COMPONENTS STREQUAL "all")
  set(IOV2_TEST_SELECTED_COMPONENTS ${IOV2_TEST_KNOWN_COMPONENTS})
else()
  set(IOV2_TEST_SELECTED_COMPONENTS "")
  foreach(component IN LISTS IOV2_TEST_COMPONENTS)
    if(component STREQUAL "all")
      message(FATAL_ERROR
          "IOV2_TEST_COMPONENTS may be 'all' or a component list, but not both")
    endif()
    if(NOT component IN_LIST IOV2_TEST_KNOWN_COMPONENTS)
      list(JOIN IOV2_TEST_KNOWN_COMPONENTS ", " known_components)
      message(FATAL_ERROR
          "Unknown IOV2_TEST_COMPONENTS entry '${component}'. Known components: ${known_components}")
    endif()
    list(APPEND IOV2_TEST_SELECTED_COMPONENTS "${component}")
  endforeach()
  list(REMOVE_DUPLICATES IOV2_TEST_SELECTED_COMPONENTS)
endif()

if(NOT IOV2_TEST_SELECTED_COMPONENTS)
  message(FATAL_ERROR "IOV2_TEST_COMPONENTS must select at least one component")
endif()

# Today only cvt and io suites exercise the optional zlib/Botan converters.
set(IOV2_TEST_NEEDS_OPTIONAL_DEPS OFF)
if("cvt" IN_LIST IOV2_TEST_SELECTED_COMPONENTS OR
   "io" IN_LIST IOV2_TEST_SELECTED_COMPONENTS)
  set(IOV2_TEST_NEEDS_OPTIONAL_DEPS ON)
endif()

include(IOv2TestOptions)
include(IOv2GoogleTest)

# One suite per directory that directly holds a source. Deriving it here is what
# makes a new directory a new suite with no other edit, and makes it impossible
# for a source to belong to no suite -- the two ways the split used to lose
# tests silently while staying green.
file(GLOB_RECURSE IOV2_TEST_ALL_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
# A plugins/ directory holds modules a suite dlopens, not tests of its own, and
# cmake/ holds the main() every suite links.
list(FILTER IOV2_TEST_ALL_SOURCES EXCLUDE REGEX "/plugins/")
set(IOV2_TEST_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gtest_main.cpp")
list(REMOVE_ITEM IOV2_TEST_ALL_SOURCES "${IOV2_TEST_MAIN}")
if(NOT IOV2_TEST_ALL_SOURCES)
  message(FATAL_ERROR "No test sources found under ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Tests of what only some platforms have sit in a subdirectory named for them:
# posix/ (signals, O_NONBLOCK, rlimits), linux/ (/dev/full, GNU ld's --wrap),
# windows/ (text-mode CRLF and Ctrl+Z). Each is built on its platform alone, so
# test sources need no #ifdef; support/platform.h covers the API differences.
# Matched on the path below this directory, never on where the checkout lives.
set(IOV2_TEST_OTHER_PLATFORM_DIRS "")
if(NOT UNIX)
  list(APPEND IOV2_TEST_OTHER_PLATFORM_DIRS posix)
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND IOV2_TEST_OTHER_PLATFORM_DIRS linux)
endif()
if(NOT WIN32)
  list(APPEND IOV2_TEST_OTHER_PLATFORM_DIRS windows)
endif()
list(JOIN IOV2_TEST_OTHER_PLATFORM_DIRS "|" _other_platforms)

set(IOV2_TEST_SELECTED_SOURCES "")
foreach(source IN LISTS IOV2_TEST_ALL_SOURCES)
  file(RELATIVE_PATH source_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
  string(REGEX REPLACE "/.*$" "" source_component "${source_rel}")
  if(NOT source_component IN_LIST IOV2_TEST_SELECTED_COMPONENTS)
    continue()
  endif()
  if(_other_platforms AND source_rel MATCHES "/(${_other_platforms})/")
    continue()
  endif()
  list(APPEND IOV2_TEST_SELECTED_SOURCES "${source}")
endforeach()

if(NOT IOV2_TEST_SELECTED_SOURCES)
  message(FATAL_ERROR
      "IOV2_TEST_COMPONENTS='${IOV2_TEST_COMPONENTS}' selected no test sources")
endif()

set(IOV2_TEST_SUITE_DIRS "")
foreach(source IN LISTS IOV2_TEST_SELECTED_SOURCES)
  get_filename_component(source_dir "${source}" DIRECTORY)
  file(RELATIVE_PATH source_dir "${CMAKE_CURRENT_SOURCE_DIR}" "${source_dir}")
  list(APPEND IOV2_TEST_SUITE_DIRS "${source_dir}")
endforeach()
list(REMOVE_DUPLICATES IOV2_TEST_SUITE_DIRS)
list(SORT IOV2_TEST_SUITE_DIRS)

set(IOV2_TEST_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/IOv2TestResources"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

foreach(suite_dir IN LISTS IOV2_TEST_SUITE_DIRS)
  string(REPLACE "/" "_" suite_slug "${suite_dir}")
  string(REPLACE "/" "." IOV2_SUITE_NAME "${suite_dir}")
  set(target "test_${suite_slug}")

  # Not GLOB_RECURSE: a nested directory is a suite of its own, and compiling it
  # into its parent as well would run those tests twice.
  file(GLOB suite_sources CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/${suite_dir}/*.cpp")

  add_executable(${target} "${IOV2_TEST_MAIN}" ${suite_sources})

  target_link_libraries(${target} PRIVATE
      iov2_test_subject iov2_test_options iov2_test_thirdparty iov2_test_gtest)

  # -I../include then -I., in that order, exactly as the Makefile passes them.
  # The second is what makes <support/file_guard.h> and friends resolve.
  target_include_directories(${target} PRIVATE
      ${IOV2_TEST_SUBJECT_INCLUDE_DIRS}
      "${CMAKE_CURRENT_SOURCE_DIR}")

  set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${IOV2_TEST_BIN_DIR}")

  # A private working directory per suite. Under the Makefile the seven binaries
  # run one after another from test/, so the temporary files and stdio
  # redirection that at least 38 sources set up cannot collide. CTest may run
  # them concurrently, so the isolation has to be explicit.
  set(suite_work_dir "${CMAKE_CURRENT_BINARY_DIR}/work/${suite_slug}")
  file(MAKE_DIRECTORY "${suite_work_dir}")

  add_test(NAME ${IOV2_SUITE_NAME} COMMAND ${target})

  # The module label keeps the old per-executable grouping addressable: `-L io`
  # still selects everything test_io used to run.
  string(REGEX REPLACE "/.*" "" suite_module "${suite_dir}")
  set(suite_labels "all;${suite_module}")
  set(suite_environment "")
  if(suite_module STREQUAL "concur")
    list(APPEND suite_labels "thread")
    # Valgrind runs one thread at a time and by default hands the turn over only
    # every ~100k basic blocks, so a thread spinning on another one burns whole
    # quanta doing nothing. This is the only suite with threads to starve, and
    # measurably so: in one CI run it took 42s under gcc and 862s under clang
    # against a 0.16s uninstrumented run, where every other suite lands near 30x.
    #
    # VALGRIND_OPTS is the per-test hook. MEMORYCHECK_COMMAND_OPTIONS applies to
    # the whole run, but valgrind reads this variable itself, and CTest can set it
    # on one test.
    set(suite_environment "VALGRIND_OPTS=--fair-sched=yes")
  endif()

  set_tests_properties(${IOV2_SUITE_NAME} PROPERTIES
      LABELS "${suite_labels}"
      WORKING_DIRECTORY "${suite_work_dir}")
  if(suite_environment)
    set_tests_properties(${IOV2_SUITE_NAME} PROPERTIES
        ENVIRONMENT "${suite_environment}")
  endif()
endforeach()

# One suite injects a failure at link time with the GNU/ELF linker's --wrap: fd 0
# closed between read() and poll(). Elsewhere the macro stays undefined and the
# tests skip themselves.
if(UNIX AND NOT APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  if(TARGET test_device_std_device_posix_poll_error)
    target_link_options(test_device_std_device_posix_poll_error PRIVATE "LINKER:--wrap=read")
    target_compile_definitions(test_device_std_device_posix_poll_error PRIVATE IOV2_TEST_WRAP_READ=1)
  endif()
endif()

# The plugin is unloadable only if g++ does not make its inline variables
# STB_GNU_UNIQUE; the test asserts it is still resident after dlclose.
if(IOV2_TEST_CONSUMER STREQUAL "installed-shared" AND TARGET test_io_objects_plugin_unload)
  add_library(test_plugin_cout_number MODULE
      "${CMAKE_CURRENT_SOURCE_DIR}/io/objects/plugin_unload/plugins/test_plugin_cout_number.cpp")
  target_link_libraries(test_plugin_cout_number PRIVATE iov2_test_subject iov2_test_options)
  target_include_directories(test_plugin_cout_number PRIVATE ${IOV2_TEST_SUBJECT_INCLUDE_DIRS})
  target_compile_options(test_plugin_cout_number PRIVATE
      $<$<CXX_COMPILER_ID:GNU>:-fno-gnu-unique>)
  set_target_properties(test_plugin_cout_number PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY "${IOV2_TEST_BIN_DIR}")
  add_dependencies(test_io_objects_plugin_unload test_plugin_cout_number)
  target_link_libraries(test_io_objects_plugin_unload PRIVATE ${CMAKE_DL_LIBS})
  target_compile_definitions(test_io_objects_plugin_unload PRIVATE
      IOV2_TEST_UNLOAD_PLUGIN="$<TARGET_FILE:test_plugin_cout_number>")
endif()

list(LENGTH IOV2_TEST_SELECTED_SOURCES IOV2_TEST_SOURCE_TOTAL)
list(LENGTH IOV2_TEST_SUITE_DIRS IOV2_TEST_SUITE_COUNT)
message(STATUS
    "IOv2 tests: ${IOV2_TEST_SOURCE_TOTAL} sources in "
    "${IOV2_TEST_SUITE_COUNT} suites (mode=${IOV2_TEST_MODE}, "
    "consumer=${IOV2_TEST_CONSUMER})")
