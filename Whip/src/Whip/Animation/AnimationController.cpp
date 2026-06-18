#include "WhipPch.h"
#include "Whip/Animation/AnimationController.h"

#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>

_WHIP_START

namespace
{
	template<typename T>
	T ReadYamlValue(const YAML::Node& node, const char* key, const T& fallback)
	{
		if (!node || !node[key])
			return fallback;
		return node[key].as<T>();
	}

	AnimationParameterType ParseParameterType(const YAML::Node& node)
	{
		if (!node)
			return AnimationParameterType::Bool;
		const std::string value = node.as<std::string>();
		if (auto type = frenum::cast<AnimationParameterType>(value))
			return type.value();
		return AnimationParameterType::Bool;
	}

	AnimationConditionMode ParseConditionMode(const YAML::Node& node)
	{
		if (!node)
			return AnimationConditionMode::If;
		const std::string value = node.as<std::string>();
		if (auto mode = frenum::cast<AnimationConditionMode>(value))
			return mode.value();
		return AnimationConditionMode::If;
	}

	AnimationBlueprintNodeType ParseBlueprintNodeType(const YAML::Node& node)
	{
		if (!node)
			return AnimationBlueprintNodeType::Parameter;
		const std::string value = node.as<std::string>();
		if (auto type = frenum::cast<AnimationBlueprintNodeType>(value))
			return type.value();
		return AnimationBlueprintNodeType::Parameter;
	}

	AnimationMotionType ParseMotionType(const YAML::Node& node)
	{
		if (!node)
			return AnimationMotionType::Clip;
		const std::string value = node.as<std::string>();
		if (auto motionType = frenum::cast<AnimationMotionType>(value))
			return motionType.value();
		return AnimationMotionType::Clip;
	}

	glm::vec2 ReadGraphPosition(const YAML::Node& node, const char* key, const glm::vec2& fallback)
	{
		const YAML::Node position = node[key];
		if (!position || !position.IsSequence() || position.size() < 2)
			return fallback;
		return { position[0].as<float>(fallback.x), position[1].as<float>(fallback.y) };
	}

	void SerializeTransition(YAML::Emitter& out, const AnimationControllerTransition& transition)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "target_state" << YAML::Value << transition.m_TargetState;
		out << YAML::Key << "duration" << YAML::Value << transition.m_Duration;
		out << YAML::Key << "exit_time" << YAML::Value << transition.m_ExitTime;
		out << YAML::Key << "has_exit_time" << YAML::Value << transition.m_HasExitTime;

		out << YAML::Key << "conditions" << YAML::Value << YAML::BeginSeq;
		for (const AnimationControllerCondition& condition : transition.m_Conditions)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "parameter" << YAML::Value << condition.m_Parameter;
			out << YAML::Key << "mode" << YAML::Value << frenum::to_string(condition.m_Mode);
			out << YAML::Key << "threshold" << YAML::Value << condition.m_Threshold;
			out << YAML::Key << "int_value" << YAML::Value << condition.m_IntValue;
			out << YAML::Key << "bool_value" << YAML::Value << condition.m_BoolValue;
			out << YAML::Key << "graph_position" << YAML::Value << YAML::Flow << YAML::BeginSeq << condition.m_GraphPosition.x << condition.m_GraphPosition.y << YAML::EndSeq;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::Key << "next_blueprint_node_id" << YAML::Value << transition.m_NextBlueprintNodeId;
		out << YAML::Key << "next_blueprint_link_id" << YAML::Value << transition.m_NextBlueprintLinkId;

