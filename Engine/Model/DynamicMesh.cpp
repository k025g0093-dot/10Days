#include "DynamicMesh.h"

DynamicMesh::DynamicMesh(int gridW, int gridH) : mGridW(gridW), mGridH(gridH) {
    mVertices.resize(gridW * gridH * 3, 0.0f);
    mNormals.resize(gridW * gridH * 3, 0.0f);
    mIndices.reserve((gridW - 1) * (gridH - 1) * 6);

    float width =1.0f;

    // XZ座標を初期化（Yは0.0fのまま）
    for (int y = 0; y < gridH; y++) {
        for (int x = 0; x < gridW; x++) {
            int index = vertexIndex(x, y) * 3;
            mVertices[index] = ((float)x - gridW / 2.0f)* width;  // 中心を0に  // X座標
            mVertices[index + 1] = 0.0f;      // Y座標（高さ）
            mVertices[index + 2] = ((float)y - gridH / 2.0f)* width;  // Z座標
        }
    }

    // 四角形を2つの三角形に分割
    for (int y = 0; y < gridH - 1; y++) {
        for (int x = 0; x < gridW - 1; x++) {
            int tl = vertexIndex(x, y);      // 左上
            int tr = vertexIndex(x + 1, y);    // 右上
            int bl = vertexIndex(x, y + 1);    // 左下
            int br = vertexIndex(x + 1, y + 1); // 右下

            // 三角形①（左下三角）
            mIndices.push_back(tl);  // 左上
            mIndices.push_back(tr);  // 右上
            mIndices.push_back(bl);  // 左下

            // 三角形②（右上三角）
            mIndices.push_back(tr);  // 右上
            mIndices.push_back(br);  // 右下
            mIndices.push_back(bl);  // 左下
        }
    }
}

DynamicMesh::~DynamicMesh() {
    // vectorは自動で解放されるので何も書かなくていい
}

void DynamicMesh::updateHeight(int x, int y, float height) {
    int index = vertexIndex(x, y) * 3; // XYZ
    mVertices[index + 1] = height; // Y座標を更新
}

void DynamicMesh::updateNormal(int x, int y, float nx, float ny, float nz) {
    int index = vertexIndex(x, y) * 3; // XYZ
    mNormals[index] = nx;
    mNormals[index + 1] = ny;
    mNormals[index + 2] = nz;
}
