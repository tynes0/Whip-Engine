//#type vertex
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

//#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 v_texture_coord;

uniform sampler2D u_texture;

void main()
{
	color = texture(u_texture, v_texture_coord);
}