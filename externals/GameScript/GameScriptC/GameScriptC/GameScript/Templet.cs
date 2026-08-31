using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public enum PhysicsEventType
{
    TriggerEnter = 0,
    TriggerExit = 1,
    CollisionEnter = 2,
    CollisionExit = 3,
}

public readonly struct ScriptVector3
{
    public float X { get; }
    public float Y { get; }
    public float Z { get; }

    public ScriptVector3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public static ScriptVector3 operator -(ScriptVector3 a, ScriptVector3 b)
    {
        return new ScriptVector3(
            a.X - b.X,
            a.Y - b.Y,
            a.Z - b.Z);
    }

    public float LengthXZ()
    {
        return MathF.Sqrt(X * X + Z * Z);
    }
}

public readonly struct ScriptVector2
{
    public float X { get; }
    public float Z { get; }

    public ScriptVector2(float x, float z)
    {
        X = x;
        Z = z;
    }
}

public readonly struct EntityInfo
{
    public string Name { get; }
    public ScriptVector3 Position { get; }

    public EntityInfo(string name, ScriptVector3 position)
    {
        Name = name;
        Position = position;
    }
}

public readonly struct CollisionInfo
{
    public int EntityId { get; }
    public string EntityName { get; }

    public CollisionInfo(int entityId, string entityName)
    {
        EntityId = entityId;
        EntityName = entityName;
    }
}

public static class World
{
    private static readonly Dictionary<string, EntityInfo> s_entities = new();

    public static EntityInfo? FindByName(string name)
    {
        return s_entities.TryGetValue(name, out var entity)
            ? entity
            : null;
    }

    // Main.cs から毎フレーム呼ばれる
    internal static void BeginFrame()
    {
        s_entities.Clear();
    }

    // Main.cs から毎フレーム呼ばれる
    internal static void UpdateEntity(string name, float x, float y, float z)
    {
        s_entities[name] = new EntityInfo(
            name,
            new ScriptVector3(x, y, z));
    }
}

public class Templet
{
    [DllImport("user32.dll")]
    static extern short GetAsyncKeyState(int vKey);

    private static readonly Dictionary<ConsoleKey, bool> s_previousKeys = new();

    private float m_pendingJumpVelocityY;
    private bool m_hasPendingJump;

    // C++から毎フレーム届く、自分自身の現在座標
    public ScriptVector3 Position { get; internal set; }

    public static bool IsKeyDown(ConsoleKey key)
    {
        return (GetAsyncKeyState((int)key) & 0x8000) != 0;
    }

    // 押した瞬間だけ true
    public static bool IsKeyTriger(ConsoleKey key)
    {
        bool now = IsKeyDown(key);

        s_previousKeys.TryGetValue(key, out bool previous);
        s_previousKeys[key] = now;

        return now && !previous;
    }

    // WASD / 矢印キー、左スティック、十字キーを統合した水平移動入力。
    // X: 左(-1)〜右(+1)、Z: 後ろ(-1)〜前(+1)。斜め移動は正規化される。
    protected static ScriptVector2 GetMoveInput()
    {
        float x = 0.0f;
        float z = 0.0f;

        if (IsKeyDown(ConsoleKey.A) || IsKeyDown(ConsoleKey.LeftArrow)) x -= 1.0f;
        if (IsKeyDown(ConsoleKey.D) || IsKeyDown(ConsoleKey.RightArrow)) x += 1.0f;
        if (IsKeyDown(ConsoleKey.W) || IsKeyDown(ConsoleKey.UpArrow)) z += 1.0f;
        if (IsKeyDown(ConsoleKey.S) || IsKeyDown(ConsoleKey.DownArrow)) z -= 1.0f;

        Gamepad.GetLeftStickInput(out float stickX, out float stickZ);
        x += stickX;
        z += stickZ;

        if (Gamepad.IsDPadLeft()) x -= 1.0f;
        if (Gamepad.IsDPadRight()) x += 1.0f;
        if (Gamepad.IsDPadUp()) z += 1.0f;
        if (Gamepad.IsDPadDown()) z -= 1.0f;

        float length = MathF.Sqrt(x * x + z * z);
        if (length > 1.0f)
        {
            x /= length;
            z /= length;
        }

        return new ScriptVector2(x, z);
    }

    // Space またはゲームパッド A ボタンを押した瞬間だけ true。
    protected static bool IsJumpTriggered()
    {
        return IsKeyTriger(ConsoleKey.Spacebar) || Gamepad.IsATriggered();
    }

    // 自分以外も含めて、名前からEntity情報を取得する
    protected EntityInfo? FindEntity(string name)
    {
        return World.FindByName(name);
    }

    // Y方向の速度を指定してジャンプする
    protected void Jump(float velocityY)
    {
        m_pendingJumpVelocityY = velocityY;
        m_hasPendingJump = true;
    }

    // Main.cs が使用する
    public bool HasPendingJump => m_hasPendingJump;

    // Main.cs が使用する
    public float ConsumeJump()
    {
        float velocityY = m_pendingJumpVelocityY;

        m_pendingJumpVelocityY = 0.0f;
        m_hasPendingJump = false;

        return velocityY;
    }

    public virtual void OnStart() { }

    public virtual void Update() { }

    // 水平移動速度を設定する
    public virtual void GetMoveVelocity(ref float vx, ref float vz, float dt) { }

    public virtual void OnTriggerEnter(CollisionInfo other) { }

    public virtual void OnTriggerExit(CollisionInfo other) { }

    public virtual void OnCollisionEnter(CollisionInfo other) { }

    public virtual void OnCollisionExit(CollisionInfo other) { }
}
