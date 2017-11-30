find_package(Qt5Core)
find_package(Qt5Network)

add_library(edict SHARED
	Broadcaster.h
	Broadcaster.cpp
)

target_link_libraries(edict
                      Qt5::Core
                      Qt5::Network)
