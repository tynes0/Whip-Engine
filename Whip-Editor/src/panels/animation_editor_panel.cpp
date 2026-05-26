#include "animation_editor_panel.h"
#include <Whip/Core/Application.h>
#include <Whip/Core/utility.h>
#include <Whip/Project/project.h>
#include <Whip/Asset/asset_manager.h>
#include <Whip/Asset/animation_importer.h>
#include <Whip/UI/UI_helpers.h>
#include <Whip/Animation/animation_manager.h>
#include <Whip/Utils/platform_utils.h>
#include "../Helpers/icon_manager.h"

#include <imgui.h>
#include <imgui_internal.h>

_WHIP_START

animation_editor_panel::animation_editor_panel()
{
}

animation_editor_panel::~animation_editor_panel() {}

// maybe improve
void animation_editor_panel::on_imgui_render()
{
	if (!m_open)
		return;
	ImGui::Begin("Animation Editor", &m_open);

	ImVec2 avail = ImGui::GetContentRegionAvail();
	draw_animation_drag_drop_area(avail.x, avail.y);

	ImGui::BeginChild("##AnimationEditorToolbar", ImVec2(0.0f, 54.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::AlignTextToFramePadding();
	draw_playback_controls(30.0f, 30.0f);
	ImGui::SameLine(0.0f, 14.0f);

	if (ImGui::Button("New", ImVec2(92.0f, 30.0f)))
	{
		m_current_animation.reset();
		ref<animation2D> new_anim = make_ref<animation2D>();
		new_anim->set_name("New Animation");
		m_current_animation = new_anim;
		m_selected_frame_index = -1;
		std::string filepath = file_dialogs::save_file("Whip Animation (*.wanim)\0*.wanim\0", project::get_active_asset_directory().string().c_str());
		if (!filepath.empty())
		{
			m_current_animation->serialize(filepath);
			asset_metadata metadata;
			metadata.type = asset_type::animation;
			metadata.filepath = std::filesystem::relative(filepath, project::get_active_asset_directory());
			project::get_active()->get_editor_asset_manager()->add_registry(m_current_animation->handle, metadata);
			project::get_active()->get_editor_asset_manager()->serialize_asset_registry();
			if (m_refresh_asset_tree_callback)
				m_refresh_asset_tree_callback();
		}
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!m_current_animation);
	if (ImGui::Button("Close", ImVec2(92.0f, 30.0f)))
	{
		m_current_animation = nullptr;
		m_selected_frame_index = -1;
	}
	ImGui::SameLine();
	if (ImGui::Button("Save", ImVec2(92.0f, 30.0f)))
	{
		const auto& metadata = asset_manager::get_asset_metadata(m_current_animation->handle);
		if (metadata)
			m_current_animation->serialize(project::get_active_asset_directory() / metadata.filepath);
	}

	ImGui::SameLine(0.0f, 14.0f);
	if (m_current_animation)
	{
		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		strncpy_s(buffer, sizeof(buffer), m_current_animation->get_name().c_str(), sizeof(buffer));
		ImGui::SetNextItemWidth(170.0f);
		if (ImGui::InputText("##AnimationName", buffer, sizeof(buffer)))
			m_current_animation->set_name(buffer);
		ImGui::SameLine();
	}
	ImGui::EndDisabled();

	ImGui::SetNextItemWidth(220.0f);
	if (ImGui::BeginCombo("##AnimationSelector", m_current_animation ? m_current_animation->get_name().data() : "Select Animation"))
	{
		const auto& reg = project::get_active()->get_editor_asset_manager()->get_asset_registry();
		reg.foreach(asset_type::animation, [&](const asset_registry::value_type& value)
			{
				auto anim = asset_manager::get_asset<animation2D>(value.first);
				if (ImGui::Selectable(anim->get_name().c_str(), m_current_animation ? m_current_animation->handle == value.first : false))
				{
					m_current_animation = anim;
					m_selected_frame_index = -1;
				}
		});
		ImGui::EndCombo();
	}
	ImGui::EndChild();

	ImGui::Spacing();
	if (!m_current_animation)
	{
		ImGui::BeginChild("##AnimationEditorEmpty", ImVec2(0.0f, 0.0f), true);
		ImGui::TextDisabled("Drop an animation here or create a new one.");
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	draw_timeline(ImGui::GetContentRegionAvail().x, 112.0f, 150.0f);

	ImGui::BeginChild("##AnimationEditorFrameTools", ImVec2(0.0f, 46.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	draw_frame_list(220.0f);
	ImGui::SameLine();
	draw_add_frame_button(120.0f);
	ImGui::SameLine();
	draw_remove_frame_button(140.0f);
	ImGui::EndChild();

	draw_frame_editor(ImGui::GetContentRegionAvail().x);

	ImGui::End();
}

void animation_editor_panel::draw_animation_drag_drop_area(float width, float height)
{
	static const auto drag_drop_callback = [this](asset_handle handle)
		{
			m_current_animation = asset_manager::get_asset<animation2D>(handle);
		};

	UI::drag_drop_target(asset_type::animation, drag_drop_callback, "Select Animation", true, width, height, false);
}

void animation_editor_panel::draw_playback_controls(float width, float height)
{
	ImVec2 size(width, height);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.12f, 0.13f, 0.92f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.22f, 0.22f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.34f, 0.32f, 1.0f));

	auto draw_button = [=](icon icon_type) -> bool
		{
			if (frenum::contains(icon_type))
			{
				const std::string button_id = "##AnimationControl" + std::to_string(static_cast<int>(icon_type));
				return UI::image_button(button_id.c_str(), UI::to_imgui_texture_id(icon_manager::get().get_icon(icon_type)->get_renderer_id()), size, ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0.0f, 0.0f, 0.0f, 0.0f), ImVec4(1, 1, 1, 1));
			}
			return ImGui::Button(frenum::to_string<icon>(icon_type).c_str());
		};

	if (draw_button(icon::step_back))
	{
	}
	ImGui::SameLine();
	if (draw_button(icon::play))
	{
	}
	ImGui::SameLine();
	if (draw_button(icon::pause))
	{
	}
	ImGui::SameLine();
	if (draw_button(icon::stop))
	{
	}
	ImGui::SameLine();
	if (draw_button(icon::step_forward))
	{
	}

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void animation_editor_panel::draw_new_button(float width, float left_padding)
{
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (left_padding + width));
	if (ImGui::Button("New", ImVec2(width, 0.0f)))
	{
		m_current_animation.reset();
		ref<animation2D> new_anim = make_ref<animation2D>();
		new_anim->set_name("New Animation");
		m_current_animation = new_anim;
		m_selected_frame_index = -1;
		std::string filepath = file_dialogs::save_file("Whip Animation (*.wanim)\0*.wanim\0", project::get_active_asset_directory().string().c_str());
		if (!filepath.empty())
		{
			m_current_animation->serialize(filepath);
			asset_metadata metadata;
			metadata.type = asset_type::animation;
			metadata.filepath = std::filesystem::relative(filepath, project::get_active_asset_directory());;
			project::get_active()->get_editor_asset_manager()->add_registry(m_current_animation->handle, metadata);
			project::get_active()->get_editor_asset_manager()->serialize_asset_registry();
			if (m_refresh_asset_tree_callback)
				m_refresh_asset_tree_callback();
		}
	}
}

void animation_editor_panel::draw_close_button(float width, float left_padding)
{
	if (!m_current_animation)
		return;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (left_padding + width));
	if (ImGui::Button("Close", ImVec2(width, 0.0f)))
	{
		m_current_animation = nullptr;
		m_selected_frame_index = -1;
	}
}

void animation_editor_panel::draw_save_button(float width, float left_padding)
{
	if (!m_current_animation)
		return;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (left_padding + width));
	if (ImGui::Button("Save", ImVec2(width, 0.0f)))
	{
		const auto& metadata = asset_manager::get_asset_metadata(m_current_animation->handle);
		if (metadata)
			m_current_animation->serialize(project::get_active_asset_directory() / metadata.filepath);
		else
		{
		}
	}
}

