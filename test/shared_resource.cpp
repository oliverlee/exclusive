#include "exclusive/exclusive.hpp"

#include "gtest/gtest.h"
#include <cstddef>
#include <thread>

TEST(SharedResource, AccessFromMultipleThreads)
{
    auto x = exclusive::shared_resource<int, 4>{};

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
