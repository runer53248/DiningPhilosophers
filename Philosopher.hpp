#pragma once
#include "Fork.hpp"
#include "Event.hpp"
#include "TimedWork.hpp"
#include <vector>
#include <thread>

enum class Action {
    None,
    Thinking,
    End_thinking,
    Taking_left,
    Not_taking_left,
    Taking_right,
    Not_taking_right,
    Taking_left_have_right,
    Taking_right_have_left,
    Not_taking_left_have_right,
    Not_taking_right_have_left,
    Dining,
    End_dining,
    Put_left_have_right,
    Put_right_have_left,
    Put_left,
    Put_right,
    Starve,
    Finish
};

enum class Hand {
    Left,
    Right
};

template<int philosophers_num>
void print_events(const auto& all_events);

struct Philosopher {
    Philosopher(int id, Hand main_hand, std::vector<Event>& events_line, Fork& left_fork, Fork& right_fork) 
        : id{id}, main_hand{main_hand}, left_fork{left_fork}, right_fork{right_fork}, events_line{events_line} {}

    void operator()() const {
        fence();

        thinking(); // thinking before dining
        for (size_t i = 0; i < eating_times_count; ++i) {
            while (not take_forks()
                .and_then([&](auto&& lock_pair) -> std::optional<bool> {
                    dining();
                    release_forks(std::move(lock_pair));
                    return true;
                })
                .value_or(false))
            {
                hungry();
                thinking(); // thinking when can't dining
            }
            thinking(); // thinking after dining
        }
        add_event<Action::Finish>("finish");
    }

    static constexpr auto getMaxEatingTimes() {
        static_assert(std::is_constant_evaluated());
        return eating_times_count;
    }

    static void setFence(int limit);

private:
    void fence() const;

    void thinking() const;
    void dining() const;
    auto take_forks() const -> std::optional<std::pair<Fork::lock_type, Fork::lock_type>>;
    void release_forks(std::pair<Fork::lock_type, Fork::lock_type>&& forks_pair) const;
    void hungry() const;
    
    template<Hand H>
    auto take() const -> Fork::lock_opt;

    template<Hand H>
    auto holding_take(Fork::lock_type&& other_lock) const -> std::optional<std::pair<Fork::lock_type, Fork::lock_type>>;

    template <Action action, typename... Args>
    void add_event(Args&&... args) const;

    int id;
    Hand main_hand;
    Fork& left_fork;
    Fork& right_fork;

    std::vector<Event>& events_line;

    static const size_t eating_times_count;
    static const int eating_time_minimum;
    static const int eating_time_maximum;
    static const int thinking_time_minimum;
    static const int thinking_time_maximum;
    static std::atomic_int counter;
};

std::atomic_int Philosopher::counter = 0;

void Philosopher::setFence(int limit) {
    counter = limit;
}

void Philosopher::fence() const {
    if (counter <= 0) {
        throw std::logic_error("Philosopher fence limit reach too low value.\n");
    }

    --counter;
    while (counter) {
        std::this_thread::yield();
    }
}

void Philosopher::thinking() const {
    TimedWork thinking_time{thinking_time_minimum, thinking_time_maximum};
    add_event<Action::Thinking>("thinking ", thinking_time);

    auto time_pair = thinking_time.work();

    add_event<Action::End_thinking>("finish thinking(", time_pair, ")");
}

void Philosopher::dining() const {
    thread_local int ate_counter{};
    TimedWork eating_time{eating_time_minimum, eating_time_maximum};
    add_event<Action::Dining>("dining(times: ", ++ate_counter, ") ", eating_time);

    auto time_pair = eating_time.work();

    add_event<Action::End_dining>("finish dining(", time_pair, ")");
}

void Philosopher::hungry() const {
    thread_local int starve_counter{};
    add_event<Action::Starve>("hungry(times: ", ++starve_counter, ") ");
}

auto Philosopher::take_forks() const -> std::optional<std::pair<Fork::lock_type, Fork::lock_type>> {
    if (main_hand == Hand::Left) {
        return take<Hand::Left>()
            .and_then([&](auto&& left_lock){
                return holding_take<Hand::Right>(std::move(left_lock));
            });
    }
    return take<Hand::Right>()
        .and_then([&](auto&& right_lock){
            return holding_take<Hand::Left>(std::move(right_lock));
        });
}

template<Hand H>
auto Philosopher::take() const -> Fork::lock_opt {
    const auto& fork = [&]() -> const Fork& {
        if constexpr (H == Hand::Left) { return left_fork; }
        else { return right_fork; }
    } ();

    auto opt_lock = fork.try_take().or_else([&]() -> Fork::lock_opt {
        if constexpr (H == Hand::Left) {
            add_event<Action::Not_taking_left>("can't take ", fork);
        } else {
            add_event<Action::Not_taking_right>("can't take ", fork);
        }
        return {};
    });

    if (opt_lock.has_value()) {
        if constexpr (H == Hand::Left) {
            add_event<Action::Taking_left>("take ", fork);
        } else {
            add_event<Action::Taking_right>("take ", fork);
        }
    }

    return opt_lock;
}

template<Hand H>
auto Philosopher::holding_take(Fork::lock_type&& other_lock) const -> std::optional<std::pair<Fork::lock_type, Fork::lock_type>> {
    const auto& fork = [&]() -> const Fork& {
        if constexpr (H == Hand::Left) { return left_fork; }
        else { return right_fork; }
    } ();

    const auto& other_fork = [&]() -> const Fork& {
        if constexpr (H == Hand::Left) { return right_fork; }
        else { return left_fork; }
    } ();

    auto opt_lock_pair = fork.try_take().and_then([&](auto&& lock) -> std::optional<std::pair<Fork::lock_type, Fork::lock_type>> {
        if constexpr (H == Hand::Right) {
            add_event<Action::Taking_right_have_left>("take ", fork);
        } else {
            add_event<Action::Taking_left_have_right>("take ", fork);
        }
        return std::make_pair(std::move(other_lock), std::move(lock));
    });

    if (not opt_lock_pair) {
        if constexpr (H == Hand::Right) {
            add_event<Action::Not_taking_right_have_left>("can't take ", fork);
            other_lock.unlock();
            add_event<Action::Put_left>("put ", other_fork);
        } else {
            add_event<Action::Not_taking_left_have_right>("can't take ", fork);
            other_lock.unlock();
            add_event<Action::Put_right>("put ", other_fork);
        }
    }

    return opt_lock_pair;
}

void Philosopher::release_forks(std::pair<Fork::lock_type, Fork::lock_type>&& forks_pair) const {
    forks_pair.first.unlock();
    add_event<Action::Put_left_have_right>("put ", left_fork);
    forks_pair.second.unlock();
    add_event<Action::Put_right>("put ", right_fork);
}

template <Action action, typename... Args>
void Philosopher::add_event(Args&&... args) const {
    const auto time = get_pased_duration();
    std::stringstream out;

    out << "Philosopher<" << id << "> ";
    ((out << std::forward<Args>(args)), ...);
    // out << ' ';

    events_line.push_back(
        Event{
            .philosopher_id = id,
            .action = action,
            .time = time,
            .text = std::move(out)
        }
    );
}
