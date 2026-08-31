#include "Sphere.h"
#include "TUFEngine.h"

constexpr float kPi = 3.14159265355f;

Sphere::~Sphere() = default;

void Sphere::InitSphere(TUFEngine* engine) {
	m_pEngine = engine;
	ID3D12Device* device = engine->GetDevice();

	const uint32_t kSubdivision = 16;
	m_vertexCount = kSubdivision * kSubdivision * 4;
	m_indexCount = kSubdivision * kSubdivision * 6;

	m_pVertexResource = CreateBufferResource(device, sizeof(VertexData) * m_vertexCount);

	VertexData* vertexData = nullptr;
	m_pVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	const float kLonEvery = (kPi * 2.0f) / float(kSubdivision);
	const float kLatEvery = kPi / float(kSubdivision);

	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		const float lat = -kPi / 2.0f + (kLatEvery * latIndex);
		const float nextLat = lat + kLatEvery;
		const float v = float(latIndex) / float(kSubdivision);
		const float nextV = float(latIndex + 1) / float(kSubdivision);

		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			const uint32_t start = (latIndex * kSubdivision + lonIndex) * 4;
			const float lon = lonIndex * kLonEvery;
			const float nextLon = lon + kLonEvery;
			const float u = float(lonIndex) / float(kSubdivision);
			const float nextU = float(lonIndex + 1) / float(kSubdivision);

			vertexData[start + 0].position = { cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon), 1.0f };
			vertexData[start + 0].texcoord = { u, 1.0f - v };
			vertexData[start + 0].normal = { vertexData[start + 0].position.x, vertexData[start + 0].position.y, vertexData[start + 0].position.z };

			vertexData[start + 1].position = { cosf(nextLat) * cosf(lon), sinf(nextLat), cosf(nextLat) * sinf(lon), 1.0f };
			vertexData[start + 1].texcoord = { u, 1.0f - nextV };
			vertexData[start + 1].normal = { vertexData[start + 1].position.x, vertexData[start + 1].position.y, vertexData[start + 1].position.z };

			vertexData[start + 2].position = { cosf(lat) * cosf(nextLon), sinf(lat), cosf(lat) * sinf(nextLon), 1.0f };
			vertexData[start + 2].texcoord = { nextU, 1.0f - v };
			vertexData[start + 2].normal = { vertexData[start + 2].position.x, vertexData[start + 2].position.y, vertexData[start + 2].position.z };

			vertexData[start + 3].position = { cosf(nextLat) * cosf(nextLon), sinf(nextLat), cosf(nextLat) * sinf(nextLon), 1.0f };
			vertexData[start + 3].texcoord = { nextU, 1.0f - nextV };
			vertexData[start + 3].normal = { vertexData[start + 3].position.x, vertexData[start + 3].position.y, vertexData[start + 3].position.z };
		}
	}
	m_pVertexResource->Unmap(0, nullptr);

	m_vertexBufferView.BufferLocation = m_pVertexResource->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = sizeof(VertexData) * m_vertexCount;
	m_vertexBufferView.StrideInBytes = sizeof(VertexData);

	m_pIndexResource = CreateBufferResource(device, sizeof(uint32_t) * m_indexCount);

	uint32_t* indexData = nullptr;
	m_pIndexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			const uint32_t vertexStart = (latIndex * kSubdivision + lonIndex) * 4;
			const uint32_t indexStart = (latIndex * kSubdivision + lonIndex) * 6;

			indexData[indexStart + 0] = vertexStart + 0;
			indexData[indexStart + 1] = vertexStart + 1;
			indexData[indexStart + 2] = vertexStart + 2;
			indexData[indexStart + 3] = vertexStart + 1;
			indexData[indexStart + 4] = vertexStart + 3;
			indexData[indexStart + 5] = vertexStart + 2;
		}
	}
	m_pIndexResource->Unmap(0, nullptr);

	m_indexBufferView.BufferLocation = m_pIndexResource->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = sizeof(uint32_t) * m_indexCount;
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	m_pMaterialResource = CreateBufferResource(device, Align256(sizeof(Material)));
	Material* materialData = nullptr;
	m_pMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableNormalMap = 0;
	materialData->enableLighting = true;//ここは要修正
	materialData->uvTransform = MakeIdentity4x4();
	m_pMaterialResource->Unmap(0, nullptr);

	m_pWvpResource = CreateBufferResource(device, Align256(sizeof(TransformationMatrix)));

	m_pLightResource = CreateBufferResource(device, Align256(sizeof(LightData)));
	LightData* lightData = nullptr;
	m_pLightResource->Map(0, nullptr, reinterpret_cast<void**>(&lightData));
	lightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	lightData->dirOrPos = { 0.0f, -1.0f, 0.0f };
	lightData->intensity = 1.0f;
	m_pLightResource->Unmap(0, nullptr);
}

void Sphere::Update() {
	m_rotation.y += 0.01f;

	Matrix4x4 viewProjectionMatrix = m_pEngine->GetViewProjectionMatrix();
	Matrix4x4 worldMatrix = GetWorldMatrix();
	Matrix4x4 wvpMatrix = Multiply(worldMatrix, viewProjectionMatrix);

	SetWorldTransform(wvpMatrix, worldMatrix);
}

void Sphere::SetWorldTransform(const Matrix4x4& wvp, const Matrix4x4& world) {
	TransformationMatrix* wvpData = nullptr;
	m_pWvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	wvpData->WVP = wvp;
	wvpData->World = world;
	m_pWvpResource->Unmap(0, nullptr);
}

void Sphere::SetLightResource(ID3D12Resource* lightResource) {
	m_pLightResource = lightResource;
}


void Sphere::Draw(ID3D12GraphicsCommandList* cmdList, int textureIndex, UINT instanceCount, UINT startInstanceLocation) {
	cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	cmdList->IASetIndexBuffer(&m_indexBufferView);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cmdList->SetGraphicsRootConstantBufferView(0, m_pMaterialResource->GetGPUVirtualAddress());
	cmdList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGPUHandle(textureIndex));



	cmdList->DrawIndexedInstanced(m_indexCount, instanceCount, 0, 0, startInstanceLocation);
}
