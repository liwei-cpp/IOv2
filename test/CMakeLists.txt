# The test suite: 189 sources, 57 per-feature suites, 57 CTest tests.
#
# test/Makefile linked everything into seven executables, one per top-level
# directory, each driven by a hand-written main() that called its tests in a
# fixed order. That was too coarse to work with: a single failure meant
# re-running hundreds of unrelated tests, and there was no way to ask for just
# the monetary facet or just ostream::seek.
#
# The suites here are those same tests, regrouped by the directory that holds
# them and given a generated main(). Which functions each one calls, and in what
# order, is derived from the seven aggregate main()s by
# test/tools/derive_suites.py and recorded in test/suites.cmake -- so this is a
# regrouping, not a rewrite. No test source changes.

include(IOv2TestOptions)
include("${CMAKE_CURRENT_SOURCE_DIR}/suites.cmake")

# Everything lands under <build>/test/ in the layout the tests already expect:
# eight of them locate message catalogues by resolving /proc/self/exe and then
# walking to <exe dir>/../IOv2TestResources. Reproducing that layout is what lets
# this migration leave those tests untouched; replacing it with an environment
# variable is a separate, later change.
set(IOV2_TEST_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/IOv2TestResources"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

set(IOV2_TEST_CLAIMED_SOURCES "")

foreach(suite_dir IN LISTS IOV2_TEST_SUITE_DIRS)
  string(REPLACE "/" "_" suite_slug "${suite_dir}")
  string(REPLACE "/" "." IOV2_SUITE_NAME "${suite_dir}")
  set(target "test_${suite_slug}")

  # Not GLOB_RECURSE: a nested directory is a suite of its own, and compiling it
  # into its parent as well would run those tests twice.
  file(GLOB suite_sources CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/${suite_dir}/*.cpp")
  foreach(excluded IN LISTS IOV2_TEST_EXCLUDED_SOURCES)
    list(REMOVE_ITEM suite_sources "${CMAKE_CURRENT_SOURCE_DIR}/${excluded}")
  endforeach()
  if(NOT suite_sources)
    message(FATAL_ERROR "Suite '${suite_dir}' has no sources left to compile")
  endif()
  list(APPEND IOV2_TEST_CLAIMED_SOURCES ${suite_sources})

  set(IOV2_SUITE_DECLARATIONS "")
  set(IOV2_SUITE_CALLS "")
  foreach(entry IN LISTS ${target}_ENTRIES)
    string(APPEND IOV2_SUITE_DECLARATIONS "void ${entry}();\n")
    string(APPEND IOV2_SUITE_CALLS "        ${entry}();\n")
  endforeach()

  set(suite_main "${CMAKE_CURRENT_BINARY_DIR}/generated/${suite_slug}_main.cpp")
  configure_file("${PROJECT_SOURCE_DIR}/cmake/suite_main.cpp.in" "${suite_main}" @ONLY)

  add_executable(${target} ${suite_main} ${suite_sources})

  target_link_libraries(${target} PRIVATE
      iov2_test_subject iov2_test_options iov2_test_thirdparty)

  # -I../include then -I., in that order, exactly as the Makefile passes them.
  # The second is what makes <support/verify.h> resolve.
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

# Two ways for the split to lose tests silently, both of which leave the suites
# green and show up only as a hole in the coverage report: a glob that stops
# matching, and a new directory that no suite covers. Check the total against the
# manifest, then check that the suites between them claim every source the
# manifest does not name as deliberately excluded.
file(GLOB_RECURSE IOV2_TEST_ALL_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(LENGTH IOV2_TEST_ALL_SOURCES IOV2_TEST_SOURCE_TOTAL)
if(NOT IOV2_TEST_SOURCE_TOTAL EQUAL IOV2_TEST_SOURCE_TOTAL_EXPECTED)
  message(FATAL_ERROR
      "Found ${IOV2_TEST_SOURCE_TOTAL} test sources, expected "
      "${IOV2_TEST_SOURCE_TOTAL_EXPECTED}. If sources were added or removed, "
      "re-run test/tools/derive_suites.py.")
endif()

set(IOV2_TEST_UNCLAIMED_SOURCES ${IOV2_TEST_ALL_SOURCES})
list(REMOVE_ITEM IOV2_TEST_UNCLAIMED_SOURCES ${IOV2_TEST_CLAIMED_SOURCES})
foreach(excluded IN LISTS IOV2_TEST_EXCLUDED_SOURCES)
  list(REMOVE_ITEM IOV2_TEST_UNCLAIMED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${excluded}")
endforeach()
if(IOV2_TEST_UNCLAIMED_SOURCES)
  message(FATAL_ERROR
      "These test sources belong to no suite: ${IOV2_TEST_UNCLAIMED_SOURCES}. "
      "Re-run test/tools/derive_suites.py.")
endif()

list(LENGTH IOV2_TEST_CLAIMED_SOURCES IOV2_TEST_CLAIMED_COUNT)
list(LENGTH IOV2_TEST_SUITE_DIRS IOV2_TEST_SUITE_COUNT)
message(STATUS
    "IOv2 tests: ${IOV2_TEST_CLAIMED_COUNT} of ${IOV2_TEST_SOURCE_TOTAL} sources in "
    "${IOV2_TEST_SUITE_COUNT} suites (mode=${IOV2_TEST_MODE}, "
    "consumer=${IOV2_TEST_CONSUMER})")
