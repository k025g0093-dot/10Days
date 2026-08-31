#pragma once
#include "Model.h"
#include <wrl.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

struct VertexData;
struct Material;
class TUFEngine;

class TriangleModel : public Model {
public:
    void Initialize(TUFEngine* engine);

    // ★追加: 毎フレームWVPを更新する
    void Update(const Vector3& pos, const Vector3& rot, const Vector3& scale);

    void UpdateVertices(const Vector3& points,
        const Vector2& texcoord,
        const Vector3& normal,
        int index) override;


    void Draw(ID3D12GraphicsCommandList* cmdList,
        int textureIndex,
        UINT instanceCount,
        UINT startInstanceLocation)override;

    void SetWorldTransform(const Matrix4x4& wvp, const Matrix4x4& world);

private:
    ComPtr<ID3D12Resource>   m_pVertexResource;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
    ComPtr<ID3D12Resource>   m_pMaterialResource;
    ComPtr<ID3D12Resource>   m_pWvpResource;
    ComPtr<ID3D12Resource>   m_pLightResource;   // ★追加
    Material* materialData = nullptr;

    VertexData* m_pVertexData = nullptr;

    TUFEngine* m_pEngine = nullptr;

    uint32_t Align256(uint32_t size) {
        return (size + 0xff) & ~0xff;
    }
};