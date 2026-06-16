#include "WhipPch.h"
#include <Whip/Core/LayerStack.h>

_WHIP_START

LayerStack::LayerStack() {}

LayerStack::~LayerStack()
{
	Clear();
}

void LayerStack::PushLayer(LayerPtr layer)
{
	m_Layers.insert(m_Layers.begin() + m_LayerInsertIndex, layer);
	m_LayerInsertIndex++;
}

void LayerStack::PushOverlay(LayerPtr overlay)
{
	m_Layers.push_back(overlay);
}

void LayerStack::PopLayer(LayerPtr layer)
{
	auto iterator = std::find(m_Layers.begin(), m_Layers.end(), layer);
	if (iterator != m_Layers.end())
	{
		m_Layers.erase(iterator);
		m_LayerInsertIndex--;
	}
}

void LayerStack::PopOverlay(LayerPtr overlay)
{
	auto iterator = std::find(m_Layers.begin(), m_Layers.end(), overlay);
	if (iterator != m_Layers.end())
	{
		m_Layers.erase(iterator);
	}
}

void LayerStack::Clear()
{
	for (LayerPtr item : m_Layers)
		delete item;

	m_Layers.clear();
	m_LayerInsertIndex = 0;
}

_WHIP_END
