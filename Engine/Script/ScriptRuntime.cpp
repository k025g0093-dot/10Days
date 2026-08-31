#include "ScriptRuntime.h"
#include "GameScript.h"
#include "Entity.h"
#include "EntityManager.h"
#include <cstdio>

#define SR_LOG(msg) { OutputDebugStringA(msg); OutputDebugStringA("\n"); }

static const wchar_t* kPipeName = L"\\\\.\\pipe\\GameScriptPipe_Engine";
static const wchar_t* kExePath =
L"externals/GameScript/GameScriptC/GameScriptC/bin/Debug/net10.0/GameScriptC.exe";
static const char* kCsprojPath =
"externals/GameScript/GameScriptC/GameScriptC/GameScriptC.csproj";

// WaitNamedPipeW returns ERROR_FILE_NOT_FOUND immediately while the C# process
// is still starting and has not created the server pipe. Retry that case until
// the overall startup timeout expires.
static bool WaitForPipeToAppear(const wchar_t* pipeName, DWORD timeoutMs) {
	const ULONGLONG deadline = GetTickCount64() + timeoutMs;
	DWORD lastError = ERROR_FILE_NOT_FOUND;

	do {
		if (WaitNamedPipeW(pipeName, 200)) return true;

		lastError = GetLastError();
		if (lastError != ERROR_FILE_NOT_FOUND && lastError != ERROR_PIPE_BUSY) {
			SetLastError(lastError);
			return false;
		}
		Sleep(50);
	} while (GetTickCount64() < deadline);

	SetLastError(lastError);
	return false;
}



ScriptRuntime* ScriptRuntime::s_instance = nullptr;

ScriptRuntime* ScriptRuntime::GetInstance() {
	if (!s_instance) s_instance = new ScriptRuntime();
	return s_instance;
}

// ───────── バイト列ヘルパ ─────────
bool ScriptRuntime::WriteAll(HANDLE h, const void* d, DWORD len) {
	const char* p = (const char*)d;
	DWORD total = 0;
	while (total < len) {
		DWORD w = 0;
		if (!WriteFile(h, p + total, len - total, &w, NULL) || w == 0) return false;
		total += w;
	}
	return true;
}

bool ScriptRuntime::ReadAll(HANDLE h, void* d, DWORD len) {
	char* p = (char*)d;
	DWORD total = 0;
	while (total < len) {
		DWORD r = 0;
		if (!ReadFile(h, p + total, len - total, &r, NULL) || r == 0) return false;
		total += r;
	}
	return true;
}

// 可変長バッファに値を積むための小道具
static void PutI32(std::vector<char>& b, int32_t v) {
	b.insert(b.end(), (char*)&v, (char*)&v + 4);
}
static void PutF32(std::vector<char>& b, float v) {
	b.insert(b.end(), (char*)&v, (char*)&v + 4);
}
static void PutStr(std::vector<char>& b, const std::string& s) {
	PutI32(b, (int32_t)s.size());
	b.insert(b.end(), s.begin(), s.end());
}

// ───────── 登録 ─────────
int ScriptRuntime::Register(GameScript* gs, const std::string& scriptName) {
	int id = m_nextId++;
	Entry e;
	e.script = gs;
	e.name = scriptName;
	e.spawnPending = true;
	m_instances[id] = e;
	char b[320];
	sprintf_s(b, "[SR] Register: id=%d name='%s' instances=%d", id, scriptName.c_str(), (int)m_instances.size());
	SR_LOG(b);
	return id;
}

void ScriptRuntime::Unregister(int instanceId) {
	auto it = m_instances.find(instanceId);
	if (it == m_instances.end()) return;
	if (!it->second.spawnPending) {
		m_destroyPending.push_back(instanceId);   // C# 側にも消してもらう
	}
	m_instances.erase(it);
	char b[128];
	sprintf_s(b, "[SR] Unregister: id=%d instances=%d", instanceId, (int)m_instances.size());
	SR_LOG(b);
}

