#include "WhipPch.h"
#include "Whip/Animation/Animation2D.h"
#include "Whip/Animation/AnimationManager.h"
#include "Whip/Scene/Components.h"

#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif
#include <yaml-cpp/yaml.h>

template<>
struct YAML::convert<whip::AssetHandle>
{
	static Node encode(const whip::AssetHandle& handle)
	{
		Node node;
		node.push_back(static_cast<uint64_t>(handle));
		return node;
	}

	static bool decode(const Node& node, whip::AssetHandle& handle)
	{
		handle = node.as<uint64_t>();
		return true;
	}
};

_WHIP_START

namespace
{
	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& value)
	{
		out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& value)
	{
		out << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << value.w << YAML::EndSeq;
		return out;
	}

	glm::vec3 ReadVec3(const YAML::Node& node, const glm::vec3& fallback = glm::vec3{ 0.0f })
	{
		if (!node || !node.IsSequence() || node.size() < 3)
			return fallback;
		return { node[0].as<float>(fallback.x), node[1].as<float>(fallback.y), node[2].as<float>(fallback.z) };
	}

	glm::vec4 ReadVec4(const YAML::Node& node, const glm::vec4& fallback = glm::vec4{ 1.0f })
	{
		if (!node || !node.IsSequence() || node.size() < 4)
			return fallback;
		return { node[0].as<float>(fallback.x), node[1].as<float>(fallback.y), node[2].as<float>(fallback.z), node[3].as<float>(fallback.w) };
	}

	void WriteVec3Track(YAML::Emitter& out, const char* key, const std::vector<AnimationVec3Key>& keys)
	{
		out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
		for (const AnimationVec3Key& frame : keys)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "time" << YAML::Value << frame.m_Time;
			out << YAML::Key << "value" << YAML::Value << frame.m_Value;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	void WriteVec4Track(YAML::Emitter& out, const char* key, const std::vector<AnimationVec4Key>& keys)
	{
		out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
		for (const AnimationVec4Key& frame : keys)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "time" << YAML::Value << frame.m_Time;
			out << YAML::Key << "value" << YAML::Value << frame.m_Value;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}

	void ReadVec3Track(const YAML::Node& node, std::vector<AnimationVec3Key>& keys)
	{
		keys.clear();
		if (!node)
			return;
		for (const YAML::Node& keyNode : node)
		{
			AnimationVec3Key key;
			key.m_Time = keyNode["time"].as<float>(0.0f);
			key.m_Value = ReadVec3(keyNode["value"]);
			keys.push_back(key);
		}
	}

	void ReadVec4Track(const YAML::Node& node, std::vector<AnimationVec4Key>& keys)
	{
		keys.clear();
		if (!node)
			return;
		for (const YAML::Node& keyNode : node)
		{
			AnimationVec4Key key;
			key.m_Time = keyNode["time"].as<float>(0.0f);
			key.m_Value = ReadVec4(keyNode["value"]);
			keys.push_back(key);
		}
	}
}

Animation2D::Animation2D(AssetHandle handleIn) : Asset(handleIn), m_Frames() { }

Animation2D::~Animation2D() = default;

Ref<Animation2D> Animation2D::Copy(const Ref<Animation2D>& anim)
{
	auto newAnimation = MakeRef<Animation2D>();
	newAnimation->m_Frames = anim->m_Frames;
	newAnimation->m_Events = anim->m_Events;
	newAnimation->m_TranslationKeys = anim->m_TranslationKeys;
	newAnimation->m_RotationKeys = anim->m_RotationKeys;
	newAnimation->m_ScaleKeys = anim->m_ScaleKeys;
	newAnimation->m_ColorKeys = anim->m_ColorKeys;
	newAnimation->m_Loop = anim->m_Loop;
	newAnimation->m_Name = anim->m_Name;
	return newAnimation;
}

void Animation2D::SetFrames(const std::vector<AnimationFrame>& frames, bool loop)
{
	m_Frames = frames;
	m_Loop = loop;
}

void Animation2D::AddFrame(const AnimationFrame& frame)
{
	m_Frames.push_back(frame);
}

