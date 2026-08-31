#pragma once
#include "allVector.h"
#include "Input.h"

class DebugCamer {
public:
    // シングルトン（GUI等からアクセスできるようにする）
    static DebugCamer& GetInstance() {
        static DebugCamer instance;
        return instance;
    }

    void Initialize(float width, float height);
    void Update();
    Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
    bool IsDebug() const { return isDebug; }
    void SetDebug(bool dbg) { isDebug = dbg; }
private:
    DebugCamer() = default;
    DebugCamer(const DebugCamer&) = delete;
    DebugCamer& operator=(const DebugCamer&) = delete;
    Vector3 scale = { 1,1,1 };
    Vector3 rotate{0,0,0};
    Vector3 translate = { 0,0,-50.0f };

    float distance = 50;

    Matrix4x4 matRot_;
    Vector3 pivot = { 0.0f, 0.0f, 0.0f };


    bool isDebug = false;

    float width_ = 0.0f;
    float height_ = 0.0f;

    

    Matrix4x4 viewProjectionMatrix_{};
};