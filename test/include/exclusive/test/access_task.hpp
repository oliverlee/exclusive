#pragma once

#include <cassert>
#include <chrono>
#include <future>
#include <mutex>

// Test utilities
namespace exclusive::test {

/// @brief Check the status of a future
template <std::future_status Status, class T>
auto status_is(const std::future<T>& fut) -> bool
{
    using namespace std::literals::chrono_literals;
    return Status == fut.wait_for(0s);
}

/// @brief A task that simplifies to acquiring access to a mutex
template <class Mutex>
class AccessTask {
    std::promise<void> access_signal_;
    std::future<void> access_fut_;
    std::promise<void> terminate_signal_;
    std::future<bool> task_;

  public:
    explicit AccessTask(Mutex& mut)
        : AccessTask{mut, (std::chrono::steady_clock::now() + std::chrono::hours{24})}
    {}

    template <class Clock, class Duration>
    AccessTask(Mutex& mut, const std::chrono::time_point<Clock, Duration>& deadline)
        : access_signal_{},
          access_fut_{access_signal_.get_future()},
          terminate_signal_{},
          task_{std::async(std::launch::async, [this, &mut, deadline] {
              if (auto access_scope = std::unique_lock{mut, deadline}) {
                  access_signal_.set_value();
                  terminate_signal_.get_future().wait();
                  return true;
              }

              return false;
          })}
    {}

    /// @brief Check the status of the task
    template <std::future_status Status>
    auto status_is() const -> bool
    {
        return exclusive::test::status_is<Status>(task_);
    }

    /// @brief Block until the task completes
    /// @return `true` is access was acquired, otherwise `false`
    auto get() -> bool { return task_.get(); }

    /// @brief Signal termination and block until the task completes
    /// @return `true` is access was acquired, otherwise `false`
    /// @pre Task has exclusive access to resource
    auto terminate() -> bool
    {
        assert(has_access());
        terminate_signal_.set_value();

        return get();
    }

    /// @brief Check if the task has exclusive access
    auto has_access() const -> bool
    {
        return exclusive::test::status_is<std::future_status::ready>(access_fut_);
    }

    /// @brief Block until the task acquires exclusive access
    auto wait_until_access() const -> void { return access_fut_.wait(); }
};

template <class Mutex, class Clock, class Duration>
AccessTask(Mutex&, const std::chrono::time_point<Clock, Duration>&) -> AccessTask<Mutex>;

}  // namespace exclusive::test
