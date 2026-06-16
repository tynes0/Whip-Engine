#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>

#include <Whip/Asset/Asset.h>

#include <utility>
#include <filesystem>

_WHIP_START

class AudioSource : public Asset
{
public:
	AudioSource(AssetHandle handle);
	AudioSource() = default;
	AudioSource(const AudioSource&) = default;
	AudioSource(AudioSource&&) = default;
	AudioSource& operator=(const AudioSource&) = default;
	~AudioSource();

	bool IsLoaded() const { return m_Loaded; }

	void SetPosition(float x, float y, float z);
	void SetGain(float gain);
	void IncreaseGain(float incrementValue);
	void DecreaseGain(float decrementValue);
	void SetPitch(float pitch);
	void IncreasePitch(float incrementValue);
	void DecreasePitch(float decrementValue);
	void SetSpitial(bool spitial);
	void SetLoop(bool loop);

	void UpdateSpatialPosition(float x, float y, float z);

	void GetPosition(float* x, float* y, float* z) const;
	float GetCurrentDuration() const;
	float GetGain() const { return m_Gain; }
	float GetPitch() const { return m_Pitch; }
	float GetLength() const { return m_TotalDuration; }
	bool IsSpitial() const { return m_Spitial; }
	bool IsLoop() const { return m_Loop; }
	bool IsStreaming() const { return m_IsStream; }

	std::pair<uint32_t, uint32_t> GetLengthMinutesAndSeconds() const;

	static Ref<AudioSource> LoadFromFile(const std::filesystem::path& filepath, bool spitial = false);

	AssetType GetType() const override { return AssetType::Audio; }
private:
	AudioSource(uint32_t handle, bool loaded, float length, bool stream = false);

	uint32_t m_BufferHandle = 0;
	uint32_t m_SourceHandle = 0;

	float m_Position[3] = { 0.0f, 0.0f, 0.0f };
	bool m_Loaded = false;
	bool m_Spitial = false;
	bool m_IsStream = false; // not used for now

	float m_TotalDuration = 0.0f;

	float m_Gain = 1.0f;
	float m_Pitch = 1.0f;
	bool m_Loop = false;

	friend class AudioEngine;
	friend class AudioImporter;
};

_WHIP_END
