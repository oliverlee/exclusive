#include "exclusive/exclusive.hpp"

#include "gtest/gtest.h"
#include <chrono>

// Given a clh_mutex,
// When there is an uncontested lock request,
// Then it should succeed with non-positive durations.
TEST(ClhLock, TryLockForNonPositiveDuration)
{
    using namespace std::literals::chrono_literals;

    auto mut = exclusive::clh_mutex<1>{};

    // No contention so both calls to `try_lock_for` should succeed
    EXPECT_TRUE(mut.try_lock_for(0s));
    mut.unlock();

    EXPECT_TRUE(mut.try_lock_for(-1s));
    mut.unlock();
}
