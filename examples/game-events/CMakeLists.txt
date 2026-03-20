add_executable(game-events main.cpp)
target_link_libraries(game-events PRIVATE edict::edict)
target_compile_features(game-events PRIVATE cxx_std_23)
