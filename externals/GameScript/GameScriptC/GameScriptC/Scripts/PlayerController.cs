using System;
using Vortice.XInput;

// ==============================================
// PlayerController スクリプトテンプレート
// ==============================================
// このクラスはC++側から「PlayerController」という名前で呼び出されます。
// Update() が毎フレーム呼ばれるので、ここにキャラクターの挙動を書いてください。
//
// 使えるAPI一覧:
//   キーボード(押しっぱなし判定)  → IsKeyDown(ConsoleKey.任意のキー)
//   キーボード(押した瞬間だけ判定) → GameScriptC.Keyboard.IsKeyTriger(ConsoleKey.任意のキー)
//   ゲームパッド(ボタン単体)      → Gamepad.IsA() / IsB() / IsX() / IsY() / IsLB() / IsRB() / IsStart()
//   ゲームパッド(任意のボタン)     → Gamepad.IsButtonDown(0x1000 のようなビットマスク値)
//   ゲームパッド(スティック等の生値) → Gamepad.GetKeystate() でState構造体を直接取得
// ==============================================
public class PlayerController : Templet
{
    // ゲーム開始時に1回だけ呼ばれます。初期化処理をここに書いてください。
    public override void OnStart() { }

    // 毎フレーム呼ばれます。入力判定やロジックはここに書いてください。
    public override void Update()
    {
        // --- キーボード: 押しっぱなし判定の例(W/A/S/Dで移動したい時など) ---
        // ※ 毎フレーム出力されて接触ログが埋もれるので、確認中はコメントアウトしている
        // if (IsKeyDown(ConsoleKey.W)) Console.WriteLine("W is held");
        // if (IsKeyDown(ConsoleKey.A)) Console.WriteLine("A is held");
        // if (IsKeyDown(ConsoleKey.S)) Console.WriteLine("S is held");
        // if (IsKeyDown(ConsoleKey.D)) Console.WriteLine("D is held");

        // --- キーボード: 「押した瞬間」だけ反応させたい場合(ジャンプなど連打防止したい時) ---
        if (GameScriptC.Keyboard.IsKeyTriger(ConsoleKey.Spacebar))
            Console.WriteLine("Space pressed");

        // --- ゲームパッド: ボタン単体の判定(よく使うボタンはショートカット関数が用意されています) ---
        if (Gamepad.IsA())
            Console.WriteLine("プレイヤーがジャンプ！");

        // 他のボタンも同様に呼べます(必要な行だけコメントを外して使ってください):
        // if (Gamepad.IsB())     Console.WriteLine("Bボタン押された");
        // if (Gamepad.IsX())     Console.WriteLine("Xボタン押された");
        // if (Gamepad.IsY())     Console.WriteLine("Yボタン押された");
        // if (Gamepad.IsLB())    Console.WriteLine("LBボタン押された");
        // if (Gamepad.IsRB())    Console.WriteLine("RBボタン押された");
        // if (Gamepad.IsStart()) Console.WriteLine("Startボタン押された");
    }

    // 座標をC++側と同期するための関数です。
    // x, y, z は「ref」なので、この関数内で書き換えた値がそのままC++側に反映されます。
    // dt (デルタタイム) は前フレームからの経過時間(秒)なので、速度計算に使ってください。
    //
    // 例: ゲームパッドの左スティックでキャラクターを移動させたい場合
    //   var state = Gamepad.GetKeystate();
    //   if (state != null)
    //   {
    //       var gp = state.Value.Gamepad;
    //       const float deadZone = 8000f; // スティックの遊び(小さい傾きは無視する)
    //       const float speed = 5.0f;
    //       if (Math.Abs((float)gp.LeftThumbX) > deadZone)
    //           x += (gp.LeftThumbX / 32768f) * speed * dt;
    //       if (Math.Abs((float)gp.LeftThumbY) > deadZone)
    //           z += (gp.LeftThumbY / 32768f) * speed * dt;
    //   }


    // ==============================================
    // 接触イベント（C++のJolt → ScriptRuntime → ここ）
    // ==============================================
    // 届いているか確認するためのテスト実装。
    // 何かにぶつかったらコンソールにログを出して、上に飛ぶ。
    private float m_jumpCooldown = 0.0f;
    private const float kJumpVelocity = 20.0f;   // 飛び上がる速度(Y)
    private const float kJumpInterval = 0.4f;    // 連続で跳ねすぎないための間隔(秒)

    public override void OnCollisionEnter(CollisionInfo other)
    {
        Console.WriteLine($"[PlayerController] OnCollisionEnter  相手='{other.EntityName}'  id={other.EntityId}");
        TryJump();
    }

    public override void OnTriggerEnter(CollisionInfo other)
    {
        Console.WriteLine($"[PlayerController] OnTriggerEnter  相手='{other.EntityName}'  id={other.EntityId}");
        TryJump();
    }

    // ※ Exit系はC++側がまだ送っていない（OnContactRemovedが未実装）ので現状呼ばれない
    public override void OnCollisionExit(CollisionInfo other)
    {
        Console.WriteLine($"[PlayerController] OnCollisionExit  相手='{other.EntityName}'");
    }

    public override void OnTriggerExit(CollisionInfo other)
    {
        Console.WriteLine($"[PlayerController] OnTriggerExit  相手='{other.EntityName}'");
    }

    private void TryJump()
    {
        if (m_jumpCooldown > 0.0f)
        {
            Console.WriteLine("[PlayerController]   → クールダウン中なので飛ばない（イベント自体は届いている）");
            return;
        }
        m_jumpCooldown = kJumpInterval;
        Jump(kJumpVelocity);
        Console.WriteLine($"[PlayerController]   → 上に飛ぶ！ vy={kJumpVelocity}");
    }

    public override void GetMoveVelocity(ref float vx, ref float vz, float dt)
    {
        // ジャンプのクールダウンをここで減らす（dtが来るのがこの関数だけなので）
        if (m_jumpCooldown > 0.0f) m_jumpCooldown -= dt;

        const float speed = 15.0f;

        if (IsKeyDown(ConsoleKey.A)) vx -= speed;
        if (IsKeyDown(ConsoleKey.D)) vx += speed;
        if (IsKeyDown(ConsoleKey.W)) vz += speed;
        if (IsKeyDown(ConsoleKey.S)) vz -= speed;
    }

    public override void InPostion(ref float x, ref float y, ref float z, float dt)
    {
        float speed = 15.0f;

        if (IsKeyDown(ConsoleKey.W)) z += speed * dt;
        if (IsKeyDown(ConsoleKey.S)) z -= speed * dt;
        if (IsKeyDown(ConsoleKey.A)) x -= speed * dt;
        if (IsKeyDown(ConsoleKey.D)) x += speed * dt;

        var state = Gamepad.GetKeystate();
        if (state != null)
        {
            var gp = state.Value.Gamepad;
            const float deadZone = 8000f;
            if (Math.Abs((float)gp.LeftThumbX) > deadZone)
                x += (gp.LeftThumbX / 32768f) * speed * dt;
            if (Math.Abs((float)gp.LeftThumbY) > deadZone)
                z -= (gp.LeftThumbY / 32768f) * speed * dt;

            if (Gamepad.IsA() || IsKeyTriger(ConsoleKey.Spacebar))
                y = 13.0f;
        }
        else
        {
            if (IsKeyTriger(ConsoleKey.Spacebar))
                y = 3.0f;
        }
    }
}