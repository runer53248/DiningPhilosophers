#include "Philosopher.hpp"
#include "PrintEvents.hpp"
#include <thread>
#include <array>
#include <algorithm>
#include <list>
#include <ranges>

using namespace std::chrono_literals;

constexpr auto philosophers_num = 5;
constexpr auto run_times = 2;
constexpr auto philosophers_eating_times_count = 300;
std::chrono::milliseconds print_delay = 0ms;
bool print_color_by_philosopher = false; // limited to 6 philosophers - enum Color limit
bool print_reset_color_after = false;
constexpr auto print_all = false;
constexpr auto print_part_range = 200;

const size_t Philosopher::eating_times_count = philosophers_eating_times_count;
const int Philosopher::eating_time_minimum = 50;
const int Philosopher::eating_time_maximum = 200;
const int Philosopher::thinking_time_minimum = 50;
const int Philosopher::thinking_time_maximum = 200;

int main() {
    static_assert(philosophers_num > 1);

    std::array<Fork, philosophers_num> forks;
    std::array<std::vector<Event>, philosophers_num> events_lines;
    std::array<std::thread, philosophers_num> threads;

    for (int times = 0; times < run_times; ++times) {
        for(auto& events : events_lines) { // clear events between runs so only last one run will be printed
            events.clear();
        }

        Philosopher::setFence(philosophers_num);
        static auto it = forks.begin();
        it = forks.begin();

        for (size_t philosopher_id = 0; philosopher_id < philosophers_num; ++philosopher_id) {
            const auto first_fork = std::ref(*it);
            std::advance(it, 1);
            if (it == forks.end()) {
                it = forks.begin();
            }
            const auto second_fork = std::ref(*it);

            Hand hand = (philosopher_id == philosophers_num - 1) ? Hand::Right : Hand::Left;
            threads[philosopher_id] = std::thread(Philosopher{philosopher_id, hand, events_lines[philosopher_id], first_fork, second_fork});
        }

        for (auto&& thread : threads) {
            thread.join();
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////// thread work end here

    auto all_events = [&events_lines]() {
        auto count_events = [&]() {
            size_t size_of_all{};
            for (auto&& container : events_lines) {
                size_of_all += container.size();
            }
            return size_of_all;
        };

        std::vector<Event> events;
        events.reserve(count_events());

        std::ranges::for_each(events_lines, [&](auto& container) {
            std::ranges::move(container, std::back_inserter(events));
        });
        std::ranges::stable_sort(events, [](const auto& lhs, const auto& rhs) {
            if (lhs.time == rhs.time) {
                return lhs.action < rhs.action; // want order of actions to be same as order in enum 
            }
            return lhs.time < rhs.time;
        });
        return events;
    } ();
    
    if (print_all || (print_part_range > all_events.size()/2)) {
        print_events<philosophers_num>(all_events);
    } else {
#ifndef __clang__
        print_events<philosophers_num>(all_events | std::views::take(print_part_range));
        std::cout << "\n  .....\n\n";
        print_events<philosophers_num>(all_events | std::views::drop(all_events.size() - print_part_range));
#else
        std::cout << "INFO: Printing parts of events feature not supported yet (no support in clang 15).\n";
#endif
    }

    auto count_events = [] (Action A) {
        return [A](const Event& event) {
            return event.action == A;
        };
    };

    std::cout << "\nPhilosophers count: " << philosophers_num << "\n";
    std::cout << "Philosophers eating times: " << philosophers_eating_times_count << "\n";
    std::cout << "Run times: " << run_times << "\n";

    auto passed_time = (all_events.back().time - all_events.front().time).count();
    std::cout << "\ntotal time of last run : " << static_cast<double>(passed_time) / 1000.0 << " us\n";

    auto hungry_times = std::ranges::count_if(all_events, count_events(Action::Starve));
    std::cout << "\ntotal starvation count: " << hungry_times << "\n";

    auto thinking_times = std::ranges::count_if(all_events, count_events(Action::Thinking));
    std::cout << "total thinking count: " << thinking_times << "\n";

    auto dining_times = std::ranges::count_if(all_events, count_events(Action::Dining));
    std::cout << "total dining count: " << dining_times << "\n\n";
}
