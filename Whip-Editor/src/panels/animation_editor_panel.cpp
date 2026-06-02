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

#include <algorithm>

_WHIP_START

animation_editor_panel::animation_editor_panel()
{
}

animation_editor_panel::~animation_editor_panel() {}

void animation_editor_panel::on_imgui_render()
{
	if (!m_open)
		return;
	bool open = m_open;
	ImGui::Begin("Animation Editor", &open);
	if (open != m_open)
		set_open(open);
	update_preview();

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));

	ImGui::BeginChild("##AnimationEditorToolbar", ImVec2(0.0f, 58.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::AlignTextToFramePadding();
	draw_playback_controls(32.0f, 32.0f);
	ImGui::SameLine(0.0f, 16.0f);

	if (ImGui::Button("New", ImVec2(82.0f, 32.0f)))
	{
		m_current_animation.reset();
		ref<animation2D> new_anim = make_ref<animation2D>();
		new_anim->set_name("New Animation");
		m_current_animation = new_anim;
		m_selected_frame_index = -1;
		stop_preview(false);
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
	if (ImGui::Button("Close", ImVec2(82.0f, 32.0f)))
	{
		m_current_animation = nullptr;
		m_selected_frame_index = -1;
		stop_preview(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Save", ImVec2(82.0f, 32.0f)))
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
		ImGui::SetNextItemWidth(190.0f);
		if (ImGui::InputText("##AnimationName", buffer, sizeof(buffer)))
			m_current_animation->set_name(buffer);
		ImGui::SameLine();
	}
	ImGui::EndDisabled();

	ImGui::SetNextItemWidth(std::min(260.0f, ImGui::GetContentRegionAvail().x));
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
					stop_preview(false);
				}
		});
		ImGui::EndCombo();
	}
	ImGui::EndChild();

	ImGui::Spacing();
	if (!m_current_animation)
	{
		ImGui::BeginChild("##AnimationEditorEmpty", ImVec2(0.0f, 0.0f), true);
		ImVec2 available = ImGui::GetContentRegionAvail();
		ImGui::SetCursorPos(ImVec2(16.0f, 16.0f));
		ImGui::TextDisabled("No animation selected");
		ImGui::SetCursorPos(ImVec2(16.0f, 48.0f));
		draw_animation_drag_drop_area(std::max(120.0f, available.x - 32.0f), std::max(80.0f, available.y - 64.0f));
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
		ImGui::End();
		return;
	}

	ImGui::BeginChild("##AnimationEditorTimelineShell", ImVec2(0.0f, 168.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::TextDisabled("%zu frame(s)", m_current_animation->get_frames().size());
	draw_timeline(ImGui::GetContentRegionAvail().x, 104.0f, 132.0f);
	ImGui::EndChild();

	ImGui::BeginChild("##AnimationEditorFrameTools", ImVec2(0.0f, 50.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	draw_frame_list(220.0f);
	ImGui::SameLine();
	draw_add_frame_button(112.0f);
	ImGui::SameLine();
	draw_remove_frame_button(128.0f);
	ImGui::EndChild();

	ImGui::BeginChild("##AnimationEditorFrameInspector", ImVec2(0.0f, 112.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	draw_frame_editor(ImGui::GetContentRegionAvail().x);
	ImGui::EndChild();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
	ImGui::End();
}

void animation_editor_panel::set_open(bool open)
{
	if (m_open == open)
		return;
	m_open = open;
	m_open_dirty = true;
}

bool animation_editor_panel::consume_open_dirty()
{
	const bool dirty = m_open_dirty;
	m_open_dirty = false;
	return dirty;
}

void animation_editor_panel::draw_animation_drag_drop_area(float width, float height)
{
	const auto drag_drop_callback = [this](asset_handle handle)
		{
			m_current_animation = asset_manager::get_asset<animation2D>(handle);
			m_selected_frame_index = -1;
			stop_preview(false);
		};

	UI::drag_drop_target(asset_type::animation, drag_drop_callback, "Select Animation", true, width, height, true);
}

void animation_editor_panel::draw_playback_controls(float width, float height)
{
	ImVec2 size(width, height);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
	const bool has_frames = m_current_animation && !m_current_animation->get_frames().empty();

	auto draw_button = [&](icon icon_type, const char* tooltip) -> bool
		{
			const std::string button_id = "##AnimationControl" + std::to_string(static_cast<int>(icon_type));
			ImGui::InvisibleButton(button_id.c_str(), size);
			bool clicked = ImGui::IsItemClicked();
			bool hovered = ImGui::IsItemHovered();
			bool active = ImGui::IsItemActive();
			ImVec2 min = ImGui::GetItemRectMin();
			ImVec2 max = ImGui::GetItemRectMax();
			ImU32 bg = active ? IM_COL32(94, 62, 34, 255) : hovered ? IM_COL32(48, 42, 34, 255) : IM_COL32(30, 28, 24, 255);
			draw_list->AddRectFilled(min, max, bg, 5.0f);
			draw_list->AddRect(min, max, hovered ? IM_COL32(118, 92, 58, 255) : IM_COL32(64, 56, 44, 255), 5.0f);

			if (frenum::contains(icon_type))
			{
				ref<texture2D> texture = icon_manager::get().get_icon(icon_type);
				const float icon_size = 17.0f;
				ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
				draw_list->AddImage(
					UI::to_imgui_texture_id(texture->get_renderer_id()),
					ImVec2(center.x - icon_size * 0.5f, center.y - icon_size * 0.5f),
					ImVec2(center.x + icon_size * 0.5f, center.y + icon_size * 0.5f),
					ImVec2(0, 1),
					ImVec2(1, 0),
					IM_COL32(240, 232, 216, 255));
			}
			else
				draw_list->AddText(ImVec2(min.x + 8.0f, min.y + 6.0f), IM_COL32(240, 232, 216, 255), frenum::to_string<icon>(icon_type).c_str());

			if (hovered)
				ImGui::SetTooltip("%s", tooltip);
			return clicked;
		};

	if (draw_button(icon::step_back, "Previous frame"))
	{
		step_preview(-1);
	}
	ImGui::SameLine();
	if (draw_button(icon::play, m_preview_paused ? "Resume preview" : "Play preview"))
	{
		if (has_frames)
		{
			if (m_selected_frame_index < 0 || m_selected_frame_index >= (int)m_current_animation->get_frames().size())
				m_selected_frame_index = 0;
			m_preview_playing = true;
			m_preview_paused = false;
			m_preview_elapsed = 0.0f;
		}
	}
	ImGui::SameLine();
	if (draw_button(icon::pause, "Pause preview"))
	{
		if (m_preview_playing)
			m_preview_paused = true;
	}
	ImGui::SameLine();
	if (draw_button(icon::stop, "Stop preview"))
	{
		stop_preview(true);
	}
	ImGui::SameLine();
	if (draw_button(icon::step_forward, "Next frame"))
	{
		step_preview(1);
	}

	ImGui::PopStyleVar();
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
		stop_preview(false);
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
				{
					m_current_animation = asset_manager::get_asset<animation2D>(value.first);
					m_selected_frame_index = -1;
					stop_preview(false);
				}
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
		animation_frame frame;
		frame.duration = 0.1f;
		m_current_animation->add_frame(frame);
		m_selected_frame_index = int(m_current_animation->get_frames().size() - 1);
		stop_preview(false);
	}
}

void animation_editor_panel::draw_remove_frame_button(float width)
{
	if (!m_current_animation || m_selected_frame_index < 0)
		return;
	if (ImGui::Button("Remove Frame", ImVec2(width, 0.0f)))
	{
		m_current_animation->remove_frame(m_selected_frame_index);
		if (m_current_animation->get_frames().empty())
			m_selected_frame_index = -1;
		else
			m_selected_frame_index = std::min(m_selected_frame_index, (int)m_current_animation->get_frames().size() - 1);
		stop_preview(false);
	}
}

void animation_editor_panel::draw_frame_editor(float width)
{
	if (!m_current_animation)
		return;

	if (m_selected_frame_index < 0 || m_selected_frame_index >= (int)m_current_animation->get_frames().size())
	{
		ImGui::TextDisabled("Select a frame to edit texture and duration.");
		return;
	}

	auto& frame = m_current_animation->get_frames()[m_selected_frame_index];
	const auto drag_drop_callback = [&frame](asset_handle handle)
		{
			frame.texture = handle;
		};

	if (ImGui::BeginTable("##AnimationFrameInspectorTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 110.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Texture");
		ImGui::TableNextColumn();
		const std::string texture_label = frame.texture ? asset_manager::get_asset_metadata(frame.texture).filepath.generic_string() : "Drop texture";
		UI::drag_drop_target(asset_type::texture2D, drag_drop_callback, texture_label.c_str(), true, std::max(160.0f, width - 140.0f), 0.0f);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Duration");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		static constexpr float min_v = 0.0f;
		ImGui::DragScalar("##DurationSeconds", ImGuiDataType_Float, &frame.duration, 0.01f, &min_v, nullptr, "%.3f s");

		ImGui::EndTable();
	}
}

void animation_editor_panel::update_preview()
{
	if (!m_preview_playing || m_preview_paused || !m_current_animation)
		return;

	auto& frames = m_current_animation->get_frames();
	if (frames.empty())
	{
		stop_preview(false);
		return;
	}

	if (m_selected_frame_index < 0 || m_selected_frame_index >= (int)frames.size())
		m_selected_frame_index = 0;

	m_preview_elapsed += ImGui::GetIO().DeltaTime;
	const float current_duration = std::max(frames[m_selected_frame_index].duration, 0.033f);
	if (m_preview_elapsed < current_duration)
		return;

	m_preview_elapsed = 0.0f;
	const int next_frame = m_selected_frame_index + 1;
	if (next_frame < (int)frames.size())
	{
		m_selected_frame_index = next_frame;
		return;
	}

	if (m_current_animation->is_looping())
		m_selected_frame_index = 0;
	else
		stop_preview(false);
}

void animation_editor_panel::step_preview(int direction)
{
	if (!m_current_animation)
		return;

	auto& frames = m_current_animation->get_frames();
	if (frames.empty())
		return;

	if (m_selected_frame_index < 0 || m_selected_frame_index >= (int)frames.size())
		m_selected_frame_index = direction < 0 ? (int)frames.size() - 1 : 0;
	else
	{
		const int frame_count = (int)frames.size();
		m_selected_frame_index = (m_selected_frame_index + direction + frame_count) % frame_count;
	}

	m_preview_elapsed = 0.0f;
}

void animation_editor_panel::stop_preview(bool reset_selection)
{
	m_preview_playing = false;
	m_preview_paused = false;
	m_preview_elapsed = 0.0f;

	if (reset_selection && m_current_animation && !m_current_animation->get_frames().empty())
		m_selected_frame_index = 0;
}

_WHIP_END
