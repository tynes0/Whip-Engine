#include "WhipPch.h"
#include <Whip/Audio/AudioEngine.h>

#include <Whip/Helper/Buffer.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <unordered_map>

#define AL_ALEXT_PROTOTYPES
_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(5030)
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
_WHP_PRAGMA_WARNING(pop)

#include <alhelpers.h>

#include <minimp3.h>
#include <minimp3_ex.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <miniaudio.h>

_WHIP_START

enum class AudioFileFormat
{
	None = 0,
	Ogg,
	Mp3,
	Wav
};

struct EffectData
{
	ALuint m_Slot;
	ALuint m_EffectHandle;
	AudioEngine::Effect m_Type;
};

struct FilterData
{
	ALuint m_FilterHandle;
	AudioEngine::Filter m_Type;
};

struct GlobalAudioData
{
	ALCdevice* m_AudioDevice = nullptr;
	mp3dec_t m_Mp3Decoder{};

	RawBuffer m_AudioScratchBuffer;
	uint32_t m_AudioScratchBufferSize = 10 * 1024 * 1024; // 10mb initially

	std::unordered_map<ALuint, EffectData> m_EffectDatas;
	std::unordered_map<ALuint, FilterData> m_FilterDatas;

	bool m_DebugLog = true;
	bool m_Initialized = false;
};

static GlobalAudioData s_Data;

static bool CheckNull(const Ref<AudioSource>& block)
{
	if (!block)
	{
		WHP_CORE_ERROR("[Audio Engine] null audio source passed to audio engine!");
		return false;
	}
	return true;
}

namespace detail
{
	static AudioFileFormat GetFileFormat(const std::filesystem::path& filepath)
	{
		std::string extension = filepath.extension().string();
		if (extension == ".ogg") return AudioFileFormat::Ogg;
		if (extension == ".mp3") return AudioFileFormat::Mp3;
		if (extension == ".wav") return AudioFileFormat::Wav;

		return AudioFileFormat::None;
	}

	static ALenum GetOpenALFormat(uint32_t channels)
	{
		switch (channels)
		{
		case 1: return AL_FORMAT_MONO16;
		case 2: return AL_FORMAT_STEREO16;
		}
		WHP_CORE_ASSERT(false, "[Audio Engine] Unknown audio format!");
		return 0;
	}

	static void PrintAudioDeviceInfo()
	{
		if (!s_Data.m_DebugLog)
			return;

		const char* deviceName = alcGetString(s_Data.m_AudioDevice, ALC_DEVICE_SPECIFIER);
		ALCint sampleRate = 0;
		ALCint monoSources = 0;
		ALCint stereoSources = 0;

		alcGetIntegerv(s_Data.m_AudioDevice, ALC_FREQUENCY, 1, &sampleRate);
		alcGetIntegerv(s_Data.m_AudioDevice, ALC_MONO_SOURCES, 1, &monoSources);
		alcGetIntegerv(s_Data.m_AudioDevice, ALC_STEREO_SOURCES, 1, &stereoSources);

		WHP_CORE_DEBUG("[Audio Engine] Audio Device Info:");
		WHP_CORE_DEBUG("[Audio Engine] Name: {}", deviceName ? deviceName : "Unknown");
		WHP_CORE_DEBUG("[Audio Engine] Sample Rate: {}", sampleRate);
		WHP_CORE_DEBUG("[Audio Engine] Mono Sources: {}", monoSources);
		WHP_CORE_DEBUG("[Audio Engine] Stereo Sources: {}", stereoSources);
	}

	static long VorbisRead(void* vorbisFile, char* buffer, size_t size)
	{
		int currentSection;
		return ov_read(static_cast<OggVorbis_File*>(vorbisFile), buffer, static_cast<int>(size), 0, 2, 1, &currentSection);
	}

	static long Mp3Read(mp3dec_file_info_t* mp3Info, char* buffer, size_t size)
	{
		static size_t position = 0;
		size_t toCopy = std::min(size, mp3Info->samples - position);
		memcpy(buffer, mp3Info->buffer + position, toCopy);
		position += toCopy;
		return static_cast<long>(toCopy);
	}

	static long WavRead(ma_decoder* decoder, char* buffer, size_t size)
	{
		ma_uint64 framesRead = 0;
		ma_decoder_read_pcm_frames(decoder, buffer, size / sizeof(ma_int16), &framesRead);
		return static_cast<long>(framesRead * sizeof(ma_int16));
	}
}

