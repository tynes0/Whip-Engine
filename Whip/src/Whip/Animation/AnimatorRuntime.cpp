#include "WhipPch.h"
#include "Whip/Animation/AnimatorRuntime.h"
#include "Whip/Animation/Animation2D.h"
#include "Whip/Asset/AssetManager.h"
#include "Whip/Scene/Components.h"
#include "Whip/Scene/Entity.h"
#include "Whip/Scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>

_WHIP_START

namespace
{
	bool NearlyEqual(float left, float right)
	{
		return std::abs(left - right) <= 0.0001f;
	}

	const AnimationControllerBlueprintNode* FindBlueprintNode(const AnimationControllerTransition& transition, uint32_t nodeId)
	{
		const auto it = std::find_if(transition.m_BlueprintNodes.begin(), transition.m_BlueprintNodes.end(), [nodeId](const AnimationControllerBlueprintNode& node)
			{
				return node.m_Id == nodeId;
			});
		return it == transition.m_BlueprintNodes.end() ? nullptr : &*it;
	}

	const AnimationControllerBlueprintLink* FindBlueprintInputLink(const AnimationControllerTransition& transition, uint32_t nodeId, uint32_t inputPin)
	{
		const auto it = std::find_if(transition.m_BlueprintLinks.begin(), transition.m_BlueprintLinks.end(), [nodeId, inputPin](const AnimationControllerBlueprintLink& link)
			{
				return link.m_InputNode == nodeId && link.m_InputPin == inputPin;
			});
		return it == transition.m_BlueprintLinks.end() ? nullptr : &*it;
	}

	glm::vec3 Lerp(const glm::vec3& left, const glm::vec3& right, float factor)
	{
		return left + (right - left) * factor;
	}

	glm::vec4 Lerp(const glm::vec4& left, const glm::vec4& right, float factor)
	{
		return left + (right - left) * factor;
	}

	bool SampleTrack(const std::vector<AnimationVec3Key>& keys, float time, glm::vec3& value)
	{
		if (keys.empty())
			return false;

		const AnimationVec3Key* previous = nullptr;
		const AnimationVec3Key* next = nullptr;
		for (const AnimationVec3Key& key : keys)
		{
			if (key.m_Time <= time && (!previous || key.m_Time > previous->m_Time))
				previous = &key;
			if (key.m_Time >= time && (!next || key.m_Time < next->m_Time))
				next = &key;
		}

		if (!previous)
		{
			value = next->m_Value;
			return true;
		}
		if (!next)
		{
			value = previous->m_Value;
			return true;
		}

		const float range = std::max(next->m_Time - previous->m_Time, 0.0001f);
		value = Lerp(previous->m_Value, next->m_Value, std::clamp((time - previous->m_Time) / range, 0.0f, 1.0f));
		return true;
	}

	bool SampleTrack(const std::vector<AnimationVec4Key>& keys, float time, glm::vec4& value)
	{
		if (keys.empty())
			return false;

		const AnimationVec4Key* previous = nullptr;
		const AnimationVec4Key* next = nullptr;
		for (const AnimationVec4Key& key : keys)
		{
			if (key.m_Time <= time && (!previous || key.m_Time > previous->m_Time))
				previous = &key;
			if (key.m_Time >= time && (!next || key.m_Time < next->m_Time))
				next = &key;
		}

		if (!previous)
		{
			value = next->m_Value;
			return true;
		}
		if (!next)
		{
			value = previous->m_Value;
			return true;
		}

		const float range = std::max(next->m_Time - previous->m_Time, 0.0001f);
		value = Lerp(previous->m_Value, next->m_Value, std::clamp((time - previous->m_Time) / range, 0.0f, 1.0f));
		return true;
	}
}

void AnimatorRuntime::Bind(Scene* scene, UUID entityId, const Ref<AnimationController>& controller, std::string_view initialState)
{
	m_Scene = scene;
	m_EntityId = entityId;
	m_Controller = controller;
	m_CurrentStateName.clear();
	m_LastTransitionSourceName.clear();
	m_LastTransitionTargetName.clear();
	m_TransitionTargetStateName.clear();
	m_StateTime = 0.0f;
	m_TransitionDebugTime = 999.0f;
	m_TransitionElapsed = 0.0f;
	m_TransitionDuration = 0.0f;
	m_TransitionTargetStateTime = 0.0f;
	m_Transitioning = false;
	m_Playing = false;
	m_FiredEvents.clear();
	m_BoolParameters.clear();
	m_IntParameters.clear();
	m_FloatParameters.clear();
	m_TriggerParameters.clear();
	m_MissingClipWarnings.clear();

	if (!m_Controller)
		return;

	for (const AnimationControllerParameter& parameter : m_Controller->GetParameters())
	{
		switch (parameter.m_Type)
		{
		case AnimationParameterType::Bool:
			m_BoolParameters[parameter.m_Name] = parameter.m_DefaultBool;
			break;
		case AnimationParameterType::Int:
			m_IntParameters[parameter.m_Name] = parameter.m_DefaultInt;
			break;
		case AnimationParameterType::Float:
			m_FloatParameters[parameter.m_Name] = parameter.m_DefaultFloat;
			break;
		case AnimationParameterType::Trigger:
			break;
		}
	}

	const std::string requestedState(initialState.empty() ? m_Controller->GetDefaultState() : initialState);
	if (m_Controller->FindState(requestedState))
		m_CurrentStateName = requestedState;
	else if (!m_Controller->GetDefaultState().empty() && m_Controller->FindState(m_Controller->GetDefaultState()))
		m_CurrentStateName = m_Controller->GetDefaultState();
	else if (!m_Controller->GetStates().empty())
		m_CurrentStateName = m_Controller->GetStates().front().m_Name;
}

