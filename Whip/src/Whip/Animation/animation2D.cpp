#include <WhipPch.h>
#include <Whip/Animation/Animation2D.h>
#include <Whip/Animation/AnimationManager.h>
#include <Whip/Scene/Components.h>

#ifndef YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC_DEFINE
#endif
#include <yaml-cpp/yaml.h>

namespace YAML
{
	template<>
	struct convert<whip::AssetHandle>
	{
		static Node encode(const whip::AssetHandle& handle)
		{
			Node node;
			node.push_back((uint64_t)handle);
			return node;
		}

		static bool decode(const Node& node, whip::AssetHandle& handle)
		{
			handle = node.as<uint64_t>();
			return true;
		}
	};
}

_WHIP_START

Animation2D::Animation2D(AssetHandle handleIn) : Asset(handleIn), m_Frames() { }

Animation2D::~Animation2D()
{
	//AnimationManager::GetAnimationNameManager().RemoveName(m_Name);
}

Ref<Animation2D> Animation2D::Copy(Ref<Animation2D> anim)
{
	auto newAnimation = MakeRef<Animation2D>();
	newAnimation->m_Frames = anim->m_Frames;
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
		m_Frames.erase(m_Frames.begin() + index);
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

	m_TargetEntity = targetEntity;
}

void Animation2D::UnbindFromEntity()
{
	m_TargetEntity = {};
	m_OriginalTexture = {};
}

void Animation2D::ApplyFrame(AssetHandle texture)
{
	if (!m_TargetEntity)
	{
		WHP_CORE_ERROR("[Animation2D] No entity bound!");
		return;
	}

	auto& spriteRenderer = m_TargetEntity.GetComponent<SpriteRendererComponent>();
	spriteRenderer.m_Texture = texture;
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
		ApplyFrame(m_Frames[m_CurrentFrame].m_Texture);
}

void Animation2D::Stop()
{
	if (!m_IsPlaying)
		return;

	m_IsPlaying = false;
	m_IsPaused = false;
	m_CurrentFrame = 0;
	m_ElapsedTime = 0.0f;

	ApplyFrame(m_OriginalTexture);
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

void Animation2D::Serialize(const std::filesystem::path& filepath)
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "name" << YAML::Value << m_Name;
	out << YAML::Key << "loop" << YAML::Value << m_Loop;
	out << YAML::Key << "frames" << YAML::Value << YAML::BeginSeq;

	for (const auto& frame : m_Frames)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "handle" << YAML::Value << frame.m_Texture;
		out << YAML::Key << "duration" << YAML::Value << frame.m_Duration;
		out << YAML::EndMap;
	}

	out << YAML::EndSeq;
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

	const auto& framesNode = data["frames"];
	for (const auto& frameNode : framesNode)
	{
		AnimationFrame frame;
		frame.m_Texture = frameNode["handle"].as<uint64_t>();
		frame.m_Duration = frameNode["duration"].as<float>();
		m_Frames.push_back(frame);
	}
	return true;
}

_WHIP_END
