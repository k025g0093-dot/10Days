#include "DynamicMeshModel.h"
#include "TextureManager.h"
#include "TUFEngine.h"
#include <cmath>
bool DynamicMeshModel::Init(TUFEngine* engine, int gridW, int gridH) {
    m_gridW = gridW;
    m_gridH = gridH;

    m_vertexCount = (uint32_t)(gridW * gridH);
    m_indexCount = (uint32_t)((gridW - 1) * (gridH - 1) * 6);

    // ═══════════════════════════════════════════════════════
    // 📌 ダブルバッファ：2 個の頂点バッファを作成
    // ═══════════════════════════════════════════════════════
    for (int i = 0; i < 2; i++) {
        m_vertexBuffers[i] = CreateBufferResource(
            engine->GetDevice(), sizeof(VertexData) * m_vertexCount);
        if (!m_vertexBuffers[i]) return false;

        m_vertexBuffers[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedDatas[i]));
        memset(m_mappedDatas[i], 0, sizeof(VertexData) * m_vertexCount);

        m_vertexBufferViews[i].BufferLocation = m_vertexBuffers[i]->GetGPUVirtualAddress();
        m_vertexBufferViews[i].SizeInBytes = sizeof(VertexData) * m_vertexCount;
        m_vertexBufferViews[i].StrideInBytes = sizeof(VertexData);

        // 初期頂点データを設定
        for (int y = 0; y < gridH; y++) {
            for (int x = 0; x < gridW; x++) {
                int vi = y * gridW + x;
                m_mappedDatas[i][vi].position = {
                    ((float)x - gridW / 2.0f),
                    0.0f,
                    ((float)y - gridH / 2.0f),
                    1.0f
                };
                m_mappedDatas[i][vi].texcoord = {
                    (float)x / (float)(gridW - 1),
                    (float)y / (float)(gridH - 1)
                };
                m_mappedDatas[i][vi].normal = { 0.0f, 1.0f, 0.0f };
                m_mappedDatas[i][vi].tangent = { 1.0f, 0.0f, 0.0f };
            }
        }
    }

    // ───────────────────────────────────────────────────────
    // インデックスバッファ（共有）
    // ───────────────────────────────────────────────────────
    m_indexBuffer = CreateBufferResource(
        engine->GetDevice(), sizeof(uint32_t) * m_indexCount);
    if (!m_indexBuffer) return false;
    m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedIndex));

    int ii = 0;
    for (int y = 0; y < gridH - 1; y++) {
        for (int x = 0; x < gridW - 1; x++) {
            uint32_t tl = y * gridW + x;
            uint32_t tr = y * gridW + (x + 1);
            uint32_t bl = (y + 1) * gridW + x;
            uint32_t br = (y + 1) * gridW + (x + 1);

            // DynamicMesh と統一（表面向き）
            m_mappedIndex[ii++] = bl;
            m_mappedIndex[ii++] = tr;
            m_mappedIndex[ii++] = tl;

            m_mappedIndex[ii++] = bl;
            m_mappedIndex[ii++] = br;
            m_mappedIndex[ii++] = tr;
        }
    }

    m_indexBuffer->Unmap(0, nullptr);
    m_mappedIndex = nullptr;

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.SizeInBytes = sizeof(uint32_t) * m_indexCount;
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // ───────────────────────────────────────────────────────
    // マテリアルバッファ
    // ───────────────────────────────────────────────────────
    m_materialBuffer = CreateBufferResource(engine->GetDevice(), sizeof(Material));
    if (!m_materialBuffer) return false;

    m_materialBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedMaterial));

    m_mappedMaterial->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_mappedMaterial->enableLighting = 1;
    m_mappedMaterial->enableNormalMap = 0;
    m_mappedMaterial->uvTransform = MakeIdentity4x4();

    // ───────────────────────────────────────────────────────
    // ライトバッファ
    // ───────────────────────────────────────────────────────
    m_lightBuffer = CreateBufferResource(engine->GetDevice(), sizeof(LightData));
    if (!m_lightBuffer) return false;
    LightData* lightData = nullptr;
    m_lightBuffer->Map(0, nullptr, reinterpret_cast<void**>(&lightData));
    lightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    lightData->dirOrPos = { 0.0f, -1.0f, 0.0f };
    lightData->intensity = 1.0f;
    m_lightBuffer->Unmap(0, nullptr);

    return true;
}

