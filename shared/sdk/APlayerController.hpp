#pragma once

#include "UObject.hpp"

namespace sdk {
class APawn;

class APlayerController : public UObject {
public:
    APawn* get_acknowledged_pawn() const;

protected:
};
}