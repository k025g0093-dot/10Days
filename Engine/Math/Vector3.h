#pragma once
#include <DirectXMath.h>
#include <cmath>

struct Vector3 : public DirectX::XMFLOAT3 {
    // コンストラクタ
    Vector3(float _x = 0, float _y = 0, float _z = 0) : XMFLOAT3(_x, _y, _z) {}
    Vector3(const DirectX::XMFLOAT3& f) : XMFLOAT3(f.x, f.y, f.z) {}

    // 基本計算（オーバーロード）
    Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
    Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
    Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
    Vector3& operator+=(const Vector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    // ゲームで絶対使う便利機能
    float Length() const { return std::sqrt(x * x + y * y + z * z); }

    Vector3 Normalized() const {
        float len = this->Length(); // this-> を追加
        if (len == 0) return Vector3(0, 0, 0);
        return *this * (1.0f / len);
    }

    // ドット積（内積）：視界判定やライティングで使う
    float Dot(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }

    // クロス積（外積）：面の向きを出すのに使う
    Vector3 Cross(const Vector3& v) const {
        return Vector3(
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        );
    }
};