void AnimatorRuntime::Play(std::string_view stateName)
{
	if (!m_Controller)
		return;

	if (!stateName.empty() && m_Controller->FindState(stateName))
		m_CurrentStateName = std::string(stateName);
	else if (m_CurrentStateName.empty())
	{
		const std::string& defaultState = m_Controller->GetDefaultState();
		if (!defaultState.empty() && m_Controller->FindState(defaultState))
			m_CurrentStateName = defaultState;
		else if (!m_Controller->GetStates().empty())
			m_CurrentStateName = m_Controller->GetStates().front().m_Name;
	}

	m_StateTime = 0.0f;
	m_TransitionDebugTime = 999.0f;
	m_TransitionElapsed = 0.0f;
	m_TransitionDuration = 0.0f;
	m_TransitionTargetStateTime = 0.0f;
	m_TransitionTargetStateName.clear();
	m_Transitioning = false;
	m_Playing = !m_CurrentStateName.empty();
	ApplyCurrentFrame();
}

void AnimatorRuntime::Stop()
{
	m_Playing = false;
	m_StateTime = 0.0f;
	m_TransitionElapsed = 0.0f;
	m_TransitionDuration = 0.0f;
	m_TransitionTargetStateTime = 0.0f;
	m_TransitionTargetStateName.clear();
	m_Transitioning = false;
}

void AnimatorRuntime::Update(Timestep ts, float speed)
{
	if (!m_Playing || !m_Controller)
		return;

	const AnimationControllerState* state = GetCurrentState();
	if (!state)
	{
		Play();
		return;
	}

	const float stateSpeed = std::max(state->m_Speed, 0.0f) * ResolveStateMotionSpeed(*state);
	const float animatorSpeed = std::max(speed, 0.0f);
	const float previousTime = m_StateTime;
	const float deltaTime = std::max(static_cast<float>(ts), 0.0f);
	m_TransitionDebugTime += deltaTime;

	if (m_Transitioning)
	{
		const AnimationControllerState* targetState = m_Controller->FindState(m_TransitionTargetStateName);
		if (!targetState)
		{
			m_Transitioning = false;
			ApplyCurrentFrame();
			return;
		}

		m_StateTime += deltaTime * stateSpeed * animatorSpeed;
		const float targetSpeed = std::max(targetState->m_Speed, 0.0f) * ResolveStateMotionSpeed(*targetState);
		m_TransitionTargetStateTime += deltaTime * targetSpeed * animatorSpeed;
		m_TransitionElapsed += deltaTime * animatorSpeed;

		if (m_TransitionDuration <= 0.0f || m_TransitionElapsed >= m_TransitionDuration)
		{
			m_CurrentStateName = m_TransitionTargetStateName;
			m_StateTime = NormalizeStateTime(*targetState, m_TransitionTargetStateTime);
			m_TransitionTargetStateName.clear();
			m_TransitionElapsed = 0.0f;
			m_TransitionDuration = 0.0f;
			m_TransitionTargetStateTime = 0.0f;
			m_Transitioning = false;
			ApplyCurrentFrame();
			return;
		}

		m_StateTime = NormalizeStateTime(*state, m_StateTime);
		m_TransitionTargetStateTime = NormalizeStateTime(*targetState, m_TransitionTargetStateTime);
		ApplyCurrentFrame();
		return;
	}

	m_StateTime += deltaTime * stateSpeed * animatorSpeed;

	const float stateDuration = GetStateDuration(*state);
	if (TryTransitionList("Any State", m_Controller->GetAnyStateTransitions(), stateDuration, false))
	{
		ApplyCurrentFrame();
		return;
	}

	if (TryTransition(*state, stateDuration))
	{
		ApplyCurrentFrame();
		return;
	}

	if (stateDuration > 0.0f)
	{
		if (state->m_Loop)
		{
			if (m_StateTime >= stateDuration)
			{
				QueueEvents(*state, previousTime, stateDuration);
				const float wrappedTime = std::fmod(m_StateTime, stateDuration);
				QueueEvents(*state, 0.0f, wrappedTime);
				m_StateTime = wrappedTime;
			}
			else
			{
				QueueEvents(*state, previousTime, m_StateTime);
			}
		}
		else
		{
			const float clampedTime = std::min(m_StateTime, stateDuration);
			QueueEvents(*state, previousTime, clampedTime);
			m_StateTime = clampedTime;
		}
	}
	else
	{
		QueueEvents(*state, previousTime, m_StateTime);
	}

	ApplyCurrentFrame();
}

void AnimatorRuntime::SetBool(std::string_view name, bool value)
{
	m_BoolParameters[std::string(name)] = value;
}

void AnimatorRuntime::SetInt(std::string_view name, int32_t value)
{
	m_IntParameters[std::string(name)] = value;
}

void AnimatorRuntime::SetFloat(std::string_view name, float value)
{
	m_FloatParameters[std::string(name)] = value;
}

void AnimatorRuntime::SetTrigger(std::string_view name)
{
	m_TriggerParameters.insert(std::string(name));
}

void AnimatorRuntime::ResetTrigger(std::string_view name)
{
	m_TriggerParameters.erase(std::string(name));
}

bool AnimatorRuntime::IsPlaying() const
{
	return m_Playing;
}

AssetHandle AnimatorRuntime::GetControllerHandle() const
{
	return m_Controller ? m_Controller->m_Handle : AssetHandle{};
}

const std::string& AnimatorRuntime::GetCurrentStateName() const
{
	return m_CurrentStateName;
}

const std::string& AnimatorRuntime::GetLastTransitionSourceName() const
{
	return m_LastTransitionSourceName;
}

const std::string& AnimatorRuntime::GetLastTransitionTargetName() const
{
	return m_LastTransitionTargetName;
}

float AnimatorRuntime::GetTransitionDebugTime() const
{
	return m_TransitionDebugTime;
}

bool AnimatorRuntime::IsTransitioning() const
{
	return m_Transitioning;
}

const std::string& AnimatorRuntime::GetTransitionTargetStateName() const
{
	return m_TransitionTargetStateName;
}

