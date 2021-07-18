#include "exclusive/exclusive.hpp"
#include "exclusive/test/fake_clock.hpp"

#include "gtest/gtest.h"
#include <chrono>
#include <future>
#include <mutex>
#include <utility>

namespace {
using namespace std::literals::chrono_literals;


template <std::future_status Status, class T>
auto status_is(const std::future<T>& fut) -> bool
{
    return Status == fut.wait_for(0s);
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
    auto mut = exclusive::clh_mutex<2>{};

    const auto access_and_wait = [&mut](auto deadline, auto on_access, auto hold_until) -> bool {
        auto access_scope = std::unique_lock{mut, deadline};

        if (access_scope) {
            on_access.set_value();
            hold_until.get();
            return true;
        }

        return false;
    };

    auto p1a = std::promise<void>{};
    auto p1b = std::promise<void>{};

    auto p2a = std::promise<void>{};
    auto p2b = std::promise<void>{};

    auto has_access1 = p1a.get_future();
    auto has_access2 = p2a.get_future();

    const auto deadline = exclusive::test::fake_clock::now() + 1s;

    // launch thread 1 and wait until it acquires access
    auto task1 = std::async(std::launch::async,
                            access_and_wait,
                            deadline,
                            std::move(p1a),
                            p1b.get_future());
    has_access1.get();

    // launch thread 2 and wait for it to spin on the lock
    auto task2 = std::async(std::launch::async,
                            access_and_wait,
                            deadline,
                            std::move(p2a),
                            p2b.get_future());
    while (!status_is<std::future_status::timeout>(task2)) {}

    // advance time and wait for task2 to timeout on lock acquire
    exclusive::test::fake_clock::set_now(deadline);
    EXPECT_FALSE(task2.get());

    // signal task1 to end
    p1b.set_value();
    EXPECT_TRUE(task1.get());
}