void AudioEngine::Init()
{
	WHP_PROFILE_FUNCTION();
	if (s_Data.m_Initialized)
		return;

	if (InitAL(s_Data.m_AudioDevice, nullptr, 0) != 0)
	{
		WHP_CORE_ERROR("[Audio Engine] Audio device error!");
		return;
	}

	s_Data.m_Initialized = true;

	detail::PrintAudioDeviceInfo();

	mp3dec_init(&s_Data.m_Mp3Decoder);

	s_Data.m_AudioScratchBuffer.Allocate(s_Data.m_AudioScratchBufferSize);

	ALfloat listenerPos[] = { 0.0, 0.0, 0.0 };
	ALfloat listenerVel[] = { 0.0, 0.0, 0.0 };
	ALfloat listenerOri[] = { 0.0, 0.0, -1.0, 0.0, 1.0, 0.0};
	alListenerfv(AL_POSITION, listenerPos);
	alListenerfv(AL_VELOCITY, listenerVel);
	alListenerfv(AL_ORIENTATION, listenerOri);
}

void AudioEngine::Shutdown()
{
	WHP_PROFILE_FUNCTION();
	if (!s_Data.m_Initialized)
		return;

	for (auto& [slot, effect] : s_Data.m_EffectDatas)
	{
		ALuint effectSlot = effect.m_Slot;
		ALuint effectHandle = effect.m_EffectHandle;
		if (effectSlot != 0)
			alDeleteAuxiliaryEffectSlots(1, &effectSlot);
		if (effectHandle != 0)
			alDeleteEffects(1, &effectHandle);
	}
	s_Data.m_EffectDatas.clear();

	for (auto& [filter, filterEntry] : s_Data.m_FilterDatas)
	{
		ALuint filterHandle = filterEntry.m_FilterHandle;
		if (filterHandle != 0)
			alDeleteFilters(1, &filterHandle);
	}
	s_Data.m_FilterDatas.clear();

	s_Data.m_AudioScratchBuffer.Release();
	CloseAL();
	s_Data.m_AudioDevice = nullptr;
	s_Data.m_Initialized = false;
}

Ref<AudioSource> AudioEngine::LoadAudioSource(const std::filesystem::path& filepath, AssetHandle handle)
{
	WHP_PROFILE_FUNCTION();
	AudioFileFormat format = detail::GetFileFormat(filepath);
	switch (format)
	{
	case whip::AudioFileFormat::Ogg: return LoadAudioSourceOgg(filepath, handle);
	case whip::AudioFileFormat::Mp3: return LoadAudioSourceMp3(filepath, handle);
	case whip::AudioFileFormat::Wav: return LoadAudioSourceWav(filepath, handle);
	}
	return nullptr;
}

Ref<AudioSource> AudioEngine::LoadAudioStream(const std::filesystem::path& filepath)
{
	WHP_CORE_ASSERT(false, "UNIMPLAMENTED!");
	return nullptr;
}

void AudioEngine::UnloadAudioSource(AudioSource* source)
{
	if (source == NULL)
	{
		WHP_CORE_ERROR("[Audio Engine] null audio source passed to audio engine!");
		return;
	}

	if (!s_Data.m_Initialized)
	{
		source->m_SourceHandle = 0;
		source->m_BufferHandle = 0;
		source->m_Loaded = false;
		source->m_TotalDuration = 0.0f;
		return;
	}

	if (source->m_SourceHandle != 0)
	{
		alSourceStop(source->m_SourceHandle);
		alSourcei(source->m_SourceHandle, AL_BUFFER, 0);

		ALuint buffer = source->m_BufferHandle;
		if (buffer != 0)
			alDeleteBuffers(1, &buffer);

		alDeleteSources(1, &source->m_SourceHandle);

		source->m_SourceHandle = 0;
		source->m_BufferHandle = 0;
		source->m_Loaded = false;
		source->m_TotalDuration = 0.0f;

		if (alGetError() != AL_NO_ERROR)
			WHP_CORE_ERROR("[Audio Engine] Failed to unload audio source.");
	}
}

void AudioEngine::UnloadAudioSource(Ref<AudioSource>& source)
{
	UnloadAudioSource(source.get());
	source.reset();
}

void AudioEngine::Play(const Ref<AudioSource>& source)
{
	if (!CheckNull(source))
		return;
	alSourcePlay(source->m_SourceHandle);
}

void AudioEngine::Stop(const Ref<AudioSource>& source)
{
	if (!CheckNull(source))
		return;
	alSourceStop(source->m_SourceHandle);
}

