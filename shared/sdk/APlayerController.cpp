#include "APlayerController.hpp"

namespace sdk {
APawn* APlayerController::get_acknowledged_pawn() const {
    return get_property<APawn*>(L"AcknowledgedPawn");
}
}