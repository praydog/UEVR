#include <sdk/UObjectArray.hpp>

#include "FUObjectArrayFunctions.hpp"


namespace uevr {
namespace fuobjectarray {
UEVR_UObjectHandle find_uobject(const wchar_t* name) {
    return (UEVR_UObjectHandle)sdk::find_uobject(name);
}

bool is_chunked() { 
    return sdk::FUObjectArray::is_chunked();
}

bool is_inlined() {
    return sdk::FUObjectArray::is_inlined();
}

unsigned int get_objects_offset() {
    return sdk::FUObjectArray::get_objects_offset();
}

unsigned int get_item_distance() {
    return sdk::FUObjectArray::get_item_distance();
}

int get_object_count(UEVR_UObjectArrayHandle array) {
    return ((sdk::FUObjectArray*)array)->get_object_count();
}

void* get_objects_ptr(UEVR_UObjectArrayHandle array) {
    return ((sdk::FUObjectArray*)array)->get_objects_ptr();
}

UEVR_UObjectHandle get_object(UEVR_UObjectArrayHandle array, int index) {
    auto item = ((sdk::FUObjectArray*)array)->get_object(index);

    if (item == nullptr) {
        return nullptr;
    }

    return (UEVR_UObjectHandle)item->object;
}

// messed up naming, I know
UEVR_FUObjectItemHandle get_item(UEVR_UObjectArrayHandle array, int index) {
    return (UEVR_FUObjectItemHandle)((sdk::FUObjectArray*)array)->get_object(index);
}

UEVR_UObjectArrayFunctions functions {
    .find_uobject = uevr::fuobjectarray::find_uobject,
    .is_chunked = uevr::fuobjectarray::is_chunked,
    .is_inlined = uevr::fuobjectarray::is_inlined,
    .get_objects_offset = uevr::fuobjectarray::get_objects_offset,
    .get_item_distance = uevr::fuobjectarray::get_item_distance,
    .get_object_count = uevr::fuobjectarray::get_object_count,
    .get_objects_ptr = uevr::fuobjectarray::get_objects_ptr,
    .get_object = uevr::fuobjectarray::get_object,
    .get_item = uevr::fuobjectarray::get_item
};
}
}