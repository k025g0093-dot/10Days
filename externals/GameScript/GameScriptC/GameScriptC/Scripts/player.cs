using System;

// ==============================================
// player
// ==============================================
// C++側の GameScript コンポーネントで Script Name を
// 「player」に設定すると、このクラスが実行されます。
//
// 自分自身の座標:
//   Position.X
//   Position.Y
//   Position.Z
//
// 名前から別Entityを探す:
//   var player = FindEntity("Player");
//
//   if (player.HasValue)
//   {
//       ScriptVector3 playerPosition = player.Value.Position;
//   }
//
// 移動:
//   GetMoveVelocity() 内で vx / vz を設定する。
//   XZ方向の移動速度としてC++ / Jolt側へ渡される。
//
// ジャンプ:
//   Update() などで Jump(8.0f) を呼ぶ。
//   Y方向の速度だけを設定する。
//
// 衝突・Trigger:
//   OnTriggerEnter / OnCollisionEnter を override する。
//   other.EntityName で相手の名前を取得できる。
// ==============================================

public class player : Templet
{
    private const float MoveSpeed = 5.0f;
    private const float JumpVelocity = 8.0f;
    private const float EnemyBounceVelocity = 18.0f;

    public override void OnStart()
    {
        // 開始時に一度だけ呼ばれる
    }

    public override void Update()
    {
        // 毎フレーム呼ばれる

        // Space またはゲームパッドの A ボタンでジャンプ
        if (IsJumpTriggered())
        {
            Jump(JumpVelocity);
        }
    }

    public override void GetMoveVelocity(ref float vx, ref float vz, float dt)
    {
        // WASD / 矢印キー / 左スティック / 十字キーで移動
        ScriptVector2 input = GetMoveInput();
        vx = input.X * MoveSpeed;
        vz = input.Z * MoveSpeed;
    }

    public override void OnTriggerEnter(CollisionInfo other)
    {
        if (IsEnemy(other))
        {
            BounceFromEnemy();
        }
    }

    public override void OnTriggerExit(CollisionInfo other)
    {
    }

    public override void OnCollisionEnter(CollisionInfo other)
    {
        if (IsEnemy(other))
        {
            BounceFromEnemy();
        }
    }

    public override void OnCollisionExit(CollisionInfo other)
    {
    }

    // GUIで敵オブジェクトの名前を "Enemy" に設定して使う。
    // 大文字・小文字は区別しないため "enemy" でも判定される。
    private static bool IsEnemy(CollisionInfo other)
    {
        return string.Equals(other.EntityName, "Enemy", StringComparison.OrdinalIgnoreCase);
    }

    private void BounceFromEnemy()
    {
        // Jump は Y 方向の速度だけを指定するため、水平速度はそのまま保たれる。
        Jump(EnemyBounceVelocity);
    }
}
