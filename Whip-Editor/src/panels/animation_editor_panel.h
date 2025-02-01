#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/UUID.h>
#include <Whip/Asset/asset.h>
#include <Whip/Animation/animation2D.h>
#include <Whip/Asset/asset_manager.h>
#include <Whip/Render/Texture.h>

#include <vector>
#include <functional>
#include <array>

_WHIP_START

class animation_editor_panel
{
public:
	animation_editor_panel();
	~animation_editor_panel();

	void open() { m_open = true; }
	void close() { m_open = false; }

	void on_imgui_render();

	//void load_icon(icon icon_type, ref<texture2D> icon_texture);
	void set_refresh_asset_tree_callback(const std::function<void()>& func) { m_refresh_asset_tree_callback = func; }
private:
	void draw_animation_drag_drop_area(float width, float height);
	void draw_playback_controls(float width, float height);
	void draw_new_button(float width, float left_padding);
	void draw_close_button(float width, float left_padding);
	void draw_save_button(float width, float left_padding);
	void draw_name_input(float width, float left_padding);
	void draw_animation_selector(float width, float left_padding);
	void draw_timeline(float width, float timeline_height, float total_height);
	void draw_frame_list(float width);
	void draw_add_frame_button(float width);
	void draw_remove_frame_button(float width);
	void draw_frame_editor(float width);

	ref<animation2D> m_current_animation = nullptr;
	int m_selected_frame_index = -1;
	bool m_open = true;

	std::function<void()> m_refresh_asset_tree_callback;
};

_WHIP_END
