#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Memory.h>
#include <vector>

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
	case ShaderDataType::None:		WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
	case ShaderDataType::Float:		return sizeof(float);
	case ShaderDataType::Float2:		return sizeof(float) * 2;
	case ShaderDataType::Float3:		return sizeof(float) * 3;
	case ShaderDataType::Float4:		return sizeof(float) * 4;
	case ShaderDataType::Mat3:		return sizeof(float) * 3 * 3;
	case ShaderDataType::Mat4:		return sizeof(float) * 4 * 4;
	case ShaderDataType::Bool:		return sizeof(bool);
	case ShaderDataType::Int:			return sizeof(int);
	case ShaderDataType::Int2:		return sizeof(int) * 2;
	case ShaderDataType::Int3:		return sizeof(int) * 3;
	case ShaderDataType::Int4:		return sizeof(int) * 4;
	}
	WHP_CORE_ASSERT(false, "Unknown ShaderDataType!");
	return 0;
}

struct BufferElement
{
	std::string m_Name;
	ShaderDataType m_Type;
	uint64_t m_Size;
	uint64_t m_Offset;
	bool m_Normalized;

	BufferElement() {}

	BufferElement(ShaderDataType typeIn, const std::string& nameIn, bool normalizedIn = false)
		: m_Name(nameIn), m_Type(typeIn), m_Size(ShaderDataTypeSize(typeIn)), m_Offset(0), m_Normalized(normalizedIn) {}

	WHP_NODISCARD uint32_t GetComponentCount() const
	{
		switch (m_Type)
		{
		case ShaderDataType::None:		WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
		case ShaderDataType::Float:		return 1;
		case ShaderDataType::Float2:		return 2;
		case ShaderDataType::Float3:		return 3;
		case ShaderDataType::Float4:		return 4;
		case ShaderDataType::Mat3:		return 3 * 3;
		case ShaderDataType::Mat4:		return 4 * 4;
		case ShaderDataType::Bool:		return 1;
		case ShaderDataType::Int:			return 1;
		case ShaderDataType::Int2:		return 2;
		case ShaderDataType::Int3:		return 3;
		case ShaderDataType::Int4:		return 4;
		}
		WHP_CORE_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}
};

class BufferLayout
{
	using BufferElementIter		= std::vector<BufferElement>::iterator;
	using BufferElementConstIter = std::vector<BufferElement>::const_iterator;
private:
	std::vector<BufferElement> m_Elements;
	uint64_t m_Stride = 0;
private:
	void CalculateOffsetsAndStride()
	{
		uint64_t offset = 0;
		m_Stride = 0;
		for (auto& elem : m_Elements)
		{
			elem.m_Offset = offset;
			offset += elem.m_Size;
			m_Stride += elem.m_Size;
		}
	}
public:
	BufferLayout() {}

	BufferLayout(std::initializer_list<BufferElement> elements)
		: m_Elements(elements)
	{
		CalculateOffsetsAndStride();
	}

	WHP_NODISCARD inline const uint64_t GetStride() const { return m_Stride; }
	WHP_NODISCARD inline const std::vector<BufferElement>& GetElements() const { return m_Elements; }

	WHP_NODISCARD BufferElementIter begin() { return m_Elements.begin(); }
	WHP_NODISCARD BufferElementIter end() { return m_Elements.end(); }
	WHP_NODISCARD BufferElementConstIter begin() const { return m_Elements.begin(); }
	WHP_NODISCARD BufferElementConstIter end() const { return m_Elements.end(); }
};

class VertexBuffer
{
public:
	virtual ~VertexBuffer() {}

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;

	virtual void SetLayout(const BufferLayout& layout) = 0;
	virtual const BufferLayout& GetLayout() const = 0;

	virtual void SetData(const void* data, uint32_t size) = 0;

	WHP_NODISCARD static Ref<VertexBuffer> Create(float* vertices, uint32_t size);
	WHP_NODISCARD static Ref<VertexBuffer> Create(uint32_t size);
};

class IndexBuffer
{
public:
	virtual ~IndexBuffer() {}

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;


	virtual uint32_t GetCount() const = 0;

	WHP_NODISCARD static Ref<IndexBuffer> Create(uint32_t* indices, uint32_t count);
};


_WHIP_END
