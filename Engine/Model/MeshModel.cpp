#include "MeshModel.h"
#include "TUFEngine.h"

MeshModel::MeshModel() = default;
MeshModel::~MeshModel() = default;

void MeshModel::InitMeshModel(ID3D12Device* device) {
	device_ = device;

	m_pMaterialResource = CreateBufferResource(device, Align256(sizeof(Material)));
	Material* materialData = nullptr;
	m_pMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = { 0.5f, 0.5f, 0.5f, 0.5f };
	materialData->enableLighting = 1;
	materialData->uvTransform = MakeIdentity4x4();
	materialData->enableNormalMap = 0;
	m_pMaterialResource->Unmap(0, nullptr);
}

bool MeshModel::LoadFromOBJ(
	const std::string& directoryPath,
	const std::string& filename
) {
	std::ifstream file(directoryPath + "/" + filename);
	//assert(file.is_open());

	std::vector<VertexData> faceVertices;

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v")
		{
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		}
		else if (identifier == "vt")
		{
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		}
		else if (identifier == "vn")
		{
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		}
		else if (identifier == "f")
		{
			std::vector<VertexData> currentFaceVertices;
			std::string vertexDefinition;

			while (s >> vertexDefinition) {
				std::istringstream vStream(vertexDefinition);
				uint32_t indices[3] = { 0, 0, 0 };

				for (int32_t i = 0; i < 3; ++i) {
					std::string indexStr;
					if (!std::getline(vStream, indexStr, '/')) break;
					if (!indexStr.empty()) {
						indices[i] = std::stoi(indexStr);
					}
				}

				if (indices[0] == 0) continue;

				uint32_t pIdx = indices[0] - 1;
				uint32_t tIdx = (indices[1] > 0 && indices[1] <= texcoords.size()) ? indices[1] - 1 : 0;
				uint32_t nIdx = (indices[2] > 0 && indices[2] <= normals.size()) ? indices[2] - 1 : 0;

				Vector4 pos = positions[pIdx];
				Vector2 uv = (texcoords.size() > 0) ? texcoords[tIdx] : Vector2{ 0,0 };
				Vector3 norm = (normals.size() > 0) ? normals[nIdx] : Vector3{ 0,0,0 };

				pos.x *= -1.0f;
				norm.x *= -1.0f;

				currentFaceVertices.push_back({ pos, uv, norm });
			}

			for (size_t i = 2; i < currentFaceVertices.size(); ++i) {
				modelData.vertices.push_back(currentFaceVertices[i]);
				modelData.vertices.push_back(currentFaceVertices[i - 1]);
				modelData.vertices.push_back(currentFaceVertices[0]);
			}
		}
		else if (identifier == "mtllib")
		{
			std::string materialFilename;
			s >> materialFilename;

			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);

			if (!modelData.material.textureFilPath.empty())
			{
				int textureIndex = TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilPath);
				SetTextureIndex(textureIndex);
			}

			if (!modelData.material.normalTextureFilePath.empty()) {
				int normalIndex = TextureManager::GetInstance()->LoadTexture(modelData.material.normalTextureFilePath);
				m_normalTextureIndex = normalIndex;
			}
		}
	}

	// LoadFromOBJ のパースが終わった後
	uint32_t vertCount = static_cast<uint32_t>(modelData.vertices.size());
	std::vector<uint32_t> indices(vertCount);
	for (uint32_t i = 0; i < vertCount; i++) indices[i] = i;

	CreateBuffers(modelData.vertices, indices);


	return true;
}


