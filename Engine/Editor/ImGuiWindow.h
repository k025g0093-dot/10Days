#pragma once
#include "ConvertString.h"
#include "LightManager.h"
#include "AllComponent.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imguizmo.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // USE_IMGUI



// 🌟 超重要：相互参照バグを防ぐための前方宣言
class TUFEngine;

class ImGuiUIWindow
{
public:
    ImGuiUIWindow();
    ~ImGuiUIWindow();

    // 💡 引数に TUFEngine* を追加（virtualなので子クラスもこれに合わせる）
    virtual void update(TUFEngine* engine);
    void Show();

protected:
    virtual bool begin(std::string name, ImGuiWindowFlags flags = 0);
    void end();
    bool show = true; // 💡 最初から表示状態（true）にしておくとバグりにくいです

    // ウィンドウ移動範囲の制限
    bool m_enableClamp = false;          // true で移動制限を有効化
    ImVec2 m_clampMin = { 0,0 };         // 制限範囲 (左上)
    ImVec2 m_clampMax = { 0,0 };         // 制限範囲 (右下)
    ImVec2 m_nextPos = { 0,0 };          // 次フレームで適用する位置
    bool m_needsClamp = false;
};



// ImGuiWindow.h に追加
class ImGuiSceneWindow : public ImGuiUIWindow
{
public:
    void update(TUFEngine* engine) override;

private:
    Entity* m_renamingEntity = nullptr;
    bool m_focusRenameField = false;
};

class ImGuiLightManagerWindow : public ImGuiUIWindow
{
public:
    void update(TUFEngine* engine) override;
};

class ImGuiViewportWindow : public ImGuiUIWindow
{
public:
    void update(TUFEngine* engine) override;
};

class ImGuiPlayViewportWindow : public ImGuiUIWindow
{
public:
    void update(TUFEngine* engine) override;
};

class ImGuiZmoWindow : public ImGuiUIWindow
{
public:

    int selectedIndex = -1;
	void update(TUFEngine* engine) override;
	void SetObjectsIndex(int index) { selectedIndex = index; }
};


class ImGuiComponentWindow :public ImGuiUIWindow
{
public:
    //ここにコンポーネントの情報表氏UIを実装していく
	void update(TUFEngine* engine) override;


};

