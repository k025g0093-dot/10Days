#include "EntityManager.h"
#include "TUFEngine.h"
#include "MonoBehaviour.h"
#include "FacadeJolt.h"
#include "ScriptRuntime.h"
Entity* EntityManager::CreateEntity(const std::string& name) {
	auto entity = std::make_unique<Entity>();
	entity->name = name;
	entity->displayName = name;
	strncpy_s(entity->displayNameBuf, name.c_str(), sizeof(entity->displayNameBuf));

	Entity* ptr = entity.get();
	m_entities.push_back(std::move(entity));


	return ptr;
}

void EntityManager::DestroyEntity(Entity* entity) {
	auto it = std::find_if(m_entities.begin(), m_entities.end(),
		[entity](const auto& e) { return e.get() == entity; });
	if (it != m_entities.end()) {
		m_entities.erase(it);
	}
}

void EntityManager::UpdateAll(float dt)
{
    if (m_playState != PlayState::Play) {
        return;
    }

    // Start
    for (auto& entity : m_runtimeEntities) {
        for (Component* component : entity->m_components) {
            if (!component) continue;

            if (auto* behaviour = dynamic_cast<MonoBehaviour*>(component)) {
                if (!behaviour->m_isStarted) {
                    behaviour->Start();
                }
            }
        }
    }

    // FixedUpdate と物理計算
    m_accumlator += dt;
    int steps = 0;
    while (m_accumlator >= kFixedTimeStep && steps < kMaxFixedSteps) {
        FacadeJolt::GetInstance()->Step(kFixedTimeStep);
        for (auto& entity : m_runtimeEntities) {
            for (Component* component : entity->m_components) {
                if (!component) continue;
                component->fixedDeltaTime = kFixedTimeStep;
                component->FixedUpdate();
            }
        }
        m_accumlator -= kFixedTimeStep;
        ++steps;
    }
    if (steps >= kMaxFixedSteps) m_accumlator = 0.0f;

    // Update
    for (auto& entity : m_runtimeEntities) {
        for (Component* component : entity->m_components) {
            if (!component) continue;
            component->deltaTime = dt;
            component->Update();
        }
    }
}

void EntityManager::Clear() {
	m_entities.clear();
}

Entity* EntityManager::DuplicateEntity(Entity* src) {
	auto* entity = CreateEntity(src->name + " (コピー)");
	entity->displayName = src->displayName + " (コピー)";
	strncpy_s(entity->displayNameBuf, entity->displayName.c_str(), sizeof(entity->displayNameBuf));
	entity->transform = src->transform;
	entity->obb = src->obb;
    for (auto* c : src->GetComponents()) {
        Component* cloned = c->Clone();
        cloned->entity = entity;
        entity->m_components.push_back(cloned);
    }
	return entity;
}

// EntityManager.cpp
void EntityManager::StartPlay()
{
    if (m_playState != PlayState::Edit) return;

    // 編集中に残っているScript登録を消して、Runtime側だけを動かす。
    ScriptRuntime::GetInstance()->Shutdown();

    // 編集用EntityのBodyを外す。残すとRuntime側のBodyと同じ場所で二重になり、
    // 互いに押し合って動かなくなる。
    {
        auto* jolt = FacadeJolt::GetInstance();
        for (const auto& editorEntity : m_entities) {
            if (editorEntity->m_bodyIdRaw != UINT32_MAX) {
                jolt->RemoveBody(editorEntity->m_bodyIdRaw);
                editorEntity->m_bodyIdRaw = UINT32_MAX;
            }
        }
    }

    m_runtimeEntities.clear();

    for (const auto& editorEntity : m_entities) {
        CloneToRuntime(*editorEntity);
    }

    m_accumlator = 0.0f;
    m_playState = PlayState::Play;
}

Entity* EntityManager::CloneToRuntime(const Entity& source)
{
    auto runtimeEntity = std::make_unique<Entity>();

    runtimeEntity->name = source.name;
    runtimeEntity->displayName = source.displayName;
    runtimeEntity->transform = source.transform;
    runtimeEntity->obb = source.obb;
    runtimeEntity->localAABB = source.localAABB;

    for (Component* component : source.GetComponents()) {
        Component* copy = component->Clone();
        copy->entity = runtimeEntity.get();
        runtimeEntity->m_components.push_back(copy);
    }

    Entity* result = runtimeEntity.get();
    m_runtimeEntities.push_back(std::move(runtimeEntity));
    return result;
}

void EntityManager::StopPlay()
{
    if (m_playState == PlayState::Edit) return;

    // Entityを破棄する前にJoltのBodyを必ず取り除く。
    auto* jolt = FacadeJolt::GetInstance();
    for (const auto& entity : m_runtimeEntities) {
        if (entity->m_bodyIdRaw != UINT32_MAX) {
            jolt->RemoveBody(entity->m_bodyIdRaw);
            entity->m_bodyIdRaw = UINT32_MAX;
        }
    }
    m_runtimeEntities.clear();
    ScriptRuntime::GetInstance()->Shutdown();

    m_accumlator = 0.0f;
    m_playState = PlayState::Edit;
}

void EntityManager::PausePlay()
{
    if (m_playState == PlayState::Play) {
        m_playState = PlayState::Pause;
    }
}

void EntityManager::ResumePlay()
{
    if (m_playState == PlayState::Pause) {
        m_playState = PlayState::Play;
    }
}