MaterialData MeshModel::LoadMaterialTemplateFile(
	const std::string& directoryPath,
	const std::string& filename
) {
	MaterialData materialData;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	//assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifire;
		std::istringstream s(line);
		s >> identifire;

		if (identifire == "map_Kd")
		{
			std::string token;
			std::string textureFilename;
			while (s >> token) {
				if (token[0] == '-') {
					if (token == "-s" || token == "-o") {
						float tmp; s >> tmp >> tmp >> tmp;
					}
					else if (token == "-bm") {
						float tmp; s >> tmp;
					}
					continue;
				}
				textureFilename = token;
			}

			std::replace(textureFilename.begin(), textureFilename.end(), '\\', '/');

			if (!textureFilename.empty()) {
				materialData.textureFilPath = directoryPath + "/" + textureFilename;
			}
		}
		else if (identifire == "map_Bump" || identifire == "bump") {
			std::string token;
			std::string textureFilename;
			while (s >> token) {
				if (token[0] == '-') {
					if (token == "-s" || token == "-o") {
						float tmp; s >> tmp >> tmp >> tmp;
					}
					else if (token == "-bm") {
						float tmp; s >> tmp;
					}
					continue;
				}
				textureFilename = token;
			}

			std::replace(textureFilename.begin(), textureFilename.end(), '\\', '/');

			if (!textureFilename.empty()) {
				materialData.normalTextureFilePath = directoryPath + "/" + textureFilename;
			}
		}
	}

	return materialData;
}


void MeshModel::Draw(
	ID3D12GraphicsCommandList* cmdList,
	int textureIndex,
	UINT instanceCount,
	UINT startInstanceLocation
)
{
	if (m_vertexCount == 0 || !m_vertexBuffer || !m_indexBuffer) return;

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->SetGraphicsRootConstantBufferView(0, m_pMaterialResource->GetGPUVirtualAddress());

	bool hasNormalTexture = false;

	if (textureIndex >= 0) {
		auto handle = TextureManager::GetInstance()->GetGPUHandle(textureIndex);
		if (handle.ptr != 0) {
			cmdList->SetGraphicsRootDescriptorTable(2, handle);
		}

		if (m_normalTextureIndex >= 0) {
			auto normalHandle = TextureManager::GetInstance()->GetGPUHandle(m_normalTextureIndex);
			if (normalHandle.ptr != 0) {
				cmdList->SetGraphicsRootDescriptorTable(4, normalHandle);
				hasNormalTexture = true;
			}
		}
	}
	else {
		Material* materialData = nullptr;
		m_pMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
		materialData->enableLighting = -1;
		materialData->enableNormalMap = 0;
		m_pMaterialResource->Unmap(0, nullptr);
	}

	if (textureIndex >= 0) {
		Material* materialData = nullptr;
		m_pMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
		materialData->enableNormalMap = hasNormalTexture ? 1 : 0;
		m_pMaterialResource->Unmap(0, nullptr);
	}

	cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	cmdList->IASetIndexBuffer(&m_indexBufferView);  // ★ 追加

	// ★ DrawInstanced → DrawIndexedInstanced に変更
	// Sphere と描画命令を統一することでインスタンスバッファのズレをなくす
	cmdList->DrawIndexedInstanced(
		m_indexCount,
		instanceCount,
		0,
		0,
		startInstanceLocation
	);
}

void MeshModel::SetEnableLighting(int val) {
	if (!m_pMaterialResource) return;
	Material* materialData = nullptr;
	m_pMaterialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->enableLighting = val;
	m_pMaterialResource->Unmap(0, nullptr);
}

void MeshModel::UpdateVertices(
	const Vector3& points,
	const Vector2& texcoord,
	const Vector3& normal,
	int index)
{
	if (!m_vertexBuffer || index < 0 || static_cast<uint32_t>(index) >= m_vertexCount) return;

	void* pData = nullptr;
	m_vertexBuffer->Map(0, nullptr, &pData);

	VertexData* vertices = static_cast<VertexData*>(pData);

	vertices[index].position = { points.x , points.y, points.z * -1.0f , 0.0f };
	vertices[index].texcoord = texcoord;
	vertices[index].normal = { normal.x , normal.y, normal.z * -1.0f };
	vertices[index].tangent = { 0.0f, 0.0f, 0.0f };

	m_vertexBuffer->Unmap(0, nullptr);
}

