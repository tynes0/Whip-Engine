#include <whippch.h>

#include "content_browser_panel.h"

#include "../Helpers/icon_manager.h"

#include <Whip/Asset/asset_manager.h>
#include <Whip/Asset/utils.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/project.h>
#include <Whip/UI/UI_helpers.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <system_error>

_WHIP_START

namespace
{
	constexpr asset_type asset_type_filters[] =
	{
		asset_type::none,
		asset_type::scene,
		asset_type::texture2D,
		asset_type::audio,
		asset_type::font,
		asset_type::animation,
		asset_type::entity
	};

	std::string to_lower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	bool path_component_is_parent_reference(const std::filesystem::path& path)
	{
		for (const auto& component : path)
			if (component == "..")
				return true;

		return false;
	}

	bool path_component_is_current_reference(const std::filesystem::path& path)
	{
		return path.empty() || path == ".";
	}

	bool is_internal_project_file(const std::filesystem::path& relative_path)
	{
		return relative_path.filename() == "asset_registry.whipr";
	}
}

content_browser_panel::content_browser_panel()
{
}

content_browser_panel::content_browser_panel(ref<project> proj)
{
	init(proj);
}

void content_browser_panel::init(ref<project> proj)
{
	m_project = proj;
	m_thumbnail_cache = make_ref<thumbnail_cache>(proj);
	m_current_directory = m_base_directory = m_project->get_asset_directory();

	std::error_code error;
	std::filesystem::create_directories(m_base_directory, error);
	if (error)
		WHP_CORE_WARN("[Content Browser] Could not create asset directory '{0}': {1}", m_base_directory.string(), error.message());

	refresh_asset_tree();
	m_mode = mode::filesystem;
	m_initialized = true;
}

void content_browser_panel::on_imgui_render()
{
	ImGui::Begin("Content Browser");

	if (!m_initialized || !m_project)
	{
		ImGui::TextDisabled("No project loaded.");
		ImGui::End();
		return;
	}

	draw_toolbar();
	ImGui::Separator();

	if (ImGui::BeginTable("##ContentBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Items", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		draw_sidebar();

		ImGui::TableNextColumn();
		draw_breadcrumbs();
		ImGui::Separator();

		std::vector<browser_item> items = collect_items();
		draw_content_grid(items);

		ImGui::EndTable();
	}

	if (ImGui::BeginPopupContextWindow("##ContentBrowserWindowMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Refresh"))
			refresh_asset_tree();
		if (ImGui::MenuItem("Settings"))
			m_show_settings_popup = true;
		ImGui::EndPopup();
	}

	draw_remove_asset_modal();
	on_settings_popup();
	ImGui::End();
}

void content_browser_panel::draw_toolbar()
{
	if (ImGui::RadioButton("Files", m_mode == mode::filesystem))
		m_mode = mode::filesystem;

	ImGui::SameLine();
	if (ImGui::RadioButton("Imported", m_mode == mode::asset))
		m_mode = mode::asset;

	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
		refresh_asset_tree();

	ImGui::SameLine();
	if (m_current_directory != m_base_directory && ImGui::Button("Up"))
		set_current_directory(m_current_directory.parent_path());

	ImGui::SameLine();
	if (m_mode == mode::filesystem)
	{
		if (ImGui::Button("Import Folder"))
			import_current_directory(false);

		ImGui::SameLine();
		if (ImGui::Button("Import Recursive"))
			import_current_directory(true);

		ImGui::SameLine();
	}

	draw_type_filter();

	ImGui::SameLine();
	ImGui::SetNextItemWidth(std::max(160.0f, ImGui::GetContentRegionAvail().x - 154.0f));
	ImGui::InputTextWithHint("##ContentBrowserSearch", "Search assets and files", &m_search_query);

	if (!m_search_query.empty())
	{
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			m_search_query.clear();
	}

	ImGui::SameLine();
	if (ImGui::Button("Settings"))
		m_show_settings_popup = true;
}

