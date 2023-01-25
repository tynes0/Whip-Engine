#include <whippch.h>
#include <Whip/Application.h>
#include <Whip/Input.h>
#include <Whip/Render/ShaderSources/ShaderSources.h>
#include <Whip/Render/Renderer.h>


_WHIP_START

Application* Application::s_Instance = nullptr;

Application::Application()
{
	WHP_CORE_ASSERT(!s_Instance, "Application already exist!");
	s_Instance = this;
	m_Window = std::unique_ptr<Window>(Window::Create());
	m_Window->SetEventCallback(WHP_BIND_EVENT_FN(Application::OnEvent));
	m_Window->SetEventCallback(std::bind(&Application::OnEvent, this, std::placeholders::_1));
	m_ImGuiLayer = new ImGuiLayer();
	PushOverlay(m_ImGuiLayer);

	// We need
	// Vertex array
	// Vertex buffer 
	// Index buffer

	// x - y - z
	float vertices[3 * 7] =
	{
		 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
		 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
	};
	uint32_t indicies[3] = { 0,1,2 };

	BufferLayout layout =
	{
		{ShaderDataType::Float3, "a_Position"},
		{ShaderDataType::Float4, "a_Color"}
	};

	m_VertexArray.reset(VertexArray::Create());
	//
	std::shared_ptr<VertexBuffer> vertexBuffer;
	vertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));
	vertexBuffer->SetLayout(layout);
	m_VertexArray->AddVertexBuffer(vertexBuffer);
	std::shared_ptr<IndexBuffer> indexBuffer;
	indexBuffer.reset(IndexBuffer::Create(indicies, sizeof(indicies) / sizeof(uint32_t)));
	m_VertexArray->SetIndexBuffer(indexBuffer);

	/////////////////// kare //////////////////

	float SquareVertices[4 * 3] =
	{
		-0.75f, -0.75f, 0.0f,
		 0.75f, -0.75f, 0.0f,
		 0.75f,  0.75f, 0.0f,
		-0.75f,  0.75f, 0.0f
	};
	uint32_t squareIndicies[6] = { 0,1,2,2,3,0 };

	m_SquareVertexArray.reset(VertexArray::Create());
	std::shared_ptr<VertexBuffer> squareVertexBuffer;
	squareVertexBuffer.reset(VertexBuffer::Create(SquareVertices, sizeof(SquareVertices)));
	squareVertexBuffer->SetLayout({
		{ShaderDataType::Float3, "a_Position"}
		});
	m_SquareVertexArray->AddVertexBuffer(squareVertexBuffer);
	std::shared_ptr<IndexBuffer> squareIndexBuffer;
	squareIndexBuffer.reset(IndexBuffer::Create(squareIndicies, sizeof(squareIndicies) / sizeof(uint32_t)));
	m_SquareVertexArray->SetIndexBuffer(squareIndexBuffer);

	/////////////////////////////////////////////
	m_Shader.reset(new Shader(WHP_VERTEX_SRC, WHP_FRAGMENT_SRC));
	m_SquareShader.reset(new Shader(WHP_SECOND_VERTEX_SRC, WHP_SECOND_FRAGMENT_SRC));
}

Application::~Application()
{

}

void Application::Run()
{
	while (m_Running)
	{
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		RenderCommand::Clear();

		Renderer::BeginScene();

		m_SquareShader->Bind();
		Renderer::Submit(m_SquareVertexArray);

		m_Shader->Bind();
		Renderer::Submit(m_VertexArray);

		Renderer::EndScene();


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
	//WHP_CORE_DEBUG("{0}", e);

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
	WHP_CORE_DEBUG("Window destroyed!");
	return true;
}

_WHIP_END