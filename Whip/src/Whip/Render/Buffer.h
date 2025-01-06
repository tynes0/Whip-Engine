#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/memory.h>
#include <vector>

_WHIP_START

enum class shader_data_type : uint16_t
{
	none = 0,
	Float,
	Float2,
	Float3,
	Float4,
	Mat3,
	Mat4,
	Bool,
	Int,
	Int2,
	Int3,
	Int4,
};

WHP_NODISCARD static uint32_t shader_data_type_size(shader_data_type type)
{
	switch (type)
	{
	case shader_data_type::none:		WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
	case shader_data_type::Float:		return sizeof(float);
	case shader_data_type::Float2:		return sizeof(float) * 2;
	case shader_data_type::Float3:		return sizeof(float) * 3;
	case shader_data_type::Float4:		return sizeof(float) * 4;
	case shader_data_type::Mat3:		return sizeof(float) * 3 * 3;
	case shader_data_type::Mat4:		return sizeof(float) * 4 * 4;
	case shader_data_type::Bool:		return sizeof(bool);
	case shader_data_type::Int:			return sizeof(int);
	case shader_data_type::Int2:		return sizeof(int) * 2;
	case shader_data_type::Int3:		return sizeof(int) * 3;
	case shader_data_type::Int4:		return sizeof(int) * 4;
	}
	WHP_CORE_ASSERT(false, "Unknown shader_data_type!");
	return 0;
}

struct buffer_element
{
	std::string name;
	shader_data_type type;
	uint64_t size;
	uint64_t offset;
	bool normalized;

	//buffer_element() {}

	buffer_element(shader_data_type type_in, const std::string& name_in, bool normalized_in = false)
		: name(name_in), type(type_in), size(shader_data_type_size(type_in)), offset(0), normalized(normalized_in) {}

	WHP_NODISCARD uint32_t get_component_count() const
	{
		switch (type)
		{
		case shader_data_type::none:		WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
		case shader_data_type::Float:		return 1;
		case shader_data_type::Float2:		return 2;
		case shader_data_type::Float3:		return 3;
		case shader_data_type::Float4:		return 4;
		case shader_data_type::Mat3:		return 3 * 3;
		case shader_data_type::Mat4:		return 4 * 4;
		case shader_data_type::Bool:		return 1;
		case shader_data_type::Int:			return 1;
		case shader_data_type::Int2:		return 2;
		case shader_data_type::Int3:		return 3;
		case shader_data_type::Int4:		return 4;
		}
		WHP_CORE_ASSERT(false, "Unknown shader_data_type!");
		return 0;
	}
};

class buffer_layout
{
	using buffer_element_iter		= std::vector<buffer_element>::iterator;
	using buffer_element_const_iter = std::vector<buffer_element>::const_iterator;
private:
	std::vector<buffer_element> m_elements;
	uint64_t m_stride = 0;
private:
	void calculate_offsets_and_stride()
	{
		uint64_t offset = 0;
		m_stride = 0;
		for (auto& elem : m_elements)
		{
			elem.offset = offset;
			offset += elem.size;
			m_stride += elem.size;
		}
	}
public:
	buffer_layout() {}

	buffer_layout(std::initializer_list<buffer_element> elements)
		: m_elements(elements)
	{
		calculate_offsets_and_stride();
	}

	WHP_NODISCARD inline const uint64_t get_stride() const { return m_stride; }
	WHP_NODISCARD inline const std::vector<buffer_element>& get_elements() const { return m_elements; }

	WHP_NODISCARD buffer_element_iter begin() { return m_elements.begin(); }
	WHP_NODISCARD buffer_element_iter end() { return m_elements.end(); }
	WHP_NODISCARD buffer_element_const_iter begin() const { return m_elements.begin(); }
	WHP_NODISCARD buffer_element_const_iter end() const { return m_elements.end(); }
};

class vertex_buffer
{
public:
	virtual ~vertex_buffer() {}

	virtual void bind() const = 0;
	virtual void unbind() const = 0;

	virtual void set_layout(const buffer_layout& layout) = 0;
	virtual const buffer_layout& get_layout() const = 0;

	virtual void set_data(const void* data, uint32_t size) = 0;

	WHP_NODISCARD static ref<vertex_buffer> create(float* vertices, uint32_t size);
	WHP_NODISCARD static ref<vertex_buffer> create(uint32_t size);
};

class index_buffer
{
public:
	virtual ~index_buffer() {}

	virtual void bind() const = 0;
	virtual void unbind() const = 0;


	virtual uint32_t get_count() const = 0;

	WHP_NODISCARD static ref<index_buffer> create(uint32_t* indices, uint32_t count);
};


_WHIP_END
