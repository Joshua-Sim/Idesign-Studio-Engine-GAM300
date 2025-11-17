#pragma once
#include <glm.hpp>

struct CameraComponent
{
    float yaw = -90.f;
    float pitch = 0.f;

    float sensitivity = 0.1f;
    float distance = 0.f;  // 0 = first-person, >0 = third-person orbit
    float heightOffset = 1.7f;

    bool active = false;   // If true this entity becomes main camera
};
