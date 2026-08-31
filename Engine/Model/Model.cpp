#include "Model.h"

Matrix4x4 Model::GetWorldMatrix() const {
    // 自作の MakeAffineMatrix を使い、
    // メンバ変数の m_scale, m_rotation, m_position からワールド行列を合成して返す
    return MakeAffineMatrix(m_scale, m_rotation, m_position);
}