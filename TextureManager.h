#pragma once
#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <cassert>
#include <filesystem>
#include <dbghelp.h>
#include <strsafe.h>
#include <string>
#include <format>
#include <wrl.h>
#include <array>
#include <vector>
#include <map>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"DbgHelp.lib")
#pragma comment(lib, "DirectXTex.lib")

#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "ConvertString.h"
#include "LogSistem.h"
#include "VertexResource.h"

using Microsoft::WRL::ComPtr;   // ★追加

struct MaterialTextureSet {
    int baseTextureIndex;
    int normalTextureIndex;
};

class TextureManager
{
public:
    static TextureManager* GetInstance() {
        static TextureManager instance;
        return &instance;
    }

    static const int MAX_TEXTURES = 256;
    static const int IMGUI_RESERVED = 8;
    int m_textureCount = 0;

    TextureManager() = default;
    ~TextureManager() = default;

    void Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap,
        ID3D12GraphicsCommandList* commandList);

    int LoadTexture(const std::string& filePath);

    ID3D12DescriptorHeap* GetSRVHeap()       const { return m_srvHeap; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(int index);
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t descriptorSize, uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t descriptorSize, uint32_t index);
    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return srvDescriptorHeap; }

    ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;

    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSrvHandleGPU() const { return textureSrvHandleGPU; }

private:
    ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
    void CreateTextureSRV(ID3D12Resource* textureResource, const DirectX::TexMetadata& metadata, int m_textureCount);
    ComPtr<ID3D12Resource> UploadTexture(
        ID3D12Resource* texture,
        const DirectX::ScratchImage& mipImages);

    std::map<std::wstring, int> m_filePathToIndexMap;

    ID3D12Device* m_device = nullptr;
    ID3D12DescriptorHeap* m_srvHeap = nullptr;

    // テクスチャリソース配列を ComPtr に統一
    std::array<ComPtr<ID3D12Resource>, MAX_TEXTURES> m_textures; // ★

    UINT m_descriptorSize = 0;

    // 以下は内部処理用。デバイス等は外部から借りる形なので生ポインタを維持
    IDXGIFactory7* dxgiFactory = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandAllocator* commandAllocator = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;
    HRESULT                     hr = S_OK;

    D3D12_RESOURCE_DESC resourceDesc{};
    D3D12_RESOURCE_DESC depthResourceDesc{};

    // 汎用リソース類を ComPtr に
    ComPtr<ID3D12Resource>      resource;             // ★
    ComPtr<ID3D12Resource>      texture;              // ★
    ComPtr<ID3D12Resource>      intermediateResource; // ★
    std::vector<ComPtr<ID3D12Resource>> m_uploadResources;

    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU{};
    uint32_t                    descriptorSizeSRV{};
};
