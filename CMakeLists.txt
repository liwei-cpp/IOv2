# IOv2 -- library packaging: install the headers (header-only, the default) or
# build and install libiov2 (shared-library mode), together with pkg-config files
# and a CMake package (find_package(IOv2) -> IOv2::iov2).
#
# IOv2 is header-only by default and needs none of this to be used: add include/
# to the include path and #include the headers. The test suite is a separate
# project rooted at test/; see test/README.md.
#
#   # header-only: headers, licence files, iov2.pc, CMake package
#   cmake -S . -B build/pkg -DCMAKE_INSTALL_PREFIX=/usr/local
#   cmake --install build/pkg
#
#   # shared library: additionally libiov2 + iov2-shared.pc, and the installed
#   # headers have the IOV2_SHARED switch turned on
#   cmake -S . -B build/pkg-shared -DIOV2_BUILD_SHARED=ON -DCMAKE_INSTALL_PREFIX=/opt/iov2
#   cmake --build build/pkg-shared
#   cmake --install build/pkg-shared
#
#   # remove what the last install from that build tree put in place
#   cmake --build build/pkg-shared --target uninstall
#
# The mode is baked into the installed iov2_export.h, so one prefix holds one
# mode. The prefix is written into the .pc files at configure time: set
# CMAKE_INSTALL_PREFIX when configuring rather than `cmake --install --prefix`.

cmake_minimum_required(VERSION 3.25)

project(IOv2
    VERSION 1.0.0
    DESCRIPTION "C++23 I/O and localization library"
    HOMEPAGE_URL "https://github.com/liwei-cpp/IOv2"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR
      "In-source builds are not supported. Configure into a separate directory, "
      "e.g. cmake -S . -B build/pkg")
endif()

option(IOV2_BUILD_SHARED
    "Build and install libiov2 (shared-library mode) instead of the header-only library"
    OFF)
