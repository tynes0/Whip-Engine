#include <whippch.h>

#include "content_browser_panel.h"

#include "../Helpers/icon_manager.h"

#include <Whip/Asset/asset_manager.h>
#include <Whip/Asset/utils.h>
#include <Whip/Core/Application.h>
#include <Whip/Project/project.h>
#include <Whip/UI/UI_helpers.h>
#include <Whip/Utils/platform_utils.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <sstream>
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

	bool path_is_under_directory(const std::filesystem::path& path, const std::filesystem::path& directory)
	{
		std::filesystem::path normalized_path = path.lexically_normal();
		std::filesystem::path normalized_directory = directory.lexically_normal();
		if (normalized_path == normalized_directory)
			return true;

		auto path_it = normalized_path.begin();
		auto directory_it = normalized_directory.begin();
		for (; directory_it != normalized_directory.end(); ++directory_it, ++path_it)
		{
			if (path_it == normalized_path.end() || *path_it != *directory_it)
				return false;
		}
		return true;
	}

	std::string import_summary_text(const content_browser_panel::import_summary& summary)
	{
		std::ostringstream stream;
		stream << "Import: " << summary.imported << " imported";
		if (summary.already_imported > 0)
			stream << ", " << summary.already_imported << " already registered";
		if (summary.unsupported > 0)
			stream << ", " << summary.unsupported << " unsupported";
		if (summary.missing > 0)
			stream << ", " << summary.missing << " missing";
		if (summary.failed > 0)
			stream << ", " << summary.failed << " failed";
		return stream.str();
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
	if (!m_open)
	{
		m_hovered = false;
		return;
	}

	bool open = m_open;
	ImGui::Begin("Content Browser", &open);
	m_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
	if (open != m_open)
		set_open(open);

	if (!m_initialized || !m_project)
	{
		ImGui::TextDisabled("No project loaded.");
		ImGui::End();
		return;
	}

	draw_toolbar();
	draw_status_bar();
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
		if (ImGui::MenuItem("Open Current Folder in Explorer"))
			utils::open_external_path(m_current_directory);
		if (ImGui::MenuItem("Settings"))
			m_show_settings_popup = true;
		ImGui::EndPopup();
	}

	draw_file_operation_modals();
	on_settings_popup();
	ImGui::End();
}

void content_browser_panel::draw_toolbar()
{
	if (ImGui::RadioButton("Files", m_mode == mode::filesystem))
	{
		m_mode = mode::filesystem;
		m_preferences_dirty = true;
	}

	ImGui::SameLine();
	if (ImGui::RadioButton("Imported", m_mode == mode::asset))
	{
		m_mode = mode::asset;
		m_preferences_dirty = true;
	}

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

void content_browser_panel::draw_status_bar()
{
	if (m_status_message.empty())
		return;

	ImGui::Spacing();
	ImGui::TextColored(m_status_error ? ImVec4(0.95f, 0.50f, 0.34f, 1.0f) : ImVec4(0.72f, 0.78f, 0.54f, 1.0f), "%s", m_status_message.c_str());
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
			{
				m_type_filter = type;
				m_preferences_dirty = true;
			}

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
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
		{
			std::filesystem::path source_relative_path(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
			move_path_to_directory(source_relative_path, m_base_directory);
		}
		ImGui::EndDragDropTarget();
	}

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
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
			{
				std::filesystem::path source_relative_path(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
				move_path_to_directory(source_relative_path, child_directory);
			}
			ImGui::EndDragDropTarget();
		}

		if (open)
		{
			draw_directory_tree(child_directory);
			ImGui::TreePop();
		}
	}
}

void content_browser_panel::draw_breadcrumbs()
{
	if (ImGui::Button("Assets##ContentBrowserBreadcrumbRoot"))
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
		ImGui::PushID(cursor.generic_string().c_str());
		if (ImGui::Button(part.string().c_str()))
			set_current_directory(cursor);
		ImGui::PopID();
	}
}

