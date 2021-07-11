#pragma once

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

// Apple Clang =/
// https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size
#if defined(__cpp_lib_hardware_interference_size) && !defined(__APPLE__)
using std::hardware_destructive_interference_size;
#else
// 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
constexpr std::size_t hardware_destructive_interference_size = 2 * sizeof(std::max_align_t);
#endif

/// @brief Array-based queue mutex
/// @tparam N Number of slots
///
/// @note Implements Mutex
template <std::size_t N>
class array_mutex {
    static_assert((std::size_t(-1) % N) == (N - 1U), "N must be a power of 2.");

    struct alignas(hardware_destructive_interference_size) cache_bool {
        std::atomic_bool value{};
        std::atomic_flag in_use{};
    };
    std::array<cache_bool, N> flag_{};

    // Tracks the last taken slot.
    // NOTE: Allowed to exceed number of slots to remove the need to CAS. Modulo
    // must be performed before indexing `flag_`.
    std::atomic_size_t tail_{};

    // Slot that each thread spins on.
    // FIXME Use of `thread_local` prevents more than one instance of an array_mutex<N>.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    thread_local static std::size_t slot;

  public:
    array_mutex()
    {
        // I guess everything needs to be explicitly initialized?
        // wg21.link/p0883
        tail_.store(0U, std::memory_order_relaxed);

        std::for_each(flag_.begin() + 1, flag_.end(), [](auto& f) {
            f.in_use.clear(std::memory_order_relaxed);
            f.value.store(false, std::memory_order_relaxed);
        });

        flag_[0].in_use.clear(std::memory_order_relaxed);
        flag_[0].value.store(true, std::memory_order_release);
    }

    ~array_mutex() = default;

    array_mutex(const array_mutex&) = delete;
    array_mutex(array_mutex&&) = delete;
    auto operator=(const array_mutex&) -> array_mutex& = delete;
    auto operator=(array_mutex&&) -> array_mutex& = delete;

    /// Locks the mutex, blocking until the mutex is available
    /// @throws `std::system_error` when locking and N slots are already taken.
    // TODO handle timeout?
    auto lock()
    {
        slot = tail_.fetch_add(1, std::memory_order_relaxed) % N;
        while (!flag_[slot].value.load(std::memory_order_acquire)) {
        }

        if (flag_[slot].in_use.test_and_set()) {
            throw std::system_error{std::make_error_code(std::errc::device_or_resource_busy)};
        }
    }

    /// Unlocks the mutex
    auto unlock()
    {
        flag_[slot].value.store(false, std::memory_order_relaxed);
        flag_[(1U + slot) % N].in_use.clear();
        flag_[(1U + slot) % N].value.store(true, std::memory_order_release);
    }

    auto try_lock();
};

template <std::size_t N>
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local std::size_t array_mutex<N>::slot;

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