void Animation2D::RemoveFrame(size_t index)
{
	if (index < m_Frames.size())
		m_Frames.erase(m_Frames.begin() + static_cast<std::vector<AnimationFrame>::difference_type>(index));
}

void Animation2D::BindWithEntity(Entity targetEntity)
{
	if (!targetEntity.HasComponent<SpriteRendererComponent>())
	{
		WHP_CORE_ERROR("[Animation2D] Target entity does not have a SpriteRendererComponent!");
		return;
	}

	auto& spriteRenderer = targetEntity.GetComponent<SpriteRendererComponent>();
	m_OriginalTexture = spriteRenderer.m_Texture;
	m_OriginalTextureSpriteIndex = spriteRenderer.m_TextureSpriteIndex;

	m_TargetEntity = targetEntity;
}

void Animation2D::UnbindFromEntity()
{
	m_TargetEntity = {};
	m_OriginalTexture = {};
	m_OriginalTextureSpriteIndex = -1;
}

void Animation2D::ApplyFrame(const AnimationFrame& frame)
{
	if (!m_TargetEntity)
	{
		WHP_CORE_ERROR("[Animation2D] No entity bound!");
		return;
	}

	auto& spriteRenderer = m_TargetEntity.GetComponent<SpriteRendererComponent>();
	spriteRenderer.m_Texture = frame.m_Texture;
	spriteRenderer.m_TextureSpriteIndex = frame.m_TextureSpriteIndex;
}

void Animation2D::RestoreOriginalFrame()
{
	if (!m_TargetEntity)
		return;

	auto& spriteRenderer = m_TargetEntity.GetComponent<SpriteRendererComponent>();
	spriteRenderer.m_Texture = m_OriginalTexture;
	spriteRenderer.m_TextureSpriteIndex = m_OriginalTextureSpriteIndex;
}

void Animation2D::Play()
{
	if (m_IsPlaying)
		return;

	if (!m_TargetEntity)
	{
		WHP_CORE_ERROR("[Animation2D] No entity bound to this animation!");
		return;
	}

	m_IsPlaying = true;
	m_IsPaused = false;
	m_CurrentFrame = 0;
	m_ElapsedTime = 0.0f;

	if (!m_Frames.empty())
		ApplyFrame(m_Frames[m_CurrentFrame]);
}

void Animation2D::Stop()
{
	if (!m_IsPlaying)
		return;

	m_IsPlaying = false;
	m_IsPaused = false;
	m_CurrentFrame = 0;
	m_ElapsedTime = 0.0f;

	RestoreOriginalFrame();
}

void Animation2D::Pause()
{
	if (!m_IsPlaying || m_IsPaused)
		return;

	m_IsPaused = true;
}

void Animation2D::Resume()
{
	if (!m_IsPlaying || !m_IsPaused)
		return;

	m_IsPaused = false;
}

void Animation2D::SetName(const std::string& newName)
{
	AnimationManager::GetAnimationNameManager().RemoveName(m_Name);
	m_Name = AnimationManager::GetAnimationNameManager().AddName(newName);
}

float Animation2D::GetDuration() const
{
	float duration = 0.0f;
	for (const AnimationFrame& frame : m_Frames)
		duration += std::max(frame.m_Duration, 0.0f);

	for (const AnimationEventKey& eventKey : m_Events)
		duration = std::max(duration, eventKey.m_Time);
	for (const AnimationVec3Key& key : m_TranslationKeys)
		duration = std::max(duration, key.m_Time);
	for (const AnimationVec3Key& key : m_RotationKeys)
		duration = std::max(duration, key.m_Time);
	for (const AnimationVec3Key& key : m_ScaleKeys)
		duration = std::max(duration, key.m_Time);
	for (const AnimationVec4Key& key : m_ColorKeys)
		duration = std::max(duration, key.m_Time);
	return duration;
}

float Animation2D::GetFrameStartTime(size_t index) const
{
	float time = 0.0f;
	const size_t end = std::min(index, m_Frames.size());
	for (size_t i = 0; i < end; ++i)
		time += std::max(m_Frames[i].m_Duration, 0.0f);
	return time;
}

