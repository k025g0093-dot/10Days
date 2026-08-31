#pragma once
#include "AllIncludeHeder.h"
#include "FacadeJolt.h"
#include "Transform.h"
#include <wrl.h>
#include  <algorithm>

//ここから下はTUFEngineの中身を定義していきます。必要に応じて、構造体や関数を追加していきます。
//現在ここの中身がくそほどごちゃついててマジでファックなので随時修正していきます
//なにとぞよろしくお願いします

using Microsoft::WRL::ComPtr;
using json = nlohmann::json;

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
	Vector3 tangent;
};

class Line;

struct Material {
	Vector4 color;
	int32_t enableLighting;
	int32_t enableNormalMap;
	Vector2 padding;
	Matrix4x4 uvTransform;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

//struct DirectionalLight {
//	Vector4 color;
//	Vector3 direction;
//	float intensity;
//};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct DrawRequest {
	Model* model = nullptr;
	std::vector<Vector3> vertices;
	Vector4            color = { 1, 1, 1, 1 };
	std::vector<Vector4> colors ;
	std::vector<Vector2> uvs;
	Vector3            rot = { 0, 0, 0 };
	Vector3            scale = { 1, 1, 1 };
	Vector3            pos = { 0, 0, 0 };
	Vector2            posV2 = { 0, 0 };
	float              width = 0;
	float              height = 0;
	int                textureIndex = 0;
	bool               isMesh = false;
	bool               isSprite = false;
	bool               is2D = false;

	int lightId = -1;
	int renderOrder = 1;
	//新規追加波を作るやつ
	DynamicMeshModel* dynamicMeshPtr = nullptr;
	bool isDynamicMesh = false;
};

struct SceneObject {
	std::string name;
	MeshModel* mesh = nullptr;
	Transform transform;
	OBB obb;
	AABB localAABB;
	int lightId{};
	bool isSelected = false;
};

class TUFEngine {
public:

	Camera m_camera;


	TUFEngine(int32_t width, int32_t height, std::wstring name);
	~TUFEngine();
	static TUFEngine* GetInstance() { return s_instance; }
	void OnUpdate();

	int        LoadTexture(const std::string& filePath);

	void DrawTriangle(const Vector3& pos, const Vector3& rot, const Vector3& scale, const Vector4 color, int textureInd);

	void DrawSphere(const Vector3& pos, const Vector3& rot, const Vector3& scale, int textureIndex, int lightId = -1);
	void DrawMesh(MeshModel* mesh, Vector3 pos, Vector3 rot, Vector3 scale, int lightId = -1);

	void DrawSprite(const Vector2& pos, const float width, const float height,
		const Vector3& rot, const Vector3& scale, const Vector4 color, int textureIndex);
	void DrawDynamicMeshWithNormal(DynamicMesh& mesh, std::vector<Vector4>& colors, int index);


	void PreDraw();
	void PostDraw();

	ID3D12Device* GetDevice() { return device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() { return commandList.Get(); }
	ID3D12RootSignature* GetRootSignature() { return rootSignature.Get(); }
	ID3D12PipelineState* GetPipelineState() { return pipelineState.Get(); }
	HWND                       GetHwnd()       const { return hwnd; }
	ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return srvDescriptorHeap.Get(); }
	ID3D12DescriptorHeap* GetDsvDescriptorHeap() const { return dsvDescriptorHeap.Get(); }
	ImGuiUIManager* GetImGuiManager() { return m_imguiManager.get(); }

