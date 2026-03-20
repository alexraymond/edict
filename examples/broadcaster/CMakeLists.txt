add_executable(broadcaster-example main.cpp)
target_link_libraries(broadcaster-example PRIVATE edict::edict)
target_compile_features(broadcaster-example PRIVATE cxx_std_23)
