#pragma once
#include"Collider.h"
#include "allVector.h"
class BoxCollider :public Collider {
public:
    Vector3 size   = { 1.0f, 1.0f, 1.0f };   // 全長
    Vector3 center = { 0.0f, 0.0f, 0.0f };   // Entity原点からのズレ

    const char* GetShapeName() const override { return "BoxCollider"; }
    Component* Clone() const override {
        auto* c = new BoxCollider();
        c->size = size;
        c->center = center;
        c->isTrigger = isTrigger;
        return c;
    }
};
