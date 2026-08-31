#include "GpuDrivenRenderer.h"
#include "PSO.h"

void GpuDrivenRenderer::Initialize(
	ID3D12Device* device,
	ID3D12DescriptorHeap* srvDescriptorHeap,
	int maxDrawCount)
{
	m_device = device;
	m_srvDescriptorHeap = srvDescriptorHeap;
	m_maxDrawCount = maxDrawCount;

	CreateBuffersAndViews(maxDrawCount);
	CreateDescriptorViews();
}

void GpuDrivenRenderer::GrowBuffers(
	ID3D12CommandQueue* commandQueue,
	ID3D12Fence* fence,
	HANDLE fenceEvent,
	uint64_t& fenceValue,
	int requiredCount)
{
	while (m_maxDrawCount < requiredCount) {
		m_maxDrawCount *= 2;
	}

	fenceValue++;
	commandQueue->Signal(fence, fenceValue);
	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	if (m_transformBuffer && m_mappedTransformData) {
		m_transformBuffer->Unmap(0, nullptr);
		m_mappedTransformData = nullptr;
	}

	if (m_indirectCommandBuffer && m_mappedIndirectCommandData) {
		m_indirectCommandBuffer->Unmap(0, nullptr);
		m_mappedIndirectCommandData = nullptr;
	}

	CreateBuffersAndViews(m_maxDrawCount);
	CreateDescriptorViews();
}

void GpuDrivenRenderer::CreateBuffersAndViews(int newMaxDrawCount)
{
	m_transformBuffer = CreateBufferResource(
		m_device.Get(),
		sizeof(RawTransform) * newMaxDrawCount,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_FLAG_NONE
	);
	m_transformBuffer->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&m_mappedTransformData)
	);


	m_indirectCommandBuffer = CreateBufferResource(
		m_device.Get(),
		sizeof(IndirectCommand) * newMaxDrawCount,
		D3D12_HEAP_TYPE_UPLOAD, // CPUからコマンドを書き込むのでUPLOAD
		D3D12_RESOURCE_FLAG_NONE
	);
	m_indirectCommandBufferState = D3D12_RESOURCE_STATE_GENERIC_READ;

	// マッピングしておく
	m_indirectCommandBuffer->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&m_mappedIndirectCommandData)
	);


	m_instanceBuffer = CreateBufferResource(
		m_device.Get(),
		sizeof(InstanceData) * newMaxDrawCount,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	);
	m_instanceBufferState = D3D12_RESOURCE_STATE_COMMON;
}

void GpuDrivenRenderer::CreateDescriptorViews()
{
	// ハンドル計算
	UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE heapStartCPU = m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE heapStartGPU = m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	// インデックス（100番はm_sceneSrvが使用中のため、101番から連番）
	UINT srvTransformIndex = 101;
	UINT uavInstanceIndex = 102;
	UINT srvInstanceIndex = 103;

	// ハンドルのポインタを設定
	m_transformSrvCpuHandle.ptr = heapStartCPU.ptr + descriptorSize * srvTransformIndex;
	m_transformSrvGpuHandle.ptr = heapStartGPU.ptr + descriptorSize * srvTransformIndex;

	m_instanceUavCpuHandle.ptr = heapStartCPU.ptr + descriptorSize * uavInstanceIndex;
	m_instanceUavGpuHandle.ptr = heapStartGPU.ptr + descriptorSize * uavInstanceIndex;

	m_instanceSrvCpuHandle.ptr = heapStartCPU.ptr + descriptorSize * srvInstanceIndex;
	m_instanceSrvGpuHandle.ptr = heapStartGPU.ptr + descriptorSize * srvInstanceIndex;

	// ============================================================
	// ① gTransforms用のSRV作成 (Compute Shaderが読む用: register(t0))
	// ============================================================
	D3D12_SHADER_RESOURCE_VIEW_DESC transformSrvDesc{};
	transformSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	transformSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	transformSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	transformSrvDesc.Buffer.FirstElement = 0;
	transformSrvDesc.Buffer.NumElements = m_maxDrawCount;
	transformSrvDesc.Buffer.StructureByteStride = sizeof(RawTransform);
	transformSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	m_device->CreateShaderResourceView(m_transformBuffer.Get(), &transformSrvDesc, m_transformSrvCpuHandle);

	// ============================================================
	// ② gInstances用のUAV作成 (Compute Shaderが書く用: register(u0))
	// ============================================================
	D3D12_UNORDERED_ACCESS_VIEW_DESC instanceUavDesc{};
	instanceUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	instanceUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	instanceUavDesc.Buffer.FirstElement = 0;
	instanceUavDesc.Buffer.NumElements = m_maxDrawCount;
	instanceUavDesc.Buffer.StructureByteStride = sizeof(InstanceData);
	instanceUavDesc.Buffer.CounterOffsetInBytes = 0;
	instanceUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	m_device->CreateUnorderedAccessView(m_instanceBuffer.Get(), nullptr, &instanceUavDesc, m_instanceUavCpuHandle);

	// ============================================================
	// ③ gInstances用のSRV作成 (Vertex Shaderが読む用: register(t2))
	// ============================================================
	D3D12_SHADER_RESOURCE_VIEW_DESC instanceSrvDesc{};
	instanceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instanceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instanceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instanceSrvDesc.Buffer.FirstElement = 0;
	instanceSrvDesc.Buffer.NumElements = m_maxDrawCount;
	instanceSrvDesc.Buffer.StructureByteStride = sizeof(InstanceData);
	instanceSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	m_device->CreateShaderResourceView(m_instanceBuffer.Get(), &instanceSrvDesc, m_instanceSrvCpuHandle);
}

