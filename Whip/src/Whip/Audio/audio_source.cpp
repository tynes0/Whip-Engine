#include "whippch.h"
#include "audio_source.h"

#include "audio_engine.h"

_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(5030)
#include <AL/al.h>
#include <AL/alext.h>
_WHP_PRAGMA_WARNING(pop)

_WHIP_START

audio_source::audio_source(asset_handle handle) : asset(handle) {}

audio_source::audio_source(uint32_t handle, bool loaded, float length, bool stream)
	: m_buffer_handle(handle), m_loaded(loaded), m_total_duration(length), m_is_stream(stream) {}

audio_source::~audio_source()
{
	audio_engine::unload_audio_source(this);
}

void audio_source::set_position(float x, float y, float z)
{
	m_position[0] = x;
	m_position[1] = y;
	m_position[2] = z;

	alSourcefv(m_source_handle, AL_POSITION, m_position);
}

void audio_source::set_gain(float gain)
{
	m_gain = gain;

	alSourcef(m_source_handle, AL_GAIN, gain);
}

void audio_source::increase_gain(float increment_v)
{
	m_gain += increment_v;

	alSourcef(m_source_handle, AL_GAIN, m_gain);
}

void audio_source::decrease_gain(float decrement_v)
{
	if (m_gain - decrement_v < 0.0f)
		m_gain = 0.0f;
	else
		m_gain -= decrement_v;

	alSourcef(m_source_handle, AL_GAIN, m_gain);
}

void audio_source::set_pitch(float pitch)
{
	m_pitch = pitch;

	alSourcef(m_source_handle, AL_PITCH, pitch);
}

void audio_source::increase_pitch(float increment_v)
{
	if (m_pitch + increment_v > 2.0f)
		m_pitch = 2.0f;
	else
		m_pitch += increment_v;

	alSourcef(m_source_handle, AL_PITCH, m_pitch);
}

void audio_source::decrease_pitch(float decrement_v)
{
	if (m_pitch - decrement_v < 0.5f)
		m_pitch = 0.5f;
	else
		m_pitch -= decrement_v;

	alSourcef(m_source_handle, AL_PITCH, m_pitch);
}

void audio_source::set_spitial(bool spitial)
{
	m_spitial = spitial;
	alSourcei(m_source_handle, AL_SOURCE_SPATIALIZE_SOFT, spitial ? AL_TRUE : AL_FALSE);
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
}

void audio_source::set_loop(bool loop)
{
	m_loop = loop;

	alSourcei(m_source_handle, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
}

void audio_source::update_spatial_position(float x, float y, float z)
{
	ALfloat velocity[] = { x - m_position[0], y - m_position[1], z - m_position[2] };
	set_position(x, y, z);
	alSourcefv(m_source_handle, AL_VELOCITY, velocity);
}

void audio_source::get_position(float* x, float* y, float* z) const
{
	*x = m_position[0];
	*y = m_position[1];
	*z = m_position[2];
}

float audio_source::get_current_duration() const
{
	ALfloat duration;
	alGetSourcef(m_source_handle, AL_SEC_OFFSET, &duration);
	return static_cast<float>(duration);
}

std::pair<uint32_t, uint32_t> audio_source::get_length_minutes_and_seconds() const
{
	return { static_cast<uint32_t>(m_total_duration / 60.0f), static_cast<uint32_t>(m_total_duration) % 60 };
}

ref<audio_source> audio_source::load_from_file(const std::filesystem::path& filepath, bool spitial)
{
	ref<audio_source> result = audio_engine::load_audio_source(filepath);
	result->set_spitial(spitial);
	return result;
}

_WHIP_END
