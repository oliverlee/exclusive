#include "gtest/gtest.h"
#include "exclusive/exclusive.hpp"

TEST(SharedResource, Dummy) {
  auto x = exclusive::shared_resource<int>{};
  (void)x;
}
