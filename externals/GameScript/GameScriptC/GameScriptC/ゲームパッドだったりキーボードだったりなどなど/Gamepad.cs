using System;
using Vortice.XInput;

public static class Gamepad
{
    // 認識済みのゲームパッド番号を保持(全エンティティ・全スクリプトで共有される状態)
    public static int? GamepadIndex { get; private set; }

    // XInputのボタンビット配置は仕様で固定なので、定数として直接持っておく
    // (enum側のメンバー名に依存しないため、型名の不確実性を回避できる)
    private const ushort BTN_DPAD_UP = 0x0001;
    private const ushort BTN_DPAD_DOWN = 0x0002;
    private const ushort BTN_DPAD_LEFT = 0x0004;
    private const ushort BTN_DPAD_RIGHT = 0x0008;
    private const ushort BTN_START = 0x0010;
    private const ushort BTN_BACK = 0x0020;
    private const ushort BTN_LEFT_THUMB = 0x0040;
    private const ushort BTN_RIGHT_THUMB = 0x0080;
    private const ushort BTN_LEFT_SHOULDER = 0x0100;
    private const ushort BTN_RIGHT_SHOULDER = 0x0200;
    private const ushort BTN_A = 0x1000;
    private const ushort BTN_B = 0x2000;
    private const ushort BTN_X = 0x4000;
    private const ushort BTN_Y = 0x8000;

    private static ushort s_previousButtons;
    private const float LEFT_STICK_DEAD_ZONE = 0.24f;

    // 他クラスから毎フレーム呼んでもらう更新関数
    public static State? GetKeystate()
    {
        if (GamepadIndex != null)
        {
            if (XInput.GetState((uint)GamepadIndex.Value, out var keystate))
                return keystate;
            else
                GamepadIndex = null;
        }
        else
        {
            for (var i = 0; i < 4; ++i)
            {
                if (XInput.GetState((uint)i, out var keystate))
                {
                    GamepadIndex = i;
                    return keystate;
                }
            }
        }
        return null;
    }

    // マスク値を直接指定して判定する汎用ヘルパー
    // (ushortにキャストしてからビット演算するので、enum型のメンバー名の有無に左右されない)
    public static bool IsButtonDown(ushort mask)
    {
        var state = GetKeystate();
        if (state == null) return false;
        return ((ushort)state.Value.Gamepad.Buttons & mask) != 0;
    }

    // 押した瞬間だけ true。ジャンプのような単発操作に使う。
    public static bool IsButtonTriggered(ushort mask)
    {
        var state = GetKeystate();
        ushort buttons = state.HasValue ? (ushort)state.Value.Gamepad.Buttons : (ushort)0;
        bool triggered = (buttons & mask) != 0 && (s_previousButtons & mask) == 0;
        s_previousButtons = buttons;
        return triggered;
    }

    // よく使うボタンはショートカット関数にしておくと、他クラスから呼びやすい
    public static bool IsA() => IsButtonDown(BTN_A);
    public static bool IsB() => IsButtonDown(BTN_B);
    public static bool IsX() => IsButtonDown(BTN_X);
    public static bool IsY() => IsButtonDown(BTN_Y);
    public static bool IsLB() => IsButtonDown(BTN_LEFT_SHOULDER);
    public static bool IsRB() => IsButtonDown(BTN_RIGHT_SHOULDER);
    public static bool IsStart() => IsButtonDown(BTN_START);
    public static bool IsATriggered() => IsButtonTriggered(BTN_A);
    public static bool IsDPadUp() => IsButtonDown(BTN_DPAD_UP);
    public static bool IsDPadDown() => IsButtonDown(BTN_DPAD_DOWN);
    public static bool IsDPadLeft() => IsButtonDown(BTN_DPAD_LEFT);
    public static bool IsDPadRight() => IsButtonDown(BTN_DPAD_RIGHT);

    // 左スティックを -1.0〜+1.0 の範囲へ正規化して返す。
    // 小さな入力は無視するので、スティックのドリフトでは動かない。
    public static void GetLeftStickInput(out float x, out float z)
    {
        x = 0.0f;
        z = 0.0f;

        var state = GetKeystate();
        if (!state.HasValue) return;

        float rawX = state.Value.Gamepad.LeftThumbX / 32767.0f;
        float rawZ = state.Value.Gamepad.LeftThumbY / 32767.0f;
        float magnitude = MathF.Min(1.0f, MathF.Sqrt(rawX * rawX + rawZ * rawZ));

        if (magnitude <= LEFT_STICK_DEAD_ZONE) return;

        float scaledMagnitude = (magnitude - LEFT_STICK_DEAD_ZONE) / (1.0f - LEFT_STICK_DEAD_ZONE);
        x = rawX / magnitude * scaledMagnitude;
        z = rawZ / magnitude * scaledMagnitude;
    }

    // デバッグ表示用(元のGamePadUpdateの役割はこちらに統合)
    public static void DebugPrintState()
    {
        var state = GetKeystate();
        if (state == null) return;

        var gp = state.Value.Gamepad;
        ushort b = (ushort)gp.Buttons; // ここで一度だけキャストしておく

        if ((b & BTN_A) != 0) Console.WriteLine("Aボタンが押されました");
        if ((b & BTN_B) != 0) Console.WriteLine("Bボタンが押されました");
        if ((b & BTN_X) != 0) Console.WriteLine("Xボタンが押されました");
        if ((b & BTN_Y) != 0) Console.WriteLine("Yボタンが押されました");
        if ((b & BTN_LEFT_SHOULDER) != 0) Console.WriteLine("LBボタンが押されました");
        if ((b & BTN_RIGHT_SHOULDER) != 0) Console.WriteLine("RBボタンが押されました");
        if ((b & BTN_START) != 0) Console.WriteLine("Startボタンが押されました");

        if (gp.LeftThumbX > 0) Console.WriteLine("左スティックが右に倒されました");
        else if (gp.LeftThumbX < 0) Console.WriteLine("左スティックが左に倒されました");

        if (gp.LeftThumbY > 0) Console.WriteLine("左スティックが上に倒されました");
        else if (gp.LeftThumbY < 0) Console.WriteLine("左スティックが下に倒されました");

        if (gp.RightThumbX > 0) Console.WriteLine("右スティックが右に倒されました");
        else if (gp.RightThumbX < 0) Console.WriteLine("右スティックが左に倒されました");

        if (gp.RightThumbY > 0) Console.WriteLine("右スティックが上に倒されました");
        else if (gp.RightThumbY < 0) Console.WriteLine("右スティックが下に倒されました");
    }
}
