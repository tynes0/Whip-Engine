#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Core/Memory.h"
#include "Whip/Helper/UniqueNameManager.h"

#include "Animation2D.h"

#include <vector>

_WHIP_START

class AnimationManager
{
public:
	void Update(Timestep ts);
	void AddAnimation(const Ref<Animation2D>& animation);
	void RemoveAnimation(const Ref<Animation2D>& animation);

	void Clear();
	bool Empty() const;

	bool HasBeenTickedThisFrame() const;
	std::vector<Ref<Animation2D>>& GetAnimations() { return m_Animations; }

	static AnimationManager& Get() { static AnimationManager instance; return instance; }
	static UniqueNameManager& GetAnimationNameManager();
private:
	std::vector<Ref<Animation2D>> m_Animations;
	uint64_t m_LastTickCount = 0;
};

_WHIP_END
