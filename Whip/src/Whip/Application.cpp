#include <whippch.h>
#include <Whip/Application.h>
#include <Whip/Input.h>
#include <Whip/Render/ShaderSources/ShaderSources.h>

#include <glad/glad.h>

_WHIP_START

Application* Application::s_Instance = nullptr;

static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
{
	switch (type)
	{
		case Whip::ShaderDataType::None:		WHP_CORE_ASSERT(false, "ShaderDataType is None!"); return 0;
		case Whip::ShaderDataType::Float:		return GL_FLOAT;
		case Whip::ShaderDataType::Float2:		return GL_FLOAT;
		case Whip::ShaderDataType::Float3:		return GL_FLOAT;
		case Whip::ShaderDataType::Float4:		return GL_FLOAT;
		case Whip::ShaderDataType::Mat3:		return GL_FLOAT;
		case Whip::ShaderDataType::Mat4:		return GL_FLOAT;
		case Whip::ShaderDataType::Bool:		return GL_BOOL;
		case Whip::ShaderDataType::Int:			return GL_INT;
		case Whip::ShaderDataType::Int2:		return GL_INT;
		case Whip::ShaderDataType::Int3:		return GL_INT;
		case Whip::ShaderDataType::Int4:		return GL_INT;
	}
	WHP_CORE_ASSERT(false, "Unknown ShaderDataType!");
	return 0;
}

Application::Application()
{
	WHP_CORE_ASSERT(!s_Instance, "Application already exist!");
	s_Instance = this;
	m_Window = std::unique_ptr<Window>(Window::Create());
	m_Window->SetEventCallback(WHP_BIND_EVENT_FN(Application::OnEvent));
	m_ImGuiLayer = new ImGuiLayer();
	PushOverlay(m_ImGuiLayer);

	// We need
	// Vertex array
	// Vertex buffer 
	// Index buffer
	glGenVertexArrays(1, &m_VertexArray);
	glBindVertexArray(m_VertexArray);

	// x - y - z
	float vertices[3 * 7] =
	{
		 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
		 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
	};

	m_VertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));
	
	{
		BufferLayout layout =
		{
			{ShaderDataType::Float3, "a_Position"},
			{ShaderDataType::Float4, "a_Color"}
		};

		m_VertexBuffer->SetLayout(layout);
	}
	uint32_t index = 0;
	for (const auto& elem : m_VertexBuffer->GetLayout())
	{
		glEnableVertexAttribArray(index);
		glVertexAttribPointer(index, elem.GetComponentCount(), ShaderDataTypeToOpenGLBaseType(elem.type), elem.normalized ? GL_TRUE : GL_FALSE, m_VertexBuffer->GetLayout().GetStride(), (const void*)elem.offset);
		index++;
	}

	
	unsigned int indicies[3] = { 0,1,2 };

	m_IndexBuffer.reset(IndexBuffer::Create(indicies, sizeof(indicies) / sizeof(uint32_t)));

	m_Shader.reset(new Shader(WHP_VERTEX_SRC, WHP_FRAGMENT_SRC));
}


Application::~Application()
{

}

void Application::Run()
{
	while (m_Running)
	{
		m_Shader->Bind();
		glDrawElements(GL_TRIANGLES, m_IndexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);

		for (layerptr item : m_LayerStack)
		{
			item->OnUpdate();
		}

		m_ImGuiLayer->begin();
		for (layerptr item : m_LayerStack)
		{
			item->OnImGuiRender();
		}
		m_ImGuiLayer->end();
		m_Window->OnUpdate();
	}
}

void Application::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowCloseEvent>(WHP_BIND_EVENT_FN(Application::OnWindowClose));
	//WHP_CORE_DEBUGL("{0}", e);

	for (auto iter = m_LayerStack.end(); iter != m_LayerStack.begin(); )
	{
		(*--iter)->OnEvent(e);
		if (e.Handled)
		{
			break;
		}
	}
}

void Application::PushLayer(layerptr layer)
{
	m_LayerStack.PushLayer(layer);
	layer->OnAttach();
}

void Application::PushOverlay(layerptr overlay)
{
	m_LayerStack.PushOverlay(overlay);
	overlay->OnAttach();
}

bool Application::OnWindowClose(WindowCloseEvent& event)
{
	m_Running = false;
	WHP_CORE_DEBUGL("Window destroyed!");
	return true;
}

_WHIP_END