#include "FacadeJolt.h"

#include "Entity.h"
#include "EntityManager.h"
#include "Rigidbody.h"
#include "Collider.h"
#include "BoxCollider.h"
#include "SphereCollider.h"
#include "ConvexHullCollider.h"
#include <algorithm>
#include <cstdio>

FacadeJolt* FacadeJolt::s_instance = nullptr;

FacadeJolt* FacadeJolt::GetInstance() {
	if (!s_instance) s_instance = new FacadeJolt();
	return s_instance;
}

// ════════════════════════════════════════════
//  Init
// ════════════════════════════════════════════
bool FacadeJolt::Init() {
	JPH::RegisterDefaultAllocator();
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	m_tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
	m_jobSystem = new JPH::JobSystemThreadPool(
		JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, -1);

	m_physicsSystem = new JPH::PhysicsSystem();
	m_physicsSystem->Init(
		10240,   // maxBodies
		0,       // numBodyMutexes
		65536,   // maxBodyPairs
		10240,   // maxContactConstraints
		m_bpLayerInterface,
		m_objectVsBpFilter,
		m_objectLayerPairFilter);

	m_physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

	// ───── 動作確認用の床（200×1×200、y = -0.5 の位置）─────
	{
		JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();

		JPH::BoxShapeSettings floorShape(JPH::Vec3(100.0f, 0.5f, 100.0f));
		floorShape.SetEmbedded();
		JPH::ShapeSettings::ShapeResult res = floorShape.Create();

		if (res.IsValid()) {
			JPH::BodyCreationSettings settings(
				res.Get(),
				JPH::RVec3(0.0f, -0.5f, 0.0f),
				JPH::Quat::sIdentity(),
				JPH::EMotionType::Static,
				Layers::NON_MOVING);

			m_floorId = bi.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
		}
	}

	m_physicsSystem->OptimizeBroadPhase();
	m_physicsSystem->SetContactListener(&m_contactListener);
	//OutputDebugStringA("[Jolt] Init OK\n");
	return true;
}

void FacadeJolt::Shutdown() {
	if (m_physicsSystem) { delete m_physicsSystem; m_physicsSystem = nullptr; }
	if (m_jobSystem) { delete m_jobSystem;     m_jobSystem = nullptr; }
	if (m_tempAllocator) { delete m_tempAllocator; m_tempAllocator = nullptr; }
	if (JPH::Factory::sInstance) {
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}
	m_bodyToEntity.clear();
}

