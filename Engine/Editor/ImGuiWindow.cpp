#include "ImGuiWindow.h"
#include "TUFEngine.h"
#include "ModelManager.h"

#include "FacadeJolt.h"
#include "BoxCollider.h"
#include <cmath>
#include <cstring>

// BoxCollider をメッシュの localAABB に合わせる
static void FitBoxColliderToMesh(Entity* e, BoxCollider* box) {
	if (!e || !box) return;
	Vector3 sz = e->localAABB.max - e->localAABB.min;
	if (sz.x <= 1e-5f && sz.y <= 1e-5f && sz.z <= 1e-5f) return;   // AABB 未計算
	box->size   = sz;
	box->center = (e->localAABB.min + e->localAABB.max) * 0.5f;
}

ImGuiUIWindow::ImGuiUIWindow() : show(true) {}
ImGuiUIWindow::~ImGuiUIWindow() {}
#ifdef USE_IMGUI
static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
#endif // DEBUG



//ここら辺は基底クラスと各種構造を理解するための仮実装です。今後、必要に応じて引数を追加したり、内容を充実させていきます。
// 💡 引数を追加
void ImGuiUIWindow::update(TUFEngine* engine) {
	if (begin("Default Window")) {
	}
	end();

}

void ImGuiUIWindow::Show() {
	show = true;
}

// ImGuiWindow.cpp
bool ImGuiUIWindow::begin(std::string name, ImGuiWindowFlags flags)
{
#ifdef USE_IMGUI
	if (!show) return false;

	// 前フレームで制限範囲外に出ていたら位置を矯正
	if (m_needsClamp) {
		ImGui::SetNextWindowPos(m_nextPos, ImGuiCond_Always);
		m_needsClamp = false;
	}

	return ImGui::Begin(name.c_str(), &show, flags);
#else
	return false;
#endif
}



void ImGuiUIWindow::end()
{
#ifdef USE_IMGUI
	if (m_enableClamp) {
		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		ImVec2 clamped;
		clamped.x = (std::max)(m_clampMin.x, (std::min)(pos.x, m_clampMax.x - size.x));
		clamped.y = (std::max)(m_clampMin.y, (std::min)(pos.y, m_clampMax.y - size.y));
		if (clamped.x != pos.x || clamped.y != pos.y) {
			m_nextPos = clamped;
			m_needsClamp = true;
		}
	}

	ImGui::End();
#endif
}

//=============================
//IGStartupWindowの実装
//=============================



