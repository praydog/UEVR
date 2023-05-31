#pragma once

namespace sdk {
struct IConsoleObject;

struct ConsoleObjectElement {
    wchar_t* key;
    int32_t unk[2];
    IConsoleObject* value;
    int32_t unk2[2];
};

struct ConsoleObjectArray {
    ConsoleObjectElement* elements;
    uint32_t count;
    uint32_t capacity;

    // begin end
    ConsoleObjectElement* begin() {
        return elements;
    }

    ConsoleObjectElement* end() {
        return elements + count;
    }
};

class IConsoleManager {
public:
    virtual ~IConsoleManager() {}
};

class FConsoleManager : public IConsoleManager {
public:
    static FConsoleManager* get();

    ConsoleObjectArray& get_console_objects() {
        return m_console_objects;
    }

private:
    ConsoleObjectArray m_console_objects;
};
}