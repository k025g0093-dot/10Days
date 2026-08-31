#include "ImGuiCamera.h"
#include "DebugCamer.h"
#include "TUFEngine.h"

void ImGuiCamera::update(TUFEngine* engine) {
#ifdef USE_IMGUI
    if (!show || !m_transform) return;

    if (begin("カメラ")) {
        // デバッグカメラモードの切り替え（'1'キーと同じ）
        bool isDebug = DebugCamer::GetInstance().IsDebug();
        if (ImGui::Checkbox("デバッグカメラ", &isDebug)) {
            DebugCamer::GetInstance().SetDebug(isDebug);
        }
        ImGui::Separator();

        ImGui::DragFloat3("カメラ位置", &m_transform->translate.x, 0.1f);
        ImGui::DragFloat3("カメラ回転", &m_transform->rotate.x, 0.01f);

        Camera* camera = new Camera();
        if (camera) {
            float nearPlane = camera->GetNearPlane();
            float farPlane = camera->GetFarPlane();

            ImGui::Separator();
            ImGui::Text("プロジェクション設定");

            if (ImGui::SliderFloat("ニアプレーン", &nearPlane, 0.01f, 10.0f, "%.3f")) {
                camera->SetNearPlane(nearPlane);
            }
            if (ImGui::SliderFloat("ファープレーン", &farPlane, 100.0f, 5000.0f, "%.1f")) {
                camera->SetFarPlane(farPlane);
            }

            float ratio = farPlane / nearPlane;
            ImGui::Text("比率 (Far/Near): %.0f", ratio);
            if (ratio > 10000.0f) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[⚠ 低精度]");
            }
        }

        ImGui::Separator();
        if (ImGui::Button("カメラリセット")) {
            m_transform->translate = { 0.0f, 0.0f, -5.0f };
            m_transform->rotate = { 0.0f, 0.0f,  0.0f };
        }


    }
    end();
#endif
}