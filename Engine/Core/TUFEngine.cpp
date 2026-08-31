#include "TUFEngine.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "ImGuiUIManager.h" 
#include "ImGuiWindow.h"
#include "EntityManager.h"
#include "MeshFilter.h"
#include "Line.h" 
#include "ScriptRuntime.h"

TUFEngine* TUFEngine::s_instance = nullptr;

// --- ウィンドウプロシージャ。Windowsからのメッセージを処理する ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
		return true;
	}
#endif // USE_IMGUI

	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED) {
			return 0; // 最小化されたときはリサイズ処理をスキップする
		}
		int pendingWidth = LOWORD(lParam);
		int pendingHeight = HIWORD(lParam);
		// ウィンドウがまだ作成されていない場合は、リサイズ処理をスキップする
		if (TUFEngine::GetInstance() == nullptr || TUFEngine::GetInstance()->GetHwnd() == nullptr) {
			return 0;
		}
		else {

			TUFEngine::GetInstance()->ResizeWindow(pendingWidth, pendingHeight);
		}

	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}


void TUFEngine::InitWindow(std::wstring name) {
	// 1. ウィンドウクラスを登録する
	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"MyWindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);

	// 2. クライアント領域のサイズからウィンドウ全体のサイズを計算する
	// width と height はクラスのメンバー変数に保存されている値を使う
	RECT wrc = { 0, 0, width, height };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// 3. ウィンドウを作成する
	// 作成したハンドルはメンバー変数 hwnd に保存する
	hwnd = CreateWindow(
		wc.lpszClassName,//利用するクラス
		name.c_str(),//体ろるばーの文字
		WS_OVERLAPPEDWINDOW,//よく見るwindowスタイル
		CW_USEDEFAULT,//表示座標X
		CW_USEDEFAULT,//Y
		wrc.right - wrc.left,//window横幅
		wrc.bottom - wrc.top,//windowの縦幅
		nullptr,//親windowハンドル
		nullptr,//メニューハンドル
		wc.hInstance,//インスタンスハンドル
		nullptr//オプション
	);

	assert(hwnd != nullptr); // 作成に失敗していないか確認する
}

#pragma region 初期化もとい必要なものの作成だったりを行っています

TUFEngine::TUFEngine(int32_t width, int32_t height, std::wstring name)
	: width(width), height(height) {
	s_instance = this;

	// --- 1. システム基盤の初期化 ---
	// COM は Windows 機能を使うために先に初期化しておく
	HRESULT hrCo = CoInitializeEx(0, COINIT_MULTITHREADED);

	// ログ用フォルダの作成とログ初期化
	std::filesystem::create_directory("logs");
	InitializeLog();

	// --- 2. ウィンドウとレンダラーの準備 ---
	InitWindow(name);


#ifdef _DEBUG
	EnableDebugLayer(); // デバッグレイヤーはデバイス作成前に有効化する
#endif

	InitializeDXGI(hwnd); // device や rootSignature などを作成する

	InitGpuDrivenResource();

	InitGpuDrivenPipeline();

	m_gpuDrivenRenderer = std::make_unique<GpuDrivenRenderer>();

	m_gpuDrivenRenderer->Initialize(device.Get(), srvDescriptorHeap.Get(), m_maxDrawCount);
	m_gpuDrivenRenderer->CreateCommandSignature(device.Get(), gpuDrivenRootSignature.Get());

	GetSceneRtv(width, height);

#ifdef _DEBUG
	SetupInfoQueue();
#endif


	pipelineState = CreatePipelineStateDesc(device.Get(), rootSignature, hr);
	gpuDrivenPipelineState =
		CreateGpuDrivenPipelineStateDesc(device.Get(), gpuDrivenRootSignature, hr);

	m_shadowPipelineState =
		CreateShadowPipelineState(device.Get(), gpuDrivenRootSignature, hr);

	m_linePipelineState =
		CreateLinePipelineState(device.Get(), m_lineRootSignature, hr);

#ifdef USE_IMGUI
	InitializeImGui(hwnd);
#endif

	LightManager::GetInstance()->Initialize(device.Get(), srvDescriptorHeap.Get());

	CreateDepthStencilTextureResource(width, height);

	TextureManager::GetInstance()->Initialize(device.Get(), srvDescriptorHeap.Get(), commandList.Get());
	ModelManager::GetInstance()->Initialize(device.Get(), commandList.Get());

	ShadowMapBuffer::GetInstance()->Initialize(device.Get(), srvDescriptorHeap.Get());
	m_lightVPBuffer = CreateBufferResource(device.Get(), Align256(sizeof(Matrix4x4)));

	FacadeJolt::GetInstance()->Init();

	auto sphere = std::make_unique<Sphere>();
	sphere->InitSphere(this);
	m_temporarySpheres = std::move(sphere);

	auto tri = std::make_unique<TriangleModel>();
	tri->Initialize(this);
	m_trianglePool.push_back(std::move(tri));

	auto line = std::make_unique<Line>();
	line->Initialize(this);
	m_line = std::move(line);

	// スプライト描画用モデルを初期化する

	auto sprite_ = std::make_unique<Sprite>();
	float sWidth = (float)width;
	float sheight = (float)height;


	sprite_->InitSprite(this, 0, sWidth, sheight);
	sprite = std::move(sprite_);



	std::ifstream f("sceneObject.json");
	if (f.is_open()) {
		json modelData = json::parse(f);
		for (const auto& object : modelData["objects"]) {
			std::string modelPath = object["modelPath"];
			std::string displyName = object["displayName"];
			std::string directory = modelPath.substr(0, modelPath.find_last_of("/"));
			std::string filename = modelPath.substr(modelPath.find_last_of("/") + 1);

			MeshModel* mesh = ModelManager::GetInstance()->LoadModel(directory, filename);
			if (!mesh) continue;

			Vector3 pos = { object["position"][0], object["position"][1], object["position"][2] };
			Vector3 rot = { object["rotation"][0], object["rotation"][1], object["rotation"][2] };
			Vector3 scale = { object["scale"][0], object["scale"][1], object["scale"][2] };

			Transform t;
			t.position = pos; t.rotation = rot; t.scale = scale;

			auto* entity = EntityManager::GetInstance()->CreateEntity(modelPath);
			auto* mf = entity->AddComponent<MeshFilter>();
			mf->model = mesh;
			entity->transform = t;
			entity->displayName = displyName;
			strncpy_s(entity->displayNameBuf, displyName.c_str(), sizeof(entity->displayNameBuf));

			Create3DObjectOBB obbCreator;
			entity->obb = obbCreator.CreateOBBForModel(*mesh, t.position);

			// localAABB を頂点データから計算（RenderGpuDrivenALLRequests での上書き対策）
			const Vertex* verts = mesh->GetVertexData();
			UINT vCount = mesh->GetVertexCount();
			Vector3 aabbMin = { FLT_MAX, FLT_MAX, FLT_MAX };
			Vector3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			for (UINT j = 0; j < vCount; j++) {
				aabbMin.x = (std::min)(aabbMin.x, verts[j].position.x);
				aabbMin.y = (std::min)(aabbMin.y, verts[j].position.y);
				aabbMin.z = (std::min)(aabbMin.z, verts[j].position.z);
				aabbMax.x = (std::max)(aabbMax.x, verts[j].position.x);
				aabbMax.y = (std::max)(aabbMax.y, verts[j].position.y);
				aabbMax.z = (std::max)(aabbMax.z, verts[j].position.z);
			}
			entity->localAABB = { aabbMin, aabbMax };

			if (object.contains("rigidbody")) {
				const auto& saved = object["rigidbody"];
				auto* rb = entity->AddComponent<Rigidbody>();
				rb->mass = saved.value("mass", rb->mass);
				rb->linearDrag = saved.value("linearDrag", rb->linearDrag);
				rb->angularDrag = saved.value("angularDrag", rb->angularDrag);
				rb->useGravity = saved.value("useGravity", rb->useGravity);
				rb->isKinematic = saved.value("isKinematic", rb->isKinematic);
			}

			if (object.contains("collider")) {
				const auto& saved = object["collider"];
				const std::string type = saved.value("type", "");
				if (type == "Box") {
					auto* box = entity->AddComponent<BoxCollider>();
					if (saved.contains("size") && saved["size"].size() == 3) {
						box->size = { saved["size"][0], saved["size"][1], saved["size"][2] };
					}
					box->isTrigger = saved.value("isTrigger", false);
				}
				else if (type == "Sphere") {
					auto* sphere = entity->AddComponent<SphereCollider>();
					sphere->radius = saved.value("radius", sphere->radius);
					sphere->isTrigger = saved.value("isTrigger", false);
				}
				else if (type == "ConvexHull") {
					auto* hull = entity->AddComponent<ConvexHullCollider>();
					hull->isTrigger = saved.value("isTrigger", false);
				}
			}

			//スクリプトがある場合それらを割り当てる
			if (object.contains("scriptName")) {
				auto* gs = entity->AddComponent<GameScript>();
				gs->m_scriptName = object["scriptName"];
				gs->ReloadScript();

			}

		}
	}
}

