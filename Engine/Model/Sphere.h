#pragma once
#include "Model.h"
#include <wrl.h>                // ★追加

using Microsoft::WRL::ComPtr;   // ★追加

struct VertexData;
class TUFEngine;

class Sphere : public Model {
public:
    Sphere() = default;
    ~Sphere() override;

    void InitSphere(TUFEngine* engine);
    void Update();
    void SetWorldTransform(const Matrix4x4& wvp, const Matrix4x4& world) override;
    
    void Draw(ID3D12GraphicsCommandList* cmdList,
        int textureIndex,
        UINT instanceCount,
        UINT startInstanceLocation)override;

    void SetLightResource(ID3D12Resource* lightResource);
    void UpdateVertices(const Vector3& points, const Vector2& texcoord, const Vector3& normal, int index) override {}

private:
    ComPtr<ID3D12Resource>   m_pVertexResource;   // ★
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};

    ComPtr<ID3D12Resource>   m_pIndexResource;    // ★
    D3D12_INDEX_BUFFER_VIEW  m_indexBufferView{};

    ComPtr<ID3D12Resource>   m_pMaterialResource; // ★
    ComPtr<ID3D12Resource>   m_pWvpResource;      // ★

    // ライトリソースは外部から差し込むケースがあるため生ポインタを維持
    ComPtr<ID3D12Resource> m_pLightResource;

    uint32_t m_indexCount = 0;
    TUFEngine* m_pEngine = nullptr;

    uint32_t Align256(uint32_t size)
    {
        return (size + 0xff) & ~0xff;
    }
};
