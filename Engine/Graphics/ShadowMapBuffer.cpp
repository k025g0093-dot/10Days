#include "ShadowMapBuffer.h"

ShadowMapBuffer* ShadowMapBuffer::s_instance = nullptr;


ShadowMapBuffer* ShadowMapBuffer::GetInstance() {
    if (!s_instance) s_instance = new ShadowMapBuffer();
    return s_instance;
}

bool ShadowMapBuffer::Initialize(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvHeap
) {
    HRESULT hr;

    // DSVヒープ（これは別でもOK）
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_));
    _ASSERT_EXPR(SUCCEEDED(hr), L"CreateDescriptorHeap(DSV) failed");
    if (FAILED(hr)) return false;

    // テクスチャ作成
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Width = 1024;
    resDesc.Height = 1024;
    resDesc.MipLevels = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    resDesc.SampleDesc.Count = 1;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&shadowMapResource_));
    _ASSERT_EXPR(SUCCEEDED(hr), L"CreateCommittedResource failed");
    if (FAILED(hr)) {
        __debugbreak();  // ここで止まるので、Visual Studio のウォッチウインドウに「hr」と入力して値を見る
        return false;
    }

    // DSV作成
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    device->CreateDepthStencilView(shadowMapResource_.Get(), &dsvDesc,
        dsvHeap_->GetCPUDescriptorHandleForHeapStart());

    // SRVは既存の srvHeap のスロット120に作る
    UINT ds = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    _ASSERT_EXPR(ds > 0, L"descriptor size is 0");
    int slot = 120;
    _ASSERT_EXPR(srvHeap != nullptr, L"srvHeap is null");
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += ds * slot;
    srvGpuHandle_ = srvHeap->GetGPUDescriptorHandleForHeapStart();
    srvGpuHandle_.ptr += ds * slot;
    _ASSERT_EXPR(srvGpuHandle_.ptr != 0, L"srvGpuHandle_ is 0 after setup");

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    device->CreateShaderResourceView(shadowMapResource_.Get(), &srvDesc, cpuHandle);

    return true;
}


void ShadowMapBuffer::TransitionToSrv(ID3D12GraphicsCommandList* cmdList) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = shadowMapResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
}

void ShadowMapBuffer::TransitionToDsv(ID3D12GraphicsCommandList* cmdList) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = shadowMapResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
}