void animation_editor_panel::draw_name_input(float width, float left_padding)
{
	if (!m_current_animation)
		return;
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (left_padding + width));
	char buffer[256];
	memset(buffer, 0, sizeof(buffer));
	strncpy_s(buffer, sizeof(buffer), m_current_animation->get_name().c_str(), sizeof(buffer));
	ImGui::SetNextItemWidth(width);
	if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
		m_current_animation->set_name(buffer);
}

void animation_editor_panel::draw_animation_selector(float width, float left_padding)
{
	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (width + left_padding));
	ImGui::SetNextItemWidth(width);
	if (ImGui::BeginCombo("##AnimationSelector", m_current_animation ? m_current_animation->get_name().data() : "Select Animation"))
	{
		const auto& reg = project::get_active()->get_editor_asset_manager()->get_asset_registry();

		reg.foreach(asset_type::animation, [&](const asset_registry::value_type& value)
			{
				auto anim = asset_manager::get_asset<animation2D>(value.first);
				if (ImGui::Selectable(anim->get_name().c_str(), m_current_animation ? m_current_animation->handle == value.first : false, 0, ImVec2(width - ImGui::GetStyle().WindowPadding.x, 0.0f)))
					m_current_animation = asset_manager::get_asset<animation2D>(value.first);
			});
		ImGui::EndCombo();
	}
}

