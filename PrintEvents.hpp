#pragma once
#include <map>
#include <array>
#include <iostream>
#include <thread>

const std::map<Action, std::string> action_draws {
    {Action::Thinking,                   "  T  "},
    {Action::Dining,                     " |D| "},
    {Action::End_thinking,               "  E  "},
    {Action::End_dining,                 " |E| "},
    {Action::Taking_left,                "|>.  "},
    {Action::Taking_right,               "  .<|"},
    {Action::Taking_left_have_right,     "|>.| "},
    {Action::Taking_right_have_left,     " |.<|"},
    {Action::Not_taking_left,            " _.  "},
    {Action::Not_taking_right,           "  ._ "},
    {Action::Not_taking_left_have_right, " _.| "},
    {Action::Not_taking_right_have_left, " |._ "},
    {Action::Put_left,                   "|<.  "},
    {Action::Put_right,                  "  .>|"},
    {Action::Put_left_have_right,        "|<.| "},
    {Action::Put_right_have_left,        " |.>|"},
    {Action::None,                       "  o  "},
    {Action::Starve,                     "  X  "},
    {Action::Finish,                     "     "}
};

constexpr auto middle_char_index = 2u;
constexpr auto before_char_index = 1u;
constexpr auto after_char_index = 3u;

enum class Color {
    Red,
    Green,
    Blue,
    Yelow,
    White,
    Reset
};

const std::map<Color, std::string> colors {
    {Color::Red,    "\033[1;31m"},
    {Color::Green,  "\033[1;32m"}, 
    {Color::Yelow,  "\033[1;33m"},
    {Color::Blue,   "\033[1;34m"},
    {Color::White,  "\033[1;37m"},
    {Color::Reset,  "\033[0m"}
};

const std::map<Action, Color> action_colors {
    {Action::Thinking,                   Color::Yelow},
    {Action::Dining,                     Color::Green},
    {Action::End_thinking,               Color::White},
    {Action::End_dining,                 Color::White},
    {Action::Taking_left,                Color::Blue},
    {Action::Taking_right,               Color::Blue},
    {Action::Taking_left_have_right,     Color::Blue},
    {Action::Taking_right_have_left,     Color::Blue},
    {Action::Not_taking_left,            Color::Red},
    {Action::Not_taking_right,           Color::Red},
    {Action::Not_taking_left_have_right, Color::Red},
    {Action::Not_taking_right_have_left, Color::Red},
    {Action::Put_left,                   Color::Blue},
    {Action::Put_right,                  Color::Blue},
    {Action::Put_left_have_right,        Color::Blue},
    {Action::Put_right_have_left,        Color::Blue},
    {Action::None,                       Color::Reset},
    {Action::Starve,                     Color::Red},
    {Action::Finish,                     Color::Reset}
};

extern std::chrono::milliseconds print_delay;
extern bool print_color_by_philosopher;
extern bool print_reset_color_after;

template<int philosophers_num>
void print_events(const auto& all_events) {
    std::array<std::tuple<std::string, Color, Color>, philosophers_num> draw;
    draw.fill(std::tuple{action_draws.at(Action::None), Color::Reset, Color::Reset});

    auto text_of = [&](auto philosopher_id) -> std::string& {
        return std::get<0>(draw.at(philosopher_id));
    };
    auto event_color_of = [&](auto philosopher_id) -> Color& {
        return std::get<1>(draw.at(philosopher_id));
    };
    auto philosopher_color_of = [&](auto philosopher_id) -> Color& {
        return std::get<2>(draw.at(philosopher_id));
    };

    for(auto& event : all_events) {
        auto text = action_draws.at(event.action);
        for (auto& draws : draw) {
            if (print_reset_color_after) {
                std::get<1>(draws) = Color::Reset;
            }
            if (std::get<0>(draws)[middle_char_index] == ' ') continue;
            if (std::get<0>(draws)[middle_char_index] == 'D') continue; // print b
            if (std::get<0>(draws)[middle_char_index] == 'T') continue;
            std::get<0>(draws)[middle_char_index] = '.';
        }
        text_of(event.philosopher_id) = text;
        event_color_of(event.philosopher_id) = action_colors.at(event.action); // print event color
        philosopher_color_of(event.philosopher_id) = static_cast<Color>(event.philosopher_id); // print philosopher color

        std::array<std::string, philosophers_num> draw_free_forks;
        draw_free_forks.fill("|");

        auto set_forks_draw = [&]{
            for (int ph_index = 0; ph_index < philosophers_num; ++ph_index) {
                auto ph_before_draw  = (ph_index - 1 < 0) ? text_of(philosophers_num - 1) : text_of(ph_index-1);
                auto ph_current_draw = text_of(ph_index);

                if (ph_before_draw[after_char_index] == ' ' && ph_current_draw[before_char_index] == ' ') {
                    draw_free_forks[ph_index] = '|';
                } else {
                    draw_free_forks[ph_index] = ' ';
                }
            }
        };
        set_forks_draw();

        for(int ph_index = 0; ph_index < philosophers_num; ++ph_index) {
            if (not print_color_by_philosopher) {
                std::cout << draw_free_forks[ph_index] << colors.at(event_color_of(ph_index)) << text_of(ph_index) << colors.at(Color::Reset);
                continue;
            }
            std::cout << draw_free_forks[ph_index] << colors.at(philosopher_color_of(ph_index)) << text_of(ph_index) << colors.at(Color::Reset);
        }
        std::cout << draw_free_forks[0];

        auto draw_info = [&]{
            static auto old_time = event.time.count();
            auto event_time = static_cast<double>(event.time.count()) / 1000.0;
            auto event_time_diff = static_cast<double>(event.time.count() - old_time) / 1000.0;
            if (not print_color_by_philosopher) {
                std::cout << "   time: " << event_time << " us \t diff: " << event_time_diff << " us\t" << colors.at(action_colors.at(event.action)) << event.text.rdbuf() << "\n" << colors.at(Color::Reset);
            } else {
                std::cout << "   time: " << event_time << " us \t diff: " << event_time_diff << " us\t" << colors.at(static_cast<Color>(event.philosopher_id)) << event.text.rdbuf() << "\n" << colors.at(Color::Reset);
            }
            old_time = event.time.count();
            std::this_thread::sleep_for(print_delay); // TODO sleep_until is much better
        };
        draw_info();
    }
};
