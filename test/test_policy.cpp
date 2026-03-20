#include <doctest/doctest.h>
#include <edict/Policy.h>
#include <edict/Error.h>
#include <type_traits>

TEST_CASE("SingleThreaded::Mutex is empty") {
    static_assert(std::is_empty_v<edict::SingleThreaded::Mutex>);
    edict::SingleThreaded::Mutex mtx;
    mtx.lock();
    mtx.unlock();
    mtx.lock_shared();
    mtx.unlock_shared();
}

TEST_CASE("SingleThreaded lock guards compile and are distinct types") {
    static_assert(!std::is_same_v<edict::SingleThreaded::SharedLock,
                                   edict::SingleThreaded::UniqueLock>);
    edict::SingleThreaded::Mutex mtx;
    { edict::SingleThreaded::UniqueLock lk(mtx); }
    { edict::SingleThreaded::SharedLock lk(mtx); }
}

TEST_CASE("MultiThreaded lock guards compile") {
    edict::MultiThreaded::Mutex mtx;
    { edict::MultiThreaded::UniqueLock lk(mtx); }
    { edict::MultiThreaded::SharedLock lk(mtx); }
}

TEST_CASE("ThreadingPolicy concept") {
    static_assert(edict::ThreadingPolicy<edict::SingleThreaded>);
    static_assert(edict::ThreadingPolicy<edict::MultiThreaded>);
    static_assert(!edict::ThreadingPolicy<int>);
}

TEST_CASE("Error enum values are distinct") {
    CHECK(edict::Error::InvalidTopic != edict::Error::TypeMismatch);
    CHECK(edict::Error::TypeMismatch != edict::Error::KeyNotFound);
    CHECK(edict::Error::KeyNotFound != edict::Error::BadCast);
}

TEST_CASE("error_message covers all values") {
    CHECK(edict::error_message(edict::Error::InvalidTopic).size() > 0);
    CHECK(edict::error_message(edict::Error::TypeMismatch).size() > 0);
    CHECK(edict::error_message(edict::Error::KeyNotFound).size() > 0);
    CHECK(edict::error_message(edict::Error::BadCast).size() > 0);
}

TEST_CASE("SubscribeOptions defaults") {
    edict::SubscribeOptions opts;
    CHECK(opts.priority == 0);
    CHECK(opts.replay == false);
}
