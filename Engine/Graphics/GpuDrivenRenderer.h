#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "GpuDrivenTypes.h"

using Microsoft::WRL::ComPtr;

// 前方宣言
Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(
	ID3D12Device* device,
	size_t size,
	D3D12_HEAP_TYPE heapType,
	D3D12_RESOURCE_FLAGS flags);

// ExecuteIndirect用のコマンド構造体（メモリの並び順が命！）
struct IndirectCommand {
	UINT baseInstance; // ① RootConstantで渡していた「開始インデックス」
	D3D12_DRAW_INDEXED_ARGUMENTS drawArgs; // ② Draw用の引数5点セット（IndexCount, InstanceCountなど）
};

class GpuDrivenRenderer
{
public:
	void Initialize(
		ID3D12Device* device,
		ID3D12DescriptorHeap* srvDescriptorHeap,
		int maxDrawCount);

	bool IsInitialized() const { return m_device != nullptr; }
	int GetMaxDrawCount() const { return m_maxDrawCount; }
	void SetMaxDrawCount(int count) { m_maxDrawCount = count; }

	// ============================================================
	// 【CommandSignature の初期化】
	// CommandSignature の作成と m_commandSignature への格納を行う
	// ============================================================
	void CreateCommandSignature(
		ID3D12Device* device,
		ID3D12RootSignature* rootSignature
	);

	// ============================================================
	// 【バッファアクセス用 Getter】
	// ============================================================
	RawTransform* GetMappedTransformData() const { return m_mappedTransformData; }
	ID3D12Resource* GetInstanceBuffer() const { return m_instanceBuffer.Get(); }
	ID3D12Resource* GetTransformBuffer() const { return m_transformBuffer.Get(); }

	// ============================================================
	// 【ディスクリプタハンドル取得】
	// ============================================================
	D3D12_GPU_DESCRIPTOR_HANDLE GetTransformSrvGpuHandle() const { return m_transformSrvGpuHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetInstanceUavGpuHandle() const { return m_instanceUavGpuHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetInstanceSrvGpuHandle() const { return m_instanceSrvGpuHandle; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetTransformSrvCpuHandle() const { return m_transformSrvCpuHandle; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetInstanceUavCpuHandle() const { return m_instanceUavCpuHandle; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetInstanceSrvCpuHandle() const { return m_instanceSrvCpuHandle; }

	// ============================================================
	// 【リソース状態管理】
	// ============================================================
	D3D12_RESOURCE_STATES GetInstanceBufferState() const { return m_instanceBufferState; }
	void SetInstanceBufferState(D3D12_RESOURCE_STATES state) { m_instanceBufferState = state; }

	// ============================================================
	// 【バッファ拡張】
	// ============================================================
	void GrowBuffers(
		ID3D12CommandQueue* commandQueue,
		ID3D12Fence* fence,
		HANDLE fenceEvent,
		uint64_t& fenceValue,
		int requiredCount);

	// ============================================================
	// 【バリア処理メソッド】
	// ============================================================
	void TransitionToUAV(ID3D12GraphicsCommandList* cmdList);
	void TransitionToSRV(ID3D12GraphicsCommandList* cmdList);

	// ============================================================
	// 【IndirectCommand バッファ関係】
	// ============================================================
	IndirectCommand* GetMappedIndirectCommandData() const { return m_mappedIndirectCommandData; }
	ID3D12CommandSignature* GetCommandSignature() const { return m_commandSignature.Get(); }
	ID3D12Resource* GetIndirectCommandBuffer() const { return m_indirectCommandBuffer.Get(); }
	D3D12_RESOURCE_STATES GetIndirectCommandBufferState() const { return m_indirectCommandBufferState; }
	void SetIndirectCommandBufferState(D3D12_RESOURCE_STATES state) { m_indirectCommandBufferState = state; }

	// ============================================================
	// 【IndirectCommandBuffer バリア処理】
	// ============================================================
	void TransitionIndirectCommandBufferToGenericRead(ID3D12GraphicsCommandList* cmdList);

private:
	ComPtr<ID3D12Resource> m_indirectCommandBuffer;
	IndirectCommand* m_mappedIndirectCommandData = nullptr;
	D3D12_RESOURCE_STATES m_indirectCommandBufferState = D3D12_RESOURCE_STATE_COMMON;
	ComPtr<ID3D12CommandSignature> m_commandSignature;


	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
	ID3D12DescriptorHeap* m_srvDescriptorHeap = nullptr;
	int m_maxDrawCount = 0;

	// ============================================================
	// 【バッファリソース】
	// ============================================================
	ComPtr<ID3D12Resource> m_transformBuffer;      // 入力用（UPLOAD ヒープ）
	ComPtr<ID3D12Resource> m_instanceBuffer;       // 出力用（DEFAULT ヒープ + UAV）

	// ============================================================
	// 【マップされたポインタ】
	// ============================================================
	RawTransform* m_mappedTransformData = nullptr;  // CPU書き込み用

	// ============================================================
	// 【リソース状態管理】
	// ============================================================
	D3D12_RESOURCE_STATES m_instanceBufferState = D3D12_RESOURCE_STATE_COMMON;

	// ============================================================
	// 【ディスクリプタハンドル】6つ
	// ============================================================
	D3D12_CPU_DESCRIPTOR_HANDLE m_transformSrvCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_transformSrvGpuHandle;

	D3D12_CPU_DESCRIPTOR_HANDLE m_instanceUavCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_instanceUavGpuHandle;

	D3D12_CPU_DESCRIPTOR_HANDLE m_instanceSrvCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_instanceSrvGpuHandle;

	// ============================================================
	// 【初期化処理用ヘルパー】
	// ============================================================
	void CreateBuffersAndViews(int newMaxDrawCount);
	void CreateDescriptorViews();
};