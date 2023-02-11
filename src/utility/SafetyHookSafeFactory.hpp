#pragma once

#include <mutex>

#include <SafetyHook.hpp>

class SafetyHookSafeFactory : public std::enable_shared_from_this<SafetyHookSafeFactory> {
public:
    SafetyHookSafeFactory() {
        s_mutex.lock();
        m_factory = SafetyHookFactory::init();
    }

    virtual ~SafetyHookSafeFactory() {
        m_factory.reset();
        s_mutex.unlock();
    }

    static auto init() { return std::shared_ptr<SafetyHookSafeFactory>{new SafetyHookSafeFactory}; }

    safetyhook::Factory::Builder acquire() { return m_factory->acquire(); }

private:
    static inline std::recursive_mutex s_mutex{};
    std::shared_ptr<SafetyHookFactory> m_factory{};
};