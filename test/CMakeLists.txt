include(FetchContent)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(doctest)

add_executable(edict_tests
    test_policy.cpp
    test_traits.cpp
    test_subscription.cpp
    test_topic_tree.cpp
    test_channel.cpp
    test_topic_router.cpp
    test_broadcaster.cpp
    test_reentrancy.cpp
    test_blackboard.cpp
    test_free_functions.cpp
    test_threading.cpp
)

target_link_libraries(edict_tests PRIVATE edict::edict doctest::doctest_with_main)
target_compile_features(edict_tests PRIVATE cxx_std_23)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(edict_tests)
