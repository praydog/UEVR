#pragma once

#include "ThreadWorker.hpp"

// Class that executes functions on the RHI thread.
class RHIThreadWorker : public ThreadWorker {
public:
    static RHIThreadWorker& get() {
        static RHIThreadWorker instance{};
        return instance;
    }
};