float AnimatorRuntime::GetStateTime() const
{
	return m_StateTime;
}

const std::vector<std::string>& AnimatorRuntime::GetFiredEvents() const
{
	return m_FiredEvents;
}

void AnimatorRuntime::ClearFiredEvents()
{
	m_FiredEvents.clear();
}

const std::unordered_map<std::string, bool>& AnimatorRuntime::GetBoolParameters() const
{
	return m_BoolParameters;
}

const std::unordered_map<std::string, int32_t>& AnimatorRuntime::GetIntParameters() const
{
	return m_IntParameters;
}

const std::unordered_map<std::string, float>& AnimatorRuntime::GetFloatParameters() const
{
	return m_FloatParameters;
}

const std::unordered_set<std::string>& AnimatorRuntime::GetTriggerParameters() const
{
	return m_TriggerParameters;
}

const AnimationControllerState* AnimatorRuntime::GetCurrentState() const
{
	return m_Controller ? m_Controller->FindState(m_CurrentStateName) : nullptr;
}

float AnimatorRuntime::GetTransitionProgress() const
{
	if (!m_Transitioning || m_TransitionDuration <= 0.0f)
		return 0.0f;
	return std::clamp(m_TransitionElapsed / m_TransitionDuration, 0.0f, 1.0f);
}

const AnimationControllerParameter* AnimatorRuntime::FindParameter(std::string_view name) const
{
	if (!m_Controller)
		return nullptr;

	for (const AnimationControllerParameter& parameter : m_Controller->GetParameters())
	{
		if (parameter.m_Name == name)
			return &parameter;
	}
	return nullptr;
}

Ref<Animation2D> AnimatorRuntime::GetStateClip(const AnimationControllerState& state) const
{
	const AssetHandle clipHandle = ResolveStateClipHandle(state);
	if (clipHandle == 0 || !AssetManager::IsAssetHandleValid(clipHandle) || AssetManager::GetAssetType(clipHandle) != AssetType::Animation)
		return nullptr;
	return AssetManager::GetAsset<Animation2D>(clipHandle);
}

AssetHandle AnimatorRuntime::ResolveStateClipHandle(const AnimationControllerState& state) const
{
	if (state.m_MotionType == AnimationMotionType::Clip || state.m_BlendChildren.empty())
		return state.m_Clip;

	const float parameterValue = GetFloatParameterValue(state.m_BlendParameter);
	const AnimationBlendChild* bestChild = nullptr;
	float bestDistance = std::numeric_limits<float>::max();
	for (const AnimationBlendChild& child : state.m_BlendChildren)
	{
		const float distance = std::abs(parameterValue - child.m_Threshold);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			bestChild = &child;
		}
	}

	return bestChild ? bestChild->m_Clip : state.m_Clip;
}

float AnimatorRuntime::ResolveStateMotionSpeed(const AnimationControllerState& state) const
{
	if (state.m_MotionType == AnimationMotionType::Clip || state.m_BlendChildren.empty())
		return 1.0f;

	const float parameterValue = GetFloatParameterValue(state.m_BlendParameter);
	const AnimationBlendChild* bestChild = nullptr;
	float bestDistance = std::numeric_limits<float>::max();
	for (const AnimationBlendChild& child : state.m_BlendChildren)
	{
		const float distance = std::abs(parameterValue - child.m_Threshold);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			bestChild = &child;
		}
	}

	return bestChild ? std::max(bestChild->m_Speed, 0.0f) : 1.0f;
}

float AnimatorRuntime::GetFloatParameterValue(std::string_view name) const
{
	const std::string key(name);
	if (const auto floatIt = m_FloatParameters.find(key); floatIt != m_FloatParameters.end())
		return floatIt->second;
	if (const auto intIt = m_IntParameters.find(key); intIt != m_IntParameters.end())
		return static_cast<float>(intIt->second);
	if (const auto boolIt = m_BoolParameters.find(key); boolIt != m_BoolParameters.end())
		return boolIt->second ? 1.0f : 0.0f;
	return 0.0f;
}

float AnimatorRuntime::GetStateDuration(const AnimationControllerState& state) const
{
	Ref<Animation2D> clip = GetStateClip(state);
	return clip ? clip->GetDuration() : 0.0f;
}

bool AnimatorRuntime::TryTransition(const AnimationControllerState& state, float stateDuration)
{
	return TryTransitionList(state.m_Name, state.m_Transitions, stateDuration, true);
}

bool AnimatorRuntime::TryTransitionList(std::string_view sourceName, const std::vector<AnimationControllerTransition>& transitions, float stateDuration, bool useExitTime)
{
	for (const AnimationControllerTransition& transition : transitions)
	{
		if (transition.m_TargetState.empty())
			continue;

		if (transition.m_TargetState != AnimationController::ExitStateName && !m_Controller->FindState(transition.m_TargetState))
			continue;

		if (transition.m_TargetState == m_CurrentStateName)
			continue;

		if ((useExitTime && !IsExitTimeReady(transition, stateDuration)) || !ConditionsPass(transition))
			continue;

		return ApplyTransition(sourceName, transition);
	}

	return false;
}

bool AnimatorRuntime::ApplyTransition(std::string_view sourceName, const AnimationControllerTransition& transition)
{
	ConsumeTransitionTriggers(transition);
	m_LastTransitionSourceName = std::string(sourceName);
	m_LastTransitionTargetName = transition.m_TargetState;
	m_TransitionDebugTime = 0.0f;

	if (transition.m_TargetState == AnimationController::ExitStateName)
	{
		Stop();
		return true;
	}

	const float duration = std::max(transition.m_Duration, 0.0f);
	if (duration > 0.0f)
	{
		m_Transitioning = true;
		m_TransitionTargetStateName = transition.m_TargetState;
		m_TransitionElapsed = 0.0f;
		m_TransitionDuration = duration;
		m_TransitionTargetStateTime = 0.0f;
		return true;
	}

	SwitchState(transition.m_TargetState);
	return true;
}

