cmake_minimum_required(VERSION 3.1)

add_executable(hello-edict
	main.cpp
)

target_compile_features(hello-edict PRIVATE cxx_std_14)
