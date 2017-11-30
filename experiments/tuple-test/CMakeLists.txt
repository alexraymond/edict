find_package(Qt5Core)

add_executable(tuple-test
               main.cpp)

target_link_libraries(tuple-test Qt5::Core)
