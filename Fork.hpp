#pragma once
#include <iostream>
#include <mutex>
#include <optional>

struct Fork {
    using lock_type = std::unique_lock<std::mutex>;
    using lock_opt = std::optional<lock_type>;

    auto try_take() const -> lock_opt;
    int get_id() const;

private:
    static int id;
    int fork_id = id++;
    mutable std::mutex mt;
};

int Fork::id = 0;

auto Fork::try_take() const -> Fork::lock_opt {
    if (lock_type mt_lock{mt, std::defer_lock}; mt_lock.try_lock()) {
        return mt_lock;
    }
    return {};
}

int Fork::get_id() const {
    return fork_id;
}

std::ostream& operator<<(std::ostream& out, const Fork& fork) {
    out << "fork<" << fork.get_id() << "> ";
    return out;
}
