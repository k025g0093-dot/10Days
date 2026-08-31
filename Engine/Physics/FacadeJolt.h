#pragma once

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max

#include "JoltAllinclude.h"

#pragma pop_macro("min")
#pragma pop_macro("max")

#include <unordered_map>
#include <cstdint>
#include <mutex>     
#include "allVector.h"

class Entity;

// ───── レイヤー定義 ─────
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
}

enum class PhysicsEventType {
    TriggerEnter,
    TriggerExit,
    CollisionEnter,
    CollisionExit,
};

struct PhysicsEvent {
    PhysicsEventType type;
    Entity* self;
    Entity* other;
};

class FacadeJolt
{
public:
    static FacadeJolt* GetInstance();

    bool Init();
    void Shutdown();

    // 毎フレーム（固定ステップから呼ぶ）
    void Step(float dt);

    // Collider / Rigidbody からボディを作る。戻り値は BodyID の生値
    uint32_t AddBody(Entity* e);
    void     RemoveBody(uint32_t idRaw);

    // BodyID から Entity を引く
    Entity* FindEntity(uint32_t idRaw);

    JPH::PhysicsSystem* GetSystem() { return m_physicsSystem; }

    // ★ ここから追加
    void    RebuildBody(Entity* e);              // 設定を変えた後に作り直す
    Vector3 GetLinearVelocity(uint32_t idRaw);   // 現在の速度（表示用）
    bool    IsBodyActive(uint32_t idRaw);        // 起きてる / 寝てる
    void    WakeBody(uint32_t idRaw);            // 叩き起こす
    void SetLinearVelocity(uint32_t idRaw, const Vector3& velocity);

    std::vector<PhysicsEvent> m_events;
    std::mutex                m_eventMutex;

    class ContactListener : public JPH::ContactListener {
    public:
        explicit ContactListener(FacadeJolt& owner) : m_owner(owner) {}

        void OnContactAdded(
            const JPH::Body& body1,
            const JPH::Body& body2,
            const JPH::ContactManifold&,
            JPH::ContactSettings&) override;

        //void OnContactRemoved(
        //    const JPH::SubShapeIDPair& pair) override;

    private:
        FacadeJolt& m_owner;
    };

    ContactListener m_contactListener{ *this };

    void DispatchEvents();

private:
    FacadeJolt() = default;
    static FacadeJolt* s_instance;

    void SyncNewBodies();   // Collider を持つがボディが無い Entity にボディを作る
    void PushTransforms();  // Static/Kinematic の transform をボディへ
    void PullTransforms();  // Dynamic のボディ位置を transform へ

    // ───── フィルター ─────
    class BPLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface {
    public:
        BPLayerInterfaceImpl() {
            m_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            m_objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }
        JPH::uint GetNumBroadPhaseLayers() const override {
            return BroadPhaseLayers::NUM_LAYERS;
        }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
            return m_objectToBroadPhase[inLayer];
        }
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override {
            return "BPLayer";
        }
    private:
        JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
    };

    class ObjectVsBPLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
            switch (inLayer1) {
            case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:     return true;
            default:                 return false;
            }
        }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer o1, JPH::ObjectLayer o2) const override {
            switch (o1) {
            case Layers::NON_MOVING: return o2 == Layers::MOVING;   // 静止物同士は判定しない
            case Layers::MOVING:     return true;
            default:                 return false;
            }
        }
    };

    // ───── メンバー ─────
    BPLayerInterfaceImpl        m_bpLayerInterface;
    ObjectVsBPLayerFilterImpl   m_objectVsBpFilter;
    ObjectLayerPairFilterImpl   m_objectLayerPairFilter;

    JPH::TempAllocatorImpl* m_tempAllocator = nullptr;
    JPH::JobSystemThreadPool* m_jobSystem = nullptr;
    JPH::PhysicsSystem* m_physicsSystem = nullptr;

    JPH::BodyID                 m_floorId;

    std::unordered_map<uint32_t, Entity*> m_bodyToEntity;
};