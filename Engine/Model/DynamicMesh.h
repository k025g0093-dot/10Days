#pragma once
#include <vector>

class DynamicMesh {
public:
    DynamicMesh(int gridW, int gridH);
    ~DynamicMesh();

    // 頂点の高さを更新
    void updateHeight(int x, int y, float height);

    // 法線を更新
    void updateNormal(int x, int y, float nx, float ny, float nz);

    // 頂点データへのアクセス（エンジンに渡す用）
    const std::vector<float>& getVertices() const { return mVertices; }
    const std::vector<float>& getNormals()  const { return mNormals; }
    const std::vector<int>& getIndices()  const { return mIndices; }

    int getGridW() const { return mGridW; }
    int getGridH() const { return mGridH; }

private:
    int mGridW, mGridH;
    std::vector<float> mVertices;  // XYZ × 頂点数
    std::vector<float> mNormals;   // XYZ × 頂点数
    std::vector<int>   mIndices;   // 三角形のインデックス

    int vertexIndex(int x, int y) const { return y * mGridW + x; }
};