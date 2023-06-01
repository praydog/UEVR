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

    IConsoleObject* find(const std::wstring& name) {
        // make lower
        std::wstring lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::towlower);

        for (auto& element : m_console_objects) {
            if (element.key != nullptr) {
                // case insensitive compare
                std::wstring lower_key = element.key;
                std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::towlower);

                if (lower_key == lower_name) {
                    return element.value;
                }
            }
        }

        return nullptr;
    }

    std::vector<ConsoleObjectElement> fuzzy_find(const std::wstring& name) {
        // make lower
        std::wstring lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::towlower);

        std::vector<ConsoleObjectElement> results{};

        for (auto& element : m_console_objects) {
            if (element.key != nullptr && element.value != nullptr) try {
                // case insensitive compare
                std::wstring lower_key = element.key;
                std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::towlower);

                if (lower_key.find(lower_name) != std::wstring::npos) {
                    results.push_back(element);
                }
            } catch(...) {
                continue;
            }
        }

        std::sort(results.begin(), results.end(), [](const ConsoleObjectElement& a, const ConsoleObjectElement& b) -> bool {
            return std::wstring{a.key} < std::wstring{b.key};
        }); 

        return results;
    }

private:
    ConsoleObjectArray m_console_objects;
};
}