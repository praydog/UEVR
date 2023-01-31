#pragma once

#include "ThreadWorker.hpp"

// Class that executes functions on the RHI thread.
class RHIThreadWorker : public ThreadWorker<void> {
public:
    static RHIThreadWorker& get() {
        static RHIThreadWorker instance{};
        return instance;
    }
};