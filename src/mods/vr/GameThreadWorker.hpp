#pragma once

#include "ThreadWorker.hpp"

// Class that executes functions on the game thread.
class GameThreadWorker : public ThreadWorker<void> {
public:
    static GameThreadWorker& get() {
        static GameThreadWorker instance{};
        return instance;
    }
};