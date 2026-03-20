add_executable(blackboard-example main.cpp)
target_link_libraries(blackboard-example PRIVATE edict::edict)
target_compile_features(blackboard-example PRIVATE cxx_std_23)
