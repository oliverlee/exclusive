#include "exclusive/exclusive.hpp"

#include "gtest/gtest.h"

TEST(SharedResource, Dummy)
{
    auto x = exclusive::shared_resource<int>{};
    (void)x;
}
