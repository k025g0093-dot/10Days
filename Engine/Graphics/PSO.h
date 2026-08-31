#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <wrl.h>                // ★追加
#include "LogSistem.h"
#include "DXC.h"
#include "ConvertString.h"
#include <format>
#include <string>
#include "VertexResource.h"

using Microsoft::WRL::ComPtr;   // ★追加

ComPtr<ID3D12PipelineState> CreatePipelineStateDesc(
    ID3D12Device* device,
    ComPtr<ID3D12RootSignature>& rootSignature,
    HRESULT& hr);

ComPtr<ID3D12RootSignature> CreateRootSignature(
    ID3D12Device* device,
    HRESULT& hr);

ComPtr<ID3D12RootSignature> CreateGpuDrivenRootSignature(
    ID3D12Device* device,
    HRESULT& hr
);

ComPtr<ID3D12PipelineState> CreateGpuDrivenPipelineStateDesc(
    ID3D12Device* device,
    ComPtr<ID3D12RootSignature>& rootSignature,
    HRESULT& hr);

ComPtr<ID3D12RootSignature> CreateComputeRootSignature(
    ID3D12Device* device,
    HRESULT& hr);
ComPtr<ID3D12PipelineState> CreateComputePipelineState(
    ID3D12Device* device,
    ComPtr<ID3D12RootSignature>& rootSignature,
    HRESULT& hr);

//陰の描画とかに必要なPSO
ComPtr<ID3D12PipelineState> CreateShadowPipelineState(
    ID3D12Device* device,
    ComPtr<ID3D12RootSignature>& rootSignature,
    HRESULT& hr
);

ComPtr<ID3D12RootSignature> CreateShadowRootSignature(
    ID3D12Device* device,
    HRESULT& hr
);



//ライン描画に必要な者たち
ComPtr<ID3D12RootSignature> CreateLineRootSignature(
    ID3D12Device* device,
    HRESULT& hr);
ComPtr<ID3D12PipelineState> CreateLinePipelineState(
    ID3D12Device* device,
    ComPtr<ID3D12RootSignature>& rootSignature,
    HRESULT& hr);

D3D12_INPUT_LAYOUT_DESC CreateLayout();
D3D12_BLEND_DESC CreateBlendState();
D3D12_RASTERIZER_DESC CreateRasterizerState();