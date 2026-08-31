#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <wrl.h>     
#include "DXC.h"
#include "LogSistem.h"

using Microsoft::WRL::ComPtr;


ComPtr<ID3D12RootSignature> WaveGridCreateComputeRootSignature(
    ID3D12Device* device,
    HRESULT& hr);


ComPtr<ID3D12PipelineState> WaveGridCreateComputePipelineState(
    ID3D12Device* device,
    ComPtr<ID3D12RootSignature>& rootSignature,
    HRESULT& hr);

