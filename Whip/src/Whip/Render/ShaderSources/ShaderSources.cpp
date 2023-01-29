#include "whippch.h"
#include "ShaderSources.h"


_WHIP_START

const std::string ShaderSources::VertexSrc = 
	R"(
		#version 330 core

		layout(location = 0) in vec3 a_Position;

		uniform mat4 u_view_projection;
		uniform mat4 u_transform;

		
		void main()
		{
			gl_Position = u_view_projection * u_transform * vec4(a_Position, 1.0);
		}
	)";

const std::string ShaderSources::FragmentSrc = 
	R"(
		#version 330 core

		layout(location = 0) out vec4 color;	

		uniform vec3 u_color;

		void main()
		{
			color = vec4(u_color, 1.0f);
		}
	)";

const std::string ShaderSources::SecondVertexSrc =
R"(
		#version 330 core

		layout(location = 0) in vec3 a_Position;

		uniform mat4 u_view_projection;
		uniform mat4 u_transform;
		
		out vec3 v_Position;
		
		void main()
		{
			v_Position = a_Position;
			gl_Position = u_view_projection * u_transform * vec4(a_Position, 1.0);
		}
	)";

const std::string ShaderSources::SecondFragmentSrc =
R"(
		#version 330 core

		layout(location = 0) out vec4 color;

		uniform vec3 u_color;

		in vec3 v_Position;

		void main()
		{
			color = vec4(u_color, 1.0f);
		}
	)";

const std::string ShaderSources::ThirdVertexSrc =
R"(
		#version 330 core

		layout(location = 0) in vec3 a_Position;
		layout(location = 1) in vec2 a_texture_coord;

		uniform mat4 u_view_projection;
		uniform mat4 u_transform;
		
		out vec2 v_texture_coord;
		
		void main()
		{
			v_texture_coord = a_texture_coord;
			gl_Position = u_view_projection * u_transform * vec4(a_Position, 1.0);
		}
	)";

const std::string ShaderSources::ThirdFragmentSrc =
R"(
		#version 330 core

		layout(location = 0) out vec4 color;

		in vec2 v_texture_coord;

		uniform sampler2D u_texture;

		void main()
		{
			color = texture(u_texture, v_texture_coord);
		}
	)";

_WHIP_END