void GpuDrivenRenderer::TransitionToUAV(ID3D12GraphicsCommandList* cmdList)
{
	D3D12_RESOURCE_BARRIER beforeComputeBarrier{};
	beforeComputeBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	beforeComputeBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	beforeComputeBarrier.Transition.pResource = m_instanceBuffer.Get();
	beforeComputeBarrier.Transition.StateBefore = m_instanceBufferState;
	beforeComputeBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	beforeComputeBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	if (m_instanceBufferState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
		cmdList->ResourceBarrier(1, &beforeComputeBarrier);
		m_instanceBufferState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
}

void GpuDrivenRenderer::TransitionToSRV(ID3D12GraphicsCommandList* cmdList)
{
	D3D12_RESOURCE_BARRIER afterComputeBarrier{};
	afterComputeBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	afterComputeBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	afterComputeBarrier.Transition.pResource = m_instanceBuffer.Get();
	afterComputeBarrier.Transition.StateBefore = m_instanceBufferState;
	afterComputeBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	afterComputeBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	cmdList->ResourceBarrier(1, &afterComputeBarrier);
	m_instanceBufferState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

void GpuDrivenRenderer::TransitionIndirectCommandBufferToGenericRead(ID3D12GraphicsCommandList* cmdList)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_indirectCommandBuffer.Get();
	barrier.Transition.StateBefore = m_indirectCommandBufferState;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	if (m_indirectCommandBufferState != D3D12_RESOURCE_STATE_GENERIC_READ) {
		cmdList->ResourceBarrier(1, &barrier);
		m_indirectCommandBufferState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}
}


void GpuDrivenRenderer::CreateCommandSignature(ID3D12Device* device, ID3D12RootSignature* rootSignature) {
	// ① GPUに「構造体の中身をどう解釈するか」を教える配列
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[2] = {};

	// 1つ目：RootConstant（開始インデックスを スロット5 にセットする）
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	argumentDescs[0].Constant.RootParameterIndex = 5; // ★TUFEngineのルートシグネチャのスロット5
	argumentDescs[0].Constant.DestOffsetIn32BitValues = 0;
	argumentDescs[0].Constant.Num32BitValuesToSet = 1;

	// 2つ目：DrawIndexedInstanced（実際の描画命令）
	argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	// ② コマンドシグネチャ全体の設定
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	// 構造体1つ分のサイズ（24バイト）を教える
	commandSignatureDesc.ByteStride = sizeof(IndirectCommand);

	// ③ 生成し、メンバーに格納！
	HRESULT hr = device->CreateCommandSignature(
		&commandSignatureDesc,
		rootSignature, // 定数をバインドするため、使っているルートシグネチャが必要
		IID_PPV_ARGS(m_commandSignature.GetAddressOf())
	);
	assert(SUCCEEDED(hr));
}