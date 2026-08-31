#include "WaveGrid.h"
#include "TUFEngine.h"
#include "PSO.h"
#include <algorithm>

WaveGrid::WaveGrid(int width, int height, std::vector<SceneObject>& sceneObjects)
    : mWidth(width), mHeight(height), sceneObjects(sceneObjects), mC(40.0f), mDeltaX(1.0f)
{
    mCurrent.resize(mWidth * mHeight, 0.0f);
    mPrevious.resize(mWidth * mHeight, 0.0f);
    mNext.resize(mWidth * mHeight, 0.0f);
    mWall.resize(mWidth * mHeight, false);

}

WaveGrid::~WaveGrid() {
    // vectorは自動で解放されるので何も書かなくていい
}

void WaveGrid::update() {
    // 各波源を処理
    for (auto& s : mSources) {
        // 波源の位置に強度を加える
        int index = s.y * mWidth + s.x;
        mCurrent[index] += s.strength * s.lifetime;

        // 寿命を減らす（減衰）
        s.lifetime -= 0.05f;
    }

    // 寿命が尽きた波源を削除
    mSources.erase(
        std::remove_if(mSources.begin(), mSources.end(),
            [](const Source& s) { return s.lifetime <= 0.0f; }),
        mSources.end()
    );

    setObjectWall(sceneObjects);

    // 波の更新（簡略化した例）
    for (int y = 1; y < mHeight - 1; ++y) {
        for (int x = 1; x < mWidth - 1; ++x) {
            int index = y * mWidth + x;
            if (mWall[index]) {
                mNext[index] = 0.0f; // 壁は波を伝えない
            }
            else {
                float deltaT = 1.0f / 60.0f;
                float mul = deltaT * deltaT * mC * mC / (mDeltaX * mDeltaX);

                float u = mCurrent[index];
                float uPre = mPrevious[index];
                float uL = mCurrent[index - 1];
                float uR = mCurrent[index + 1];
                float uT = mCurrent[index - mWidth];
                float uB = mCurrent[index + mWidth];
                // 減衰係数（1.0に近いほどゆっくり消える）
                float damping = 0.993f;

                mNext[index] = damping * (u + u - uPre + mul * (-4.0f * u + uL + uR + uT + uB));
            }
        }
    }
    // フレームをローテーション
    std::swap(mPrevious, mCurrent);
    std::swap(mCurrent, mNext);

}

// addSource の実装
void WaveGrid::addSource(int x, int y, float strength) {
    Source s;
    s.x = x;
    s.y = y;
    s.strength = strength;
    s.lifetime = 1.0f;  // 最大1.0からスタート
    mSources.push_back(s);  // vectorに追加
}

void WaveGrid::setObjectWall(const std::vector<SceneObject>& objects) {
    std::fill(mWall.begin(), mWall.end(), false);
    for (const auto& object : objects) {

        // ---- Y軸交差判定: localAABB + scale を使って正確に ----
        float worldMinY = object.transform.position.y + object.localAABB.min.y * object.transform.scale.y;
        float worldMaxY = object.transform.position.y + object.localAABB.max.y * object.transform.scale.y;
        if (worldMinY > 0.0f || worldMaxY < 0.0f) {
            continue;
        }

        // ---- XZ OBB判定: 回転に対応 ----
        // OBB中心をグリッド座標に変換
        float cx = object.obb.center.x / mDeltaX + mWidth * 0.5f;
        float cz = object.obb.center.z / mDeltaX + mHeight * 0.5f;

        // OBBのX軸・Z軸（XZ平面に投影、すでに正規化済み）
        float axx = object.obb.orientations[0].x;
        float axz = object.obb.orientations[0].z;
        float azx = object.obb.orientations[2].x;
        float azz = object.obb.orientations[2].z;

        // 半サイズ（グリッド単位、既にscale適用済み）
        float hx = object.obb.size.x / mDeltaX;
        float hz = object.obb.size.z / mDeltaX;

        // OBBを囲むワールドAABB（グリッド座標）を計算して走査範囲を絞る
        float extentX = fabsf(axx * hx) + fabsf(azx * hz);
        float extentZ = fabsf(axz * hx) + fabsf(azz * hz);

        int minX = (std::max)(0, (int)floorf(cx - extentX));
        int maxX = (std::min)(mWidth - 1, (int)ceilf(cx + extentX));
        int minZ = (std::max)(0, (int)floorf(cz - extentZ));
        int maxZ = (std::min)(mHeight - 1, (int)ceilf(cz + extentZ));

        for (int z = minZ; z <= maxZ; z++) {
            for (int x = minX; x <= maxX; x++) {
                float gx = (float)x - cx;
                float gz = (float)z - cz;

                // グリッド中心をOBBの各軸に射影 → 半サイズ内なら壁
                float projX = fabsf(gx * axx + gz * axz);
                float projZ = fabsf(gx * azx + gz * azz);

                if (projX <= hx && projZ <= hz) {
                    setWall(x, z, true);
                }
            }
        }
    }
}


