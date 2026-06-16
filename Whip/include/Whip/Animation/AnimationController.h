#pragma once

#include <Whip/Asset/Asset.h>
#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

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

struct AnimationControllerState
{
	std::string m_Name = "State";
	AssetHandle m_Clip = 0;
	float m_Speed = 1.0f;
	bool m_Loop = true;
	std::vector<AnimationControllerTransition> m_Transitions;
};

class AnimationController : public Asset
{
public:
	static constexpr uint32_t FormatVersion = 1;

	AnimationController(AssetHandle handle = AssetHandle{});

	AssetType GetType() const override { return AssetType::AnimationController; }

	AnimationControllerState& AddState(std::string name, AssetHandle clip = 0);
	bool RemoveState(std::string_view name);
	AnimationControllerState* FindState(std::string_view name);
	const AnimationControllerState* FindState(std::string_view name) const;

	AnimationControllerParameter& AddParameter(std::string name, AnimationParameterType type);
	bool RemoveParameter(std::string_view name);

	void SetDefaultState(std::string_view name);
	const std::string& GetDefaultState() const { return m_DefaultState; }

	std::vector<AnimationControllerState>& GetStates() { return m_States; }
	const std::vector<AnimationControllerState>& GetStates() const { return m_States; }
	std::vector<AnimationControllerParameter>& GetParameters() { return m_Parameters; }
	const std::vector<AnimationControllerParameter>& GetParameters() const { return m_Parameters; }

	void Serialize(const std::filesystem::path& filepath) const;
	bool Deserialize(const std::filesystem::path& filepath);

private:
	std::string MakeUniqueStateName(std::string_view baseName) const;
	std::string MakeUniqueParameterName(std::string_view baseName) const;

	std::string m_DefaultState;
	std::vector<AnimationControllerState> m_States;
	std::vector<AnimationControllerParameter> m_Parameters;
};

_WHIP_END
