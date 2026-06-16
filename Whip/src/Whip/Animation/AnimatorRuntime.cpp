#include "WhipPch.h"
#include <Whip/Animation/AnimatorRuntime.h>

#include <Whip/Animation/Animation2D.h>
#include <Whip/Asset/AssetManager.h>
#include <Whip/Scene/Components.h>
#include <Whip/Scene/Entity.h>
#include <Whip/Scene/Scene.h>

#include <cmath>

_WHIP_START

namespace
{
	bool NearlyEqual(float left, float right)
	{
		return std::abs(left - right) <= 0.0001f;
	}
}

void AnimatorRuntime::Bind(Scene* scene, UUID entityId, const Ref<AnimationController>& controller, std::string_view initialState)
{
	m_Scene = scene;
	m_EntityId = entityId;
	m_Controller = controller;
	m_CurrentStateName.clear();
	m_StateTime = 0.0f;
	m_Playing = false;
	m_BoolParameters.clear();
	m_IntParameters.clear();
	m_FloatParameters.clear();
	m_TriggerParameters.clear();

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
	m_Playing = !m_CurrentStateName.empty();
	ApplyCurrentFrame();
}

void AnimatorRuntime::Stop()
{
	m_Playing = false;
	m_StateTime = 0.0f;
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

	const float stateSpeed = std::max(state->m_Speed, 0.0f);
	const float animatorSpeed = std::max(speed, 0.0f);
	m_StateTime += std::max(static_cast<float>(ts), 0.0f) * stateSpeed * animatorSpeed;

	const float stateDuration = GetStateDuration(*state);
	if (TryTransition(*state, stateDuration))
	{
		ApplyCurrentFrame();
		return;
	}

	if (stateDuration > 0.0f)
	{
		if (state->m_Loop)
			m_StateTime = std::fmod(m_StateTime, stateDuration);
		else
			m_StateTime = std::min(m_StateTime, stateDuration);
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

const AnimationControllerState* AnimatorRuntime::GetCurrentState() const
{
	return m_Controller ? m_Controller->FindState(m_CurrentStateName) : nullptr;
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
	if (state.m_Clip == 0 || !AssetManager::IsAssetHandleValid(state.m_Clip) || AssetManager::GetAssetType(state.m_Clip) != AssetType::Animation)
		return nullptr;
	return AssetManager::GetAsset<Animation2D>(state.m_Clip);
}

float AnimatorRuntime::GetStateDuration(const AnimationControllerState& state) const
{
	Ref<Animation2D> clip = GetStateClip(state);
	return clip ? clip->GetDuration() : 0.0f;
}

bool AnimatorRuntime::TryTransition(const AnimationControllerState& state, float stateDuration)
{
	for (const AnimationControllerTransition& transition : state.m_Transitions)
	{
		if (transition.m_TargetState.empty() || !m_Controller->FindState(transition.m_TargetState))
			continue;

		if (!IsExitTimeReady(transition, stateDuration) || !ConditionsPass(transition))
			continue;

		ConsumeTransitionTriggers(transition);
		SwitchState(transition.m_TargetState);
		return true;
	}

	return false;
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
		default: return false;
		}
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
		default: return false;
		}
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
		default: return false;
		}
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
		default: return false;
		}
	}
	}

	return false;
}

void AnimatorRuntime::ConsumeTransitionTriggers(const AnimationControllerTransition& transition)
{
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
}

void AnimatorRuntime::ApplyCurrentFrame()
{
	if (!m_Scene || !m_Controller)
		return;

	const AnimationControllerState* state = GetCurrentState();
	if (!state)
		return;

	Ref<Animation2D> clip = GetStateClip(*state);
	if (!clip || clip->GetFrames().empty())
		return;

	Entity entity = m_Scene->FindEntityByUUID(m_EntityId);
	if (!entity || !entity.HasComponent<SpriteRendererComponent>())
		return;

	const float duration = clip->GetDuration();
	float sampleTime = m_StateTime;
	if (duration > 0.0f)
		sampleTime = state->m_Loop ? std::fmod(sampleTime, duration) : std::min(sampleTime, duration);

	const size_t frameIndex = clip->GetFrameIndexAtTime(sampleTime);
	entity.GetComponent<SpriteRendererComponent>().m_Texture = clip->GetFrames()[frameIndex].m_Texture;
}

_WHIP_END