void WaveGrid::setWall(int x, int y, bool isWall) {
    mWall[y * mWidth + x] = isWall;
}


float WaveGrid::getHeight(int x, int y) {
    return mCurrent[y * mWidth + x];  // グリッドの値を返すだけ

}

void WaveGrid::reset() {
    std::fill(mCurrent.begin(), mCurrent.end(), 0.0f);
    std::fill(mPrevious.begin(), mPrevious.end(), 0.0f);
    std::fill(mNext.begin(), mNext.end(), 0.0f);
    mSources.clear();
}

int WaveGrid::valueIndex(int x, int y) const {
    return y * mWidth + x;
}

WaveGrid::Normal WaveGrid::getNormal(int x, int y) {
    // 端の処理（範囲外アクセス防止）
    int left = x > 0 ? x - 1 : x;
    int right = x < mWidth - 1 ? x + 1 : x;
    int up = y > 0 ? y - 1 : y;
    int down = y < mHeight - 1 ? y + 1 : y;

    float hL = getHeight(left, y);
    float hR = getHeight(right, y);
    float hU = getHeight(x, up);
    float hD = getHeight(x, down);

    float nx = hL - hR;
    float ny = 2.0f;
    float nz = hU - hD;

    // 正規化
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len > 0.0f) {
        nx /= len;
        ny /= len;
        nz /= len;
    }

    return { nx, ny, nz };
}

//======================================================
//GPU実装変
//======================================================

//初期化処理
void WaveGrid::InitializeGPU(ID3D12Device* device, TUFEngine* engine)
{
    assert(device && "Device is null");
    assert(engine && "Engine is null");

    mDevice = device;
    mEngine = engine;
    mParamsBufferMappedPtr = nullptr;

    OutputDebugStringA("=== InitializeGPU start ===\n");

    CreateGPUResources();
    OutputDebugStringA("CreateGPUResources done\n");

    HRESULT hr = S_OK;
    mRootSignature = WaveGridCreateComputeRootSignature(device, hr);
    OutputDebugStringA("RootSignature created\n");

    if (FAILED(hr)) {
        OutputDebugStringA("ERROR: RootSignature creation failed\n");
        assert(false);
        return;
    }

    mComputePSO = WaveGridCreateComputePipelineState(device, mRootSignature, hr);
    OutputDebugStringA("ComputePSO created\n");

    if (FAILED(hr)) {
        OutputDebugStringA("ERROR: ComputePSO creation failed\n");
        assert(false);
        return;
    }

    mHeightCPUCache.resize(mWidth * mHeight, 0.0f);
    mNormalCPUCache.resize(mWidth * mHeight);

    mIsGPUReady = true;
    OutputDebugStringA("=== InitializeGPU done ===\n");
}