void content_browser_panel::draw_content_grid(const std::vector<browser_item>& items)
{
	const char* mode_label = m_mode == mode::filesystem ? "filesystem" : "imported";
	const size_t imported_count = static_cast<size_t>(std::count_if(items.begin(), items.end(), [](const browser_item& item) { return item.imported && !item.directory; }));
	const size_t missing_count = static_cast<size_t>(std::count_if(items.begin(), items.end(), [](const browser_item& item) { return item.missing; }));
	const size_t unsupported_count = static_cast<size_t>(std::count_if(items.begin(), items.end(), [](const browser_item& item) { return !item.directory && !item.supported; }));
	ImGui::TextDisabled("%zu item(s) in %s view | %zu imported | %zu missing | %zu unsupported %s",
		items.size(), mode_label, imported_count, missing_count, unsupported_count, m_show_unsupported ? "visible" : "hidden");

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

	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 8.0f));
	if (ImGui::BeginTable("##ContentBrowserGrid", column_count, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX))
	{
		for (int column = 0; column < column_count; ++column)
			ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, cell_size);

		int column_index = 0;
		for (const browser_item& item : items)
		{
			if (column_index == 0)
				ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(column_index);
			draw_item(item);

			column_index = (column_index + 1) % column_count;
		}
		ImGui::EndTable();
	}
	ImGui::PopStyleVar();
}

void content_browser_panel::draw_item(const browser_item& item)
{
	const std::string item_id = item.relative_path.generic_string();
	ImGui::PushID(item_id.c_str());
	ImGui::BeginGroup();

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
	else if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !item.directory)
		open_asset(item);

	if (ImGui::BeginDragDropSource())
	{
		std::string relative_path = item.relative_path.generic_string();
		ImGui::SetDragDropPayload("CONTENT_BROWSER_PATH", relative_path.data(), relative_path.size());
		if (item.supported && !item.directory && !item.missing)
		{
			asset_handle handle = item.handle;
			if (handle == 0)
			{
				import_file(item.relative_path);
				handle = find_asset_handle(item.relative_path);
			}
			if (handle != 0)
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", &handle, sizeof(asset_handle));
		}
		ImGui::TextUnformatted(item.relative_path.filename().string().c_str());
		ImGui::EndDragDropSource();
	}

	if (item.directory && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_PATH"))
		{
			std::filesystem::path source_relative_path(std::string(static_cast<const char*>(payload->Data), payload->DataSize));
			move_path_to_directory(source_relative_path, item.absolute_path);
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (item.directory)
		{
			if (ImGui::MenuItem("Open"))
				set_current_directory(item.absolute_path);
			if (ImGui::MenuItem("Open in Explorer"))
				utils::open_external_path(item.absolute_path);
			if (ImGui::MenuItem("Rename"))
				request_rename_item(item);
			if (ImGui::MenuItem("Move To..."))
				request_move_item(item);
			if (ImGui::MenuItem("Duplicate"))
				duplicate_item(item);
			if (ImGui::MenuItem("Delete"))
				request_delete_item(item);
			ImGui::Separator();
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
			if (item.missing)
			{
				if (ImGui::MenuItem("Remove Missing Registration"))
					request_remove_asset(item.handle, item.relative_path);
			}
			else
			{
				if (item.imported && item.type == asset_type::scene)
				{
					if (ImGui::MenuItem("Open Scene"))
						open_asset(item);
					const bool is_start_scene = m_project && m_project->get_config().start_scene == item.handle;
					if (ImGui::MenuItem("Set as Start Scene", nullptr, is_start_scene))
						set_scene_as_start_scene(item);
					ImGui::Separator();
				}

				if (ImGui::MenuItem("Show in Explorer"))
					utils::open_external_path(item.absolute_path.parent_path());
				if (item.supported && !item.imported && ImGui::MenuItem("Import"))
					import_file(item.relative_path);
				if (ImGui::MenuItem("Rename"))
					request_rename_item(item);
				if (ImGui::MenuItem("Move To..."))
					request_move_item(item);
				if (ImGui::MenuItem("Duplicate"))
					duplicate_item(item);
				if (ImGui::MenuItem("Delete"))
					request_delete_item(item);
			}
			if (item.imported && !item.missing && ImGui::MenuItem("Remove from Registry"))
				request_remove_asset(item.handle, item.relative_path);
			if (!item.supported && !item.missing)
				ImGui::TextDisabled("Unsupported asset type");
		}

		ImGui::EndPopup();
	}

	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + m_thumbnail_size);
	ImGui::TextWrapped(item.relative_path.filename().string().c_str());
	ImGui::PopTextWrapPos();
	if (item.missing)
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "Missing %s", item_type_label(item).c_str());
	else if (item.imported && !item.directory)
		ImGui::TextColored(ImVec4(0.42f, 0.72f, 0.52f, 1.0f), "%s", item_type_label(item).c_str());
	else if (item.supported || item.directory)
		ImGui::TextDisabled("%s", item_type_label(item).c_str());
	else
		ImGui::TextColored(ImVec4(0.86f, 0.62f, 0.34f, 1.0f), "%s", item_type_label(item).c_str());
	ImGui::EndGroup();
	ImGui::PopID();
}