void DynamicMeshModel::UpdateUVTransform(const Vector3& uvScale, float uvRotation, const Vector3& uvTranslation) {
    if (!m_mappedMaterial) return;
    m_mappedMaterial->uvTransform = MakeAffineMatrix(uvScale, { 0.0f, 0.0f, uvRotation }, uvTranslation);
}

// 📌 最適化版 UpdateHeights（オプション：OpenMP で並列化）
void DynamicMeshModel::UpdateHeights(const DynamicMesh& mesh) {
    if (!m_mappedDatas[m_currentBufferIndex]) return;

    auto& verts = mesh.getVertices();
    VertexData* mappedData = m_mappedDatas[m_currentBufferIndex];  // 現在の書き込み対象

    // 🎯 最適化：OpenMP で並列化（オプション）
#pragma omp parallel for collapse(2) if(m_gridH * m_gridW > 10000)
    for (int y = 0; y < m_gridH; y++) {
        for (int x = 0; x < m_gridW; x++) {
            int i = y * m_gridW + x;
            int idx = i * 3;

            // 高さを更新
            mappedData[i].position.y = verts[idx + 1];

            // 📌 境界チェック（clamp を使うか、境界分岐を使うか）
            // ここでは clamp を使用（簡潔）
            auto getH = [&](int xi, int yi) -> float {
                xi = std::clamp(xi, 0, m_gridW - 1);
                yi = std::clamp(yi, 0, m_gridH - 1);
                return verts[(yi * m_gridW + xi) * 3 + 1];
                };

            float nx = getH(x - 1, y) - getH(x + 1, y);
            float ny = 2.0f;
            float nz = getH(x, y - 1) - getH(x, y + 1);

            //最適化：rsqrt (reciprocal sqrt) を使用
            float lengthSq = nx * nx + ny * ny + nz * nz;
            float invLen = 1.0f/ sqrtf(lengthSq);  // ← sqrt より高速

            mappedData[i].normal = { nx * invLen, ny * invLen, nz * invLen };
        }
    }
}


void DynamicMeshModel::UpdateVertexColors(const std::vector<Vector4>& colors) {
    if (colors.empty() || !m_mappedDatas[m_currentBufferIndex]) return;

    VertexData* mappedData = m_mappedDatas[m_currentBufferIndex];

    for (size_t i = 0; i < colors.size() && i < (size_t)(m_gridW * m_gridH); ++i) {
        // tangent（Vector3）に色を格納
        mappedData[i].tangent = { colors[i].x, colors[i].y, colors[i].z };
    }
}

void DynamicMeshModel::Draw(
    ID3D12GraphicsCommandList* cmdList,
    int textureIndex,
    UINT instanceCount,
    UINT startInstanceLocation) {
    if (m_indexCount == 0) return;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // テクスチャだけセット（material/light は呼び出し側で済み）
    auto handle = TextureManager::GetInstance()->GetGPUHandle(textureIndex);
    if (handle.ptr == 0) handle = TextureManager::GetInstance()->GetGPUHandle(0);
    if (handle.ptr == 0) return;

    cmdList->SetGraphicsRootDescriptorTable(2, handle);

    cmdList->SetGraphicsRootConstantBufferView(0, m_materialBuffer->GetGPUVirtualAddress());


    // ダブルバッファから描画用を選択
    int drawIndex = (m_currentBufferIndex + 1) % 2;
    cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferViews[drawIndex]);
    cmdList->IASetIndexBuffer(&m_indexBufferView);

    cmdList->DrawIndexedInstanced(
        m_indexCount,
        instanceCount,
        0,
        0,
        startInstanceLocation
    );
}