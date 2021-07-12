#include "exclusive/exclusive.hpp"

#include "gtest/gtest.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <future>
#include <thread>

namespace {

template <std::future_status Status, class T>
auto status_is(const std::future<T>& fut) -> bool
{
    return Status == fut.wait_for(std::chrono::seconds{0});
}

}  // namespace

TEST(SharedResource, AccessFromMultipleThreads)
{
    auto x = exclusive::shared_resource<int, exclusive::array_mutex<4>>{};

    const auto inc_n = [&x](std::size_t n) {
        for (std::size_t i = 0U; i != n; ++i) {
            ++(*x.access());
        }
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

TEST(SharedResourceCLhLock, AccessFromMultipleThreads)
{
    auto x = exclusive::shared_resource<int, exclusive::clh_mutex<5>>{};

    const auto inc_n = [&x](std::size_t n) {
        for (std::size_t i = 0U; i != n; ++i) {
            ++(*x.access());
        }
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

TEST(SharedResourceClhLock, ThrowsWhenSlotsExceeded)
{
    // While 3 slots may fail, it is possible that the initial tail may be
    // returned to the free list before the 3rd slot is accessed.
    auto x = exclusive::shared_resource<int, exclusive::clh_mutex<2>>{};

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
