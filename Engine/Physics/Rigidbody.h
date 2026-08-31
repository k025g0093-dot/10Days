#pragma once
#include "Component.h"

class Rigidbody : public Component {
public:
    float mass = 1.0f;
    float linearDrag = 0.0f;
    float angularDrag = 0.0f;
    bool useGravity = true;
    bool isKinematic = false;

    Component* Clone() const override {
        auto* c = new Rigidbody();
        c->mass = mass;
        c->linearDrag = linearDrag;
        c->angularDrag = angularDrag;
        c->useGravity = useGravity;
        c->isKinematic = isKinematic;
        return c;
    }
};