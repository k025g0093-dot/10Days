// FBXLoader.h
#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>



class FBXLoader {
public:
    static const aiScene* Load(const std::string& filepath, Assimp::Importer& importer);
};