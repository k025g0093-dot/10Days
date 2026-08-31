#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "DynamicMesh.h"
#include "Model.h"

using Microsoft::WRL::ComPtr;

class TUFEngine;
struct VertexData;
struct Material;
struct Vector3;

class DynamicMeshModel : public Model {
public:
    bool Init(TUFEngine* engine, int gridW, int gridH);
    void UpdateHeights(const DynamicMesh& mesh);

    // Model の仮想関数をオーバーライド
    void UpdateVertices(
        const Vector3& points,
        const Vector2& texcoord,
        const Vector3& normal,
        int index) override {}  // DynamicMesh 使うから実装不要

    void Draw(
        ID3D12GraphicsCommandList* cmdList,
        int textureIndex,
        UINT instanceCount,
        UINT startInstanceLocation) override;

    void UpdateUVTransform(const Vector3& uvScale, float uvRotation, const Vector3& uvTranslation);
    void SwapBuffers() {
        m_currentBufferIndex = (m_currentBufferIndex + 1) % 2;
    }
    void UpdateVertexColors(const std::vector<Vector4>& colors);
    VertexData* GetMappedVertexBuffer() const { return m_mappedDatas[m_currentBufferIndex]; }
private:
    ComPtr<ID3D12Resource>   m_vertexBuffers[2];
    ComPtr<ID3D12Resource>   m_indexBuffer;
    ComPtr<ID3D12Resource>   m_materialBuffer;
    ComPtr<ID3D12Resource>   m_lightBuffer;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferViews[2];
    D3D12_INDEX_BUFFER_VIEW  m_indexBufferView;

    VertexData* m_mappedDatas[2] = { nullptr, nullptr };
    uint32_t* m_mappedIndex = nullptr;
    Material* m_mappedMaterial = nullptr;

    int m_currentBufferIndex = 0;

    uint32_t m_indexCount = 0;
    int m_gridW = 0;
    int m_gridH = 0;
};