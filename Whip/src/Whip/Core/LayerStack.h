#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Layer.h>
#include <vector>

_WHIP_START


class WHIP_API layer_stack
{
	using layer_ptr_iter = std::vector<layer*>::iterator;
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

	WHP_NODISCARD layer_ptr_iter begin() noexcept { return m_layers.begin(); }
	WHP_NODISCARD layer_ptr_iter end() noexcept { return m_layers.end(); }
};

_WHIP_END