#pragma once

#include "ImGuiWindow.h"
#include <vector>
#include <memory> 
#include <functional>
#include <unordered_map>

struct ID3D12Device;
struct ID3D12DescriptorHeap;
struct ID3D12Fence;
struct ImGuiViewport;

class TUFEngine;

class ImGuiUIManager {
public:
    ImGuiUIManager(HWND hwnd);
    ~ImGuiUIManager();
    void update(TUFEngine* engine);
    void render();

    static void ViewportRenderCallback(ImGuiViewport* vp, void* render_arg);
    static void ViewportSwapCallback(ImGuiViewport* vp, void* render_arg);

    void addWindow(std::shared_ptr<ImGuiUIWindow> newWin);
    std::function<void()> onDrawGUI = nullptr;
    std::function<void(const std::wstring&)> onFileDrop = nullptr;

    static ImGuiUIManager* s_instance;

private:
    std::vector<std::shared_ptr<ImGuiUIWindow>> windows;

    struct ViewportSync {
        ID3D12Fence* fence = nullptr;
        HANDLE event = nullptr;
        UINT64 lastSignaledValue = 0;
    };
    std::unordered_map<ImGuiViewport*, ViewportSync> m_viewportSync;

    ID3D12Device* m_device = nullptr;
    ID3D12DescriptorHeap* m_srvHeap = nullptr;

    void ShowDockSpace(bool* p_open);
    void setstyle();
    void updateWindows(TUFEngine* engine);
    void deleteWindows();
};