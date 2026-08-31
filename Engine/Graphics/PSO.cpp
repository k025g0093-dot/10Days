#include "PSO.h"

// ルートシグネチャの生成
// シェーダーにどんなリソースを渡すかを定義する
ComPtr<ID3D12RootSignature> CreateRootSignature(
	ID3D12Device* device,
	HRESULT& hr) {

	// ルートシグネチャの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[3] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRange[1].BaseShaderRegister = 1;
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRange[2].BaseShaderRegister = 3;
	descriptorRange[2].NumDescriptors = 1;
	descriptorRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameter[7] = {};
	rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[0].Descriptor.ShaderRegister = 0;

	rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameter[1].Descriptor.ShaderRegister = 1;

	rootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[2].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
	rootParameter[2].DescriptorTable.NumDescriptorRanges = 1;

	rootParameter[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[3].DescriptorTable.pDescriptorRanges = &descriptorRange[2];
	rootParameter[3].DescriptorTable.NumDescriptorRanges = 1;

	rootParameter[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[4].DescriptorTable.pDescriptorRanges = &descriptorRange[1];
	rootParameter[4].DescriptorTable.NumDescriptorRanges = 1;

	rootParameter[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[5].Descriptor.ShaderRegister = 2;

	rootParameter[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameter[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[6].Constants.ShaderRegister = 3; // b3
	rootParameter[6].Constants.RegisterSpace = 0;
	rootParameter[6].Constants.Num32BitValues = 1; // uintひとつだけ送る

	descriptionRootSignature.pParameters = rootParameter;
	descriptionRootSignature.NumParameters = _countof(rootParameter);


	//Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//0~1の範囲を繰り返す
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//最大LOD
	staticSamplers[0].ShaderRegister = 0;//register(s0)に対応
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//ピクセルシェーダーで使用

	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);



	// ルートシグネチャをバイナリに変換
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	// L"Object3d.VS.hlsl" を L"VertexShader.hlsl" に変更
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

	if (FAILED(hr)) {
		Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// ルートシグネチャの生成
	ComPtr<ID3D12RootSignature> rootSignature;
	hr = device->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return rootSignature;
}

// インプットレイアウトの生成
// 頂点データの形式をGPUに教える
D3D12_INPUT_LAYOUT_DESC CreateLayout() {

	// 頂点データの要素定義（今回はPOSITIONのみ）
	static D3D12_INPUT_ELEMENT_DESC inputElementDescs[4] = {};
	inputElementDescs[0].SemanticName = "POSITION"; // シェーダー側のセマンティクス名
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // float4形式
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD"; // シェーダー側のセマンティクス名
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT; // float2形式
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset =
		D3D12_APPEND_ALIGNED_ELEMENT;


	inputElementDescs[3].SemanticName = "TANGENT";
	inputElementDescs[3].SemanticIndex = 0;
	inputElementDescs[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);
	return inputLayoutDesc;
}

// ブレンドステートの生成
// 色の合成方法を定義する（今回は透明度なしのシンプルな設定）
D3D12_BLEND_DESC CreateBlendState() {
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL; // 全チャンネルへの書き込みを有効化
	return blendDesc;
}

// ラスタライザステートの生成
// ポリゴンの描画方法を定義する
D3D12_RASTERIZER_DESC CreateRasterizerState() {
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;  // 裏面を描画しない
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴンを塗りつぶして描画
	//rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	return rasterizerDesc;
}

// パイプラインステートオブジェクト（PSO）の生成
// 描画に必要な全設定をまとめたオブジェクトを作る
ComPtr<ID3D12PipelineState> CreatePipelineStateDesc(
	ID3D12Device* device,
	ComPtr<ID3D12RootSignature>& rootSignature,
	HRESULT& hr)
{
	// DXCコンパイラの初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* includeHandler = nullptr;
	DxcCompilerInclude(hr, dxcUtils, dxcCompiler, includeHandler);

	// シェーダーのコンパイル
	IDxcBlob* vertexShaderBlob = CompileShader(
		L"shaders/Object3d.VS.hlsl", L"vs_6_0",
		dxcUtils, dxcCompiler, includeHandler);

	IDxcBlob* pixelShaderBlob = CompileShader(
		L"shaders/Object3d.PS.hlsl", L"ps_6_0",
		dxcUtils, dxcCompiler, includeHandler);


	// 各設定の生成
	D3D12_INPUT_LAYOUT_DESC inputLayout = CreateLayout();
	D3D12_BLEND_DESC blendDesc = CreateBlendState();
	D3D12_RASTERIZER_DESC rasterizerDesc = CreateRasterizerState();

	// PSOの設定をまとめる
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};

	// ルートシグネチャの設定
	rootSignature = CreateRootSignature(device, hr);
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();

	// インプットレイアウトの設定
	graphicsPipelineStateDesc.InputLayout = inputLayout;


	// シェーダーの設定
	graphicsPipelineStateDesc.VS = {
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize()
	};
	graphicsPipelineStateDesc.PS = {
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize()
	};

	// ブレンド・ラスタライザの設定
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	//ステートの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//depth機能を有効にする
	depthStencilDesc.DepthEnable = true;
	//書き込みをするところ
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はlessEqual。つまり近いと描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	//DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// レンダーターゲットの設定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// プリミティブトポロジーの設定（三角形として描画）
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// サンプリングの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// PSOの生成
	ComPtr<ID3D12PipelineState> graphicsPipelineState;
	hr = device->CreateGraphicsPipelineState(
		&graphicsPipelineStateDesc,
		IID_PPV_ARGS(graphicsPipelineState.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return graphicsPipelineState;
}


//GPU用のルートシグネチャ作成とパイプラインステート

ComPtr<ID3D12RootSignature> CreateGpuDrivenRootSignature(
	ID3D12Device* device,
	HRESULT& hr
) {

	// ルートシグネチャの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// t0: texture, t1: normal texture, t3: lights
	// t2: InstanceData は RootSRV で直接アドレスを渡すので不要
	D3D12_DESCRIPTOR_RANGE descriptorRange[4] = {};
	descriptorRange[0].BaseShaderRegister = 0; // t0 texture
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRange[1].BaseShaderRegister = 1; // t1 normal texture
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRange[2].BaseShaderRegister = 3; // t3 lights
	descriptorRange[2].NumDescriptors = 1;
	descriptorRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRange[3].BaseShaderRegister = 4; // t4 shadowMap
	descriptorRange[3].NumDescriptors = 1;
	descriptorRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameter[10] = {};
	rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[0].Descriptor.ShaderRegister = 0; // b0 material

	rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameter[1].Descriptor.ShaderRegister = 2; // register(t2)
	rootParameter[1].Descriptor.RegisterSpace = 0;

	rootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[2].DescriptorTable.pDescriptorRanges = &descriptorRange[0]; // t0 texture
	rootParameter[2].DescriptorTable.NumDescriptorRanges = 1;

	rootParameter[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[3].DescriptorTable.pDescriptorRanges = &descriptorRange[2]; // t3 lights
	rootParameter[3].DescriptorTable.NumDescriptorRanges = 1;

	rootParameter[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[4].DescriptorTable.pDescriptorRanges = &descriptorRange[1]; // t1 normal texture
	rootParameter[4].DescriptorTable.NumDescriptorRanges = 1;

	rootParameter[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameter[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameter[5].Constants.ShaderRegister = 2; // b2
	rootParameter[5].Constants.RegisterSpace = 0;
	rootParameter[5].Constants.Num32BitValues = 1;

	// [6] CBV b2 - Camera (pixel)
	rootParameter[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[6].Descriptor.ShaderRegister = 2;

	// [7] 32bit constant b3 - ActiveLightCount (pixel)
	rootParameter[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameter[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[7].Constants.ShaderRegister = 3; // b3
	rootParameter[7].Constants.RegisterSpace = 0;
	rootParameter[7].Constants.Num32BitValues = 1; // uintひとつだけ送る

	// [8] DescriptorTable t4 - シャドウマップ
	rootParameter[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter[8].DescriptorTable.pDescriptorRanges = &descriptorRange[3];
	rootParameter[8].DescriptorTable.NumDescriptorRanges = 1;

	// [9] CBV b4 - ライトVP (ALL visibility)
	rootParameter[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameter[9].Descriptor.ShaderRegister = 4;



	descriptionRootSignature.pParameters = rootParameter;
	descriptionRootSignature.NumParameters = _countof(rootParameter);


	//Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//0~1の範囲を繰り返す
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//最大LOD
	staticSamplers[0].ShaderRegister = 0;//register(s0)に対応
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//ピクセルシェーダーで使用
	
	staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//0~1の範囲を繰り返す
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;//最大LOD
	staticSamplers[1].ShaderRegister = 1;//register(s0)に対応
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//ピクセルシェーダーで使用

	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);



	// ルートシグネチャをバイナリに変換
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	// L"Object3d.VS.hlsl" を L"VertexShader.hlsl" に変更
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

	if (FAILED(hr)) {
		Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// ルートシグネチャの生成
	ComPtr<ID3D12RootSignature> rootSignature;
	hr = device->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return rootSignature;

}


ComPtr<ID3D12PipelineState> CreateGpuDrivenPipelineStateDesc(
	ID3D12Device* device,
	ComPtr<ID3D12RootSignature>& rootSignature,
	HRESULT& hr) {

	// DXCコンパイラの初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* includeHandler = nullptr;
	DxcCompilerInclude(hr, dxcUtils, dxcCompiler, includeHandler);

	// シェーダーのコンパイル
	IDxcBlob* vertexShaderBlob = CompileShader(
		L"shaders/GpuDrivenObject.VS.hlsl", L"vs_6_0",
		dxcUtils, dxcCompiler, includeHandler);

	IDxcBlob* pixelShaderBlob = CompileShader(
		L"shaders/GpuDrivenObject.PS.hlsl", L"ps_6_0",
		dxcUtils, dxcCompiler, includeHandler);

	// 各設定の生成
	D3D12_INPUT_LAYOUT_DESC inputLayout = CreateLayout();
	D3D12_BLEND_DESC blendDesc = CreateBlendState();
	D3D12_RASTERIZER_DESC rasterizerDesc = CreateRasterizerState();

	// ★ 深度ステート設定（統一）
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;  // ★ 念のため明示的に

	// PSOの設定をまとめる
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};

	// ルートシグネチャの設定
	rootSignature = CreateGpuDrivenRootSignature(device, hr);
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();

	// インプットレイアウトの設定
	graphicsPipelineStateDesc.InputLayout = inputLayout;

	// シェーダーの設定
	graphicsPipelineStateDesc.VS = {
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize()
	};
	graphicsPipelineStateDesc.PS = {
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize()
	};

	// ブレンド・ラスタライザの設定
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	// ★ 深度ステート設定（ここに統一）
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// レンダーターゲットの設定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// プリミティブトポロジーの設定
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// サンプリングの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// PSOの生成
	ComPtr<ID3D12PipelineState> graphicsPipelineState;
	hr = device->CreateGraphicsPipelineState(
		&graphicsPipelineStateDesc,
		IID_PPV_ARGS(graphicsPipelineState.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return graphicsPipelineState;
}


// --- PSO.cpp の一番下に追加 ---

// --- PSO.cpp の CreateComputeRootSignature を修正版に差し替え ---
ComPtr<ID3D12RootSignature> CreateComputeRootSignature(
	ID3D12Device* device,
	HRESULT& hr
) {
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	// [0]: register(b0) 用の32bit定数設定
	// 行列(16個のfloat) + 個数(1個のint) = 計17個の32bit値を直接送れるようにします
	D3D12_ROOT_PARAMETER rootParameter[3] = {};
	rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameter[0].Constants.ShaderRegister = 0; // register(b0)
	rootParameter[0].Constants.Num32BitValues = 17; // 17個の値を送る

	// [1]: register(t0) 用 (gTransforms用のSRV)
	D3D12_DESCRIPTOR_RANGE descriptorRangeSRV{};
	descriptorRangeSRV.BaseShaderRegister = 0; // register(t0)
	descriptorRangeSRV.NumDescriptors = 1;
	descriptorRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameter[1].DescriptorTable.pDescriptorRanges = &descriptorRangeSRV;
	rootParameter[1].DescriptorTable.NumDescriptorRanges = 1;

	// [2]: register(u0) 用 (gInstances用のUAV)
	D3D12_DESCRIPTOR_RANGE descriptorRangeUAV{};
	descriptorRangeUAV.BaseShaderRegister = 0; // register(u0)
	descriptorRangeUAV.NumDescriptors = 1;
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

// ② Compute Shader用のパイプラインステート（PSO）作成
ComPtr<ID3D12PipelineState> CreateComputePipelineState(
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
		L"shaders/GpuDrivenObject.CS.hlsl", L"cs_6_0",
		dxcUtils, dxcCompiler, includeHandler);

	// Compute専用のPSO設定（グラフィックス用より設定項目が少なくてシンプル！）
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc{};

	rootSignature = CreateComputeRootSignature(device, hr);
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

// --- Line rendering root signature ---
ComPtr<ID3D12RootSignature> CreateLineRootSignature(
	ID3D12Device* device,
	HRESULT& hr) {

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER params[1] = {};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[0].Descriptor.ShaderRegister = 0;

	desc.pParameters = params;
	desc.NumParameters = 1;

	ID3DBlob* sigBlob = nullptr;
	ID3DBlob* errBlob = nullptr;
	hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) {
		Log(logStream, reinterpret_cast<char*>(errBlob->GetBufferPointer()));
		assert(false);
	}

	ComPtr<ID3D12RootSignature> rootSig;
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(rootSig.GetAddressOf()));
	assert(SUCCEEDED(hr));
	return rootSig;
}

// --- Line PSO ---
ComPtr<ID3D12PipelineState> CreateLinePipelineState(
	ID3D12Device* device,
	ComPtr<ID3D12RootSignature>& rootSignature,
	HRESULT& hr) {

	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* includeHandler = nullptr;
	DxcCompilerInclude(hr, dxcUtils, dxcCompiler, includeHandler);

	IDxcBlob* vertexBlob = CompileShader(L"shaders/Line.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	IDxcBlob* pixelBlob = CompileShader(L"shaders/Line.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);

	rootSignature = CreateLineRootSignature(device, hr);

	D3D12_INPUT_ELEMENT_DESC inputElements[2] = {};
	inputElements[0].SemanticName = "POSITION";
	inputElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElements[1].SemanticName = "COLOR";
	inputElements[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayout{};
	inputLayout.pInputElementDescs = inputElements;
	inputLayout.NumElements = 2;

	D3D12_BLEND_DESC blend{};
	blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC raster{};
	raster.CullMode = D3D12_CULL_MODE_NONE;
	raster.FillMode = D3D12_FILL_MODE_SOLID;
	raster.FrontCounterClockwise = FALSE;

	D3D12_DEPTH_STENCIL_DESC depth{};
	depth.DepthEnable = TRUE;
	depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.InputLayout = inputLayout;
	psoDesc.VS = { vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize() };
	psoDesc.PS = { pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize() };
	psoDesc.BlendState = blend;
	psoDesc.RasterizerState = raster;
	psoDesc.DepthStencilState = depth;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pso.GetAddressOf()));
	assert(SUCCEEDED(hr));
	return pso;
}


//陰描画用のPSO設定
// 陰描画用のPSO設定
ComPtr<ID3D12PipelineState> CreateShadowPipelineState(
	ID3D12Device* device,
	ComPtr<ID3D12RootSignature>& rootSignature,
	HRESULT& hr
) {
	// DXCコンパイラの初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* includeHandler = nullptr;
	DxcCompilerInclude(hr, dxcUtils, dxcCompiler, includeHandler);

	// 頂点シェーダーのみコンパイル（深度だけでいいのでピクセルシェーダーは不要）
	IDxcBlob* vertexShaderBlob = CompileShader(
		L"shaders/shadow.VS.hlsl", L"vs_6_0",
		dxcUtils, dxcCompiler, includeHandler);

	// 各設定の生成（既存のヘルパーを流用）
	D3D12_INPUT_LAYOUT_DESC inputLayout = CreateLayout();
	D3D12_BLEND_DESC blendDesc = CreateBlendState();
	D3D12_RASTERIZER_DESC rasterizerDesc = CreateRasterizerState();

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	// PSOの設定をまとめる（グラフィックス用）
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};

	//rootSignature = CreateShadowRootSignature(device, hr); // 🌟 シャドウ専用のルートシグネチャ
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();

	graphicsPipelineStateDesc.InputLayout = inputLayout;

	graphicsPipelineStateDesc.VS = {
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize()
	};
	// ピクセルシェーダーは設定しない（深度専用、色出力なし）
	graphicsPipelineStateDesc.PS = {}; // 空のまま

	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // ShadowMapBufferと合わせる

	graphicsPipelineStateDesc.NumRenderTargets = 0; // 🌟 カラー出力は無し
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> graphicsPipelineState;
	hr = device->CreateGraphicsPipelineState(
		&graphicsPipelineStateDesc,
		IID_PPV_ARGS(graphicsPipelineState.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return graphicsPipelineState;
}

ComPtr<ID3D12RootSignature> CreateShadowRootSignature(ID3D12Device* device, HRESULT& hr) {
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER params[2] = {};
	// [0] instance buffer (root SRV, vertex) — t2
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[0].Descriptor.ShaderRegister = 2;

	// [1] LightVP (CBV, vertex) — b4
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[1].Descriptor.ShaderRegister = 4;

	desc.pParameters = params;
	desc.NumParameters = _countof(params);

	ID3DBlob* sigBlob = nullptr;
	ID3DBlob* errBlob = nullptr;
	hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) { assert(false); }

	ComPtr<ID3D12RootSignature> rootSig;
	hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(rootSig.GetAddressOf()));
	return rootSig;
}


