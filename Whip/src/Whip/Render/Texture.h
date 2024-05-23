#pragma once

#include <Whip/Core/Core.h>
#include <string>

_WHIP_START

class texture
{
public:
	virtual ~texture() = default;

	virtual uint32_t get_width() const = 0;
	virtual uint32_t get_height() const = 0;
	
	virtual void set_data(void* data, uint32_t size) = 0;
	virtual void bind(uint32_t slot = 0) const = 0;
	virtual bool operator==(const texture& other) const = 0;
};

class texture2D : public texture
{
public:
	static ref<texture2D> create(uint32_t width, uint32_t height);
	static ref<texture2D> create(const std::string& path);
};

_WHIP_END