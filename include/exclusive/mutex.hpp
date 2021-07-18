#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <new>
#include <system_error>
#include <type_traits>

/// @brief Provides exclusive access to shared resources
namespace exclusive {

inline auto error_on_slots_exceeded()
{
    return std::system_error{std::make_error_code(std::errc::device_or_resource_busy)};
}

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
/// @note Implements Mutex (sortof)
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

    // Slot granted exclusive access
    std::size_t active_{};

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
        auto slot = tail_.fetch_add(1, std::memory_order_relaxed) % N;
        while (!flag_[slot].value.load(std::memory_order_acquire)) {}

        if (flag_[slot].in_use.test_and_set()) {
            throw error_on_slots_exceeded();
        }

        active_ = slot;
    }

    /// Unlocks the mutex
    auto unlock()
    {
        auto slot = active_;

        flag_[slot].value.store(false, std::memory_order_relaxed);
        flag_[(1U + slot) % N].in_use.clear();
        flag_[(1U + slot) % N].value.store(true, std::memory_order_release);
    }

    auto try_lock();
};

/// Tag types for selecting behavior on lock failure
namespace failure {
struct retry {};
struct die {};
}  // namespace failure

/// @brief Mutex implementing a CLH Queue Lock
///
/// @tparam N Number of nodes in the fixed sized pool. Should match the number
///     of concurrent threads accessing the lock. Additional nodes may be used
///     for bookkeeping.
/// @tparam Failure Policy when failing to obtain a node on calling lock. Must
///     be `failure::retry` or `failure::die`.
///
/// Implements a mutex similar to CLH queue lock. This class manages a
/// fixed-size pool of nodes instead of threads allocating a node when locking.
/// A node will be recycled to the available pool of nodes after a thread
/// unlocks.
///
/// @note Implements TimedMutex
template <std::size_t N, class Failure = failure::retry>
class clh_mutex {
    static_assert(N > 0, "Number of nodes must be greater than 0.");

    static_assert(std::disjunction_v<std::is_same<failure::retry, Failure>,
                                     std::is_same<failure::die, Failure>>);

    /// A node queue for a clh_mutex with timeout
    class queue {
      public:
        struct alignas(hardware_destructive_interference_size) node {
            /// Intrusive pointer to the next node. Used while a node is available.
            std::atomic<node*> next{};

            /// The predecessor to wait on. Set if node is abandoned due to timeout.
            node* pred{};

            /// Set if a thread is intending to acquire the lock
            std::atomic_bool locked{};
        };

        /// Construct a queue, initializing with nodes from a separate pool
        queue(node* first, node* last)
        {
            assert(first != last);
            assert(first != nullptr);

            head_.store(first, std::memory_order_relaxed);

            auto* prev = first;
            while (++first != last) {
                prev->next = first;
                prev = first;
            }

            prev->next = nullptr;
            tail_.store(prev, std::memory_order_relaxed);
        }

        auto push(node* new_tail) -> void
        {
            new_tail->next.store(nullptr, std::memory_order_relaxed);

            // No other threads can push without holding the lock.
            // How strong does the memory order need to be?
            auto* t = tail_.load(std::memory_order_relaxed);

            // Spurious failure is not okay here
            auto ok = tail_.compare_exchange_strong(
                t, new_tail, std::memory_order_relaxed, std::memory_order_relaxed);
            assert(ok);

            // (Q1) update old tail to point to the new tail
            // synchronizes with (Q3)
            t->next.store(new_tail, std::memory_order_release);
        }

        auto try_pop() -> node*
        {
            // (Q2) grab the head node
            // synchronizes with (Q4)
            auto* h = head_.load(std::memory_order_acquire);

            for (;;) {
                // (Q3) if next is empty, give up
                // synchronizes with (Q1)
                auto* next = h->next.load(std::memory_order_acquire);
                if (next == nullptr) {
                    return nullptr;
                }

                // (Q4) update head
                // synchronizes with (Q2)
                if (head_.compare_exchange_weak(
                        h, next, std::memory_order_release, std::memory_order_acquire)) {
                    break;
                }
            }

            return h;
        }

      private:
        alignas(hardware_destructive_interference_size) std::atomic<node*> head_{};
        alignas(hardware_destructive_interference_size) std::atomic<node*> tail_{};
    };