void content_browser_panel::draw_file_operation_modals()
{
	if (m_pending_operation == file_operation::none)
		return;

	const char* popup_name = "Content Browser Operation";
	ImGui::OpenPopup(popup_name);
	if (!ImGui::BeginPopupModal(popup_name, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	if (m_pending_operation == file_operation::rename)
	{
		ImGui::TextUnformatted("Rename asset");
		ImGui::TextDisabled("%s", m_pending_operation_path.generic_string().c_str());
		ImGui::SetNextItemWidth(360.0f);
		ImGui::InputText("Name", &m_operation_text);
	}
	else if (m_pending_operation == file_operation::move)
	{
		ImGui::TextUnformatted("Move asset");
		ImGui::TextDisabled("%s", m_pending_operation_path.generic_string().c_str());
		ImGui::SetNextItemWidth(360.0f);
		ImGui::InputTextWithHint("Destination", "Relative folder under Assets, e.g. textures/ui", &m_operation_text);
	}
	else if (m_pending_operation == file_operation::delete_path)
	{
		ImGui::TextWrapped("Delete this %s from disk?", m_pending_operation_is_directory ? "folder" : "file");
		ImGui::TextDisabled("%s", m_pending_operation_path.generic_string().c_str());
	}
	else if (m_pending_operation == file_operation::remove_registry)
	{
		ImGui::TextWrapped("Remove this asset from the registry?");
		ImGui::TextDisabled("%s", m_pending_operation_path.generic_string().c_str());
	}

	if (!m_operation_error.empty())
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.95f, 0.50f, 0.34f, 1.0f), "%s", m_operation_error.c_str());
	}

	ImGui::Spacing();
	const char* confirm_label = m_pending_operation == file_operation::delete_path ? "Delete" :
		m_pending_operation == file_operation::remove_registry ? "Remove" :
		m_pending_operation == file_operation::move ? "Move" : "Rename";
	if (ImGui::Button(confirm_label, ImVec2(108.0f, 0.0f)))
	{
		bool success = false;
		switch (m_pending_operation)
		{
		case file_operation::rename: success = rename_pending_item(); break;
		case file_operation::move: success = move_pending_item(); break;
		case file_operation::delete_path: success = delete_pending_item(); break;
		case file_operation::remove_registry: success = remove_pending_registry_entry(); break;
		default: break;
		}

		if (success)
		{
			clear_pending_operation();
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(108.0f, 0.0f)))
	{
		clear_pending_operation();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
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
			item.missing = !std::filesystem::exists(item.absolute_path);

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
	m_preferences_dirty = true;
}

bool content_browser_panel::import_file(const std::filesystem::path& relative_path, import_summary* summary)
{
	const std::filesystem::path absolute_path = m_base_directory / relative_path;
	if (!std::filesystem::exists(absolute_path))
	{
		if (summary)
			++summary->missing;
		set_status("Import failed: file is missing.", true);
		return false;
	}

	if (utils::try_get_asset_type_from_file_extension(relative_path.extension()) == asset_type::none)
	{
		if (summary)
			++summary->unsupported;
		set_status("Import skipped: unsupported file format.", true);
		return false;
	}

	if (find_asset_handle(relative_path) != 0)
	{
		if (summary)
			++summary->already_imported;
		set_status("Asset is already registered.");
		return true;
	}

	asset_handle handle = m_project->get_editor_asset_manager()->import_asset(relative_path);
	if (handle != 0)
	{
		if (summary)
			++summary->imported;
		set_status("Asset imported: " + relative_path.generic_string());
		refresh_asset_tree();
		return true;
	}

	if (summary)
		++summary->failed;
	set_status("Import failed: " + relative_path.generic_string(), true);
	return false;
}

