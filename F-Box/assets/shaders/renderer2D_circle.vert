#type vertex
#version 450 core

layout(location = 0) in vec3 a_world_position;
layout(location = 1) in vec3 a_local_position;
layout(location = 2) in vec4 a_color;
layout(location = 3) in float a_thickness;
layout(location = 4) in float a_fade;
layout(location = 5) in int a_entityID;

layout(std140, binding = 0) uniform camera
{
	mat4 u_view_projection;
};

struct vertex_output
{
	vec3 local_position;
	vec4 color;
	float thickness;
	float fade;
};

layout (location = 0) out vertex_output Output;
layout (location = 4) out flat int v_entityID;

void main()
{
	Output.local_position = a_local_position;
	Output.color = a_color;
	Output.thickness = a_thickness;
	Output.fade = a_fade;

	v_entityID = a_entityID;

	gl_Position = u_view_projection * vec4(a_world_position, 1.0);
}
