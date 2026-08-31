#include "ModelManager.h"
#include "Create3DObjectOBB.h"
#include "ConvertString.h"
#include <fstream>
#include "GameScript.h"
#include "Rigidbody.h"
#include "Collider.h"
#include "BoxCollider.h"
#include "SphereCollider.h"
#include "ConvexHullCollider.h"




MeshModel* ModelManager::LoadModel(const std::string& directoryPath, const std::string& filename) {

	const std::string key = directoryPath + "/" + filename;   // ★ 追加

	if (m_meshes.count(key) > 0) {                            // ★ filename → key
		return m_meshes[key].get();                           // ★ filename → key
	}

	auto mesh = std::make_unique<MeshModel>();
	mesh->InitMeshModel(m_device);

	std::string ext = filename.substr(filename.find_last_of("."));
	bool isFBX = (ext == ".fbx" || ext == ".FBX");

	if (isFBX) {
		if (!mesh->LoadFormFBX(directoryPath + "/" + filename)) {
			OutputDebugStringA(("Error: Failed to load FBX: " + directoryPath + "/" + filename + "\n").c_str());
			return nullptr;
		}
		// FBX側のマテリアルからテクスチャ読み込み（LoadFormFBX内でやる前提）
	}
	else {
		if (!mesh->LoadFromOBJ(directoryPath, filename)) {
			OutputDebugStringA(("Error: Failed to load OBJ: " + directoryPath + "/" + filename + "\n").c_str());
			return nullptr;
		}

		// フォールバックテクスチャ自動検出（OBJのみ）
		std::string baseName = filename;
		size_t lastDot = filename.find_last_of(".");
		if (lastDot != std::string::npos) {
			baseName = filename.substr(0, lastDot);
		}
		std::string folderAndBase = directoryPath + "/" + baseName;
		std::string texPath = "";
		if (GetFileAttributesA((folderAndBase + ".jpg").c_str()) != INVALID_FILE_ATTRIBUTES) {
			texPath = folderAndBase + ".jpg";
		}
		else if (GetFileAttributesA((folderAndBase + ".png").c_str()) != INVALID_FILE_ATTRIBUTES) {
			texPath = folderAndBase + ".png";
		}
		if (!texPath.empty()) {
			mesh->SetTextureIndex(TextureManager::GetInstance()->LoadTexture(texPath));
		}
		std::string normalMapPath = folderAndBase + "_normal.png";
		if (GetFileAttributesA(normalMapPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
			mesh->SetNormalMapIndex(TextureManager::GetInstance()->LoadTexture(normalMapPath));
		}
	}

	MeshModel* ptr = mesh.get();
	m_meshes[key] = std::move(mesh);                          // ★ filename → key
	return ptr;
}


void ModelManager::OnFileDropped(const std::wstring& path) {
	std::filesystem::path filePath(path);
	std::string ext = filePath.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".png" || ext == ".jpg") {
		std::string pathStr = ConvertString(path); // パスを文字列に変換する
		int index = TextureManager::GetInstance()->LoadTexture("resources/" + pathStr);
		OutputDebugStringA(("Texture Loaded: " + pathStr + "\n").c_str());
	}
	else if (ext == ".obj" || ext == ".fbx") {
		std::string dir = filePath.parent_path().string();
		std::string filename = filePath.filename().string();
		MeshModel* mesh = LoadModel("resources/" + dir, filename);
		if (mesh) {
			std::string modelPath = "resources/" + dir + "/" + filename;
			Create3DObjectOBB obbCreator;
			OBB obb = obbCreator.CreateOBBForModel(*mesh, { 0.0f, 0.0f, 0.0f });

			// ローカルAABBをキャッシュ
			const Vertex* verts = mesh->GetVertexData();
			UINT vCount = mesh->GetVertexCount();
			Vector3 lMin = { FLT_MAX, FLT_MAX, FLT_MAX }, lMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			for (UINT j = 0; j < vCount; j++) {
				if (verts[j].position.x < lMin.x) lMin.x = verts[j].position.x;
				if (verts[j].position.x > lMax.x) lMax.x = verts[j].position.x;
				if (verts[j].position.y < lMin.y) lMin.y = verts[j].position.y;
				if (verts[j].position.y > lMax.y) lMax.y = verts[j].position.y;
				if (verts[j].position.z < lMin.z) lMin.z = verts[j].position.z;
				if (verts[j].position.z > lMax.z) lMax.z = verts[j].position.z;
			}

			Transform t;
			t.position = { 0.0f, 0.0f, 0.0f };
			t.rotation = { 0.0f, 0.0f, 0.0f };
			t.scale = { 1.0f, 1.0f, 1.0f };

			auto* entity = EntityManager::GetInstance()->CreateEntity(modelPath);
			auto* meshComponent = entity->AddComponent<MeshFilter>();
			meshComponent->model = mesh;
			entity->transform = t;
			entity->obb = obb;
			entity->localAABB = { lMin, lMax };


			SaveToFile();
		}


	}
	else {
		OutputDebugStringA("Unknown file type dropped\n");
	}
}



