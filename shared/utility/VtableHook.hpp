#pragma once

#include <cstdint>
#include <vector>

#include <Windows.h>

#include "Address.hpp"

class VtableHook {
public:
    VtableHook();
    VtableHook(Address target);
    VtableHook(const VtableHook& other) = delete;
    VtableHook(VtableHook&& other);

    virtual ~VtableHook();

    bool create(Address target);
    bool recreate();
    bool remove();

    bool hook_method(uint32_t index, Address newMethod);

    auto get_instance() {
        return m_vtable_ptr;
    }

    // Access to original methods.
    Address get_method(uint32_t index) {
        if (index < m_vtable_size && m_old_vtable && m_new_vtable) {
            return m_old_vtable.as<Address*>()[index];
        }
        else {
            return nullptr;
        }
    }

    template <typename T>
    T get_method(uint32_t index) {
        return (T)get_method(index).ptr();
    }

private:
    std::vector<Address> m_raw_data;
    Address m_vtable_ptr;
    Address* m_new_vtable;
    Address m_old_vtable;
    size_t m_vtable_size;

    size_t get_vtable_size(Address vtable);
};