		out << YAML::Key << "blueprint_nodes" << YAML::Value << YAML::BeginSeq;
		for (const AnimationControllerBlueprintNode& node : transition.m_BlueprintNodes)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "id" << YAML::Value << node.m_Id;
			out << YAML::Key << "type" << YAML::Value << frenum::to_string(node.m_Type);
			out << YAML::Key << "parameter" << YAML::Value << node.m_Parameter;
			out << YAML::Key << "threshold" << YAML::Value << node.m_Threshold;
			out << YAML::Key << "int_value" << YAML::Value << node.m_IntValue;
			out << YAML::Key << "bool_value" << YAML::Value << node.m_BoolValue;
			out << YAML::Key << "input_float_values" << YAML::Value << YAML::Flow << YAML::BeginSeq << node.m_InputFloatValues[0] << node.m_InputFloatValues[1] << YAML::EndSeq;
			out << YAML::Key << "input_int_values" << YAML::Value << YAML::Flow << YAML::BeginSeq << node.m_InputIntValues[0] << node.m_InputIntValues[1] << YAML::EndSeq;
			out << YAML::Key << "input_bool_values" << YAML::Value << YAML::Flow << YAML::BeginSeq << node.m_InputBoolValues[0] << node.m_InputBoolValues[1] << YAML::EndSeq;
			out << YAML::Key << "graph_position" << YAML::Value << YAML::Flow << YAML::BeginSeq << node.m_GraphPosition.x << node.m_GraphPosition.y << YAML::EndSeq;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;

