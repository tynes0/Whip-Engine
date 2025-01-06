#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_texture_coord;
layout(location = 3) in float a_texture_index;
layout(location = 4) in float a_tiling_factor;
layout(location = 5) in int a_entityID;

layout(std140, binding = 0) uniform camera
{
	mat4 u_view_projection;
};

struct vertex_output
{
	vec4 color;
	vec2 texture_coord;
	float tiling_factor;
};

layout (location = 0) out vertex_output Output;
layout (location = 3) out flat float v_texture_index;
layout (location = 4) out flat int v_entityID;

void main()
{
	Output.color = a_color;
	Output.texture_coord = a_texture_coord;
	Output.tiling_factor = a_tiling_factor;
	v_texture_index = a_texture_index;
	v_entityID = a_entityID;

	gl_Position = u_view_projection * vec4(a_position, 1.0);
}
