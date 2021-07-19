#pragma once

#include "mutex.hpp"

#include <cassert>
#include <chrono>
#include <mutex>
#include <type_traits>

/// @brief Provides exclusive access to shared resources
namespace exclusive {

template <class T, class Mutex>
class shared_resource;

/// @brief Scoped access token for a shared resource
/// @tparam T Resource type
/// @tparam Mutex Mutex type
///
/// Wrapper type providing RAII mechanism for access to a shared resource.
/// On creation, attempts to acquire ownership of a mutex. On destruction,
/// releases the mutex if it was acquired.
///
/// This type is only intended to be created by a `shared_resource<T>`.
template <class T, class Mutex>
class scoped_access {
    std::unique_lock<Mutex> lock_;
    T* resource_;

    friend class shared_resource<T, Mutex>;

    template <class... LockArgs>
    scoped_access(T& r, Mutex& m, LockArgs&&... lock_args)
        : lock_{m, std::forward<LockArgs>(lock_args)...}, resource_{lock_ ? &r : nullptr}
    {}

  public:
    ~scoped_access() = default;

    scoped_access(const scoped_access&) = delete;
    scoped_access(scoped_access&&) = delete;
    auto operator=(const scoped_access&) -> scoped_access& = delete;
    auto operator=(scoped_access&&) -> scoped_access& = delete;

    /// @{
    /// @brief Checks whether `*this` acquired access
    [[nodiscard]] auto owns_lock() const noexcept -> bool { return lock_.owns_lock(); }
    [[nodiscard]] explicit operator bool() const noexcept { return owns_lock(); }
    /// @}


    /// @brief Access the shared resource
    /// @pre `owns_lock()` returns `true`
    ///
    /// Users are expected *not* to store the underlying reference.
    [[nodiscard]] auto operator*() const -> T&
    {
        assert(*this);
        return *resource_;
    }
};

/// @brief A shared resource with synchronized access
/// @tparam T Resource type
/// @tparam Mutex Mutex type (except `try_lock()` isn't necessary)
template <class T, class Mutex = std::mutex>
class shared_resource {
    static_assert(std::is_object_v<T>);
    static_assert(std::is_default_constructible_v<T>);

    T resource_{};
    Mutex mutex_{};

  public:
    using resource_type = T;
    using mutex_type = Mutex;

    /// @brief Constructs a shared resource using the type's default constructor
    shared_resource() = default;
    ~shared_resource() = default;

    shared_resource(const shared_resource&) = delete;
    shared_resource(shared_resource&&) = delete;
    auto operator=(const shared_resource&) -> shared_resource& = delete;
    auto operator=(shared_resource&&) -> shared_resource& = delete;

    /// @brief Acquire access to the shared resource
    /// @return A scoped_access token
    [[nodiscard]] auto access() -> scoped_access<T, Mutex> { return {resource_, mutex_}; }

    /// @brief Acquire access to the shared resource within a timeout
    /// @tparam Rep Duration representation type
    /// @tparam Period Duration period type
    /// @param duration Elapsed time to wait for
    /// @return An optional containing a scoped_access token on success, an empty optional on
    /// failure
    ///
    /// Attempts to acquire exclusive access within a duration, with respect to
    /// `std::chrono::steady_clock`. Blocks until the specified duration has
    /// elapsed or until access is acquired, whichever comes first.
    template <class Rep, class Period>
    [[nodiscard]] auto access_within(const std::chrono::duration<Rep, Period>& duration)
        -> scoped_access<T, Mutex>
    {
        return {resource_, mutex_, duration};
    }
};

}  // namespace exclusive
