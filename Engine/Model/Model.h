#pragma once
#include "allVector.h" 
#include <d3d12.h>
#include <vector>

struct Vertex {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
    Vector3 tangent;
};

class Model {
protected:
    Vector3 m_position = { 0, 0, 0 };
    Vector3 m_rotation = { 0, 0, 0 };
    Vector3 m_scale = { 1, 1, 1 };

    Vertex* m_pVertexData = nullptr;
    UINT m_vertexCount = 0;
    UINT m_indexCount = 0;

public:

    // Model.h の public: セクション内に追加
    const Vertex* GetVertexData() const { return m_pVertexData; }
    UINT GetVertexCount() const { return m_vertexCount; }

    virtual ~Model() {}

    UINT GetIndexCount() const { return m_indexCount; }

    void SetPosition(const Vector3& p) { m_position = p; }
    void SetRotation(const Vector3& r) { m_rotation = r; }
    void SetScale(const Vector3& s) { m_scale = s; }

    Matrix4x4 GetWorldMatrix() const;

    // オプション：各モデルが独自の transform を設定する場合
    virtual void SetWorldTransform(const Matrix4x4&, const Matrix4x4&) {}
    virtual void SetUVTransform(const Matrix4x4&) {}

    // 純粋仮想関数
    virtual void UpdateVertices(
        const Vector3& points,
        const Vector2& texcoord,
        const Vector3& normal,
        int index) = 0;

    virtual void Draw(
        ID3D12GraphicsCommandList* cmdList,
        int textureIndex,
        UINT instanceCount,
        UINT startInstanceLocation) = 0;
};