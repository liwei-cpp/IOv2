# The test suite: 189 sources, seven aggregate executables, seven CTest tests.
#
# The grouping is deliberately identical to test/Makefile for now. Splitting the
# seven binaries into per-feature suites is worthwhile and is the next change,
# but doing it here would mean the Makefile and CTest results could differ for
# two reasons at once, and coverage or timing drift could not be attributed.

include(IOv2TestOptions)

set(IOV2_TEST_MODULES device cvt facet locale io common concur)

# The Makefile collects sources with `find`, so a new .cpp is picked up simply by
# existing. CONFIGURE_DEPENDS keeps that property: the glob is re-run when the
# build directory is out of date rather than only at configure time.
set(IOV2_TEST_SOURCE_TOTAL 0)

# Everything lands under <build>/test/ in the layout the tests already expect:
# eight of them locate message catalogues by resolving /proc/self/exe and then
# walking to <exe dir>/../IOv2TestResources. Reproducing that layout is what lets
# this migration leave those tests untouched; replacing it with an environment
# variable is a separate, later change.
set(IOV2_TEST_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(IOV2_TEST_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/IOv2TestResources")

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/IOv2TestResources"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

foreach(module IN LISTS IOV2_TEST_MODULES)
  file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/${module}/*.cpp")

  list(LENGTH module_sources module_source_count)
  if(module_source_count EQUAL 0)
    message(FATAL_ERROR "No sources found for test module '${module}'")
  endif()
  math(EXPR IOV2_TEST_SOURCE_TOTAL "${IOV2_TEST_SOURCE_TOTAL} + ${module_source_count}")

  add_executable(test_${module} ${module_sources})

  target_link_libraries(test_${module} PRIVATE
      iov2_test_subject iov2_test_options iov2_test_thirdparty)

  # -I../include then -I., in that order, exactly as the Makefile passes them.
  # The second is what makes <support/verify.h> resolve.
  target_include_directories(test_${module} PRIVATE
      ${IOV2_TEST_SUBJECT_INCLUDE_DIRS}
      "${CMAKE_CURRENT_SOURCE_DIR}")

  set_target_properties(test_${module} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${IOV2_TEST_BIN_DIR}")

  # A private working directory per module. Under the Makefile all seven binaries
  # run one after another from test/, so the temporary files and stdio
  # redirection that at least 38 sources set up cannot collide. CTest may run
  # them concurrently, so the isolation has to be explicit.
  set(module_work_dir "${CMAKE_CURRENT_BINARY_DIR}/work/${module}")
  file(MAKE_DIRECTORY "${module_work_dir}")

  add_test(NAME module.${module} COMMAND test_${module})

  set(module_labels "all;${module}")
  if(module STREQUAL "concur")
    list(APPEND module_labels "thread")
  endif()

  set_tests_properties(module.${module} PROPERTIES
      LABELS "${module_labels}"
      WORKING_DIRECTORY "${module_work_dir}")
endforeach()

# A glob that silently stops matching a directory would leave the tests green and
# only show up as a hole in the coverage report. The count is cheap insurance.
set(IOV2_TEST_SOURCE_EXPECTED 189 CACHE STRING
    "Number of test sources expected; guards against a glob that stops matching")
if(NOT IOV2_TEST_SOURCE_TOTAL EQUAL IOV2_TEST_SOURCE_EXPECTED)
  message(FATAL_ERROR
      "Collected ${IOV2_TEST_SOURCE_TOTAL} test sources, expected ${IOV2_TEST_SOURCE_EXPECTED}. "
      "If sources were deliberately added or removed, update IOV2_TEST_SOURCE_EXPECTED.")
endif()

message(STATUS
    "IOv2 tests: ${IOV2_TEST_SOURCE_TOTAL} sources -> ${IOV2_TEST_MODULES} "
    "(mode=${IOV2_TEST_MODE}, consumer=${IOV2_TEST_CONSUMER})")
