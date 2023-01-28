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
    Philosopher(size_t id, Hand main_hand, std::vector<Event>& events_line, Fork& left_fork, Fork& right_fork) 
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
                }))
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

    static void setFence(int limit) {
        counter = limit;
    }

private:
    void fence() const;

    void thinking() const;
    void dining() const;
    auto take_forks() const -> std::optional<std::pair<Fork::lock_type, Fork::lock_type>>;
    void release_forks(std::pair<Fork::lock_type, Fork::lock_type>&& forks_pair) const;
    void hungry() const;
    
    template<Hand H>
    auto mainHandFork() const -> const Fork&;

    template<Hand H>
    auto otherHandfork() const -> const Fork&;
    
    template<Hand H>
    auto take() const -> Fork::lock_opt;

    template<Hand H>
    auto holding_take(Fork::lock_type&& other_lock) const -> std::optional<std::pair<Fork::lock_type, Fork::lock_type>>;
    
    template <Action action, typename... Args>
    void add_event(Args&&... args) const;

    size_t id;
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
    const auto take_in_main_hand = [&]() {
        if (main_hand == Hand::Left) {
            return take<Hand::Left>();
        } 
        return take<Hand::Right>();
    };

    const auto take_in_next_hand = [&](auto&& lock) {
        if (main_hand == Hand::Left) {
            return holding_take<Hand::Left>(std::move(lock));
        } 
        return holding_take<Hand::Right>(std::move(lock));
    };
    
    return take_in_main_hand()
        .and_then([&](auto&& lock) {
            return take_in_next_hand(std::move(lock));
        });
}

template<Hand H>
auto Philosopher::mainHandFork() const -> const Fork& {
    if (H == Hand::Left) { return left_fork; }
    return right_fork;
}

template<Hand H>
auto Philosopher::otherHandfork() const -> const Fork& {
    if (H == Hand::Left) { return right_fork; }
    return left_fork;
}

template<Hand H>
auto Philosopher::take() const -> Fork::lock_opt {
    constexpr auto action_ignored = [](){
        if constexpr (H == Hand::Left) { return Action::Not_taking_left; } 
        return Action::Not_taking_right;
    } ();

    constexpr auto action_accept = [](){
        if constexpr (H == Hand::Left) { return Action::Taking_left; } 
        return Action::Taking_right;
    } ();

    auto& main_fork = mainHandFork<H>();
    
    if (auto opt_lock = main_fork.try_take()
        .and_then([&](auto&& fork_lock) -> Fork::lock_opt {
             add_event<action_accept>("take ", main_fork);
             return fork_lock;
        })) {
        return opt_lock;
    }

    add_event<action_ignored>("can't take ", main_fork);
    return {};
}

template<Hand H>
auto Philosopher::holding_take(Fork::lock_type&& other_lock) const -> std::optional<std::pair<Fork::lock_type, Fork::lock_type>> {
    constexpr auto action_ignored = [](){
        if constexpr (H == Hand::Left) {
            return Action::Not_taking_right_have_left;
        } 
        return Action::Not_taking_left_have_right;
    } ();

    constexpr auto action_accept = [](){
        if constexpr (H == Hand::Left) {
            return Action::Taking_right_have_left;
        } 
        return Action::Taking_left_have_right;
    } ();

    constexpr auto action_put_back = [](){
        if constexpr (H == Hand::Left) {
            return Action::Put_left;
        } 
        return Action::Put_right;
    } ();

    auto& secondary_fork = otherHandfork<H>();

    if (auto opt_lock_pair = secondary_fork.try_take()
        .and_then([&](auto&& lock) -> std::optional<std::pair<Fork::lock_type, Fork::lock_type>> {
            add_event<action_accept>("take ", secondary_fork);
            return std::make_pair(std::move(other_lock), std::move(lock));
        })) {
        return opt_lock_pair;
    }

    auto& main_fork = mainHandFork<H>();
    
    add_event<action_ignored>("can't take ", secondary_fork);
    other_lock.unlock();
    add_event<action_put_back>("put ", main_fork);
    return {};
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
