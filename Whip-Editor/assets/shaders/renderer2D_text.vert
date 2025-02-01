#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_texture_coord;
layout(location = 3) in int a_entityID;

layout(std140, binding = 0) uniform camera
{
	mat4 u_view_projection;
};

layout (location = 0) out vec4 v_color;
layout (location = 1) out vec2 v_texture_coord;
layout (location = 2) out flat int v_entityID;

void main()
{
	v_color = a_color;
	v_texture_coord = a_texture_coord;
	v_entityID = a_entityID;

	gl_Position = u_view_projection * vec4(a_position, 1.0);
}
