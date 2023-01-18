#pragma once

#include <Whip/Core.h>
#include <Whip/Layer.h>

_WHIP_START


class WHIP_API LayerStack
{
	using LayPtrVec = std::vector<Layer*>;
	using LayPtrVecIt = std::vector<Layer*>::iterator;
private:
	LayPtrVec m_Layers;  // katmanlar
	size_t m_LayerInsertIndex = 0;
public:
	LayerStack();
	~LayerStack();

	void PushLayer(layerptr layer);  // katman ekle
	void PushOverlay(layerptr overlay);  // kaplama ekle
	void PopLayer(layerptr layer);
	void PopOverlay(layerptr overlay);

	WHP_NODISCARD LayPtrVecIt begin() noexcept { return m_Layers.begin(); }
	WHP_NODISCARD LayPtrVecIt end() noexcept { return m_Layers.end(); }
};

_WHIP_END