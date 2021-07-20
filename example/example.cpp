#include "exclusive/exclusive.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

template <class F, std::size_t... Is>
auto make_array_with_invocable_impl(F&& f, std::index_sequence<Is...>)
{
    return std::array{((void)Is, std::invoke(f))...};
}

template <std::size_t N, class F>
auto make_array_with_invocable(F&& f)
{
    return make_array_with_invocable_impl(std::forward<F>(f), std::make_index_sequence<N>{});
}

}  // namespace

int main()
{
    using namespace std::chrono_literals;
    using clock = std::chrono::steady_clock;

    constexpr auto N = 3;
    constexpr auto TASK_COUNT = 50;
    constexpr auto ENFORCE_FAIRNESS = true;
    static constexpr auto TIMEOUT = 1s;

    std::cout << "running exclusive access example... \n\n";

    std::cout << N << " threads take turns incrementing a shared resource (an int) " << TASK_COUNT
              << " times.\n\n";

    auto resource = exclusive::shared_resource<int, exclusive::clh_mutex<N>>{};

    const auto wait_for_others = [](auto i, const auto& rsc) {
        // Wait for other threads to queue so we can't race around and
        // beat them.
        // Access timeout *should* be large enough that we will always
        // reach the expected count, but this may slow things down.
        const auto deadline = clock::now() + TIMEOUT;
        while ((i != (TASK_COUNT - 1)) && (N != rsc.queue_count())) {
            // If it's taking too long, give up.
            if (clock::now() > deadline) {
                throw std::runtime_error{"Try increasing the timeout duration?"};
            }
        }
    };

    const auto access_and_increment = [&resource, wait_for_others] {
        auto longest_wait = clock::duration{};
        auto prev_count = std::optional<int>{};
        int count = 0;

        for (auto i = 0; i != TASK_COUNT; ++i) {
            const auto wait_start = clock::now();

            if (auto access_scope = resource.access_within(TIMEOUT)) {
                if (ENFORCE_FAIRNESS) {
                    wait_for_others(i, resource);
                }
                longest_wait = std::max(longest_wait, clock::now() - wait_start);

                count = ++*access_scope;
            } else {
                throw std::runtime_error{"Try increasing the timeout duration?"};
            }

            if (ENFORCE_FAIRNESS) {
                if (prev_count && ((count - *prev_count) != N)) {
                    throw std::runtime_error{"My turn got skipped 😞"};
                }
                prev_count = count;
            }
        }

        return longest_wait;
    };

    const auto spawn = [f = access_and_increment] { return std::async(std::launch::async, f); };

    auto i = 0;
    for (auto& task : make_array_with_invocable<N>(spawn)) {
        std::cout << "longest wait for thread " << i++ << " was "
                  << double{clock::period::num} / double{clock::period::den} *
                         double(task.get().count())
                  << " s\n";
    }

    std::cout << "🌈 done -- I counted to " << *resource.access() << "! ✨" << '\n';
    return 0;
}
