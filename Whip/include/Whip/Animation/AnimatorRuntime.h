#pragma once

#include "Whip/Animation/AnimationController.h"
#include "Whip/Core/Core.h"
#include "Whip/Core/Memory.h"
#include "Whip/Core/Timestep.h"
#include "Whip/Core/UUID.h"

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

	bool IsPlaying() const;
	AssetHandle GetControllerHandle() const;
	const std::string& GetCurrentStateName() const;
	const std::string& GetLastTransitionSourceName() const;
	const std::string& GetLastTransitionTargetName() const;
	float GetTransitionDebugTime() const;
	bool IsTransitioning() const;
	const std::string& GetTransitionTargetStateName() const;
	float GetTransitionProgress() const;
	float GetStateTime() const;
	const std::vector<std::string>& GetFiredEvents() const;
	void ClearFiredEvents();

	const std::unordered_map<std::string, bool>& GetBoolParameters() const;
	const std::unordered_map<std::string, int32_t>& GetIntParameters() const;
	const std::unordered_map<std::string, float>& GetFloatParameters() const;
	const std::unordered_set<std::string>& GetTriggerParameters() const;

private:
	const AnimationControllerState* GetCurrentState() const;
	const AnimationControllerParameter* FindParameter(std::string_view name) const;
	Ref<Animation2D> GetStateClip(const AnimationControllerState& state) const;
	AssetHandle ResolveStateClipHandle(const AnimationControllerState& state) const;
	float ResolveStateMotionSpeed(const AnimationControllerState& state) const;
	float GetFloatParameterValue(std::string_view name) const;
	float GetStateDuration(const AnimationControllerState& state) const;

	bool TryTransition(const AnimationControllerState& state, float stateDuration);
	bool TryTransitionList(std::string_view sourceName, const std::vector<AnimationControllerTransition>& transitions, float stateDuration, bool useExitTime);
	bool ApplyTransition(std::string_view sourceName, const AnimationControllerTransition& transition);
	bool IsExitTimeReady(const AnimationControllerTransition& transition, float stateDuration) const;
	bool ConditionsPass(const AnimationControllerTransition& transition) const;
	bool ConditionPasses(const AnimationControllerCondition& condition) const;
	bool BlueprintConditionsPass(const AnimationControllerTransition& transition) const;
	bool BlueprintExecPasses(const AnimationControllerTransition& transition) const;
	bool ExecuteBlueprintNode(const AnimationControllerTransition& transition, uint32_t nodeId, std::unordered_set<uint32_t>& visiting) const;
	bool ExecuteBlueprintOutput(const AnimationControllerTransition& transition, uint32_t nodeId, uint32_t outputPin, std::unordered_set<uint32_t>& visiting) const;
	bool EvaluateBlueprintNodeBool(const AnimationControllerTransition& transition, uint32_t nodeId, uint32_t outputPin, std::unordered_set<uint32_t>& visiting) const;
	float EvaluateBlueprintNodeNumber(const AnimationControllerTransition& transition, uint32_t nodeId, uint32_t outputPin, std::unordered_set<uint32_t>& visiting) const;
	bool EvaluateBlueprintInputBool(const AnimationControllerTransition& transition, const AnimationControllerBlueprintNode& node, uint32_t inputPin, bool fallback, std::unordered_set<uint32_t>& visiting) const;
	float EvaluateBlueprintInputNumber(const AnimationControllerTransition& transition, const AnimationControllerBlueprintNode& node, uint32_t inputPin, float fallback, std::unordered_set<uint32_t>& visiting) const;
	bool GetBlueprintParameterBool(std::string_view name) const;
	float GetBlueprintParameterNumber(std::string_view name) const;
	void ConsumeTransitionTriggers(const AnimationControllerTransition& transition);

	void SwitchState(std::string_view stateName);
	float NormalizeStateTime(const AnimationControllerState& state, float stateTime) const;
	void ApplyCurrentFrame();
	void ApplyStateFrame(const AnimationControllerState& state, float stateTime);
	void ApplyBlendedFrame(const AnimationControllerState& sourceState, float sourceTime, const AnimationControllerState& targetState, float targetTime, float factor);
	void QueueEvents(const AnimationControllerState& state, float startTime, float endTime);
	void ApplyPropertyTracks(const Animation2D& clip, float sampleTime);
	void WarnMissingStateClip(const AnimationControllerState& state);

	Scene* m_Scene = nullptr;
	UUID m_EntityId = 0;
	Ref<AnimationController> m_Controller = nullptr;

	std::string m_CurrentStateName;
	std::string m_LastTransitionSourceName;
	std::string m_LastTransitionTargetName;
	std::string m_TransitionTargetStateName;
	float m_StateTime = 0.0f;
	float m_TransitionDebugTime = 999.0f;
	float m_TransitionElapsed = 0.0f;
	float m_TransitionDuration = 0.0f;
	float m_TransitionTargetStateTime = 0.0f;
	bool m_Transitioning = false;
	bool m_Playing = false;
	std::vector<std::string> m_FiredEvents;

	std::unordered_map<std::string, bool> m_BoolParameters;
	std::unordered_map<std::string, int32_t> m_IntParameters;
	std::unordered_map<std::string, float> m_FloatParameters;
	std::unordered_set<std::string> m_TriggerParameters;
	std::unordered_set<std::string> m_MissingClipWarnings;
};

_WHIP_END
