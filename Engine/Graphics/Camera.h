#pragma once
#include "Vector.h"

struct CameraForGPU {
    Vector3 worldPosition;
};

class Camera {
public:
    Camera() {
        transform.scale = { 1.0f, 1.0f,  1.0f };
        transform.rotate = { 0.0f, 0.0f,  0.0f };
        transform.translate = { 0.0f, 0.0f, -10.0f };
        m_nearPlane = 0.1f;
        m_farPlane = 10000.0f;
    }

    Vector3 scale;

    Matrix4x4 GetViewMatrix() {
        Matrix4x4 cameraMatrix = MakeAffineMatrix(
            transform.scale, transform.rotate, transform.translate);
        return Inverse(cameraMatrix);
    }

    Matrix4x4 GetProjectionMatrix(float width, float height) {
        return MakePerspectiveFovMatrix(0.45f, width / height, m_nearPlane, m_farPlane);
    }

    Matrix4x4 GetViewProjectionMatrix(float width, float height) {
        Matrix4x4 cameraMatrix = MakeAffineMatrix(
            transform.scale, transform.rotate, transform.translate);
        Matrix4x4 viewMatrix = Inverse(cameraMatrix);
        Matrix4x4 projMatrix = MakePerspectiveFovMatrix(
            0.45f, width / height, m_nearPlane, m_farPlane);
        return Multiply(viewMatrix, projMatrix);
    }

    float GetNearPlane() const { return m_nearPlane; }
    float GetFarPlane() const { return m_farPlane; }

    void SetNearPlane(float n) { m_nearPlane = n; }
    void SetFarPlane(float f) { m_farPlane = f; }

    TransformData transform;



private:
    float m_nearPlane;
    float m_farPlane;
};