void content_browser_panel::import_current_directory(bool recursive)
{
	std::error_code error;
	if (!std::filesystem::exists(m_current_directory, error))
		return;

	import_summary summary;
	if (recursive)
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(m_current_directory, error))
		{
			if (!entry.is_regular_file(error))
				continue;

			browser_item item = make_filesystem_item(entry);
			import_file(item.relative_path, &summary);
		}
	}
	else
	{
		for (const auto& entry : std::filesystem::directory_iterator(m_current_directory, error))
		{
			if (!entry.is_regular_file(error))
				continue;

			browser_item item = make_filesystem_item(entry);
			import_file(item.relative_path, &summary);
		}
	}

	refresh_asset_tree();
	set_status(import_summary_text(summary), summary.failed > 0 || summary.missing > 0);
}

void content_browser_panel::request_remove_asset(asset_handle handle, const std::filesystem::path& relative_path)
{
	m_pending_operation = file_operation::remove_registry;
	m_pending_operation_handle = handle;
	m_pending_operation_path = relative_path;
	m_pending_operation_is_directory = false;
	m_operation_text.clear();
	m_operation_error.clear();
}

void content_browser_panel::request_rename_item(const browser_item& item)
{
	m_pending_operation = file_operation::rename;
	m_pending_operation_handle = item.handle;
	m_pending_operation_path = item.relative_path;
	m_pending_operation_is_directory = item.directory;
	m_operation_text = item.relative_path.filename().string();
	m_operation_error.clear();
}

void content_browser_panel::request_move_item(const browser_item& item)
{
	m_pending_operation = file_operation::move;
	m_pending_operation_handle = item.handle;
	m_pending_operation_path = item.relative_path;
	m_pending_operation_is_directory = item.directory;
	std::filesystem::path parent = item.relative_path.parent_path();
	m_operation_text = parent.empty() ? "" : parent.generic_string();
	m_operation_error.clear();
}

void content_browser_panel::request_delete_item(const browser_item& item)
{
	m_pending_operation = file_operation::delete_path;
	m_pending_operation_handle = item.handle;
	m_pending_operation_path = item.relative_path;
	m_pending_operation_is_directory = item.directory;
	m_operation_text.clear();
	m_operation_error.clear();
}

bool content_browser_panel::open_asset(const browser_item& item)
{
	if (item.directory || item.missing || !item.supported)
		return false;

	asset_handle handle = item.handle;
	if (handle == 0)
	{
		if (!import_file(item.relative_path))
			return false;
		handle = find_asset_handle(item.relative_path);
	}

	if (handle == 0 || !m_asset_open_callback)
		return false;

	if (m_asset_open_callback(handle))
	{
		set_status("Opened: " + item.relative_path.generic_string());
		return true;
	}

	return false;
}

bool content_browser_panel::set_scene_as_start_scene(const browser_item& item)
{
	if (!m_project || item.directory || item.missing || item.type != asset_type::scene)
		return false;

	asset_handle handle = item.handle;
	if (handle == 0)
	{
		if (!import_file(item.relative_path))
			return false;
		handle = find_asset_handle(item.relative_path);
	}

	if (handle == 0)
		return false;

	m_project->get_config().start_scene = handle;
	project::save_active();
	set_status("Start scene set: " + item.relative_path.generic_string());
	return true;
}

void content_browser_panel::clear_pending_operation()
{
	m_pending_operation = file_operation::none;
	m_pending_operation_handle = 0;
	m_pending_operation_path.clear();
	m_pending_operation_is_directory = false;
	m_operation_text.clear();
	m_operation_error.clear();
}

bool content_browser_panel::rename_pending_item()
{
	if (m_operation_text.empty())
	{
		m_operation_error = "Name is required.";
		return false;
	}

	std::filesystem::path source = m_base_directory / m_pending_operation_path;
	std::filesystem::path new_name = m_operation_text;
	if (!m_pending_operation_is_directory && new_name.extension().empty())
		new_name.replace_extension(m_pending_operation_path.extension());
	std::filesystem::path target = source.parent_path() / new_name.filename();

	if (!is_inside_base_directory(target))
	{
		m_operation_error = "Target must stay inside Assets.";
		return false;
	}
	if (std::filesystem::exists(target))
	{
		m_operation_error = "An item with that name already exists.";
		return false;
	}

	std::error_code error;
	std::filesystem::rename(source, target, error);
	if (error)
	{
		m_operation_error = error.message();
		return false;
	}

	const std::filesystem::path target_relative = make_relative_path(target);
	if (m_pending_operation_is_directory)
		m_project->get_editor_asset_manager()->update_asset_directory_paths(m_pending_operation_path, target_relative);
	else if (m_pending_operation_handle != 0)
		m_project->get_editor_asset_manager()->update_asset_filepath(m_pending_operation_handle, target_relative);

	if (path_is_under_directory(m_current_directory, source))
		m_current_directory = target;
	refresh_asset_tree();
	set_status("Renamed: " + target_relative.generic_string());
	return true;
}

