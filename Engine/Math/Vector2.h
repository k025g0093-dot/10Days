#pragma once

// U, V などの 2次元ベクトル用
struct Vector2 {
    float x;
    float y;

    // コンストラクタ
    Vector2() : x(0.0f), y(0.0f) {}
    Vector2(float _x, float _y) : x(_x), y(_y) {}

    // 基本的な演算子（必要に応じて）
    Vector2 operator+(const Vector2& other) const { return Vector2(x + other.x, y + other.y); }
    Vector2 operator-(const Vector2& other) const { return Vector2(x - other.x, y - other.y); }
    Vector2 operator*(float s) const { return Vector2(x * s, y * s); }
};