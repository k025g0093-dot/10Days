#include "LightManager.h"
#include "VertexResource.h"
#include "LogSistem.h"

LightManager* LightManager::s_instance = nullptr;

LightManager* LightManager::GetInstance() {
    if (!s_instance) s_instance = new LightManager();
    return s_instance;
}

void LightManager::Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap) {
    m_device = device;

    LightData defaultLight{};
    defaultLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    defaultLight.type = 0;
    defaultLight.dirOrPos = { 0.0f, -1.0f, 0.0f };
    defaultLight.intensity = 1.0f;
    m_lights[0] = defaultLight;
    m_activeLightCount = 1;

    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    UINT bufferSize = sizeof(LightData) * MAX_LIGHTS;
    m_lightBuffer = CreateBufferResource(device, bufferSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = MAX_LIGHTS;
    srvDesc.Buffer.StructureByteStride = sizeof(LightData);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    cpuHandle.ptr = srvHeap->GetCPUDescriptorHandleForHeapStart().ptr + descriptorSize * LIGHT_SRV_SLOT;
    device->CreateShaderResourceView(m_lightBuffer.Get(), &srvDesc, cpuHandle);

    m_lightSrvGpuHandle.ptr = srvHeap->GetGPUDescriptorHandleForHeapStart().ptr + descriptorSize * LIGHT_SRV_SLOT;

    Upload();
}

// LightManager.cpp
void LightManager::SetLight(int index, const LightData& light) {
    if (index < 0 || index >= MAX_LIGHTS) return;
    m_lights[index] = light;
    if (index >= m_activeLightCount) m_activeLightCount = index + 1;
    Upload();
}

void LightManager::Bind(ID3D12GraphicsCommandList* cmdList, int id) {
    cmdList->SetGraphicsRootDescriptorTable(3, m_lightSrvGpuHandle);
}

void LightManager::Upload() {
    if (!m_lightBuffer) return;

    void* p = nullptr;
    HRESULT hr = m_lightBuffer->Map(0, nullptr, &p);
    if (SUCCEEDED(hr) && p) {
        memcpy(p, m_lights, sizeof(LightData) * MAX_LIGHTS);
        m_lightBuffer->Unmap(0, nullptr);
    }
}

int LightManager::AddLight() {
    for (int i = 1; i < MAX_LIGHTS; i++) {
        if (!m_lightActive[i]) {
            m_lightActive[i] = true;
            LightData d{};
            d.type = 1;
            d.color = { 1.0f, 1.0f, 1.0f, 1.0f };
            d.intensity = 1.0f;
            d.dirOrPos = { 0.0f, 2.0f, 0.0f };
            m_lights[i] = d;
            m_activeLightCount = i + 1;
            Upload();
            return i;
        }
    }
    return -1;
}


void LightManager::RemoveLight(int index) {
    if (index <= 0 || index >= MAX_LIGHTS) return;
    m_lightActive[index] = false;
    m_lights[index] = LightData{};
    if (m_selectedLightIndex == index) m_selectedLightIndex = -1;

    // 🌟 一番後ろの有効indexを再計算
    int lastActive = 0;
    for (int i = 1; i < MAX_LIGHTS; i++) {
        if (m_lightActive[i]) lastActive = i;
    }
    m_activeLightCount = lastActive + 1;

    Upload();
}