// TUFEngine.cpp
int TUFEngine::LoadTexture(const std::string& filePath) {
	return TextureManager::GetInstance()->LoadTexture(filePath);
}

Entity* TUFEngine::FindMainCameraEntity()
{
	for (const auto& entity : EntityManager::GetInstance()->GetActiveEntities()) {
		auto* camera = entity->GetComponent<CameraComponent>();

		if (camera && camera->isMainCamera) {
			return entity.get();
		}
	}

	return nullptr;
}

void TUFEngine::UpdateActiveCamera()
{
	Entity* mainCameraEntity = FindMainCameraEntity();
	if (!mainCameraEntity) return;

	auto* cameraComponent =
		mainCameraEntity->GetComponent<CameraComponent>();

	m_camera.transform.translate = mainCameraEntity->transform.position;
	m_camera.transform.rotate = mainCameraEntity->transform.rotation;
	m_camera.SetNearPlane(cameraComponent->nearClip);
	m_camera.SetFarPlane(cameraComponent->farClip);
}


// スタートストップなどの機能
void TUFEngine::StartPlayMode()
{
	EntityManager::GetInstance()->StartPlay();
}

void TUFEngine::PausePlayMode()
{
	auto* entities = EntityManager::GetInstance();

	if (entities->GetPlayState() == EntityManager::PlayState::Play) {
		entities->PausePlay();
	}
	else if (entities->GetPlayState() == EntityManager::PlayState::Pause) {
		entities->ResumePlay();
	}
}

void TUFEngine::StopPlayMode()
{
	EntityManager::GetInstance()->StopPlay();
}


void TUFEngine::OnUpdate() {
	static auto s_prevTime = std::chrono::high_resolution_clock::now();
	auto  nowTime = std::chrono::high_resolution_clock::now();
	float dt = std::chrono::duration<float>(nowTime - s_prevTime).count();
	s_prevTime = nowTime;
	dt = (std::min)(dt, 0.1f);

	Input::Update();
	EntityManager::GetInstance()->UpdateAll(dt);
	UpdateActiveCamera();
	if (EntityManager::GetInstance()->GetPlayState() == EntityManager::PlayState::Play) {
		ScriptRuntime::GetInstance()->Tick(dt);
	}
#ifdef USE_IMGUI
	if (m_imguiManager) { m_imguiManager->update(this); }
#endif
}

TUFEngine::~TUFEngine() {
	ModelManager::GetInstance()->SaveToFile();
	if (s_instance == this) {
		s_instance = nullptr;
	}
	if (m_fenceEvent) {
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}
	logStream.close();
	CoUninitialize();
}

void TUFEngine::ResetSpriteUVTransform() {
	m_spriteUVScale = { 1.0f, 1.0f };
	m_spriteUVTranslate = { 0.0f, 0.0f };
	m_spriteUVRotate = 0.0f;
}

Matrix4x4 TUFEngine::GetSpriteUVTransformMatrix() const {
	return MakeAffineMatrix(
		{ m_spriteUVScale.x, m_spriteUVScale.y, 1.0f },
		{ 0.0f, 0.0f, m_spriteUVRotate },
		{ m_spriteUVTranslate.x, m_spriteUVTranslate.y, 0.0f });
}

#pragma endregion

//ちなみにmainでも書けるように調整済み

#pragma region ImGuiの初期化関数基本的に ImGui を初期化していくつかのウィンドウを作成している

#ifdef USE_IMGUI

void TUFEngine::ImGuiSrvAlloc(
	ImGui_ImplDX12_InitInfo* info,
	D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
	D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
{
	auto* engine = static_cast<TUFEngine*>(info->UserData);

	const UINT descriptorSize =
		engine->device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	const uint32_t index = engine->m_imguiSrvNext++;
	assert(index < TextureManager::IMGUI_RESERVED);

	*cpu = engine->srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	cpu->ptr += static_cast<SIZE_T>(index) * descriptorSize;

	*gpu = engine->srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	gpu->ptr += static_cast<UINT64>(index) * descriptorSize;
}

void TUFEngine::ImGuiSrvFree(
	ImGui_ImplDX12_InitInfo*,
	D3D12_CPU_DESCRIPTOR_HANDLE,
	D3D12_GPU_DESCRIPTOR_HANDLE)
{
	// 今回は終了まで確保したままにする。
}

void TUFEngine::InitializeImGui(HWND hwnd) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

	ImGui::StyleColorsDark();

	// カスタムカラースキーム
	auto& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.06f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.08f, 0.22f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.06f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.06f, 0.35f, 0.35f, 0.90f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.08f, 0.45f, 0.45f, 0.90f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.06f, 0.35f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.10f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.80f, 0.80f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.70f, 0.70f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.85f, 0.85f, 1.00f);
	style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.04f, 0.04f, 0.06f, 1.00f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);


	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_InitInfo initInfo{};
	initInfo.Device = device.Get();
	initInfo.CommandQueue = commandQueue.Get();
	initInfo.NumFramesInFlight = swapChainDesc.BufferCount;
	initInfo.RTVFormat = swapChainDesc.Format;
	initInfo.SrvDescriptorHeap = srvDescriptorHeap.Get();
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
	initInfo.SrvDescriptorHeap = srvDescriptorHeap.Get();
	initInfo.UserData = this;
	initInfo.SrvDescriptorAllocFn = TUFEngine::ImGuiSrvAlloc;
	initInfo.SrvDescriptorFreeFn = TUFEngine::ImGuiSrvFree;
