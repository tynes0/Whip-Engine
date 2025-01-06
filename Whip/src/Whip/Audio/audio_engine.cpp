#include "whippch.h"
#include "audio_engine.h"

#include <Whip/Core/buffer.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <unordered_map>

#define AL_ALEXT_PROTOTYPES
_WHP_PRAGMA_WARNING(push)
_WHP_PRAGMA_WARNING_DISABLE(5030)
#include <AL/al.h>
#include <AL/alext.h>
#include <alc/alcmain.h>
#include <AL/efx.h>
_WHP_PRAGMA_WARNING(pop)

#include <alhelpers.h>

#include <minimp3.h>
#include <minimp3_ex.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <miniaudio.h>

_WHIP_START

enum class audio_file_format
{
	none = 0,
	ogg,
	mp3,
	wav
};

struct effect_data
{
	ALuint slot;
	ALuint effct;
	audio_engine::effect type;
};

struct filter_data
{
	ALuint filtr;
	audio_engine::filter type;
};

struct global_audio_data
{
	ALCdevice* audio_device = nullptr;
	mp3dec_t mp3d{};

	raw_buffer audio_scratch_buffer;
	uint32_t audio_scratch_buffer_size = 10 * 1024 * 1024; // 10mb initially

	std::unordered_map<ALuint, effect_data> effect_datas;
	std::unordered_map<ALuint, filter_data> filter_datas;

	bool debug_log = true;
};

static global_audio_data s_data;

static bool check_null(const ref<audio_source>& block)
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
	static audio_file_format get_file_format(const std::filesystem::path& filepath)
	{
		std::string extension = filepath.extension().string();
		if (extension == ".ogg") return audio_file_format::ogg;
		if (extension == ".mp3") return audio_file_format::mp3;
		if (extension == ".wav") return audio_file_format::wav;

		return audio_file_format::none;
	}

	static ALenum get_openAL_format(uint32_t channels)
	{
		switch (channels)
		{
		case 1: return AL_FORMAT_MONO16;
		case 2: return AL_FORMAT_STEREO16;
		}
		WHP_CORE_ASSERT(false, "[Audio Engine] Unknown audio format!");
		return 0;
	}

	static void print_audio_device_info()
	{
		if (s_data.debug_log)
		{
			WHP_CORE_DEBUG("[Audio Engine] Audio Device Info:");
			WHP_CORE_DEBUG("[Audio Engine] 	Name: {}", s_data.audio_device->DeviceName);
			WHP_CORE_DEBUG("[Audio Engine] 	Sample Rate: {}", s_data.audio_device->Frequency);
			WHP_CORE_DEBUG("[Audio Engine] 	Max Sources: {}", s_data.audio_device->SourcesMax);
			WHP_CORE_DEBUG("[Audio Engine] 	Mono: {}", s_data.audio_device->NumMonoSources);
			WHP_CORE_DEBUG("[Audio Engine] 	Stereo: {}", s_data.audio_device->NumStereoSources);
		}
	}

	static long vorbis_read(void* vorbis_file, char* buffer, size_t size)
	{
		int current_section;
		return ov_read(static_cast<OggVorbis_File*>(vorbis_file), buffer, static_cast<int>(size), 0, 2, 1, &current_section);
	}

	static long mp3_read(mp3dec_file_info_t* mp3_info, char* buffer, size_t size)
	{
		static size_t position = 0;
		size_t to_copy = std::min(size, mp3_info->samples - position);
		memcpy(buffer, mp3_info->buffer + position, to_copy);
		position += to_copy;
		return static_cast<long>(to_copy);
	}

	static long wav_read(ma_decoder* decoder, char* buffer, size_t size)
	{
		ma_uint64 frames_read = 0;
		ma_decoder_read_pcm_frames(decoder, buffer, size / sizeof(ma_int16), &frames_read);
		return static_cast<long>(frames_read * sizeof(ma_int16));
	}
}

void audio_engine::init()
{
	if (InitAL(s_data.audio_device, nullptr, 0) != 0)
		WHP_CORE_ERROR("[Audio Engine] Audio device error!");

	detail::print_audio_device_info();

	mp3dec_init(&s_data.mp3d);

	s_data.audio_scratch_buffer.allocate(s_data.audio_scratch_buffer_size);

	ALfloat listener_pos[] = { 0.0, 0.0, 0.0 };
	ALfloat listener_vel[] = { 0.0, 0.0, 0.0 };
	ALfloat listener_ori[] = { 0.0, 0.0, -1.0, 0.0, 1.0, 0.0};
	alListenerfv(AL_POSITION, listener_pos);
	alListenerfv(AL_VELOCITY, listener_vel);
	alListenerfv(AL_ORIENTATION, listener_ori);
}

