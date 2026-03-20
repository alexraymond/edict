add_executable(threaded-example main.cpp)
target_link_libraries(threaded-example PRIVATE edict::edict)
target_compile_features(threaded-example PRIVATE cxx_std_23)
