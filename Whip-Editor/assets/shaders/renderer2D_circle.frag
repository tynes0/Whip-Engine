#version 450 core

layout(location = 0) out vec4 o_color;
layout(location = 1) out int o_entityID;

struct vertex_output
{
	vec3 local_position;
	vec4 color;
	float thickness;
	float fade;
};

layout (location = 0) in vertex_output Input;
layout (location = 4) in flat int v_entityID;

void main()
{
    // Calculate distance and fill circle with white
    float distance = 1.0 - length(Input.local_position);
    float circle = smoothstep(0.0, Input.fade, distance);
    circle *= smoothstep(Input.thickness + Input.fade, Input.thickness, distance);

	if (circle == 0.0)
		discard;

    // Set output color
    o_color = Input.color;
	o_color.a *= circle;

	o_entityID = v_entityID;
}