ref<audio_source> audio_engine::load_audio_source(const std::filesystem::path& filepath, asset_handle handle)
{
	audio_file_format format = detail::get_file_format(filepath);
	switch (format)
	{
	case whip::audio_file_format::ogg: return load_audio_source_ogg(filepath, handle);
	case whip::audio_file_format::mp3: return load_audio_source_mp3(filepath, handle);
	case whip::audio_file_format::wav: return load_audio_source_wav(filepath, handle);
	}
	return nullptr;
}

ref<audio_source> audio_engine::load_audio_stream(const std::filesystem::path& filepath)
{
	WHP_CORE_ASSERT(false, "UNIMPLAMENTED!");
	return nullptr;
}

void audio_engine::unload_audio_source(audio_source* source)
{
	if (source == NULL)
	{
		WHP_CORE_ERROR("[Audio Engine] null audio source passed to audio engine!");
		return;
	}
	if (source->m_source_handle != 0)
	{
		alSourceStop(source->m_source_handle);
		alSourcei(source->m_source_handle, AL_BUFFER, 0);

		ALuint buffer = source->m_buffer_handle;
		if (buffer != 0)
			alDeleteBuffers(1, &buffer);

		alDeleteSources(1, &source->m_source_handle);

		source->m_source_handle = 0;
		source->m_buffer_handle = 0;
		source->m_loaded = false;
		source->m_total_duration = 0.0f;

		if (alGetError() != AL_NO_ERROR)
			WHP_CORE_ERROR("[Audio Engine] Failed to unload audio source.");
	}
}

void audio_engine::unload_audio_source(ref<audio_source>& source)
{
	unload_audio_source(source.get());
	source.reset();
}

void audio_engine::play(const ref<audio_source>& source)
{
	if (!check_null(source))
		return;
	alSourcePlay(source->m_source_handle);
}

void audio_engine::stop(const ref<audio_source>& source)
{
	if (!check_null(source))
		return;
	alSourceStop(source->m_source_handle);
}

void audio_engine::pause(const ref<audio_source>& source)
{
	if (!check_null(source))
		return;
	alSourcePause(source->m_source_handle);
}

void audio_engine::rewind(const ref<audio_source>& source)
{
	if (!check_null(source))
		return;
	alSourceRewind(source->m_source_handle);
}

void audio_engine::seek(const ref<audio_source>& source, float seconds)
{
	if (!check_null(source))
		return;
	if (source->is_streaming())
	{
		WHP_CORE_WARN("[Audio Engine] Seeking is not available while streaming!");
		return;
	}
	if (seconds < 0 || seconds > source->m_total_duration)
	{
		WHP_CORE_ERROR("[Audio Engine] Seek position out of range: {}", seconds);
		return;
	}
	alSourcef(source->m_source_handle, AL_SEC_OFFSET, seconds);
}

void audio_engine::set_listener_position(float x, float y, float z)
{
	ALfloat listenerPos[] = { x, y, z };
	alListenerfv(AL_POSITION, listenerPos);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set listener position!");
}

void audio_engine::set_listener_orientation(float atX, float atY, float atZ, float upX, float upY, float upZ)
{
	ALfloat listenerOri[] = { atX, atY, atZ, upX, upY, upZ };
	alListenerfv(AL_ORIENTATION, listenerOri);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set listener orientation!");
}

void audio_engine::set_listener_velocity(float x, float y, float z)
{
	ALfloat listenerVel[] = { x, y, z };
	alListenerfv(AL_VELOCITY, listenerVel);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set listener velocity!");
}

void audio_engine::set_doppler_factor(float factor)
{
	alDopplerFactor(factor);
	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set Doppler factor!");
}

void audio_engine::set_speed_of_sound(float speed)
{
	alSpeedOfSound(speed);
	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to set speed of sound!");
}