		out << YAML::Key << "blueprint_links" << YAML::Value << YAML::BeginSeq;
		for (const AnimationControllerBlueprintLink& link : transition.m_BlueprintLinks)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "id" << YAML::Value << link.m_Id;
			out << YAML::Key << "output_node" << YAML::Value << link.m_OutputNode;
			out << YAML::Key << "output_pin" << YAML::Value << link.m_OutputPin;
			out << YAML::Key << "input_node" << YAML::Value << link.m_InputNode;
			out << YAML::Key << "input_pin" << YAML::Value << link.m_InputPin;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;
	}

	AnimationControllerTransition DeserializeTransition(const YAML::Node& transitionNode)
	{
		AnimationControllerTransition transition;
		transition.m_TargetState = ReadYamlValue<std::string>(transitionNode, "target_state", {});
		transition.m_Duration = ReadYamlValue<float>(transitionNode, "duration", 0.1f);
		transition.m_ExitTime = ReadYamlValue<float>(transitionNode, "exit_time", 1.0f);
		transition.m_HasExitTime = ReadYamlValue<bool>(transitionNode, "has_exit_time", true);

		if (const YAML::Node conditions = transitionNode["conditions"])
		{
			for (const YAML::Node& conditionNode : conditions)
			{
				AnimationControllerCondition condition;
				condition.m_Parameter = ReadYamlValue<std::string>(conditionNode, "parameter", {});
				condition.m_Mode = ParseConditionMode(conditionNode["mode"]);
				condition.m_Threshold = ReadYamlValue<float>(conditionNode, "threshold", 0.0f);
				condition.m_IntValue = ReadYamlValue<int32_t>(conditionNode, "int_value", 0);
				condition.m_BoolValue = ReadYamlValue<bool>(conditionNode, "bool_value", true);
				condition.m_GraphPosition = ReadGraphPosition(conditionNode, "graph_position", condition.m_GraphPosition);
				transition.m_Conditions.push_back(condition);
			}
		}

		transition.m_NextBlueprintNodeId = ReadYamlValue<uint32_t>(transitionNode, "next_blueprint_node_id", 1);
		transition.m_NextBlueprintLinkId = ReadYamlValue<uint32_t>(transitionNode, "next_blueprint_link_id", 1);

		if (const YAML::Node blueprintNodes = transitionNode["blueprint_nodes"])
		{
			for (const YAML::Node& nodeYaml : blueprintNodes)
			{
				AnimationControllerBlueprintNode node;
				node.m_Id = ReadYamlValue<uint32_t>(nodeYaml, "id", transition.m_NextBlueprintNodeId++);
				node.m_Type = ParseBlueprintNodeType(nodeYaml["type"]);
				node.m_Parameter = ReadYamlValue<std::string>(nodeYaml, "parameter", {});
				node.m_Threshold = ReadYamlValue<float>(nodeYaml, "threshold", 0.0f);
				node.m_IntValue = ReadYamlValue<int32_t>(nodeYaml, "int_value", 0);
				node.m_BoolValue = ReadYamlValue<bool>(nodeYaml, "bool_value", true);
				if (const YAML::Node inputFloatValues = nodeYaml["input_float_values"]; inputFloatValues && inputFloatValues.IsSequence() && inputFloatValues.size() >= 2)
				{
					node.m_InputFloatValues[0] = inputFloatValues[0].as<float>(0.0f);
					node.m_InputFloatValues[1] = inputFloatValues[1].as<float>(node.m_Threshold);
				}
				else
				{
					node.m_InputFloatValues[1] = node.m_Threshold;
				}
				if (const YAML::Node inputIntValues = nodeYaml["input_int_values"]; inputIntValues && inputIntValues.IsSequence() && inputIntValues.size() >= 2)
				{
					node.m_InputIntValues[0] = inputIntValues[0].as<int32_t>(0);
					node.m_InputIntValues[1] = inputIntValues[1].as<int32_t>(node.m_IntValue);
				}
				else
				{
					node.m_InputIntValues[1] = node.m_IntValue;
				}
				if (const YAML::Node inputBoolValues = nodeYaml["input_bool_values"]; inputBoolValues && inputBoolValues.IsSequence() && inputBoolValues.size() >= 2)
				{
					node.m_InputBoolValues[0] = inputBoolValues[0].as<bool>(false);
					node.m_InputBoolValues[1] = inputBoolValues[1].as<bool>(node.m_BoolValue);
				}
				else
				{
					node.m_InputBoolValues[1] = node.m_BoolValue;
				}
				node.m_GraphPosition = ReadGraphPosition(nodeYaml, "graph_position", node.m_GraphPosition);
				transition.m_NextBlueprintNodeId = std::max(transition.m_NextBlueprintNodeId, node.m_Id + 1);
				transition.m_BlueprintNodes.push_back(node);
			}
		}

		if (const YAML::Node blueprintLinks = transitionNode["blueprint_links"])
		{
			for (const YAML::Node& linkYaml : blueprintLinks)
			{
				AnimationControllerBlueprintLink link;
				link.m_Id = ReadYamlValue<uint32_t>(linkYaml, "id", transition.m_NextBlueprintLinkId++);
				link.m_OutputNode = ReadYamlValue<uint32_t>(linkYaml, "output_node", 0);
				link.m_OutputPin = ReadYamlValue<uint32_t>(linkYaml, "output_pin", 0);
				link.m_InputNode = ReadYamlValue<uint32_t>(linkYaml, "input_node", 0);
				link.m_InputPin = ReadYamlValue<uint32_t>(linkYaml, "input_pin", 0);
				transition.m_NextBlueprintLinkId = std::max(transition.m_NextBlueprintLinkId, link.m_Id + 1);
				if (link.m_OutputNode != 0 && link.m_InputNode != 0)
					transition.m_BlueprintLinks.push_back(link);
			}
		}

		return transition;
	}
}

AnimationController::AnimationController(AssetHandle handle)
	: Asset(handle)
{
	AddState("State");
	SetDefaultState("State");
}

AssetType AnimationController::GetType() const
{
	return AssetType::AnimationController;
}

const std::string& AnimationController::GetDefaultState() const
{
	return m_DefaultState;
}

std::vector<AnimationControllerState>& AnimationController::GetStates()
{
	return m_States;
}

const std::vector<AnimationControllerState>& AnimationController::GetStates() const
{
	return m_States;
}

std::vector<AnimationControllerParameter>& AnimationController::GetParameters()
{
	return m_Parameters;
}

const std::vector<AnimationControllerParameter>& AnimationController::GetParameters() const
{
	return m_Parameters;
}

