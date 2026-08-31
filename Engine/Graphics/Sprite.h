#pragma once
#include "Model.h"
#include <wrl.h>

using Microsoft::WRL::ComPtr;   // ★追加

class TUFEngine;
struct Material;

class Sprite : public Model {
public:
    Sprite();
    ~Sprite() override;

    void InitSprite(TUFEngine* engine, int textureIndex, float w, float h);

    void UpdateVertices(
        const Vector3& point,
        const Vector2& texcoord,
        const Vector3& normal,
        int index) override;

    void Resize(float w, float h);
    void SetWorldTransform(const Matrix4x4& wvp, const Matrix4x4& world) override;
    void SetUVTransform(const Matrix4x4& uvTransform) override;


    void Draw(ID3D12GraphicsCommandList* cmdList,
        int textureIndex,
        UINT instanceCount,
        UINT startInstanceLocation)override;

    int GetTextureIndex() const { return m_textureIndex; }

private:
    TUFEngine* m_pEngine = nullptr;

    int   m_textureIndex = 0;
    float m_width = 0.0f;
    float m_height = 0.0f;

    ComPtr<ID3D12Resource>   m_pVertexResource;   // ★
    ComPtr<ID3D12Resource>   m_pMaterialResource; // ★
    ComPtr<ID3D12Resource>   m_pWvpResource;      // ★

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};

    uint32_t Align256(uint32_t size)
    {
        return (size + 0xff) & ~0xff;
    }
};