void content_browser_panel::draw_type_filter()
{
	const std::string label = asset_type_filter_label();
	ImGui::SetNextItemWidth(128.0f);
	if (ImGui::BeginCombo("##ContentBrowserTypeFilter", label.c_str()))
	{
		for (asset_type type : asset_type_filters)
		{
			const bool selected = m_type_filter == type;
			std::string item_label = type == asset_type::none ? "All Types" : std::string(frenum::to_string(type));
			if (ImGui::Selectable(item_label.c_str(), selected))
				m_type_filter = type;

			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

void content_browser_panel::draw_sidebar()
{
	ImGui::BeginChild("##ContentBrowserSidebar", ImVec2(0.0f, 0.0f), false);

	ImGuiTreeNodeFlags root_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (m_current_directory == m_base_directory)
		root_flags |= ImGuiTreeNodeFlags_Selected;

	const bool open = ImGui::TreeNodeEx("Assets", root_flags);
	if (ImGui::IsItemClicked())
		set_current_directory(m_base_directory);

	if (open)
	{
		draw_directory_tree(m_base_directory);
		ImGui::TreePop();
	}

	ImGui::EndChild();
}

void content_browser_panel::draw_directory_tree(const std::filesystem::path& directory)
{
	std::vector<std::filesystem::path> directories;
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(directory, error))
	{
		if (entry.is_directory(error))
			directories.push_back(entry.path());
	}

	std::sort(directories.begin(), directories.end(), [](const auto& left, const auto& right)
		{
			return to_lower(left.filename().string()) < to_lower(right.filename().string());
		});

	for (const auto& child_directory : directories)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (child_directory == m_current_directory)
			flags |= ImGuiTreeNodeFlags_Selected;

		const bool open = ImGui::TreeNodeEx(child_directory.filename().string().c_str(), flags);
		if (ImGui::IsItemClicked())
			set_current_directory(child_directory);

		if (open)
		{
			draw_directory_tree(child_directory);
			ImGui::TreePop();
		}
	}
}

void content_browser_panel::draw_breadcrumbs()
{
	if (ImGui::Button("Assets"))
		set_current_directory(m_base_directory);

	std::error_code error;
	std::filesystem::path relative_path = std::filesystem::relative(m_current_directory, m_base_directory, error);
	if (error || path_component_is_current_reference(relative_path))
		return;

	std::filesystem::path cursor = m_base_directory;
	for (const auto& part : relative_path)
	{
		cursor /= part;
		ImGui::SameLine();
		ImGui::TextUnformatted("/");
		ImGui::SameLine();
		if (ImGui::Button(part.string().c_str()))
			set_current_directory(cursor);
	}
}

void content_browser_panel::draw_content_grid(const std::vector<browser_item>& items)
{
	const char* mode_label = m_mode == mode::filesystem ? "filesystem" : "imported";
	const size_t imported_count = static_cast<size_t>(std::count_if(items.begin(), items.end(), [](const browser_item& item) { return item.imported && !item.directory; }));
	ImGui::TextDisabled("%zu item(s) in %s view | %zu imported | unsupported %s", items.size(), mode_label, imported_count, m_show_unsupported ? "visible" : "hidden");

	if (items.empty())
	{
		ImGui::Spacing();
		ImGui::TextDisabled(m_search_query.empty() ? "This folder is empty." : "No matches.");
		return;
	}

	float cell_size = m_thumbnail_size + m_padding;
	float panel_width = ImGui::GetContentRegionAvail().x;
	int column_count = static_cast<int>(panel_width / cell_size);
	if (column_count < 1)
		column_count = 1;

	ImGui::Columns(column_count, nullptr, false);
	for (const browser_item& item : items)
	{
		draw_item(item);
		ImGui::NextColumn();
	}
	ImGui::Columns(1);
}

void content_browser_panel::draw_item(const browser_item& item)
{
	const std::string item_id = item.relative_path.generic_string();
	ImGui::PushID(item_id.c_str());

	ref<texture2D> thumbnail = item.directory ? icon_manager::get().get_icon(icon::directory) : nullptr;
	if (!thumbnail && item.type == asset_type::texture2D && std::filesystem::exists(item.absolute_path))
		thumbnail = m_thumbnail_cache->get_or_create_thumbnail(item.relative_path);
	if (!thumbnail)
		thumbnail = icon_manager::get().get_icon(icon::file);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	const bool icon_clicked = UI::image_button("##ContentBrowserItemIcon", UI::to_imgui_texture_id(thumbnail->get_renderer_id()), { m_thumbnail_size, m_thumbnail_size }, { 0, 1 }, { 1, 0 });
	ImGui::PopStyleColor();

	if (icon_clicked && item.directory)
		set_current_directory(item.absolute_path);

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && item.directory)
		set_current_directory(item.absolute_path);

	if (item.imported && !item.directory && ImGui::BeginDragDropSource())
	{
		asset_handle handle = item.handle;
		ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", &handle, sizeof(asset_handle));
		ImGui::TextUnformatted(item.relative_path.filename().string().c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (item.directory)
		{
			if (ImGui::MenuItem("Open"))
				set_current_directory(item.absolute_path);
			if (m_mode == mode::filesystem && ImGui::MenuItem("Import Folder"))
			{
				set_current_directory(item.absolute_path);
				import_current_directory(false);
			}
			if (m_mode == mode::filesystem && ImGui::MenuItem("Import Recursive"))
			{
				set_current_directory(item.absolute_path);
				import_current_directory(true);
			}
		}
		else
		{
			if (item.supported && !item.imported && ImGui::MenuItem("Import"))
				import_file(item.relative_path);
			if (item.imported && ImGui::MenuItem("Remove from Registry"))
				request_remove_asset(item.handle, item.relative_path);
			if (!item.supported)
				ImGui::TextDisabled("Unsupported asset type");
		}

		ImGui::EndPopup();
	}

	ImGui::TextWrapped(item.relative_path.filename().string().c_str());
	if (item.imported && !item.directory)
		ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.48f, 1.0f), "%s", item_type_label(item).c_str());
	else if (item.supported || item.directory)
		ImGui::TextDisabled("%s", item_type_label(item).c_str());
	else
		ImGui::TextColored(ImVec4(0.85f, 0.58f, 0.28f, 1.0f), "%s", item_type_label(item).c_str());
	ImGui::PopID();
}

