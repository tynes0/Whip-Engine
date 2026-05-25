#include "whippch.h"
#include "LayerStack.h"

_WHIP_START

layer_stack::layer_stack() {}

layer_stack::~layer_stack()
{
	clear();
}

void layer_stack::push_layer(layerptr layer)
{
	m_layers.insert(m_layers.begin() + m_layer_insert_index, layer);
	m_layer_insert_index++;
}

void layer_stack::push_overlay(layerptr overlay)
{
	m_layers.push_back(overlay);
}

void layer_stack::pop_layer(layerptr layer)
{
	auto iterator = std::find(m_layers.begin(), m_layers.end(), layer);
	if (iterator != m_layers.end())
	{
		m_layers.erase(iterator);
		m_layer_insert_index--;
	}
}

void layer_stack::pop_overlay(layerptr overlay)
{
	auto iterator = std::find(m_layers.begin(), m_layers.end(), overlay);
	if (iterator != m_layers.end())
	{
		m_layers.erase(iterator);
	}
}

void layer_stack::clear()
{
	for (layerptr item : m_layers)
		delete item;

	m_layers.clear();
	m_layer_insert_index = 0;
}

_WHIP_END
