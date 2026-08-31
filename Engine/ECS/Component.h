#pragma once
//コンポーネントの基底クラス
//ここが基盤でここから別別に派生していく


class Entity;
class Component
{
public:

    Entity* entity = nullptr;   // ★ 初期化子を付ける
    float   deltaTime = 0.0f;      // ★
    float   fixedDeltaTime = 0.0f;      // ★

    virtual ~Component() = default;
    virtual void Start() {}
    virtual void Update() {}
    virtual void FixedUpdate() {}
    virtual Component* Clone() const = 0;

    virtual void OnTriggerEnter(Entity* other) {}
    virtual void OnTriggerExit(Entity* other) {}

    virtual void OnCollisionEnter(Entity* other) {}
    virtual void OnCollisionExit(Entity* other) {}

};