bool AnimatorRuntime::IsExitTimeReady(const AnimationControllerTransition& transition, float stateDuration) const
{
	if (!transition.m_HasExitTime)
		return true;
	if (stateDuration <= 0.0f)
		return true;

	const float normalizedTime = m_StateTime / stateDuration;
	return normalizedTime >= transition.m_ExitTime;
}

bool AnimatorRuntime::ConditionsPass(const AnimationControllerTransition& transition) const
{
	if (!transition.m_BlueprintNodes.empty())
		return BlueprintConditionsPass(transition);

	for (const AnimationControllerCondition& condition : transition.m_Conditions)
	{
		if (!ConditionPasses(condition))
			return false;
	}
	return true;
}

bool AnimatorRuntime::ConditionPasses(const AnimationControllerCondition& condition) const
{
	const AnimationControllerParameter* parameter = FindParameter(condition.m_Parameter);
	if (!parameter)
		return false;

	switch (parameter->m_Type)
	{
	case AnimationParameterType::Bool:
	{
		const auto it = m_BoolParameters.find(parameter->m_Name);
		const bool value = it != m_BoolParameters.end() ? it->second : parameter->m_DefaultBool;
		switch (condition.m_Mode)
		{
		case AnimationConditionMode::If: return value;
		case AnimationConditionMode::IfNot: return !value;
		case AnimationConditionMode::Equals: return value == condition.m_BoolValue;
		case AnimationConditionMode::NotEquals: return value != condition.m_BoolValue;
		case AnimationConditionMode::Greater:
		case AnimationConditionMode::Less:
			break;
		}
		return false;
	}
	case AnimationParameterType::Int:
	{
		const auto it = m_IntParameters.find(parameter->m_Name);
		const int32_t value = it != m_IntParameters.end() ? it->second : parameter->m_DefaultInt;
		switch (condition.m_Mode)
		{
		case AnimationConditionMode::Greater: return value > condition.m_IntValue;
		case AnimationConditionMode::Less: return value < condition.m_IntValue;
		case AnimationConditionMode::Equals: return value == condition.m_IntValue;
		case AnimationConditionMode::NotEquals: return value != condition.m_IntValue;
		case AnimationConditionMode::If:
		case AnimationConditionMode::IfNot:
			break;
		}
		return false;
	}
	case AnimationParameterType::Float:
	{
		const auto it = m_FloatParameters.find(parameter->m_Name);
		const float value = it != m_FloatParameters.end() ? it->second : parameter->m_DefaultFloat;
		switch (condition.m_Mode)
		{
		case AnimationConditionMode::Greater: return value > condition.m_Threshold;
		case AnimationConditionMode::Less: return value < condition.m_Threshold;
		case AnimationConditionMode::Equals: return NearlyEqual(value, condition.m_Threshold);
		case AnimationConditionMode::NotEquals: return !NearlyEqual(value, condition.m_Threshold);
		case AnimationConditionMode::If:
		case AnimationConditionMode::IfNot:
			break;
		}
		return false;
	}
	case AnimationParameterType::Trigger:
	{
		const bool value = m_TriggerParameters.contains(parameter->m_Name);
		switch (condition.m_Mode)
		{
		case AnimationConditionMode::If: return value;
		case AnimationConditionMode::IfNot: return !value;
		case AnimationConditionMode::Equals: return value == condition.m_BoolValue;
		case AnimationConditionMode::NotEquals: return value != condition.m_BoolValue;
		case AnimationConditionMode::Greater:
		case AnimationConditionMode::Less:
			break;
		}
		return false;
	}
	}

	return false;
}

bool AnimatorRuntime::BlueprintConditionsPass(const AnimationControllerTransition& transition) const
{
	const bool hasExecLinks = std::any_of(transition.m_BlueprintLinks.begin(), transition.m_BlueprintLinks.end(), [](const AnimationControllerBlueprintLink& link)
		{
			return IsAnimationBlueprintExecPin(link.m_OutputPin) || IsAnimationBlueprintExecPin(link.m_InputPin);
		});
	if (hasExecLinks)
		return BlueprintExecPasses(transition);

	const auto resultIt = std::find_if(transition.m_BlueprintNodes.begin(), transition.m_BlueprintNodes.end(), [](const AnimationControllerBlueprintNode& node)
		{
			return node.m_Type == AnimationBlueprintNodeType::Result;
		});

	if (resultIt == transition.m_BlueprintNodes.end())
		return transition.m_Conditions.empty() ? true : std::all_of(transition.m_Conditions.begin(), transition.m_Conditions.end(), [this](const AnimationControllerCondition& condition)
			{
				return ConditionPasses(condition);
			});

	for (const AnimationControllerBlueprintLink& link : transition.m_BlueprintLinks)
	{
		if (link.m_InputNode != resultIt->m_Id || link.m_InputPin != 0)
			continue;

		std::unordered_set<uint32_t> visiting;
		if (!EvaluateBlueprintNodeBool(transition, link.m_OutputNode, link.m_OutputPin, visiting))
			return false;
	}

	return true;
}

bool AnimatorRuntime::BlueprintExecPasses(const AnimationControllerTransition& transition) const
{
	const auto startIt = std::find_if(transition.m_BlueprintNodes.begin(), transition.m_BlueprintNodes.end(), [](const AnimationControllerBlueprintNode& node)
		{
			return node.m_Type == AnimationBlueprintNodeType::Start;
		});
	if (startIt == transition.m_BlueprintNodes.end())
		return false;

	std::unordered_set<uint32_t> visiting;
	return ExecuteBlueprintNode(transition, startIt->m_Id, visiting);
}

