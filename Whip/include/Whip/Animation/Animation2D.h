#pragma once

#include "Whip/Core/Core.h"
#include "Whip/Core/UUID.h"
#include "Whip/Asset/Asset.h"
#include "Whip/Scene/Entity.h"

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

_WHIP_START

struct AnimationFrame
{
	AssetHandle m_Texture = 0;
	int32_t m_TextureSpriteIndex = -1;
	float m_Duration = 0.0f;
};

enum class AnimationTrackType : uint8_t
{
	Sprite,
	Event
};

MakeFrenumInNamespace(whip, AnimationTrackType, Sprite, Event)

struct AnimationEventKey
{
	float m_Time = 0.0f;
	std::string m_Name;
};

struct AnimationVec3Key
{
	float m_Time = 0.0f;
	glm::vec3 m_Value{ 0.0f };
};

struct AnimationVec4Key
{
	float m_Time = 0.0f;
	glm::vec4 m_Value{ 1.0f };
};

class Animation2D : public Asset
{
public:
	static constexpr uint32_t FormatVersion = 1;

	Animation2D(AssetHandle handleIn = AssetHandle{});
	~Animation2D() override;

	AssetType GetType() const override { return AssetType::Animation; }

	static Ref<Animation2D> Copy(const Ref<Animation2D>& anim);

	void SetFrames(const std::vector<AnimationFrame>& frames, bool loop);
	void AddFrame(const AnimationFrame& frame);
	void RemoveFrame(size_t index);

	void BindWithEntity(Entity targetEntity);
	void UnbindFromEntity();

	void Play();
	void Stop();
	void Pause();
	void Resume();

	void SetName(const std::string& newName);
	const std::string& GetName() const { return m_Name; }

	std::vector<AnimationFrame>& GetFrames() { return m_Frames; }
	const std::vector<AnimationFrame>& GetFrames() const { return m_Frames; }
	std::vector<AnimationEventKey>& GetEvents() { return m_Events; }
	const std::vector<AnimationEventKey>& GetEvents() const { return m_Events; }
	std::vector<AnimationVec3Key>& GetTranslationKeys() { return m_TranslationKeys; }
	const std::vector<AnimationVec3Key>& GetTranslationKeys() const { return m_TranslationKeys; }
	std::vector<AnimationVec3Key>& GetRotationKeys() { return m_RotationKeys; }
	const std::vector<AnimationVec3Key>& GetRotationKeys() const { return m_RotationKeys; }
	std::vector<AnimationVec3Key>& GetScaleKeys() { return m_ScaleKeys; }
	const std::vector<AnimationVec3Key>& GetScaleKeys() const { return m_ScaleKeys; }
	std::vector<AnimationVec4Key>& GetColorKeys() { return m_ColorKeys; }
	const std::vector<AnimationVec4Key>& GetColorKeys() const { return m_ColorKeys; }
	float GetDuration() const;
	float GetFrameStartTime(size_t index) const;
	size_t GetFrameIndexAtTime(float time) const;

	bool IsPlaying() const { return m_IsPlaying; }
	bool IsPaused() const { return m_IsPaused; }
	bool IsLooping() const { return m_Loop; }

	void SetLoop(bool loop) { m_Loop = loop; }

	void Serialize(const std::filesystem::path& filepath);
	bool Deserialize(const std::filesystem::path& filepath);
private:
	void ApplyFrame(const AnimationFrame& frame);
	void RestoreOriginalFrame();

	std::vector<AnimationFrame> m_Frames;
	std::vector<AnimationEventKey> m_Events;
	std::vector<AnimationVec3Key> m_TranslationKeys;
	std::vector<AnimationVec3Key> m_RotationKeys;
	std::vector<AnimationVec3Key> m_ScaleKeys;
	std::vector<AnimationVec4Key> m_ColorKeys;

	Entity m_TargetEntity;
	AssetHandle m_OriginalTexture = AssetHandle(0);
	int32_t m_OriginalTextureSpriteIndex = -1;

	bool m_Loop = false;
	bool m_IsPlaying = false;
	bool m_IsPaused = false;
	float m_ElapsedTime = 0.0f;
	size_t m_CurrentFrame = 0;

	std::string m_Name;

	friend class AnimationManager;
};

_WHIP_END
