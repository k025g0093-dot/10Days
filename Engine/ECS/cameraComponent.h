#pragma once

#include "Component.h"

// Entity をゲーム用カメラとして扱うための設定です。
// 実際の位置・回転は Entity::transform を使用します。
class CameraComponent : public Component {
public:
    bool isMainCamera = false;

    float fov = 0.45f;
    float nearClip = 0.1f;
    float farClip = 1000.0f;

    Component* Clone() const override {
        return new CameraComponent(*this);
    }
};
