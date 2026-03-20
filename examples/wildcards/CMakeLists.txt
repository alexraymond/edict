add_executable(wildcards-example main.cpp)
target_link_libraries(wildcards-example PRIVATE edict::edict)
target_compile_features(wildcards-example PRIVATE cxx_std_23)