bool AnimatorRuntime::ExecuteBlueprintNode(const AnimationControllerTransition& transition, uint32_t nodeId, std::unordered_set<uint32_t>& visiting) const
{
	const AnimationControllerBlueprintNode* node = FindBlueprintNode(transition, nodeId);
	if (!node)
		return false;
	if (!visiting.insert(nodeId).second)
		return false;

	bool result = false;
	switch (node->m_Type)
	{
	case AnimationBlueprintNodeType::Start:
		result = ExecuteBlueprintOutput(transition, node->m_Id, AnimationBlueprintThenPin, visiting);
		break;
	case AnimationBlueprintNodeType::Reroute:
		result = ExecuteBlueprintOutput(transition, node->m_Id, AnimationBlueprintThenPin, visiting);
		break;
	case AnimationBlueprintNodeType::If:
	case AnimationBlueprintNodeType::IfNot:
	case AnimationBlueprintNodeType::Greater:
	case AnimationBlueprintNodeType::Less:
	case AnimationBlueprintNodeType::Equals:
	case AnimationBlueprintNodeType::NotEquals:
	{
		std::unordered_set<uint32_t> valueVisiting;
		const bool pass = EvaluateBlueprintNodeBool(transition, node->m_Id, 0, valueVisiting);
		result = ExecuteBlueprintOutput(transition, node->m_Id, pass ? AnimationBlueprintTruePin : AnimationBlueprintFalsePin, visiting);
		break;
	}
	case AnimationBlueprintNodeType::Result:
	{
		std::unordered_set<uint32_t> valueVisiting;
		result = EvaluateBlueprintInputBool(transition, *node, 0, true, valueVisiting);
		break;
	}
	default:
		result = false;
		break;
	}

	visiting.erase(nodeId);
	return result;
}

bool AnimatorRuntime::ExecuteBlueprintOutput(const AnimationControllerTransition& transition, uint32_t nodeId, uint32_t outputPin, std::unordered_set<uint32_t>& visiting) const
{
	bool hasOutgoing = false;
	for (const AnimationControllerBlueprintLink& link : transition.m_BlueprintLinks)
	{
		if (link.m_OutputNode != nodeId || link.m_OutputPin != outputPin || !IsAnimationBlueprintExecPin(link.m_InputPin))
			continue;

		hasOutgoing = true;
		if (ExecuteBlueprintNode(transition, link.m_InputNode, visiting))
			return true;
	}
	return hasOutgoing ? false : false;
}

bool AnimatorRuntime::EvaluateBlueprintNodeBool(const AnimationControllerTransition& transition, uint32_t nodeId, uint32_t outputPin, std::unordered_set<uint32_t>& visiting) const
{
	const AnimationControllerBlueprintNode* node = FindBlueprintNode(transition, nodeId);
	if (!node)
		return false;

	if (!visiting.insert(nodeId).second)
		return false;

	bool value = false;
	switch (node->m_Type)
	{
	case AnimationBlueprintNodeType::Start:
		value = true;
		break;
	case AnimationBlueprintNodeType::Parameter:
		value = GetBlueprintParameterBool(node->m_Parameter);
		break;
	case AnimationBlueprintNodeType::If:
		value = EvaluateBlueprintInputBool(transition, *node, 0, node->m_InputBoolValues[0], visiting);
		break;
	case AnimationBlueprintNodeType::IfNot:
		value = !EvaluateBlueprintInputBool(transition, *node, 0, node->m_InputBoolValues[0], visiting);
		break;
	case AnimationBlueprintNodeType::Greater:
		if (const AnimationControllerParameter* parameter = FindParameter(node->m_Parameter); parameter && parameter->m_Type == AnimationParameterType::Int)
			value = EvaluateBlueprintInputNumber(transition, *node, 0, (float)node->m_InputIntValues[0], visiting) > EvaluateBlueprintInputNumber(transition, *node, 1, (float)node->m_InputIntValues[1], visiting);
		else
			value = EvaluateBlueprintInputNumber(transition, *node, 0, node->m_InputFloatValues[0], visiting) > EvaluateBlueprintInputNumber(transition, *node, 1, node->m_InputFloatValues[1], visiting);
		break;
	case AnimationBlueprintNodeType::Less:
		if (const AnimationControllerParameter* parameter = FindParameter(node->m_Parameter); parameter && parameter->m_Type == AnimationParameterType::Int)
			value = EvaluateBlueprintInputNumber(transition, *node, 0, (float)node->m_InputIntValues[0], visiting) < EvaluateBlueprintInputNumber(transition, *node, 1, (float)node->m_InputIntValues[1], visiting);
		else
			value = EvaluateBlueprintInputNumber(transition, *node, 0, node->m_InputFloatValues[0], visiting) < EvaluateBlueprintInputNumber(transition, *node, 1, node->m_InputFloatValues[1], visiting);
		break;
	case AnimationBlueprintNodeType::Equals:
	{
		const AnimationControllerParameter* parameter = FindParameter(node->m_Parameter);
		if (parameter && (parameter->m_Type == AnimationParameterType::Bool || parameter->m_Type == AnimationParameterType::Trigger))
			value = EvaluateBlueprintInputBool(transition, *node, 0, node->m_InputBoolValues[0], visiting) == EvaluateBlueprintInputBool(transition, *node, 1, node->m_InputBoolValues[1], visiting);
		else if (parameter && parameter->m_Type == AnimationParameterType::Int)
			value = (int32_t)EvaluateBlueprintInputNumber(transition, *node, 0, (float)node->m_InputIntValues[0], visiting) == (int32_t)EvaluateBlueprintInputNumber(transition, *node, 1, (float)node->m_InputIntValues[1], visiting);
		else
			value = NearlyEqual(EvaluateBlueprintInputNumber(transition, *node, 0, node->m_InputFloatValues[0], visiting), EvaluateBlueprintInputNumber(transition, *node, 1, node->m_InputFloatValues[1], visiting));
		break;
	}
	case AnimationBlueprintNodeType::NotEquals:
	{
		const AnimationControllerParameter* parameter = FindParameter(node->m_Parameter);
		if (parameter && (parameter->m_Type == AnimationParameterType::Bool || parameter->m_Type == AnimationParameterType::Trigger))
			value = EvaluateBlueprintInputBool(transition, *node, 0, node->m_InputBoolValues[0], visiting) != EvaluateBlueprintInputBool(transition, *node, 1, node->m_InputBoolValues[1], visiting);
		else if (parameter && parameter->m_Type == AnimationParameterType::Int)
			value = (int32_t)EvaluateBlueprintInputNumber(transition, *node, 0, (float)node->m_InputIntValues[0], visiting) != (int32_t)EvaluateBlueprintInputNumber(transition, *node, 1, (float)node->m_InputIntValues[1], visiting);
		else
			value = !NearlyEqual(EvaluateBlueprintInputNumber(transition, *node, 0, node->m_InputFloatValues[0], visiting), EvaluateBlueprintInputNumber(transition, *node, 1, node->m_InputFloatValues[1], visiting));
		break;
	}
	case AnimationBlueprintNodeType::Not:
		value = !EvaluateBlueprintInputBool(transition, *node, 0, false, visiting);
		break;
	case AnimationBlueprintNodeType::And:
		value = EvaluateBlueprintInputBool(transition, *node, 0, false, visiting) && EvaluateBlueprintInputBool(transition, *node, 1, true, visiting);
		break;
	case AnimationBlueprintNodeType::Or:
		value = EvaluateBlueprintInputBool(transition, *node, 0, false, visiting) || EvaluateBlueprintInputBool(transition, *node, 1, false, visiting);
		break;
	case AnimationBlueprintNodeType::Reroute:
		value = EvaluateBlueprintInputBool(transition, *node, 0, false, visiting);
		break;
	case AnimationBlueprintNodeType::Result:
		value = EvaluateBlueprintInputBool(transition, *node, 0, true, visiting);
		break;
	}

	visiting.erase(nodeId);
	if (outputPin == 1)
	{
		switch (node->m_Type)
		{
		case AnimationBlueprintNodeType::If:
		case AnimationBlueprintNodeType::IfNot:
		case AnimationBlueprintNodeType::Greater:
		case AnimationBlueprintNodeType::Less:
		case AnimationBlueprintNodeType::Equals:
		case AnimationBlueprintNodeType::NotEquals:
			return !value;
		default:
			return false;
		}
	}
	return value;
}