option(IOV2_INSTALL "Generate the install and uninstall rules" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ------------------------------------------------------------------ the library
#
# IOv2::iov2 is the one target consumers link in either mode: an interface library
# when header-only, libiov2 when shared. Its usage requirements are what iov2.pc /
# iov2-shared.pc hand out: the include root, C++23, and on POSIX the feature-test
# macro the headers need (locale_t, newlocale, uselocale, the *_l helpers).
if(IOV2_BUILD_SHARED)
  # Release unless told otherwise: an unoptimized libiov2 is never what is wanted.
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  endif()

  add_library(iov2 SHARED src/iov2_objects.cpp)
  # Hidden by default keeps the exported surface to the IOV2_API-tagged singleton
  # references; everything else stays internal to the library.
  set_target_properties(iov2 PROPERTIES
      CXX_EXTENSIONS OFF
      CXX_VISIBILITY_PRESET hidden
      VERSION ${PROJECT_VERSION}
      SOVERSION ${PROJECT_VERSION_MAJOR})
  target_link_libraries(iov2 PRIVATE ${CMAKE_DL_LIBS})
  set(IOV2_USAGE PUBLIC)
else()
  add_library(iov2 INTERFACE)
  set(IOV2_USAGE INTERFACE)
endif()
add_library(IOv2::iov2 ALIAS iov2)

target_include_directories(iov2 ${IOV2_USAGE}
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(iov2 ${IOV2_USAGE} cxx_std_23)
target_compile_definitions(iov2 ${IOV2_USAGE}
    $<$<NOT:$<PLATFORM_ID:Windows>>:_POSIX_C_SOURCE=200809L>)
if(IOV2_BUILD_SHARED)
  # The source tree's iov2_export.h keeps the switch off, so a consumer of this
  # build tree (add_subdirectory, FetchContent) gets it from here instead. The
  # installed headers carry it themselves. INTERFACE: iov2_objects.cpp defines it.
  target_compile_definitions(iov2 INTERFACE $<BUILD_INTERFACE:IOV2_SHARED>)
endif()

if(NOT IOV2_INSTALL)
  return()
endif()

# ------------------------------------------------------------- the mode switch
#
# The installed iov2_export.h is generated: a copy of the source one, with the
# IOV2_SHARED line uncommented in shared mode. Regenerated whenever the source
# header changes, and rewritten only when its content does.
set(IOV2_EXPORT_H_SRC "${PROJECT_SOURCE_DIR}/include/IOv2/common/iov2_export.h")
set(IOV2_EXPORT_H_OUT "${PROJECT_BINARY_DIR}/include/IOv2/common/iov2_export.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${IOV2_EXPORT_H_SRC}")
file(READ "${IOV2_EXPORT_H_SRC}" iov2_export_h)
if(IOV2_BUILD_SHARED)
  string(REPLACE "\n// #define IOV2_SHARED 1\n" "\n#define IOV2_SHARED 1\n"
         iov2_export_h_shared "${iov2_export_h}")
  if(iov2_export_h_shared STREQUAL iov2_export_h)
    message(FATAL_ERROR
        "The line '// #define IOV2_SHARED 1' was not found in ${IOV2_EXPORT_H_SRC}; "
        "the installed headers would not be in shared mode.")
  endif()
  set(iov2_export_h "${iov2_export_h_shared}")
endif()
file(WRITE "${IOV2_EXPORT_H_OUT}.tmp" "${iov2_export_h}")
file(COPY_FILE "${IOV2_EXPORT_H_OUT}.tmp" "${IOV2_EXPORT_H_OUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${IOV2_EXPORT_H_OUT}.tmp")

# ------------------------------------------------------------------- install
#
# Headers below a package-owned include/IOv2/ rather than generic names such as
# include/common/; consumers write #include <IOv2/...> against the include root.
set(IOV2_INSTALL_HEADERDIR "${CMAKE_INSTALL_INCLUDEDIR}/IOv2")
set(IOV2_INSTALL_LICENSEDIR "${CMAKE_INSTALL_DATAROOTDIR}/licenses/IOv2")
if(IOV2_BUILD_SHARED)
  set(IOV2_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/IOv2")
  set(IOV2_PC_NAME iov2-shared)
  set(IOV2_ARCH_INDEPENDENT "")
else()
  # Nothing architecture-specific is installed, so the package goes below share/.
  set(IOV2_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/cmake/IOv2")
  set(IOV2_PC_NAME iov2)
  set(IOV2_ARCH_INDEPENDENT ARCH_INDEPENDENT)
endif()

install(DIRECTORY include/IOv2/ DESTINATION "${IOV2_INSTALL_HEADERDIR}"
        REGEX "/common/iov2_export\\.h$" EXCLUDE)
install(FILES "${IOV2_EXPORT_H_OUT}" DESTINATION "${IOV2_INSTALL_HEADERDIR}/common")
install(FILES LICENSE NOTICE DESTINATION "${IOV2_INSTALL_LICENSEDIR}")

install(TARGETS iov2 EXPORT IOv2Targets
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(EXPORT IOv2Targets NAMESPACE IOv2:: DESTINATION "${IOV2_INSTALL_CMAKEDIR}")

configure_package_config_file(cmake/IOv2Config.cmake.in
    "${PROJECT_BINARY_DIR}/IOv2Config.cmake"
    INSTALL_DESTINATION "${IOV2_INSTALL_CMAKEDIR}")
write_basic_package_version_file("${PROJECT_BINARY_DIR}/IOv2ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion ${IOV2_ARCH_INDEPENDENT})
install(FILES "${PROJECT_BINARY_DIR}/IOv2Config.cmake"
              "${PROJECT_BINARY_DIR}/IOv2ConfigVersion.cmake"
        DESTINATION "${IOV2_INSTALL_CMAKEDIR}")

configure_file("pkgconfig/${IOV2_PC_NAME}.pc.in" "${PROJECT_BINARY_DIR}/${IOV2_PC_NAME}.pc" @ONLY)
install(FILES "${PROJECT_BINARY_DIR}/${IOV2_PC_NAME}.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# ------------------------------------------------------------------ uninstall
#
# Removes what install_manifest.txt lists, then the IOv2-owned directories once
# that leaves them empty. Nothing else under the prefix is touched.
set(IOV2_OWNED_DIRS
    "${IOV2_INSTALL_HEADERDIR}" "${IOV2_INSTALL_LICENSEDIR}" "${IOV2_INSTALL_CMAKEDIR}")
configure_file(cmake/IOv2Uninstall.cmake.in "${PROJECT_BINARY_DIR}/IOv2Uninstall.cmake" @ONLY)
add_custom_target(uninstall
    COMMAND "${CMAKE_COMMAND}" -P "${PROJECT_BINARY_DIR}/IOv2Uninstall.cmake"
    COMMENT "Uninstalling IOv2 from ${CMAKE_INSTALL_PREFIX}"
    VERBATIM)
