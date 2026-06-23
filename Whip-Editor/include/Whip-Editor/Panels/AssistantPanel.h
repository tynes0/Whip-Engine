#pragma once

#include <Whip-Assistant/WhipAssistant.h>
#include <Whip-Editor/Panels/EditorPanel.h>

#include <functional>
#include <future>
#include <string>
#include <vector>

_WHIP_START

class AssistantPanel final : public EditorPanel
{
public:
	using ContextCallback = std::function<Assistant::ContextSnapshot()>;
	using SettingsCallback = std::function<const Assistant::Settings&()>;
	using ApplyProposalCallback = std::function<bool(const Assistant::ToolProposal&)>;

	AssistantPanel();

	void OnImGuiRender() override;
	void RegisterShortcuts(EditorShortcutManager& shortcutManager) override;

	void SetContextCallback(ContextCallback callback) { m_ContextCallback = std::move(callback); }
	void SetSettingsCallback(SettingsCallback callback) { m_SettingsCallback = std::move(callback); }
	void SetApplyProposalCallback(ApplyProposalCallback callback) { m_ApplyProposalCallback = std::move(callback); }

	void FocusPrompt();
	bool IsShortcutContextActive() const { return m_Open; }

private:
	void SubmitPrompt(bool online);
	void PollRequest();
	void DrawHeader(const Assistant::ContextSnapshot& context, const Assistant::Settings& settings);
	void DrawMessages();
	void DrawProposals();
	void AddAssistantMessage(std::string content);
	bool CanAutoApplyProposal(const Assistant::ToolProposal& proposal, const Assistant::Settings& settings) const;
	bool ApplyProposalNow(const Assistant::ToolProposal& proposal, std::string* outMessage = nullptr);
	void HandleProposals(std::vector<Assistant::ToolProposal>&& proposals);
	void ApplyAllQueuedProposals();
	const Assistant::Settings& GetSettings() const;
	Assistant::ContextSnapshot BuildContext() const;

	ContextCallback m_ContextCallback;
	SettingsCallback m_SettingsCallback;
	ApplyProposalCallback m_ApplyProposalCallback;

	std::vector<Assistant::Message> m_Messages;
	std::vector<Assistant::ToolProposal> m_Proposals;
	std::future<Assistant::Response> m_RequestFuture;
	bool m_RequestInFlight = false;
	bool m_FocusPrompt = false;
	bool m_ShouldScrollMessages = true;
	std::string m_Input;
	std::string m_Status;
};

_WHIP_END