	void EnableDebugLayer();
	void SetupInfoQueue();

	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
		ID3D12Device* device,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		uint32_t numDescriptors,
		bool shaderVisible);

	Matrix4x4 GetViewMatrix() { return m_camera.GetViewMatrix(); }
	Matrix4x4 GetProjectionMatrix() { return m_camera.GetProjectionMatrix((float)width, (float)height); }

	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix; }
	// 外部からビュー行列を設定する場合（デバッグカメラなど）。PreDrawでの自動計算をスキップする
	void SetViewProjectionMatrix(const Matrix4x4& vp) { viewProjectionMatrix = vp; m_useCustomViewProjection = true; }
	void ResetViewProjectionMatrix() { m_useCustomViewProjection = false; }
	void SetDirectionalLightResource(ID3D12Resource* lightResource) { m_directionalLightResource = lightResource; }
	const Vector2& GetSpriteUVScale() const { return m_spriteUVScale; }
	const Vector2& GetSpriteUVTranslate() const { return m_spriteUVTranslate; }
	float GetSpriteUVRotate() const { return m_spriteUVRotate; }
	void SetSpriteUVScale(const Vector2& scale) { m_spriteUVScale = scale; }
	void SetSpriteUVTranslate(const Vector2& translate) { m_spriteUVTranslate = translate; }
	void SetSpriteUVRotate(float rotate) { m_spriteUVRotate = rotate; }
	void ResetSpriteUVTransform();
	Matrix4x4 GetSpriteUVTransformMatrix() const;


	void SetViewportSize(float  width, float  height) {
		m_sceneViewportSize = { static_cast<float>(width), static_cast<float>(height) };
	};

	Vector2 GetViewportSize() const {
		return m_sceneViewportSize;
	};
	ComPtr<ID3D12DescriptorHeap> m_sceneRtvDescriptorHeap;

	void SetCurrentRenderSize(float w, float h) {
		m_currentRenderWidth = w;
		m_currentRenderHeight = h;
	}

	void ResizeSceneRenderTexture(int newWidth, int newHeight);
	void GetSceneRtv(int32_t width, int32_t height);
	void BeginSceneRender();
	void EndSceneRender();


	DynamicMeshModel* GetDynamicMeshModel() const { return m_dynamicMeshModel.get(); }

	void ResizeWindow(int newWidth, int newHeight);

	float GetViewportWidth() const { return static_cast<float>(width); }
	float GetViewportHeight() const { return static_cast<float>(height); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetSceneSrvGpuHandle() const { return m_sceneSrvGpuHandle; }



	void DrawDebugOBB(const OBB& obb, const Vector4& color);

	void DrawLine(const Vector3& from, const Vector3& to, const Vector4& color);

	Entity* FindMainCameraEntity();
	void UpdateActiveCamera();

	//現在のモードを管理する関数
	void StartPlayMode();
	void PausePlayMode();
	void StopPlayMode();

private:




	static TUFEngine* s_instance;

	int          m_cbvIndex = 0;
	int          m_triangleRequestCount = 0;
	int m_maxDrawCount = 128;
	UINT8* m_pCbvDataBegin = nullptr;

	std::unique_ptr<ImGuiUIManager> m_imguiManager;
	std::unique_ptr<GpuDrivenRenderer> m_gpuDrivenRenderer;

	HWND    hwnd = nullptr;
	int32_t width = 0;
	int32_t height = 0;

	Vector2 m_sceneViewportSize;

	ComPtr<IDXGIFactory7>             dxgiFactory;
	ComPtr<ID3D12Device>              device;
	ComPtr<ID3D12CommandQueue>        commandQueue;
	ComPtr<ID3D12CommandAllocator>    commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	HRESULT hr = S_OK;

	ComPtr<IDXGISwapChain4>  swapChain;
	DXGI_SWAP_CHAIN_DESC1    swapChainDesc{};
	ComPtr<ID3D12Resource>   swapChainResources[2];



	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE  rtvHandles[2]{};
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};

	//ルートシグネチャ
	ComPtr<ID3D12RootSignature>  rootSignature;
	ComPtr<ID3D12PipelineState>  pipelineState;

	//GPU用のルートシグネチャ
	ComPtr<ID3D12RootSignature> gpuDrivenRootSignature;
	ComPtr<ID3D12PipelineState> gpuDrivenPipelineState;

	//shadow.VSに送るためのパイプ設定だったり
	ComPtr<ID3D12PipelineState> m_shadowPipelineState;
	ComPtr<ID3D12Resource> m_lightVPBuffer;

	// --- CSに送るためのシグネチャ関係 ---
	ComPtr<ID3D12RootSignature> m_computeRootSignature;
	ComPtr<ID3D12PipelineState> m_computePipelineState;

	ComPtr<ID3D12RootSignature> m_lineRootSignature;
	ComPtr<ID3D12PipelineState> m_linePipelineState;



	ComPtr<ID3D12Resource> depthStencilResource;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;

	Matrix4x4   viewProjectionMatrix;
	bool        m_useCustomViewProjection = false;
	Vector2     m_spriteUVScale = { 1.0f, 1.0f };
	Vector2     m_spriteUVTranslate = { 0.0f, 0.0f };
	float       m_spriteUVRotate = 0.0f;

	ComPtr<ID3D12Resource> m_dynamicMeshInstanceBuffer;
	InstanceData* m_mappedDynamicMeshInstanceData = nullptr;

	UINT m_descriptorSize = 0;


	// 2. CPU側のハンドル（「ここにビューを作ってね」とデバイスに教える用）
	D3D12_CPU_DESCRIPTOR_HANDLE heapStartCPU;

	// 3. GPU側のハンドル（「ここにあるビューを読んでね」とコマンドリストに教える用）
	D3D12_GPU_DESCRIPTOR_HANDLE heapStartGPU;

