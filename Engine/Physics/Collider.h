#pragma once
#include "Component.h"

class Collider : public Component {
public:
    bool isTrigger = false;  

    virtual const char* GetShapeName() const = 0;
};
