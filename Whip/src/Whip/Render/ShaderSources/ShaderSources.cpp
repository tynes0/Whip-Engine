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

_WHIP_END