bool content_browser_panel::move_pending_item()
{
	std::filesystem::path destination_directory = m_operation_text.empty() ? m_base_directory : m_base_directory / m_operation_text;
	return move_path_to_directory(m_pending_operation_path, destination_directory);
}

bool content_browser_panel::delete_pending_item()
{
	if (!m_project || !m_project->get_editor_asset_manager())
	{
		m_operation_error = "No project asset manager is available.";
		return false;
	}

	const std::filesystem::path absolute_path = m_base_directory / m_pending_operation_path;
	std::error_code error;
	const bool path_exists = std::filesystem::exists(absolute_path, error);
	if (error)
	{
		m_operation_error = error.message();
		return false;
	}

	ref<project> active_project = project::get_active();
	bool clears_start_scene = false;
	if (m_pending_operation_is_directory)
	{
		if (active_project && active_project->get_editor_asset_manager() && active_project->get_config().start_scene != 0 && active_project->get_editor_asset_manager()->is_asset_handle_valid(active_project->get_config().start_scene))
		{
			const std::filesystem::path start_scene_path = active_project->get_editor_asset_manager()->get_filepath(active_project->get_config().start_scene);
			if (path_is_under_directory(start_scene_path, m_pending_operation_path))
				clears_start_scene = true;
		}

		if (path_exists)
			std::filesystem::remove_all(absolute_path, error);
	}
	else
	{
		if (!path_exists && m_pending_operation_handle == 0)
		{
			m_operation_error = "Item is missing.";
			return false;
		}

		clears_start_scene = active_project && m_pending_operation_handle != 0 && active_project->get_config().start_scene == m_pending_operation_handle;

		if (path_exists)
			std::filesystem::remove(absolute_path, error);
	}

	if (error)
	{
		m_operation_error = error.message();
		return false;
	}

	if (clears_start_scene && active_project)
	{
		active_project->get_config().start_scene = 0;
		project::save_active();
	}

	if (m_pending_operation_is_directory)
		m_project->get_editor_asset_manager()->delete_assets_under_directory(m_pending_operation_path);
	else if (m_pending_operation_handle != 0)
		m_project->get_editor_asset_manager()->delete_asset(m_pending_operation_handle);

	if (path_is_under_directory(m_current_directory, absolute_path))
		m_current_directory = m_base_directory;
	refresh_asset_tree();
	set_status("Deleted: " + m_pending_operation_path.generic_string());
	return true;
}

bool content_browser_panel::remove_pending_registry_entry()
{
	if (!m_project || !m_project->get_editor_asset_manager())
	{
		m_operation_error = "No project asset manager is available.";
		return false;
	}

	if (m_pending_operation_handle == 0)
	{
		m_operation_error = "No asset registration selected.";
		return false;
	}

	ref<project> active_project = project::get_active();
	if (active_project && active_project->get_config().start_scene == m_pending_operation_handle)
	{
		active_project->get_config().start_scene = 0;
		project::save_active();
	}

	m_project->get_editor_asset_manager()->delete_asset(m_pending_operation_handle);
	refresh_asset_tree();
	set_status("Removed registry entry: " + m_pending_operation_path.generic_string());
	return true;
}

