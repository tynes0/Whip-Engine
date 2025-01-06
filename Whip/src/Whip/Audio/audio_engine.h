#pragma once

#include "audio_source.h"
#include <Whip/Core/memory.h>

#include <functional>

_WHIP_START

class audio_engine
{
public:
	enum class audio_state
	{
		none,
		stopped,
		playing,
		paused
	};

	enum class effect
	{
		none = 0,
		reverb,
		echo,
		chorus,
		distortion,
		flanger,
		equalizer,
		frequency_shifter,
		autowah,
		ring_modulator
	};

	enum class filter
	{
		none = 0,
		low_pass_filter,
		high_pass_filter
	};
public:
	static void init();
	static ref<audio_source> load_audio_source(const std::filesystem::path& filepath, asset_handle handle = asset_handle{});
	static ref<audio_source> load_audio_stream(const std::filesystem::path& filepath);
	static void unload_audio_source(audio_source* source);
	static void unload_audio_source(ref<audio_source>& source);

	static void play(const ref<audio_source>& source);
	static void stop(const ref<audio_source>& source);
	static void pause(const ref<audio_source>& source);
	static void rewind(const ref<audio_source>& source);
	static void seek(const ref<audio_source>& source, float seconds);

	static void set_listener_position(float x, float y, float z);
	static void set_listener_orientation(float atX, float atY, float atZ, float upX, float upY, float upZ);
	static void set_listener_velocity(float x, float y, float z);

	static void set_doppler_factor(float factor);
	static void set_speed_of_sound(float speed);

	// effects
	static void apply_reverb(const ref<audio_source>& source, float decay_time, float density);
	static void apply_echo(const ref<audio_source>& source, float delay, float damping);
	static void apply_chorus(const ref<audio_source>& source, float rate, float depth, float feedback);
	static void apply_distortion(const ref<audio_source>& source, float edge, float gain, float lowpass_cutoff);
	static void apply_flanger(const ref<audio_source>& source, float rate, float depth, float feedback);
	static void apply_equalizer(const ref<audio_source>& source, float low_gain, float mid_gain, float high_gain);
	static void apply_frequency_shifter(const ref<audio_source>& source, float frequency, int direction);
	static void apply_autowah(const ref<audio_source>& source, float attack_time, float release_time, float resonance);
	static void apply_ring_modulator(const ref<audio_source>& source, float frequency, float highpass_cutoff);
	static void remove_effect(const ref<audio_source>& source, effect type);

	// filters
	static void apply_low_pass_filter(const ref<audio_source>& source, float gain, float gainHF);
	static void apply_high_pass_filter(const ref<audio_source>& source, float gain, float gainLF);
	static void remove_filter(const ref<audio_source>& source, filter type);

	static audio_state get_state(const ref<audio_source>& source);

	static void set_debug_log_state(bool state);
private:
	struct audio_data
	{
		int al_format = 0;
		raw_buffer buffer;
		int sample_rate = 0;
		float track_length = 0.0f;

		bool is_null = true;
	};
	static audio_data load_audio_data_ogg(const std::filesystem::path& filepath);
	static audio_data load_audio_data_mp3(const std::filesystem::path& filepath);
	static audio_data load_audio_data_wav(const std::filesystem::path& filepath);

	static ref<audio_source> load_audio_source_al(audio_data& data, asset_handle handle);

	static ref<audio_source> load_audio_source_ogg(const std::filesystem::path& filepath, asset_handle handle);
	static ref<audio_source> load_audio_source_mp3(const std::filesystem::path& filepath, asset_handle handle);
	static ref<audio_source> load_audio_source_wav(const std::filesystem::path& filepath, asset_handle handle);
};


_WHIP_END
