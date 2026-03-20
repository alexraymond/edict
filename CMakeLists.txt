cmake_minimum_required(VERSION 3.25)
project(edict VERSION 1.0.0 LANGUAGES CXX)

add_library(edict INTERFACE)
add_library(edict::edict ALIAS edict)

target_include_directories(edict INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(edict INTERFACE cxx_std_23)

option(EDICT_BUILD_TESTS "Build tests" OFF)
option(EDICT_BUILD_EXAMPLES "Build examples" OFF)

if(EDICT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
if(EDICT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(GNUInstallDirs)
install(TARGETS edict EXPORT edictTargets)
install(DIRECTORY include/edict DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT edictTargets FILE edictTargets.cmake NAMESPACE edict::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edict)

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/edictConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/edictConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edict)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/edictConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edictConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/edictConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edict)