bool content_browser_panel::duplicate_item(const browser_item& item)
{
	if (item.missing)
	{
		set_status("Duplicate failed: source asset is missing.", true);
		return false;
	}

	std::filesystem::path target = make_unique_copy_path(item.absolute_path);
	std::error_code error;
	if (item.directory)
		std::filesystem::copy(item.absolute_path, target, std::filesystem::copy_options::recursive, error);
	else
		std::filesystem::copy_file(item.absolute_path, target, std::filesystem::copy_options::none, error);

	if (error)
	{
		set_status("Duplicate failed: " + error.message(), true);
		return false;
	}

	import_summary summary;
	if (item.directory)
		import_supported_files_under(target, summary);
	else if (utils::try_get_asset_type_from_file_extension(target.extension()) != asset_type::none)
		import_file(make_relative_path(target), &summary);

	refresh_asset_tree();
	if (summary.imported > 0 || summary.failed > 0 || summary.unsupported > 0)
		set_status("Duplicated: " + make_relative_path(target).generic_string() + " | " + import_summary_text(summary), summary.failed > 0);
	else
		set_status("Duplicated: " + make_relative_path(target).generic_string());
	return true;
}

bool content_browser_panel::move_path_to_directory(const std::filesystem::path& source_relative_path, const std::filesystem::path& destination_directory)
{
	const std::filesystem::path source = m_base_directory / source_relative_path;
	if (!std::filesystem::exists(source))
	{
		m_operation_error = "Source item is missing.";
		set_status("Move failed: source item is missing.", true);
		return false;
	}
	if (!std::filesystem::exists(destination_directory) || !std::filesystem::is_directory(destination_directory))
	{
		m_operation_error = "Destination folder does not exist.";
		set_status("Move failed: destination folder does not exist.", true);
		return false;
	}
	if (!is_inside_base_directory(destination_directory))
	{
		m_operation_error = "Destination must stay inside Assets.";
		set_status("Move failed: destination must stay inside Assets.", true);
		return false;
	}
	if (path_is_under_directory(destination_directory, source))
	{
		m_operation_error = "Cannot move a folder into itself.";
		set_status("Move failed: cannot move a folder into itself.", true);
		return false;
	}

	std::filesystem::path target = destination_directory / source.filename();
	if (source.lexically_normal() == target.lexically_normal())
		return true;
	if (std::filesystem::exists(target))
	{
		m_operation_error = "Destination already contains an item with this name.";
		set_status("Move failed: destination item already exists.", true);
		return false;
	}

	const bool source_is_directory = std::filesystem::is_directory(source);
	std::error_code error;
	std::filesystem::rename(source, target, error);
	if (error)
	{
		m_operation_error = error.message();
		set_status("Move failed: " + error.message(), true);
		return false;
	}

	const std::filesystem::path target_relative = make_relative_path(target);
	if (source_is_directory)
		m_project->get_editor_asset_manager()->update_asset_directory_paths(source_relative_path, target_relative);
	else if (asset_handle handle = find_asset_handle(source_relative_path); handle != 0)
		m_project->get_editor_asset_manager()->update_asset_filepath(handle, target_relative);

	if (path_is_under_directory(m_current_directory, source))
		m_current_directory = target;
	refresh_asset_tree();
	set_status("Moved: " + source_relative_path.generic_string() + " -> " + target_relative.generic_string());
	return true;
}

void content_browser_panel::import_supported_files_under(const std::filesystem::path& directory, import_summary& summary)
{
	std::error_code error;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, error))
	{
		if (!entry.is_regular_file(error))
			continue;
		import_file(make_relative_path(entry.path()), &summary);
	}
}

bool content_browser_panel::handle_external_drop(const std::vector<std::filesystem::path>& paths)
{
	if (!m_project || paths.empty())
		return false;

	import_summary summary;
	bool handled = false;
	for (const auto& source_path : paths)
		handled = import_external_path(source_path, summary) || handled;

	refresh_asset_tree();
	set_status("External drop: " + import_summary_text(summary), summary.failed > 0 || summary.missing > 0);
	return handled;
}

