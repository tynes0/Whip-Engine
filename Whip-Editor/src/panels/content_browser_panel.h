#pragma once

#include "thumbnail_cache.h"

#include <Whip/Core/Core.h>
#include <Whip/Asset/asset.h>
#include <Whip/Render/Texture.h>

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
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

	struct import_summary
	{
		size_t imported = 0;
		size_t already_imported = 0;
		size_t unsupported = 0;
		size_t failed = 0;
		size_t missing = 0;
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
	bool is_hovered() const { return m_hovered; }
	void set_asset_open_callback(std::function<bool(asset_handle)> callback) { m_asset_open_callback = std::move(callback); }
	bool handle_external_drop(const std::vector<std::filesystem::path>& paths);
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
		bool missing = false;
	};

	enum class file_operation
	{
		none = 0,
		rename,
		move,
		delete_path,
		remove_registry
	};

	void draw_toolbar();
	void draw_status_bar();
	void draw_sidebar();
	void draw_directory_tree(const std::filesystem::path& directory);
	void draw_breadcrumbs();
	void draw_content_grid(const std::vector<browser_item>& items);
	void draw_item(const browser_item& item);
	void draw_file_operation_modals();
	void draw_type_filter();

	std::vector<browser_item> collect_items() const;
	std::vector<browser_item> collect_filesystem_items() const;
	std::vector<browser_item> collect_asset_items() const;

	void set_current_directory(const std::filesystem::path& directory);
	bool import_file(const std::filesystem::path& relative_path, import_summary* summary = nullptr);
	void import_current_directory(bool recursive);
	void request_rename_item(const browser_item& item);
	void request_move_item(const browser_item& item);
	void request_delete_item(const browser_item& item);
	void request_remove_asset(asset_handle handle, const std::filesystem::path& relative_path);
	bool open_asset(const browser_item& item);
	bool set_scene_as_start_scene(const browser_item& item);
	void clear_pending_operation();
	bool rename_pending_item();
	bool move_pending_item();
	bool delete_pending_item();
	bool remove_pending_registry_entry();
	bool duplicate_item(const browser_item& item);
	bool move_path_to_directory(const std::filesystem::path& source_relative_path, const std::filesystem::path& destination_directory);
	void import_supported_files_under(const std::filesystem::path& directory, import_summary& summary);
	bool import_external_path(const std::filesystem::path& source_path, import_summary& summary);

	bool is_inside_base_directory(const std::filesystem::path& path) const;
	bool matches_search(const browser_item& item) const;
	bool passes_type_filter(const browser_item& item) const;
	browser_item make_filesystem_item(const std::filesystem::directory_entry& entry) const;
	asset_handle find_asset_handle(const std::filesystem::path& relative_path) const;
	std::filesystem::path make_relative_path(const std::filesystem::path& absolute_path) const;
	std::filesystem::path make_unique_copy_path(const std::filesystem::path& absolute_path) const;
	std::filesystem::path make_unique_import_path(const std::filesystem::path& absolute_path) const;
	void set_status(std::string message, bool error = false);
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
	file_operation m_pending_operation = file_operation::none;
	asset_handle m_pending_operation_handle = 0;
	std::filesystem::path m_pending_operation_path;
	bool m_pending_operation_is_directory = false;
	std::string m_operation_text;
	std::string m_operation_error;
	std::function<bool(asset_handle)> m_asset_open_callback;

	std::string m_search_query;
	std::string m_status_message;
	bool m_status_error = false;
	asset_type m_type_filter = asset_type::none;
	mode m_mode = mode::asset;
	bool m_show_unsupported = false;
	bool m_initialized = false;
	bool m_preferences_dirty = false;
	bool m_open = true;
	bool m_hovered = false;
};

_WHIP_END