    // Pool of nodes for the mutex queue
    // Adds 1 to start in the tail, 1 as the queue sentinel, leaving N available
    // nodes for threads.
    std::array<typename queue::node, N + 2> node_storage_{};

    queue available_;

    alignas(hardware_destructive_interference_size) std::atomic<typename queue::node*> tail_{};

    // Node granted exclusive access
    typename queue::node* active_;

    // Number of times a node has been acquired (thread has queued for the lock)
    std::atomic_uint queue_count_{};

  public:
    clh_mutex() : available_(node_storage_.begin(), node_storage_.end())
    {
        queue_count_.store(0, std::memory_order_relaxed);

        auto* n = available_.try_pop();
        assert(n != nullptr);

        n->locked.store(false, std::memory_order_relaxed);
        tail_.store(n, std::memory_order_relaxed);
    }

    ~clh_mutex() = default;

    clh_mutex(const clh_mutex&) = delete;
    clh_mutex(clh_mutex&&) = delete;
    auto operator=(const clh_mutex&) -> clh_mutex& = delete;
    auto operator=(clh_mutex&&) -> clh_mutex& = delete;

    auto lock()
    {
        static constexpr auto years = std::chrono::hours{24 * 365};
        assert(try_lock_for(10 * years));
    }

    auto try_lock() -> bool { return try_lock_for(std::chrono::seconds{0}); }

    template <class Rep, class Period>
    auto try_lock_for(const std::chrono::duration<Rep, Period>& duration) -> bool
    {
        return try_lock_until(std::chrono::steady_clock::now() + duration);
    }

    template <class Clock, class Duration>
    auto try_lock_until(const std::chrono::time_point<Clock, Duration>& deadline) -> bool
    {
        auto* n = try_pop_node_until(deadline);
        if (n == nullptr) {
            return false;
        }

        // signal intent to acquire lock
        n->locked.store(true, std::memory_order_relaxed);

        // (C1) grab predecessor
        // synchronizes with (C2)
        auto* pred = tail_.load(std::memory_order_acquire);

        // (C2) swap predecessor with self, becoming the predecessor for the
        // next thread
        // synchronizes with (C1)
        while (!tail_.compare_exchange_weak(
            pred, n, std::memory_order_release, std::memory_order_acquire)) {
            if (Clock::now() >= deadline) {
                return false;
            }
        }

        // (X1) increase counter for observation in tests
        // synchronizes with (X2)
        queue_count_.fetch_add(1, std::memory_order_release);

        for (;;) {
            // (C3) spin on predecessor until the lock is released
            // synchronizes with (C4),(C5)
            while (pred->locked.load(std::memory_order_acquire)) {
                if (Clock::now() >= deadline) {
                    // propagate the predecessor to denote abandonment
                    n->pred = pred;

                    // (C4) release lock
                    // synchronizes with (C3)
                    n->locked.store(false, std::memory_order_release);
                    return false;
                }
            }

            // save pred's pred in case it needs to be waited upon
            auto* abandonned = pred->pred;

            // recycle the predecessor node
            available_.push(pred);

            // check if pred was abandonned due to timeout
            if (abandonned) {
                pred = abandonned;
            } else {
                break;
            }
        }

        active_ = n;
        return true;
    }

    auto unlock()
    {
        // clear the predecessor, no timeout here
        active_->pred = nullptr;

        // (C5) release lock
        // synchronizes with (C3)
        active_->locked.store(false, std::memory_order_release);
    }

    // Number of times a thread has requested a lock and queued up
    // NOTE: This only exists for testing fairness
    [[nodiscard]] auto queue_count() const -> unsigned int
    {
        // (X2) load queue count
        // synchronizes with (X1)
        return queue_count_.load(std::memory_order_acquire);
    }

  private:
    template <class Clock, class Duration>
    auto try_pop_node_until(const std::chrono::time_point<Clock, Duration>& deadline)
    {
        auto* n = available_.try_pop();

        while ((n == nullptr) && (Clock::now() < deadline)) {
            // This can fail due to ABA - if after popping the head, but before
            // loading head->next, the entire queue gets popped/pushed by other
            // threads.
            // This could be resolved by DCAS but there are no standard library
            // functions to use that and requires more implementation work.
            if (std::is_same_v<failure::die, Failure>) {
                throw error_on_slots_exceeded();
            }
            n = available_.try_pop();
        }

        return n;
    }
};

}  // namespace exclusive