void ModelManager::SaveToFile() {

	json data;
	data["objects"] = json::array();

	for (auto& entity : EntityManager::GetInstance()->GetEntities()){
		json obj;

		obj["modelPath"] = entity->name;
		obj["displayName"] = entity->displayName;
		obj["position"] = { entity->transform.position.x, entity->transform.position.y, entity->transform.position.z };
		obj["rotation"] = { entity->transform.rotation.x, entity->transform.rotation.y, entity->transform.rotation.z };
		obj["scale"] = { entity->transform.scale.x, entity->transform.scale.y, entity->transform.scale.z };

		obj["obb"]["center"] = { entity->obb.center.x, entity->obb.center.y, entity->obb.center.z };
		obj["obb"]["size"] = { entity->obb.size.x, entity->obb.size.y, entity->obb.size.z };
		obj["obb"]["orientationX"] = { entity->obb.orientations[0].x, entity->obb.orientations[0].y, entity->obb.orientations[0].z };
		obj["obb"]["orientationY"] = { entity->obb.orientations[1].x, entity->obb.orientations[1].y, entity->obb.orientations[1].z };
		obj["obb"]["orientationZ"] = { entity->obb.orientations[2].x, entity->obb.orientations[2].y, entity->obb.orientations[2].z };

		auto* gs = entity->GetComponent<GameScript>();
		if (gs) {
			obj["scriptName"] = gs->m_scriptName;
		}

		if (auto* rb = entity->GetComponent<Rigidbody>()) {
			obj["rigidbody"] = {
				{ "mass", rb->mass },
				{ "linearDrag", rb->linearDrag },
				{ "angularDrag", rb->angularDrag },
				{ "useGravity", rb->useGravity },
				{ "isKinematic", rb->isKinematic }
			};
		}

		if (auto* box = entity->GetComponent<BoxCollider>()) {
			obj["collider"] = {
				{ "type", "Box" },
				{ "size", { box->size.x, box->size.y, box->size.z } },
				{ "isTrigger", box->isTrigger }
			};
		}
		else if (auto* sphere = entity->GetComponent<SphereCollider>()) {
			obj["collider"] = {
				{ "type", "Sphere" },
				{ "radius", sphere->radius },
				{ "isTrigger", sphere->isTrigger }
			};
		}
		else if (auto* hull = entity->GetComponent<ConvexHullCollider>()) {
			obj["collider"] = {
				{ "type", "ConvexHull" },
				{ "isTrigger", hull->isTrigger }
			};
		}

		data["objects"].push_back(obj);
	}

	std::ofstream file("sceneObject.json");
	file << data.dump(4);

}

ModelManager* ModelManager::GetInstance() {
	static ModelManager instance;
	return &instance;
}

void ModelManager::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList) {
	m_device = device;
	m_commandList = cmdList;
}

void ModelManager::AddSceneObject(const std::string& modelPath, const Transform& transform) {
	MeshModel* mesh = LoadModel(
		modelPath.substr(0, modelPath.find_last_of("/")),
		modelPath.substr(modelPath.find_last_of("/") + 1)
	);
	if (!mesh) return;

	Create3DObjectOBB obbCreator;
	OBB obb = obbCreator.CreateOBBForModel(*mesh, transform.position);

	const Vertex* verts = mesh->GetVertexData();
	UINT vCount = mesh->GetVertexCount();
	Vector3 lMin = { FLT_MAX, FLT_MAX, FLT_MAX }, lMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (UINT j = 0; j < vCount; j++) {
		if (verts[j].position.x < lMin.x) lMin.x = verts[j].position.x;
		if (verts[j].position.x > lMax.x) lMax.x = verts[j].position.x;
		if (verts[j].position.y < lMin.y) lMin.y = verts[j].position.y;
		if (verts[j].position.y > lMax.y) lMax.y = verts[j].position.y;
		if (verts[j].position.z < lMin.z) lMin.z = verts[j].position.z;
		if (verts[j].position.z > lMax.z) lMax.z = verts[j].position.z;
	}

	m_droppedMeshes.push_back({ modelPath, mesh, transform, obb, { lMin, lMax } });
	SaveToFile();
}

void ModelManager::RemoveSceneObject(int index) {
	m_droppedMeshes.erase(m_droppedMeshes.begin() + index);
	SaveToFile();
}
