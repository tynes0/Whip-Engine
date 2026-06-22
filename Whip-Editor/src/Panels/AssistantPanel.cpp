#include <WhipPch.h>
#include <Whip-Editor/Panels/AssistantPanel.h>

#include <Whip-Editor/Managers/EditorShortcutManager.h>
#include <Whip-Editor/UI/UIScopedStyle.h>

#include <algorithm>
#include <chrono>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

_WHIP_START

namespace
{
	Assistant::Settings DefaultSettings;

	ImVec4 RoleColor(Assistant::Role role)
	{
		switch (role)
		{
		case Assistant::Role::User: return ImVec4(0.74f, 0.86f, 0.96f, 1.0f);
		case Assistant::Role::Assistant: return ImVec4(0.78f, 0.88f, 0.72f, 1.0f);
		case Assistant::Role::System: return ImVec4(0.92f, 0.72f, 0.52f, 1.0f);
		default: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
		}
	}

	void DrawChip(const char* text)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
		ImGui::SmallButton(text);
		ImGui::PopStyleColor(4);
	}
}

AssistantPanel::AssistantPanel()
	: EditorPanel("Whip Assistant", false, true)
{
	m_Messages.push_back({ Assistant::Role::Assistant, "Ready. I can inspect the current editor context, draft scene changes, and apply safe proposals through Undo-friendly editor actions." });
}

void AssistantPanel::OnImGuiRender()
{
	if (!m_Open)
		return;

	PollRequest();

	ImGui::SetNextWindowSize(ImVec2(460.0f, 620.0f), ImGuiCond_FirstUseEver);
	bool open = m_Open;
	if (!ImGui::Begin("Whip Assistant", &open))
	{
		SetOpen(open);
		ImGui::End();
		return;
	}
	SetOpen(open);

	const Assistant::Settings& settings = GetSettings();
	const Assistant::ContextSnapshot context = BuildContext();
	DrawHeader(context, settings);
	ImGui::Separator();
	DrawMessages();
	ImGui::Separator();
	DrawProposals();
	ImGui::Separator();

	if (!m_Status.empty())
		ImGui::TextDisabled("%s", m_Status.c_str());
	if (m_RequestInFlight)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("Thinking...");
	}

	const float buttonRowHeight = ImGui::GetFrameHeightWithSpacing();
	const float inputHeight = std::max(80.0f, ImGui::GetTextLineHeightWithSpacing() * 4.0f);
	if (m_FocusPrompt)
	{
		ImGui::SetKeyboardFocusHere();
		m_FocusPrompt = false;
	}
	ImGui::InputTextMultiline("##AssistantPrompt", &m_Input, ImVec2(-1.0f, inputHeight), ImGuiInputTextFlags_AllowTabInput);

	const bool canSubmit = !m_RequestInFlight && !m_Input.empty() && settings.m_Enabled;
	ImGui::BeginDisabled(!canSubmit);
	if (ImGui::Button("Ask", ImVec2(86.0f, 0.0f)))
		SubmitPrompt(true);
	ImGui::SameLine();
	if (ImGui::Button("Plan Local", ImVec2(106.0f, 0.0f)))
		SubmitPrompt(false);
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Clear", ImVec2(78.0f, 0.0f)))
	{
		m_Messages.clear();
		m_Proposals.clear();
		m_Status.clear();
		m_Input.clear();
		m_ShouldScrollMessages = true;
		AddAssistantMessage("Cleared. I am ready for the next move.");
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%s", settings.m_UseOnlineResponses ? "Online allowed" : "Local first");

	(void)buttonRowHeight;
	ImGui::End();
}

void AssistantPanel::RegisterShortcuts(EditorShortcutManager& shortcutManager)
{
	EditorShortcutOptions options;
	options.m_AllowWhenActiveWidget = true;
	options.m_AllowWhenTextInput = true;
	shortcutManager.Add(
		EditorShortcutScope::Assistant,
		"assistant.focus_prompt",
		"Focus Whip Assistant",
		"Assistant",
		{ Key::I, true, false, true },
		[this]()
		{
			SetOpen(true);
			FocusPrompt();
			return true;
		},
		[]() { return true; },
		[]() { return true; },
		options);
}

void AssistantPanel::FocusPrompt()
{
	m_FocusPrompt = true;
}

