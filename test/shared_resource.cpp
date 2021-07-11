#include "exclusive/exclusive.hpp"

#include "gtest/gtest.h"
#include <cstddef>
#include <thread>

// Add a test that should obviously trigger a failure from tsan
TEST(SharedResource, AccessFromMultipleThreads)
{
    auto x = exclusive::shared_resource<int>{};

    const auto inc_n = [&resource = x.resource](std::size_t n) {
        for (std::size_t i = 0U; i != n; ++i) {
            ++resource;
        }
    };

    auto t1 = std::thread{inc_n, 100U};
    auto t2 = std::thread{inc_n, 100U};
    auto t3 = std::thread{inc_n, 100U};

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(300U, x.resource);
}
