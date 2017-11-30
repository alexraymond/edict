cmake_minimum_required(VERSION 3.1.0)

project(edict)

# Global configuration for all projects
set(EDICT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/edict)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Configure the output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
else()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
endif()

# Add all the source projects
add_subdirectory(experiments)
add_subdirectory(src)
