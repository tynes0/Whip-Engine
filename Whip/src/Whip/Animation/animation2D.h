#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>
#include <Whip/Asset/asset.h>
#include <Whip/Scene/entity.h>

#include <vector>

_WHIP_START

struct animation_frame
{
	asset_handle texture = 0;
	float duration = 0.0f;
};

class animation2D : public asset
{
public:
	animation2D(asset_handle handle_in = asset_handle{});
	~animation2D();

	asset_type get_type() const override { return asset_type::animation; }

	static ref<animation2D> copy(ref<animation2D> anim);

	void set_frames(const std::vector<animation_frame>& frames, bool loop);
	void add_frame(const animation_frame& frame);
	void remove_frame(size_t index);

	void bound_with_entity(entity target_entity);
	void unbound_from_entity();

	void play();
	void stop();
	void pause();
	void resume();

	void set_name(const std::string& new_name);
	const std::string& get_name() const { return m_name; }

	std::vector<animation_frame>& get_frames() { return m_frames; }

	bool is_playing() const { return m_is_playing; }
	bool is_paused() const { return m_is_paused; }
	bool is_looping() const { return m_loop; }
	
	void set_loop(bool loop) { m_loop = loop; }

	void serialize(const std::filesystem::path& filepath);
	bool deserialize(const std::filesystem::path& filepath);
private:
	void apply_frame(asset_handle texture);

	std::vector<animation_frame> m_frames;

	entity m_target_entity;
	asset_handle m_original_texture = asset_handle(0);

	bool m_loop = false;
	bool m_is_playing = false;
	bool m_is_paused = false;
	float m_elapsed_time = 0.0f;
	size_t m_current_frame = 0;

	std::string m_name;

	friend class animation_manager;
};

_WHIP_END
