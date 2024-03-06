#pragma once

#include "uevr/API.h"

namespace uevr {
namespace uscriptstruct {
UEVR_StructOpsHandle get_struct_ops(UEVR_UScriptStructHandle script_struct);
int get_struct_size(UEVR_UScriptStructHandle script_struct);

extern UEVR_UScriptStructFunctions functions;
}
}