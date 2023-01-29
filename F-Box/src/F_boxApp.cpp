#include <Whip.h>

#include <Platform/OpenGL/OpenGLShader.h>

#include "imgui/imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


class ExLayer : public Whip::Layer
{
private:
	Whip::ref<Whip::Shader> m_Shader;
	Whip::ref<Whip::VertexArray> m_VertexArray;
	Whip::ref<Whip::Shader> m_SquareShader, m_texture_shader;
	Whip::ref<Whip::VertexArray> m_SquareVertexArray;
	Whip::ref<Whip::Texture2D> m_texture;

	Whip::orthographic_camera m_camera;
	glm::vec3 m_camera_position;
	float m_camera_rotation = 0.0f;
	float m_camera_move_speed = 2.0f;
	float m_camera_rotation_speed = 120.0f;

	glm::vec3 m_square_color = { 0.5f, 0.1f, 0.5f };
	glm::vec3 m_triangle_color = { 0.1f, 0.1f, 0.5f };

	struct camera_movement_buttons
	{
		int camera_to_left, camera_to_right, camera_to_down, camera_to_up;
		int camera_rotation_left, camera_rotation_right;
		camera_movement_buttons() : camera_to_left(WHP_KEY_LEFT), camera_to_right(WHP_KEY_RIGHT), camera_to_down(WHP_KEY_DOWN), camera_to_up(WHP_KEY_UP), camera_rotation_left(WHP_KEY_A), camera_rotation_right(WHP_KEY_D) {}
		camera_movement_buttons(int ctl, int ctr, int ctd, int ctu, int crl, int crr) : camera_to_left(ctl), camera_to_right(ctr), camera_to_down(ctd), camera_to_up(ctu), camera_rotation_left(crl), camera_rotation_right(crr) {}
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

		m_VertexArray = Whip::VertexArray::Create();
		std::shared_ptr<Whip::VertexBuffer> vertexBuffer;
		vertexBuffer = Whip::VertexBuffer::Create(vertices, sizeof(vertices));
		vertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(vertexBuffer);
		std::shared_ptr<Whip::IndexBuffer> indexBuffer;
		indexBuffer = Whip::IndexBuffer::Create(indicies, sizeof(indicies) / sizeof(uint32_t));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		/////////////////// kare //////////////////

		float SquareVertices[4 * 5] =
		{
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
		};
		uint32_t squareIndicies[6] = { 0,1,2,2,3,0 };

		m_SquareVertexArray = Whip::VertexArray::Create();
		std::shared_ptr<Whip::VertexBuffer> squareVertexBuffer;
		squareVertexBuffer = Whip::VertexBuffer::Create(SquareVertices, sizeof(SquareVertices));
		squareVertexBuffer->SetLayout({
			{Whip::ShaderDataType::Float3, "a_Position"},
			{Whip::ShaderDataType::Float2, "a_texture_coord"}
			});
		m_SquareVertexArray->AddVertexBuffer(squareVertexBuffer);
		std::shared_ptr<Whip::IndexBuffer> squareIndexBuffer;
		squareIndexBuffer = Whip::IndexBuffer::Create(squareIndicies, sizeof(squareIndicies) / sizeof(uint32_t));
		m_SquareVertexArray->SetIndexBuffer(squareIndexBuffer);

		/////////////////////////////////////////////
		m_Shader = Whip::Shader::create(WHP_VERTEX_SRC, WHP_FRAGMENT_SRC);
		m_SquareShader = Whip::Shader::create(WHP_SECOND_VERTEX_SRC, WHP_SECOND_FRAGMENT_SRC);
		m_texture_shader = Whip::Shader::create(WHP_THIRD_VERTEX_SRC, WHP_THIRD_FRAGMENT_SRC);
		m_texture = Whip::Texture2D::create("assets/textures/test3.png");

		std::dynamic_pointer_cast<Whip::OpenGLShader>(m_texture_shader)->Bind();
		std::dynamic_pointer_cast<Whip::OpenGLShader>(m_texture_shader)->upload_uniform_int("u_texture", 0);
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

		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.09f));

		std::dynamic_pointer_cast<Whip::OpenGLShader>(m_SquareShader)->Bind();
		std::dynamic_pointer_cast<Whip::OpenGLShader>(m_SquareShader)->upload_uniform_float3("u_color", m_square_color);
		std::dynamic_pointer_cast<Whip::OpenGLShader>(m_Shader)->Bind();
		std::dynamic_pointer_cast<Whip::OpenGLShader>(m_Shader)->upload_uniform_float3("u_color", m_triangle_color);

		for (int y = 0; y < 20; ++y)
		{
			for (int x = 0; x < 20; ++x)
			{
				glm::vec3 pos((float)x * 0.1f, (float)y * 0.1f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;
				Whip::Renderer::submit(m_SquareShader, m_SquareVertexArray, transform);
			}
		}

		m_texture->bind();
		Whip::Renderer::submit(m_texture_shader, m_SquareVertexArray, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));
		//Whip::Renderer::submit(m_Shader, m_VertexArray);

		Whip::Renderer::end_scene();

	}
	void OnImGuiRender() override
	{
		ImGui::Begin("Color Settings");
		ImGui::ColorEdit3("Square Color", glm::value_ptr(m_square_color));
		ImGui::ColorEdit3("Triangle Color", glm::value_ptr(m_triangle_color));
		ImGui::End();
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