void content_browser_panel::draw_remove_asset_modal()
{
	if (m_pending_remove_handle == 0)
		return;

	ImGui::OpenPopup("Remove Asset Registration");
	if (ImGui::BeginPopupModal("Remove Asset Registration", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("Remove this asset from the registry?");
		ImGui::TextDisabled("%s", m_pending_remove_path.generic_string().c_str());
		ImGui::Spacing();

		if (ImGui::Button("Remove", ImVec2(96.0f, 0.0f)))
		{
			remove_requested_asset();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(96.0f, 0.0f)))
		{
			m_pending_remove_handle = 0;
			m_pending_remove_path.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

std::vector<content_browser_panel::browser_item> content_browser_panel::collect_items() const
{
	std::vector<browser_item> items = m_mode == mode::filesystem ? collect_filesystem_items() : collect_asset_items();
	items.erase(std::remove_if(items.begin(), items.end(), [this](const browser_item& item)
		{
			if (!matches_search(item) || !passes_type_filter(item))
				return true;

			if (!m_show_unsupported && !item.directory && !item.supported)
				return true;

			return false;
		}), items.end());

	std::sort(items.begin(), items.end(), [](const browser_item& left, const browser_item& right)
		{
			if (left.directory != right.directory)
				return left.directory > right.directory;

			return to_lower(left.relative_path.filename().string()) < to_lower(right.relative_path.filename().string());
		});

	return items;
}

std::vector<content_browser_panel::browser_item> content_browser_panel::collect_filesystem_items() const
{
	std::vector<browser_item> items;
	std::error_code error;

	if (m_search_query.empty())
	{
		for (const auto& entry : std::filesystem::directory_iterator(m_current_directory, error))
		{
			browser_item item = make_filesystem_item(entry);
			if (!is_internal_project_file(item.relative_path))
				items.push_back(item);
		}
	}
	else
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(m_base_directory, error))
		{
			browser_item item = make_filesystem_item(entry);
			if (!is_internal_project_file(item.relative_path))
				items.push_back(item);
		}
	}

	return items;
}

std::vector<content_browser_panel::browser_item> content_browser_panel::collect_asset_items() const
{
	std::vector<browser_item> items;
	std::set<std::filesystem::path> directory_paths;
	const auto& registry = m_project->get_editor_asset_manager()->get_asset_registry();

	registry.foreach([&](const asset_registry::value_type& value)
		{
			browser_item item;
			item.relative_path = value.second.filepath;
			item.absolute_path = m_base_directory / item.relative_path;
			item.handle = value.first;
			item.type = value.second.type;
			item.imported = true;
			item.supported = true;

			if (!m_search_query.empty())
			{
				items.push_back(item);
				return;
			}

			if (item.absolute_path.parent_path() == m_current_directory)
			{
				items.push_back(item);
				return;
			}

			if (!is_inside_base_directory(item.absolute_path))
				return;

			std::error_code error;
			std::filesystem::path relative_to_current = std::filesystem::relative(item.absolute_path, m_current_directory, error);
			if (error || path_component_is_parent_reference(relative_to_current) || path_component_is_current_reference(relative_to_current))
				return;

			auto part = relative_to_current.begin();
			if (part == relative_to_current.end() || std::next(part) == relative_to_current.end())
				return;

			directory_paths.insert(m_current_directory / *part);
		});

	for (const auto& directory : directory_paths)
	{
		browser_item item;
		item.absolute_path = directory;
		item.relative_path = std::filesystem::relative(directory, m_base_directory);
		item.directory = true;
		items.push_back(item);
	}

	return items;
}

void content_browser_panel::set_current_directory(const std::filesystem::path& directory)
{
	std::error_code error;
	if (!std::filesystem::exists(directory, error) || !std::filesystem::is_directory(directory, error))
		return;

	if (!is_inside_base_directory(directory))
		return;

	m_current_directory = directory.lexically_normal();
}

void content_browser_panel::import_file(const std::filesystem::path& relative_path)
{
	asset_handle handle = m_project->get_editor_asset_manager()->import_asset(relative_path);
	if (handle != 0)
		refresh_asset_tree();
}

void content_browser_panel::import_current_directory(bool recursive)
{
	std::error_code error;
	if (!std::filesystem::exists(m_current_directory, error))
		return;

	if (recursive)
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(m_current_directory, error))
		{
			if (!entry.is_regular_file(error))
				continue;

			browser_item item = make_filesystem_item(entry);
			if (item.supported && !item.imported)
				import_file(item.relative_path);
		}
	}
	else
	{
		for (const auto& entry : std::filesystem::directory_iterator(m_current_directory, error))
		{
			if (!entry.is_regular_file(error))
				continue;

			browser_item item = make_filesystem_item(entry);
			if (item.supported && !item.imported)
				import_file(item.relative_path);
		}
	}

	refresh_asset_tree();
}