private:

	void GrowConstantBuffer();


	void InitWindow(std::wstring name);
	void InitializeDXGI(HWND hwnd);
	void InitializeImGui(HWND hwnd);
	ID3D12Resource* CreateDepthStencilTextureResource(int32_t width, int32_t height);
	void RenderAllRequests();
	void RenderGpuDrivenALLRequests();

	void RenderGpuDriven3D(const std::vector<DrawRequest>& requests3D);
	void RenderSprites2D(const std::vector<DrawRequest>& requests2D);

	void InitGpuDrivenResource();
	void InitGpuDrivenPipeline();

	static void ImGuiSrvAlloc(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpu);

	static void ImGuiSrvFree(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpu,
		D3D12_GPU_DESCRIPTOR_HANDLE gpu);

	uint32_t m_imguiSrvNext = 0;

private://描画物のリソース

	std::unique_ptr<DynamicMeshModel> m_dynamicMeshModel;
	std::unique_ptr<Sprite>           sprite;

	ComPtr<ID3D12Resource> m_pConstantBuffer;
	ComPtr<ID3D12Resource> m_cameraBuffer;
	ID3D12Resource* m_directionalLightResource = nullptr;

	std::vector<DrawRequest> m_drawRequests;

	std::unique_ptr<Sphere>       m_temporarySpheres;
	std::vector<std::unique_ptr<TriangleModel>> m_trianglePool;
	std::unique_ptr<Line>         m_line;



	ComPtr<ID3D12Resource> m_sceneColorResource;
	ComPtr<ID3D12Resource> m_sceneDepthResource;

	D3D12_CPU_DESCRIPTOR_HANDLE m_sceneRtvHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_sceneSrvCpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_sceneSrvGpuHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_sceneDsvHandle{};

	int32_t m_sceneTextureWidth = 0;
	int32_t m_sceneTextureHeight = 0;



	ComPtr<ID3D12Fence> m_fence;
	uint64_t            m_fenceValue = 0;
	HANDLE              m_fenceEvent = nullptr;
	bool m_needsImGuiRebuild = false;


	//ビューポートの値を保持するため
	float m_currentRenderWidth = (float)width;
	float m_currentRenderHeight = (float)height;


	uint32_t Align256(uint32_t size)
	{
		return (size + 0xff) & ~0xff;
	}
};