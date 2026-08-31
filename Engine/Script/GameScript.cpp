#include "GameScript.h"
#include "ScriptRuntime.h"

GameScript::~GameScript() {
    if (m_instanceId >= 0) {
        ScriptRuntime::GetInstance()->Unregister(m_instanceId);
    }
}

void GameScript::Start() {
    MonoBehaviour::Start();
    if (m_instanceId < 0) {
        m_instanceId = ScriptRuntime::GetInstance()->Register(this, m_scriptName);
        char log[320];
        sprintf_s(log, "[GameScript] Start: registered id=%d name='%s'", m_instanceId, m_scriptName.c_str());
        LOG(log);
    }
}

// ───────── 物理イベントの転送 ─────────
// Jolt が接触を検知 → FacadeJolt::DispatchEvents() → ここ → ScriptRuntime に積む
// → 同フレームの ScriptRuntime::Tick() でパイプ越しに C# の OnXxx が呼ばれる
void GameScript::OnTriggerEnter(Entity* other) {
    if (m_instanceId < 0 || !other) return;
    ScriptRuntime::GetInstance()->QueuePhysicsEvent(
        m_instanceId, ScriptPhysicsEventType::TriggerEnter, other);
}

void GameScript::OnTriggerExit(Entity* other) {
    if (m_instanceId < 0 || !other) return;
    ScriptRuntime::GetInstance()->QueuePhysicsEvent(
        m_instanceId, ScriptPhysicsEventType::TriggerExit, other);
}

void GameScript::OnCollisionEnter(Entity* other) {
    if (m_instanceId < 0 || !other) return;
    char log[320];
    sprintf_s(log, "[GameScript] OnCollisionEnter: id=%d other='%s'",
        m_instanceId, other->displayName.c_str());
    LOG(log);
    ScriptRuntime::GetInstance()->QueuePhysicsEvent(
        m_instanceId, ScriptPhysicsEventType::CollisionEnter, other);
}

void GameScript::OnCollisionExit(Entity* other) {
    if (m_instanceId < 0 || !other) return;
    ScriptRuntime::GetInstance()->QueuePhysicsEvent(
        m_instanceId, ScriptPhysicsEventType::CollisionExit, other);
}

void GameScript::StartScript() {
    if (!m_vsOpened) {
        m_vsOpened = true;
        ShellExecuteA(NULL, "open", "devenv.exe",
            "\"externals/GameScript/GameScriptC/GameScriptC/GameScriptC.csproj\"",
            NULL, SW_SHOWNORMAL);
    }
    CreateScript(m_scriptName);
    ReloadScript();
}

void GameScript::ReloadScript() {
    char log[320];
    sprintf_s(log, "[GameScript] ReloadScript: name='%s' oldId=%d", m_scriptName.c_str(), m_instanceId);
    LOG(log);
    // 名前が変わっているかもしれないので登録し直す
    if (m_instanceId >= 0) {
        ScriptRuntime::GetInstance()->Unregister(m_instanceId);
        m_instanceId = -1;
    }
    m_instanceId = ScriptRuntime::GetInstance()->Register(this, m_scriptName);

    ScriptRuntime::GetInstance()->RebuildAndRestart();
}

//csファイルがない場合に作成する
void GameScript::CreateScript(const std::string& name) {
	std::string tmplPath = "externals/GameScript/GameScriptC/GameScriptC/GameScript/Templet.cs.txt";
	std::string outPath = "externals/GameScript/GameScriptC/GameScriptC/Scripts/" + name + ".cs";
	//テンプレートなどのパスの取得

	//パスがそもそもあるかの確認
	if (std::filesystem::exists(outPath)) return;

	//ファイルシステム
	std::filesystem::create_directories(
		"externals/GameScript/GameScriptC/GameScriptC/Scripts");

	// テンプレートファイルを開いて読み込む
	std::ifstream tmpl(tmplPath);
	if (!tmpl) {
		LOG("[GameScript] Template not found: ");
		return;
	}
	std::string content((std::istreambuf_iterator<char>(tmpl)), std::istreambuf_iterator<char>());
	tmpl.close();

	//スクリプトの名前を新しくできるクラスに変更
	{
		// プレースホルダーをスクリプト名に全置換
		size_t pos = 0;
		const std::string placeholder = "#SCRIPTNAME#";
		while ((pos = content.find(placeholder, pos)) != std::string::npos) {
			content.replace(pos, placeholder.size(), name);
			pos += name.size();
		}
	}

	// 置換後の内容を出力先ファイルに書き込む
	std::ofstream file(outPath);
	file << content;
	file.close();
	LOG("[GameScript] Connected!");
}
