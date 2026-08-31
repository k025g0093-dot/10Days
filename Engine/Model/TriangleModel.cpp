#include "TriangleModel.h"
#include <wrl.h>
#include "TUFEngine.h"

void TriangleModel::Initialize(TUFEngine* engine) {
    m_pEngine = engine;
    ID3D12Device* device = engine->GetDevice();

    // 1つの三角形を描画するので頂点数は3
    const UINT maxVertices = 3;
    const UINT bufferSize = sizeof(VertexData) * maxVertices;
    m_vertexCount = maxVertices;

    m_pVertexResource = CreateBufferResource(device, bufferSize);
    m_pVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&m_pVertexData));

    m_vertexBufferView.BufferLocation = m_pVertexResource->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(VertexData);
    m_vertexBufferView.SizeInBytes = bufferSize;

    // ★ 変更点: 個別の m_pWvpResource の初期化は不要になったため削除しました
    // （行列はすべて TUFEngine 側のインスタンスバッファで一括管理するため）

    // マテリアルのセットアップ（Mapしたまま保持してDrawのたびに色を書き換えられるようにする）
    m_pMaterialResource = CreateBufferResource(device, Align256(sizeof(Material)));
    m_pMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData->enableLighting = false;
    materialData->uvTransform = MakeIdentity4x4();

    // ライトのセットアップ
    m_pLightResource = CreateBufferResource(device, Align256(sizeof(LightData)));
    LightData* lightData = nullptr;
    m_pLightResource->Map(0, nullptr, reinterpret_cast<void**>(&lightData));
    lightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    lightData->dirOrPos = { 0.0f, -1.0f, 0.0f };
    lightData->intensity = 1.0f;
    m_pLightResource->Unmap(0, nullptr);
}

// ★ 変更点: 以前の個別Update関数は使用しないためコメントアウト、または削除して大丈夫です
void TriangleModel::Update(const Vector3& pos, const Vector3& rot, const Vector3& scale) {
    // 行列計算はすべて TUFEngine::RenderGpuDrivenALLRequests 側で行うため、この関数は空で問題ありません
}

void TriangleModel::UpdateVertices(const Vector3& points,
    const Vector2& texcoord,
    const Vector3& normal,
    int index)
{
    if (!m_pVertexData || index < 0 || static_cast<uint32_t>(index) >= m_vertexCount) return;

    m_pVertexData[index].position = { points.x , points.y, points.z, 1.0f };
    m_pVertexData[index].texcoord = texcoord;
    m_pVertexData[index].normal = { normal.x , normal.y, normal.z };
}

void TriangleModel::SetWorldTransform(const Matrix4x4& wvp, const Matrix4x4& world) {
    // 個別の定数バッファは使わないため、この関数も空で問題ありません
}

// 🌟 GPU駆動（インスタンシング対応）の Draw 関数
void TriangleModel::Draw(
    ID3D12GraphicsCommandList* cmdList,
    int textureIndex,
    UINT instanceCount,
    UINT startInstanceLocation) // ヘッダー（.h）の定義と引数を一致させました
{
    if (m_vertexCount == 0 || !m_pVertexResource) return;

    // トポロジーと頂点バッファのセット
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    // マテリアルセット
    cmdList->SetGraphicsRootConstantBufferView(0, m_pMaterialResource->GetGPUVirtualAddress());

    // テクスチャセット
    if (textureIndex >= 0) {
        cmdList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGPUHandle(textureIndex));
    }

 

    cmdList->DrawInstanced(3, instanceCount, 0, startInstanceLocation);
}