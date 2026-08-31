// FBXLoader.cpp
#include "FBXLoader.h"

const aiScene* FBXLoader::Load(const std::string& filepath, Assimp::Importer& importer) {
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_ValidateDataStructure |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType |
        aiProcess_ConvertToLeftHanded);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        return nullptr;
    }

    return scene;
}