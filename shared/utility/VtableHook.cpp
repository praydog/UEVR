#include "VtableHook.hpp"

using namespace std;

VtableHook::VtableHook()
    : m_raw_data{},
    m_vtable_ptr(),
    m_new_vtable(nullptr),
    m_old_vtable(),
    m_vtable_size(0)
{}

VtableHook::VtableHook(Address target)
    : VtableHook()
{
    create(target);
}

VtableHook::VtableHook(VtableHook&& other)
    : m_raw_data(move(other.m_raw_data)),
    m_vtable_ptr(other.m_vtable_ptr),
    m_new_vtable(other.m_new_vtable),
    m_old_vtable(other.m_old_vtable),
    m_vtable_size(other.m_vtable_size)
{
    other.m_vtable_ptr = nullptr;
    other.m_new_vtable = nullptr;
    other.m_old_vtable = nullptr;
    other.m_vtable_size = 0;
}

VtableHook::~VtableHook() {
    remove();
}

bool VtableHook::create(Address target) {
    if (!m_raw_data.empty()) {
        remove();
        m_raw_data.clear();
    }

    m_vtable_ptr = target;
    m_old_vtable = m_vtable_ptr.to<Address>();
    m_vtable_size = get_vtable_size(m_old_vtable);
    // RTTI.
    m_raw_data.resize(m_vtable_size + 1);
    m_new_vtable = m_raw_data.data() + 1;

    memcpy(m_raw_data.data(), m_old_vtable.as<Address*>() - 1, sizeof(Address) * (m_vtable_size + 1));

    // At this point we have the address of the old vtable, and a copy of it
    // stored in m_new_vtable.  Set the target objects vtable
    // pointer to our copy of the original.
    *m_vtable_ptr.as<Address*>() = m_new_vtable;

    return true;
}

bool VtableHook::recreate() {
    if (m_vtable_ptr != nullptr) {
        *m_vtable_ptr.as<Address*>() = m_new_vtable;
        return true;
    }

    return false;
}

bool VtableHook::remove() {
    // Can cause issues where we set the vtable/random memory of some other pointer.
    if (m_vtable_ptr != nullptr && IsBadReadPtr(m_vtable_ptr.ptr(), sizeof(void*)) == FALSE && m_vtable_ptr.to<void*>() == m_new_vtable) {
        *m_vtable_ptr.as<Address*>() = m_old_vtable;
        return true;
    }

    return false;
}

bool VtableHook::hook_method(uint32_t index, Address newMethod) {
    if (m_old_vtable != nullptr && m_new_vtable != nullptr && index < m_vtable_size) {
        m_new_vtable[index] = newMethod;
        return true;
    }

    return false;
}

size_t VtableHook::get_vtable_size(Address vtable) {
    size_t i = 0;

    for (; vtable.as<Address*>()[i] != nullptr; ++i) {
        if (IsBadCodePtr(vtable.as<FARPROC*>()[i]) == TRUE) {
            break;
        }
    }

    return i;
}
