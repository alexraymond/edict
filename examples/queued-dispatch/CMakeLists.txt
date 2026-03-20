add_executable(queued-dispatch main.cpp)
target_link_libraries(queued-dispatch PRIVATE edict::edict)
target_compile_features(queued-dispatch PRIVATE cxx_std_23)
