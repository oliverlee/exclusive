#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <mutex>
#include <new>
#include <system_error>
#include <type_traits>

/// @brief Provides exclusive access to shared resources
namespace exclusive {

auto error_on_slots_exceeded()
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
        while (!flag_[slot].value.load(std::memory_order_acquire)) {}

        if (flag_[slot].in_use.test_and_set()) {
            throw error_on_slots_exceeded();
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


class queue {
  public:
    struct alignas(hardware_destructive_interference_size) node {
        std::atomic_bool locked{};
        std::atomic<node*> next{};
    };

    /// Construct a queue, initializing with nodes from a separate pool
    queue(node* first, node* last);

    auto push(node* new_tail) -> void;
    auto try_pop() -> node*;

  private:
    alignas(hardware_destructive_interference_size) std::atomic<node*> head_{};
    alignas(hardware_destructive_interference_size) std::atomic<node*> tail_{};
};

queue::queue(queue::node* first, queue::node* last)
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

auto queue::push(queue::node* new_tail) -> void
{
    new_tail->next.store(nullptr, std::memory_order_relaxed);

    // No other threads can push without holding the lock.
    // How strong does the memory order need to be?
    auto* t = tail_.load(std::memory_order_relaxed);

    auto ok = tail_.compare_exchange_weak(
        t, new_tail, std::memory_order_relaxed, std::memory_order_relaxed);
    assert(ok);

    // (Q1) update old tail to point to the new tail
    // synchronizes with (Q3)
    t->next.store(new_tail, std::memory_order_release);
}

auto queue::try_pop() -> node*
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


template <std::size_t N>
class clh_mutex {
    static_assert(N > 1, "");

    std::array<queue::node, N> node_storage_{};

    queue available_;

    std::atomic<queue::node*> tail_{};

    // Slot that each thread spins on.
    // FIXME Use of `thread_local` prevents more than one instance of an clh_mutex<N>.
    thread_local static queue::node* slot;

  public:
    clh_mutex() : available_(node_storage_.begin(), node_storage_.end())
    {
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
        // get a node
        auto* n = available_.try_pop();
        if (n == nullptr) {
            throw error_on_slots_exceeded();
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
            pred, n, std::memory_order_release, std::memory_order_acquire)) {}

        // (C3) spin on predecessor until the lock is released
        // synchronizes with (C4)
        while (pred->locked.load(std::memory_order_acquire)) {}

        // recycle the predecessor node
        available_.push(pred);

        slot = n;
    }

    auto unlock()
    {
        // (C4) release lock
        // synchronizes with (C3)
        slot->locked.store(false, std::memory_order_release);
    }

    auto try_lock();
};

template <std::size_t N>
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local typename queue::node* clh_mutex<N>::slot;


}  // namespace exclusive
