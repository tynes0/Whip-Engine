#include <whippch.h>
#include <Whip/LayerStack.h>

_WHIP_START

LayerStack::LayerStack() {}

LayerStack::~LayerStack()
{
	for (layerptr item : m_Layers)
	{
		delete item;
	}
}

void LayerStack::PushLayer(layerptr layer)
{
	m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
	m_LayerInsertIndex++;
}

void LayerStack::PushOverlay(layerptr overlay)
{
	m_Layers.emplace_back(overlay);
}

void LayerStack::PopLayer(layerptr layer)
{
	auto iterator = std::find(m_Layers.begin(), m_Layers.end(), layer);
	if (iterator != m_Layers.end())
	{
		m_Layers.erase(iterator);
		m_LayerInsertIndex--;
	}
}

void LayerStack::PopOverlay(layerptr overlay)
{
	auto iterator = std::find(m_Layers.begin(), m_Layers.end(), overlay);
	if (iterator != m_Layers.end())
	{
		m_Layers.erase(iterator);
	}
}

_WHIP_END