// ════════════════════════════════════════════
//  AddBody
// ════════════════════════════════════════════
uint32_t FacadeJolt::AddBody(Entity* e) {
	if (!m_physicsSystem || !e) return UINT32_MAX;

	Collider* col = e->GetComponent<Collider>();
	if (!col) return UINT32_MAX;          // コライダーが無ければ作らない

	const Vector3 s = e->transform.scale;

	// ───── 形状 ─────
	JPH::ShapeRefC shape;

	if (auto* box = dynamic_cast<BoxCollider*>(col)) {
		float hx = (std::max)(box->size.x * 0.5f * s.x, 0.005f);   // クランプを緩める
		float hy = (std::max)(box->size.y * 0.5f * s.y, 0.005f);
		float hz = (std::max)(box->size.z * 0.5f * s.z, 0.005f);

		// 一番薄い辺より凸半径が大きいと形が崩れる
		float minHalf = (std::min)(hx, (std::min)(hy, hz));
		float convexRadius = (std::min)(0.05f, minHalf * 0.5f);

		JPH::BoxShapeSettings bs(JPH::Vec3(hx, hy, hz), convexRadius);
		bs.SetEmbedded();
		auto r = bs.Create();
		if (!r.IsValid()) return UINT32_MAX;
		shape = r.Get();

		// メッシュのAABB中心が原点からズレている場合、その分だけ形状をずらす
		const Vector3 c = box->center;
		if (fabsf(c.x) > 1e-5f || fabsf(c.y) > 1e-5f || fabsf(c.z) > 1e-5f) {
			JPH::RotatedTranslatedShapeSettings off(
				JPH::Vec3(c.x * s.x, c.y * s.y, c.z * s.z),
				JPH::Quat::sIdentity(), shape);
			off.SetEmbedded();
			auto r2 = off.Create();
			if (r2.IsValid()) shape = r2.Get();
		}
	}
	else if (auto* sph = dynamic_cast<SphereCollider*>(col)) {
		float maxScale = (std::max)(s.x, (std::max)(s.y, s.z));
		float radius = (std::max)(sph->radius * maxScale, 0.06f);

		JPH::SphereShapeSettings ss(radius);
		ss.SetEmbedded();
		auto r = ss.Create();
		if (!r.IsValid()) return UINT32_MAX;
		shape = r.Get();
	}
	else {
		// ConvexHullCollider は localAABB のボックスで代用する
		Vector3 half = (e->localAABB.max - e->localAABB.min) * 0.5f;

		// localAABB が未計算だと極小の箱になってすり抜ける。気づけるようにログを出す
		if (half.x <= 1e-5f && half.y <= 1e-5f && half.z <= 1e-5f) {
			char msg[192];
			sprintf_s(msg, "[Jolt] localAABB が空です: %s (コライダーが極小になります)\n",
				e->name.c_str());
			//OutputDebugStringA(msg);
		}

		float hx = (std::max)(half.x * s.x, 0.005f);
		float hy = (std::max)(half.y * s.y, 0.005f);
		float hz = (std::max)(half.z * s.z, 0.005f);

		// 一番薄い辺より凸半径が大きいと形が崩れる
		float minHalf = (std::min)(hx, (std::min)(hy, hz));
		float convexRadius = (std::min)(0.05f, minHalf * 0.5f);

		JPH::BoxShapeSettings bs(JPH::Vec3(hx, hy, hz), convexRadius);
		bs.SetEmbedded();
		auto r = bs.Create();
		if (!r.IsValid()) return UINT32_MAX;
		shape = r.Get();
	}

	// ───── モーションタイプ ─────
	Rigidbody* rb = e->GetComponent<Rigidbody>();

	JPH::EMotionType motion = JPH::EMotionType::Static;
	JPH::ObjectLayer layer = Layers::NON_MOVING;

	if (rb) {
		motion = rb->isKinematic ? JPH::EMotionType::Kinematic
			: JPH::EMotionType::Dynamic;
		layer = Layers::MOVING;
	}

	// ───── 生成 ─────
	const Vector3& p = e->transform.position;
	const Vector3& r = e->transform.rotation;

	JPH::BodyCreationSettings settings(
		shape,
		JPH::RVec3(p.x, p.y, p.z),
		JPH::Quat::sEulerAngles(JPH::Vec3(r.x, r.y, r.z)),
		motion,
		layer);

	if (rb) {
		settings.mLinearDamping = rb->linearDrag;
		settings.mAngularDamping = rb->angularDrag;
		settings.mGravityFactor = rb->useGravity ? 1.0f : 0.0f;

		if (!rb->isKinematic) {
			settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
		}

		settings.mOverrideMassProperties =
			JPH::EOverrideMassProperties::CalculateInertia;
		settings.mMassPropertiesOverride.mMass = (std::max)(rb->mass, 0.001f);
	}

	if (col->isTrigger) {
		settings.mIsSensor = true;
	}

	JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();
	JPH::BodyID id = bi.CreateAndAddBody(
		settings,
		(motion == JPH::EMotionType::Static) ? JPH::EActivation::DontActivate
		: JPH::EActivation::Activate);

	uint32_t raw = id.GetIndexAndSequenceNumber();
	e->m_bodyIdRaw = raw;
	m_bodyToEntity[raw] = e;

	return raw;
}

void FacadeJolt::RemoveBody(uint32_t idRaw) {
	if (!m_physicsSystem || idRaw == UINT32_MAX) return;

	JPH::BodyID id(idRaw);
	JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();
	bi.RemoveBody(id);
	bi.DestroyBody(id);

	m_bodyToEntity.erase(idRaw);
}

Entity* FacadeJolt::FindEntity(uint32_t idRaw) {
	auto it = m_bodyToEntity.find(idRaw);
	return (it != m_bodyToEntity.end()) ? it->second : nullptr;
}

// ════════════════════════════════════════════
//  毎フレーム
// ════════════════════════════════════════════
void FacadeJolt::SyncNewBodies() {
    for (auto& e : EntityManager::GetInstance()->GetActiveEntities()) {
		Entity* ent = e.get();
		if (ent->m_bodyIdRaw != UINT32_MAX) continue;   // もうある
		if (!ent->GetComponent<Collider>())   continue; // コライダー無し
		AddBody(ent);
	}
}

void FacadeJolt::PushTransforms() {
	JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();

    for (auto& e : EntityManager::GetInstance()->GetActiveEntities()) {
		Entity* ent = e.get();
		if (ent->m_bodyIdRaw == UINT32_MAX) continue;

		Rigidbody* rb = ent->GetComponent<Rigidbody>();
		// Dynamic は物理が主。Static / Kinematic だけ transform を反映する
		if (rb && !rb->isKinematic) continue;

		JPH::BodyID id(ent->m_bodyIdRaw);
		const Vector3& p = ent->transform.position;
		const Vector3& r = ent->transform.rotation;

		bi.SetPositionAndRotation(
			id,
			JPH::RVec3(p.x, p.y, p.z),
			JPH::Quat::sEulerAngles(JPH::Vec3(r.x, r.y, r.z)),
			JPH::EActivation::DontActivate);
	}
}

