#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Timestep.h>

#include <box2d/box2d.h>

_WHIP_START

class Scene;

class PhysicsWorld // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	PhysicsWorld();
	PhysicsWorld(Scene* sceneContext);
	~PhysicsWorld();

	void SetSceneContext(Scene* sceneContext);

	void Create(float gravityX = 0.0f, float gravityY = 9.8f);
	void Update(Timestep ts) const;
	void Destroy();
private:
	bool Verify(bool checkWorld) const;
	void ResetRuntimeHandles() const;
	void SyncMovedBodyTransforms() const;

	b2WorldId m_PhysicsWorld = b2_nullWorldId;
	Scene* m_SceneContext = nullptr;
};

_WHIP_END
