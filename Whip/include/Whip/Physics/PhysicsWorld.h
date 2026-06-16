#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Timestep.h>

#include <entt.hpp>

class b2World;

_WHIP_START

class Scene;

class PhysicsWorld
{
public:
	PhysicsWorld() {}
	PhysicsWorld(Scene* sceneContext) : m_SceneContext(sceneContext) {}
	~PhysicsWorld();

	void SetSceneContext(Scene* sceneContext);

	void Create(float gravityX = 0.0f, float gravityY = 9.8f);
	void Update(Timestep ts);
	void Destroy();
private:
	bool PrivateCheck(bool checkWorld) const;

	b2World* m_PhysicsWorld = nullptr;
	Scene* m_SceneContext = nullptr;
};

_WHIP_END
