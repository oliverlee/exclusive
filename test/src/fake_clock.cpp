#include "exclusive/test/fake_clock.hpp"

#include <atomic>

namespace exclusive::test {
std::atomic<fake_clock::time_point> fake_clock::now_{fake_clock::time_point{}};
}  // namespace exclusive::test
