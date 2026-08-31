#include "ImGuiUIManager.h"
#include "TUFEngine.h"
#include <d3d12.h>
#include <dxgi1_4.h>

// --- Reader structs: must match internal imgui_impl_dx12 layout exactly (read-only, no writes beyond FenceValue) ---
struct FCReader {
    UINT64                          FenceValue;
    ID3D12CommandAllocator*         CommandAllocator;
    ID3D12Resource*                 RenderTarget;
    D3D12_CPU_DESCRIPTOR_HANDLE     RenderTargetCpuDescriptors;
};

struct VPDataReader {
    ID3D12CommandQueue*             CommandQueue;
    ID3D12GraphicsCommandList*      CommandList;
    ID3D12DescriptorHeap*           RtvDescHeap;
    IDXGISwapChain3*                SwapChain;
    HANDLE                          SwapChainWaitableObject;
    UINT                            NumFramesInFlight;
    FCReader*                       FrameCtx;
    UINT                            FrameIndex;
};
// ---

ImGuiUIManager* ImGuiUIManager::s_instance = nullptr;

ImGuiUIManager::ImGuiUIManager(HWND hwnd)
{
#ifdef USE_IMGUI
    s_instance = this;

    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGuiStyle& style = ImGui::GetStyle();
#endif
}

ImGuiUIManager::~ImGuiUIManager()
{
    for (auto& [vp, sync] : m_viewportSync)
    {
        if (sync.fence) sync.fence->Release();
        if (sync.event) CloseHandle(sync.event);
    }
}

void ImGuiUIManager::addWindow(std::shared_ptr<ImGuiUIWindow> newWin) {
    windows.push_back(newWin);
}

void ImGuiUIManager::updateWindows(TUFEngine* engine) {
    for (auto& win : windows) {
        win->update(engine);
    }
}

void ImGuiUIManager::update(TUFEngine* engine)
{
#ifdef USE_IMGUI
    if (!m_device) {
        m_device = engine->GetDevice();
        m_srvHeap = engine->GetSrvDescriptorHeap();
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    updateWindows(engine);

    if (onDrawGUI) { onDrawGUI(); }

    ImGui::Render();

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
#endif
}

void ImGuiUIManager::ViewportRenderCallback(ImGuiViewport* vp, void* /*render_arg*/)
{
#ifdef USE_IMGUI



    auto* vd = (VPDataReader*)vp->RendererUserData;
    auto& sync = s_instance->m_viewportSync[vp];

    // Create per-viewport fence on first use
    if (!sync.fence)
    {
        s_instance->m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&sync.fence));
        sync.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    // Wait for previous frame on this viewport
    if (sync.fence->GetCompletedValue() < sync.lastSignaledValue)
    {
        sync.fence->SetEventOnCompletion(sync.lastSignaledValue, sync.event);
        WaitForSingleObject(sync.event, INFINITE);
    }

    UINT frameIdx = vd->FrameIndex % vd->NumFramesInFlight;
    FCReader* fc = &vd->FrameCtx[frameIdx];
    UINT backIdx = vd->SwapChain->GetCurrentBackBufferIndex();

    // Reset command list
    fc->CommandAllocator->Reset();
    vd->CommandList->Reset(fc->CommandAllocator, nullptr);

    // Barrier: PRESENT → RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = vd->FrameCtx[backIdx].RenderTarget;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    vd->CommandList->ResourceBarrier(1, &barrier);

    // Set render target
    vd->CommandList->OMSetRenderTargets(1, &vd->FrameCtx[backIdx].RenderTargetCpuDescriptors, FALSE, nullptr);

    if (!(vp->Flags & ImGuiViewportFlags_NoRendererClear))
    {
        const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        vd->CommandList->ClearRenderTargetView(vd->FrameCtx[backIdx].RenderTargetCpuDescriptors, clearColor, 0, nullptr);
    }

    // Set descriptor heap & render ImGui content
    ID3D12DescriptorHeap* heaps[] = { s_instance->m_srvHeap };
    vd->CommandList->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(vp->DrawData, vd->CommandList);

    // Barrier: RENDER_TARGET → PRESENT
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    vd->CommandList->ResourceBarrier(1, &barrier);
    vd->CommandList->Close();

    // Execute
    ID3D12CommandList* lists[] = { vd->CommandList };
    vd->CommandQueue->ExecuteCommandLists(1, lists);

    // Signal fence
    sync.lastSignaledValue++;
    vd->CommandQueue->Signal(sync.fence, sync.lastSignaledValue);
    fc->FenceValue = sync.lastSignaledValue;
    vd->SwapChain->Present(0, 0);
#endif // USE_IMGUI
}

void ImGuiUIManager::ViewportSwapCallback(ImGuiViewport* vp, void* /*render_arg*/)
{
   // auto* vd = (VPDataReader*)vp->RendererUserData;
}

void ImGuiUIManager::render() {}