void content_browser_panel::request_remove_asset(asset_handle handle, const std::filesystem::path& relative_path)
{
	m_pending_remove_handle = handle;
	m_pending_remove_path = relative_path;
}

void content_browser_panel::remove_requested_asset()
{
	if (m_pending_remove_handle != 0)
	{
		m_project->get_editor_asset_manager()->delete_asset(m_pending_remove_handle);
		refresh_asset_tree();
	}

	m_pending_remove_handle = 0;
	m_pending_remove_path.clear();
}

bool content_browser_panel::is_inside_base_directory(const std::filesystem::path& path) const
{
	std::error_code error;
	std::filesystem::path relative_path = std::filesystem::relative(path, m_base_directory, error);
	if (error)
		return false;

	return !path_component_is_parent_reference(relative_path);
}

bool content_browser_panel::matches_search(const browser_item& item) const
{
	if (m_search_query.empty())
		return true;

	const std::string query = to_lower(m_search_query);
	const std::string filename = to_lower(item.relative_path.filename().string());
	const std::string path = to_lower(item.relative_path.generic_string());
	const std::string type = to_lower(item_type_label(item));

	return filename.find(query) != std::string::npos || path.find(query) != std::string::npos || type.find(query) != std::string::npos;
}

bool content_browser_panel::passes_type_filter(const browser_item& item) const
{
	if (m_type_filter == asset_type::none || item.directory)
		return true;

	return item.type == m_type_filter;
}

