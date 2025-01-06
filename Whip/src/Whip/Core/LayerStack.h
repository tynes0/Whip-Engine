#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Layer.h>
#include <vector>

_WHIP_START


class WHIP_API layer_stack
{
private:
	std::vector<layer*> m_layers;  // katmanlar
	size_t m_layer_insert_index = 0;
public:
	layer_stack();
	~layer_stack();

	void push_layer(layerptr layer);  // katman ekle
	void push_overlay(layerptr overlay);  // kaplama ekle
	void pop_layer(layerptr layer);
	void pop_overlay(layerptr overlay);

	WHP_NODISCARD std::vector<layer*>::iterator begin() noexcept { return m_layers.begin(); }
	WHP_NODISCARD std::vector<layer*>::iterator end() noexcept { return m_layers.end(); }
	WHP_NODISCARD std::vector<layer*>::reverse_iterator rbegin() noexcept { return m_layers.rbegin(); }
	WHP_NODISCARD std::vector<layer*>::reverse_iterator rend() noexcept { return m_layers.rend(); }

	WHP_NODISCARD std::vector<layer*>::const_iterator begin() const noexcept { return m_layers.begin(); }
	WHP_NODISCARD std::vector<layer*>::const_iterator end() const noexcept { return m_layers.end(); }
	WHP_NODISCARD std::vector<layer*>::const_reverse_iterator rbegin() const noexcept { return m_layers.rbegin(); }
	WHP_NODISCARD std::vector<layer*>::const_reverse_iterator rend() const noexcept { return m_layers.rend(); }
};

_WHIP_END
