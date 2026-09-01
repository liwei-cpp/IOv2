# The test suite: 137 sources, 57 per-feature suites, 57 CTest tests.
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

include(IOv2TestOptions)
include(IOv2GoogleTest)

# One suite per directory that directly holds a source. Deriving it here is what
# makes a new directory a new suite with no other edit, and makes it impossible
# for a source to belong to no suite -- the two ways the split used to lose
# tests silently while staying green.
file(GLOB_RECURSE IOV2_TEST_ALL_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if(NOT IOV2_TEST_ALL_SOURCES)
  message(FATAL_ERROR "No test sources found under ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

set(IOV2_TEST_SUITE_DIRS "")
foreach(source IN LISTS IOV2_TEST_ALL_SOURCES)
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

  add_executable(${target} "${PROJECT_SOURCE_DIR}/cmake/gtest_main.cpp" ${suite_sources})

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

list(LENGTH IOV2_TEST_ALL_SOURCES IOV2_TEST_SOURCE_TOTAL)
list(LENGTH IOV2_TEST_SUITE_DIRS IOV2_TEST_SUITE_COUNT)
message(STATUS
    "IOv2 tests: ${IOV2_TEST_SOURCE_TOTAL} sources in "
    "${IOV2_TEST_SUITE_COUNT} suites (mode=${IOV2_TEST_MODE}, "
    "consumer=${IOV2_TEST_CONSUMER})")
