# Common properties
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x600)
endif()

# Edict experiment executables
add_subdirectory(tuple-test)
