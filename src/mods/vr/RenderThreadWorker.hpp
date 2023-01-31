#pragma once

#include "ThreadWorker.hpp"

// Class that executes functions on the render thread.
class RenderThreadWorker : public ThreadWorker<void> {
public:
    static RenderThreadWorker& get() {
        static RenderThreadWorker instance{};
        return instance;
    }
};