bool content_browser_panel::import_external_path(const std::filesystem::path& source_path, import_summary& summary)
{
	std::error_code error;
	if (!std::filesystem::exists(source_path, error))
	{
		++summary.missing;
		return false;
	}

	const bool source_inside_assets = is_inside_base_directory(source_path);
	std::filesystem::path import_path = source_path;
	if (!source_inside_assets)
	{
		std::filesystem::create_directories(m_current_directory, error);
		if (error)
		{
			++summary.failed;
			return false;
		}

		import_path = make_unique_import_path(m_current_directory / source_path.filename());
		error.clear();
		if (std::filesystem::is_directory(source_path, error))
			std::filesystem::copy(source_path, import_path, std::filesystem::copy_options::recursive, error);
		else if (std::filesystem::is_regular_file(source_path, error))
			std::filesystem::copy_file(source_path, import_path, std::filesystem::copy_options::none, error);
		else
		{
			++summary.unsupported;
			return false;
		}

		if (error)
		{
			++summary.failed;
			set_status("External drop failed: " + error.message(), true);
			return false;
		}
	}

	error.clear();
	if (std::filesystem::is_directory(import_path, error))
	{
		import_supported_files_under(import_path, summary);
		return true;
	}

	if (std::filesystem::is_regular_file(import_path, error))
		return import_file(make_relative_path(import_path), &summary);

	++summary.unsupported;
	return false;
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

std::filesystem::path content_browser_panel::make_relative_path(const std::filesystem::path& absolute_path) const
{
	std::error_code error;
	std::filesystem::path relative_path = std::filesystem::relative(absolute_path, m_base_directory, error);
	if (error)
		return absolute_path.filename();
	return relative_path.lexically_normal();
}

std::filesystem::path content_browser_panel::make_unique_copy_path(const std::filesystem::path& absolute_path) const
{
	const std::filesystem::path parent = absolute_path.parent_path();
	const std::string stem = absolute_path.stem().string();
	const std::string extension = absolute_path.extension().string();
	std::filesystem::path candidate = parent / (stem + " Copy" + extension);
	int suffix = 2;
	while (std::filesystem::exists(candidate))
		candidate = parent / (stem + " Copy " + std::to_string(suffix++) + extension);
	return candidate;
}

std::filesystem::path content_browser_panel::make_unique_import_path(const std::filesystem::path& absolute_path) const
{
	if (!std::filesystem::exists(absolute_path))
		return absolute_path;
	return make_unique_copy_path(absolute_path);
}

void content_browser_panel::set_status(std::string message, bool error)
{
	m_status_message = std::move(message);
	m_status_error = error;
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

		if (ImGui::BeginPopupModal("Content Browser Settings", NULL, ImGuiWindowFlags_NoResize))
		{
			ImGui::Text("Thumbnail Size");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("##Thumbnail Size", &m_thumbnail_size, 0.5f, 16, 512))
				m_preferences_dirty = true;
			ImGui::Text("Padding");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("##Padding", &m_padding, 0.05f, 0, 32))
				m_preferences_dirty = true;
			if (ImGui::Checkbox("Show unsupported files", &m_show_unsupported))
				m_preferences_dirty = true;
			const float footer_y = ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().WindowPadding.y;
			if (ImGui::GetCursorPosY() < footer_y)
				ImGui::SetCursorPosY(footer_y);
			ImGui::Separator();
			ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 112.0f);
			if (ImGui::Button("OK", ImVec2(96.0f, 0.0f)))
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

content_browser_panel::preferences content_browser_panel::get_preferences() const
{
	preferences prefs;
	prefs.thumbnail_size = m_thumbnail_size;
	prefs.padding = m_padding;
	prefs.show_unsupported = m_show_unsupported;
	prefs.open = m_open;
	prefs.mode = static_cast<int>(m_mode);
	prefs.type_filter = static_cast<int>(m_type_filter);
	prefs.current_directory = m_current_directory;
	return prefs;
}

void content_browser_panel::apply_preferences(const preferences& prefs)
{
	m_thumbnail_size = std::clamp(prefs.thumbnail_size, 16.0f, 512.0f);
	m_padding = std::clamp(prefs.padding, 0.0f, 32.0f);
	m_show_unsupported = prefs.show_unsupported;
	m_open = prefs.open;
	m_mode = prefs.mode == static_cast<int>(mode::asset) ? mode::asset : mode::filesystem;
	m_type_filter = static_cast<asset_type>(prefs.type_filter);
	if (!prefs.current_directory.empty())
		set_current_directory(prefs.current_directory);
	m_preferences_dirty = false;
}

bool content_browser_panel::consume_preferences_dirty()
{
	bool dirty = m_preferences_dirty;
	m_preferences_dirty = false;
	return dirty;
}

void content_browser_panel::set_open(bool open)
{
	if (m_open == open)
		return;
	m_open = open;
	m_preferences_dirty = true;
}

_WHIP_END
