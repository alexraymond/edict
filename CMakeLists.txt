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

# Common properties
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x600)
endif()

# Add all the example projects
add_subdirectory(examples)
