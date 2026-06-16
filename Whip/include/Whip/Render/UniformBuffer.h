#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>

#include <cstdint>

_WHIP_START

class UniformBuffer
{
public:
	virtual ~UniformBuffer() {}
	virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;

	static Ref<UniformBuffer> Create(uint32_t size, uint32_t binding);
};

_WHIP_END
