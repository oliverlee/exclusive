#include "exclusive/exclusive.hpp"

// Test depending on wall time. These may be flaky if run on a loaded machine.

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <future>
#include <mutex>
#include <utility>

namespace {
using namespace std::chrono_literals;

constexpr auto WALL_TIME_WAIT_DURATION = 100ms;
constexpr auto TOL = WALL_TIME_WAIT_DURATION / 10;
}  // namespace

// Given a clh_mutex locked by another thread,
// When calling `try_lock_for` with a positive duration,
// Then the call blocks for the given duration and fails.
TEST(ClhLockWallTime, WhileLockedTryLockForShortDuration)
{
    auto mut = exclusive::clh_mutex<1>{};

    const auto access_and_wait = [&mut](auto on_access, auto hold_until) {
        auto access_scope = std::scoped_lock{mut};
        on_access.set_value();
        hold_until.get();
    };

    auto on_access = std::promise<void>{};
    auto has_access = on_access.get_future();

    auto release_access = std::promise<void>{};
    auto task = std::async(
        std::launch::async, access_and_wait, std::move(on_access), release_access.get_future());

    has_access.get();

    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(mut.try_lock_for(WALL_TIME_WAIT_DURATION));
    const auto end = std::chrono::steady_clock::now();

    EXPECT_THAT((end - start),
                testing::AllOf(testing::Ge(WALL_TIME_WAIT_DURATION),
                               testing::Le(WALL_TIME_WAIT_DURATION + TOL)));

    release_access.set_value();
}

// Given a clh_mutex locked by another thread,
// When calling `try_lock_for` after another call has been abandoned due to
// timeout but after the lock is released,
// Then `try_lock_for` returns early and succeeds.
TEST(ClhLockWallTime, TestWithTimeoutAbandonned)
{
    auto mut = exclusive::clh_mutex<3>{};

    const auto access_and_wait = [&mut](auto on_access, auto hold_until) {
        auto access_scope = std::scoped_lock{mut};
        on_access.set_value();
        hold_until.get();
    };

    auto on_access = std::promise<void>{};
    auto has_access = on_access.get_future();

    auto release_access = std::promise<void>{};
    auto task = std::async(
        std::launch::async, access_and_wait, std::move(on_access), release_access.get_future());

    has_access.get();

    EXPECT_FALSE(mut.try_lock_for(WALL_TIME_WAIT_DURATION));

    release_access.set_value();
    task.get();

    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(mut.try_lock_for(WALL_TIME_WAIT_DURATION));
    const auto end = std::chrono::steady_clock::now();

    EXPECT_LT((end - start), WALL_TIME_WAIT_DURATION);
}