void AudioEngine::Pause(const Ref<AudioSource>& source)
{
	if (!CheckNull(source))
		return;
	alSourcePause(source->m_SourceHandle);
}

void AudioEngine::Rewind(const Ref<AudioSource>& source)
{
	if (!CheckNull(source))
		return;
	ALint state = AL_STOPPED;
	alGetSourcei(source->m_SourceHandle, AL_SOURCE_STATE, &state);
	alSourceStop(source->m_SourceHandle);
	if (!source->IsStreaming())
		alSourcef(source->m_SourceHandle, AL_SEC_OFFSET, 0.0f);
	alSourceRewind(source->m_SourceHandle);
	if (state == AL_PLAYING)
		alSourcePlay(source->m_SourceHandle);
}

void AudioEngine::Seek(const Ref<AudioSource>& source, float seconds)
{
	if (!CheckNull(source))
		return;
	if (source->IsStreaming())
	{
		WHP_CORE_WARN("[Audio Engine] Seeking is not available while streaming!");
		return;
	}
	if (seconds < 0 || seconds > source->m_TotalDuration)
	{
		WHP_CORE_ERROR("[Audio Engine] Seek position out of range: {}", seconds);
		return;
	}
	alSourcef(source->m_SourceHandle, AL_SEC_OFFSET, seconds);
}

void AudioEngine::SetListenerPosition(float x, float y, float z)
{
	ALfloat listenerPos[] = { x, y, z };
	alListenerfv(AL_POSITION, listenerPos);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set listener position!");
}

void AudioEngine::SetListenerOrientation(float atX, float atY, float atZ, float upX, float upY, float upZ)
{
	ALfloat listenerOri[] = { atX, atY, atZ, upX, upY, upZ };
	alListenerfv(AL_ORIENTATION, listenerOri);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set listener orientation!");
}

void AudioEngine::SetListenerVelocity(float x, float y, float z)
{
	ALfloat listenerVel[] = { x, y, z };
	alListenerfv(AL_VELOCITY, listenerVel);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set listener velocity!");
}

void AudioEngine::SetDopplerFactor(float factor)
{
	alDopplerFactor(factor);
	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set Doppler factor!");
}

void AudioEngine::SetSpeedOfSound(float speed)
{
	alSpeedOfSound(speed);
	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set speed of sound!");
}

