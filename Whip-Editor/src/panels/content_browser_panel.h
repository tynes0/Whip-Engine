#pragma once

#include "thumbnail_cache.h"

#include <Whip/Core/Core.h>
#include <Whip/Asset/asset.h>
#include <Whip/Render/Texture.h>

#include <filesystem>
#include <string>
#include <vector>


_WHIP_START

class content_browser_panel
{
public:
	content_browser_panel();
	content_browser_panel(ref<project> proj);

	struct preferences
	{
		float thumbnail_size = 64.0f;
		float padding = 16.0f;
		bool show_unsupported = false;
		bool open = true;
		int mode = 0;
		int type_filter = 0;
		std::filesystem::path current_directory;
	};
	
	void init(ref<project> proj);

	void on_imgui_render();
	void on_settings_popup();
	void refresh_asset_tree();
	preferences get_preferences() const;
	void apply_preferences(const preferences& prefs);
	bool consume_preferences_dirty();
	void set_open(bool open);
	bool is_open() const { return m_open; }
private:
	enum class mode
	{
		filesystem = 0,
		asset = 1
	};

	struct browser_item
	{
		std::filesystem::path absolute_path;
		std::filesystem::path relative_path;
		asset_handle handle = 0;
		asset_type type = asset_type::none;
		bool directory = false;
		bool imported = false;
		bool supported = false;
	};

	void draw_toolbar();
	void draw_sidebar();
	void draw_directory_tree(const std::filesystem::path& directory);
	void draw_breadcrumbs();
	void draw_content_grid(const std::vector<browser_item>& items);
	void draw_item(const browser_item& item);
	void draw_remove_asset_modal();
	void draw_type_filter();

	std::vector<browser_item> collect_items() const;
	std::vector<browser_item> collect_filesystem_items() const;
	std::vector<browser_item> collect_asset_items() const;

	void set_current_directory(const std::filesystem::path& directory);
	void import_file(const std::filesystem::path& relative_path);
	void import_current_directory(bool recursive);
	void request_remove_asset(asset_handle handle, const std::filesystem::path& relative_path);
	void remove_requested_asset();

	bool is_inside_base_directory(const std::filesystem::path& path) const;
	bool matches_search(const browser_item& item) const;
	bool passes_type_filter(const browser_item& item) const;
	browser_item make_filesystem_item(const std::filesystem::directory_entry& entry) const;
	asset_handle find_asset_handle(const std::filesystem::path& relative_path) const;
	std::string display_path(const std::filesystem::path& path) const;
	std::string item_type_label(const browser_item& item) const;
	std::string asset_type_filter_label() const;

	ref<project> m_project;
	ref<thumbnail_cache> m_thumbnail_cache;

	// directories
	std::filesystem::path m_base_directory;
	std::filesystem::path m_current_directory;

	// style
	float m_thumbnail_size = 64.0f;
	float m_padding = 16.0f;

	// popup
	bool m_show_settings_popup = false;
	asset_handle m_pending_remove_handle = 0;
	std::filesystem::path m_pending_remove_path;

	std::string m_search_query;
	asset_type m_type_filter = asset_type::none;
	mode m_mode = mode::asset;
	bool m_show_unsupported = false;
	bool m_initialized = false;
	bool m_preferences_dirty = false;
	bool m_open = true;
};

_WHIP_END
