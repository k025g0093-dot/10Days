using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

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

            using var server = new NamedPipeServerStream(
                PipeName,
                PipeDirection.InOut,
                1,
                PipeTransmissionMode.Byte);

            Console.WriteLine("接続待機中...");
            server.WaitForConnection();
            Console.WriteLine("接続されました");

            try
            {
                Loop(server);
            }
            catch (EndOfStreamException)
            {
                Console.WriteLine("エンジンが切断しました");
            }
            catch (Exception ex)
            {
                Console.WriteLine("エラー: " + ex);
            }
        }

        static void Loop(NamedPipeServerStream pipe)
        {
            while (true)
            {
                int len = BitConverter.ToInt32(ReadExactly(pipe, 4), 0);
                byte[] buf = ReadExactly(pipe, len);
                int o = 0;

                int msgType = ReadI32(buf, ref o);
                if (msgType != 1)
                {
                    continue;
                }

                // ── spawn ──
                int spawnCount = ReadI32(buf, ref o);

                for (int i = 0; i < spawnCount; i++)
                {
                    int id = ReadI32(buf, ref o);
                    string name = ReadStr(buf, ref o);

                    Type? type =
                        Assembly.GetExecutingAssembly().GetType(name) ??
                        Assembly.GetExecutingAssembly()
                            .GetTypes()
                            .FirstOrDefault(t => t.Name == name);

                    if (type == null)
                    {
                        Console.WriteLine($"[警告] クラス '{name}' が見つかりません");
                        continue;
                    }

                    if (!typeof(Templet).IsAssignableFrom(type))
                    {
                        Console.WriteLine($"[警告] クラス '{name}' は Templet を継承していません");
                        continue;
                    }

                    var script = (Templet)Activator.CreateInstance(type)!;
                    script.OnStart();

                    s_instances[id] = script;
                    Console.WriteLine($"spawn id={id} {name}");
                }

                // ── destroy ──
                int destroyCount = ReadI32(buf, ref o);

                for (int i = 0; i < destroyCount; i++)
                {
                    int id = ReadI32(buf, ref o);
                    s_instances.Remove(id);
                    Console.WriteLine($"destroy id={id}");
                }

                // ── World Entity一覧 ──
                // C++側の送信順:
                // spawn → destroy → Entity一覧 → tick → physics events
                int entityCount = ReadI32(buf, ref o);

                World.BeginFrame();

                for (int i = 0; i < entityCount; i++)
                {
                    string name = ReadStr(buf, ref o);
                    float x = ReadF32(buf, ref o);
                    float y = ReadF32(buf, ref o);
                    float z = ReadF32(buf, ref o);

                    World.UpdateEntity(name, x, y, z);
                }

                // ── tick ──
                int tickCount = ReadI32(buf, ref o);

                var commands = new List<byte>();
                int commandCount = 0;

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

                    if (!s_instances.TryGetValue(id, out var script))
                    {
                        continue;
                    }

                    // このスクリプト自身の現在座標
                    script.Position = new ScriptVector3(px, py, pz);

                    script.Update();

                    float desiredVx = 0.0f;
                    float desiredVz = 0.0f;

                    script.GetMoveVelocity(ref desiredVx, ref desiredVz, dt);

                    // mode 1 = SetVelocity
                    commands.AddRange(BitConverter.GetBytes(id));
                    commands.AddRange(BitConverter.GetBytes(1));
                    commands.AddRange(BitConverter.GetBytes(desiredVx));
                    commands.AddRange(BitConverter.GetBytes(0.0f));
                    commands.AddRange(BitConverter.GetBytes(desiredVz));
                    commandCount++;
                }

                // ── physics events ──
                int eventCount = ReadI32(buf, ref o);

                for (int i = 0; i < eventCount; i++)
                {
                    int targetScriptInstanceId = ReadI32(buf, ref o);
                    PhysicsEventType eventType = (PhysicsEventType)ReadI32(buf, ref o);
                    int otherEntityId = ReadI32(buf, ref o);
                    string otherEntityName = ReadStr(buf, ref o);

                    if (!s_instances.TryGetValue(targetScriptInstanceId, out var script))
                    {
                        continue;
                    }

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
                    }
                }

                // ── Jump要求 ──
                foreach (var pair in s_instances)
                {
                    int id = pair.Key;
                    Templet script = pair.Value;

                    if (!script.HasPendingJump)
                    {
                        continue;
                    }

                    float jumpVelocityY = script.ConsumeJump();

                    // mode 2 = Jump
                    commands.AddRange(BitConverter.GetBytes(id));
                    commands.AddRange(BitConverter.GetBytes(2));
                    commands.AddRange(BitConverter.GetBytes(0.0f));
                    commands.AddRange(BitConverter.GetBytes(jumpVelocityY));
                    commands.AddRange(BitConverter.GetBytes(0.0f));
                    commandCount++;
                }

                // ── C++へ返信 ──
                var outBuf = new List<byte>();

                outBuf.AddRange(BitConverter.GetBytes(commandCount));
                outBuf.AddRange(commands);

                pipe.Write(BitConverter.GetBytes(outBuf.Count), 0, 4);
                pipe.Write(outBuf.ToArray(), 0, outBuf.Count);
                pipe.Flush();
            }
        }

        static int ReadI32(byte[] buffer, ref int offset)
        {
            int value = BitConverter.ToInt32(buffer, offset);
            offset += 4;
            return value;
        }

        static float ReadF32(byte[] buffer, ref int offset)
        {
            float value = BitConverter.ToSingle(buffer, offset);
            offset += 4;
            return value;
        }

        static string ReadStr(byte[] buffer, ref int offset)
        {
            int byteCount = ReadI32(buffer, ref offset);
            string value = Encoding.UTF8.GetString(buffer, offset, byteCount);
            offset += byteCount;
            return value.TrimEnd('\0');
        }

        static byte[] ReadExactly(NamedPipeServerStream pipe, int count)
        {
            byte[] buffer = new byte[count];
            int read = 0;

            while (read < count)
            {
                int received = pipe.Read(buffer, read, count - read);

                if (received <= 0)
                {
                    throw new EndOfStreamException("Pipe closed");
                }

                read += received;
            }

            return buffer;
        }
    }
}

public static class KeyboardHelper
{
    [DllImport("user32.dll")]
    static extern short GetAsyncKeyState(int vKey);

    public static bool IsKeyDown(ConsoleKey key)
    {
        return (GetAsyncKeyState((int)key) & 0x8000) != 0;
    }

    public static bool IsKeyPressed(ConsoleKey key)
    {
        return (GetAsyncKeyState((int)key) & 0x0001) != 0;
    }
}