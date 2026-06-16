#include "WhipPch.h"
#include <Whip/Audio/AudioSource.h>

#include <Whip/Audio/AudioEngine.h>

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(5030)
#include <AL/al.h>
#include <AL/alext.h>
_WHP_PRAGMA_WARNING(pop)

_WHIP_START

AudioSource::AudioSource(AssetHandle handle) : Asset(handle) {}

AudioSource::AudioSource(uint32_t handle, bool loaded, float length, bool stream)
	: m_BufferHandle(handle), m_Loaded(loaded), m_TotalDuration(length), m_IsStream(stream) {}

AudioSource::~AudioSource()
{
	AudioEngine::UnloadAudioSource(this);
}

void AudioSource::SetPosition(float x, float y, float z)
{
	m_Position[0] = x;
	m_Position[1] = y;
	m_Position[2] = z;

	alSourcefv(m_SourceHandle, AL_POSITION, m_Position);
}

void AudioSource::SetGain(float gain)
{
	m_Gain = gain;

	alSourcef(m_SourceHandle, AL_GAIN, gain);
}

void AudioSource::IncreaseGain(float incrementValue)
{
	m_Gain += incrementValue;

	alSourcef(m_SourceHandle, AL_GAIN, m_Gain);
}

void AudioSource::DecreaseGain(float decrementValue)
{
	if (m_Gain - decrementValue < 0.0f)
		m_Gain = 0.0f;
	else
		m_Gain -= decrementValue;

	alSourcef(m_SourceHandle, AL_GAIN, m_Gain);
}

void AudioSource::SetPitch(float pitch)
{
	m_Pitch = pitch;

	alSourcef(m_SourceHandle, AL_PITCH, pitch);
}

void AudioSource::IncreasePitch(float incrementValue)
{
	if (m_Pitch + incrementValue > 2.0f)
		m_Pitch = 2.0f;
	else
		m_Pitch += incrementValue;

	alSourcef(m_SourceHandle, AL_PITCH, m_Pitch);
}

void AudioSource::DecreasePitch(float decrementValue)
{
	if (m_Pitch - decrementValue < 0.5f)
		m_Pitch = 0.5f;
	else
		m_Pitch -= decrementValue;

	alSourcef(m_SourceHandle, AL_PITCH, m_Pitch);
}

void AudioSource::SetSpitial(bool spitial)
{
	m_Spitial = spitial;
	alSourcei(m_SourceHandle, AL_SOURCE_SPATIALIZE_SOFT, spitial ? AL_TRUE : AL_FALSE);
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
}

void AudioSource::SetLoop(bool loop)
{
	m_Loop = loop;

	alSourcei(m_SourceHandle, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
}

void AudioSource::UpdateSpatialPosition(float x, float y, float z)
{
	ALfloat velocity[] = { x - m_Position[0], y - m_Position[1], z - m_Position[2] };
	SetPosition(x, y, z);
	alSourcefv(m_SourceHandle, AL_VELOCITY, velocity);
}

void AudioSource::GetPosition(float* x, float* y, float* z) const
{
	*x = m_Position[0];
	*y = m_Position[1];
	*z = m_Position[2];
}

float AudioSource::GetCurrentDuration() const
{
	ALfloat duration;
	alGetSourcef(m_SourceHandle, AL_SEC_OFFSET, &duration);
	return static_cast<float>(duration);
}

std::pair<uint32_t, uint32_t> AudioSource::GetLengthMinutesAndSeconds() const
{
	return { static_cast<uint32_t>(m_TotalDuration / 60.0f), static_cast<uint32_t>(m_TotalDuration) % 60 };
}

Ref<AudioSource> AudioSource::LoadFromFile(const std::filesystem::path& filepath, bool spitial)
{
	Ref<AudioSource> result = AudioEngine::LoadAudioSource(filepath);
	result->SetSpitial(spitial);
	return result;
}

_WHIP_END
