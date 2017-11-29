cmake_minimum_required(VERSION 3.9)
project(edict)

file(GLOB SOURCES *.cpp)
file(GLOB HEADERS *.cpp)

add_library(edict SHARED
	Broadcaster.h
	Broadcaster.cpp
)

find_package(Qt5Core)
find_package(Qt5Network)

target_link_libraries(edict
                      Qt5::Core
                      Qt5::Network)

install(TARGETS edict DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)