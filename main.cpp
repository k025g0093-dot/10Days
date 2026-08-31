#include "TUFEngine.h"
#include "ModelManager.h"
#include "Sphere.h"
#include "WaveGrid.h"
#include "Camera.h"
#include <algorithm>
#include <vector>
#include <cmath>

#include <fstream>
#include <iomanip>

#include "DebugCamer.h"
#include "Sound.h"

#include "LearnComponent.h"

#ifdef USE_IMGUI
#include "externals/imgui/imguizmo.h"
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	SetUnhandledExceptionFilter(ExportDump);



	const int32_t kClineWidth = 1280;
	const int32_t kClineHeight = 720;


	TUFEngine* engine = new TUFEngine(kClineWidth, kClineHeight, L"CG2_TUFEngine_LE2B_29_ヤマト_ユウヤ");
	assert(engine->GetDevice() != nullptr);

	ShowWindow(engine->GetHwnd(), nCmdShow);

	int uvChecker = engine->LoadTexture("resources/uvChecker.png");
	int monsterBall = engine->LoadTexture("resources/monsterBall.png");
	int umi = engine->LoadTexture("resources/ao.jpg");

	int normal = engine->LoadTexture("resources/top normal.png");

	MeshModel* modelData = ModelManager::GetInstance()->LoadModel("resources/skyBox", "skyDome.fbx");
	if (modelData) modelData->SetEnableLighting(0);

	Sound* sound = new Sound;
	SoundData soundData1 = sound->SoundLoad("resources/fanfare.wav");
	SoundData title = sound->SoundLoad("resources/title.mp3");

	DebugCamer::GetInstance().Initialize((float)kClineWidth, (float)kClineHeight);

	const int cubeCountX = 200;
	const int cubeCountZ = 200;

	WaveGrid waveGrid(cubeCountX, cubeCountZ, ModelManager::GetInstance()->GetSceneObjects());

	// ========== GPU初期化 ==========
	waveGrid.InitializeGPU(engine->GetDevice(), engine);

	int wallX = cubeCountX / 5;
	int wall2 = cubeCountX / 2;
	int holeStart = cubeCountZ / 2 - 3;
	int holeEnd = cubeCountZ / 2 + 3;

	//for (int gz = 0; gz < cubeCountZ; gz++) {
	//	waveGrid.setWall(wallX, gz, (gz < holeStart || gz >= holeEnd));
	//	waveGrid.setWall(wall2, gz, (gz < holeStart || gz >= holeEnd));
	//}

	float waveStrength = 10.0f;
	DynamicMesh mesh(cubeCountX, cubeCountZ);
	std::vector<Vector4> normalColors(cubeCountX * cubeCountZ);
	float t = 0.0f;

	const auto& droppedMeshes = ModelManager::GetInstance()->GetSceneObjects();

	engine->m_camera.transform.translate.x = 0.0f;
	engine->m_camera.transform.translate.y = 5.0f;
	engine->m_camera.transform.translate.z = -20.0f;
	engine->m_camera.transform.rotate.x = 0.0f;

	bool  useMonsterBall = true;
	float cameraRotateSpeed = 0.016f;

	Vector3 spherePos[2] = { {0.0f, 0.0f, 5.0f}, {5.0f, 0.0f, 5.0f} };
	Vector3 sphereRot[2] = { {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Vector3 sphereScale[2] = { {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f} };
	bool    sphereUseMonsterBall[2] = { true, true };

	Vector3 meshPos = { 0.0f, 0.0f, 0.0f };
	Vector3 meshRot = { 0.0f, 0.0f, 0.0f };
	Vector3 meshScale = { 1.0f, 1.0f, 1.0f };

	Vector3 triBasePos = { 0.0f, 2.0f, 0.0f };
	Vector3 triRot = { 0.0f, 0.0f, 0.0f };
	Vector3 triScale = { 1.0f, 1.0f, 1.0f };
	Vector4 triColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	Vector2 spritePos = { 0.0f, 0.0f };
	Vector2 spriteScale = { 1.0f, 1.0f };
	Vector3 spriteRot = { 0.0f, 0.0f, 0.0f };

	TransformData uvTransformSprite{};
	uvTransformSprite.scale = { 1.0f, 1.0f, 1.0f };
	uvTransformSprite.rotate = { 0.0f, 0.0f, 0.0f };
	uvTransformSprite.translate = { 0.0f, 0.0f, 0.0f };

	int frameIndex = 0;
	int observeX = wallX + 40;





	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		else {

			////================================================================================================================
			////更新処理ここから
			////================================================================================================================

			engine->OnUpdate();

			//auto* entity = EntityManager::GetInstance()->CreateEntity("test");
			//entity->AddComponent<LearnComponent>();

#ifdef USE_IMGUI
			engine->GetImGuiManager()->onDrawGUI = [&]() {



				if (ImGui::Begin("シーン設定")) {

					if (ImGui::CollapsingHeader("ウェーブ設定")) {
						ImGui::DragFloat("波形の強さ", &waveStrength, 0.1f, 0.0f, 50.0f);
					}

					if (ImGui::CollapsingHeader("球体設定")) {
						for (int i = 0; i < 2; ++i) {
							ImGui::PushID(i);
							ImGui::Text("球体 %d", i);
							ImGui::DragFloat3("位置", &spherePos[i].x, 0.1f);
							ImGui::DragFloat3("回転", &sphereRot[i].x, 0.01f);
							ImGui::DragFloat3("拡大", &sphereScale[i].x, 0.01f, 0.01f, 10.0f);
							ImGui::Checkbox("モンスターボール", &sphereUseMonsterBall[i]);
							ImGui::Separator();
							ImGui::PopID();
						}
					}

					if (ImGui::CollapsingHeader("メッシュ設定")) {
						ImGui::DragFloat3("位置", &meshPos.x, 0.1f);
						ImGui::DragFloat3("回転", &meshRot.x, 0.01f);
						ImGui::DragFloat3("拡大", &meshScale.x, 0.01f, 0.01f, 10.0f);
					}

					if (ImGui::CollapsingHeader("三角形設定")) {
						ImGui::DragFloat3("基準位置", &triBasePos.x, 0.1f);
						ImGui::DragFloat3("回転", &triRot.x, 0.01f);
						ImGui::DragFloat3("拡大", &triScale.x, 0.01f, 0.01f, 10.0f);
						ImGui::ColorEdit4("色", &triColor.x);
					}

					if (ImGui::CollapsingHeader("スプライト設定")) {
						ImGui::DragFloat2("位置", &spritePos.x, 1.0f, -1280.0f, 1280.0f);
						ImGui::DragFloat2("拡大", &spriteScale.x, 0.1f, 0.01f, 20.0f);
						ImGui::SliderAngle("回転", &spriteRot.z);
					}

					if (ImGui::CollapsingHeader("スプライトUV設定")) {
						Vector2 uvScale = engine->GetSpriteUVScale();
						Vector2 uvTranslate = engine->GetSpriteUVTranslate();
						float uvRotate = engine->GetSpriteUVRotate();
						bool uvChanged = false;
						uvChanged |= ImGui::DragFloat2("UV移動", &uvTranslate.x, 0.01f, -10.0f, 10.0f);
						uvChanged |= ImGui::DragFloat2("UV拡大", &uvScale.x, 0.01f, -10.0f, 10.0f);
						uvChanged |= ImGui::SliderAngle("UV回転", &uvRotate);
						if (uvChanged) {
							engine->SetSpriteUVScale(uvScale);
							engine->SetSpriteUVTranslate(uvTranslate);
							engine->SetSpriteUVRotate(uvRotate);
						}
						if (ImGui::Button("UVリセット")) {
							engine->ResetSpriteUVTransform();
						}
					}
					
				}
				ImGui::End();
			};
#endif // USE_IMGUI

			engine->m_camera.transform.rotate.y += Input::GetRightStickX() * cameraRotateSpeed;
			engine->m_camera.transform.rotate.x -= Input::GetRightStickY() * cameraRotateSpeed;
			engine->m_camera.transform.translate.x += Input::GetLeftStickX();
			engine->m_camera.transform.translate.z += Input::GetLeftStickY();

			DebugCamer::GetInstance().Update();

			if (DebugCamer::GetInstance().IsDebug()) {
				engine->SetViewProjectionMatrix(DebugCamer::GetInstance().GetViewProjectionMatrix());
			}
			else {
				engine->ResetViewProjectionMatrix();
			}



			////================================================================================================================
			////描画処理ここから
			////================================================================================================================

			engine->PreDraw();


			for (int i = 0; i < 2; ++i) {
				engine->DrawSphere(
					spherePos[i],
					sphereRot[i],
					sphereScale[i],
					sphereUseMonsterBall[i] ? monsterBall : uvChecker,
					i + 1
				);
			}

			meshRot.y += 0.0016f;

			engine->DrawMesh(modelData, meshPos, meshRot, meshScale, -1);

			for (int i = 0; i < 10; i++) {
				engine->DrawTriangle(
					{ triBasePos.x + i * 0.5f, triBasePos.y, triBasePos.z },
					triRot,
					triScale,
					triColor,
					uvChecker);
			}

			engine->DrawSprite(
				spritePos,        // 表示位置
				300, 200,              // 幅・高さ
				spriteRot,     // 回転（なし）
				Vector3(spriteScale.x, spriteScale.y, 1.0f),     // スケール（等倍）
				Vector4(1, 1, 1, 1),  // 色（白＝そのまま表示）
				uvChecker          // 使いたいテクスチャのインデックス
			);
			// Grid
			{
				const float gridSize = 100.0f;
				const float step = 10.0f;
				Vector4 gridColor = { 0.3f, 0.3f, 0.3f, 1.0f };
				for (float i = -gridSize; i <= gridSize; i += step) {
					engine->DrawLine({ i, 0.0f, -gridSize }, { i, 0.0f, gridSize }, gridColor);
					engine->DrawLine({ -gridSize, 0.0f, i }, { gridSize, 0.0f, i }, gridColor);
				}
			}

			

			engine->PostDraw();

			////================================================================================================================
			////描画処理ここまで
			////================================================================================================================

			if (Input::GetKeyDown(VK_SPACE)) {
				sound->SoundPlayer(soundData1);
				sound->SoundPlayer(title);
			}
		}

		if (Input::GetKeyDown(VK_ESCAPE)) break;
	}

	sound->SoundUnLoad(&soundData1);
	delete sound;
	delete engine;

	return 0;
}