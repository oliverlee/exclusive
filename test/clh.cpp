#include "exclusive/exclusive.hpp"
#include "exclusive/test/access_task.hpp"
#include "exclusive/test/fake_clock.hpp"

#include "gtest/gtest.h"
#include <chrono>
#include <future>

namespace {
using namespace std::literals::chrono_literals;
namespace test = exclusive::test;

}  // namespace

// Given a clh_mutex,
// When there is an uncontested lock request,
// Then it should succeed with non-positive durations.
TEST(ClhLock, TryLockForNonPositiveDuration)
{
    auto mut = exclusive::clh_mutex<1>{};

    // No contention so both calls to `try_lock_for` should succeed
    EXPECT_TRUE(mut.try_lock_for(0s));
    mut.unlock();

    EXPECT_TRUE(mut.try_lock_for(-1s));
    mut.unlock();
}

// Given a clh_mutex,
// When waiting on a lock until a deadline,
// Then locking fails after the deadline is reached.
TEST(ClhLock, TimeoutWithFakeClock)
{
    auto mut = exclusive::clh_mutex<2>{};

    const auto deadline = test::fake_clock::now() + 1s;

    // launch thread 1 and wait until it acquires access
    auto task1 = test::AccessTask{mut, deadline};
    task1.wait_until_access();

    // launch thread 2 and wait for it to spin on the lock
    auto task2 = test::AccessTask{mut, deadline};
    while (!task2.status_is<std::future_status::timeout>()) {}
    EXPECT_FALSE(task2.has_access());

    // advance time and wait for task2 to timeout on lock acquire
    test::fake_clock::set_now(deadline);
    EXPECT_FALSE(task2.get());

    // signal task1 to end
    EXPECT_TRUE(task1.terminate());
}
