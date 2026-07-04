#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Layer.h>
#include <Whip/Core/Memory/AllocatorRegistry.h>

_WHIP_START


class WHIP_API LayerStack
{
private:
	using LayerContainer = memory::Vector<Layer*>;

	LayerContainer m_Layers;
	size_t m_LayerInsertIndex = 0;
public:
	LayerStack();
	~LayerStack();

	void PushLayer(LayerPtr layer);
	void PushOverlay(LayerPtr overlay);
	void PopLayer(LayerPtr layer);
	void PopOverlay(LayerPtr overlay);
	void Clear();

	WHP_NODISCARD LayerContainer::iterator begin() noexcept { return m_Layers.begin(); }
	WHP_NODISCARD LayerContainer::iterator end() noexcept { return m_Layers.end(); }
	WHP_NODISCARD LayerContainer::reverse_iterator rbegin() noexcept { return m_Layers.rbegin(); }
	WHP_NODISCARD LayerContainer::reverse_iterator rend() noexcept { return m_Layers.rend(); }

	WHP_NODISCARD LayerContainer::const_iterator begin() const noexcept { return m_Layers.begin(); }
	WHP_NODISCARD LayerContainer::const_iterator end() const noexcept { return m_Layers.end(); }
	WHP_NODISCARD LayerContainer::const_reverse_iterator rbegin() const noexcept { return m_Layers.rbegin(); }
	WHP_NODISCARD LayerContainer::const_reverse_iterator rend() const noexcept { return m_Layers.rend(); }
};

_WHIP_END