std::vector<AnimationControllerTransition>& AnimationController::GetAnyStateTransitions()
{
	return m_AnyStateTransitions;
}

const std::vector<AnimationControllerTransition>& AnimationController::GetAnyStateTransitions() const
{
	return m_AnyStateTransitions;
}

glm::vec2& AnimationController::GetEntryGraphPosition()
{
	return m_EntryGraphPosition;
}

const glm::vec2& AnimationController::GetEntryGraphPosition() const
{
	return m_EntryGraphPosition;
}

glm::vec2& AnimationController::GetAnyStateGraphPosition()
{
	return m_AnyStateGraphPosition;
}

const glm::vec2& AnimationController::GetAnyStateGraphPosition() const
{
	return m_AnyStateGraphPosition;
}

glm::vec2& AnimationController::GetExitGraphPosition()
{
	return m_ExitGraphPosition;
}

const glm::vec2& AnimationController::GetExitGraphPosition() const
{
	return m_ExitGraphPosition;
}

AnimationControllerState& AnimationController::AddState(std::string name, AssetHandle clip)
{
	AnimationControllerState state;
	state.m_Name = MakeUniqueStateName(name.empty() ? "State" : std::move(name));
	state.m_Clip = clip;
	m_States.push_back(state);

	if (m_DefaultState.empty())
		m_DefaultState = m_States.back().m_Name;

	return m_States.back();
}

bool AnimationController::RemoveState(std::string_view name)
{
	const auto it = std::ranges::find_if(m_States, [name](const AnimationControllerState& state)
	{
		return state.m_Name == name;
	});

	if (it == m_States.end())
		return false;

	m_States.erase(it);
	for (AnimationControllerState& state : m_States)
	{
		std::erase_if(state.m_Transitions, [name](const AnimationControllerTransition& transition)
			{
				return transition.m_TargetState == name;
			});
	}
	std::erase_if(m_AnyStateTransitions, [name](const AnimationControllerTransition& transition)
		{
			return transition.m_TargetState == name;
		});

	if (m_DefaultState == name)
		m_DefaultState = m_States.empty() ? std::string{} : m_States.front().m_Name;

	return true;
}

AnimationControllerState* AnimationController::FindState(std::string_view name)
{
	const auto it = std::ranges::find_if(m_States, [name](const AnimationControllerState& state)
	{
		return state.m_Name == name;
	});
	return it == m_States.end() ? nullptr : &*it;
}

const AnimationControllerState* AnimationController::FindState(std::string_view name) const
{
	const auto it = std::ranges::find_if(m_States, [name](const AnimationControllerState& state)
	{
		return state.m_Name == name;
	});
	return it == m_States.end() ? nullptr : &*it;
}

AnimationControllerParameter& AnimationController::AddParameter(std::string name, AnimationParameterType type)
{
	AnimationControllerParameter parameter;
	parameter.m_Name = MakeUniqueParameterName(name.empty() ? "Parameter" : std::move(name));
	parameter.m_Type = type;
	m_Parameters.push_back(parameter);
	return m_Parameters.back();
}

bool AnimationController::RemoveParameter(std::string_view name)
{
	const auto oldSize = m_Parameters.size();
	std::erase_if(m_Parameters, [name](const AnimationControllerParameter& parameter)
		{
			return parameter.m_Name == name;
		});

	if (m_Parameters.size() == oldSize)
		return false;

	for (AnimationControllerState& state : m_States)
	{
		for (AnimationControllerTransition& transition : state.m_Transitions)
		{
			std::erase_if(transition.m_Conditions, [name](const AnimationControllerCondition& condition)
				{
					return condition.m_Parameter == name;
				});
		}
	}
	for (AnimationControllerTransition& transition : m_AnyStateTransitions)
	{
		std::erase_if(transition.m_Conditions, [name](const AnimationControllerCondition& condition)
			{
				return condition.m_Parameter == name;
			});
	}

	return true;
}

