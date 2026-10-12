# GoogleTest, for the suites being converted away from support/verify.h.
#
# Built from source rather than taken as a package. The tests are compiled five
# ways -- release, debug, sanitizer, tsan, coverage -- and a prebuilt libgtest
# would be none of them; building it here lets each build tree hold a copy that
# matches. The CI image bakes the source at /opt/googletest so no job reaches the
# network for it; see .github/docker/gcc15.Dockerfile for why that is pinned.

include_guard(GLOBAL)

include(FetchContent)

# Honour the baked-in copy automatically. A machine without it downloads instead,
# so there is nothing to install locally either way.
if(NOT FETCHCONTENT_SOURCE_DIR_GOOGLETEST AND EXISTS "/opt/googletest/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "/opt/googletest" CACHE PATH
      "GoogleTest source directory")
endif()

# Same tarball and same hash the Dockerfile verifies.
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# iov2_test_options is an INTERFACE target, so none of its flags reach gtest. For
# -Wall -Werror -Wshadow that is what we want: those are ours to satisfy, not
# upstream's. For -std it is not. gtest's headers change shape with __cplusplus
# (GTEST_INTERNAL_HAS_STRING_VIEW and friends), so a library compiled at the
# compiler's default and a test TU compiled at C++23 would disagree about which
# inline functions exist -- an ODR violation no diagnostic would report.
#
# CXX_EXTENSIONS OFF is what makes this -std=c++23 rather than -std=gnu++23,
# matching what iov2_test_options passes.
foreach(_gtest_target IN ITEMS gtest gtest_main gmock gmock_main)
  if(TARGET ${_gtest_target})
    set_target_properties(${_gtest_target} PROPERTIES
        CXX_STANDARD 23
        CXX_EXTENSIONS OFF)
  endif()
endforeach()

# gmock pulls gtest, but a suite that uses only EXPECT_EQ should not have to know
# that. Both are named so either header works from any converted source.
add_library(iov2_test_gtest INTERFACE)
target_link_libraries(iov2_test_gtest INTERFACE GTest::gtest GTest::gmock)
