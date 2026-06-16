#include "WhipPch.h"
#include <Whip/Animation/AnimationManager.h>
#include <Whip/Core/Application.h>

_WHIP_START

static UniqueNameManager s_NameManager;

void AnimationManager::Update(Timestep ts)
{
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

		if (anim->m_ElapsedTime >= anim->m_Frames[anim->m_CurrentFrame].m_Duration)
		{
			anim->m_CurrentFrame++;

			if (anim->m_CurrentFrame >= anim->m_Frames.size())
			{
				anim->m_ElapsedTime = 0.0f;
				if (anim->IsLooping())
					anim->m_CurrentFrame = 0;
				else
				{
					anim->Stop();
					continue;
				}
			}

			anim->ApplyFrame(anim->m_Frames[anim->m_CurrentFrame].m_Texture);
		}
	}
}

void AnimationManager::AddAnimation(Ref<Animation2D> animation)
{
	m_Animations.push_back(animation);
}

void AnimationManager::RemoveAnimation(Ref<Animation2D> animation)
{
	m_Animations.erase(std::remove(m_Animations.begin(), m_Animations.end(), animation), m_Animations.end());
}

void AnimationManager::Clear()
{
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
