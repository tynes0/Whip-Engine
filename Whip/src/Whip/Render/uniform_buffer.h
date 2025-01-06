#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>

#include <cstdint>

_WHIP_START

class uniform_buffer
{
public:
	virtual ~uniform_buffer() {}
	virtual void set_data(const void* data, uint32_t size, uint32_t offset = 0) = 0;

	static ref<uniform_buffer> create(uint32_t size, uint32_t binding);
};

_WHIP_END
