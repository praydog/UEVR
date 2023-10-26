#pragma once

#include <sdk/Math.hpp>

#include "UObject.hpp"

namespace sdk {
class APawn;
class APlayerCameraManager;

class AController : public UObject {
public:
    static UClass* static_class();

    void set_control_rotation(const glm::vec3& newrotation);
    glm::vec3 get_control_rotation();

protected:
};

class APlayerController : public AController {
public:
    static UClass* static_class();

    APawn* get_acknowledged_pawn() const;
    APlayerCameraManager* get_player_camera_manager() const;
    void add_rotation_input(const glm::vec3& rotation);

protected:
};
}