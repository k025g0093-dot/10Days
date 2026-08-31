#include "TextureManager.h"

void TextureManager::Initialize(
	ID3D12Device* device,
	ID3D12DescriptorHeap* srvHeap,
	ID3D12GraphicsCommandList* commandList)
{
	m_commandList = commandList;
	m_device = device;
	m_srvHeap = srvHeap;
	if (device) {
		m_descriptorSize = device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}
	m_textureCount = 0;

}



int TextureManager::LoadTexture(const std::string& filePath) {
	std::wstring filePathW = ConvertString(filePath);

	std::wstring ext = std::filesystem::path(filePathW).extension().wstring();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

	DirectX::ScratchImage image{};
	if (ext == L".dds") {
		// DDSだけがキューブマップを表現できる
		hr = DirectX::LoadFromDDSFile(
			filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
	}
	else {
		hr = DirectX::LoadFromWICFile(
			filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	}
	if (FAILED(hr)) return -1;

	const DirectX::TexMetadata& srcMeta = image.GetMetadata();

	// ミップ未生成 かつ 1x1でない かつ BC圧縮でない ときだけ生成する
	const bool needMips =
		srcMeta.mipLevels <= 1 &&
		srcMeta.width > 1 && srcMeta.height > 1 &&
		!DirectX::IsCompressed(srcMeta.format);

	DirectX::ScratchImage mipImages{};
	if (needMips) {
		hr = DirectX::GenerateMipMaps(
			image.GetImages(), image.GetImageCount(), srcMeta,
			DirectX::TEX_FILTER_SRGB, 0, mipImages);
		assert(SUCCEEDED(hr));
	}
	else {
		mipImages = std::move(image);   // ミップ済み / BC圧縮 / 1x1 はそのまま流す
	}

	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	// ↓ ここから下は今のまま、変更なし
	ComPtr<ID3D12Resource> texResource = CreateTextureResource(metadata);
	m_uploadResources.push_back(UploadTexture(texResource.Get(), mipImages));
	CreateTextureSRV(texResource.Get(), metadata, m_textureCount);

	int index = m_textureCount;
	m_textures[index] = texResource;
	m_textureCount++;
	return index;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGPUHandle(int index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = {};
	if (m_srvHeap && index >= 0 && index < m_textureCount) {
		handle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
		// ★ImGui分オフセットしてGPUハンドルを返す
		handle.ptr += static_cast<UINT64>(IMGUI_RESERVED + index) * m_descriptorSize;
	}
	return handle;
}


ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(
	const DirectX::TexMetadata& metadata) {
	resourceDesc = {};
	resourceDesc.Width = static_cast<UINT>(metadata.width);
	resourceDesc.Height = static_cast<UINT>(metadata.height);
	resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);

	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	resource.Reset();
	hr = m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,//データ転送設定
		nullptr,
		IID_PPV_ARGS(resource.GetAddressOf())//作成するリソースポインタへ
	);
	assert(SUCCEEDED(hr));
	return resource;
}


[[nodiscard]]//属性定義：この関数の戻り値を無視しないでね、という意味
ComPtr<ID3D12Resource> TextureManager::UploadTexture(
	ID3D12Resource* texture,
	const DirectX::ScratchImage& mipImages)
{
	//中間バッファの作成と転送の準備
	std::vector<D3D12_SUBRESOURCE_DATA>subresources;

	DirectX::PrepareUpload(
		m_device,
		mipImages.GetImages(),
		mipImages.GetImageCount(),
		mipImages.GetMetadata(),
		subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
	ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(m_device, intermediateSize);

	//データ転送コマンドの作成と積み込み
	UpdateSubresources(
		m_commandList, texture,
		intermediateResource.Get(), 0, 0,
		UINT(subresources.size()), subresources.data()
	);

	//Tetrueへの転送が終わったら、テクスチャの使用目的をコピー先からシェーダーリソースへ変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	m_commandList->ResourceBarrier(1, &barrier);
	return intermediateResource;

}


void TextureManager::CreateTextureSRV(
	ID3D12Resource* textureResource,
	const DirectX::TexMetadata& metadata,
	int index)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (metadata.IsCubemap()) {
		// 6面を1本のキューブSRVとして見る
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = static_cast<UINT>(metadata.mipLevels);
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}

	// ↓ ここから下は今のまま、変更なし
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU =
		GetCPUDescriptorHandle(m_descriptorSize, IMGUI_RESERVED + index);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU =
		GetGPUDescriptorHandle(m_descriptorSize, IMGUI_RESERVED + index);

	this->textureSrvHandleGPU = textureSrvHandleGPU;
	m_device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetCPUDescriptorHandle(
	uint32_t descriptorSize,
	uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += descriptorSize * index;
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGPUDescriptorHandle(
	uint32_t descriptorSize,
	uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += descriptorSize * index;
	return handleGPU;
}



#pragma endregion