//ここも似た感じの者です将来的にはもっといろんな情報であったりほかのクラスと連動していろんなことができるようにしていきたいですね
void ImGuiSceneWindow::update(TUFEngine* engine) {
#ifdef USE_IMGUI
	if (begin("シーン", ImGuiWindowFlags_MenuBar)) {

		// 🌟 メニューバー
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("ファイル")) {
				if (ImGui::MenuItem("保存")) {
					ModelManager::GetInstance()->SaveToFile();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		// オブジェクト一覧。クリックで選択、ダブルクリックで名前編集。
		auto& objects = EntityManager::GetInstance()->GetEntities();
		for (int i = 0; i < (int)objects.size(); i++) {
			ImGui::PushID(i);
			Entity* entity = objects[i].get();

			if (m_renamingEntity == entity) {
				if (m_focusRenameField) {
					ImGui::SetKeyboardFocusHere();
					m_focusRenameField = false;
				}

				const bool submitted = ImGui::InputText(
					"##EntityName", entity->displayNameBuf, sizeof(entity->displayNameBuf),
					ImGuiInputTextFlags_EnterReturnsTrue);
				const bool finished = submitted || ImGui::IsItemDeactivatedAfterEdit();
				if (finished) {
					if (entity->displayNameBuf[0] != '\0') {
						entity->displayName = entity->displayNameBuf;
					}
					m_renamingEntity = nullptr;
				}
			}
			else {
				const std::string& label = entity->displayName.empty() ? entity->name : entity->displayName;
				if (ImGui::Selectable(label.c_str(), entity->isSelected,
					ImGuiSelectableFlags_SpanAllColumns)) {
				for (int j = 0; j < (int)objects.size(); j++) {
					objects[j]->isSelected = false;
				}
				entity->isSelected = true;
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					strncpy_s(entity->displayNameBuf, entity->displayName.c_str(), sizeof(entity->displayNameBuf));
					m_renamingEntity = entity;
					m_focusRenameField = true;
				}
			}

			ImGui::PopID();
		}

	}
	end();

#endif
}

void ImGuiLightManagerWindow::update(TUFEngine* engine) {
#ifdef _DEBUG

	if (begin("ライティングのシーン管理", ImGuiWindowFlags_MenuBar))
	{

		//メニューバー
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("ファイル")) {
				if (ImGui::MenuItem("保存")) {
					//ここでいろいろなライトの保存の実装だったりを行います
					//ModelManager::GetInstance()->SaveToFile();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		LightManager* lm = LightManager::GetInstance();

		// --- グローバルライト（常に1個だけ、固定） ---
		if (ImGui::CollapsingHeader("グローバルライト")) {
			LightData global = lm->GetLight(0);
			bool changed = false;
			changed |= ImGui::ColorEdit4("色", &global.color.x);
			changed |= ImGui::DragFloat3("方向", &global.dirOrPos.x, 0.01f, -1.0f, 1.0f);
			changed |= ImGui::DragFloat("強度", &global.intensity, 0.01f, 0.0f, 10.0f);
			if (changed) lm->SetLight(0, global);
		}

		ImGui::Dummy(ImVec2(0.0f, 6.0f));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 6.0f));

		// --- ポイントライト一覧（使われているものだけ表示） ---
		for (int i = 1; i < LightManager::MAX_LIGHTS; i++) {
			if (!lm->IsLightActive(i)) continue;

			ImGui::PushID(i);
			std::string label = "ポイントライト " + std::to_string(i);

			if (ImGui::CollapsingHeader(label.c_str())) {
				LightData point = lm->GetLight(i);
				bool changed = false;
				changed |= ImGui::ColorEdit4("色", &point.color.x);
				changed |= ImGui::DragFloat3("位置", &point.dirOrPos.x, 0.1f);
				changed |= ImGui::DragFloat("強度", &point.intensity, 0.01f, 0.0f, 10.0f);

				if (ImGui::Button("ギズモで選択")) {
					lm->SetSelectedLight(i);
				}
				ImGui::SameLine();
				if (ImGui::Button("削除")) {
					lm->RemoveLight(i);
					ImGui::PopID();
					continue; // 削除直後にPopIDしたので、このまま次のループへ
				}

				if (changed) {
					point.type = 1;
					lm->SetLight(i, point);
				}
			}
			ImGui::PopID();
		}

		ImGui::Dummy(ImVec2(0.0f, 8.0f));

		// --- 追加ボタン ---
		if (ImGui::Button("+ ポイントライト追加")) {
			int newIndex = lm->AddLight();
			if (newIndex == -1) {
				ImGui::OpenPopup("LightFullPopup");
			}
		}
		if (ImGui::BeginPopup("LightFullPopup")) {
			ImGui::Text("これ以上ライトを追加できません（上限: %d）", LightManager::MAX_LIGHTS - 1);
			ImGui::EndPopup();
		}


	}
	end();

#endif // _DEBUG
}


//ギズモの仮実装。まだまだいらないものとか将来的に自由にアイテムを選択できるようにしたりしていきたい
void ImGuiZmoWindow::update(TUFEngine* engine) {
#ifdef _DEBUG
	auto& objects = EntityManager::GetInstance()->GetEntities();
	if (objects.empty()) return; // オブジェクトがなければ何もしない

	// キー入力による切り替え判定だけを行う
	if (ImGui::IsKeyPressed(ImGuiKey_T)) mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if (ImGui::IsKeyPressed(ImGuiKey_R)) mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if (ImGui::IsKeyPressed(ImGuiKey_S)) mCurrentGizmoOperation = ImGuizmo::SCALE;
#endif
}




// --- ImGuiWindow.cpp の最後に追記 ---

