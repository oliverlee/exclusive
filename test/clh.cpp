#include "exclusive/exclusive.hpp"
#include "exclusive/test/access_task.hpp"
#include "exclusive/test/fake_clock.hpp"

#include "gtest/gtest.h"
#include <chrono>
#include <future>
#include <utility>
#include <vector>

namespace {
using namespace std::literals::chrono_literals;
namespace test = exclusive::test;

// A convenience function to setup a scenario with a mutex and N threads.
// - Thread1 acquires the lock
// - Thread2 is waiting on the lock, queued after Thread1, with timeout T2
// - Thread3 is waiting on the lock, queued after Thread2, with timeout T3
// ...
// where Tn is a duration with respect to fake_clock::now()
template <class Mutex, class... Durations>
auto queue_n_with_timeouts(Mutex& mut, Durations... dur) -> std::vector<test::AccessTask<Mutex>>
{
    auto tasks = std::vector<test::AccessTask<Mutex>>{};
    tasks.reserve(1 + sizeof...(Durations));

    tasks.emplace_back(mut);
    tasks.front().wait_for_access();

    auto count = 1U;
    while (count != mut.queue_count()) {}

    const auto spawn_next = [&mut, &tasks, &count](auto d) {
        tasks.emplace_back(mut, d);

        ++count;
        while (count != mut.queue_count()) {}

        return 0;
    };

    const auto unused = {spawn_next(dur)...};
    (void)unused;

    return tasks;
}

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
    auto mut = exclusive::clh_mutex<3>{};

    // launch thread 1 and 2, where 1 acquires access and 2 spins waiting on the
    // lock
    const auto deadline = test::fake_clock::now() + 1s;
    auto task = queue_n_with_timeouts(mut, deadline);

    EXPECT_TRUE(task[0].has_access());
    EXPECT_FALSE(task[1].has_access());

    // advance time and wait for task2 to timeout on lock acquire
    test::fake_clock::set_now(deadline);
    EXPECT_FALSE(task[1].get());

    // signal task1 to end
    EXPECT_TRUE(task[0].terminate());
}

// Given a clh_mutex,
// When queuing a bunch of threads on the lock,
// Then threads are given access in queue order.
TEST(ClhLock, FairnessInQueueAccess)
{
    auto mut = exclusive::clh_mutex<3>{};

    const auto deadline = test::fake_clock::now() + 1s;
    auto task = queue_n_with_timeouts(mut, deadline, deadline);

    EXPECT_TRUE(task[0].has_access());
    EXPECT_FALSE(task[1].has_access());
    EXPECT_FALSE(task[2].has_access());

    EXPECT_TRUE(task[0].terminate());
    task[1].wait_for_access();

    EXPECT_TRUE(task[1].terminate());
    task[2].wait_for_access();

    EXPECT_TRUE(task[2].terminate());
}

// Given a clh_mutex and 3 threads requesting access in order,
// When queuing 3 threads on the lock and thread 2 times-out,
// Then thread3 gets access after thread1 releases access.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(ClhLock, AbandonnedRequestIsSkippedOver)
{
    auto mut = exclusive::clh_mutex<3>{};

    const auto now = test::fake_clock::now();
    auto task = queue_n_with_timeouts(mut, now + 100ms, now + 200ms);

    EXPECT_TRUE(task[0].has_access());
    EXPECT_FALSE(task[1].has_access());
    EXPECT_FALSE(task[2].has_access());

    test::fake_clock::set_now(now + 150ms);
    EXPECT_FALSE(task[1].get());

    EXPECT_TRUE(task[0].has_access());
    EXPECT_FALSE(task[2].has_access());

    EXPECT_TRUE(task[0].terminate());
    task[2].wait_for_access();

    EXPECT_TRUE(task[2].terminate());
}

// Given a clh_mutex and 3 threads requesting access in order,
// When time advances and threads 2 and 3 time-out, while holding onto the lock in thread 1,
// Then the mutex is lockable after thread 1 releases access.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(ClhLock, AllAbandonnedRequestsAreSkipped)
{
    auto mut = exclusive::clh_mutex<3>{};

    const auto now = test::fake_clock::now();
    auto task = queue_n_with_timeouts(mut, now + 100ms, now + 200ms);

    EXPECT_TRUE(task[0].has_access());
    EXPECT_FALSE(task[1].has_access());
    EXPECT_FALSE(task[2].has_access());

    test::fake_clock::set_now(now + 250ms);
    EXPECT_FALSE(task[1].get());
    EXPECT_FALSE(task[2].get());

    EXPECT_TRUE(task[0].has_access());

    EXPECT_TRUE(task[0].terminate());

    EXPECT_TRUE(mut.try_lock());
}
