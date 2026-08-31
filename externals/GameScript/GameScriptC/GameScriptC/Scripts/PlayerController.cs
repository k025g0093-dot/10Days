using System;

// ==============================================
// PlayerController
// ==============================================
// C++側の GameScript コンポーネントで Script Name を
// 「PlayerController」に設定すると、このクラスが実行されます。
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

public class PlayerController : Templet
{
    public override void OnStart()
    {
        // 開始時に一度だけ呼ばれる
    }

    public override void Update()
    {
        // 毎フレーム呼ばれる

        // 例: Spaceを押した瞬間にジャンプ
        if (IsKeyTriger(ConsoleKey.Spacebar))
        {
            Jump(8.0f);
        }
    }

    public override void GetMoveVelocity(ref float vx, ref float vz, float dt)
    {
        // 例: 名前が Player のEntityへ向かって移動する
        var player = FindEntity("player");

        if (!player.HasValue)
        {
            return;
        }

        ScriptVector3 targetPosition = player.Value.Position;

        float dx = targetPosition.X - Position.X;
        float dz = targetPosition.Z - Position.Z;

        float distance = MathF.Sqrt(dx * dx + dz * dz);

        // 近づきすぎたら止まる
        if (distance < 1.0f)
        {
            return;
        }

        const float speed = 4.0f;

        vx = dx / distance * speed;
        vz = dz / distance * speed;
    }

    public override void OnTriggerEnter(CollisionInfo other)
    {
        if (other.EntityName == "Player")
        {
            // PlayerがTriggerに入った瞬間
        }
    }

    public override void OnTriggerExit(CollisionInfo other)
    {
    }

    public override void OnCollisionEnter(CollisionInfo other)
    {
        if (other.EntityName == "Player")
        {
            // Playerと物理衝突した瞬間
        }
    }

    public override void OnCollisionExit(CollisionInfo other)
    {
    }
}