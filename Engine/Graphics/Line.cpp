#include "Line.h"
#include "TUFEngine.h"
#include "VertexResource.h"

void Line::Initialize(TUFEngine* engine) {
    m_pEngine = engine;
    ID3D12Device* device = engine->GetDevice();

    m_maxVertices = 8192;
    const UINT bufferSize = sizeof(LineVertex) * m_maxVertices;

    m_pVertexResource = CreateBufferResource(device, bufferSize);
    m_pVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&m_pMappedVertexData));

    m_vertexBufferView.BufferLocation = m_pVertexResource->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(LineVertex);
    m_vertexBufferView.SizeInBytes = bufferSize;

    const UINT wvpSize = (sizeof(Matrix4x4) + 255) & ~255;
    m_pWvpResource = CreateBufferResource(device, wvpSize);
    m_pWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&m_pWvpData));

    m_vertices.reserve(4096);
}

void Line::Clear() {
    m_vertices.clear();
}

void Line::Add(const Vector3& from, const Vector3& to, const Vector4& color) {
    if (m_vertices.size() + 2 > m_maxVertices) return;
    LineVertex v0, v1;
    v0.position = { from.x, from.y, from.z, 1.0f };
    v0.color = { color.x, color.y, color.z, color.w };
    v1.position = { to.x, to.y, to.z, 1.0f };
    v1.color = { color.x, color.y, color.z, color.w };
    m_vertices.push_back(v0);
    m_vertices.push_back(v1);
}

void Line::Draw(ID3D12GraphicsCommandList* cmdList, const Matrix4x4& viewProj) {
    if (m_vertices.empty() || !m_pMappedVertexData || !m_pWvpData) return;

    memcpy(m_pMappedVertexData, m_vertices.data(), m_vertices.size() * sizeof(LineVertex));
    *m_pWvpData = viewProj;

    cmdList->SetGraphicsRootConstantBufferView(0, m_pWvpResource->GetGPUVirtualAddress());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    cmdList->DrawInstanced(static_cast<UINT>(m_vertices.size()), 1, 0, 0);
}