void AssistantPanel::SubmitPrompt(bool online)
{
	const std::string prompt = m_Input;
	if (prompt.empty())
		return;

	const Assistant::Settings settings = GetSettings();
	const Assistant::ContextSnapshot context = BuildContext();

	m_Messages.push_back({ Assistant::Role::User, prompt });
	m_Input.clear();
	m_ShouldScrollMessages = true;
	m_Status.clear();

	std::vector<Assistant::ToolProposal> localProposals = Assistant::BuildLocalProposals(context, prompt);
	if (!localProposals.empty())
	{
		for (Assistant::ToolProposal& proposal : localProposals)
			m_Proposals.push_back(std::move(proposal));
		AddAssistantMessage("I prepared reviewable editor action(s). Check the proposal queue before applying them.");
	}

	if (!online)
	{
		if (localProposals.empty())
			AddAssistantMessage("I do not have a safe local action for that yet. I can still help plan it if online responses are enabled in Settings.");
		return;
	}

	if (!settings.m_Enabled)
	{
		AddAssistantMessage("Whip Assistant is disabled in Settings.");
		return;
	}
	if (!settings.m_UseOnlineResponses || settings.m_ApiKey.empty())
	{
		AddAssistantMessage("Online responses are not configured. Enable them and add an API key in Project Settings > Whip Assistant.");
		return;
	}

	m_RequestInFlight = true;
	m_Status = "Sending request";
	m_RequestFuture = std::async(std::launch::async, [settings, context, prompt]()
	{
		return Assistant::RequestOpenAIResponse(settings, context, prompt);
	});
}

void AssistantPanel::PollRequest()
{
	if (!m_RequestInFlight || !m_RequestFuture.valid())
		return;

	if (m_RequestFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
		return;

	Assistant::Response response = m_RequestFuture.get();
	m_RequestInFlight = false;
	m_Status.clear();
	if (!response.m_Success)
	{
		m_Messages.push_back({ Assistant::Role::System, response.m_Error.empty() ? "Assistant request failed." : response.m_Error });
		m_ShouldScrollMessages = true;
		return;
	}

	AddAssistantMessage(response.m_Text);
	for (Assistant::ToolProposal& proposal : response.m_Proposals)
		m_Proposals.push_back(std::move(proposal));
}

void AssistantPanel::DrawHeader(const Assistant::ContextSnapshot& context, const Assistant::Settings& settings)
{
	ImGui::TextUnformatted("Whip Assistant");
	ImGui::SameLine();
	ImGui::TextDisabled("%s", settings.m_UseOnlineResponses && !settings.m_ApiKey.empty() ? "Responses API" : "Offline proposals");

	if (context.m_HasProject)
	{
		DrawChip(context.m_ProjectName.c_str());
		ImGui::SameLine();
	}
	if (context.m_HasSelection)
	{
		std::string selection = "Selected: " + context.m_SelectedEntityName;
		DrawChip(selection.c_str());
	}
	else
	{
		DrawChip("No selection");
	}
}

void AssistantPanel::DrawMessages()
{
	const float messagesHeight = std::max(160.0f, ImGui::GetContentRegionAvail().y - 260.0f);
	if (!ImGui::BeginChild("##AssistantMessages", ImVec2(0.0f, messagesHeight), true))
	{
		ImGui::EndChild();
		return;
	}

	for (const Assistant::Message& message : m_Messages)
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextColored(RoleColor(message.m_Role), "%s", Assistant::RoleName(message.m_Role));
		ImGui::SameLine();
		ImGui::TextWrapped("%s", message.m_Content.c_str());
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
	}

	if (m_ShouldScrollMessages)
	{
		ImGui::SetScrollHereY(1.0f);
		m_ShouldScrollMessages = false;
	}
	ImGui::EndChild();
}

void AssistantPanel::DrawProposals()
{
	if (m_Proposals.empty())
	{
		ImGui::TextDisabled("No pending proposals.");
		return;
	}

	ImGui::TextDisabled("%zu pending proposal(s)", m_Proposals.size());
	if (!ImGui::BeginChild("##AssistantProposals", ImVec2(0.0f, 116.0f), true))
	{
		ImGui::EndChild();
		return;
	}

	for (size_t i = 0; i < m_Proposals.size();)
	{
		Assistant::ToolProposal& proposal = m_Proposals[i];
		ImGui::PushID(static_cast<int>(i));
		ImGui::TextUnformatted(proposal.m_Title.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("%s", Assistant::ToolKindName(proposal.m_Kind));
		ImGui::TextWrapped("%s", proposal.m_Description.c_str());

		bool remove = false;
		if (ImGui::SmallButton("Apply"))
		{
			const bool applied = m_ApplyProposalCallback && m_ApplyProposalCallback(proposal);
			m_Messages.push_back({ applied ? Assistant::Role::Assistant : Assistant::Role::System, applied ? "Proposal applied." : "Proposal could not be applied in the current editor state." });
			m_ShouldScrollMessages = true;
			remove = applied;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Dismiss"))
			remove = true;

		ImGui::Separator();
		ImGui::PopID();

		if (remove)
			m_Proposals.erase(m_Proposals.begin() + static_cast<std::ptrdiff_t>(i));
		else
			++i;
	}

	ImGui::EndChild();
}

void AssistantPanel::AddAssistantMessage(std::string content)
{
	if (content.empty())
		return;
	m_Messages.push_back({ Assistant::Role::Assistant, std::move(content) });
	m_ShouldScrollMessages = true;
}

const Assistant::Settings& AssistantPanel::GetSettings() const
{
	if (m_SettingsCallback)
		return m_SettingsCallback();
	return DefaultSettings;
}

Assistant::ContextSnapshot AssistantPanel::BuildContext() const
{
	if (m_ContextCallback)
		return m_ContextCallback();
	return {};
}

_WHIP_END
