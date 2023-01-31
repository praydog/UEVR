#pragma once

#include <deque>
#include <mutex>
#include <functional>

// Class that executes functions on a specific thread when inherited from
template<typename... Args>
class ThreadWorker {
public:
    template<typename... T>
    void execute(T... args) {
        std::scoped_lock _{m_mutex};
        if (m_queue.empty()) {
            return;
        }

        for (auto& func : m_queue) {
            func(args...);
        }

        m_queue.clear();
    }

    void enqueue(std::function<void(Args...)> func) {
        std::scoped_lock _{m_mutex};
        m_queue.push_back(func);
    }

private:
    std::recursive_mutex m_mutex{};
    std::deque<std::function<void(Args...)>> m_queue{};
};