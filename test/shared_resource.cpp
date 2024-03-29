#include "exclusive/exclusive.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <future>
#include <system_error>
#include <thread>

namespace {
using namespace std::chrono_literals;

template <std::future_status Status, class T>
auto status_is(const std::future<T>& fut) -> bool
{
    return Status == fut.wait_for(0s);
}

}  // namespace

TEST(SharedResource, AccessFromMultipleThreads)
{
    auto x = exclusive::shared_resource<int, exclusive::array_mutex<4>>{};

    const auto inc_n = [&x](std::size_t n) {
        for (std::size_t i = 0U; i != n; ++i) { ++(*x.access()); }
    };

    constexpr auto n = 1'000U;

    auto t1 = std::thread{inc_n, n};
    auto t2 = std::thread{inc_n, n};
    auto t3 = std::thread{inc_n, n};
    auto t4 = std::thread{inc_n, n};

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    EXPECT_EQ(4 * n, *x.access());
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(SharedResource, ThrowsWhenSlotsExceeded)
{
    auto x = exclusive::shared_resource<int, exclusive::array_mutex<2>>{};

    const auto access_and_wait = [&x](auto stop) {
        auto access_scope = x.access();
        stop.get();
    };

    auto p1 = std::promise<void>{};
    auto p2 = std::promise<void>{};
    auto p3 = std::promise<void>{};

    auto tasks = std::array{std::async(std::launch::async, access_and_wait, p1.get_future()),
                            std::async(std::launch::async, access_and_wait, p2.get_future()),
                            std::async(std::launch::async, access_and_wait, p3.get_future())};

    auto no_slot = tasks.end();
    while (no_slot == tasks.end()) {
        no_slot = std::find_if(tasks.begin(), tasks.end(), [](const auto& fut) {
            return status_is<std::future_status::ready>(fut);
        });
    }

    ASSERT_THROW(no_slot->get(), std::system_error);

    for (const auto& fut : tasks) {
        ASSERT_TRUE((&fut == &*no_slot) || status_is<std::future_status::timeout>(fut));
    }

    p1.set_value();
    p2.set_value();
    p3.set_value();
}

TEST(SharedResourceClhLock, AccessFromMultipleThreads)
{
    auto x = exclusive::shared_resource<int, exclusive::clh_mutex<4>>{};

    const auto inc_n = [&x](std::size_t n) {
        for (std::size_t i = 0U; i != n; ++i) { ++(*x.access()); }
    };

    constexpr auto n = 1'000U;

    auto t1 = std::thread{inc_n, n};
    auto t2 = std::thread{inc_n, n};
    auto t3 = std::thread{inc_n, n};
    auto t4 = std::thread{inc_n, n};

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    EXPECT_EQ(4 * n, *x.access());
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(SharedResourceClhLock, ThrowsWhenSlotsExceeded)
{
    // With 1 (+ 2) slots, the clh_mutex starts with:
    // - tail : [x]
    // - available : [ ], [ ]
    //
    // Threads can only take an available slot if it's not the last one and the
    // following situations are possible:
    // - tail : [x]
    // - available : [ ]
    // - taken: [1]
    //
    // - tail : [x] [1]
    // - available : [ ]
    // - taken :
    //
    // - tail : [1]
    // - available : [ ]
    // - taken : [2]
    //
    auto x = exclusive::shared_resource<int, exclusive::clh_mutex<1, exclusive::failure::die>>{};

    const auto access_and_wait = [&x](auto stop_after) {
        auto access_scope = x.access();
        stop_after.get();
    };

    auto p1 = std::promise<void>{};
    auto p2 = std::promise<void>{};
    auto p3 = std::promise<void>{};

    auto tasks = std::array{std::async(std::launch::async, access_and_wait, p1.get_future()),
                            std::async(std::launch::async, access_and_wait, p2.get_future()),
                            std::async(std::launch::async, access_and_wait, p3.get_future())};

    auto no_slot = tasks.end();
    while (no_slot == tasks.end()) {
        no_slot = std::find_if(tasks.begin(), tasks.end(), [](const auto& fut) {
            return status_is<std::future_status::ready>(fut);
        });
    }

    ASSERT_THROW(no_slot->get(), std::system_error);

    auto num_waiting = 0;

    for (auto& fut : tasks) {
        if (&fut == &*no_slot) {
            continue;
        }

        if (status_is<std::future_status::timeout>(fut)) {
            ++num_waiting;
        } else {
            ASSERT_TRUE(status_is<std::future_status::ready>(fut));
            ASSERT_THROW(fut.get(), std::system_error);
        }
    }

    EXPECT_THAT(num_waiting, testing::AllOf(testing::Ge(1), testing::Le(2)));

    p1.set_value();
    p2.set_value();
    p3.set_value();
}

TEST(SharedResourceClhLock, ScopedAccessFailureOnTimeout)
{
    auto x = exclusive::shared_resource<int, exclusive::clh_mutex<2>>{};

    auto end = std::promise<void>{};
    auto on_access = std::promise<void>{};
    auto has_access = on_access.get_future();

    auto task = std::async(
        std::launch::async,
        [&x](auto on_access, auto stop_after) {
            auto access_scope = x.access();

            ASSERT_TRUE(access_scope);
            on_access.set_value();

            stop_after.get();
        },
        std::move(on_access),
        end.get_future());

    has_access.wait();

    EXPECT_FALSE(x.access_within(0s));

    end.set_value();
}