void audio_engine::apply_reverb(const ref<audio_source>& source, float decay_time, float density)
{
	if (!check_null(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
	alEffectf(effect, AL_REVERB_DECAY_TIME, decay_time);
	alEffectf(effect, AL_REVERB_DENSITY, density);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply reverb!");
	else
		s_data.effect_datas[source->m_source_handle] = { effectSlot, effect, effect::reverb };
}

void audio_engine::apply_echo(const ref<audio_source>& source, float delay, float damping)
{
	if (!check_null(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_ECHO);
	alEffectf(effect, AL_ECHO_DELAY, delay);
	alEffectf(effect, AL_ECHO_DAMPING, damping);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Error attaching echo effect to source!");
	else
		s_data.effect_datas[source->m_source_handle] = { effectSlot, effect, effect::echo };
}

void audio_engine::apply_chorus(const ref<audio_source>& source, float rate, float depth, float feedback)
{
	if (!check_null(source))
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
	alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply chorus effect!");
	else
		s_data.effect_datas[source->m_source_handle] = { effectSlot, effect, effect::chorus };
}

void audio_engine::apply_distortion(const ref<audio_source>& source, float edge, float gain, float lowpass_cutoff)
{
	if (!check_null(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_DISTORTION);
	alEffectf(effect, AL_DISTORTION_EDGE, edge);
	alEffectf(effect, AL_DISTORTION_GAIN, gain);
	alEffectf(effect, AL_DISTORTION_LOWPASS_CUTOFF, lowpass_cutoff);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply distortion effect!");
	else
		s_data.effect_datas[source->m_source_handle] = { effectSlot, effect, effect::distortion };
}

void audio_engine::apply_flanger(const ref<audio_source>& source, float rate, float depth, float feedback)
{
	if (!check_null(source))
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
	alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply flanger effect!");
	else
		s_data.effect_datas[source->m_source_handle] = { effectSlot, effect, effect::flanger };
}

void audio_engine::apply_equalizer(const ref<audio_source>& source, float low_gain, float mid_gain, float high_gain)
{
	if (!check_null(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_EQUALIZER);
	alEffectf(effect, AL_EQUALIZER_LOW_GAIN, low_gain);
	alEffectf(effect, AL_EQUALIZER_MID1_GAIN, mid_gain);
	alEffectf(effect, AL_EQUALIZER_HIGH_GAIN, high_gain);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply equalizer effect!");
	else
		s_data.effect_datas[source->m_source_handle] = { effectSlot, effect, effect::equalizer };
}

void audio_engine::apply_frequency_shifter(const ref<audio_source>& source, float frequency, int direction)
{
	if (!check_null(source))
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
	alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply frequency shifter effect!");
	else
		s_data.effect_datas[source->m_source_handle] = { effectSlot, effect, effect::chorus };
}

void audio_engine::apply_autowah(const ref<audio_source>& source, float attack_time, float release_time, float resonance)
{
	if (!check_null(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_AUTOWAH);
	alEffectf(effect, AL_AUTOWAH_ATTACK_TIME, attack_time);
	alEffectf(effect, AL_AUTOWAH_RELEASE_TIME, release_time);
	alEffectf(effect, AL_AUTOWAH_RESONANCE, resonance);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply autowah effect!");
	else
		s_data.effect_datas[source->m_source_handle] = { effectSlot, effect, effect::autowah };
}

void audio_engine::apply_ring_modulator(const ref<audio_source>& source, float frequency, float highpass_cutoff)
{
	if (!check_null(source))
		return;
	ALuint effect;
	alGenEffects(1, &effect);
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_RING_MODULATOR);
	alEffectf(effect, AL_RING_MODULATOR_FREQUENCY, frequency);
	alEffectf(effect, AL_RING_MODULATOR_HIGHPASS_CUTOFF, highpass_cutoff);

	ALuint effectSlot;
	alGenAuxiliaryEffectSlots(1, &effectSlot);
	alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT, effect);
	alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, effectSlot, 0, AL_FILTER_NULL);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply ring modulator effect!");
	else
		s_data.effect_datas[source->m_source_handle] = { effectSlot, effect, effect::ring_modulator };
}

void audio_engine::remove_effect(const ref<audio_source>& source, effect type)
{
	if (!check_null(source))
		return;
	auto it = s_data.effect_datas.find(source->m_source_handle);
	if (it != s_data.effect_datas.end())
	{
		if (it->second.type == type)
		{
			ALuint effectSlot = it->second.slot;
			ALuint effect = it->second.effct;

			alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
			alDeleteEffects(1, &effect);
			alDeleteAuxiliaryEffectSlots(1, &effectSlot);

			s_data.effect_datas.erase(it);
		}
	}
}

void audio_engine::apply_low_pass_filter(const ref<audio_source>& source, float gain, float gainHF)
{
	if (!check_null(source))
		return;
	ALuint filter;
	alGenFilters(1, &filter);
	alFilteri(filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
	alFilterf(filter, AL_LOWPASS_GAIN, gain);
	alFilterf(filter, AL_LOWPASS_GAINHF, gainHF);
	alSourcei(source->m_source_handle, AL_DIRECT_FILTER, filter);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Error attaching low-pass filter to source!");
	else
		s_data.filter_datas[source->m_source_handle] = { filter, filter::low_pass_filter };
}

void audio_engine::apply_high_pass_filter(const ref<audio_source>& source, float gain, float gainLF)
{
	if (!check_null(source))
		return;
	ALuint filter;
	alGenFilters(1, &filter);
	alFilteri(filter, AL_FILTER_TYPE, AL_FILTER_HIGHPASS);
	alFilterf(filter, AL_HIGHPASS_GAIN, gain);
	alFilterf(filter, AL_HIGHPASS_GAINLF, gainLF);
	alSourcei(source->m_source_handle, AL_DIRECT_FILTER, filter);

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to apply high-pass filter!");
	else
		s_data.filter_datas[source->m_source_handle] = { filter, filter::high_pass_filter };
}

void audio_engine::remove_filter(const ref<audio_source>& source, filter type)
{
	if (!check_null(source))
		return;
	auto it = s_data.filter_datas.find(source->m_source_handle);
	if (it != s_data.filter_datas.end())
	{
		if (it->second.type == type)
		{
			ALuint filtr = it->second.filtr;

			alSource3i(source->m_source_handle, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
			alDeleteFilters(1, &filtr);

			s_data.filter_datas.erase(it);
		}
	}
}

audio_engine::audio_state audio_engine::get_state(const ref<audio_source>& source)
{
	if (!check_null(source))
		return audio_state::none;
	ALint state;
	alGetSourcei(source->m_source_handle, AL_SOURCE_STATE, &state);
	switch (state)
	{
	case AL_STOPPED:	return audio_state::stopped;
	case AL_PLAYING:	return audio_state::playing;
	case AL_PAUSED:		return audio_state::paused;
	default:			return audio_state::none;
	}
}

void audio_engine::set_debug_log_state(bool state)
{
	s_data.debug_log = state;
}

audio_engine::audio_data audio_engine::load_audio_data_ogg(const std::filesystem::path& filepath)
{
	FILE* file = fopen(filepath.string().c_str(), "rb");
	OggVorbis_File vorbis_file;
	if (ov_open_callbacks(file, &vorbis_file, NULL, 0, OV_CALLBACKS_NOCLOSE) < 0)
	{
		WHP_CORE_ERROR("[Audio Engine] Could not open ogg stream! (The file doesn't use the vorbis codec.)");
		return audio_data();
	}
	vorbis_info* vb_info = ov_info(&vorbis_file, -1);
	long sample_rate = vb_info->rate;
	int channels = vb_info->channels;
	uint64_t samples = ov_pcm_total(&vorbis_file, -1);
	float track_length = (float)samples / (float)sample_rate;
	uint32_t buffer_size = static_cast<uint32_t>(2 * static_cast<unsigned long long>(channels) * samples);

	if (s_data.audio_scratch_buffer_size < buffer_size)
	{
		s_data.audio_scratch_buffer_size = buffer_size;
		s_data.audio_scratch_buffer.release();
		s_data.audio_scratch_buffer.allocate(s_data.audio_scratch_buffer_size);
	}

	raw_buffer ogg_buffer = s_data.audio_scratch_buffer;
	raw_buffer buffer_ptr = ogg_buffer;
	int eof = 0;
	while (!eof)
	{
		int current_section;
		long length = ov_read(&vorbis_file, buffer_ptr.as<char>(), 4096, 0, 2, 1, &current_section);
		buffer_ptr.data += length;
		if (length == 0)
			eof = 1;
		else if (length < 0)
		{
			WHP_CORE_ASSERT(length != OV_EBADLINK, "[Audio Engine] Corrupt bitstream section!");
			ov_clear(&vorbis_file);
			fclose(file);
			return audio_data();
		}
	}

	uint32_t size = static_cast<uint32_t>(buffer_ptr - ogg_buffer);
	ogg_buffer.size = size;
	WHP_CORE_ASSERT(buffer_size == size, "[Audio Engine] The bitstream was not read to the expected size.");

	ALenum al_format = detail::get_openAL_format(channels);

	audio_data data
	{
		al_format,
		ogg_buffer,
		sample_rate,
		track_length,
		false
	};

	return data;
}

audio_engine::audio_data audio_engine::load_audio_data_mp3(const std::filesystem::path& filepath)
{
	mp3dec_file_info_t info;
	int load_result = mp3dec_load(&s_data.mp3d, filepath.string().c_str(), &info, NULL, NULL);
	uint32_t size = static_cast<uint32_t>(info.samples * sizeof(mp3d_sample_t));
	int sample_rate = info.hz;
	int channels = info.channels;
	float length_seconds = size / (info.avg_bitrate_kbps * 1024.0f);

	ALenum al_format = detail::get_openAL_format(channels);

	audio_data data
	{
		al_format,
		raw_buffer(info.buffer, size),
		sample_rate,
		length_seconds,
		false
	};

	return data;
}

audio_engine::audio_data audio_engine::load_audio_data_wav(const std::filesystem::path& filepath)
{
	ma_result result;
	ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 2, 44100);
	ma_decoder decoder;

	result = ma_decoder_init_file(filepath.string().c_str(), &config, &decoder);
	if (result != MA_SUCCESS)
	{
		WHP_CORE_ERROR("[Audio Engine] Failed to load WAV file: {}", filepath.string());
		return audio_data();
	}

	ma_uint64 total_frames = 0;
	result = ma_decoder_get_length_in_pcm_frames(&decoder, &total_frames);
	if (result != MA_SUCCESS)
	{
		WHP_CORE_ERROR("[Audio Engine] Failed to get the length of the WAV file.");
		ma_decoder_uninit(&decoder);
		return audio_data();
	}

	size_t data_size = decoder.outputChannels * total_frames * sizeof(ma_int16);
	raw_buffer buffer(data_size);
	size_t frames_read = 0;
	result = ma_decoder_read_pcm_frames(&decoder, buffer.as<void>(), total_frames, &frames_read);
	if (result != MA_SUCCESS)
	{
		WHP_CORE_ERROR("[Audio Engine] Failed to read WAV frames.");
		ma_decoder_uninit(&decoder);
		buffer.release();
		return audio_data();
	}

	ALenum al_format = detail::get_openAL_format(decoder.outputChannels);

	audio_data data
	{
		al_format,
		buffer,
		static_cast<int>(decoder.outputSampleRate),
		(static_cast<float>(total_frames) / static_cast<float>(decoder.outputSampleRate)),
		false
	};

	return data;
}

ref<audio_source> audio_engine::load_audio_source_al(audio_data& data, asset_handle handle)
{
	ALuint buffer;
	alGenBuffers(1, &buffer);
	alBufferData(buffer, static_cast<ALenum>(data.al_format), data.buffer.as<ALvoid>(), static_cast<ALsizei>(data.buffer.size), static_cast<ALsizei>(data.sample_rate));

	ref<audio_source> result_source = make_ref<audio_source>(handle);
	result_source->m_buffer_handle = static_cast<uint32_t>(buffer);
	result_source->m_loaded = true;
	result_source->m_total_duration = data.track_length;

	alGenSources(1, &result_source->m_source_handle);
	alSourcei(result_source->m_source_handle, AL_BUFFER, buffer);

	data.buffer.release();

	if (alGetError() != AL_NO_ERROR)
		WHP_CORE_ERROR("[Audio Engine] Failed to setup sound source!");

	return result_source;
}

ref<audio_source> audio_engine::load_audio_source_ogg(const std::filesystem::path& filepath, asset_handle handle)
{
	audio_data data = load_audio_data_ogg(filepath);
	if (data.is_null)
	{
		WHP_CORE_ERROR("[Audio Engine] Error while loading audio source!");
		return nullptr;
	}
	return load_audio_source_al(data, handle);
}

ref<audio_source> audio_engine::load_audio_source_mp3(const std::filesystem::path& filepath, asset_handle handle)
{
	audio_data data = load_audio_data_mp3(filepath);
	if (data.is_null)
	{
		WHP_CORE_ERROR("[Audio Engine] Error while loading audio source!");
		return nullptr;
	}
	return load_audio_source_al(data, handle);
}

ref<audio_source> audio_engine::load_audio_source_wav(const std::filesystem::path& filepath, asset_handle handle)
{
	audio_data data = load_audio_data_wav(filepath);
	if (data.is_null)
	{
		WHP_CORE_ERROR("[Audio Engine] Error while loading audio source!");
		return nullptr;
	}
	return load_audio_source_al(data, handle);
}

_WHIP_END
