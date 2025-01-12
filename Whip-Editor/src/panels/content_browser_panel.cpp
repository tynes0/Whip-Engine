#include <whippch.h>

#include <Whip/Core/Application.h>
#include <Whip/Utils/platform_utils.h>
#include <Whip/Project/project.h>
#include <Whip/Asset/asset_manager.h>
#include <Whip/Asset/texture_importer.h>

#include "content_browser_panel.h"

#include <imgui/imgui.h>

_WHIP_START

content_browser_panel::content_browser_panel()
{
	m_initialized = false;
}

content_browser_panel::content_browser_panel(ref<project> proj) : m_project(proj), m_thumbnail_cache(make_ref<thumbnail_cache>(proj)), m_base_directory(m_project->get_asset_directory()), m_current_directory(m_base_directory)
{
	m_tree_nodes.push_back(tree_node(".", 0));
	m_directory_icon	= texture_importer::load_texture2D("resources/icons/content_browser/directory_icon.png");
	m_file_icon			= texture_importer::load_texture2D("resources/icons/content_browser/file_icon.png");
	m_return_icon		= texture_importer::load_texture2D("resources/icons/return_icon.png");
	refresh_asset_tree();
	m_mode = mode::filesystem;
	m_initialized = true;
}

void content_browser_panel::init(ref<project> proj)
{
	m_project = proj;
	m_thumbnail_cache = make_ref<thumbnail_cache>(proj);
	m_current_directory = m_base_directory = m_project->get_asset_directory();
	m_tree_nodes.push_back(tree_node(".", 0));
	m_directory_icon = texture_importer::load_texture2D("resources/icons/content_browser/directory_icon.png");
	m_file_icon = texture_importer::load_texture2D("resources/icons/content_browser/file_icon.png");
	m_return_icon = texture_importer::load_texture2D("resources/icons/return_icon.png");
	refresh_asset_tree();
	m_mode = mode::filesystem;
	m_initialized = true;
}