size_t Animation2D::GetFrameIndexAtTime(float time) const
{
	if (m_Frames.empty())
		return 0;

	float cursor = 0.0f;
	const float sampleTime = std::max(time, 0.0f);
	for (size_t i = 0; i < m_Frames.size(); ++i)
	{
		cursor += std::max(m_Frames[i].m_Duration, 0.0f);
		if (sampleTime <= cursor || i == m_Frames.size() - 1)
			return i;
	}

	return m_Frames.size() - 1;
}

void Animation2D::Serialize(const std::filesystem::path& filepath)
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "version" << YAML::Value << FormatVersion;
	out << YAML::Key << "name" << YAML::Value << m_Name;
	out << YAML::Key << "loop" << YAML::Value << m_Loop;
	out << YAML::Key << "frames" << YAML::Value << YAML::BeginSeq;

	for (const auto& frame : m_Frames)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "handle" << YAML::Value << frame.m_Texture;
		out << YAML::Key << "sprite_index" << YAML::Value << frame.m_TextureSpriteIndex;
		out << YAML::Key << "duration" << YAML::Value << frame.m_Duration;
		out << YAML::EndMap;
	}

	out << YAML::EndSeq;
	out << YAML::Key << "events" << YAML::Value << YAML::BeginSeq;
	for (const AnimationEventKey& eventKey : m_Events)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "time" << YAML::Value << eventKey.m_Time;
		out << YAML::Key << "name" << YAML::Value << eventKey.m_Name;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;

	out << YAML::Key << "property_tracks" << YAML::Value << YAML::BeginMap;
	WriteVec3Track(out, "translation", m_TranslationKeys);
	WriteVec3Track(out, "rotation", m_RotationKeys);
	WriteVec3Track(out, "scale", m_ScaleKeys);
	WriteVec4Track(out, "color", m_ColorKeys);
	out << YAML::EndMap;

	out << YAML::EndMap;

	std::ofstream fout(filepath);
	fout << out.c_str();
}

bool Animation2D::Deserialize(const std::filesystem::path& filepath)
{
	YAML::Node data;
	try
	{
		data = YAML::LoadFile(filepath.string());
	}
	catch (YAML::Exception& e)
	{
		WHP_CORE_ERROR("[Animation2D] Failed to load .wanim file '{0}' -> {1}", filepath.string(), e.what());
		return false;
	}

	m_Name = AnimationManager::GetAnimationNameManager().AddName(data["name"].as<std::string>());
	m_Loop = data["loop"].as<bool>();
	m_Frames.clear();
	m_Events.clear();
	m_TranslationKeys.clear();
	m_RotationKeys.clear();
	m_ScaleKeys.clear();
	m_ColorKeys.clear();

	const auto& framesNode = data["frames"];
	for (const auto& frameNode : framesNode)
	{
		AnimationFrame frame;
		frame.m_Texture = frameNode["handle"].as<uint64_t>();
		if (const YAML::Node spriteIndexNode = frameNode["sprite_index"])
			frame.m_TextureSpriteIndex = spriteIndexNode.as<int32_t>(-1);
		frame.m_Duration = frameNode["duration"].as<float>();
		m_Frames.push_back(frame);
	}

	if (const YAML::Node eventsNode = data["events"])
	{
		for (const YAML::Node& eventNode : eventsNode)
		{
			AnimationEventKey eventKey;
			eventKey.m_Time = eventNode["time"].as<float>(0.0f);
			eventKey.m_Name = eventNode["name"].as<std::string>("");
			if (!eventKey.m_Name.empty())
				m_Events.push_back(eventKey);
		}
	}

	if (const YAML::Node propertyTracks = data["property_tracks"])
	{
		ReadVec3Track(propertyTracks["translation"], m_TranslationKeys);
		ReadVec3Track(propertyTracks["rotation"], m_RotationKeys);
		ReadVec3Track(propertyTracks["scale"], m_ScaleKeys);
		ReadVec4Track(propertyTracks["color"], m_ColorKeys);
	}

	return true;
}

_WHIP_END
