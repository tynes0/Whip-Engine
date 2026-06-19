#pragma once

#include <Whip/Core/Core.h>

#include <string>
#include <functional>
#include <utility>

_WHIP_START

class EditorPanel
{
public:
	EditorPanel(std::string name, bool open = true, bool requiresProject = true)
		: m_Name(std::move(name)), m_Open(open), m_RequiresProject(requiresProject)
	{
	}

	virtual ~EditorPanel() = default;

	const std::string& GetName() const { return m_Name; }

	virtual void OnImGuiRender() = 0;
	virtual void SetOpen(bool open)
	{
		if (m_Open == open)
			return;

		m_Open = open;
		m_OpenDirty = true;
	}

	virtual bool IsOpen() const { return m_Open; }
	virtual bool ConsumeOpenDirty()
	{
		const bool dirty = m_OpenDirty;
		m_OpenDirty = false;
		return dirty;
	}

	virtual bool CanOpenFromMenu() const { return true; }
	bool RequiresProject() const { return m_RequiresProject; }

protected:
	std::string m_Name;
	bool m_Open = true;
	bool m_OpenDirty = false;
	bool m_RequiresProject = true;
};

class CallbackEditorPanel : public EditorPanel
{
public:
	CallbackEditorPanel(
		std::string name,
		std::function<void()> renderCallback,
		std::function<bool()> isOpenCallback,
		std::function<void(bool)> setOpenCallback,
		std::function<bool()> consumeDirtyCallback = {},
		bool requiresProject = true)
		: EditorPanel(std::move(name), true, requiresProject),
		m_RenderCallback(std::move(renderCallback)),
		m_IsOpenCallback(std::move(isOpenCallback)),
		m_SetOpenCallback(std::move(setOpenCallback)),
		m_ConsumeDirtyCallback(std::move(consumeDirtyCallback))
	{
	}

	void OnImGuiRender() override
	{
		if (m_RenderCallback)
			m_RenderCallback();
	}

	void SetOpen(bool open) override
	{
		if (m_SetOpenCallback)
			m_SetOpenCallback(open);
		else
			EditorPanel::SetOpen(open);
	}

	bool IsOpen() const override
	{
		return m_IsOpenCallback ? m_IsOpenCallback() : EditorPanel::IsOpen();
	}

	bool ConsumeOpenDirty() override
	{
		return m_ConsumeDirtyCallback ? m_ConsumeDirtyCallback() : EditorPanel::ConsumeOpenDirty();
	}

private:
	std::function<void()> m_RenderCallback;
	std::function<bool()> m_IsOpenCallback;
	std::function<void(bool)> m_SetOpenCallback;
	std::function<bool()> m_ConsumeDirtyCallback;
};

_WHIP_END
