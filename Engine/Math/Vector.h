#pragma once
#include <cmath>
#include<assert.h>
#include "Vector3.h"

struct Matrix4x4
{
    float m[4][4];
};



struct TransformData {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

//平行移動
Matrix4x4 MakeTranslateMatrix(const Vector3& Vector);
//拡縮
Matrix4x4 MakeScaleMatrix(const Vector3& Vector);
//座標変換
Vector3 TransformMatrix(const Vector3& Vector, const Matrix4x4& matrix);

Matrix4x4 MakeRotateXMatrix(float radian);
Matrix4x4 MakeRotateYMatrix(float radian);
Matrix4x4 MakeRotateZMatrix(float radian);

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
Matrix4x4 MakeIdentity4x4();

Matrix4x4 Inverse(const Matrix4x4& m);

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearClip, float farClip);

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up);