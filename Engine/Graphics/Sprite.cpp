#include "Sprite.h"
#include "TUFEngine.h"

Sprite::Sprite() : m_textureIndex(0), m_width(0.0f), m_height(0.0f) {}

Sprite::~Sprite() = default;

void Sprite::InitSprite(TUFEngine* engine, int textureIndex, float w, float h) {
    m_pEngine = engine;
    m_textureIndex = textureIndex;
    m_width = w;
    m_height = h;

    ID3D12Device* device = engine->GetDevice();

    // --- 頂点バッファ ---
    m_pVertexResource = CreateBufferResource(device, sizeof(Vertex) * 4);
    m_pVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&m_pVertexData));

    m_vertexBufferView.BufferLocation = m_pVertexResource->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = sizeof(Vertex) * 4;

    // ピクセル座標で頂点を初期化 (Y-down, (0,0)が左上)
    UpdateVertices({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 0); // 左上
    UpdateVertices({ w,    0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 1); // 右上
    UpdateVertices({ 0.0f, h,    0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, 2); // 左下
    UpdateVertices({ w,    h,    0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, 3); // 右下

    m_pVertexResource->Unmap(0, nullptr);

    // --- マテリアルバッファ ---
    m_pMaterialResource = CreateBufferResource(device, Align256(sizeof(Material)));
    Material* materialData = nullptr;
    m_pMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData->enableLighting = false;
    materialData->uvTransform = MakeIdentity4x4();
    materialData->enableNormalMap = 0;
    m_pMaterialResource->Unmap(0, nullptr);

    // --- WVP（正射影行列）バッファ ---
    m_pWvpResource = CreateBufferResource(device, Align256(sizeof(TransformationMatrix)));
    TransformationMatrix* wvpData = nullptr;
    m_pWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
    // 正射影行列をセット（左上が原点の画面座標系、Y-down）
    wvpData->WVP = MakeOrthographicMatrix(0.0f, 0.0f, w, h, 0.1f, 100.0f);
    wvpData->World = MakeIdentity4x4();
    m_pWvpResource->Unmap(0, nullptr);
}

void Sprite::UpdateVertices(
    const Vector3& point,
    const Vector2& texcoord,
    const Vector3& normal,
    int index)
{
    if (!m_pVertexData) return;

    m_pVertexData[index].position = { point.x, point.y, point.z, 1.0f };
    m_pVertexData[index].texcoord = texcoord;
    m_pVertexData[index].normal = normal;
    m_pVertexData[index].tangent = { 1.0f, 0.0f, 0.0f };
}

void Sprite::SetWorldTransform(const Matrix4x4& wvp, const Matrix4x4& world) {
    TransformationMatrix* wvpData = nullptr;
    m_pWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
    wvpData->WVP = wvp;
    wvpData->World = world;
    m_pWvpResource->Unmap(0, nullptr);
}

void Sprite::SetUVTransform(const Matrix4x4& uvTransform) {
    if (!m_pMaterialResource) return;

    Material* materialData = nullptr;
    m_pMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->uvTransform = uvTransform;
    m_pMaterialResource->Unmap(0, nullptr);
}

void Sprite::Resize(float w, float h) {
    m_width = w;
    m_height = h;

    Vertex* data = nullptr;
    m_pVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&data));

    // InitSprite と完全に同じ並び順にする！(Y-down, (0,0)が左上)
    data[0].position = { 0.0f, 0.0f, 0.0f, 1.0f }; data[0].texcoord = { 0.0f, 0.0f }; data[0].normal = { 0.0f, 0.0f, -1.0f }; data[0].tangent = { 1.0f, 0.0f, 0.0f };
    data[1].position = { w,    0.0f, 0.0f, 1.0f }; data[1].texcoord = { 1.0f, 0.0f }; data[1].normal = { 0.0f, 0.0f, -1.0f }; data[1].tangent = { 1.0f, 0.0f, 0.0f };
    data[2].position = { 0.0f, h,    0.0f, 1.0f }; data[2].texcoord = { 0.0f, 1.0f }; data[2].normal = { 0.0f, 0.0f, -1.0f }; data[2].tangent = { 1.0f, 0.0f, 0.0f };
    data[3].position = { w,    h,    0.0f, 1.0f }; data[3].texcoord = { 1.0f, 1.0f }; data[3].normal = { 0.0f, 0.0f, -1.0f }; data[3].tangent = { 1.0f, 0.0f, 0.0f };

    m_pVertexResource->Unmap(0, nullptr);
}


void Sprite::Draw(ID3D12GraphicsCommandList* cmdList,
    int textureIndex,
    UINT instanceCount,
    UINT startInstanceLocation
)
{
    cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    cmdList->SetGraphicsRootConstantBufferView(0,
        m_pMaterialResource->GetGPUVirtualAddress()
    );

    cmdList->SetGraphicsRootConstantBufferView(1,
        m_pWvpResource->GetGPUVirtualAddress()
    );

    cmdList->SetGraphicsRootDescriptorTable(
        2,
        TextureManager::GetInstance()->GetGPUHandle(textureIndex)
    );

    cmdList->DrawInstanced(4, instanceCount, 0, startInstanceLocation);
}