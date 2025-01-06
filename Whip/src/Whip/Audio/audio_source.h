#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/ref_counter.h>
#include <Whip/Core/memory.h>

#include <Whip/Asset/asset.h>

#include <utility>
#include <filesystem>

_WHIP_START

class audio_source : public asset
{
public:
	audio_source(asset_handle handle);
	audio_source() = default;
	audio_source(const audio_source&) = default;
	audio_source(audio_source&&) = default;
	audio_source& operator=(const audio_source&) = default;
	~audio_source();

	bool is_loaded() const { return m_loaded; }

	void set_position(float x, float y, float z);
	void set_gain(float gain);
	void increase_gain(float increment_v);
	void decrease_gain(float decrement_v);
	void set_pitch(float pitch);
	void increase_pitch(float increment_v);
	void decrease_pitch(float decrement_v);
	void set_spitial(bool spitial);
	void set_loop(bool loop);

	void update_spatial_position(float x, float y, float z);

	void get_position(float* x, float* y, float* z) const;
	float get_current_duration() const;
	float get_gain() const { return m_gain; }
	float get_pitch() const { return m_pitch; }
	float get_length() const { return m_total_duration; }
	bool is_spitial() const { return m_spitial; }
	bool is_loop() const { return m_loop; }
	bool is_streaming() const { return m_is_stream; }

	std::pair<uint32_t, uint32_t> get_length_minutes_and_seconds() const;

	static ref<audio_source> load_from_file(const std::filesystem::path& filepath, bool spitial = false);

	virtual asset_type get_type() const override { return asset_type::audio; }
private:
	audio_source(uint32_t handle, bool loaded, float length, bool stream = false);

	uint32_t m_buffer_handle = 0;
	uint32_t m_source_handle = 0;

	float m_position[3] = { 0.0f, 0.0f, 0.0f };
	bool m_loaded = false;
	bool m_spitial = false;
	bool m_is_stream = false; // not used for now

	float m_total_duration = 0.0f;

	float m_gain = 1.0f;
	float m_pitch = 1.0f;
	bool m_loop = false;

	friend class audio_engine;
	friend class audio_importer;
};

_WHIP_END
