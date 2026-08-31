#pragma once
#include"Collider.h"

class ConvexHullCollider :public Collider {

public:
    const char* GetShapeName() const override { return "ConvexHullCollider"; }
    Component* Clone() const override {
        return new ConvexHullCollider();
    }


};
