#include "VertexResource.h"

ComPtr<ID3D12Resource> CreateVertexResource(
	ID3D12Device* device,
	size_t sizeInBytes,
	HRESULT& hr)
{

	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC vertexResourceDesc{};

	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;

	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> vertexResource;

	hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(vertexResource.GetAddressOf()));

	assert(SUCCEEDED(hr));
	return vertexResource;
}

D3D12_VERTEX_BUFFER_VIEW CreateVertexBufferView(
	ID3D12Resource* vertexResource,
	size_t sizeInBytes,
	size_t strideInBytes)
{

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = static_cast<UINT>(sizeInBytes);
	vertexBufferView.StrideInBytes = static_cast<UINT>(strideInBytes);

	return vertexBufferView;

}

ComPtr<ID3D12Resource> CreateBufferResource(
	ID3D12Device* device,
	size_t sizeInBytes,
	D3D12_HEAP_TYPE heapType,
	D3D12_RESOURCE_FLAGS flags) {

	size_t alignedSize = (sizeInBytes + 255) & ~255;

	// 変数名を「upload...」から使い回せるように「heapProperties」に変更
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = heapType; // ★1. 固定をやめて引数を使う

	D3D12_RESOURCE_DESC bufferResourceDesc{};
	bufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferResourceDesc.Width = alignedSize;
	bufferResourceDesc.Height = 1;
	bufferResourceDesc.DepthOrArraySize = 1;
	bufferResourceDesc.MipLevels = 1;
	bufferResourceDesc.SampleDesc.Count = 1;
	bufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferResourceDesc.Flags = flags; // ★2. 引数のフラグをセットする

	// ★3. ヒープの種類によって初期状態（リソースステート）を自動で切り替える
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
	if (heapType == D3D12_HEAP_TYPE_DEFAULT) {
		initialState = D3D12_RESOURCE_STATE_COMMON; // DEFAULTヒープの安全な初期状態
	}

	ComPtr<ID3D12Resource> bufferResource;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // ★変数名変更を反映
		D3D12_HEAP_FLAG_NONE,
		&bufferResourceDesc,
		initialState, // ★固定をやめて自動切り替えにした変数を使う
		nullptr,
		IID_PPV_ARGS(bufferResource.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return bufferResource;
}