void WaveGrid::DispatchWaveSimulation(float time, float freq, float strength)
{
    if (!mIsGPUReady || !mEngine) {
        return;
    }

    UploadWallDataToGPU();

    WaveParams params{};
    params.gTime = time;
    params.gWaveFreq = freq;
    params.gWaveStrength = strength;
    params.gDamping = 0.993f;
    params.gMul = (1.0f / 60.0f) * (1.0f / 60.0f) * mC * mC / (mDeltaX * mDeltaX);
    params.gWidth = static_cast<uint32_t>(mWidth);
    params.gHeight = static_cast<uint32_t>(mHeight);
    params.gPad = 0;

    if (mParamsBufferMappedPtr) {
        memcpy(mParamsBufferMappedPtr, &params, sizeof(WaveParams));
    }

    ID3D12GraphicsCommandList* cmdList = mEngine->GetCommandList();
    if (!cmdList) {
        return;
    }

    ID3D12DescriptorHeap* heaps[] = {
        mEngine->GetSrvDescriptorHeap()
    };
    cmdList->SetDescriptorHeaps(1, heaps);

    cmdList->SetComputeRootSignature(mRootSignature.Get());
    cmdList->SetPipelineState(mComputePSO.Get());

    cmdList->SetComputeRootConstantBufferView(
        0,
        mConstBufferWaveParams->GetGPUVirtualAddress()
    );

    // rootParameter[1] : t0 = gWall
    cmdList->SetComputeRootDescriptorTable(1, mWallSrvGpuHandle);

    // rootParameter[2] : u0,u1,u2 = gCurr,gPrev,gNext
    cmdList->SetComputeRootDescriptorTable(2, mWaveUavGpuHandle);

    uint32_t dispatchX = (mWidth + 7) / 8;
    uint32_t dispatchY = (mHeight + 7) / 8;
    cmdList->Dispatch(dispatchX, dispatchY, 1);

    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = mNextBuffer.Get();
    cmdList->ResourceBarrier(1, &uavBarrier);

    D3D12_RESOURCE_BARRIER toCopySource =
        CD3DX12_RESOURCE_BARRIER::Transition(
            mNextBuffer.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        );
    cmdList->ResourceBarrier(1, &toCopySource);

    if (mNextBuffer && mHeightStaging) {
        cmdList->CopyResource(mHeightStaging.Get(), mNextBuffer.Get());
    }

    D3D12_RESOURCE_BARRIER backToUav =
        CD3DX12_RESOURCE_BARRIER::Transition(
            mNextBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        );
    cmdList->ResourceBarrier(1, &backToUav);


    // Normal も UAV barrier + transition + copy + 戻し
    D3D12_RESOURCE_BARRIER normalUavBarrier{};
    normalUavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    normalUavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    normalUavBarrier.UAV.pResource = mNormalBuffer.Get();
    cmdList->ResourceBarrier(1, &normalUavBarrier);

    D3D12_RESOURCE_BARRIER normalToCopy =
        CD3DX12_RESOURCE_BARRIER::Transition(
            mNormalBuffer.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        );
    cmdList->ResourceBarrier(1, &normalToCopy);

    if (mNormalBuffer && mNormalStaging) {
        cmdList->CopyResource(mNormalStaging.Get(), mNormalBuffer.Get());
    }

    D3D12_RESOURCE_BARRIER normalBackToUav =
        CD3DX12_RESOURCE_BARRIER::Transition(
            mNormalBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        );
    cmdList->ResourceBarrier(1, &normalBackToUav);



    std::swap(mPreviousBuffer, mCurrentBuffer);
    std::swap(mCurrentBuffer, mNextBuffer);

    // buffer を swap したので、u0/u1/u2 の UAV descriptor は作り直す必要あり
    CreateWaveUavDescriptors();
}