void AudioEngine::ApplyReverb(const Ref<AudioSource>& source, float decayTime, float density)
{
	if (!CheckNull(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
	alEffectf(effect, AL_REVERB_DECAY_TIME, decayTime);
	alEffectf(effect, AL_REVERB_DENSITY, density);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply reverb!");
	else
		s_Data.m_EffectDatas[source->m_SourceHandle] = { effectSlot, effect, Effect::Reverb };
}

void AudioEngine::ApplyEcho(const Ref<AudioSource>& source, float delay, float damping)
{
	if (!CheckNull(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_ECHO);
	alEffectf(effect, AL_ECHO_DELAY, delay);
	alEffectf(effect, AL_ECHO_DAMPING, damping);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Error attaching echo effect to source!");
	else
		s_Data.m_EffectDatas[source->m_SourceHandle] = { effectSlot, effect, Effect::Echo };
}

void AudioEngine::ApplyChorus(const Ref<AudioSource>& source, float rate, float depth, float feedback)
{
	if (!CheckNull(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);

	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_CHORUS);
	alEffectf(effect, AL_CHORUS_RATE, rate);
	alEffectf(effect, AL_CHORUS_DEPTH, depth);
	alEffectf(effect, AL_CHORUS_FEEDBACK, feedback);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply chorus effect!");
	else
		s_Data.m_EffectDatas[source->m_SourceHandle] = { effectSlot, effect, Effect::Chorus };
}

void AudioEngine::ApplyDistortion(const Ref<AudioSource>& source, float edge, float gain, float lowpassCutoff)
{
	if (!CheckNull(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_DISTORTION);
	alEffectf(effect, AL_DISTORTION_EDGE, edge);
	alEffectf(effect, AL_DISTORTION_GAIN, gain);
	alEffectf(effect, AL_DISTORTION_LOWPASS_CUTOFF, lowpassCutoff);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply distortion effect!");
	else
		s_Data.m_EffectDatas[source->m_SourceHandle] = { effectSlot, effect, Effect::Distortion };
}

void AudioEngine::ApplyFlanger(const Ref<AudioSource>& source, float rate, float depth, float feedback)
{
	if (!CheckNull(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_FLANGER);
	alEffectf(effect, AL_FLANGER_RATE, rate);
	alEffectf(effect, AL_FLANGER_DEPTH, depth);
	alEffectf(effect, AL_FLANGER_FEEDBACK, feedback);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply flanger effect!");
	else
		s_Data.m_EffectDatas[source->m_SourceHandle] = { effectSlot, effect, Effect::Flanger };
}

void AudioEngine::ApplyEqualizer(const Ref<AudioSource>& source, float lowGain, float midGain, float highGain)
{
	if (!CheckNull(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_EQUALIZER);
	alEffectf(effect, AL_EQUALIZER_LOW_GAIN, lowGain);
	alEffectf(effect, AL_EQUALIZER_MID1_GAIN, midGain);
	alEffectf(effect, AL_EQUALIZER_HIGH_GAIN, highGain);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply equalizer effect!");
	else
		s_Data.m_EffectDatas[source->m_SourceHandle] = { effectSlot, effect, Effect::Equalizer };
}

void AudioEngine::ApplyFrequencyShifter(const Ref<AudioSource>& source, float frequency, int direction)
{
	if (!CheckNull(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_FREQUENCY_SHIFTER);
	alEffectf(effect, AL_FREQUENCY_SHIFTER_FREQUENCY, frequency);
	alEffecti(effect, AL_FREQUENCY_SHIFTER_LEFT_DIRECTION, direction);
	alEffecti(effect, AL_FREQUENCY_SHIFTER_RIGHT_DIRECTION, direction);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply frequency shifter effect!");
	else
		s_Data.m_EffectDatas[source->m_SourceHandle] = { effectSlot, effect, Effect::FrequencyShifter };
}

void AudioEngine::ApplyAutowah(const Ref<AudioSource>& source, float attackTime, float releaseTime, float resonance)
{
	if (!CheckNull(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_AUTOWAH);
	alEffectf(effect, AL_AUTOWAH_ATTACK_TIME, attackTime);
	alEffectf(effect, AL_AUTOWAH_RELEASE_TIME, releaseTime);
	alEffectf(effect, AL_AUTOWAH_RESONANCE, resonance);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply autowah effect!");
	else
		s_Data.m_EffectDatas[source->m_SourceHandle] = { effectSlot, effect, Effect::Autowah };
}

void AudioEngine::ApplyRingModulator(const Ref<AudioSource>& source, float frequency, float highpassCutoff)
{
	if (!CheckNull(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_RING_MODULATOR);
	alEffectf(effect, AL_RING_MODULATOR_FREQUENCY, frequency);
	alEffectf(effect, AL_RING_MODULATOR_HIGHPASS_CUTOFF, highpassCutoff);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply ring modulator effect!");
	else
		s_Data.m_EffectDatas[source->m_SourceHandle] = { effectSlot, effect, Effect::RingModulator };
}

void AudioEngine::RemoveEffect(const Ref<AudioSource>& source, Effect type)
{
	if (!CheckNull(source))
		return;
	auto it = s_Data.m_EffectDatas.find(source->m_SourceHandle);
	if (it != s_Data.m_EffectDatas.end())
	{
		if (it->second.m_Type == type)
		{
			ALuint effectSlot = it->second.m_Slot;
			ALuint effect = it->second.m_EffectHandle;

			alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
			alDeleteEffects(1, &effect);
			alDeleteAuxiliaryEffectSlots(1, &effectSlot);

			s_Data.m_EffectDatas.erase(it);
		}
	}
}

void AudioEngine::ApplyLowPassFilter(const Ref<AudioSource>& source, float gain, float gainHF)
{
	if (!CheckNull(source))
		return;
	ALuint filter;
	alGenFilters(1, &filter);
	alFilteri(filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
	alFilterf(filter, AL_LOWPASS_GAIN, gain);
	alFilterf(filter, AL_LOWPASS_GAINHF, gainHF);
	alSourcei(source->m_SourceHandle, AL_DIRECT_FILTER, filter);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Error attaching low-pass filter to source!");
	else
		s_Data.m_FilterDatas[source->m_SourceHandle] = { filter, Filter::LowPassFilter };
}

void AudioEngine::ApplyHighPassFilter(const Ref<AudioSource>& source, float gain, float gainLF)
{
	if (!CheckNull(source))
		return;
	ALuint filter;
	alGenFilters(1, &filter);
	alFilteri(filter, AL_FILTER_TYPE, AL_FILTER_HIGHPASS);
	alFilterf(filter, AL_HIGHPASS_GAIN, gain);
	alFilterf(filter, AL_HIGHPASS_GAINLF, gainLF);
	alSourcei(source->m_SourceHandle, AL_DIRECT_FILTER, filter);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply high-pass filter!");
	else
		s_Data.m_FilterDatas[source->m_SourceHandle] = { filter, Filter::HighPassFilter };
}

void AudioEngine::RemoveFilter(const Ref<AudioSource>& source, Filter type)
{
	if (!CheckNull(source))
		return;
	auto it = s_Data.m_FilterDatas.find(source->m_SourceHandle);
	if (it != s_Data.m_FilterDatas.end())
	{
		if (it->second.m_Type == type)
		{
			ALuint filtr = it->second.m_FilterHandle;

			alSource3i(source->m_SourceHandle, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
			alDeleteFilters(1, &filtr);

			s_Data.m_FilterDatas.erase(it);
		}
	}
}

AudioEngine::AudioState AudioEngine::GetState(const Ref<AudioSource>& source)
{
	if (!CheckNull(source))
		return AudioState::None;
	ALint state;
	alGetSourcei(source->m_SourceHandle, AL_SOURCE_STATE, &state);
	switch (state)
	{
	case AL_STOPPED:	return AudioState::Stopped;
	case AL_PLAYING:	return AudioState::Playing;
	case AL_PAUSED:		return AudioState::Paused;
	default:			return AudioState::None;
	}
}

void AudioEngine::SetDebugLogState(bool state)
{
	s_Data.m_DebugLog = state;
}

AudioEngine::AudioData AudioEngine::LoadAudioDataOgg(const std::filesystem::path& filepath)
{
	WHP_PROFILE_FUNCTION();
	FILE* file = fopen(filepath.string().c_str(), "rb");
	OggVorbis_File vorbisFile;
	if (ov_open_callbacks(file, &vorbisFile, NULL, 0, OV_CALLBACKS_NOCLOSE) < 0)
	{
		WHP_CORE_ERROR("[Audio Engine] Could not open ogg stream! (The file doesn't use the vorbis codec.)");
		return AudioData();
	}
	vorbis_info* vbInfo = ov_info(&vorbisFile, -1);
	long sampleRate = vbInfo->rate;
	int channels = vbInfo->channels;
	uint64_t samples = ov_pcm_total(&vorbisFile, -1);
	float trackLength = (float)samples / (float)sampleRate;
	uint32_t bufferSize = static_cast<uint32_t>(2 * static_cast<unsigned long long>(channels) * samples);

	if (s_Data.m_AudioScratchBufferSize < bufferSize)
	{
		s_Data.m_AudioScratchBufferSize = bufferSize;
		s_Data.m_AudioScratchBuffer.Release();
		s_Data.m_AudioScratchBuffer.Allocate(s_Data.m_AudioScratchBufferSize);
	}

	RawBuffer oggBuffer = s_Data.m_AudioScratchBuffer;
	RawBuffer bufferPtr = oggBuffer;
	int eof = 0;
	while (!eof)
	{
		int currentSection;
		long length = ov_read(&vorbisFile, bufferPtr.As<char>(), 4096, 0, 2, 1, &currentSection);
		bufferPtr.m_Data += length;
		if (length == 0)
			eof = 1;
		else if (length < 0)
		{
			WHP_CORE_ASSERT(length != OV_EBADLINK, "[Audio Engine] Corrupt bitstream section!");
			ov_clear(&vorbisFile);
			fclose(file);
			return AudioData();
		}
	}

	uint32_t size = static_cast<uint32_t>(bufferPtr - oggBuffer);
	oggBuffer.m_Size = size;
	WHP_CORE_ASSERT(bufferSize == size, "[Audio Engine] The bitstream was not read to the expected size.");

	ALenum alFormat = detail::GetOpenALFormat(channels);

	AudioData data
	{
		alFormat,
		oggBuffer,
		sampleRate,
		trackLength,
		false
	};

	return data;
}

AudioEngine::AudioData AudioEngine::LoadAudioDataMp3(const std::filesystem::path& filepath)
{
	WHP_PROFILE_FUNCTION();
	mp3dec_file_info_t info;
	int loadResult = mp3dec_load(&s_Data.m_Mp3Decoder, filepath.string().c_str(), &info, NULL, NULL);
	uint32_t size = static_cast<uint32_t>(info.samples * sizeof(mp3d_sample_t));
	int sampleRate = info.hz;
	int channels = info.channels;
	float lengthSeconds = size / (info.avg_bitrate_kbps * 1024.0f);

	ALenum alFormat = detail::GetOpenALFormat(channels);

	AudioData data
	{
		alFormat,
		RawBuffer(info.buffer, size),
		sampleRate,
		lengthSeconds,
		false
	};

	return data;
}

AudioEngine::AudioData AudioEngine::LoadAudioDataWav(const std::filesystem::path& filepath)
{
	WHP_PROFILE_FUNCTION();
	ma_result result;
	ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 2, 44100);
	ma_decoder decoder;

	result = ma_decoder_init_file(filepath.string().c_str(), &config, &decoder);
	if (result != MA_SUCCESS)
	{
		WHP_CORE_ERROR("[Audio Engine] Failed to load WAV file: {}", filepath.string());
		return AudioData();
	}

	ma_uint64 totalFrames = 0;
	result = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
	if (result != MA_SUCCESS)
	{
		WHP_CORE_ERROR("[Audio Engine] Failed to get the length of the WAV file.");
		ma_decoder_uninit(&decoder);
		return AudioData();
	}

	size_t dataSize = decoder.outputChannels * totalFrames * sizeof(ma_int16);
	RawBuffer buffer(dataSize);
	size_t framesRead = 0;
	result = ma_decoder_read_pcm_frames(&decoder, buffer.As<void>(), totalFrames, &framesRead);
	if (result != MA_SUCCESS)
	{
		WHP_CORE_ERROR("[Audio Engine] Failed to read WAV frames.");
		ma_decoder_uninit(&decoder);
		buffer.Release();
		return AudioData();
	}

	ALenum alFormat = detail::GetOpenALFormat(decoder.outputChannels);

	AudioData data
	{
		alFormat,
		buffer,
		static_cast<int>(decoder.outputSampleRate),
		(static_cast<float>(totalFrames) / static_cast<float>(decoder.outputSampleRate)),
		false
	};

	return data;
}

Ref<AudioSource> AudioEngine::LoadAudioSourceAL(AudioData& data, AssetHandle handle)
{
	WHP_PROFILE_FUNCTION();
	ALuint buffer;
	alGenBuffers(1, &buffer);
	alBufferData(buffer, static_cast<ALenum>(data.m_AlFormat), data.m_Buffer.As<ALvoid>(), static_cast<ALsizei>(data.m_Buffer.m_Size), static_cast<ALsizei>(data.m_SampleRate));

	Ref<AudioSource> resultSource = MakeRef<AudioSource>(handle);
	resultSource->m_BufferHandle = static_cast<uint32_t>(buffer);
	resultSource->m_Loaded = true;
	resultSource->m_TotalDuration = data.m_TrackLength;

	alGenSources(1, &resultSource->m_SourceHandle);
	alSourcei(resultSource->m_SourceHandle, AL_BUFFER, buffer);

	data.m_Buffer.Release();

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to setup sound source!");

	return resultSource;
}

Ref<AudioSource> AudioEngine::LoadAudioSourceOgg(const std::filesystem::path& filepath, AssetHandle handle)
{
	WHP_PROFILE_FUNCTION();
	AudioData data = LoadAudioDataOgg(filepath);
	if (data.m_IsNull)
	{
		WHP_CORE_ERROR("[Audio Engine] Error while loading audio source!");
		return nullptr;
	}
	return LoadAudioSourceAL(data, handle);
}

Ref<AudioSource> AudioEngine::LoadAudioSourceMp3(const std::filesystem::path& filepath, AssetHandle handle)
{
	WHP_PROFILE_FUNCTION();
	AudioData data = LoadAudioDataMp3(filepath);
	if (data.m_IsNull)
	{
		WHP_CORE_ERROR("[Audio Engine] Error while loading audio source!");
		return nullptr;
	}
	return LoadAudioSourceAL(data, handle);
}

Ref<AudioSource> AudioEngine::LoadAudioSourceWav(const std::filesystem::path& filepath, AssetHandle handle)
{
	WHP_PROFILE_FUNCTION();
	AudioData data = LoadAudioDataWav(filepath);
	if (data.m_IsNull)
	{
		WHP_CORE_ERROR("[Audio Engine] Error while loading audio source!");
		return nullptr;
	}
	return LoadAudioSourceAL(data, handle);
}

_WHIP_END
