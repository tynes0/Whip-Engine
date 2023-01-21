#include "whippch.h"
#include "ShaderSources.h"


_WHIP_START

const std::string ShaderSources::VertexSrc = 
	R"(
		#version 330 core

		layout(location = 0) in vec3 a_Position;
		layout(location = 1) in vec4 a_Color;

		out vec3 v_Position;
		out vec4 v_Color;
		
		void main()
		{
			v_Position = a_Position;
			v_Color = a_Color;
			gl_Position = vec4(a_Position, 1.0);
		}
	)";

const std::string ShaderSources::FragmentSrc = 
	R"(
		#version 330 core

		layout(location = 0) out vec4 color;

		in vec3 v_Position;
		in vec4 v_Color;		

		void main()
		{
			color = vec4(v_Position * 0.5 + 0.5, 1.0);
			color = v_Color;
		}
	)";

const std::string ShaderSources::SecondVertexSrc =
R"(
		#version 330 core

		layout(location = 0) in vec3 a_Position;

		out vec3 v_Position;
		
		void main()
		{
			v_Position = a_Position;
			gl_Position = vec4(a_Position, 1.0);
		}
	)";

const std::string ShaderSources::SecondFragmentSrc =
R"(
		#version 330 core

		layout(location = 0) out vec4 color;

		in vec3 v_Position;

		void main()
		{
			color = vec4(0.3, 0.5, 0.7, 1.0);
		}
	)";

_WHIP_END


