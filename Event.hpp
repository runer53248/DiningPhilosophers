#pragma once
#include <sstream>
#include <chrono>

enum class Action;

struct Event {
    size_t philosopher_id;
    Action action;
    std::chrono::nanoseconds time;
    std::stringstream text{};
};
