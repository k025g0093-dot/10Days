#pragma once
#include "allVector.h"
#include <wrl.h>
#include <d3d12.h>
#include <vector>

using Microsoft::WRL::ComPtr;

class TUFEngine;

class Line {
public:
    void Initialize(TUFEngine* engine);
    void Clear();
    void Add(const Vector3& from, const Vector3& to, const Vector4& color);
    void Draw(ID3D12GraphicsCommandList* cmdList, const Matrix4x4& viewProj);

private:
    struct LineVertex {
        Vector4 position;
        Vector4 color;
    };

    ComPtr<ID3D12Resource>      m_pVertexResource;
    ComPtr<ID3D12Resource>      m_pWvpResource;
    D3D12_VERTEX_BUFFER_VIEW    m_vertexBufferView{};
    LineVertex*                 m_pMappedVertexData = nullptr;
    Matrix4x4*                  m_pWvpData = nullptr;
    TUFEngine*                  m_pEngine = nullptr;
    UINT                        m_maxVertices = 0;

    std::vector<LineVertex>     m_vertices;
};