void WaveGrid::ReadbackToCPU() {
    // --- Height readback（既存）---
    if (mHeightStaging) {
        void* mappedPtr = nullptr;
        D3D12_RANGE readRange = { 0, mWidth * mHeight * sizeof(float) };
        HRESULT hr = mHeightStaging->Map(0, &readRange, &mappedPtr);
        if (SUCCEEDED(hr) && mappedPtr) {
            memcpy(mHeightCPUCache.data(), mappedPtr, mHeightCPUCache.size() * sizeof(float));
            mHeightStaging->Unmap(0, nullptr);
        }
    }

    // --- Normal readback（追加）---
    if (mNormalStaging) {
        void* mappedPtr = nullptr;
        D3D12_RANGE readRange = { 0, mWidth * mHeight * sizeof(float) * 4 };
        HRESULT hr = mNormalStaging->Map(0, &readRange, &mappedPtr);
        if (SUCCEEDED(hr) && mappedPtr) {
            memcpy(mNormalCPUCache.data(), mappedPtr, mNormalCPUCache.size() * sizeof(Vector4));
            mNormalStaging->Unmap(0, nullptr);
        }
    }
}

float WaveGrid::GetHeightFromCache(int x, int y) const
{
    if (x < 0 || x >= mWidth || y < 0 || y >= mHeight) {
        return 0.0f;
    }
    return mHeightCPUCache[y * mWidth + x];
}

