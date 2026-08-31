#pragma once
#include "ImGuiWindow.h"
#include "Vector.h"

class ImGuiCamera : public ImGuiUIWindow {
public:

    // 外からカメラ情報を取得したい場合用
    const Vector3& GetEye()    const { return m_eye; }
    const Vector3& GetTarget() const { return m_target; }
    void SetTransform(TransformData* transform) { m_transform = transform; }
    void update(TUFEngine* engine) override;
private:
    Vector3 m_eye = { 0.0f, 5.0f, -10.0f };
    Vector3 m_target = { 0.0f, 0.0f,   0.0f };
    TransformData* m_transform = nullptr; // mainのcameraTransformを指す
};