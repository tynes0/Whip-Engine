#pragma once

#include "thumbnail_cache.h"

#include <Whip/Core/Core.h>
#include <Whip/Render/Texture.h>

#include <filesystem>
#include <map>
#include <set>


_WHIP_START

class content_browser_panel
{
public:
	content_browser_panel();
	content_browser_panel(ref<project> proj);
	
	void init(ref<project> proj);

	void on_imgui_render();
	void on_settings_popup();
	void refresh_asset_tree();
private:
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

	bool m_initialized;
	ref<texture2D> m_directory_icon;
	ref<texture2D> m_file_icon;
	ref<texture2D> m_return_icon;

	struct tree_node
	{
		std::filesystem::path path;
		asset_handle handle;

		uint32_t parent = (uint32_t)-1;
		std::map<std::filesystem::path, uint32_t> children;

		tree_node(const std::filesystem::path& path, asset_handle handle) : path(path), handle(handle) {}
	};

	std::vector<tree_node> m_tree_nodes;

	std::map<std::filesystem::path, std::vector<std::filesystem::path>> m_asset_tree;

	enum class mode
	{
		asset = 0, filesystem = 1
	};

	mode m_mode = mode::asset;
};

_WHIP_END