float AnimatorRuntime::EvaluateBlueprintNodeNumber(const AnimationControllerTransition& transition, uint32_t nodeId, uint32_t outputPin, std::unordered_set<uint32_t>& visiting) const
{
	const AnimationControllerBlueprintNode* node = FindBlueprintNode(transition, nodeId);
	if (!node)
		return 0.0f;
	if (node->m_Type == AnimationBlueprintNodeType::Parameter)
		return GetBlueprintParameterNumber(node->m_Parameter);
	if (node->m_Type == AnimationBlueprintNodeType::Reroute)
		return EvaluateBlueprintInputNumber(transition, *node, 0, 0.0f, visiting);
	return EvaluateBlueprintNodeBool(transition, nodeId, outputPin, visiting) ? 1.0f : 0.0f;
}

bool AnimatorRuntime::EvaluateBlueprintInputBool(const AnimationControllerTransition& transition, const AnimationControllerBlueprintNode& node, uint32_t inputPin, bool fallback, std::unordered_set<uint32_t>& visiting) const
{
	if (const AnimationControllerBlueprintLink* link = FindBlueprintInputLink(transition, node.m_Id, inputPin))
		return EvaluateBlueprintNodeBool(transition, link->m_OutputNode, link->m_OutputPin, visiting);
	return fallback;
}

float AnimatorRuntime::EvaluateBlueprintInputNumber(const AnimationControllerTransition& transition, const AnimationControllerBlueprintNode& node, uint32_t inputPin, float fallback, std::unordered_set<uint32_t>& visiting) const
{
	if (const AnimationControllerBlueprintLink* link = FindBlueprintInputLink(transition, node.m_Id, inputPin))
		return EvaluateBlueprintNodeNumber(transition, link->m_OutputNode, link->m_OutputPin, visiting);
	return fallback;
}

bool AnimatorRuntime::GetBlueprintParameterBool(std::string_view name) const
{
	const AnimationControllerParameter* parameter = FindParameter(name);
	if (!parameter)
		return false;

	switch (parameter->m_Type)
	{
	case AnimationParameterType::Bool:
	{
		const auto it = m_BoolParameters.find(parameter->m_Name);
		return it != m_BoolParameters.end() ? it->second : parameter->m_DefaultBool;
	}
	case AnimationParameterType::Int:
	{
		const auto it = m_IntParameters.find(parameter->m_Name);
		return (it != m_IntParameters.end() ? it->second : parameter->m_DefaultInt) != 0;
	}
	case AnimationParameterType::Float:
	{
		const auto it = m_FloatParameters.find(parameter->m_Name);
		return std::abs(it != m_FloatParameters.end() ? it->second : parameter->m_DefaultFloat) > 0.0001f;
	}
	case AnimationParameterType::Trigger:
		return m_TriggerParameters.contains(parameter->m_Name);
	}

	return false;
}

float AnimatorRuntime::GetBlueprintParameterNumber(std::string_view name) const
{
	const AnimationControllerParameter* parameter = FindParameter(name);
	if (!parameter)
		return 0.0f;

	switch (parameter->m_Type)
	{
	case AnimationParameterType::Bool:
	{
		const auto it = m_BoolParameters.find(parameter->m_Name);
		return (it != m_BoolParameters.end() ? it->second : parameter->m_DefaultBool) ? 1.0f : 0.0f;
	}
	case AnimationParameterType::Int:
	{
		const auto it = m_IntParameters.find(parameter->m_Name);
		return (float)(it != m_IntParameters.end() ? it->second : parameter->m_DefaultInt);
	}
	case AnimationParameterType::Float:
	{
		const auto it = m_FloatParameters.find(parameter->m_Name);
		return it != m_FloatParameters.end() ? it->second : parameter->m_DefaultFloat;
	}
	case AnimationParameterType::Trigger:
		return m_TriggerParameters.contains(parameter->m_Name) ? 1.0f : 0.0f;
	}

	return 0.0f;
}