void animation_editor_panel::draw_timeline(float width, float timeline_height, float total_height)
{
	if (!m_current_animation)
		return;
	UI::draw_timeline_with_nodes_sl(m_current_animation, 4.0f, width, timeline_height, total_height, 120.0f, &m_selected_frame_index);
}

void animation_editor_panel::draw_frame_list(float width)
{
	if (!m_current_animation)
		return;
	auto& frames = m_current_animation->get_frames();

	ImGui::SetNextItemWidth(width);
	if (ImGui::BeginCombo("##FrameList", m_selected_frame_index != -1 ? ("Frame " + std::to_string(m_selected_frame_index)).c_str() : "Select Frame"))
	{
		for (size_t i = 0; i < frames.size(); ++i)
		{
			std::string label = "Frame " + std::to_string(i);
			if (ImGui::Selectable(label.c_str(), m_selected_frame_index == i, 0, ImVec2(width - ImGui::GetStyle().WindowPadding.x, 0.0f)))
				m_selected_frame_index = (int)i;
		}
		ImGui::EndCombo();
	}
}

void animation_editor_panel::draw_add_frame_button(float width)
{
	if (!m_current_animation)
		return;
	if (ImGui::Button("Add Frame", ImVec2(width, 0.0f)))
	{
		m_current_animation->add_frame(animation_frame());
		m_selected_frame_index = int(m_current_animation->get_frames().size() - 1);
	}
}

void animation_editor_panel::draw_remove_frame_button(float width)
{
	if (!m_current_animation || m_selected_frame_index < 0)
		return;
	if (ImGui::Button("Remove Frame", ImVec2(width, 0.0f)))
	{
		m_current_animation->remove_frame(m_selected_frame_index);
		m_selected_frame_index = -1;
	}
}

void animation_editor_panel::draw_frame_editor(float width)
{
	if (!m_current_animation || m_selected_frame_index < 0 || m_selected_frame_index >= (int)m_current_animation->get_frames().size())
		return;

	auto& frame = m_current_animation->get_frames()[m_selected_frame_index];
	static const auto drag_drop_callback = [&frame](asset_handle handle)
		{
			frame.texture = handle;
		};
	ImGuiStyle style = ImGui::GetStyle();

	float item_width = width == 0.0f ? width : (width - (style.WindowPadding.x * 2.0f + style.ItemSpacing.x + style.ScrollbarSize)) * 0.5f;

	UI::drag_drop_target(asset_type::texture2D, drag_drop_callback, frame.texture ? asset_manager::get_asset_metadata(frame.texture).filepath.string().c_str() : "none", true, item_width, 0.0f);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(item_width);
	static constexpr float min_v = 0.0f;
	ImGui::DragScalar("##Duration (s)", ImGuiDataType_Float, &frame.duration, 0.01f, &min_v);
}

_WHIP_END