void content_browser_panel::on_imgui_render()
{
	ImGui::Begin("Content Browser");

	if(m_initialized)
	{
		const char* label = m_mode == mode::asset ? "Asset" : "File";
		if (ImGui::Button(label))
			m_mode = m_mode == mode::asset ? mode::filesystem : mode::asset;

		if (m_current_directory != m_base_directory)
		{
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ref<texture2D> icon = m_return_icon;
			if (ImGui::ImageButton(reinterpret_cast<ImTextureID>((uint64_t)icon->get_renderer_id()), { 16.0f, 16.0f }, { 0, 1 }, { 1, 0 }))
				m_current_directory = m_current_directory.parent_path();
			ImGui::PopStyleColor();
		}

		float cell_size = m_thumbnail_size + m_padding;

		float panel_width = ImGui::GetContentRegionAvail().x;
		int column_count = (int)(panel_width / cell_size);
		if (column_count < 1)
			column_count = 1;

	ImGui::Columns(column_count, 0, false);

		if (m_mode == mode::asset)
		{
			tree_node* node = &m_tree_nodes[0];

			auto currentDir = std::filesystem::relative(m_current_directory, project::get_active_asset_directory());
			for (const auto& p : currentDir)
			{
				// if only one level
				if (node->path == currentDir)
					break;

				if (node->children.find(p) != node->children.end())
				{
					node = &m_tree_nodes[node->children[p]];
					continue;
				}
				else
				{
					// can't find path
					WHP_CORE_WARN("There is no asset in this filepath!");
					m_mode = mode::filesystem;
				}

			}

			for (const auto& [item, treeNodeIndex] : node->children)
			{
				bool isDirectory = std::filesystem::is_directory(project::get_active_asset_directory() / item);

				std::string itemStr = item.generic_string();

				ImGui::PushID(itemStr.c_str());
				ref<texture2D> icon = isDirectory ? m_directory_icon : m_file_icon;

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::ImageButton(reinterpret_cast<ImTextureID>((uint64_t)icon->get_renderer_id()), { m_thumbnail_size, m_thumbnail_size }, { 0, 1 }, { 1, 0 });

				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem("Delete"))
					{
						auto& node = m_tree_nodes[treeNodeIndex];
						auto parent_index = m_tree_nodes[treeNodeIndex].parent;
						auto& parent_node = m_tree_nodes[parent_index];
						auto& child_map = parent_node.children;
						auto it = child_map.find(node.path);
						size_t index = treeNodeIndex;
						if (it != child_map.end())
							child_map.erase(it);

						asset_handle handle = node.handle;
						project::get_active()->get_editor_asset_manager()->delete_asset(handle);
						m_tree_nodes.erase(m_tree_nodes.begin() + index);
						ImGui::EndPopup();
						ImGui::PopStyleColor();
						ImGui::PopID();
						break;
					}

					static constexpr ImVec2 popup_size(50.0f, 50.0f);
					if (ImGui::MenuItem("Import Subtextures"))
					{
						if (ImGui::BeginPopupModal("Import Subtextures", NULL, ImGuiWindowFlags_AlwaysAutoResize))
						{
							static float size[]{ 0,0 };
							ImGui::Text("Sprite Size");
							ImGui::InputFloat2("##SpriteSize", size);
							if (ImGui::Button("Close"))
							{
								ImGui::CloseCurrentPopup();
							}
							ImGui::EndPopup();
						}
						ref<texture2D> base_tex = asset_manager::get_asset<texture2D>(node->handle);

						//texture2D::create_from_coords(base_tex, );
					}				

					ImGui::EndPopup();
				}

				if (ImGui::BeginDragDropSource())
				{
					asset_handle handle = m_tree_nodes[treeNodeIndex].handle;
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", &handle, sizeof(asset_handle));
					ImGui::EndDragDropSource();
				}

				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
				{
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && isDirectory)
						m_current_directory /= item.filename();
					
				}

				ImGui::TextWrapped(itemStr.c_str());

				ImGui::NextColumn();

				ImGui::PopID();
			}
		}
		else
		{
			for (auto& directoryEntry : std::filesystem::directory_iterator(m_current_directory))
			{
				const auto& path = directoryEntry.path();
				std::string filenameString = path.filename().string();

				ImGui::PushID(filenameString.c_str());
				ref<texture2D> icon = directoryEntry.is_directory() ? m_directory_icon : m_file_icon;

				auto relativePath = std::filesystem::relative(path, project::get_active_asset_directory());
				ref<texture2D> thumbnail = m_directory_icon;
				if (!directoryEntry.is_directory())
				{
					thumbnail = m_thumbnail_cache->get_or_create_thumbnail(relativePath);
					if (!thumbnail)
						thumbnail = m_file_icon;
				}

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::ImageButton((ImTextureID)(uint64_t)thumbnail->get_renderer_id(), { m_thumbnail_size, m_thumbnail_size }, { 0, 1 }, { 1, 0 });

				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem("Import"))
					{
						project::get_active()->get_editor_asset_manager()->import_asset(relativePath);
						refresh_asset_tree();
					}
					ImGui::EndPopup();
				}

				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (directoryEntry.is_directory())
						m_current_directory /= path.filename();
				}

				ImGui::TextWrapped(filenameString.c_str());

				ImGui::NextColumn();

				ImGui::PopID();
			}
		}
	}
	
	if (ImGui::BeginPopupContextWindow(0, 1 | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Settings"))
			m_show_settings_popup = true;
		ImGui::EndPopup();
	}
	on_settings_popup();
	ImGui::End();
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
	const auto& asset_reg = project::get_active()->get_editor_asset_manager()->get_asset_registry();
	asset_reg.foreach([this](const asset_registry::value_type& value)
		{
			uint32_t current_node_index = 0;
			for (const auto& p : value.second.filepath)
			{
				auto it = m_tree_nodes[current_node_index].children.find(p.generic_string());
				if (it != m_tree_nodes[current_node_index].children.end())
				{
					current_node_index = it->second;
				}
				else
				{
					// add node
					tree_node newNode(p, value.first);
					newNode.parent = current_node_index;
					m_tree_nodes.push_back(newNode);

					m_tree_nodes[current_node_index].children[p] = static_cast<uint32_t>(m_tree_nodes.size() - 1);
					current_node_index = static_cast<uint32_t>(m_tree_nodes.size() - 1);
				}
			}
		});
	
}

void content_browser_panel::init_popups()
{
	static float sprite_width = 0.0f, sprite_height = 0.0f;

	m_subtexture_popup
		.set_popup_name("Import Subtextures")
		.set_width(200).set_height(200)
		.add([]() { ImGui::Text("Sprite Width:"); })
		.same_line()
		.add([]() { ImGui::InputFloat("##SpriteWidth", &sprite_width); })
		.add([]() { ImGui::Text("Sprite Height:"); })
		.same_line()
		.add([]() { ImGui::InputFloat("##SpriteHeight", &sprite_height); })
		//.add_button([](raw_buffer user_data)
		//	{ 
		//		asset_handle handle = user_data.load<asset_handle>(); 
		//		auto atlas = asset_manager::get_asset<texture2D>(handle); 
		//		/*for (uint32_t i = 0; i < atlas->get_width();)
		//		{
		//			for (uint32_t j = 0; j < atlas->get_height();)
		//			{
		//				atlas->create_from_coords();
		//			}
		//		*/
		//	}, "Import", 50, 25);
		;
}

_WHIP_END
