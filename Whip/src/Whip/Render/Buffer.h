#pragma once

#include <Whip/Core.h>


_WHIP_START

enum class ShaderDataType : uint16_t
{
	None = 0,
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

WHP_NODISCARD static uint32_t ShaderDataTypeSize(ShaderDataType type)
{
	switch (type)
	{
		case Whip::ShaderDataType::None:		WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
		case Whip::ShaderDataType::Float:		return sizeof(float);
		case Whip::ShaderDataType::Float2:		return sizeof(float) * 2;
		case Whip::ShaderDataType::Float3:		return sizeof(float) * 3;
		case Whip::ShaderDataType::Float4:		return sizeof(float) * 4;
		case Whip::ShaderDataType::Mat3:		return sizeof(float) * 3 * 3;
		case Whip::ShaderDataType::Mat4:		return sizeof(float) * 4 * 4;
		case Whip::ShaderDataType::Bool:		return sizeof(bool);
		case Whip::ShaderDataType::Int:			return sizeof(int);
		case Whip::ShaderDataType::Int2:		return sizeof(int) * 2;
		case Whip::ShaderDataType::Int3:		return sizeof(int) * 3;
		case Whip::ShaderDataType::Int4:		return sizeof(int) * 4;
	}
	WHP_CORE_ASSERT(false, "Unknown ShaderDataType!");
	return 0;
}

struct BufferElement
{
	std::string name;
	ShaderDataType type;
	uint32_t size;
	uint32_t offset;
	bool normalized;

	BufferElement() {}
	
	BufferElement(ShaderDataType type_in, const std::string& name_in, bool normalized_in = false)
		: name(name_in), type(type_in), size(ShaderDataTypeSize(type_in)), offset(0), normalized(normalized_in) {}

	uint32_t GetComponentCount() const
	{
		switch (type)
		{
			case Whip::ShaderDataType::None:		WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
			case Whip::ShaderDataType::Float:		return 1;
			case Whip::ShaderDataType::Float2:		return 2;
			case Whip::ShaderDataType::Float3:		return 3;
			case Whip::ShaderDataType::Float4:		return 4;
			case Whip::ShaderDataType::Mat3:		return 3 * 3;
			case Whip::ShaderDataType::Mat4:		return 4 * 4;
			case Whip::ShaderDataType::Bool:		return 1;
			case Whip::ShaderDataType::Int:			return 1;
			case Whip::ShaderDataType::Int2:		return 2;
			case Whip::ShaderDataType::Int3:		return 3;
			case Whip::ShaderDataType::Int4:		return 4;
		}
		WHP_CORE_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}
};

class BufferLayout
{
	using BufferElementVecIt = std::vector<BufferElement>::iterator;
	using BufferElementVecConIt = std::vector<BufferElement>::const_iterator;
private:
	std::vector<BufferElement> m_Elements;
	uint32_t m_Stride = 0;
private:
	void CalculateOffsetsAndStride()
	{
		uint32_t offset = 0;
		m_Stride = 0;
		for (auto& elem : m_Elements)
		{
			elem.offset = offset;
			offset += elem.size;
			m_Stride += elem.size;
		}
	}
public:
	BufferLayout() {}

	BufferLayout(const std::initializer_list<BufferElement>& elements) 
		: m_Elements(elements) 
	{
		CalculateOffsetsAndStride();
	}

	inline const uint32_t GetStride() const { return m_Stride; }
	inline const std::vector<BufferElement>& getElements() const { return m_Elements; }
	
	BufferElementVecIt begin() { return m_Elements.begin(); }
	BufferElementVecIt end() { return m_Elements.end(); }
	BufferElementVecConIt begin() const { return m_Elements.begin(); }
	BufferElementVecConIt end() const { return m_Elements.end(); }
};

class VertexBuffer
{
public:
	virtual ~VertexBuffer() {}

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual void SetLayout(const BufferLayout& layout) = 0;
	virtual const BufferLayout& GetLayout() const = 0;
	
	static VertexBuffer* Create(float* vertices, uint32_t size);
};

class IndexBuffer
{
public:
	virtual ~IndexBuffer() {}

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;


	virtual uint32_t GetCount() const = 0;

	static IndexBuffer* Create(renderer_id_t* indices, uint32_t count);
};


_WHIP_END