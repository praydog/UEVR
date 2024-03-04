#pragma once

#include "uevr/API.h"

namespace uevr {
namespace fuobjectarray {
UEVR_UObjectHandle find_uobject(const wchar_t* name);
bool is_chunked();
bool is_inlined();
unsigned int get_objects_offset();
unsigned int get_item_distance();
int get_object_count(UEVR_UObjectArrayHandle array);
void* get_objects_ptr(UEVR_UObjectArrayHandle array);
UEVR_UObjectHandle get_object(UEVR_UObjectArrayHandle array, int index);
UEVR_FUObjectItemHandle get_item(UEVR_UObjectArrayHandle array, int index);

extern UEVR_UObjectArrayFunctions functions;
}
}