void WaveGrid::InitializeGPUBuffers()
{
    //コマンドリストの取得
    auto cmdList = mEngine->GetCommandList();

    //0で初期化する
    uint32_t bufferSize = mWidth * mHeight * sizeof(float);
    std::vector<float> zeroData(mWidth * mHeight, 0.0f);

    //バッファーの取得
    D3D12_HEAP_PROPERTIES uploadHeap =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    //ComPtrで初期化
    ComPtr<ID3D12Resource> uploadBuffer;
    
    //リソースの取得
    HRESULT hr = mDevice->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())
    );
    assert(SUCCEEDED(hr));//壊れてないかの確認

    void* mappedPtr = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    uploadBuffer->Map(0, &readRange, &mappedPtr);
    memcpy(mappedPtr, zeroData.data(), bufferSize);
    uploadBuffer->Unmap(0, nullptr);

    //ボックスの設定
    D3D12_BOX sourceRegion{};
    sourceRegion.left = 0;
    sourceRegion.top = 0;
    sourceRegion.front = 0;
    sourceRegion.right = bufferSize;
    sourceRegion.bottom = 1;
    sourceRegion.back = 1;

    D3D12_RESOURCE_BARRIER barrierToDest1 = CD3DX12_RESOURCE_BARRIER::Transition(
        mCurrentBuffer.Get(),  // ← Current を COPY_DEST に
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    cmdList->ResourceBarrier(1, &barrierToDest1);

    cmdList->CopyBufferRegion(
        mCurrentBuffer.Get(), 0,  // ← Current へコピー
        uploadBuffer.Get(), 0,
        bufferSize
    );

    D3D12_RESOURCE_BARRIER barrierToUav1 = CD3DX12_RESOURCE_BARRIER::Transition(
        mCurrentBuffer.Get(),  // ← Current を戻す
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    cmdList->ResourceBarrier(1, &barrierToUav1);

    // ========== mPreviousBuffer ==========
    D3D12_RESOURCE_BARRIER barrierToDest2 = CD3DX12_RESOURCE_BARRIER::Transition(
        mPreviousBuffer.Get(),  // ← Previous を COPY_DEST に
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    cmdList->ResourceBarrier(1, &barrierToDest2);

    cmdList->CopyBufferRegion(
        mPreviousBuffer.Get(), 0,  // ← Previous へコピー
        uploadBuffer.Get(), 0,
        bufferSize
    );

    D3D12_RESOURCE_BARRIER barrierToUav2 = CD3DX12_RESOURCE_BARRIER::Transition(
        mPreviousBuffer.Get(),  // ← Previous を戻す
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    cmdList->ResourceBarrier(1, &barrierToUav2);


    // --- Normal buffer の初期化 ---
    uint32_t normalBufferSize = mWidth * mHeight * sizeof(float) * 4;
    std::vector<float> normalZeroData(mWidth * mHeight * 4, 0.0f);
    D3D12_RESOURCE_DESC normalBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(normalBufferSize);

    ComPtr<ID3D12Resource> uploadNormal;
    hr = mDevice->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE,
        &normalBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(uploadNormal.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    void* normalMapped = nullptr;
    uploadNormal->Map(0, &readRange, &normalMapped);
    memcpy(normalMapped, normalZeroData.data(), normalBufferSize);
    uploadNormal->Unmap(0, nullptr);

    D3D12_RESOURCE_BARRIER barrierToDestN = CD3DX12_RESOURCE_BARRIER::Transition(
        mNormalBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    cmdList->ResourceBarrier(1, &barrierToDestN);

    cmdList->CopyBufferRegion(
        mNormalBuffer.Get(), 0,
        uploadNormal.Get(), 0,
        normalBufferSize
    );

    D3D12_RESOURCE_BARRIER barrierToUavN = CD3DX12_RESOURCE_BARRIER::Transition(
        mNormalBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    cmdList->ResourceBarrier(1, &barrierToUavN);
}

void WaveGrid::UploadWallDataToGPU() {

    //バッファがあるかの確認
    if (!mWallBuffer)return;

    //バッファのサイズ取得
    uint32_t wallBufferSize = mWidth * mHeight * sizeof(uint32_t);
    std::vector<uint32_t>wallData(mWidth * mHeight);
    
    //bool を uint32_t に変換
    for (size_t i = 0; i < mWall.size(); ++i) {
        wallData[i] = mWall[i] ? 1u : 0u;
    }

    //メモリにコピーしてGPUが読み込める状態に戻す
    void* mappedPtr = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    mWallBuffer->Map(0, &readRange, &mappedPtr);
    memcpy(mappedPtr, wallData.data(), wallBufferSize);
    mWallBuffer->Unmap(0, nullptr);
}


//---------------------------------
//private関数
//---------------------------------


void WaveGrid::CreateGPUResources()
{
    if (!mDevice) return;

    CreateConstantBuffer();
    CreateStructuredBuffers();
    CreateStagingBuffers();
    CreateDescriptorViews();
}



void WaveGrid::CreateConstantBuffer() {

    //サイズのアライメント
    uint32_t bufferSize = ((sizeof(WaveParams) + 255) / 256) * 256;
   
    D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    HRESULT hr = mDevice->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(mConstBufferWaveParams.GetAddressOf())
    );

    if (SUCCEEDED(hr)) {
        D3D12_RANGE readRange = { 0,0 };
        mConstBufferWaveParams->Map(0, &readRange, &mParamsBufferMappedPtr);
    }


}

void WaveGrid::CreateStructuredBuffers()
{
    uint32_t elementCount = mWidth * mHeight;
    uint32_t waveBufferSize = elementCount * sizeof(float);

    D3D12_HEAP_PROPERTIES defaultHeap =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC waveDesc =
        CD3DX12_RESOURCE_DESC::Buffer(waveBufferSize);
    waveDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    HRESULT hr = S_OK;

    hr = mDevice->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &waveDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(mCurrentBuffer.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    hr = mDevice->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &waveDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(mPreviousBuffer.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    hr = mDevice->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &waveDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(mNextBuffer.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    uint32_t normalBufferSize = elementCount * sizeof(float) * 4;
    D3D12_RESOURCE_DESC normalDesc = CD3DX12_RESOURCE_DESC::Buffer(normalBufferSize);
    normalDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = mDevice->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &normalDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
        IID_PPV_ARGS(mNormalBuffer.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    uint32_t wallBufferSize = elementCount * sizeof(uint32_t);

    D3D12_HEAP_PROPERTIES uploadHeap =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    D3D12_RESOURCE_DESC wallDesc =
        CD3DX12_RESOURCE_DESC::Buffer(wallBufferSize);

    hr = mDevice->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &wallDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(mWallBuffer.GetAddressOf())
    );
    assert(SUCCEEDED(hr));
}



void WaveGrid::CreateStagingBuffers()
{
    // CPU が読み込むための Staging Buffer

    uint32_t bufferSize = mWidth * mHeight * sizeof(float);


    D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    mDevice->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(mHeightStaging.GetAddressOf())
    );

    // Normal Staging も同様に作成（後で）

    uint32_t normalBufferSize = mWidth * mHeight * sizeof(float) * 4;
    D3D12_RESOURCE_DESC resourceDescNormal = CD3DX12_RESOURCE_DESC::Buffer(normalBufferSize);

    mDevice->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resourceDescNormal,  
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(mNormalStaging.GetAddressOf())
    );

}

void WaveGrid::CreateDescriptorViews()
{
    ID3D12DescriptorHeap* heap = mEngine->GetSrvDescriptorHeap();

    UINT descriptorSize =
        mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = heap->GetGPUDescriptorHandleForHeapStart();

    // GpuDrivenRenderer が 101-103 を使ってるので、被らない番号にする
    UINT wallSrvIndex = 110;
    UINT waveUavIndex = 111; // 111,112,113 を u0,u1,u2 に使う

    mWallSrvCpuHandle.ptr = cpuStart.ptr + descriptorSize * wallSrvIndex;
    mWallSrvGpuHandle.ptr = gpuStart.ptr + descriptorSize * wallSrvIndex;

    mWaveUavCpuHandle.ptr = cpuStart.ptr + descriptorSize * waveUavIndex;
    mWaveUavGpuHandle.ptr = gpuStart.ptr + descriptorSize * waveUavIndex;

    // t0 : gWall
    D3D12_SHADER_RESOURCE_VIEW_DESC wallSrvDesc{};
    wallSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    wallSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    wallSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    wallSrvDesc.Buffer.FirstElement = 0;
    wallSrvDesc.Buffer.NumElements = mWidth * mHeight;
    wallSrvDesc.Buffer.StructureByteStride = sizeof(uint32_t);
    wallSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    mDevice->CreateShaderResourceView(
        mWallBuffer.Get(),
        &wallSrvDesc,
        mWallSrvCpuHandle
    );

    CreateWaveUavDescriptors();
}


void WaveGrid::CreateWaveUavDescriptors()
{
    UINT descriptorSize =
        mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = mWidth * mHeight;
    uavDesc.Buffer.StructureByteStride = sizeof(float);
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = mWaveUavCpuHandle;

    // u0 : gCurr
    mDevice->CreateUnorderedAccessView(
        mCurrentBuffer.Get(),
        nullptr,
        &uavDesc,
        handle
    );

    // u1 : gPrev
    handle.ptr += descriptorSize;
    mDevice->CreateUnorderedAccessView(
        mPreviousBuffer.Get(),
        nullptr,
        &uavDesc,
        handle
    );

    // u2 : gNext
    handle.ptr += descriptorSize;
    mDevice->CreateUnorderedAccessView(
        mNextBuffer.Get(),
        nullptr,
        &uavDesc,
        handle
    );

    // u3 : Normal法線
   // u3 : Normal法線
    handle.ptr += descriptorSize;
    D3D12_UNORDERED_ACCESS_VIEW_DESC normalUavDesc = uavDesc;
    normalUavDesc.Buffer.StructureByteStride = sizeof(float) * 4;  // float4
    mDevice->CreateUnorderedAccessView(
        mNormalBuffer.Get(),
        nullptr,
        &normalUavDesc,
        handle
    );
}