// ───────── プロセス起動 ─────────
bool ScriptRuntime::EnsureStarted() {
	if (m_hPipe != INVALID_HANDLE_VALUE) return true;
	if (m_startFailed) {
		SR_LOG("[SR] Start is suppressed after a previous failure. Reload the script to retry.");
		return false;
	}

	SR_LOG("[SR] EnsureStarted 開始");                    // ★

	SR_LOG("[SR] EnsureStarted: launching C# runtime");
	STARTUPINFOW si = { sizeof(si) };
	std::wstring cmd = L"GameScriptC.exe";

	if (!CreateProcessW(kExePath, &cmd[0], NULL, NULL, FALSE,
		0, NULL, NULL, &si, &m_pi)) {
		DWORD error = GetLastError();
		char log[512];
		sprintf_s(log, "[SR] ERROR CreateProcessW: err=%lu exe='%ls'", error, kExePath);
		SR_LOG(log);
		char b[128]; sprintf_s(b, "[SR] CreateProcessW 失敗 err=%lu", GetLastError());
		SR_LOG(b);                                        // ★ エラー番号も出す
		m_startFailed = true;
		return false;
	}
	SR_LOG("[SR] CreateProcessW OK。パイプ待機に入ります");  // ★

	if (!WaitForPipeToAppear(kPipeName, 8000)) {
		DWORD error = GetLastError();
		char log[256];
		sprintf_s(log, "[SR] ERROR WaitNamedPipeW: err=%lu pipe='%ls'", error, kPipeName);
		SR_LOG(log);
		DWORD exitCode = STILL_ACTIVE;
		if (m_pi.hProcess && GetExitCodeProcess(m_pi.hProcess, &exitCode)) {
			char processLog[160];
			sprintf_s(processLog, "[SR] C# process state after pipe wait: pid=%lu exitCode=%lu%s",
				m_pi.dwProcessId, exitCode, exitCode == STILL_ACTIVE ? " (still running)" : " (exited)");
			SR_LOG(processLog);
		}
		char b[128]; sprintf_s(b, "[SR] WaitNamedPipeW 失敗 err=%lu", GetLastError());
		SR_LOG(b);                                        // ★
		KillProcess();
		m_startFailed = true;
		return false;
	}
	SR_LOG("[SR] WaitNamedPipeW OK");                     // ★

	m_hPipe = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, 0, NULL);
	if (m_hPipe == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		char log[256];
		sprintf_s(log, "[SR] ERROR CreateFileW: err=%lu pipe='%ls'", error, kPipeName);
		SR_LOG(log);
		char b[128]; sprintf_s(b, "[SR] CreateFileW 失敗 err=%lu", GetLastError());
		SR_LOG(b);                                        // ★
		KillProcess();
		m_startFailed = true;
		return false;
	}

	SR_LOG("[ScriptRuntime] 接続しました");
	return true;
}

void ScriptRuntime::KillProcess() {
	if (m_hPipe != INVALID_HANDLE_VALUE) {
		SR_LOG("[SR] Closing pipe");
		CloseHandle(m_hPipe);
		m_hPipe = INVALID_HANDLE_VALUE;
	}
	if (m_pi.hProcess) {
		SR_LOG("[SR] Terminating C# runtime process");
		TerminateProcess(m_pi.hProcess, 0);
		CloseHandle(m_pi.hProcess);
		CloseHandle(m_pi.hThread);
		m_pi = {};
	}
}

void ScriptRuntime::Shutdown() {
	KillProcess();
	m_instances.clear();
	m_destroyPending.clear();
	m_physicsEvents.clear();
}

// ───────── 再ビルド ─────────
void ScriptRuntime::RebuildAndRestart() {
	KillProcess();
	m_startFailed = false;

	std::string cmd = std::string("dotnet build ") + kCsprojPath + " -c Debug --force";
	int r = system(cmd.c_str());
	if (r != 0) {
		SR_LOG("[ScriptRuntime] ★ C# のビルドに失敗。古い exe で起動します");
	}
	else {
		SR_LOG("[ScriptRuntime] C# build OK");
	}

	// 全インスタンスを作り直す
	for (auto& kv : m_instances) kv.second.spawnPending = true;
	m_destroyPending.clear();
}

