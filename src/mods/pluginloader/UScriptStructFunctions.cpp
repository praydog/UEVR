#include <sdk/UClass.hpp>

#include "UScriptStructFunctions.hpp"

namespace uevr {
namespace uscriptstruct {
UEVR_StructOpsHandle get_struct_ops(UEVR_UScriptStructHandle script_struct) {
    return (UEVR_StructOpsHandle)((sdk::UScriptStruct*)script_struct)->get_struct_ops();
}

int get_struct_size(UEVR_UScriptStructHandle script_struct) {
    return ((sdk::UScriptStruct*)script_struct)->get_struct_size();
}

UEVR_UScriptStructFunctions functions {
    .get_struct_ops = get_struct_ops,
    .get_struct_size = get_struct_size
};
}
}