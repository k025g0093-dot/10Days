#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <wrl.h>          // ★追加
#include "LogSistem.h"
#include "DXC.h"
#include "ConvertString.h"
#include <format>
#include <string>
#include "Vector4.h"

using Microsoft::WRL::ComPtr; // ★追加

// 戻り値を ComPtr に変更（所有権を明示）
ComPtr<ID3D12Resource> CreateVertexResource(
    ID3D12Device* device,
    size_t sizeInBytes,
    HRESULT& hr);

// VertexBufferView は COM オブジェクトではないためそのまま
D3D12_VERTEX_BUFFER_VIEW CreateVertexBufferView(
    ID3D12Resource* vertexResource,
    size_t sizeInBytes,
    size_t strideInBytes);

ComPtr<ID3D12Resource> CreateBufferResource(
    ID3D12Device* device,
    size_t sizeInBytes,
    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_UPLOAD, // 省略したら今まで通りUPLOAD
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
);