#endif


	ImGui_ImplDX12_Init(&initInfo);



	// フォント設定は DX12 の初期化後に行う
	ImGuiIO& io = ImGui::GetIO();
	ImFontConfig config;
	config.SizePixels = 13.0f;
	io.Fonts->AddFontDefault(&config);
	ImFontConfig jpConfig;
	jpConfig.MergeMode = true;
	jpConfig.SizePixels = 13.0f;
	io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msgothic.ttc", 13.0f, &jpConfig, io.Fonts->GetGlyphRangesJapanese());
	io.Fonts->Build();
	m_imguiManager = std::make_unique<ImGuiUIManager>(hwnd);

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
		platform_io.Renderer_RenderWindow = ImGuiUIManager::ViewportRenderCallback;
		platform_io.Renderer_SwapBuffers = ImGuiUIManager::ViewportSwapCallback;
	}


	//ここから各種windowを宣言していきます

	auto cameraWin = std::make_shared<ImGuiCamera>();
	cameraWin->SetTransform(&m_camera.transform);
	m_imguiManager->addWindow(cameraWin);

	auto debugWin = std::make_shared<ImGuiDebug>();
	m_imguiManager->addWindow(debugWin);

	auto contentBrowser = std::make_shared<ImGuiContentBrowser>();
	m_imguiManager->addWindow(contentBrowser);

	m_imguiManager->onFileDrop = [](const std::wstring& path) {
		ModelManager::GetInstance()->OnFileDropped(path);
		};

	auto sceneWin = std::make_shared<ImGuiSceneWindow>();
	m_imguiManager->addWindow(sceneWin);

	auto viewportWin = std::make_shared<ImGuiViewportWindow>();
	m_imguiManager->addWindow(viewportWin);

	auto imGuizmowindow = std::make_shared<ImGuiZmoWindow>();
	m_imguiManager->addWindow(imGuizmowindow);

	auto imGuiPlayWindow = std::make_shared<ImGuiPlayViewportWindow>();
	m_imguiManager->addWindow(imGuiPlayWindow);

	auto componentWin = std::make_shared<ImGuiComponentWindow>();
	m_imguiManager->addWindow(componentWin);

	auto ImGuiLightWin = std::make_shared<ImGuiLightManagerWindow>();
	m_imguiManager->addWindow(ImGuiLightWin);



}
#endif // USE_IMGUI

#pragma endregion



#pragma region DirectX 12 初期化関連

void TUFEngine::InitializeDXGI(HWND hwnd) {
	hr = CreateDXGIFactory(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
	assert(SUCCEEDED(hr));

	IDXGIAdapter4* useAdapter = nullptr;
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter))
		!= DXGI_ERROR_NOT_FOUND; i++) {

		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Log(logStream, ConvertString(std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };

	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(device.GetAddressOf()));
		if (SUCCEEDED(hr)) {
			Log(logStream, std::format("Feature Level {} is supported.\n", featureLevelStrings[i]));
			break;
		}
	}
	assert(device != nullptr);
	Log(logStream, "Complete DirectX 12 Device Creation.\n");

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue.GetAddressOf()));
	assert(SUCCEEDED(hr));

	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(commandAllocator.GetAddressOf()));
	assert(SUCCEEDED(hr));

	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf()));
	assert(SUCCEEDED(hr));

	swapChainDesc;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd,
		&swapChainDesc, nullptr, nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	rtvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	srvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);




	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(swapChainResources[0].GetAddressOf()));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(swapChainResources[1].GetAddressOf()));
	assert(SUCCEEDED(hr));

	rtvDesc;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle =
		rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	rtvHandles[1].ptr = rtvHandles[0].ptr +
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);

	// 変換行列用の定数バッファを作成する
	UINT cbSize = (sizeof(TransformationMatrix) + 255) & ~255;
	// 複数オブジェクト分の行列を書き込める大きめのバッファを用意する
	m_pConstantBuffer = CreateBufferResource(device.Get(), cbSize * m_maxDrawCount);
	// 最初に一度だけ Map して、書き込み先ポインタを保持する
	m_pConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_pCbvDataBegin));



	UINT descriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);




	m_fenceValue = 0;
	hr = device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf()));
	assert(SUCCEEDED(hr));

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(m_fenceEvent != nullptr);

}
#pragma endregion

//深度バッファー

#pragma region	深度バッファーについてです

ID3D12Resource* TUFEngine::CreateDepthStencilTextureResource(
	int32_t width,
	int32_t height)
{
	D3D12_RESOURCE_DESC depthResourceDesc{};
	depthResourceDesc.Width = width;
	depthResourceDesc.Height = height;
	depthResourceDesc.MipLevels = 1;
	depthResourceDesc.DepthOrArraySize = 1;
	depthResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthResourceDesc.SampleDesc.Count = 1;
	depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	depthStencilResource.Reset();
	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&depthResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(depthStencilResource.GetAddressOf())
	);
	assert(SUCCEEDED(hr));

	dsvDescriptorHeap = CreateDescriptorHeap(
		device.Get(),
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		1,
		false
	);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	device->CreateDepthStencilView(
		depthStencilResource.Get(),
		&dsvDesc,
		dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
	);

	return depthStencilResource.Get();
}

#pragma endregion


#pragma region 描画コマンド




void TUFEngine::PreDraw() {
	m_triangleRequestCount = 0;
	m_cbvIndex = 0;
	m_drawRequests.clear();
	if (m_line) m_line->Clear();

	float renderWidth = m_sceneTextureWidth > 0 ? static_cast<float>(m_sceneTextureWidth) : static_cast<float>(width);
	float renderHeight = m_sceneTextureHeight > 0 ? static_cast<float>(m_sceneTextureHeight) : static_cast<float>(height);

	if (!m_useCustomViewProjection) {
		Matrix4x4 view = m_camera.GetViewMatrix();
		Matrix4x4 proj = m_camera.GetProjectionMatrix(renderWidth, renderHeight);
		viewProjectionMatrix = Multiply(view, proj);
	}

	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);

	float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };

	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