void ImGuiViewportWindow::update(TUFEngine* engine) {
#ifdef USE_IMGUI
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

	ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus;
	if (ImGuizmo::IsOver()) {
		viewportFlags |= ImGuiWindowFlags_NoMove;
	}

	if (ImGui::Begin("デバック用ビューポート", nullptr, viewportFlags)) {



		// --- ① シーン（ゲーム画面）の描画 ---
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		engine->SetCurrentRenderSize(viewportSize.x, viewportSize.y);

		// 🌟重要：タイトルバーを排除した、画像が描画される正確なスクリーン左上座標を取得
		ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();

		ImTextureID sceneTextureId = (ImTextureID)engine->GetSceneSrvGpuHandle().ptr;

		ImGui::Image(
			sceneTextureId,
			viewportSize,
			ImVec2(0.0f, 0.0f),
			ImVec2(1.0f, 1.0f),
			ImVec4(1.0f, 1.0f, 1.0f, 1.0f), // Tint（画像の色：白＝そのまま）
			ImVec4(0.0f, 0.0f, 0.0f, 0.0f)  // 🌟Border（枠線の色：透明。ここが1.0fで黒枠になると1ピクセルズレる原因になります）
		);

		// --- ② ギズモ（ImGuizmo）の描画 ---
#ifdef _DEBUG
		LightManager* lm = LightManager::GetInstance();
		int selectedLight = lm->GetSelectedLight();

		auto& objects = EntityManager::GetInstance()->GetEntities();

		bool hasEntitySelection = false;
		for (auto& obj : objects) {
			if (obj->isSelected) { hasEntitySelection = true; break; }
		}

		ImGuizmo::BeginFrame();
		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGuizmo::SetRect(imageScreenPos.x, imageScreenPos.y, viewportSize.x, viewportSize.y);

		if (selectedLight >= 1 && selectedLight < LightManager::MAX_LIGHTS) {
			// 🌟ライトのギズモ操作
			LightData light = lm->GetLight(selectedLight);

			Matrix4x4 lightTransform = MakeAffineMatrix(
				{ 1.0f, 1.0f, 1.0f },
				{ 0.0f, 0.0f, 0.0f },
				light.dirOrPos
			);

			if (ImGuizmo::Manipulate(
				&engine->GetViewMatrix().m[0][0],
				&engine->GetProjectionMatrix().m[0][0],
				ImGuizmo::TRANSLATE,
				ImGuizmo::LOCAL,
				&lightTransform.m[0][0]
			)) {
				float t[3], r[3], s[3];
				ImGuizmo::DecomposeMatrixToComponents(&lightTransform.m[0][0], t, r, s);
				light.dirOrPos = { t[0], t[1], t[2] };
				lm->SetLight(selectedLight, light);
			}
		}
		else if (hasEntitySelection) {
			// 🌟既存のEntity用ギズモ処理
			constexpr float kDegToRad = 3.1415926535f / 180.0f;

			for (int i = 0; i < (int)objects.size(); i++) {
				if (!objects[i]->isSelected) continue;

				Matrix4x4 transform = MakeAffineMatrix(objects[i]->transform.scale, objects[i]->transform.rotation, objects[i]->transform.position);

				if (ImGuizmo::Manipulate(
					&engine->GetViewMatrix().m[0][0],
					&engine->GetProjectionMatrix().m[0][0],
					mCurrentGizmoOperation,
					mCurrentGizmoMode,
					&transform.m[0][0]
				)) {
					float matrixTranslation[3], matrixRotation[3], matrixScale[3];
					ImGuizmo::DecomposeMatrixToComponents(&transform.m[0][0], matrixTranslation, matrixRotation, matrixScale);

					objects[i]->transform.position = { matrixTranslation[0], matrixTranslation[1], matrixTranslation[2] };
					objects[i]->transform.rotation = { matrixRotation[0] * kDegToRad, matrixRotation[1] * kDegToRad, matrixRotation[2] * kDegToRad };
					objects[i]->transform.scale = { matrixScale[0], matrixScale[1], matrixScale[2] };
				}
			}
		}
#endif // _DEBUG

		// --- ③ ドロップターゲットの設定 ---
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const wchar_t* itemPath = (const wchar_t*)payload->Data;
				auto manager = engine->GetImGuiManager();
				if (manager && manager->onFileDrop) {
					manager->onFileDrop(itemPath);
				}
			}
			ImGui::EndDragDropTarget();
		}

		// --- ③ Play操作バー（デバッグ用Viewport内のオーバーレイ） ---
		const auto playState = EntityManager::GetInstance()->GetPlayState();
		const char* stateLabel = playState == EntityManager::PlayState::Edit ? "EDIT"
			: playState == EntityManager::PlayState::Play ? "PLAY"
			: "PAUSE";

		ImGui::SetCursorScreenPos(ImVec2(imageScreenPos.x + 10.0f, imageScreenPos.y + 10.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.07f, 0.88f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
		ImGui::BeginChild("##ViewportPlayControls", ImVec2(245.0f, 32.0f), ImGuiChildFlags_Borders);
		ImGui::TextColored(
			playState == EntityManager::PlayState::Edit ? ImVec4(0.70f, 0.70f, 0.70f, 1.0f)
			: ImVec4(0.35f, 0.85f, 0.45f, 1.0f),
			"%s", stateLabel);
		ImGui::SameLine();
		if (ImGui::Button("Play##Viewport", ImVec2(50.0f, 0.0f))
			&& playState == EntityManager::PlayState::Edit) {
			engine->StartPlayMode();
		}
		ImGui::SameLine();
		if (ImGui::Button(playState == EntityManager::PlayState::Pause
			? "Resume##Viewport" : "Pause##Viewport", ImVec2(58.0f, 0.0f))
			&& playState != EntityManager::PlayState::Edit) {
			engine->PausePlayMode();
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop##Viewport", ImVec2(48.0f, 0.0f))
			&& playState != EntityManager::PlayState::Edit) {
			engine->StopPlayMode();
		}
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}

	ImGui::End();
	ImGui::PopStyleVar(2);
#endif
}






