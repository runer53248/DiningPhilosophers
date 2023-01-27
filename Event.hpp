#pragma once
#include <sstream>
#include <chrono>

enum class Action;

struct Event {
    int philosopher_id;
    Action action;
    std::chrono::nanoseconds time;
    std::stringstream text{};
};
