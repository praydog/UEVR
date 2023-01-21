#pragma once

#include <cstdint>

namespace sdk {
struct FRHICommandBase;

// Minimal implementation of FRHICommandList
// this structure seems to be consistent really far back in UE history
// so we can just hardcode it instead of trying to perform analysis to describe it
// doesn't appear to have a vtable.
struct FRHICommandListBase {
    FRHICommandBase* root;
    FRHICommandBase** command_link;
    bool is_executing;
    uint32_t num_commands;
    uint32_t uid;

    // blah blah rest we don't care about
};

// Now FRHICommandBase actually slightly differs between UE4 versions
// in newer versions, it has a vtable pointer which the only function is ExecuteAndDestruct
// in older versions, there is no vtable, but it has a pointer to the lambda/function instead at the end
struct FRHICommandBase_New {
    FRHICommandBase_New* next{nullptr};
    virtual void execute_and_destruct(FRHICommandListBase& cmd_list, void* debug_context) {};
};

struct FRHICommandBase_Old {
    using Func = void(*)(FRHICommandListBase& cmd_list, void* debug_context);

    FRHICommandBase_Old* next{nullptr};
    Func func;
};
}