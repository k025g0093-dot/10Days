#include "ImGuiDebug.h"
#include <Psapi.h>

void ImGuiDebug::update(TUFEngine* engine) {
#ifdef USE_IMGUI
    if (!show) return;

    // FPS計算
    DWORD currentTime = timeGetTime();
    if (m_lastTime != 0) {
        m_frameTime = (currentTime - m_lastTime) / 1000.0f;
        m_fps = 1.0f / m_frameTime;
    }
    m_lastTime = currentTime;

    // メモリ取得
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(),
        (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    float memoryMB = pmc.WorkingSetSize / (1024.0f * 1024.0f);

    if (begin("デバッグ")) {
        ImGui::Text("FPS        : %.2f", m_fps);
        ImGui::Text("フレーム時間: %.3f ms", m_frameTime * 1000.0f);
        ImGui::Separator();
        ImGui::Text("メモリ     : %.1f MB", memoryMB);
        end();
    }
#endif
}
