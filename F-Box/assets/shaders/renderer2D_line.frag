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
