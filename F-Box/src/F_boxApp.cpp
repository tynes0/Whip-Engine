#include <Whip.h>

#include "imgui/imgui.h"

#include <glm/gtc/matrix_transform.hpp>

class ExLayer : public Whip::Layer
{
private:
	std::shared_ptr<Whip::Shader> m_Shader;
	std::shared_ptr<Whip::VertexArray> m_VertexArray;
	std::shared_ptr<Whip::Shader> m_SquareShader;
	std::shared_ptr<Whip::VertexArray> m_SquareVertexArray;

	Whip::orthographic_camera m_camera;
	glm::vec3 m_camera_position;
	float m_camera_rotation = 0.0f;
	float m_camera_move_speed = 2.0f;
	float m_camera_rotation_speed = 120.0f;

	struct camera_movement_buttons
	{
		int camera_to_left, camera_to_right, camera_to_down, camera_to_up;
		int camera_rotation_left, camera_rotation_right;
		camera_movement_buttons()
			: camera_to_left(WHP_KEY_LEFT), camera_to_right(WHP_KEY_RIGHT),
			camera_to_down(WHP_KEY_DOWN), camera_to_up(WHP_KEY_UP),
			camera_rotation_left(WHP_KEY_A), camera_rotation_right(WHP_KEY_D) {}
		camera_movement_buttons(int ctl, int ctr, int ctd, int ctu, int crl, int crr)
			: camera_to_left(ctl), camera_to_right(ctr),
			camera_to_down(ctd), camera_to_up(ctu),
			camera_rotation_left(crl), camera_rotation_right(crr) {}
	};
	camera_movement_buttons buttons{WHP_KEY_A, WHP_KEY_D, WHP_KEY_S, WHP_KEY_W, WHP_KEY_LEFT, WHP_KEY_RIGHT};
public:
	ExLayer() 
		: Layer("Whip Layer"), m_camera(-1.6f, 1.6f, -0.9f, 0.9f) , m_camera_position(0.0f)
	{
		// x - y - z
		float vertices[3 * 7] =
		{
			 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
		};
		uint32_t indicies[3] = { 0,1,2 };

		Whip::BufferLayout layout =
		{
			{Whip::ShaderDataType::Float3, "a_Position"},
			{Whip::ShaderDataType::Float4, "a_Color"}
		};

		m_VertexArray.reset(Whip::VertexArray::Create());
		//
		std::shared_ptr<Whip::VertexBuffer> vertexBuffer;
		vertexBuffer.reset(Whip::VertexBuffer::Create(vertices, sizeof(vertices)));
		vertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(vertexBuffer);
		std::shared_ptr<Whip::IndexBuffer> indexBuffer;
		indexBuffer.reset(Whip::IndexBuffer::Create(indicies, sizeof(indicies) / sizeof(uint32_t)));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		/////////////////// kare //////////////////

		float SquareVertices[4 * 3] =
		{
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.5f,  0.5f, 0.0f,
			-0.5f,  0.5f, 0.0f
		};
		uint32_t squareIndicies[6] = { 0,1,2,2,3,0 };

		m_SquareVertexArray.reset(Whip::VertexArray::Create());
		std::shared_ptr<Whip::VertexBuffer> squareVertexBuffer;
		squareVertexBuffer.reset(Whip::VertexBuffer::Create(SquareVertices, sizeof(SquareVertices)));
		squareVertexBuffer->SetLayout({
			{Whip::ShaderDataType::Float3, "a_Position"}
			});
		m_SquareVertexArray->AddVertexBuffer(squareVertexBuffer);
		std::shared_ptr<Whip::IndexBuffer> squareIndexBuffer;
		squareIndexBuffer.reset(Whip::IndexBuffer::Create(squareIndicies, sizeof(squareIndicies) / sizeof(uint32_t)));
		m_SquareVertexArray->SetIndexBuffer(squareIndexBuffer);

		/////////////////////////////////////////////
		m_Shader.reset(new Whip::Shader(WHP_VERTEX_SRC, WHP_FRAGMENT_SRC));
		m_SquareShader.reset(new Whip::Shader(WHP_SECOND_VERTEX_SRC, WHP_SECOND_FRAGMENT_SRC));

	}

	void OnUpdate(Whip::timestep ts) override
	{
		if (Whip::Input::isKeyPressed(buttons.camera_to_left))
		{
			m_camera_position.x -= m_camera_move_speed * ts;
		}
		else if (Whip::Input::isKeyPressed(buttons.camera_to_right))
		{
			m_camera_position.x += m_camera_move_speed * ts;
		}
		if (Whip::Input::isKeyPressed(buttons.camera_to_down))
		{
			m_camera_position.y -= m_camera_move_speed * ts;
		}
		else if (Whip::Input::isKeyPressed(buttons.camera_to_up))
		{
			m_camera_position.y += m_camera_move_speed * ts;
		}
		if (Whip::Input::isKeyPressed(buttons.camera_rotation_left))
		{
			m_camera_rotation += m_camera_rotation_speed * ts;
		}
		else if (Whip::Input::isKeyPressed(buttons.camera_rotation_right))
		{
			m_camera_rotation -= m_camera_rotation_speed * ts;
		}

		Whip::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
		Whip::RenderCommand::Clear();

		m_camera.set_position(m_camera_position);
		m_camera.set_rotation(m_camera_rotation);

		Whip::Renderer::begin_scene(m_camera);

		static glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.09f));

		for (int y = 0; y < 20; ++y)
		{
			for (int x = 0; x < 20; ++x)
			{
				glm::vec3 pos((float)x * 0.1f, (float)y * 0.1f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;
				Whip::Renderer::submit(m_SquareShader, m_SquareVertexArray, transform);
			}
		}

		//Whip::Renderer::submit(m_Shader, m_VertexArray);

		Whip::Renderer::end_scene();

	}
	void OnEvent(Whip::Event& event) override
	{

	}
};

class F_Box : public Whip::Application
{
public:
	F_Box()
	{
		PushLayer(new ExLayer());
	}
	~F_Box()
	{

	}
};

Whip::Application* Whip::CreateApplication()
{
	return new F_Box();
}
