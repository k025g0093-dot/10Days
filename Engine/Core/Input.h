#pragma once
#include <windows.h>
#include <vector>
#include <cmath> // スティックの処理で std::max を使うため追加

// ゲームパッド出力ができるように追加
#include <XInput.h>
#pragma comment(lib, "xinput.lib")

class Input {
public:
    static void Update() {
        // --- キーボード ---
        memcpy(m_keysPrev, m_keysCurr, 256);
        GetKeyboardState(m_keysCurr);

        // --- マウス ---
        POINT point;
        GetCursorPos(&point);

        m_mouseDeltaX = point.x - m_mouseX;
        m_mouseDeltaY = point.y - m_mouseY;

        m_mouseX = point.x;
        m_mouseY = point.y;

        // --- ゲームパッド (追加) ---
        m_padPrev = m_padCurr; // 前フレームの状態を保存

        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(XINPUT_STATE));

        // 0番目のコントローラー（1P）の状態を取得
        if (XInputGetState(0, &state) == ERROR_SUCCESS) {
            m_isPadConnected = true;
            m_padCurr = state.Gamepad;
        }
        else {
            m_isPadConnected = false;
            ZeroMemory(&m_padCurr, sizeof(XINPUT_GAMEPAD));
        }
    }

    // --- キーボード関数 ---
    static bool GetKey(int vKey) { return (m_keysCurr[vKey] & 0x80) != 0; }
    static bool GetKeyDown(int vKey) { return ((m_keysCurr[vKey] & 0x80) != 0) && ((m_keysPrev[vKey] & 0x80) == 0); }
    static bool GetKeyUp(int vKey) { return ((m_keysCurr[vKey] & 0x80) == 0) && ((m_keysPrev[vKey] & 0x80) != 0); }

    // --- マウス関数 ---
    static int GetMouseDeltaX() { return m_mouseDeltaX; }
    static int GetMouseDeltaY() { return m_mouseDeltaY; }
    static bool GetMouseButton(int button) {
        int vKey = (button == 0) ? VK_LBUTTON : (button == 1) ? VK_RBUTTON : VK_MBUTTON;
        return (m_keysCurr[vKey] & 0x80) != 0;
    }

    // --- ゲームパッド関数 (追加) ---

    // パッドが接続されているか確認
    static bool IsPadConnected() { return m_isPadConnected; }

    // ボタンを「押し続けているか」 (例: XINPUT_GAMEPAD_A)
    static bool GetPadButton(WORD button) {
        return (m_padCurr.wButtons & button) != 0;
    }

    // ボタンを「押した瞬間か」
    static bool GetPadButtonDown(WORD button) {
        return ((m_padCurr.wButtons & button) != 0) && ((m_padPrev.wButtons & button) == 0);
    }

    // ボタンを「離した瞬間か」
    static bool GetPadButtonUp(WORD button) {
        return ((m_padCurr.wButtons & button) == 0) && ((m_padPrev.wButtons & button) != 0);
    }

    // 左スティックの入力を取る（戻り値は -1.0f ～ 1.0f。遊びも自動計算）
    static float GetLeftStickX() {
        if (std::abs(m_padCurr.sThumbLX) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) return 0.0f;
        return m_padCurr.sThumbLX / 32767.0f;
    }
    static float GetLeftStickY() {
        if (std::abs(m_padCurr.sThumbLY) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) return 0.0f;
        return m_padCurr.sThumbLY / 32767.0f;
    }

    static float GetLeftStick() {
        if (std::abs(m_padCurr.sThumbLX) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) return 0.0f;
        return m_padCurr.sThumbLX / 32767.0f;
    }

    // 右スティックの入力を取る（戻り値は -1.0f ～ 1.0f。遊びも自動計算）
    static float GetRightStickX() {
        if (std::abs(m_padCurr.sThumbRX) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) return 0.0f;
        return m_padCurr.sThumbRX / 32767.0f;
    }
    static float GetRightStickY() {
        if (std::abs(m_padCurr.sThumbRY) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) return 0.0f;
        return m_padCurr.sThumbRY / 32767.0f;
    }

    static float GetRightStick() {
        if (std::abs(m_padCurr.sThumbRX) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) return 0.0f;
        return m_padCurr.sThumbRX / 32767.0f;
    }

private:
    // キーボード管理
    static inline BYTE m_keysCurr[256] = { 0 };
    static inline BYTE m_keysPrev[256] = { 0 };

    // マウス管理
    static inline int m_mouseX = 0;
    static inline int m_mouseY = 0;
    static inline int m_mouseDeltaX = 0;
    static inline int m_mouseDeltaY = 0;

    // ゲームパッド管理 (追加)
    static inline bool           m_isPadConnected = false;
    static inline XINPUT_GAMEPAD m_padCurr = { 0 };
    static inline XINPUT_GAMEPAD m_padPrev = { 0 };
};