void FacadeJolt::PullTransforms() {
	JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();

    for (auto& e : EntityManager::GetInstance()->GetActiveEntities()) {
		Entity* ent = e.get();
		if (ent->m_bodyIdRaw == UINT32_MAX) continue;

		Rigidbody* rb = ent->GetComponent<Rigidbody>();
		if (!rb || rb->isKinematic) continue;   // Dynamic だけ書き戻す

		JPH::BodyID id(ent->m_bodyIdRaw);

		JPH::RVec3 p = bi.GetPosition(id);
		ent->transform.position = { (float)p.GetX(), (float)p.GetY(), (float)p.GetZ() };

		JPH::Vec3 euler = bi.GetRotation(id).GetEulerAngles();
		ent->transform.rotation = { euler.GetX(), euler.GetY(), euler.GetZ() };
	}
}

void FacadeJolt::Step(float dt) {
	if (!m_physicsSystem) return;

	SyncNewBodies();
	PushTransforms();

	m_physicsSystem->Update(dt, 1, m_tempAllocator, m_jobSystem);

	PullTransforms();
	DispatchEvents();
}

void FacadeJolt::RebuildBody(Entity* e) {
	if (!e) return;
	if (e->m_bodyIdRaw != UINT32_MAX) {
		RemoveBody(e->m_bodyIdRaw);
		e->m_bodyIdRaw = UINT32_MAX;
	}
	AddBody(e);
}

Vector3 FacadeJolt::GetLinearVelocity(uint32_t idRaw) {
	if (!m_physicsSystem || idRaw == UINT32_MAX) return { 0, 0, 0 };
	JPH::Vec3 v = m_physicsSystem->GetBodyInterface().GetLinearVelocity(JPH::BodyID(idRaw));
	return { v.GetX(), v.GetY(), v.GetZ() };
}

bool FacadeJolt::IsBodyActive(uint32_t idRaw) {
	if (!m_physicsSystem || idRaw == UINT32_MAX) return false;
	return m_physicsSystem->GetBodyInterface().IsActive(JPH::BodyID(idRaw));
}

void FacadeJolt::WakeBody(uint32_t idRaw) {
	if (!m_physicsSystem || idRaw == UINT32_MAX) return;
	m_physicsSystem->GetBodyInterface().ActivateBody(JPH::BodyID(idRaw));
}

void FacadeJolt::SetLinearVelocity(uint32_t idRaw, const Vector3& velocity) {
	if (!m_physicsSystem || idRaw == UINT32_MAX) return;

	m_physicsSystem->GetBodyInterface().SetLinearVelocity(
		JPH::BodyID(idRaw),
		JPH::Vec3(velocity.x, velocity.y, velocity.z));
}

void FacadeJolt::ContactListener::OnContactAdded(
	const JPH::Body& body1,
	const JPH::Body& body2,
	const JPH::ContactManifold&,
	JPH::ContactSettings&
) {

	Entity* a = m_owner.FindEntity(
		body1.GetID().GetIndexAndSequenceNumber());

	Entity* b = m_owner.FindEntity(
		body2.GetID().GetIndexAndSequenceNumber());

	if (!a || !b)return;
	
	Collider* colliderA = a->GetComponent<Collider>();
	Collider* colliderB = b->GetComponent<Collider>();

	const bool IsTrigger =
		(colliderA && colliderA->isTrigger) ||
		(colliderB && colliderB->isTrigger);

	const auto eventType = IsTrigger
		? PhysicsEventType::TriggerEnter
		: PhysicsEventType::CollisionEnter;

	// 両者に通知するイベントを積む
	std::lock_guard<std::mutex> lock(m_owner.m_eventMutex);
	m_owner.m_events.push_back({ eventType, a, b });
	m_owner.m_events.push_back({ eventType, b, a });;

}

void FacadeJolt::DispatchEvents()
{
	// ロック中は取り出すだけ。処理はロックの外でやる
	std::vector<PhysicsEvent> events;
	{
		std::lock_guard<std::mutex> lock(m_eventMutex);
		events.swap(m_events);
	}

	for (const PhysicsEvent& event : events) {
		if (!event.self || !event.other) continue;

		for (Component* component : event.self->GetComponents()) {
			switch (event.type) {
			case PhysicsEventType::TriggerEnter:
				component->OnTriggerEnter(event.other);
				break;

			case PhysicsEventType::CollisionEnter:
				component->OnCollisionEnter(event.other);
				break;
			}
		}
	}
}
