#pragma once
#include "allVector.h"

struct Transform {
    Vector3 position{};
    Vector3 rotation{};
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
};
