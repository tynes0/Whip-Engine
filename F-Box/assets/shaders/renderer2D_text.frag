#version 450 core

layout(location = 0) out vec4 o_color;
layout(location = 1) out int o_entityID;

layout (location = 0) in vec4 v_color;
layout (location = 1) in vec2 v_texture_coord;
layout (location = 2) in flat int v_entityID;

layout (binding = 0) uniform sampler2D u_font_atlas;

float screen_px_range() 
{
	const float px_range = 2.0; // set to distance field's pixel range
    vec2 unit_range = vec2(px_range) / vec2(textureSize(u_font_atlas, 0));
    vec2 screen_text_size = vec2(1.0) / max(fwidth(v_texture_coord), vec2(0.0001));
    return max(0.5 * dot(unit_range, screen_text_size), 1.0);
}

float median(float r, float g, float b) 
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
	vec4 sampled_color = v_color * texture(u_font_atlas, v_texture_coord);

	vec3 msd = texture(u_font_atlas, v_texture_coord).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screen_px_distance = screen_px_range() * (sd - 0.5);
    float opacity = clamp(screen_px_distance + 0.5, 0.0, 1.0);

	if (opacity == 0.0) {
		o_color = vec4(0.0);
		o_entityID = -1;
		discard;
	}

	vec4 bg_color = vec4(0.0);
    o_color = mix(bg_color, v_color, opacity);

	if (o_color.a == 0.0) {
		o_entityID = -1;
		discard;
	}

	o_entityID = v_entityID;
}
