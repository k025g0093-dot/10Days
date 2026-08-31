#pragma once
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class ShadowMapBuffer
{
public:
    static constexpr UINT SHADOW_MAP_SIZE = 1024;
    static ShadowMapBuffer* GetInstance();

    bool Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap);
    void Bind(ID3D12GraphicsCommandList* cmdList, int rootParamIndex);
    // ShadowMapBuffer.h に追加
    void TransitionToSrv(ID3D12GraphicsCommandList* cmdList);
    void TransitionToDsv(ID3D12GraphicsCommandList* cmdList);

    ID3D12Resource* GetTexture() const { return shadowBuffer_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const { return srvGpuHandle_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpuHandle() const { return dsvHeap_->GetCPUDescriptorHandleForHeapStart(); }
private:
    static ShadowMapBuffer* s_instance;

    ComPtr<ID3D12Resource> shadowBuffer_;
    ComPtr<ID3D12DescriptorHeap>  dsvHeap_;          // DSV用ヒープ
    ComPtr<ID3D12Resource>        shadowMapResource_; // テクスチャ本体
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle_{};
    UINT srvHeapSlot_ = 0;
};
