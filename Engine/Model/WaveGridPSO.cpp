#include "WaveGridPSO.h"


ComPtr<ID3D12RootSignature> WaveGridCreateComputeRootSignature(
	ID3D12Device* device,
	HRESULT& hr
) {
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	// [0]: register(b0) 用の32bit定数設定
	// 行列(16個のfloat) + 個数(1個のint) = 計17個の32bit値を直接送れるようにします
	D3D12_ROOT_PARAMETER rootParameter[3] = {};
	rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameter[0].Descriptor.ShaderRegister = 0;


	// [1]: register(t0) 用 (gTransforms用のSRV)
	D3D12_DESCRIPTOR_RANGE descriptorRangeSRV{};
	descriptorRangeSRV.BaseShaderRegister = 0; // register(t0)
	descriptorRangeSRV.NumDescriptors = 3;
	descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameter[1].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
	rootParameter[1].DescriptorTable.NumDescriptorRanges = 1;

	// [2]: register(u0) 用 (gInstances用のUAV)
	D3D12_DESCRIPTOR_RANGE descriptorRangeUAV{};
	descriptorRangeUAV.BaseShaderRegister = 0; // register(u0)
	descriptorRangeUAV.NumDescriptors = 4;
	descriptorRangeUAV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRangeUAV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameter[2].DescriptorTable.pDescriptorRanges = &descriptorRangeUAV;
	rootParameter[2].DescriptorTable.NumDescriptorRanges = 1;

	descriptionRootSignature.pParameters = rootParameter;
	descriptionRootSignature.NumParameters = _countof(rootParameter);

	// シリアライズと生成（ここはさっきと同じです）
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

	if (FAILED(hr)) {
		Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	ComPtr<ID3D12RootSignature> rootSignature;
	hr = device->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return rootSignature;
}


ComPtr<ID3D12PipelineState> WaveGridCreateComputePipelineState(
	ID3D12Device* device,
	ComPtr<ID3D12RootSignature>& rootSignature,
	HRESULT& hr
) {
	// DXCコンパイラの初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* includeHandler = nullptr;
	DxcCompilerInclude(hr, dxcUtils, dxcCompiler, includeHandler);

	// ★Compute Shader（GpuDrivenObject.CS.hlsl）をコンパイル
	// ターゲットは "cs_6_0" になります
	IDxcBlob* computeShaderBlob = CompileShader(
		L"shaders/WaveGrid.CS.hlsl", L"cs_6_0",
		dxcUtils, dxcCompiler, includeHandler);

	// Compute専用のPSO設定（グラフィックス用より設定項目が少なくてシンプル！）
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc{};

	rootSignature = WaveGridCreateComputeRootSignature(device, hr);
	computePsoDesc.pRootSignature = rootSignature.Get();
	computePsoDesc.CS = {
		computeShaderBlob->GetBufferPointer(),
		computeShaderBlob->GetBufferSize()
	};
	computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ComPtr<ID3D12PipelineState> computePipelineState;
	hr = device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(computePipelineState.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return computePipelineState;
}