void AnimatorRuntime::ConsumeTransitionTriggers(const AnimationControllerTransition& transition)
{
	if (!transition.m_BlueprintNodes.empty())
	{
		for (const AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
		{
			const AnimationControllerParameter* parameter = FindParameter(node.m_Parameter);
			if (parameter && parameter->m_Type == AnimationParameterType::Trigger)
				m_TriggerParameters.erase(parameter->m_Name);
		}
	}

	for (const AnimationControllerCondition& condition : transition.m_Conditions)
	{
		const AnimationControllerParameter* parameter = FindParameter(condition.m_Parameter);
		if (parameter && parameter->m_Type == AnimationParameterType::Trigger)
			m_TriggerParameters.erase(parameter->m_Name);
	}
}

void AnimatorRuntime::SwitchState(std::string_view stateName)
{
	if (!m_Controller || !m_Controller->FindState(stateName))
		return;

	m_CurrentStateName = std::string(stateName);
	m_StateTime = 0.0f;
	m_TransitionTargetStateName.clear();
	m_TransitionElapsed = 0.0f;
	m_TransitionDuration = 0.0f;
	m_TransitionTargetStateTime = 0.0f;
	m_Transitioning = false;
}

float AnimatorRuntime::NormalizeStateTime(const AnimationControllerState& state, float stateTime) const
{
	const float duration = GetStateDuration(state);
	if (duration <= 0.0f)
		return stateTime;
	return state.m_Loop ? std::fmod(std::max(stateTime, 0.0f), duration) : std::min(std::max(stateTime, 0.0f), duration);
}

void AnimatorRuntime::ApplyCurrentFrame()
{
	if (!m_Scene || !m_Controller)
		return;

	const AnimationControllerState* state = GetCurrentState();
	if (!state)
		return;

	if (m_Transitioning)
	{
		if (const AnimationControllerState* targetState = m_Controller->FindState(m_TransitionTargetStateName))
		{
			ApplyBlendedFrame(*state, m_StateTime, *targetState, m_TransitionTargetStateTime, GetTransitionProgress());
			return;
		}
	}

	ApplyStateFrame(*state, m_StateTime);
}

void AnimatorRuntime::ApplyStateFrame(const AnimationControllerState& state, float stateTime)
{
	Ref<Animation2D> clip = GetStateClip(state);
	if (!clip)
	{
		WarnMissingStateClip(state);
		return;
	}

	Entity entity = m_Scene->FindEntityByUUID(m_EntityId);
	if (!entity)
		return;

	const float duration = clip->GetDuration();
	float sampleTime = stateTime;
	if (duration > 0.0f)
		sampleTime = state.m_Loop ? std::fmod(sampleTime, duration) : std::min(sampleTime, duration);

	if (!clip->GetFrames().empty() && entity.HasComponent<SpriteRendererComponent>())
	{
		const size_t frameIndex = clip->GetFrameIndexAtTime(sampleTime);
		const AnimationFrame& frame = clip->GetFrames()[frameIndex];
		auto& spriteRenderer = entity.GetComponent<SpriteRendererComponent>();
		spriteRenderer.m_Texture = frame.m_Texture;
		spriteRenderer.m_TextureSpriteIndex = frame.m_TextureSpriteIndex;
	}
	ApplyPropertyTracks(*clip, sampleTime);
}

void AnimatorRuntime::ApplyBlendedFrame(const AnimationControllerState& sourceState, float sourceTime, const AnimationControllerState& targetState, float targetTime, float factor)
{
	if (!m_Scene)
		return;

	Ref<Animation2D> sourceClip = GetStateClip(sourceState);
	Ref<Animation2D> targetClip = GetStateClip(targetState);
	if (!sourceClip)
		WarnMissingStateClip(sourceState);
	if (!targetClip)
		WarnMissingStateClip(targetState);
	if (!sourceClip && !targetClip)
		return;

	Entity entity = m_Scene->FindEntityByUUID(m_EntityId);
	if (!entity)
		return;

	const float clampedFactor = std::clamp(factor, 0.0f, 1.0f);
	const auto resolveSampleTime = [](const AnimationControllerState& state, const Ref<Animation2D>& clip, float time)
		{
			if (!clip)
				return 0.0f;
			const float duration = clip->GetDuration();
			if (duration <= 0.0f)
				return time;
			return state.m_Loop ? std::fmod(std::max(time, 0.0f), duration) : std::min(std::max(time, 0.0f), duration);
		};

	const float sourceSampleTime = resolveSampleTime(sourceState, sourceClip, sourceTime);
	const float targetSampleTime = resolveSampleTime(targetState, targetClip, targetTime);

	if (entity.HasComponent<SpriteRendererComponent>())
	{
		auto& spriteRenderer = entity.GetComponent<SpriteRendererComponent>();
		const Ref<Animation2D>& spriteClip = clampedFactor >= 0.5f && targetClip ? targetClip : sourceClip;
		const float spriteTime = clampedFactor >= 0.5f ? targetSampleTime : sourceSampleTime;
		if (spriteClip && !spriteClip->GetFrames().empty())
		{
			const size_t frameIndex = spriteClip->GetFrameIndexAtTime(spriteTime);
			const AnimationFrame& frame = spriteClip->GetFrames()[frameIndex];
			spriteRenderer.m_Texture = frame.m_Texture;
			spriteRenderer.m_TextureSpriteIndex = frame.m_TextureSpriteIndex;
		}
	}

	if (entity.HasComponent<TransformComponent>())
	{
		auto& transform = entity.GetComponent<TransformComponent>();
		auto blendVec3Track = [clampedFactor](const std::vector<AnimationVec3Key>& sourceKeys, float sourceSample, const std::vector<AnimationVec3Key>& targetKeys, float targetSample, glm::vec3 fallback)
			{
				glm::vec3 sourceValue = fallback;
				glm::vec3 targetValue = fallback;
				const bool hasSource = SampleTrack(sourceKeys, sourceSample, sourceValue);
				const bool hasTarget = SampleTrack(targetKeys, targetSample, targetValue);
				if (hasSource && hasTarget)
					return Lerp(sourceValue, targetValue, clampedFactor);
				if (hasTarget)
					return clampedFactor >= 0.5f ? targetValue : fallback;
				if (hasSource)
					return clampedFactor < 0.5f ? sourceValue : fallback;
				return fallback;
			};

		if (sourceClip || targetClip)
		{
			transform.m_Translation = blendVec3Track(
				sourceClip ? sourceClip->GetTranslationKeys() : std::vector<AnimationVec3Key>{},
				sourceSampleTime,
				targetClip ? targetClip->GetTranslationKeys() : std::vector<AnimationVec3Key>{},
				targetSampleTime,
				transform.m_Translation);
			transform.m_Rotation = blendVec3Track(
				sourceClip ? sourceClip->GetRotationKeys() : std::vector<AnimationVec3Key>{},
				sourceSampleTime,
				targetClip ? targetClip->GetRotationKeys() : std::vector<AnimationVec3Key>{},
				targetSampleTime,
				transform.m_Rotation);
			transform.m_Scale = blendVec3Track(
				sourceClip ? sourceClip->GetScaleKeys() : std::vector<AnimationVec3Key>{},
				sourceSampleTime,
				targetClip ? targetClip->GetScaleKeys() : std::vector<AnimationVec3Key>{},
				targetSampleTime,
				transform.m_Scale);
		}
	}

	if (entity.HasComponent<SpriteRendererComponent>())
	{
		auto& spriteRenderer = entity.GetComponent<SpriteRendererComponent>();
		glm::vec4 sourceColor = spriteRenderer.m_Color;
		glm::vec4 targetColor = spriteRenderer.m_Color;
		const bool hasSourceColor = sourceClip && SampleTrack(sourceClip->GetColorKeys(), sourceSampleTime, sourceColor);
		const bool hasTargetColor = targetClip && SampleTrack(targetClip->GetColorKeys(), targetSampleTime, targetColor);
		if (hasSourceColor && hasTargetColor)
			spriteRenderer.m_Color = Lerp(sourceColor, targetColor, clampedFactor);
		else if (hasTargetColor && clampedFactor >= 0.5f)
			spriteRenderer.m_Color = targetColor;
		else if (hasSourceColor && clampedFactor < 0.5f)
			spriteRenderer.m_Color = sourceColor;
	}
}

void AnimatorRuntime::QueueEvents(const AnimationControllerState& state, float startTime, float endTime)
{
	if (endTime <= startTime)
		return;

	Ref<Animation2D> clip = GetStateClip(state);
	if (!clip)
		return;

	for (const AnimationEventKey& eventKey : clip->GetEvents())
	{
		if (eventKey.m_Time > startTime && eventKey.m_Time <= endTime && !eventKey.m_Name.empty())
			m_FiredEvents.push_back(eventKey.m_Name);
	}
}

void AnimatorRuntime::ApplyPropertyTracks(const Animation2D& clip, float sampleTime)
{
	if (!m_Scene)
		return;

	Entity entity = m_Scene->FindEntityByUUID(m_EntityId);
	if (!entity)
		return;

	if (entity.HasComponent<TransformComponent>())
	{
		auto& transform = entity.GetComponent<TransformComponent>();
		glm::vec3 value;
		if (SampleTrack(clip.GetTranslationKeys(), sampleTime, value))
			transform.m_Translation = value;
		if (SampleTrack(clip.GetRotationKeys(), sampleTime, value))
			transform.m_Rotation = value;
		if (SampleTrack(clip.GetScaleKeys(), sampleTime, value))
			transform.m_Scale = value;
	}

	if (entity.HasComponent<SpriteRendererComponent>())
	{
		glm::vec4 color;
		if (SampleTrack(clip.GetColorKeys(), sampleTime, color))
			entity.GetComponent<SpriteRendererComponent>().m_Color = color;
	}
}

void AnimatorRuntime::WarnMissingStateClip(const AnimationControllerState& state)
{
	if (!m_MissingClipWarnings.insert(state.m_Name).second)
		return;

	const uint64_t entityId = static_cast<uint64_t>(m_EntityId);
	if (state.m_MotionType == AnimationMotionType::BlendTree1D)
	{
		WHP_CORE_WARN("[AnimatorRuntime] Entity {0} state '{1}' has no valid blend child animation clip.", entityId, state.m_Name);
		return;
	}

	if (state.m_Clip == 0)
	{
		WHP_CORE_WARN("[AnimatorRuntime] Entity {0} state '{1}' has no animation clip assigned.", entityId, state.m_Name);
		return;
	}

	if (!AssetManager::IsAssetHandleValid(state.m_Clip))
	{
		WHP_CORE_WARN("[AnimatorRuntime] Entity {0} state '{1}' references missing animation clip handle {2}.", entityId, state.m_Name, static_cast<uint64_t>(state.m_Clip));
		return;
	}

	if (AssetManager::GetAssetType(state.m_Clip) != AssetType::Animation)
	{
		WHP_CORE_WARN("[AnimatorRuntime] Entity {0} state '{1}' clip handle {2} is not an Animation asset.", entityId, state.m_Name, static_cast<uint64_t>(state.m_Clip));
		return;
	}

	WHP_CORE_WARN("[AnimatorRuntime] Entity {0} state '{1}' animation clip {2} could not be loaded.", entityId, state.m_Name, static_cast<uint64_t>(state.m_Clip));
}

_WHIP_END
