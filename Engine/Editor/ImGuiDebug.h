#pragma once
#include "ImGuiWindow.h"
#pragma comment(lib,"Winmm.lib")
#pragma comment(lib,"Psapi.lib")
class ImGuiDebug : public ImGuiUIWindow {
public:
    ImGuiDebug() {
        show = true;
    }
    void update(TUFEngine* engine) override;

private:
    float m_fps = 0.0f;
    float m_frameTime = 0.0f;
    DWORD m_lastTime = 0;
};