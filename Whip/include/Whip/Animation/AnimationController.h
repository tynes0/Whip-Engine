#pragma once

#include "Whip/Asset/Asset.h"
#include "Whip/Core/Core.h"
#include "Whip/Core/UUID.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

_WHIP_START

enum class AnimationParameterType : uint8_t
{
	Bool,
	Int,
	Float,
	Trigger
};

MakeFrenumInNamespace(whip, AnimationParameterType, Bool, Int, Float, Trigger)

enum class AnimationConditionMode : uint8_t
{
	If,
	IfNot,
	Greater,
	Less,
	Equals,
	NotEquals
};

MakeFrenumInNamespace(whip, AnimationConditionMode, If, IfNot, Greater, Less, Equals, NotEquals)

enum class AnimationMotionType : uint8_t
{
	Clip,
	BlendTree1D
};

MakeFrenumInNamespace(whip, AnimationMotionType, Clip, BlendTree1D)

struct AnimationControllerParameter
{
	std::string m_Name;
	AnimationParameterType m_Type = AnimationParameterType::Bool;
	float m_DefaultFloat = 0.0f;
	int32_t m_DefaultInt = 0;
	bool m_DefaultBool = false;
};

struct AnimationControllerCondition
{
	std::string m_Parameter;
	AnimationConditionMode m_Mode = AnimationConditionMode::If;
	float m_Threshold = 0.0f;
	int32_t m_IntValue = 0;
	bool m_BoolValue = true;
};

struct AnimationControllerTransition
{
	std::string m_TargetState;
	float m_Duration = 0.1f;
	float m_ExitTime = 1.0f;
	bool m_HasExitTime = true;
	std::vector<AnimationControllerCondition> m_Conditions;
};

struct AnimationBlendChild
{
	AssetHandle m_Clip = 0;
	float m_Threshold = 0.0f;
	float m_Speed = 1.0f;
};

struct AnimationControllerState
{
	std::string m_Name = "State";
	AnimationMotionType m_MotionType = AnimationMotionType::Clip;
	AssetHandle m_Clip = 0;
	std::string m_BlendParameter;
	std::vector<AnimationBlendChild> m_BlendChildren;
	float m_Speed = 1.0f;
	bool m_Loop = true;
	glm::vec2 m_GraphPosition{ 0.0f, 0.0f };
	std::vector<AnimationControllerTransition> m_Transitions;
};

class AnimationController : public Asset
{
public:
	static constexpr uint32_t FormatVersion = 2;
	static constexpr std::string_view ExitStateName = "Exit";

	AnimationController(AssetHandle handle = AssetHandle{});

	AssetType GetType() const override;

	AnimationControllerState& AddState(std::string name, AssetHandle clip = 0);
	bool RemoveState(std::string_view name);
	AnimationControllerState* FindState(std::string_view name);
	const AnimationControllerState* FindState(std::string_view name) const;

	AnimationControllerParameter& AddParameter(std::string name, AnimationParameterType type);
	bool RemoveParameter(std::string_view name);

	void SetDefaultState(std::string_view name);
	const std::string& GetDefaultState() const;

	std::vector<AnimationControllerState>& GetStates();
	const std::vector<AnimationControllerState>& GetStates() const;
	std::vector<AnimationControllerParameter>& GetParameters();
	const std::vector<AnimationControllerParameter>& GetParameters() const;
	std::vector<AnimationControllerTransition>& GetAnyStateTransitions();
	const std::vector<AnimationControllerTransition>& GetAnyStateTransitions() const;
	glm::vec2& GetEntryGraphPosition();
	const glm::vec2& GetEntryGraphPosition() const;
	glm::vec2& GetAnyStateGraphPosition();
	const glm::vec2& GetAnyStateGraphPosition() const;
	glm::vec2& GetExitGraphPosition();
	const glm::vec2& GetExitGraphPosition() const;

	void Serialize(const std::filesystem::path& filepath) const;
	bool Deserialize(const std::filesystem::path& filepath);

private:
	std::string MakeUniqueStateName(std::string_view baseName) const;
	std::string MakeUniqueParameterName(std::string_view baseName) const;

	std::string m_DefaultState;
	std::vector<AnimationControllerState> m_States;
	std::vector<AnimationControllerParameter> m_Parameters;
	std::vector<AnimationControllerTransition> m_AnyStateTransitions;
	glm::vec2 m_EntryGraphPosition{ 22.0f, 58.0f };
	glm::vec2 m_AnyStateGraphPosition{ 22.0f, 160.0f };
	glm::vec2 m_ExitGraphPosition{ 620.0f, 108.0f };
};

_WHIP_END
