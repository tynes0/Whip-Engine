#include <Whip.h>

#include "F_boxApp2d.h"

#include <Platform/OpenGL/OpenGLShader.h>

#include "imgui/imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

 /*class ex_layer : public whip::layer
{
private:
	whip::shader_library m_shader_library;
	whip::ref<whip::vertex_array> m_vertex_array;
	whip::ref<whip::vertex_array> m_square_vertex_array;
	whip::ref<whip::texture2D> m_texture;

	whip::orthographic_camera_controller m_camera_controller;

	glm::vec3 m_square_color = { 0.5f, 0.1f, 0.5f };
	glm::vec3 m_triangle_color = { 0.1f, 0.1f, 0.5f };
public:
	ex_layer()
		: layer("Whip Layer"), m_camera_controller(whip::calculate_aspect_ratio(1280.0f, 720.0f), true)
	{

		// x - y - z
		float vertices[3 * 7] =
		{
			 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
		};

		uint32_t indicies[3] = { 0,1,2 };

		m_vertex_array = whip::vertex_array::create();
		whip::ref<whip::vertex_buffer> a_vertex_buffer;
		a_vertex_buffer = whip::vertex_buffer::create(vertices, sizeof(vertices));
		a_vertex_buffer->set_layout({
			{whip::shader_data_type::Float3, "a_Position"},
			{whip::shader_data_type::Float4, "a_Color"}
			});
		m_vertex_array->add_vertex_buffer(a_vertex_buffer);
		whip::ref<whip::index_buffer> a_index_buffer;
		a_index_buffer = whip::index_buffer::create(indicies, sizeof(indicies) / sizeof(uint32_t));
		m_vertex_array->set_index_buffer(a_index_buffer);

		/////////////////// kare //////////////////

		float square_verticies[4 * 5] =
		{
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
		};
		uint32_t square_indicies[6] = { 0,1,2,2,3,0 };

		m_square_vertex_array = whip::vertex_array::create();
		whip::ref<whip::vertex_buffer> square_vertex_buffer;
		square_vertex_buffer = whip::vertex_buffer::create(square_verticies, sizeof(square_verticies));
		square_vertex_buffer->set_layout({
			{whip::shader_data_type::Float3, "a_Position"},
			{whip::shader_data_type::Float2, "a_texture_coord"}
			});
		m_square_vertex_array->add_vertex_buffer(square_vertex_buffer);
		whip::ref<whip::index_buffer> square_index_buffer;
		square_index_buffer = whip::index_buffer::create(square_indicies, sizeof(square_indicies) / sizeof(uint32_t));
		m_square_vertex_array->set_index_buffer(square_index_buffer);

		/////////////////////////////////////////////
		m_shader_library.load("triangle", "C:\\Dev\\Whip\\F-Box\\assets\\shaders\\triangle.vert", "C:\\Dev\\Whip\\F-Box\\assets\\shaders\\triangle.frag");
		m_shader_library.load("square" ,"C:\\Dev\\Whip\\F-Box\\assets\\shaders\\square.vert", "C:\\Dev\\Whip\\F-Box\\assets\\shaders\\square.frag");
		auto texture_shader = m_shader_library.load("C:\\Dev\\Whip\\F-Box\\assets\\shaders\\texture.glsl");

		m_texture = whip::texture2D::create("C:\\Dev\\Whip\\F-Box\\assets\\textures\\test3.png");

		std::dynamic_pointer_cast<whip::opengl_shader>(texture_shader)->bind();
		std::dynamic_pointer_cast<whip::opengl_shader>(texture_shader)->upload_uniform_int("u_texture", 0);
	} 

	void on_update(whip::timestep ts) override
	{
		m_camera_controller.on_update(ts);

		whip::render_command::set_clear_color({ 0.1f, 0.1f, 0.1f, 1.0f });
		whip::render_command::clear();

		whip::renderer::begin_scene(m_camera_controller.get_camera());

		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.09f));

		auto square_shader = m_shader_library.get("square");
		auto triangle_shader = m_shader_library.get("triangle");

		std::dynamic_pointer_cast<whip::opengl_shader>(square_shader)->bind();
		std::dynamic_pointer_cast<whip::opengl_shader>(square_shader)->upload_uniform_float3("u_color", m_square_color);
		std::dynamic_pointer_cast<whip::opengl_shader>(triangle_shader)->bind();
		std::dynamic_pointer_cast<whip::opengl_shader>(triangle_shader)->upload_uniform_float3("u_color", m_triangle_color);

		for (int y : whip::range(20))
		{
			for (int x = 0; x < 20; ++x)
			{
				glm::vec3 pos((float)x * 0.1f, (float)y * 0.1f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;
				whip::renderer::submit(square_shader, m_square_vertex_array, transform);
			}
		}

		m_texture->bind();
		whip::renderer::submit(m_shader_library.get("texture"), m_square_vertex_array, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));
		//Whip::Renderer::submit(m_Shader, m_VertexArray);

		whip::renderer::end_scene();

	}
	void on_imgui_render() override
	{
		ImGui::Begin("Color Settings");
		ImGui::ColorEdit3("Square Color", glm::value_ptr(m_square_color));
		ImGui::ColorEdit3("Triangle Color", glm::value_ptr(m_triangle_color));
		ImGui::End();
	}

	void on_event(whip::Event& evnt) override
	{
		m_camera_controller.on_event(evnt);
	}
};*/


class f_box : public whip::application
{
public:
	f_box()
	{
		//push_layer(new ex_layer());
		push_layer(new fbox_app2D());
	}
	~f_box()
	{

	}
};

whip::application* whip::create_application()
{
	return new f_box();
}
