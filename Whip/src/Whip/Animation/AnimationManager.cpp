#include "WhipPch.h"
#include "Whip/Animation/AnimationManager.h"
#include "Whip/Core/Application.h"

_WHIP_START

namespace
{
	UniqueNameManager s_NameManager;
}

void AnimationManager::Update(Timestep ts)
{
	WHP_PROFILE_FUNCTION();
	if (HasBeenTickedThisFrame())
		return;

	m_LastTickCount = Application::Get().GetTickCount();

	for (auto& anim : m_Animations)
	{
		if (!anim || !anim->IsPlaying() || anim->IsPaused())
			continue;

		if (anim->m_Frames.empty())
		{
			anim->Stop();
			continue;
		}

		anim->m_ElapsedTime += ts.GetSeconds();

		static constexpr float MinFrameDuration = 1.0f / 240.0f;
		while (anim->m_IsPlaying && anim->m_ElapsedTime >= std::max(anim->m_Frames[anim->m_CurrentFrame].m_Duration, MinFrameDuration))
		{
			anim->m_ElapsedTime -= std::max(anim->m_Frames[anim->m_CurrentFrame].m_Duration, MinFrameDuration);
			anim->m_CurrentFrame++;

			if (anim->m_CurrentFrame >= anim->m_Frames.size())
			{
				if (anim->IsLooping())
					anim->m_CurrentFrame = 0;
				else
				{
					anim->Stop();
					continue;
				}
			}

			anim->ApplyFrame(anim->m_Frames[anim->m_CurrentFrame]);
		}
	}
}

void AnimationManager::AddAnimation(const Ref<Animation2D>& animation)
{
	WHP_PROFILE_FUNCTION();
	m_Animations.push_back(animation);
}

void AnimationManager::RemoveAnimation(const Ref<Animation2D>& animation)
{
	WHP_PROFILE_FUNCTION();
	std::erase(m_Animations, animation);
}

void AnimationManager::Clear()
{
	WHP_PROFILE_FUNCTION();
	m_Animations.clear();
}

bool AnimationManager::Empty() const
{
	return m_Animations.empty();
}

bool AnimationManager::HasBeenTickedThisFrame() const
{
	return m_LastTickCount == Application::Get().GetTickCount();
}

UniqueNameManager& AnimationManager::GetAnimationNameManager()
{
	return s_NameManager;
}

_WHIP_END