#ifdef USE_IMGUI
	// デバッグ時：オフスクリーン（ImGuiのビューポート用）に描画
	D3D12_RESOURCE_BARRIER offScreenbarrier{};
	offScreenbarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	offScreenbarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	offScreenbarrier.Transition.pResource = m_sceneColorResource.Get();
	offScreenbarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	offScreenbarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &offScreenbarrier);

	commandList->OMSetRenderTargets(1, &m_sceneRtvHandle, false, &dsvHandle);
	commandList->ClearRenderTargetView(m_sceneRtvHandle, clearColor, 0, nullptr);
#else
	// リリース時：直接バックバッファ（画面）に描画
	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

	// バックバッファ描画用にビューポートをウィンドウサイズに合わせる
	renderWidth = static_cast<float>(width);
	renderHeight = static_cast<float>(height);
#endif

	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	D3D12_VIEWPORT viewport{};
	viewport.Width = renderWidth;
	viewport.Height = renderHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &viewport);

	D3D12_RECT scissorRect{};
	scissorRect.left = 0;
	scissorRect.right = static_cast<LONG>(renderWidth);
	scissorRect.top = 0;
	scissorRect.bottom = static_cast<LONG>(renderHeight);
	commandList->RSSetScissorRects(1, &scissorRect);
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

}



void TUFEngine::PostDraw() {
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	// =============================================================
	// 【GPU描画の完了】 (デバッグ・リリース共通で1回だけ実行)
	// =============================================================

	RenderGpuDrivenALLRequests();

	// =============================================================
	// 【ライン描画】（シーンテクスチャに直接描画 → 両ビューポートに表示）
	// =============================================================
	if (m_line) {
		commandList->SetGraphicsRootSignature(m_lineRootSignature.Get());
		commandList->SetPipelineState(m_linePipelineState.Get());
		ID3D12DescriptorHeap* lineHeaps[] = { srvDescriptorHeap.Get() };
		commandList->SetDescriptorHeaps(1, lineHeaps);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandleLine = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		commandList->OMSetRenderTargets(1, &m_sceneRtvHandle, false, &dsvHandleLine);

		float lineRenderWidth = m_sceneTextureWidth > 0 ? static_cast<float>(m_sceneTextureWidth) : static_cast<float>(width);
		float lineRenderHeight = m_sceneTextureHeight > 0 ? static_cast<float>(m_sceneTextureHeight) : static_cast<float>(height);
		D3D12_VIEWPORT lineViewport{};
		lineViewport.Width = lineRenderWidth;
		lineViewport.Height = lineRenderHeight;
		lineViewport.MinDepth = 0.0f;
		lineViewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &lineViewport);

		D3D12_RECT lineScissorRect{};
		lineScissorRect.right = static_cast<LONG>(lineRenderWidth);
		lineScissorRect.bottom = static_cast<LONG>(lineRenderHeight);
		commandList->RSSetScissorRects(1, &lineScissorRect);

		m_line->Draw(commandList.Get(), viewProjectionMatrix);
	}

#ifdef USE_IMGUI
	// =============================================================
	// デバッグ時のみ：シーンテクスチャ → スワップチェーンへの切り替えと ImGui 描画
	// =============================================================
	D3D12_RESOURCE_BARRIER offScreenBarrier{};
	offScreenBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	offScreenBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	offScreenBarrier.Transition.pResource = m_sceneColorResource.Get();
	offScreenBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	offScreenBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList->ResourceBarrier(1, &offScreenBarrier);

	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

	float clearColor[] = { 0.01f, 0.125f, 0.5f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

	ID3D12DescriptorHeap* imguiHeaps[] = { srvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(1, imguiHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
#endif

	// =============================================================
	// 【動的メッシュのバッファ スワップ】
	// =============================================================
	if (m_dynamicMeshModel) {
		m_dynamicMeshModel->SwapBuffers();
	}

	// =============================================================
	// 【スワップチェーンを PRESENT 状態に遷移】
	// =============================================================
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList->ResourceBarrier(1, &barrier);

	// =============================================================
	// 【コマンドリスト実行 & フレーム同期】
	// =============================================================
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* commandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);

	swapChain->Present(1, 0);

	m_fenceValue++;
	commandQueue->Signal(m_fence.Get(), m_fenceValue);

	if (m_fence->GetCompletedValue() < m_fenceValue) {
		m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));
}



#pragma endregion

ComPtr<ID3D12DescriptorHeap> TUFEngine::CreateDescriptorHeap(
	ID3D12Device* device,
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	uint32_t numDescriptors,
	bool shaderVisible)
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(descriptorHeap.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

#pragma region Logに関する処理ここにあっていいのかの検討中

#ifdef _DEBUG
// --- デバッグレイヤー。エラー時に詳細情報を出すための機能 ---
void TUFEngine::EnableDebugLayer() {
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(FALSE);
	}
}

// --- メッセージフィルター。不要な警告や情報を無視する設定 ---
void TUFEngine::SetupInfoQueue() {
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);
	}
}
#endif

#pragma endregion 

#pragma region RenderRequests



