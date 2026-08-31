#pragma once
#include "MonoBehaviour.h"

class LearnComponent : public MonoBehaviour {
public:
    void Update() override {
        OutputDebugStringA("LearnComponent::Update called\n");
    }
    Component* Clone() const override {
        return new LearnComponent();
    }
};
