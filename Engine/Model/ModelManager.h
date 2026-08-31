#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "MeshModel.h"
#include "TextureManager.h"
#include "TUFEngine.h" // SceneObject, OBB, Vector3 のため

class ModelManager {
public:
    static ModelManager* GetInstance();

    void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

    // モデル読み込み + キャッシュ
    MeshModel* LoadModel(const std::string& directoryPath, const std::string& filename);

    // シーンオブジェクト管理
    void AddSceneObject(const std::string& modelPath, const Transform& transform);
    void RemoveSceneObject(int index);
    std::vector<SceneObject>& GetSceneObjects() { return m_droppedMeshes; }

    // ドロップ処理（今 TUFEngine::OnFileDropped にある内容）
    void OnFileDropped(const std::wstring& path);

    // 永続化
    void SaveToFile();
    void LoadFromFile();

private:
    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

    std::map<std::string, std::unique_ptr<MeshModel>> m_meshes;
    std::vector<SceneObject> m_droppedMeshes;

    ID3D12Device* m_device = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;
};