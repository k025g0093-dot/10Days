#pragma once
#include"Collider.h"

class SphereCollider :
	public Collider 
{
public:
    float radius = 0.5f;

    const char* GetShapeName() const override { return "SphereCollider"; }
    Component* Clone() const override {
        auto* c = new SphereCollider();
        c->radius = radius;
        c->isTrigger = isTrigger;
        return c;
    }
};