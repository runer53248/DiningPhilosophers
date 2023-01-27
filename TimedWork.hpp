#pragma once
#include <mutex>
#include <chrono>
#include <random>
#include <thread>

namespace {
std::mutex time_mt;

std::random_device r;
std::default_random_engine e1(r());
}

inline auto get_pased_duration() {
    std::lock_guard lock{time_mt};
    static auto start_time = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start_time);
}

inline auto get_time() {
    std::lock_guard lock{time_mt};
    return std::chrono::steady_clock::now();
}

inline auto random(int minimum, int maximum) {
    std::uniform_int_distribution<int> uniform_dist(minimum, maximum);
    return uniform_dist(e1);
}

struct TimedWork {
    using time_point = std::chrono::steady_clock::time_point;
    using time_tuple = std::tuple<time_point, time_point, time_point>;

    TimedWork(int minimum = 50, int maximum = 200) : duration(random(minimum, maximum)) {}

    time_tuple work() const;
    time_tuple busy_sleep() const;

    std::chrono::microseconds duration;
};

TimedWork::time_tuple TimedWork::work() const {
    return busy_sleep();
}

TimedWork::time_tuple TimedWork::busy_sleep() const {
    const auto start = get_time();
    const auto end = start + duration;

    while (get_time() < end) {
        std::this_thread::yield();
    };
    
    return std::make_tuple(start, end, get_time());
}

std::ostream& operator<<(std::ostream& out, const TimedWork& work) {
    out << "for duration: " << work.duration.count() << " us ";
    return out;
}

std::ostream& operator<<(std::ostream& out, const TimedWork::time_tuple& times) {
    const auto start_time = std::get<0>(times);
    const auto end_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::get<1>(times) - start_time).count();
    const auto real_end_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::get<2>(times) - start_time).count();
    
    out << "time: " << static_cast<double>(end_time) / 1000.0 << " us - ";
    out << static_cast<double>(real_end_time) / 1000.0 << " us";
    return out;
}
