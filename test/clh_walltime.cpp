#include "exclusive/exclusive.hpp"
#include "exclusive/test/access_task.hpp"

// Test depending on wall time. These may be flaky if run on a loaded machine.

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <future>
#include <mutex>
#include <utility>

namespace {
using namespace std::chrono_literals;
namespace test = exclusive::test;

constexpr auto WALL_TIME_WAIT_DURATION = 100ms;
constexpr auto TOL = WALL_TIME_WAIT_DURATION / 10;
}  // namespace

// Given a clh_mutex locked by another thread,
// When calling `try_lock_for` with a positive duration,
// Then the call blocks for the given duration and fails.
TEST(ClhLockWallTime, WhileLockedTryLockForShortDuration)
{
    auto mut = exclusive::clh_mutex<1>{};

    // launch a thread to acquire the lock
    const auto deadline = std::chrono::steady_clock::now() + 24h;
    auto task1 = test::AccessTask{mut, deadline};
    task1.wait_until_access();

    // verify that `try_lock_for` fails due to timeout
    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(mut.try_lock_for(WALL_TIME_WAIT_DURATION));
    const auto end = std::chrono::steady_clock::now();

    // chcke that elapsed time roughly matches the requested duration
    EXPECT_THAT((end - start),
                testing::AllOf(testing::Ge(WALL_TIME_WAIT_DURATION),
                               testing::Le(WALL_TIME_WAIT_DURATION + TOL)));

    task1.terminate();
}

// Given a clh_mutex locked by another thread,
// When calling `try_lock_for` after another call has been abandoned due to
// timeout but after the lock is released,
// Then `try_lock_for` returns early and succeeds.
TEST(ClhLockWallTime, TestWithTimeoutAbandonned)
{
    auto mut = exclusive::clh_mutex<3>{};

    // launch a thread to acquire the lock
    auto task1 = test::AccessTask{mut};
    task1.wait_until_access();

    EXPECT_FALSE(mut.try_lock_for(WALL_TIME_WAIT_DURATION));

    task1.terminate();

    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(mut.try_lock_for(WALL_TIME_WAIT_DURATION));
    const auto end = std::chrono::steady_clock::now();

    EXPECT_LT((end - start), WALL_TIME_WAIT_DURATION);
}
