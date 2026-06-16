#pragma once

#include "AudioSource.h"
#include <Whip/Core/Memory.h>

#include <functional>

_WHIP_START

class AudioEngine
{
public:
	enum class AudioState
	{
		None,
		Stopped,
		Playing,
		Paused
	};

	enum class Effect
	{
		None = 0,
		Reverb,
		Echo,
		Chorus,
		Distortion,
		Flanger,
		Equalizer,
		FrequencyShifter,
		Autowah,
		RingModulator
	};

	enum class Filter
	{
		None = 0,
		LowPassFilter,
		HighPassFilter
	};
public:
	static void Init();
	static void Shutdown();
	static Ref<AudioSource> LoadAudioSource(const std::filesystem::path& filepath, AssetHandle handle = AssetHandle{});
	static Ref<AudioSource> LoadAudioStream(const std::filesystem::path& filepath);
	static void UnloadAudioSource(AudioSource* source);
	static void UnloadAudioSource(Ref<AudioSource>& source);

	static void Play(const Ref<AudioSource>& source);
	static void Stop(const Ref<AudioSource>& source);
	static void Pause(const Ref<AudioSource>& source);
	static void Rewind(const Ref<AudioSource>& source);
	static void Seek(const Ref<AudioSource>& source, float seconds);

	static void SetListenerPosition(float x, float y, float z);
	static void SetListenerOrientation(float atX, float atY, float atZ, float upX, float upY, float upZ);
	static void SetListenerVelocity(float x, float y, float z);

	static void SetDopplerFactor(float factor);
	static void SetSpeedOfSound(float speed);

	// effects
	static void ApplyReverb(const Ref<AudioSource>& source, float decayTime, float density);
	static void ApplyEcho(const Ref<AudioSource>& source, float delay, float damping);
	static void ApplyChorus(const Ref<AudioSource>& source, float rate, float depth, float feedback);
	static void ApplyDistortion(const Ref<AudioSource>& source, float edge, float gain, float lowpassCutoff);
	static void ApplyFlanger(const Ref<AudioSource>& source, float rate, float depth, float feedback);
	static void ApplyEqualizer(const Ref<AudioSource>& source, float lowGain, float midGain, float highGain);
	static void ApplyFrequencyShifter(const Ref<AudioSource>& source, float frequency, int direction);
	static void ApplyAutowah(const Ref<AudioSource>& source, float attackTime, float releaseTime, float resonance);
	static void ApplyRingModulator(const Ref<AudioSource>& source, float frequency, float highpassCutoff);
	static void RemoveEffect(const Ref<AudioSource>& source, Effect type);

	// filters
	static void ApplyLowPassFilter(const Ref<AudioSource>& source, float gain, float gainHF);
	static void ApplyHighPassFilter(const Ref<AudioSource>& source, float gain, float gainLF);
	static void RemoveFilter(const Ref<AudioSource>& source, Filter type);

	static AudioState GetState(const Ref<AudioSource>& source);

	static void SetDebugLogState(bool state);
private:
	struct AudioData
	{
		int m_AlFormat = 0;
		RawBuffer m_Buffer;
		int m_SampleRate = 0;
		float m_TrackLength = 0.0f;

		bool m_IsNull = true;
	};
	static AudioData LoadAudioDataOgg(const std::filesystem::path& filepath);
	static AudioData LoadAudioDataMp3(const std::filesystem::path& filepath);
	static AudioData LoadAudioDataWav(const std::filesystem::path& filepath);

	static Ref<AudioSource> LoadAudioSourceAL(AudioData& data, AssetHandle handle);

	static Ref<AudioSource> LoadAudioSourceOgg(const std::filesystem::path& filepath, AssetHandle handle);
	static Ref<AudioSource> LoadAudioSourceMp3(const std::filesystem::path& filepath, AssetHandle handle);
	static Ref<AudioSource> LoadAudioSourceWav(const std::filesystem::path& filepath, AssetHandle handle);
};


_WHIP_END
