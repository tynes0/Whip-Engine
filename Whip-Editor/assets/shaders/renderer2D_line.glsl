#type vertex
#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in int a_entityID;

layout(std140, binding = 0) uniform camera
{
	mat4 u_view_projection;
};

struct vertex_output
{
	vec4 color;
};

layout (location = 0) out vertex_output Output;
layout (location = 1) out flat int v_entityID;

void main()
{
	Output.color = a_color;
	v_entityID = a_entityID;

	gl_Position = u_view_projection * vec4(a_position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_color;
layout(location = 1) out int o_entityID;

struct vertex_output
{
	vec4 color;
};

layout (location = 0) in vertex_output Input;
layout (location = 1) in flat int v_entityID;

void main()
{
	o_color = Input.color;
	o_entityID = v_entityID;
}
