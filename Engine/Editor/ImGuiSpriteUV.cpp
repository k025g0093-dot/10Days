#include "ImGuiSpriteUV.h"
#include "TUFEngine.h"

void ImGuiSpriteUV::update(TUFEngine* engine) {
#ifdef USE_IMGUI
    if (!show) return;

    if (begin("スプライトUV")) {
        Vector2 uvScale = engine->GetSpriteUVScale();
        Vector2 uvTranslate = engine->GetSpriteUVTranslate();
        float uvRotate = engine->GetSpriteUVRotate();

        bool changed = false;
        changed |= ImGui::DragFloat2("UVスケール", &uvScale.x, 0.01f, -10.0f, 10.0f);
        changed |= ImGui::DragFloat("UV回転", &uvRotate, 0.01f, -6.28318f, 6.28318f);
        changed |= ImGui::DragFloat2("UV移動", &uvTranslate.x, 0.01f, -10.0f, 10.0f);

        if (changed) {
            engine->SetSpriteUVScale(uvScale);
            engine->SetSpriteUVRotate(uvRotate);
            engine->SetSpriteUVTranslate(uvTranslate);
        }

        if (ImGui::Button("UVリセット")) {
            engine->ResetSpriteUVTransform();
        }

        end();
    }
#endif
}
