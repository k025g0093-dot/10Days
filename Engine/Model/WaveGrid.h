#pragma once
#include <vector>
#include <cmath>
#include <wrl/client.h>
#include "WaveGridPSO.h"
#include "allVector.h"

// 前方宣言
struct SceneObject;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;
struct ID3D12PipelineState;
struct ID3D12RootSignature;
class TUFEngine;

using Microsoft::WRL::ComPtr;

// ========================
// WaveParams: HLSL cbuffer と同じレイアウト
// ========================
struct WaveParams {
    float gTime;
    float gWaveFreq;
    float gWaveStrength;
    float gDamping;
    float gMul;
    uint32_t gWidth;
    uint32_t gHeight;
    uint32_t gPad;
};

// ========================
// WaveGrid クラス
// ========================
class WaveGrid {
public:
    // ========== Constructor / Destructor ==========
    WaveGrid(int width, int height, std::vector<SceneObject>& sceneObjects);
    ~WaveGrid();

    // ========== GPU 初期化・実行 ==========
    void InitializeGPU(ID3D12Device* device, TUFEngine* engine);
    void DispatchWaveSimulation(float time, float freq, float strength);
    void ReadbackToCPU();

    //========== 初期化関数 ==============
    void InitializeGPUBuffers();  // GPU バッファを 0 で初期化
    void UploadWallDataToGPU();


    // ========== CPU 側メソッド（従来） ==========
    void update();
    void addSource(int x, int y, float strength);
    void setWall(int x, int y, bool isWall);
    bool isWall(int x, int y);
    void setObjectWall(const std::vector<SceneObject>& objects);
    float getHeight(int x, int y);
    void reset();
    int valueIndex(int x, int y) const;

    // ========== Getter for Cached Data ==========
    float GetHeightFromCache(int x, int y) const;

    Vector4 GetNormalFromCache(int x, int z) const {
        return mNormalCPUCache[z * mWidth + x];
    }

    // ========== 構造体定義 ==========
    struct Source {
        int x, y;
        float strength;
        float lifetime;
    };

    struct Normal {
        float x, y, z;
    };

    Normal getNormal(int x, int y);
    
    // ========== Public メンバ（設定値など） ==========
    int mWidth, mHeight;
    float mC;      // 波の速さ
    float mDeltaX; // グリッド間隔

    std::vector<SceneObject>& sceneObjects;
    std::vector<Source> mSources;
    std::vector<float> mCurrent;

private:
    // ========== CPU 側バッファ（デバッグ用に残す） ==========
    std::vector<float> mPrevious;
    std::vector<float> mNext;
    std::vector<bool>  mWall;
    std::vector<Vector4>mNormalCPUCache;//サイズ:mWidth*mHeight

    // ========== GPU リソース ==========
    // ConstantBuffer
    ComPtr<ID3D12Resource> mConstBufferWaveParams;
    void* mParamsBufferMappedPtr;  // マップ済みポインタ

    // Compute 用バッファ（StructuredBuffer<float>）
    ComPtr<ID3D12Resource> mCurrentBuffer;    // u0 でも使う（可視用）
    ComPtr<ID3D12Resource> mPreviousBuffer;   // u1
    ComPtr<ID3D12Resource> mNextBuffer;       // u2
    ComPtr<ID3D12Resource> mNormalBuffer;     // u3 (UAV) - 後で


    // Wall バッファ（StructuredBuffer<uint>）
    ComPtr<ID3D12Resource> mWallBuffer;       // t0 (SRV)


    // Staging Buffer（Readback用）
    ComPtr<ID3D12Resource> mHeightStaging;
    ComPtr<ID3D12Resource> mNormalStaging;    // READBACK用


    // ========== PSO / RootSignature ==========
    ComPtr<ID3D12PipelineState> mComputePSO;
    ComPtr<ID3D12RootSignature> mRootSignature;

    // ========== CPU キャッシュ（フレーム遅延対応） ==========
    std::vector<float> mHeightCPUCache;

    // ========== 状態フラグ ==========
    bool mIsGPUReady;

    // ========== エンジン参照 ==========
    ID3D12Device* mDevice;
    TUFEngine* mEngine;

    //=========== ハンドル関係 ==========
    D3D12_CPU_DESCRIPTOR_HANDLE mWallSrvCpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE mWallSrvGpuHandle{};

    D3D12_CPU_DESCRIPTOR_HANDLE mWaveUavCpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE mWaveUavGpuHandle{};

    // ========== Private Helper Methods ==========
    void CreateGPUResources();
    void CreateConstantBuffer();
    void CreateStructuredBuffers();
    void CreateStagingBuffers();
    void CreateDescriptorViews();
    void CreateWaveUavDescriptors();
};