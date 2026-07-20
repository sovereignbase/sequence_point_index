cmake_minimum_required(VERSION 3.20)

project(
  uuid_map
  VERSION 0.1.0
  DESCRIPTION "A direct sequence-point registry with 96-bit actors and 32-bit indices"
  LANGUAGES CXX
)

add_library(sovereignbase_uuid_map INTERFACE)
add_library(sovereignbase::uuid_map ALIAS sovereignbase_uuid_map)

set_target_properties(
  sovereignbase_uuid_map
  PROPERTIES EXPORT_NAME uuid_map
)

target_compile_features(sovereignbase_uuid_map INTERFACE cxx_std_17)
target_include_directories(
  sovereignbase_uuid_map
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

option(UUID_MAP_BUILD_TESTS "Build uuid_map tests" ${PROJECT_IS_TOP_LEVEL})
option(UUID_MAP_BUILD_BENCHMARKS "Build uuid_map benchmarks" OFF)
option(UUID_MAP_INSTALL "Generate uuid_map install rules" ${PROJECT_IS_TOP_LEVEL})

if(UUID_MAP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(UUID_MAP_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(UUID_MAP_INSTALL)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  set(uuid_map_cmake_directory "${CMAKE_INSTALL_LIBDIR}/cmake/uuid_map")

  install(
    TARGETS sovereignbase_uuid_map
    EXPORT uuid_mapTargets
  )
  install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )
  install(
    EXPORT uuid_mapTargets
    FILE uuid_mapTargets.cmake
    NAMESPACE sovereignbase::
    DESTINATION "${uuid_map_cmake_directory}"
  )

  configure_package_config_file(
    cmake/uuid_mapConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/uuid_mapConfig.cmake"
    INSTALL_DESTINATION "${uuid_map_cmake_directory}"
  )
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/uuid_mapConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
  )
  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/uuid_mapConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/uuid_mapConfigVersion.cmake"
    DESTINATION "${uuid_map_cmake_directory}"
  )
endif()