void TUFEngine::RenderGpuDrivenALLRequests() {

	for (auto& obj : ModelManager::GetInstance()->GetSceneObjects()) {
		// 現在のtransformでOBBを更新
		Vector3 localCenter = (obj.localAABB.min + obj.localAABB.max) * 0.5f;
		Vector3 localHalf = (obj.localAABB.max - obj.localAABB.min) * 0.5f;

		Matrix4x4 rotMat = Multiply(Multiply(MakeRotateXMatrix(obj.transform.rotation.x), MakeRotateYMatrix(obj.transform.rotation.y)), MakeRotateZMatrix(obj.transform.rotation.z));

		Vector3 scaledLocalCenter = {
			localCenter.x * obj.transform.scale.x,
			localCenter.y * obj.transform.scale.y,
			localCenter.z * obj.transform.scale.z
		};
		Vector3 rotatedCenter = {
			scaledLocalCenter.x * rotMat.m[0][0] + scaledLocalCenter.y * rotMat.m[1][0] + scaledLocalCenter.z * rotMat.m[2][0],
			scaledLocalCenter.x * rotMat.m[0][1] + scaledLocalCenter.y * rotMat.m[1][1] + scaledLocalCenter.z * rotMat.m[2][1],
			scaledLocalCenter.x * rotMat.m[0][2] + scaledLocalCenter.y * rotMat.m[1][2] + scaledLocalCenter.z * rotMat.m[2][2]
		};

		obj.obb.center = obj.transform.position + rotatedCenter;
		obj.obb.orientations[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
		obj.obb.orientations[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
		obj.obb.orientations[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
		obj.obb.size = { localHalf.x * obj.transform.scale.x, localHalf.y * obj.transform.scale.y, localHalf.z * obj.transform.scale.z };

		DrawRequest req;
		req.model = obj.mesh;
		req.pos = obj.transform.position;
		req.rot = obj.transform.rotation;
		req.scale = obj.transform.scale;
		req.textureIndex = obj.mesh->GetTextureIndex();
		req.isMesh = true;
		req.lightId = 0;
		req.renderOrder = 1;
		m_drawRequests.push_back(req);
	}

	// Entity（MeshFilter）からの描画リクエスト
	for (auto& entity : EntityManager::GetInstance()->GetActiveEntities()) {
		auto* mf = entity->GetComponent<MeshFilter>();
		if (!mf || !mf->model) continue;

		// OBB更新
		Vector3 localCenter = (entity->localAABB.min + entity->localAABB.max) * 0.5f;
		Vector3 localHalf = (entity->localAABB.max - entity->localAABB.min) * 0.5f;
		Matrix4x4 rotMat = Multiply(
			Multiply(MakeRotateXMatrix(entity->transform.rotation.x),
				MakeRotateYMatrix(entity->transform.rotation.y)),
			MakeRotateZMatrix(entity->transform.rotation.z));
		Vector3 scaledLocalCenter = {
			localCenter.x * entity->transform.scale.x,
			localCenter.y * entity->transform.scale.y,
			localCenter.z * entity->transform.scale.z
		};
		Vector3 rotatedCenter = {
			scaledLocalCenter.x * rotMat.m[0][0] + scaledLocalCenter.y * rotMat.m[1][0] + scaledLocalCenter.z * rotMat.m[2][0],
			scaledLocalCenter.x * rotMat.m[0][1] + scaledLocalCenter.y * rotMat.m[1][1] + scaledLocalCenter.z * rotMat.m[2][1],
			scaledLocalCenter.x * rotMat.m[0][2] + scaledLocalCenter.y * rotMat.m[1][2] + scaledLocalCenter.z * rotMat.m[2][2]
		};
		entity->obb.center = entity->transform.position + rotatedCenter;
		entity->obb.orientations[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
		entity->obb.orientations[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
		entity->obb.orientations[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
		entity->obb.size = { localHalf.x * entity->transform.scale.x, localHalf.y * entity->transform.scale.y, localHalf.z * entity->transform.scale.z };

		DrawRequest req;
		req.model = mf->model;
		req.pos = entity->transform.position;
		req.rot = entity->transform.rotation;
		req.scale = entity->transform.scale;
		req.textureIndex = mf->model->GetTextureIndex();
		req.isMesh = true;
		req.lightId = 0;
		req.renderOrder = 1;
		m_drawRequests.push_back(req);
	}

	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 【前処理】3Dリクエストと2Dリクエストを分離
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	std::vector<DrawRequest> requests3D;
	std::vector<DrawRequest> requests2D;

	for (const auto& req : m_drawRequests) {
		if (req.isSprite) {
			requests2D.push_back(req);
		}
		else {
			requests3D.push_back(req);
		}
	}

	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 【3D オブジェクト描画】（GPU駆動）
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	if (!requests3D.empty()) {
		RenderGpuDriven3D(requests3D);
	}


	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	// 【2D スプライト描画】（旧パイプライン）
	// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
	if (!requests2D.empty()) {
		RenderSprites2D(requests2D);
	}



	m_drawRequests.clear();
}


void TUFEngine::RenderGpuDriven3D(const std::vector<DrawRequest>& requests3D) {
	if (requests3D.empty()) return;

	// バッファ拡張チェック
	if ((int)requests3D.size() > m_gpuDrivenRenderer->GetMaxDrawCount()) {
		m_gpuDrivenRenderer->GrowBuffers(
			commandQueue.Get(),
			m_fence.Get(),
			m_fenceEvent,
			m_fenceValue,
			static_cast<int>(requests3D.size())
		);
	}

	std::vector<DrawRequest> sortedRequests = requests3D;
	std::sort(sortedRequests.begin(), sortedRequests.end(),
		[](const DrawRequest& a, const DrawRequest& b) {
			// renderOrder を最優先（小さい方が先に描画）
			if (a.renderOrder != b.renderOrder) return a.renderOrder < b.renderOrder;

			// 次点：バッチ化
			if (a.model != b.model) return a.model < b.model;
			if (a.textureIndex != b.textureIndex) return a.textureIndex < b.textureIndex;
			return false;
		});

	// CPU→GPU データ転送
	int32_t currentInstanceCount = 0;
	std::vector<int> gpuInstanceIndex(sortedRequests.size());
	RawTransform* mappedTransformData = m_gpuDrivenRenderer->GetMappedTransformData();

	for (int i = 0; i < (int)sortedRequests.size(); i++) {
		if (currentInstanceCount >= m_gpuDrivenRenderer->GetMaxDrawCount()) break;

		gpuInstanceIndex[i] = currentInstanceCount;
		const DrawRequest& request = sortedRequests[i];

		// 安全ガード処理（既存コード）
		Vector3 safePos = request.pos;
		Vector3 safeRot = request.rot;
		Vector3 safeScale = request.scale;

		if (std::isnan(safePos.x) || std::isnan(safePos.y) || std::isnan(safePos.z)) {
			safePos = { 0.0f, 0.0f, 0.0f };
		}
		if (std::isnan(safeRot.x) || std::isnan(safeRot.y) || std::isnan(safeRot.z)) {
			safeRot = { 0.0f, 0.0f, 0.0f };
		}
		if (std::isnan(safeScale.x) || std::isnan(safeScale.y) || std::isnan(safeScale.z)) {
			safeScale = { 1.0f, 1.0f, 1.0f };
		}

		const float minScale = 0.0001f;
		if (std::abs(safeScale.x) < minScale) safeScale.x = (safeScale.x >= 0.0f) ? minScale : -minScale;
		if (std::abs(safeScale.y) < minScale) safeScale.y = (safeScale.y >= 0.0f) ? minScale : -minScale;
		if (std::abs(safeScale.z) < minScale) safeScale.z = (safeScale.z >= 0.0f) ? minScale : -minScale;

		mappedTransformData[currentInstanceCount].pos = safePos;
		mappedTransformData[currentInstanceCount].rot = safeRot;
		mappedTransformData[currentInstanceCount].scale = safeScale;

		currentInstanceCount++;
	}

	if (currentInstanceCount == 0) return;

	// Compute Shader 実行（既存のロジック）
	m_gpuDrivenRenderer->TransitionToUAV(commandList.Get());

	commandList->SetComputeRootSignature(m_computeRootSignature.Get());
	commandList->SetPipelineState(m_computePipelineState.Get());

	ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(1, heaps);

	struct ComputeParams {
		Matrix4x4 viewProj;
		int instanceCount;
		float pad[3];
	};
	ComputeParams cParams{};
	cParams.viewProj = viewProjectionMatrix;
	cParams.instanceCount = currentInstanceCount;

	commandList->SetComputeRoot32BitConstants(0, 17, &cParams, 0);
	commandList->SetComputeRootDescriptorTable(1, m_gpuDrivenRenderer->GetTransformSrvGpuHandle());
	commandList->SetComputeRootDescriptorTable(2, m_gpuDrivenRenderer->GetInstanceUavGpuHandle());

	UINT threadGroupsX = (currentInstanceCount + 63) / 64;
	commandList->Dispatch(threadGroupsX, 1, 1);

	m_gpuDrivenRenderer->TransitionToSRV(commandList.Get());

	// ──── Shadow Pass ────
	auto* shadowBuf = ShadowMapBuffer::GetInstance();
	shadowBuf->TransitionToDsv(commandList.Get());

	commandList->SetGraphicsRootSignature(gpuDrivenRootSignature.Get()); // ← 追加
	commandList->SetDescriptorHeaps(1, heaps);                             // ← 追加
	commandList->SetPipelineState(m_shadowPipelineState.Get());

	D3D12_VIEWPORT shadowVP{};
	shadowVP.Width = (float)ShadowMapBuffer::SHADOW_MAP_SIZE;
	shadowVP.Height = (float)ShadowMapBuffer::SHADOW_MAP_SIZE;
	shadowVP.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &shadowVP);



	D3D12_RECT shadowRect{};
	shadowRect.right = ShadowMapBuffer::SHADOW_MAP_SIZE;
	shadowRect.bottom = ShadowMapBuffer::SHADOW_MAP_SIZE;
	commandList->RSSetScissorRects(1, &shadowRect);

	auto shadowDsv = shadowBuf->GetDsvCpuHandle();
	commandList->OMSetRenderTargets(0, nullptr, false, &shadowDsv);
	commandList->ClearDepthStencilView(shadowDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// Light VP計算（仮の行列）
	LightData shadowLight = LightManager::GetInstance()->GetLight(0);
	Vector3 lightDir = shadowLight.dirOrPos.Normalized();
	Vector3 lightPos = lightDir * -50.0f;

	// forwardとupが平行(ライトがほぼ真上/真下を向いている)だと
	// MakeLookAtMatrixの外積計算がゼロベクトルになり、行列が縮退してしまう。
	// その場合はupベクトルを別の軸に切り替える。
	Vector3 upVector = { 0.0f, 1.0f, 0.0f };
	if (std::abs(lightDir.Dot(upVector)) > 0.99f) {
		upVector = { 0.0f, 0.0f, 1.0f };
	}

	Matrix4x4 lightView = MakeLookAtMatrix(lightPos, { 0.0f, 0.0f, 0.0f }, upVector);
	Matrix4x4 lightProj = MakeOrthographicMatrix(-50.0f, 50.0f, 50.0f, -50.0f, 0.1f, 100.0f);
	Matrix4x4 lightVP = Multiply(lightView, lightProj);
	


	Matrix4x4* mapped = nullptr;
	m_lightVPBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	if (mapped) { *mapped = lightVP; m_lightVPBuffer->Unmap(0, nullptr); }
	commandList->SetGraphicsRootConstantBufferView(9, m_lightVPBuffer->GetGPUVirtualAddress());

	commandList->SetGraphicsRootShaderResourceView(1,
		m_gpuDrivenRenderer->GetInstanceBuffer()->GetGPUVirtualAddress());

	// 全モデルをシャドウPSOで描画（既存のdraw loopと同じ構造）
	int sStart = 0;
	while (sStart < (int)sortedRequests.size()) {
		const DrawRequest& head = sortedRequests[sStart];
		int count = 1;
		while (sStart + count < (int)sortedRequests.size()) {
			const DrawRequest& next = sortedRequests[sStart + count];
			if (next.model != head.model || next.textureIndex != head.textureIndex || next.renderOrder != head.renderOrder) break;
			count++;
		}
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		if (head.model) {
			commandList->SetGraphicsRoot32BitConstant(5, gpuInstanceIndex[sStart], 0);
			head.model->Draw(commandList.Get(), head.textureIndex, count, gpuInstanceIndex[sStart]);
		}
		sStart += count;
	}

	shadowBuf->TransitionToSrv(commandList.Get());

	// ビューポート復元（PreDrawと同じ値）
	const float sceneRenderWidth = m_sceneTextureWidth > 0 ? static_cast<float>(m_sceneTextureWidth) : static_cast<float>(width);
	const float sceneRenderHeight = m_sceneTextureHeight > 0 ? static_cast<float>(m_sceneTextureHeight) : static_cast<float>(height);

	D3D12_VIEWPORT mainVP{};
	mainVP.Width = sceneRenderWidth;
	mainVP.Height = sceneRenderHeight;
	mainVP.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &mainVP);
	D3D12_RECT mainRect{};
	mainRect.right = (LONG)sceneRenderWidth;
	mainRect.bottom = (LONG)sceneRenderHeight;
	commandList->RSSetScissorRects(1, &mainRect);

	// 🌟 ここが抜けていた：メインシーン用のレンダーターゲットに戻す
	D3D12_CPU_DESCRIPTOR_HANDLE mainDsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &m_sceneRtvHandle, false, &mainDsvHandle);


	// グラフィックス描画


	// 🌟【順序注意】ルートシグネチャ切り替え後でないと、
	// SetGraphicsRoot32BitConstant等のパラメータ番号は正しく解釈されない。
	// Compute用シグネチャがバインドされたままここでパラメータ7番に書き込むと
	// 存在しない番号への書き込みになりGPUドライバごとクラッシュする（nvwgf2umx.dll等）。
	commandList->SetGraphicsRootSignature(gpuDrivenRootSignature.Get());
	commandList->SetPipelineState(gpuDrivenPipelineState.Get());
	commandList->SetDescriptorHeaps(1, heaps);

	commandList->SetGraphicsRoot32BitConstant(
		7, static_cast<UINT>(LightManager::GetInstance()->
			GetActiveLightCount()), 0);

	// RenderGpuDriven3D 内
	auto shadowHandle = ShadowMapBuffer::GetInstance()->GetSrvGpuHandle();

	commandList->SetGraphicsRootDescriptorTable(8, shadowHandle);

	commandList->SetGraphicsRootConstantBufferView(9, m_lightVPBuffer->GetGPUVirtualAddress());

	if (!m_cameraBuffer) {
		m_cameraBuffer = CreateBufferResource(device.Get(), Align256(sizeof(Vector4)));
	}

	Vector4* cameraData = nullptr;
	HRESULT hr = m_cameraBuffer->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
	if (FAILED(hr)) { assert(false); return; }
	cameraData->x = m_camera.transform.translate.x;
	cameraData->y = m_camera.transform.translate.y;
	cameraData->z = m_camera.transform.translate.z;
	cameraData->w = 1.0f;
	m_cameraBuffer->Unmap(0, nullptr);

	commandList->SetGraphicsRootConstantBufferView(6, m_cameraBuffer->GetGPUVirtualAddress());

	commandList->SetGraphicsRootShaderResourceView(
		1,
		m_gpuDrivenRenderer->GetInstanceBuffer()->GetGPUVirtualAddress()
	);

	LightManager::GetInstance()->Bind(commandList.Get(), 0);

	int start = 0;
	while (start < (int)sortedRequests.size()) {
		const DrawRequest& head = sortedRequests[start];

		int count = 1;
		while (start + count < (int)sortedRequests.size()) {
			const DrawRequest& next = sortedRequests[start + count];
			if (next.model != head.model ||
				next.textureIndex != head.textureIndex ||
				next.renderOrder != head.renderOrder) {
				break;
			}
			count++;
		}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (head.model) {
			UINT gpuStartIndex = static_cast<UINT>(gpuInstanceIndex[start]);
			commandList->SetGraphicsRoot32BitConstant(5, gpuStartIndex, 0);
			head.model->Draw(
				commandList.Get(),
				head.textureIndex,
				static_cast<UINT>(count),
				gpuStartIndex
			);
		}

		start += count;
	}
}

void TUFEngine::RenderSprites2D(const std::vector<DrawRequest>& requests2D) {
	if (requests2D.empty()) return;

	// 旧パイプラインに切り替え
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->SetPipelineState(pipelineState.Get());

	ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(1, heaps);

	LightManager::GetInstance()->Bind(commandList.Get(), 0);

	// 2D描画用にビューポートを設定（ウィンドウサイズ）
	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &viewport);


	D3D12_RECT scissorRect{};
	scissorRect.left = 0;
	scissorRect.right = width;
	scissorRect.top = 0;
	scissorRect.bottom = height;
	commandList->RSSetScissorRects(1, &scissorRect);

	for (const auto& req : requests2D) {
		if (!req.model || !sprite) continue;

		// スプライトをサイズ変更
		sprite->Resize(req.width, req.height);

		// 正射影行列を構築（画面座標系）
		Matrix4x4 ortho = MakeOrthographicMatrix(
			0.0f, 0.0f,
			static_cast<float>(width), static_cast<float>(height),
			0.1f, 100.0f
		);


		Matrix4x4 world = MakeAffineMatrix(
			req.scale,
			req.rot,
			{ req.posV2.x, req.posV2.y, 0.0f }
		);

		Matrix4x4 wvp = Multiply(world, ortho);

		sprite->SetWorldTransform(wvp, world);

		Matrix4x4 uvTransform = GetSpriteUVTransformMatrix();
		sprite->SetUVTransform(uvTransform);

		// ===== テクスチャを設定 =====
		commandList->SetGraphicsRootDescriptorTable(
			2,
			TextureManager::GetInstance()->GetGPUHandle(req.textureIndex)
		);

		// ===== 描画 =====
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		sprite->Draw(
			commandList.Get(),
			req.textureIndex,
			1,      // instanceCount
			0       // startInstanceLocation
		);
	}
}



#pragma endregion

// --- TUFEngine.cpp ---

#pragma region 各モデルの描画リクエスト処理

void TUFEngine::DrawSphere(const Vector3& pos, const Vector3& rot, const Vector3& scale,
	int textureIndex, int lightId) {
	if (!m_temporarySpheres) {
		m_temporarySpheres = std::make_unique<Sphere>();
		m_temporarySpheres->InitSphere(this);
	}

	DrawRequest req;
	req.pos = pos;
	req.rot = rot;
	req.scale = scale;
	req.textureIndex = textureIndex;
	req.isMesh = true;
	req.lightId = lightId; // ← DrawRequestにも追加が必要
	req.model = m_temporarySpheres.get();
	m_drawRequests.push_back(req);
}

// 三角形
void TUFEngine::DrawTriangle(const Vector3& pos, const Vector3& rot, const Vector3& scale, const Vector4 color, int textureIndex) {

	int triIndex = m_triangleRequestCount++;
	while ((int)m_trianglePool.size() <= triIndex) {
		auto tri = std::make_unique<TriangleModel>();
		tri->Initialize(this);
		m_trianglePool.push_back(std::move(tri));
	}

	TriangleModel* tri = m_trianglePool[triIndex].get();
	tri->UpdateVertices({ -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, 0);
	tri->UpdateVertices({ 0.0f,  0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, 1);
	tri->UpdateVertices({ 0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, 2);

	DrawRequest req;
	req.model = tri;  // このフレーム用の三角形インスタンス
	req.pos = pos;
	req.rot = rot;
	req.scale = scale;
	req.color = color;
	req.textureIndex = textureIndex;
	req.isMesh = false;
	m_drawRequests.push_back(req);
}

void TUFEngine::DrawSprite(
	const Vector2& pos,
	const float width,
	const float height,
	const Vector3& rot,
	const Vector3& scale,
	const Vector4 color,
	int textureIndex)
{
	sprite->Resize(width, height);
	DrawRequest req;
	req.isSprite = true;
	req.posV2 = pos;
	req.rot = rot;
	req.scale = scale;
	req.width = width;
	req.height = height;
	req.textureIndex = textureIndex; // 引数で指定されたテクスチャを使う
	req.isMesh = true;
	req.model = sprite.get();
	m_drawRequests.push_back(req);

}


void TUFEngine::DrawMesh(MeshModel* mesh, Vector3 pos, Vector3 rot, Vector3 scale, int lightId) {
	if (!mesh) return;
	DrawRequest req;
	req.model = mesh;
	req.pos = pos;
	req.rot = rot;
	req.scale = scale;
	req.textureIndex = mesh->GetTextureIndex();
	req.isMesh = true;
	req.lightId = lightId;
	m_drawRequests.push_back(req);
}




void TUFEngine::DrawDynamicMeshWithNormal(
	DynamicMesh& mesh,
	std::vector<Vector4>& colors,
	int index)
{
	uint32_t vertexCount = (uint32_t)mesh.getIndices().size();
	if (vertexCount == 0) return;

	if (!m_dynamicMeshModel) {
		m_dynamicMeshModel = std::make_unique<DynamicMeshModel>();
		m_dynamicMeshModel->Init(this, mesh.getGridW(), mesh.getGridH());
	}

	m_dynamicMeshModel->UpdateVertexColors(colors);

	DrawRequest req;
	req.model = m_dynamicMeshModel.get();
	req.pos = { 0.0f, 0.0f, 0.0f };
	req.rot = { 0.0f, 0.0f, 0.0f };
	req.scale = { 1.0f, 1.0f, 1.0f };
	req.textureIndex = index;
	req.lightId = 0;
	req.renderOrder = -1;  // ← -1 から 999 に変更（最後に描画）
	req.isMesh = true;
	m_drawRequests.push_back(req);
}




void TUFEngine::DrawLine(const Vector3& from, const Vector3& to, const Vector4& color) {
	if (!m_line) {
		m_line = std::make_unique<Line>();
		m_line->Initialize(this);
	}
	m_line->Add(from, to, color);
}

void TUFEngine::DrawDebugOBB(const OBB& obb, const Vector4& color) {
	Vector3 corners[8];
	for (int i = 0; i < 8; i++) {
		Vector3 offset = {
			obb.size.x * ((i & 1) ? 1.0f : -1.0f),
			obb.size.y * ((i & 2) ? 1.0f : -1.0f),
			obb.size.z * ((i & 4) ? 1.0f : -1.0f)
		};
		corners[i] = obb.center
			+ obb.orientations[0] * offset.x
			+ obb.orientations[1] * offset.y
			+ obb.orientations[2] * offset.z;
	}
	const int edges[12][2] = {
		{0,1},{2,3},{4,5},{6,7},
		{0,2},{1,3},{4,6},{5,7},
		{0,4},{1,5},{2,6},{3,7}
	};
	for (int i = 0; i < 12; i++) {
		DrawLine(corners[edges[i][0]], corners[edges[i][1]], color);
	}
}

#pragma endregion


//windowだったりいろいろなものの拡張機能作成場所

#pragma region 現在はその他機能として実装していますここは時期に削ったり移動する予定です
void TUFEngine::GrowConstantBuffer() {
	m_maxDrawCount *= 2;
	UINT cbSize = (sizeof(TransformationMatrix) + 255) & ~255;

	m_fenceValue++;
	commandQueue->Signal(m_fence.Get(), m_fenceValue);
	if (m_fence->GetCompletedValue() < m_fenceValue) {
		m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_pConstantBuffer->Unmap(0, nullptr);
	m_pConstantBuffer = CreateBufferResource(device.Get(), cbSize * m_maxDrawCount);
	m_pConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_pCbvDataBegin));
}



void TUFEngine::ResizeWindow(int newWidth, int newHeight) {
	if (newWidth <= 0 || newHeight <= 0) {
		return;
	}


	m_fenceValue++;
	commandQueue->Signal(m_fence.Get(), m_fenceValue);
	if (m_fence->GetCompletedValue() < m_fenceValue) {
		m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	width = newWidth;
	height = newHeight;

#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown(); // ← Invalidate/Createの代わり
	ImGui_ImplWin32_Shutdown();
#endif

	// GPUがbackbufferを使い終わるまで待つ

	swapChainResources[0].Reset();
	swapChainResources[1].Reset();

	swapChain->ResizeBuffers(
		2,
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		0
	);

	// RTVを再作成する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle =
		rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandles[0] = rtvStartHandle;
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(swapChainResources[0].GetAddressOf()));
	assert(SUCCEEDED(hr));
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	rtvHandles[1].ptr = rtvHandles[0].ptr +
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(swapChainResources[1].GetAddressOf()));
	assert(SUCCEEDED(hr));
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);

	CreateDepthStencilTextureResource(width, height);

#ifdef USE_IMGUI
	InitializeImGui(hwnd);
#endif

}

void TUFEngine::ResizeSceneRenderTexture(int newWidth, int newHeight) {
	if (newWidth <= 0 || newHeight <= 0) {
		return;
	}

	// 1. GPUが現在使っているリソースを解放する前に、処理の完了をしっかり待つ
	m_fenceValue++;
	commandQueue->Signal(m_fence.Get(), m_fenceValue);
	if (m_fence->GetCompletedValue() < m_fenceValue) {
		m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_sceneTextureWidth = newWidth;
	m_sceneTextureHeight = newHeight;

	// 3. 古いシーンテクスチャを一度リセットし、新しいサイズでRTVとSRVを作り直す
	m_sceneColorResource.Reset();
	GetSceneRtv(m_sceneTextureWidth, m_sceneTextureHeight);

	// 4. カラーバッファのサイズに合わせて、深度バッファも同じサイズで作り直す
	// （これを行わないと、サイズ不一致で3D描画時にエラーが出ます）
	CreateDepthStencilTextureResource(m_sceneTextureWidth, m_sceneTextureHeight);
}

void TUFEngine::GetSceneRtv(int32_t width,
	int32_t height) {

	D3D12_RESOURCE_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.DepthOrArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.SampleDesc.Count = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	clearValue.Color[0] = 0.05f;
	clearValue.Color[1] = 0.12f;
	clearValue.Color[2] = 0.25f;
	clearValue.Color[3] = 1.0f;

	m_sceneRtvDescriptorHeap.Reset();
	hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,  // ← RENDER_TARGET
		&clearValue,  // ← clearValue を渡す（nullptrは禁止）
		IID_PPV_ARGS(m_sceneColorResource.GetAddressOf()));
	assert(SUCCEEDED(hr));

	m_sceneRtvDescriptorHeap = CreateDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
	m_sceneRtvHandle = m_sceneRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateRenderTargetView(m_sceneColorResource.Get(), nullptr, m_sceneRtvHandle);

	// SRV作成
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	UINT srvIndex = 100;
	m_sceneSrvCpuHandle.ptr = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + descriptorSize * srvIndex;
	m_sceneSrvGpuHandle.ptr = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + descriptorSize * srvIndex;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(m_sceneColorResource.Get(), &srvDesc, m_sceneSrvCpuHandle);
}


void TUFEngine::InitGpuDrivenResource() {

	// 動的メッシュ用バッファのみ
	m_dynamicMeshInstanceBuffer = CreateBufferResource(
		device.Get(),
		sizeof(InstanceData),
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_FLAG_NONE
	);

	m_dynamicMeshInstanceBuffer->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&m_mappedDynamicMeshInstanceData)
	);
}

void TUFEngine::InitGpuDrivenPipeline() {

	HRESULT hr = S_OK;
	gpuDrivenRootSignature = CreateGpuDrivenRootSignature(device.Get(), hr);
	assert(SUCCEEDED(hr));
	gpuDrivenPipelineState = CreateGpuDrivenPipelineStateDesc(device.Get(), gpuDrivenRootSignature, hr);
	assert(SUCCEEDED(hr));
	m_computePipelineState = CreateComputePipelineState(device.Get(), m_computeRootSignature, hr);
	assert(SUCCEEDED(hr));

}

void TUFEngine::BeginSceneRender() {
	PreDraw();
}

void TUFEngine::EndSceneRender() {
	PostDraw();
}





#pragma endregion
