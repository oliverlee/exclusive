#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ratio>

// Test utilities
namespace exclusive::test {

/// @brief A clock that allows time travel
///
/// A clock initialized to time 0 and which only changes with calls to `set_now`.
class fake_clock {
  public:
    using rep = std::intmax_t;
    using period = std::nano;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<fake_clock>;

    static constexpr bool is_steady = false;

    /// @brief Gets the current time
    static auto now() noexcept -> time_point
    {
        // (T1)
        // synchronizes with (T2)
        return now_.load(std::memory_order_acquire);
    }

    /// @brief Sets the current time
    static auto set_now(time_point now) noexcept -> void
    {
        // (T2)
        // synchronizes with (T1)
        // NOTE: Probably don't call this from multiple threads
        now_.store(now, std::memory_order_release);
    }

  private:
    static std::atomic<time_point> now_;
};

}  // namespace exclusive::test
