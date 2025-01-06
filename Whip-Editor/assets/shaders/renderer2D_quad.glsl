#type vertex
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

#type fragment

#version 450 core

layout(location = 0) out vec4 o_color;
layout(location = 1) out int o_entity_id;

struct vertex_output
{
	vec4 color;
	vec2 texture_coord;
	float tiling_factor;
};

layout (location = 0) in vertex_output Input;
layout (location = 3) in flat float v_texture_index;
layout (location = 4) in flat int v_entityID;

layout (binding = 0) uniform sampler2D u_textures[32];

void main()
{
	vec4 texture_color = Input.color;
	switch(int(v_texture_index))
	{
		case  0: texture_color *= texture(u_textures[ 0], Input.texture_coord * Input.tiling_factor); break;
		case  1: texture_color *= texture(u_textures[ 1], Input.texture_coord * Input.tiling_factor); break;
		case  2: texture_color *= texture(u_textures[ 2], Input.texture_coord * Input.tiling_factor); break;
		case  3: texture_color *= texture(u_textures[ 3], Input.texture_coord * Input.tiling_factor); break;
		case  4: texture_color *= texture(u_textures[ 4], Input.texture_coord * Input.tiling_factor); break;
		case  5: texture_color *= texture(u_textures[ 5], Input.texture_coord * Input.tiling_factor); break;
		case  6: texture_color *= texture(u_textures[ 6], Input.texture_coord * Input.tiling_factor); break;
		case  7: texture_color *= texture(u_textures[ 7], Input.texture_coord * Input.tiling_factor); break;
		case  8: texture_color *= texture(u_textures[ 8], Input.texture_coord * Input.tiling_factor); break;
		case  9: texture_color *= texture(u_textures[ 9], Input.texture_coord * Input.tiling_factor); break;
		case 10: texture_color *= texture(u_textures[10], Input.texture_coord * Input.tiling_factor); break;
		case 11: texture_color *= texture(u_textures[11], Input.texture_coord * Input.tiling_factor); break;
		case 12: texture_color *= texture(u_textures[12], Input.texture_coord * Input.tiling_factor); break;
		case 13: texture_color *= texture(u_textures[13], Input.texture_coord * Input.tiling_factor); break;
		case 14: texture_color *= texture(u_textures[14], Input.texture_coord * Input.tiling_factor); break;
		case 15: texture_color *= texture(u_textures[15], Input.texture_coord * Input.tiling_factor); break;
		case 16: texture_color *= texture(u_textures[16], Input.texture_coord * Input.tiling_factor); break;
		case 17: texture_color *= texture(u_textures[17], Input.texture_coord * Input.tiling_factor); break;
		case 18: texture_color *= texture(u_textures[18], Input.texture_coord * Input.tiling_factor); break;
		case 19: texture_color *= texture(u_textures[19], Input.texture_coord * Input.tiling_factor); break;
		case 20: texture_color *= texture(u_textures[20], Input.texture_coord * Input.tiling_factor); break;
		case 21: texture_color *= texture(u_textures[21], Input.texture_coord * Input.tiling_factor); break;
		case 22: texture_color *= texture(u_textures[22], Input.texture_coord * Input.tiling_factor); break;
		case 23: texture_color *= texture(u_textures[23], Input.texture_coord * Input.tiling_factor); break;
		case 24: texture_color *= texture(u_textures[24], Input.texture_coord * Input.tiling_factor); break;
		case 25: texture_color *= texture(u_textures[25], Input.texture_coord * Input.tiling_factor); break;
		case 26: texture_color *= texture(u_textures[26], Input.texture_coord * Input.tiling_factor); break;
		case 27: texture_color *= texture(u_textures[27], Input.texture_coord * Input.tiling_factor); break;
		case 28: texture_color *= texture(u_textures[28], Input.texture_coord * Input.tiling_factor); break;
		case 29: texture_color *= texture(u_textures[29], Input.texture_coord * Input.tiling_factor); break;
		case 30: texture_color *= texture(u_textures[30], Input.texture_coord * Input.tiling_factor); break;
		case 31: texture_color *= texture(u_textures[31], Input.texture_coord * Input.tiling_factor); break;
	}
	o_color = texture_color;
	o_entity_id = v_entityID;
}
