#pragma once

#include <Whip/Animation/AnimationController.h>
#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <Whip/Core/Timestep.h>
#include <Whip/Core/UUID.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

_WHIP_START

class Animation2D;
class Scene;

class AnimatorRuntime
{
public:
	void Bind(Scene* scene, UUID entityId, const Ref<AnimationController>& controller, std::string_view initialState = {});

	void Play(std::string_view stateName = {});
	void Stop();
	void Update(Timestep ts, float speed = 1.0f);

	void SetBool(std::string_view name, bool value);
	void SetInt(std::string_view name, int32_t value);
	void SetFloat(std::string_view name, float value);
	void SetTrigger(std::string_view name);
	void ResetTrigger(std::string_view name);

	bool IsPlaying() const { return m_Playing; }
	AssetHandle GetControllerHandle() const { return m_Controller ? m_Controller->m_Handle : AssetHandle{}; }
	const std::string& GetCurrentStateName() const { return m_CurrentStateName; }
	float GetStateTime() const { return m_StateTime; }
	const std::vector<std::string>& GetFiredEvents() const { return m_FiredEvents; }
	void ClearFiredEvents() { m_FiredEvents.clear(); }

	const std::unordered_map<std::string, bool>& GetBoolParameters() const { return m_BoolParameters; }
	const std::unordered_map<std::string, int32_t>& GetIntParameters() const { return m_IntParameters; }
	const std::unordered_map<std::string, float>& GetFloatParameters() const { return m_FloatParameters; }
	const std::unordered_set<std::string>& GetTriggerParameters() const { return m_TriggerParameters; }

private:
	const AnimationControllerState* GetCurrentState() const;
	const AnimationControllerParameter* FindParameter(std::string_view name) const;
	Ref<Animation2D> GetStateClip(const AnimationControllerState& state) const;
	AssetHandle ResolveStateClipHandle(const AnimationControllerState& state) const;
	float ResolveStateMotionSpeed(const AnimationControllerState& state) const;
	float GetFloatParameterValue(std::string_view name) const;
	float GetStateDuration(const AnimationControllerState& state) const;

	bool TryTransition(const AnimationControllerState& state, float stateDuration);
	bool IsExitTimeReady(const AnimationControllerTransition& transition, float stateDuration) const;
	bool ConditionsPass(const AnimationControllerTransition& transition) const;
	bool ConditionPasses(const AnimationControllerCondition& condition) const;
	void ConsumeTransitionTriggers(const AnimationControllerTransition& transition);

	void SwitchState(std::string_view stateName);
	void ApplyCurrentFrame();
	void QueueEvents(const AnimationControllerState& state, float startTime, float endTime);
	void ApplyPropertyTracks(const Animation2D& clip, float sampleTime);

	Scene* m_Scene = nullptr;
	UUID m_EntityId = 0;
	Ref<AnimationController> m_Controller = nullptr;

	std::string m_CurrentStateName;
	float m_StateTime = 0.0f;
	bool m_Playing = false;
	std::vector<std::string> m_FiredEvents;

	std::unordered_map<std::string, bool> m_BoolParameters;
	std::unordered_map<std::string, int32_t> m_IntParameters;
	std::unordered_map<std::string, float> m_FloatParameters;
	std::unordered_set<std::string> m_TriggerParameters;
};

_WHIP_END