void AnimationController::SetDefaultState(std::string_view name)
{
	if (FindState(name))
		m_DefaultState = std::string(name);
}

void AnimationController::Serialize(const std::filesystem::path& filepath) const
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "animation_controller" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "version" << YAML::Value << FormatVersion;
	out << YAML::Key << "default_state" << YAML::Value << m_DefaultState;
	out << YAML::Key << "entry_graph_position" << YAML::Value << YAML::Flow << YAML::BeginSeq << m_EntryGraphPosition.x << m_EntryGraphPosition.y << YAML::EndSeq;
	out << YAML::Key << "any_state_graph_position" << YAML::Value << YAML::Flow << YAML::BeginSeq << m_AnyStateGraphPosition.x << m_AnyStateGraphPosition.y << YAML::EndSeq;
	out << YAML::Key << "exit_graph_position" << YAML::Value << YAML::Flow << YAML::BeginSeq << m_ExitGraphPosition.x << m_ExitGraphPosition.y << YAML::EndSeq;

	out << YAML::Key << "parameters" << YAML::Value << YAML::BeginSeq;
	for (const AnimationControllerParameter& parameter : m_Parameters)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << parameter.m_Name;
		out << YAML::Key << "type" << YAML::Value << frenum::to_string(parameter.m_Type);
		out << YAML::Key << "default_float" << YAML::Value << parameter.m_DefaultFloat;
		out << YAML::Key << "default_int" << YAML::Value << parameter.m_DefaultInt;
		out << YAML::Key << "default_bool" << YAML::Value << parameter.m_DefaultBool;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;

	out << YAML::Key << "any_state_transitions" << YAML::Value << YAML::BeginSeq;
	for (const AnimationControllerTransition& transition : m_AnyStateTransitions)
		SerializeTransition(out, transition);
	out << YAML::EndSeq;

	out << YAML::Key << "states" << YAML::Value << YAML::BeginSeq;
	for (const AnimationControllerState& state : m_States)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << state.m_Name;
		out << YAML::Key << "motion_type" << YAML::Value << frenum::to_string(state.m_MotionType);
		out << YAML::Key << "clip" << YAML::Value << static_cast<uint64_t>(state.m_Clip);
		out << YAML::Key << "blend_parameter" << YAML::Value << state.m_BlendParameter;
		out << YAML::Key << "blend_children" << YAML::Value << YAML::BeginSeq;
		for (const AnimationBlendChild& child : state.m_BlendChildren)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "clip" << YAML::Value << static_cast<uint64_t>(child.m_Clip);
			out << YAML::Key << "threshold" << YAML::Value << child.m_Threshold;
			out << YAML::Key << "speed" << YAML::Value << child.m_Speed;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::Key << "speed" << YAML::Value << state.m_Speed;
		out << YAML::Key << "loop" << YAML::Value << state.m_Loop;
		out << YAML::Key << "graph_position" << YAML::Value << YAML::Flow << YAML::BeginSeq << state.m_GraphPosition.x << state.m_GraphPosition.y << YAML::EndSeq;

		out << YAML::Key << "transitions" << YAML::Value << YAML::BeginSeq;
		for (const AnimationControllerTransition& transition : state.m_Transitions)
			SerializeTransition(out, transition);
		out << YAML::EndSeq;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	out << YAML::EndMap;

	std::ofstream fout(filepath);
	fout << out.c_str();
}