content_browser_panel::browser_item content_browser_panel::make_filesystem_item(const std::filesystem::directory_entry& entry) const
{
	std::error_code error;
	browser_item item;
	item.absolute_path = entry.path().lexically_normal();
	item.relative_path = std::filesystem::relative(item.absolute_path, m_base_directory, error);
	item.directory = entry.is_directory(error);

	if (!item.directory)
	{
		item.handle = find_asset_handle(item.relative_path);
		item.imported = item.handle != 0;
		item.type = item.imported ? m_project->get_editor_asset_manager()->get_asset_type(item.handle) : utils::try_get_asset_type_from_file_extension(item.relative_path.extension());
		item.supported = item.type != asset_type::none;
	}

	return item;
}

asset_handle content_browser_panel::find_asset_handle(const std::filesystem::path& relative_path) const
{
	return m_project->get_editor_asset_manager()->get_handle_from_filepath(relative_path);
}

std::string content_browser_panel::display_path(const std::filesystem::path& path) const
{
	if (path.empty())
		return "Assets";

	return path.generic_string();
}

std::string content_browser_panel::item_type_label(const browser_item& item) const
{
	if (item.directory)
		return "Folder";

	if (item.imported)
		return std::string(frenum::to_string(item.type)) + " asset";

	if (item.supported)
		return std::string(frenum::to_string(item.type)) + " file";

	return "Unsupported";
}

std::string content_browser_panel::asset_type_filter_label() const
{
	if (m_type_filter == asset_type::none)
		return "All Types";

	return std::string(frenum::to_string(m_type_filter));
}

void content_browser_panel::on_settings_popup()
{
	if (m_show_settings_popup)
	{
		ImVec2 window_size{ (float)application::get().get_window().get_width(), (float)application::get().get_window().get_height() };
		ImVec2 window_pos{ (float)application::get().get_window().get_position().first, (float)application::get().get_window().get_position().second };
		ImVec2 popup_size(352, 200);
		ImVec2 popup_pos = ImVec2{ ((window_size.x - popup_size.x) * 0.5f) + window_pos.x, ((window_size.y - popup_size.y) * 0.5f) + window_pos.y };

		ImGui::SetNextWindowSize(popup_size);
		ImGui::SetNextWindowPos(popup_pos);

		ImGui::OpenPopup("Content Browser Settings");

		if (ImGui::BeginPopupModal("Content Browser Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Thumbnail Size");
			ImGui::DragFloat("##Thumbnail Size", &m_thumbnail_size, 0.5f, 16, 512);
			ImGui::Text("Padding");
			ImGui::DragFloat("##Padding", &m_padding, 0.05f, 0, 32);
			ImGui::Checkbox("Show unsupported files", &m_show_unsupported);
			ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::GetFrameHeightWithSpacing()) * 0.5f);
			ImGui::SetCursorPosY(ImGui::GetWindowSize().y - (ImGui::GetFrameHeightWithSpacing() + 10.0f));
			if (ImGui::Button("Ok"))
			{
				ImGui::CloseCurrentPopup();
				m_show_settings_popup = false;
			}
			ImGui::EndPopup();
		}
	}
}

void content_browser_panel::refresh_asset_tree()
{
	std::error_code error;
	if (!std::filesystem::exists(m_base_directory, error))
		std::filesystem::create_directories(m_base_directory, error);

	if (!std::filesystem::exists(m_current_directory, error) || !is_inside_base_directory(m_current_directory))
		m_current_directory = m_base_directory;
}

_WHIP_END