void ImGuiPlayViewportWindow::update(TUFEngine* engine) {
#ifdef USE_IMGUI
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

	if (ImGui::Begin("プレイビューポート", nullptr,
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBringToFrontOnFocus)) {

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();

		ImTextureID sceneTextureId = (ImTextureID)engine->GetSceneSrvGpuHandle().ptr;

		ImGui::Image(
			sceneTextureId,
			viewportSize,
			ImVec2(0.0f, 0.0f),
			ImVec2(1.0f, 1.0f),
			ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
			ImVec4(0.0f, 0.0f, 0.0f, 0.0f)
		);
	}

	ImGui::End();
	ImGui::PopStyleVar(2);
#endif
}



void ImGuiComponentWindow::update(TUFEngine* engine)
{
#ifdef USE_IMGUI


	// 🌟全体のパディング（内側の余白）を少し広げる
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));


	if (begin("コンポーネント")) {

		ImGui::Dummy(ImVec2(0.0f, 10.0f));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));



		for (auto& entity : EntityManager::GetInstance()->GetEntities()) {
			if (!entity->isSelected) continue;

			if (ImGui::Button("複製")) {
				Entity* selected = nullptr;
				for (auto& e : EntityManager::GetInstance()->GetEntities()) {
					if (e->isSelected) { selected = e.get(); break; }
				}
				if (selected) {
					EntityManager::GetInstance()->DuplicateEntity(selected);
					ModelManager::GetInstance()->SaveToFile();
				}
				break; // イテレータ無効化を回避
			}

			if (ImGui::CollapsingHeader("トランスフォーム")) {
				ImGui::DragFloat3("位置", &entity->transform.position.x, 0.1f);
				ImGui::DragFloat3("回転", &entity->transform.rotation.x, 0.01f);
				ImGui::DragFloat3("拡大", &entity->transform.scale.x, 0.01f, 0.01f, 10.0f);
			}

			// ループ中にコンポーネントを消すとイテレータが壊れるので、あとでまとめて消す
			Component* removeTarget = nullptr;

			for (auto* c : entity->GetComponents()) {
				const char* typeName = typeid(*c).name();
				if (std::strncmp(typeName, "class ", 6) == 0) typeName += 6;   // "class " を落とす
				if (dynamic_cast<CameraComponent*>(c)) typeName = "カメラ";

				ImGui::PushID(c);

				// CollapsingHeader が行全体のクリックを取ってしまうので、削除ボタンは左に置く
				bool wantRemove = ImGui::SmallButton("削除");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("このコンポーネントを削除します");
				}
				ImGui::SameLine();

				bool opened = ImGui::CollapsingHeader(typeName);

				if (wantRemove) {
					removeTarget = c;
				}

				if (opened) {
					if (auto* mf = dynamic_cast<MeshFilter*>(c)) {

						ImGui::Spacing();
						ImGui::Text("モデル: %s", mf->model ? "読み込み済み" : "なし");
						ImGui::Spacing();

					}
					else if (auto* lc = dynamic_cast<LearnComponent*>(c)) {
						ImGui::Spacing();
						ImGui::Text("ステータス: 実行中");

						ImGui::Spacing();

					}
					else if (auto* gs = dynamic_cast<GameScript*>(c)) {

						ImGui::Text("ここはC#のスクリプトの作成から実行までを行っています");

						ImGui::Spacing();


						ImGui::Text("これはからスクリプトの初期化またヴィジュアルスタジオを起動します");
						if (ImGui::Button("C#スクリプトを初期化＆起動")) {

							gs->StartScript(); // ボタンを押した時に1回だけ初期化されるので安全！
						}

						ImGui::Spacing();

						ImGui::Text("これは実際に書いたスクリプトののビルドを行います");
						if (ImGui::Button("C#のビルド")) {
							gs->ReloadScript();
							gs->Update();

						}


						ImGui::Text("現在の.csのクラス名を決めるところです");

						ImGui::Text("test: %s", gs->m_scriptNameBuf);
						if (ImGui::InputText("作成するスクリプトの名前（.csクラス）##script",
							gs->m_scriptNameBuf, sizeof(gs->m_scriptNameBuf))) {
							gs->m_scriptName = gs->m_scriptNameBuf;
						}

						if (ImGui::Button("スクリプト作成")) {
							//ここで新しいスクリプトと同時にヴィジュアルスタジオを立ち上げる
							gs->StartScript();
						}

					}
					else if (auto* rb = dynamic_cast<Rigidbody*>(c)) {
						bool dirty = false;

						dirty |= ImGui::Checkbox("重力を受ける", &rb->useGravity);
						dirty |= ImGui::Checkbox("キネマティック（手動で動かす）", &rb->isKinematic);

						ImGui::Spacing();
						dirty |= ImGui::DragFloat("質量", &rb->mass, 0.1f, 0.01f, 1000.0f);
						dirty |= ImGui::DragFloat("移動の減衰", &rb->linearDrag, 0.01f, 0.0f, 10.0f);
						dirty |= ImGui::DragFloat("回転の減衰", &rb->angularDrag, 0.01f, 0.0f, 10.0f);

						ImGui::Separator();

						// ── 現在の状態（読み取り専用）──
						if (entity->m_bodyIdRaw != UINT32_MAX) {
							auto* jolt = FacadeJolt::GetInstance();
							bool active = jolt->IsBodyActive(entity->m_bodyIdRaw);
							Vector3 vel = jolt->GetLinearVelocity(entity->m_bodyIdRaw);
							float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);

							ImGui::Text("BodyID : %u", entity->m_bodyIdRaw);
							ImGui::TextColored(
								active ? ImVec4(0.4f, 0.9f, 0.5f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
								active ? "状態   : 動作中" : "状態   : 停止（スリープ）");
							ImGui::Text("速度   : %.2f, %.2f, %.2f", vel.x, vel.y, vel.z);
							ImGui::Text("速さ   : %.2f m/s", speed);

							if (!active && ImGui::Button("叩き起こす")) {
								jolt->WakeBody(entity->m_bodyIdRaw);
							}
						}
						else {
							ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
								"物理ボディ未生成（コライダーを付けてください）");
						}

						ImGui::Separator();
						if (dirty) {
							ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
								"変更は再生成するまで反映されません");
						}
						if (ImGui::Button("物理ボディを再生成")) {
							FacadeJolt::GetInstance()->RebuildBody(entity.get());
						}
					}
					else if (auto* camera = dynamic_cast<CameraComponent*>(c)) {
						if (ImGui::Checkbox("メインカメラ", &camera->isMainCamera)
							&& camera->isMainCamera) {
							// Main Camera は同時に1台だけ選べるようにする。
							for (const auto& otherEntity : EntityManager::GetInstance()->GetEntities()) {
								if (otherEntity.get() == entity.get()) continue;
								if (auto* otherCamera = otherEntity->GetComponent<CameraComponent>()) {
									otherCamera->isMainCamera = false;
								}
							}
						}

						ImGui::DragFloat("視野角(FOV)", &camera->fov, 0.01f, 0.01f, 3.0f);
						ImGui::DragFloat("Near Clip", &camera->nearClip, 0.01f, 0.01f, camera->farClip);
						ImGui::DragFloat("Far Clip", &camera->farClip, 1.0f, camera->nearClip, 100000.0f);
					}
					else if (auto* box = dynamic_cast<BoxCollider*>(c)) {
						ImGui::DragFloat3("サイズ（全長）", &box->size.x, 0.05f, 0.001f, 10000.0f);
						ImGui::DragFloat3("中心オフセット", &box->center.x, 0.05f);
						ImGui::Checkbox("トリガー（すり抜けて検知だけ）", &box->isTrigger);

						if (ImGui::Button("メッシュに合わせる##fitbox")) {
							FitBoxColliderToMesh(entity.get(), box);
							FacadeJolt::GetInstance()->RebuildBody(entity.get());
						}
						ImGui::SameLine();
						ImGui::TextDisabled("(%.2f, %.2f, %.2f)",
							entity->localAABB.max.x - entity->localAABB.min.x,
							entity->localAABB.max.y - entity->localAABB.min.y,
							entity->localAABB.max.z - entity->localAABB.min.z);
						if (ImGui::Button("物理ボディを再生成##box")) {
							FacadeJolt::GetInstance()->RebuildBody(entity.get());
						}
					}
					else if (auto* sph = dynamic_cast<SphereCollider*>(c)) {
						ImGui::DragFloat("半径", &sph->radius, 0.05f, 0.01f, 100.0f);
						ImGui::Checkbox("トリガー（すり抜けて検知だけ）", &sph->isTrigger);
						if (ImGui::Button("物理ボディを再生成##sph")) {
							FacadeJolt::GetInstance()->RebuildBody(entity.get());
						}
					}
					else if (auto* hull = dynamic_cast<ConvexHullCollider*>(c)) {
						ImGui::Checkbox("トリガー（すり抜けて検知だけ）", &hull->isTrigger);
						ImGui::TextDisabled("現在は AABB のボックスで代用しています");
						if (ImGui::Button("物理ボディを再生成##hull")) {
							FacadeJolt::GetInstance()->RebuildBody(entity.get());
						}
					}
				}
				ImGui::PopID();
			}

			// ここで実際に削除する（ループの外なので安全）
			if (removeTarget) {
				entity->RemoveComponent(removeTarget);
				// Collider や Rigidbody を消した場合に備えて物理ボディを作り直す
				FacadeJolt::GetInstance()->RebuildBody(entity.get());
			}

			ImGui::Separator();
			if (ImGui::Button("+ コンポーネント追加")) {
				ImGui::OpenPopup("AddComponentPopup");
			}
			if (ImGui::BeginPopup("AddComponentPopup")) {
				if (!entity->GetComponent<MeshFilter>() && ImGui::MenuItem("MeshFilter"))
					entity->AddComponent<MeshFilter>();

				if (!entity->GetComponent<LearnComponent>() && ImGui::MenuItem("LearnComponent"))
					entity->AddComponent<LearnComponent>();

				if (!entity->GetComponent<GameScript>() && ImGui::MenuItem("C#スクリプト"))
					entity->AddComponent<GameScript>();

				if (!entity->GetComponent<Rigidbody>() && ImGui::MenuItem("rigidbodyコンポーネント"))
					entity->AddComponent<Rigidbody>();

				if (!entity->GetComponent<CameraComponent>() && ImGui::MenuItem("カメラ"))
					entity->AddComponent<CameraComponent>();


				// コライダーは1つだけ。基底 Collider で判定する
				if (!entity->GetComponent<Collider>() && ImGui::MenuItem("ボックスコライダー"))
					FitBoxColliderToMesh(entity.get(), entity->AddComponent<BoxCollider>());

				if (!entity->GetComponent<Collider>() && ImGui::MenuItem("コベクスコライダーコンポーネント"))
					entity->AddComponent<ConvexHullCollider>();

				if (!entity->GetComponent<Collider>() && ImGui::MenuItem("スフィアコライダーコンポーネント"))
					entity->AddComponent<SphereCollider>();



				ImGui::EndPopup();
			}

			ImGui::Spacing();
			if (ImGui::Button("削除")) {
				EntityManager::GetInstance()->DestroyEntity(entity.get());
				ModelManager::GetInstance()->SaveToFile();
				break;
			}
		}
	}


	ImGui::PopStyleVar(); // スタイル設定を元に戻す


	ImGui::End();

#endif // USE_IMGUI

}