bool AnimationController::Deserialize(const std::filesystem::path& filepath)
{
	YAML::Node data;
	try
	{
		data = YAML::LoadFile(filepath.string());
	}
	catch (const YAML::Exception& e)
	{
		WHP_CORE_ERROR("[AnimationController] Failed to load controller '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}

	YAML::Node root = data["animation_controller"] ? data["animation_controller"] : data;
	m_DefaultState = ReadYamlValue<std::string>(root, "default_state", {});
	m_States.clear();
	m_Parameters.clear();
	m_AnyStateTransitions.clear();

	if (const YAML::Node parameters = root["parameters"])
	{
		for (const YAML::Node& parameterNode : parameters)
		{
			AnimationControllerParameter parameter;
			parameter.m_Name = ReadYamlValue<std::string>(parameterNode, "name", "Parameter");
			parameter.m_Type = ParseParameterType(parameterNode["type"]);
			parameter.m_DefaultFloat = ReadYamlValue<float>(parameterNode, "default_float", 0.0f);
			parameter.m_DefaultInt = ReadYamlValue<int32_t>(parameterNode, "default_int", 0);
			parameter.m_DefaultBool = ReadYamlValue<bool>(parameterNode, "default_bool", false);
			m_Parameters.push_back(parameter);
		}
	}

	if (const YAML::Node transitions = root["any_state_transitions"])
	{
		for (const YAML::Node& transitionNode : transitions)
			m_AnyStateTransitions.push_back(DeserializeTransition(transitionNode));
	}

	if (const YAML::Node states = root["states"])
	{
		for (const YAML::Node& stateNode : states)
		{
			AnimationControllerState state;
			state.m_Name = ReadYamlValue<std::string>(stateNode, "name", "State");
			state.m_MotionType = ParseMotionType(stateNode["motion_type"]);
			state.m_Clip = ReadYamlValue<uint64_t>(stateNode, "clip", 0);
			state.m_BlendParameter = ReadYamlValue<std::string>(stateNode, "blend_parameter", {});
			state.m_Speed = ReadYamlValue<float>(stateNode, "speed", 1.0f);
			state.m_Loop = ReadYamlValue<bool>(stateNode, "loop", true);
			state.m_GraphPosition = ReadGraphPosition(stateNode, "graph_position", state.m_GraphPosition);

			if (const YAML::Node blendChildren = stateNode["blend_children"])
			{
				for (const YAML::Node& childNode : blendChildren)
				{
					AnimationBlendChild child;
					child.m_Clip = ReadYamlValue<uint64_t>(childNode, "clip", 0);
					child.m_Threshold = ReadYamlValue<float>(childNode, "threshold", 0.0f);
					child.m_Speed = ReadYamlValue<float>(childNode, "speed", 1.0f);
					state.m_BlendChildren.push_back(child);
				}
			}

			if (const YAML::Node transitions = stateNode["transitions"])
			{
				for (const YAML::Node& transitionNode : transitions)
					state.m_Transitions.push_back(DeserializeTransition(transitionNode));
			}

			m_States.push_back(state);
		}
	}

	if (m_States.empty())
		AddState("State");
	if (m_DefaultState.empty() || !FindState(m_DefaultState))
		m_DefaultState = m_States.front().m_Name;

	m_EntryGraphPosition = ReadGraphPosition(root, "entry_graph_position", m_EntryGraphPosition);
	m_AnyStateGraphPosition = ReadGraphPosition(root, "any_state_graph_position", m_AnyStateGraphPosition);
	m_ExitGraphPosition = ReadGraphPosition(root, "exit_graph_position", m_ExitGraphPosition);

	return true;
}

std::string AnimationController::MakeUniqueStateName(std::string_view baseName) const
{
	std::string candidate(baseName);
	for (int suffix = 1; FindState(candidate); ++suffix)
		candidate = std::string(baseName) + " " + std::to_string(suffix);
	return candidate;
}

std::string AnimationController::MakeUniqueParameterName(std::string_view baseName) const
{
	std::string candidate(baseName);
	const auto exists = [this](std::string_view name)
		{
			return std::ranges::any_of(m_Parameters, [name](const AnimationControllerParameter& parameter)
			{
				return parameter.m_Name == name;
			});
		};

	for (int suffix = 1; exists(candidate); ++suffix)
		candidate = std::string(baseName) + " " + std::to_string(suffix);
	return candidate;
}

_WHIP_END