// ───────── 毎フレーム ─────────
void ScriptRuntime::Tick(float dt) {

	static int s_n = 0;
	if (++s_n % 120 == 1) {
		char b[128];
		sprintf_s(b, "[SR] tick inst=%d pending=%d pipe=%d",
			(int)m_instances.size(), (int)m_destroyPending.size(),
			m_hPipe != INVALID_HANDLE_VALUE ? 1 : 0);
		SR_LOG(b);
	}

	if (m_instances.empty() && m_destroyPending.empty()) return;
	if (!EnsureStarted()) return;

	// ── 送信バッファを組む ──
	m_send.clear();
	PutI32(m_send, 1);                       // msgType = TICK

	// spawn
	std::vector<int> spawns;
	for (auto& kv : m_instances)
		if (kv.second.spawnPending) spawns.push_back(kv.first);

	PutI32(m_send, (int32_t)spawns.size());
	for (int id : spawns) {
		PutI32(m_send, id);
		PutStr(m_send, m_instances[id].name);
	}

	// destroy
	PutI32(m_send, (int32_t)m_destroyPending.size());
	for (int id : m_destroyPending) PutI32(m_send, id);
	m_destroyPending.clear();

	const auto& entities = EntityManager::GetInstance()->GetRuntimeEntities();

	PutI32(m_send, static_cast<int32_t>(entities.size()));

	for (const auto& entity : entities) {
		PutStr(m_send, entity->displayName);
		PutF32(m_send, entity->transform.position.x);
		PutF32(m_send, entity->transform.position.y);
		PutF32(m_send, entity->transform.position.z);
	}


	// tick（spawn 済みのものだけ）
	std::vector<int> ticks;
	for (auto& kv : m_instances)
		if (!kv.second.spawnPending) ticks.push_back(kv.first);


	PutI32(m_send, (int32_t)ticks.size());
	for (int id : ticks) {
		GameScript* gs = m_instances[id].script;
		Entity* e = gs ? gs->GetOwner() : nullptr;
		Vector3 p = e ? e->transform.position : Vector3{ 0,0,0 };

		Vector3 velocity = e && e->m_bodyIdRaw != UINT32_MAX
			? FacadeJolt::GetInstance()->GetLinearVelocity(e->m_bodyIdRaw)
			: Vector3{ 0, 0, 0 };

		PutI32(m_send, id);
		PutF32(m_send, p.x); PutF32(m_send, p.y); PutF32(m_send, p.z);
		PutF32(m_send, velocity.x); PutF32(m_send, velocity.y); PutF32(m_send, velocity.z);  // vel（ステップCで埋める）
		PutF32(m_send, dt);
		PutI32(m_send, 0);                   // flags
	}

	PutI32(m_send, static_cast<int32_t>(m_physicsEvents.size()));

	for (const auto& event : m_physicsEvents) {
		PutI32(m_send, event.targetScriptInstanceId);
		PutI32(m_send, static_cast<int32_t>(event.type));
		PutI32(m_send, event.otherEntityId);
		PutStr(m_send, event.otherEntityName);
	}

	if (!m_physicsEvents.empty()) {
		char eb[128];
		sprintf_s(eb, "[SR] Send physicsEvents=%d", (int)m_physicsEvents.size());
		SR_LOG(eb);
	}

	// 送ったら必ず捨てる。残すと毎フレーム同じ接触が C# に届き続ける
	m_physicsEvents.clear();

	// spawn 済みフラグを落とす（次フレームから tick 対象）
	for (int id : spawns) m_instances[id].spawnPending = false;

	// ── 送信 ──
	if (!spawns.empty() || !ticks.empty() || !m_destroyPending.empty()) {
		char b[160];
		sprintf_s(b, "[SR] Send: spawn=%d destroy=%d update=%d bytes=%d",
			(int)spawns.size(), (int)m_destroyPending.size(), (int)ticks.size(), (int)m_send.size());
		SR_LOG(b);
	}

	int32_t len = (int32_t)m_send.size();
	if (!WriteAll(m_hPipe, &len, 4) || !WriteAll(m_hPipe, m_send.data(), (DWORD)len)) {
		SR_LOG("[ScriptRuntime] 送信に失敗。接続を切ります");
		KillProcess();
		return;
	}

	// ── 受信 ──
	int32_t rlen = 0;
	if (!ReadAll(m_hPipe, &rlen, 4) || rlen < 0 || rlen > 8 * 1024 * 1024) {
		SR_LOG("[ScriptRuntime] 受信に失敗。接続を切ります");
		KillProcess();
		return;
	}
	m_recv.resize(rlen);
	if (rlen > 0 && !ReadAll(m_hPipe, m_recv.data(), (DWORD)rlen)) {
		KillProcess();
		return;
	}

	// ── コマンド適用 ──
	const char* rp = m_recv.data();
	auto GetI32 = [&]() { int32_t v; memcpy(&v, rp, 4); rp += 4; return v; };
	auto GetF32 = [&]() { float   v; memcpy(&v, rp, 4); rp += 4; return v; };

	int32_t cmdCount = GetI32();
	if (cmdCount > 0 || !spawns.empty()) {
		char b[128];
		sprintf_s(b, "[SR] Received: commands=%d bytes=%d", cmdCount, rlen);
		SR_LOG(b);
	}
	for (int32_t i = 0; i < cmdCount; ++i) {
		int32_t id = GetI32();
		int32_t mode = GetI32();
		float x = GetF32(), y = GetF32(), z = GetF32();

		auto it = m_instances.find(id);
		if (it == m_instances.end()) continue;
		GameScript* gs = it->second.script;
		Entity* e = gs ? gs->GetOwner() : nullptr;
		//joltから速度のgetterをもらって詰めれるように


		if (!e) continue;

		auto* jolt = FacadeJolt::GetInstance();
		auto velocity = jolt->GetLinearVelocity(e->m_bodyIdRaw);

		switch (mode) {
		case 0:  // SetPosition
			e->transform.position = { x, y, z };
			break;
		case 1:  // SetVelocity   ← 速度APIができたら繋ぐ
			if (e->m_bodyIdRaw == UINT32_MAX) break;

			// C#が指定した左右・前後の速度だけを反映
			velocity.x = x;
			velocity.z = z;

			// velocity.y はJoltの重力・ジャンプの結果を残す
			jolt->SetLinearVelocity(e->m_bodyIdRaw, velocity);

			break;
		case 2: { // Jump : Y だけ上書きする。XZ は Jolt / mode1 の結果をそのまま残す
			if (e->m_bodyIdRaw == UINT32_MAX) break;

			Vector3 jumpVelocity = velocity;
			jumpVelocity.y = y;

			jolt->WakeBody(e->m_bodyIdRaw);
			jolt->SetLinearVelocity(e->m_bodyIdRaw, jumpVelocity);

			char jb[160];
			sprintf_s(jb, "[SR] Jump: id=%d vy=%.2f", id, y);
			SR_LOG(jb);
			break;
		}

		case 3: // scale

			e->transform.scale = { x, y, z };
			FacadeJolt::GetInstance()->RebuildBody(e);

			break;

		default:
			break;
		}
	}
}


// ScriptRuntime.cpp
void ScriptRuntime::QueuePhysicsEvent(
	int targetScriptInstanceId,
	ScriptPhysicsEventType type,
	Entity* other)
{
	if (targetScriptInstanceId < 0 || !other) return;

	ScriptPhysicsEvent event;
	event.targetScriptInstanceId = targetScriptInstanceId;
	event.type = type;
	event.otherEntityId = static_cast<int>(other->m_bodyIdRaw);
	event.otherEntityName = other->displayName;

	m_physicsEvents.push_back(std::move(event));
}