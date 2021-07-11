#pragma once

#include "mutex.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <new>
#include <system_error>
#include <type_traits>

/// @brief Provides exclusive access to shared resources
namespace exclusive {

template <class T, class Mutex>
class shared_resource;

/// @brief Scoped access token for a shared resource
/// @tparam T Resource type
/// @tparam Mutex Mutex type (except `try_lock()` isn't necessary)
///
/// Wrapper type providing RAII mechanism for access to a shared resource.
/// On creation, acquires ownership of a mutex. On destruction, releases the
/// mutex.
///
/// This type is only intended to be created by a `shared_resource<T>`.
template <class T, class Mutex>
class scoped_access {
    T& resource_;
    std::scoped_lock<Mutex> lock_;

    friend class shared_resource<T, Mutex>;
    scoped_access(T& r, Mutex& m) : resource_{r}, lock_{m} {}

  public:
    ~scoped_access() = default;

    scoped_access(const scoped_access&) = delete;
    scoped_access(scoped_access&&) = delete;
    auto operator=(const scoped_access&) -> scoped_access& = delete;
    auto operator=(scoped_access&&) -> scoped_access& = delete;

    /// @brief Access the shared resource
    ///
    /// Users are expected *not* to store the underlying reference.
    [[nodiscard]] auto operator*() const noexcept -> T& { return resource_; }
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
};

}  // namespace exclusive
