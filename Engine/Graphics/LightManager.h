#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "allVector.h"

using Microsoft::WRL::ComPtr;

struct LightData {
    Vector3 dirOrPos;
    float type;
    Vector4 color;
    float intensity;
};

class LightManager {
public:
    static LightManager* GetInstance();

    static const int MAX_LIGHTS = 10;
    static const int LIGHT_SRV_SLOT = 108;

    void Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap);

    void SetLight(int index, const LightData& light);
    const LightData& GetLight(int index) const { return m_lights[index]; }
    int GetActiveLightCount() const { return m_activeLightCount; }

    void Bind(ID3D12GraphicsCommandList* cmdList, int id);

    void SetSelectedLight(int index) { m_selectedLightIndex = index; }
    int GetSelectedLight() const { return m_selectedLightIndex; }

    int AddLight();
    void RemoveLight(int index);
    bool IsLightActive(int index){return m_lightActive[index];}


private:
    LightManager() = default;
    void Upload();

    static LightManager* s_instance;

    ID3D12Device* m_device = nullptr;

    ComPtr<ID3D12Resource> m_lightBuffer;
    D3D12_GPU_DESCRIPTOR_HANDLE m_lightSrvGpuHandle{};

    LightData m_lights[MAX_LIGHTS] = {};
    bool m_lightActive[MAX_LIGHTS] = { true };
    int m_activeLightCount = 0;   // ← m_lightDataの代わりにこれを追加
    int m_selectedLightIndex = -1;
};