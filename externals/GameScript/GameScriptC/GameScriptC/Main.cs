using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Linq;

namespace GameScriptC
{



    public class MyScript
    {
        [DllImport("kernel32.dll")]
        static extern bool AllocConsole();

        const string PipeName = "GameScriptPipe_Engine";

        static readonly Dictionary<int, Templet> s_instances = new();

        public static void Main(string[] args)
        {
            AllocConsole();
            Console.WriteLine("=== GameScript Runtime ===");

            var server = new NamedPipeServerStream(PipeName, PipeDirection.InOut);
            Console.WriteLine("接続待機中...");
            server.WaitForConnection();
            Console.WriteLine("接続されました");

            try { Loop(server); }
            catch (EndOfStreamException) { Console.WriteLine("エンジンが切断しました"); }
            catch (Exception ex) { Console.WriteLine("エラー: " + ex); }
        }

        static void Loop(NamedPipeServerStream pipe)
        {
            while (true)
            {
                int len = BitConverter.ToInt32(ReadExactly(pipe, 4), 0);
                byte[] buf = ReadExactly(pipe, len);
                int o = 0;

                int msgType = ReadI32(buf, ref o);
                if (msgType != 1) continue;

                // ── spawn ──
                int spawnCount = ReadI32(buf, ref o);
                for (int i = 0; i < spawnCount; i++)
                {
                    int id = ReadI32(buf, ref o);
                    int nameLen = ReadI32(buf, ref o);
                    string name = Encoding.UTF8.GetString(buf, o, nameLen).TrimEnd('\0');
                    o += nameLen;

                    Type? t = Assembly.GetExecutingAssembly().GetType(name)
                                ?? Assembly.GetExecutingAssembly()
                                .GetTypes()
                                .FirstOrDefault(type => type.Name == name);

                    if (t == null)
                    {
                        Console.WriteLine($"[警告] クラス '{name}' が見つかりません");
                        continue;
                    }
                    var inst = (Templet)Activator.CreateInstance(t);
                    inst.OnStart();
                    s_instances[id] = inst;
                    Console.WriteLine($"spawn  id={id}  {name}");
                }

                // ── destroy ──
                int destroyCount = ReadI32(buf, ref o);
                for (int i = 0; i < destroyCount; i++)
                {
                    int id = ReadI32(buf, ref o);
                    s_instances.Remove(id);
                    Console.WriteLine($"destroy id={id}");
                }

                // ── tick ──
                int tickCount = ReadI32(buf, ref o);
                var outBuf = new List<byte>();
                var cmds = new List<byte>();
                int cmdCount = 0;

                for (int i = 0; i < tickCount; i++)
                {
                    int id = ReadI32(buf, ref o);
                    float px = ReadF32(buf, ref o);
                    float py = ReadF32(buf, ref o);
                    float pz = ReadF32(buf, ref o);
                    float vx = ReadF32(buf, ref o);
                    float vy = ReadF32(buf, ref o);
                    float vz = ReadF32(buf, ref o);
                    float dt = ReadF32(buf, ref o);
                    int flags = ReadI32(buf, ref o);

                    if (!s_instances.TryGetValue(id, out var script)) continue;

                    float x = px, y = py, z = pz;
                    script.Update();
                    //script.InPostion(ref x, ref y, ref z, dt);

                    float desiredVx = 0.0f;
                    float desiredVz = 0.0f;
                    script.GetMoveVelocity(ref desiredVx, ref desiredVz, dt);

                    // mode 0 = SetPosition
                    // mode 1 = SetVercity
                    cmds.AddRange(BitConverter.GetBytes(id));
                    cmds.AddRange(BitConverter.GetBytes(1));
                    cmds.AddRange(BitConverter.GetBytes(desiredVx));
                    cmds.AddRange(BitConverter.GetBytes(0.0f));
                    cmds.AddRange(BitConverter.GetBytes(desiredVz));
                    cmdCount++;
                }

                // ── physics events ──
                // C++: targetScriptInstanceId / eventType / otherEntityId / otherEntityName
                int eventCount = ReadI32(buf, ref o);
                if (eventCount > 0)
                    Console.WriteLine($"[C#] physics events received: {eventCount}");
                for (int i = 0; i < eventCount; i++)
                {
                    int targetScriptInstanceId = ReadI32(buf, ref o);
                    PhysicsEventType eventType = (PhysicsEventType)ReadI32(buf, ref o);
                    int otherEntityId = ReadI32(buf, ref o);
                    string otherEntityName = ReadStr(buf, ref o);

                    if (!s_instances.TryGetValue(targetScriptInstanceId, out var script))
                        continue;

                    var other = new CollisionInfo(otherEntityId, otherEntityName);
                    switch (eventType)
                    {
                    case PhysicsEventType.TriggerEnter:
                        script.OnTriggerEnter(other);
                        break;
                    case PhysicsEventType.TriggerExit:
                        script.OnTriggerExit(other);
                        break;
                    case PhysicsEventType.CollisionEnter:
                        script.OnCollisionEnter(other);
                        break;
                    case PhysicsEventType.CollisionExit:
                        script.OnCollisionExit(other);
                        break;
                    default:
                        Console.WriteLine($"[警告] 未知のPhysicsEventType: {(int)eventType}");
                        break;
                    }
                }

                // ── ジャンプ要求を mode 2 コマンドとして送る ──
                // 接触イベントの中で Jump() が呼ばれていたら、同じフレームで C++ に届く。
                foreach (var kv in s_instances)
                {
                    if (!kv.Value.HasPendingJump) continue;
                    float jumpVy = kv.Value.ConsumeJump();

                    cmds.AddRange(BitConverter.GetBytes(kv.Key));
                    cmds.AddRange(BitConverter.GetBytes(2));       // mode 2 = Jump(Yだけ上書き)
                    cmds.AddRange(BitConverter.GetBytes(0.0f));
                    cmds.AddRange(BitConverter.GetBytes(jumpVy));
                    cmds.AddRange(BitConverter.GetBytes(0.0f));
                    cmdCount++;
                    Console.WriteLine($"[C#] jump command  id={kv.Key}  vy={jumpVy}");
                }

                // ── 返信 ──
                outBuf.AddRange(BitConverter.GetBytes(cmdCount));
                outBuf.AddRange(cmds);

                pipe.Write(BitConverter.GetBytes(outBuf.Count), 0, 4);
                pipe.Write(outBuf.ToArray(), 0, outBuf.Count);
                pipe.Flush();
            }
        }

        static int ReadI32(byte[] b, ref int o) { int v = BitConverter.ToInt32(b, o); o += 4; return v; }
        static float ReadF32(byte[] b, ref int o) { float v = BitConverter.ToSingle(b, o); o += 4; return v; }
        static string ReadStr(byte[] b, ref int o)
        {
            int byteCount = ReadI32(b, ref o);
            string value = Encoding.UTF8.GetString(b, o, byteCount).TrimEnd('\0');
            o += byteCount;
            return value;
        }

        static byte[] ReadExactly(NamedPipeServerStream pipe, int count)
        {
            byte[] buf = new byte[count];
            int read = 0;
            while (read < count)
            {
                int n = pipe.Read(buf, read, count - read);
                if (n <= 0) throw new EndOfStreamException("Pipe closed");
                read += n;
            }
            return buf;
        }
    }
}

public static class KeyboardHelper
{
    [DllImport("user32.dll")]
    static extern short GetAsyncKeyState(int vKey);

    public static bool IsKeyDown(ConsoleKey key)
        => (GetAsyncKeyState((int)key) & 0x8000) != 0;

    public static bool IsKeyPressed(ConsoleKey key)
        => (GetAsyncKeyState((int)key) & 0x0001) != 0;
}
