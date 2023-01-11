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
	LayPtrVecIt m_LayerInsert;
public:
	LayerStack();
	~LayerStack();

	void PushLayer(layerptr layer);  // katman ekle
	void PushOverlay(layerptr overlay);  // kaplama ekle
	void PopLayer(layerptr layer);
	void PopOverlay(layerptr overlay);

	LayPtrVecIt begin() { return m_Layers.begin(); }
	LayPtrVecIt end() { return m_Layers.end(); }
};

_WHIP_END