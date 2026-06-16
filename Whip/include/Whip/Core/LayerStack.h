#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Layer.h>
#include <vector>

_WHIP_START


class WHIP_API LayerStack
{
private:
	std::vector<Layer*> m_Layers;
	size_t m_LayerInsertIndex = 0;
public:
	LayerStack();
	~LayerStack();

	void PushLayer(LayerPtr layer);
	void PushOverlay(LayerPtr overlay);
	void PopLayer(LayerPtr layer);
	void PopOverlay(LayerPtr overlay);
	void Clear();

	WHP_NODISCARD std::vector<Layer*>::iterator begin() noexcept { return m_Layers.begin(); }
	WHP_NODISCARD std::vector<Layer*>::iterator end() noexcept { return m_Layers.end(); }
	WHP_NODISCARD std::vector<Layer*>::reverse_iterator rbegin() noexcept { return m_Layers.rbegin(); }
	WHP_NODISCARD std::vector<Layer*>::reverse_iterator rend() noexcept { return m_Layers.rend(); }

	WHP_NODISCARD std::vector<Layer*>::const_iterator begin() const noexcept { return m_Layers.begin(); }
	WHP_NODISCARD std::vector<Layer*>::const_iterator end() const noexcept { return m_Layers.end(); }
	WHP_NODISCARD std::vector<Layer*>::const_reverse_iterator rbegin() const noexcept { return m_Layers.rbegin(); }
	WHP_NODISCARD std::vector<Layer*>::const_reverse_iterator rend() const noexcept { return m_Layers.rend(); }
};

_WHIP_END