//=============FBX形式のモデルロード関数====================
bool MeshModel::LoadFormFBX(const std::string& filepath) {

	//ファイルが合っているか確認、間違っていたりすればfalseを返す
	Assimp::Importer importer;
	const aiScene* scene = FBXLoader::Load(filepath, importer);
	if (!scene || !scene->mRootNode)return false;

	//頂点インデックスを集める
	std::vector<VertexData>vertices;
	std::vector<uint32_t>indices;

	ProcessNode(scene->mRootNode, scene, vertices, indices); // 再帰トラバース

	//バッファ作成
	m_vertexCount = (uint32_t)vertices.size();
	m_indexCount = (uint32_t)indices.size();

	// modelData.vertices に移してから CreateBuffers を呼ぶ（m_pVertexDataがmodelData.verticesを指すように）
	modelData.vertices = std::move(vertices);
	CreateBuffers(modelData.vertices, indices);

	if (scene->mNumMaterials > 0) {
		aiMaterial* mtl = scene->mMaterials[0];
		aiString path;
		if (mtl->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
			std::string texPath = path.C_Str();
			// ファイル名だけの相対パスならFBXと同じディレクトリから探す
			if (texPath.find('/') == std::string::npos && texPath.find('\\') == std::string::npos) {
				std::string dir = filepath.substr(0, filepath.find_last_of("/\\"));
				texPath = dir + "/" + texPath;
			}
			// ファイルが存在しなければスキップ
			if (GetFileAttributesA(texPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
				int texIndex = TextureManager::GetInstance()->LoadTexture(texPath);
				SetTextureIndex(texIndex);
			}
		}
		// normal map, color なども同様に
	}
	return true;

}

void MeshModel::ProcessNode(aiNode* node, const aiScene* scene,
	std::vector<VertexData>& vertices, std::vector<uint32_t>& indices,
	const aiMatrix4x4& parentTransform)
{
	aiMatrix4x4 nodeTransform = parentTransform * node->mTransformation;

	for (UINT i = 0; i < node->mNumMeshes; i++) {
		unsigned int meshIndex = node->mMeshes[i];
		if (meshIndex >= scene->mNumMeshes) continue;
		aiMesh* mesh = scene->mMeshes[meshIndex];
		if (!mesh || !mesh->mVertices) continue;
		UINT baseVertex = (UINT)vertices.size();
		for (UINT v = 0; v < mesh->mNumVertices; v++) {
			VertexData vert{};
			aiVector3D pos = nodeTransform * mesh->mVertices[v];
			vert.position = { pos.x, pos.y, pos.z, 1.0f };
			if (mesh->mTextureCoords[0]) {
				vert.texcoord = { mesh->mTextureCoords[0][v].x, 1.0f - mesh->mTextureCoords[0][v].y };
			}
			if (mesh->mNormals) {
				aiVector3D nm = aiMatrix3x3(nodeTransform) * mesh->mNormals[v];
				vert.normal = { nm.x, nm.y, nm.z };
			}
			if (mesh->mTangents) {
				vert.tangent = { mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z };
			}
			vertices.push_back(vert);
		}
		for (UINT f = 0; f < mesh->mNumFaces; f++) {
			aiFace& face = mesh->mFaces[f];
			for (UINT idx = 0; idx < face.mNumIndices; idx++)
				indices.push_back(baseVertex + face.mIndices[idx]);
		}
	}
	for (UINT i = 0; i < node->mNumChildren; i++)
		if (node->mChildren[i])
			ProcessNode(node->mChildren[i], scene, vertices, indices, nodeTransform);
}


//バッファの作成関数
void MeshModel::CreateBuffers(std::vector<VertexData>& vertices, std::vector<uint32_t>& indices)
{
	m_vertexCount = (uint32_t)vertices.size();
	m_indexCount = (uint32_t)indices.size();

	// 頂点バッファ
	m_vertexBuffer = CreateBufferResource(device_, sizeof(VertexData) * m_vertexCount);
	void* pData = nullptr;
	m_vertexBuffer->Map(0, nullptr, &pData);
	memcpy(pData, vertices.data(), sizeof(VertexData) * m_vertexCount);
	m_vertexBuffer->Unmap(0, nullptr);

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = sizeof(VertexData) * (UINT)m_vertexCount;
	m_vertexBufferView.StrideInBytes = sizeof(VertexData);

	m_pVertexData = reinterpret_cast<Vertex*>(vertices.data());

	// インデックスバッファ
	m_indexBuffer = CreateBufferResource(device_, sizeof(uint32_t) * m_indexCount);
	uint32_t* idxData = nullptr;
	m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&idxData));
	memcpy(idxData, indices.data(), sizeof(uint32_t) * m_indexCount);
	m_indexBuffer->Unmap(0, nullptr);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = sizeof(uint32_t) * m_indexCount;
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}
