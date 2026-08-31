#pragma once
#include "ImGuiWindow.h"

class ImGuiSpriteUV : public ImGuiUIWindow {
public:
    ImGuiSpriteUV() {
        show = true;
    }

    void update(TUFEngine* engine) override;
};
