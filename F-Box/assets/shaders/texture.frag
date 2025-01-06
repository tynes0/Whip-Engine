#version 450

layout(location = 0) out vec4 color;

in vec4 v_color;
in vec2 v_texture_coord;
in flat float v_texture_index;
in float v_tiling_factor;

uniform sampler2D u_textures[32];

void main()
{
	vec4 texture_color = v_color;
	switch(int(v_texture_index))
	{
		case  0: texture_color *= texture(u_textures[ 0], v_texture_coord * v_tiling_factor); break;
		case  1: texture_color *= texture(u_textures[ 1], v_texture_coord * v_tiling_factor); break;
		case  2: texture_color *= texture(u_textures[ 2], v_texture_coord * v_tiling_factor); break;
		case  3: texture_color *= texture(u_textures[ 3], v_texture_coord * v_tiling_factor); break;
		case  4: texture_color *= texture(u_textures[ 4], v_texture_coord * v_tiling_factor); break;
		case  5: texture_color *= texture(u_textures[ 5], v_texture_coord * v_tiling_factor); break;
		case  6: texture_color *= texture(u_textures[ 6], v_texture_coord * v_tiling_factor); break;
		case  7: texture_color *= texture(u_textures[ 7], v_texture_coord * v_tiling_factor); break;
		case  8: texture_color *= texture(u_textures[ 8], v_texture_coord * v_tiling_factor); break;
		case  9: texture_color *= texture(u_textures[ 9], v_texture_coord * v_tiling_factor); break;
		case 10: texture_color *= texture(u_textures[10], v_texture_coord * v_tiling_factor); break;
		case 11: texture_color *= texture(u_textures[11], v_texture_coord * v_tiling_factor); break;
		case 12: texture_color *= texture(u_textures[12], v_texture_coord * v_tiling_factor); break;
		case 13: texture_color *= texture(u_textures[13], v_texture_coord * v_tiling_factor); break;
		case 14: texture_color *= texture(u_textures[14], v_texture_coord * v_tiling_factor); break;
		case 15: texture_color *= texture(u_textures[15], v_texture_coord * v_tiling_factor); break;
		case 16: texture_color *= texture(u_textures[16], v_texture_coord * v_tiling_factor); break;
		case 17: texture_color *= texture(u_textures[17], v_texture_coord * v_tiling_factor); break;
		case 18: texture_color *= texture(u_textures[18], v_texture_coord * v_tiling_factor); break;
		case 19: texture_color *= texture(u_textures[19], v_texture_coord * v_tiling_factor); break;
		case 20: texture_color *= texture(u_textures[20], v_texture_coord * v_tiling_factor); break;
		case 21: texture_color *= texture(u_textures[21], v_texture_coord * v_tiling_factor); break;
		case 22: texture_color *= texture(u_textures[22], v_texture_coord * v_tiling_factor); break;
		case 23: texture_color *= texture(u_textures[23], v_texture_coord * v_tiling_factor); break;
		case 24: texture_color *= texture(u_textures[24], v_texture_coord * v_tiling_factor); break;
		case 25: texture_color *= texture(u_textures[25], v_texture_coord * v_tiling_factor); break;
		case 26: texture_color *= texture(u_textures[26], v_texture_coord * v_tiling_factor); break;
		case 27: texture_color *= texture(u_textures[27], v_texture_coord * v_tiling_factor); break;
		case 28: texture_color *= texture(u_textures[28], v_texture_coord * v_tiling_factor); break;
		case 29: texture_color *= texture(u_textures[29], v_texture_coord * v_tiling_factor); break;
		case 30: texture_color *= texture(u_textures[30], v_texture_coord * v_tiling_factor); break;
		case 31: texture_color *= texture(u_textures[31], v_texture_coord * v_tiling_factor); break;
	}
	color = texture_color;
}