#pragma once
#include <vector>
#include <memory>
#include "Entity.h"



class EntityManager {
public:

    enum class PlayState {
        Edit,
        Play,
        Pause,
    };

    using EntityList = std::vector<std::unique_ptr<Entity>>;
    PlayState GetPlayState() const { return m_playState; }
    void SetPlayState(PlayState state) { m_playState = state; }

    // Play制御
    void StartPlay();
    void PausePlay();
    void ResumePlay();
    void StopPlay();

    // GUIが参照する。Play中でも編集Sceneを表示する。
    const EntityList& GetEditorEntities() const {
        return m_entities;
    }

    // 物理・C#・ゲーム描画が参照する。
    const EntityList& GetRuntimeEntities() const {
        return m_runtimeEntities;
    }

    // 通常の描画・更新用
    const EntityList& GetActiveEntities() const {
        return m_playState == PlayState::Edit
            ? m_entities
            : m_runtimeEntities;
    }

    static EntityManager* GetInstance() {
        static EntityManager instance;
        return &instance;
    }

    Entity* CreateEntity(const std::string& name);
    void DestroyEntity(Entity* entity);
    void UpdateAll(float dt);
    void Clear();
    Entity* DuplicateEntity(Entity* src);
    const std::vector<std::unique_ptr<Entity>>& GetEntities() const { return m_entities; }
    bool isSelected;

    static constexpr float kFixedTimeStep = 1.0f / 60.0f;
    static constexpr int   kMaxFixedSteps = 5;

private:
    EntityManager() = default;
    ~EntityManager() = default;
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;

    EntityList m_entities;
    float m_accumlator = 0.0f;


    Entity* CloneToRuntime(const Entity& source);

    EntityList m_runtimeEntities;
    PlayState m_playState = PlayState::Edit;

};

