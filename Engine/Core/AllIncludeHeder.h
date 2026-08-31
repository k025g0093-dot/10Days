#pragma once
#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <cassert>
#include <filesystem>
#include <dbghelp.h>
#include <strsafe.h>
#include <string>
#include <format>

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"DbgHelp.lib")
// TUFEngine.h などのライブラリリンク部分に追加
#pragma comment(lib, "DirectXTex.lib")

#include "ConvertString.h"
#include "LogSistem.h"
#include "DXC.h"
#include "PSO.h"
#include "VertexResource.h"


#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include "externals/nlohmann/json.hpp"

#include <vector>
#include "TextureManager.h"
#include "allVector.h"

#include "Input.h"
#include "Model.h"
#include "allShapesModel.h"


#include "DynamicMeshModel.h"
#include "GpuDrivenRenderer.h"
#include "GpuDrivenTypes.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // USE_IMGUI

//機能敵なの
#include"ImGuiUIManager.h"
#include "Camera.h"
#include "LightManager.h"
#include "Create3DObjectOBB.h"
#include "AllComponent.h"
#include "EntityManager.h"


//陰に関するヘッダー
#include "ShadowMapBuffer.h"

//GUIのヘッダー関連
#include "ImGuiCamera.h"
#include "ImGuiDebug.h"
#include "ImGuiSpriteUV.h"
#include "ImGuiContentBrowser.h"

