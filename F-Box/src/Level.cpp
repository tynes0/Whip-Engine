#include "Level.h"

#include <glm/gtc/matrix_transform.hpp>

static glm::vec4 HSVtoRGB(const glm::vec3& hsv)
{
	int H = (int)(hsv.x * 360.0f);
	double S = hsv.y;
	double V = hsv.z;

	double C = S * V;
	double X = C * (1 - whip::wabs(fmod(H / 60.0, 2) - 1));
	double m = V - C;
	double Rs, Gs, Bs;

	if (H >= 0 && H < 60)
	{
		Rs = C;
		Gs = X;
		Bs = 0;
	}

	else if (H >= 60 && H < 120)
	{
		Rs = X;
		Gs = C;
		Bs = 0;
	}

	else if (H >= 120 && H < 180)
	{
		Rs = 0;
		Gs = C;
		Bs = X;
	}

	else if (H >= 180 && H < 240)
	{
		Rs = 0;
		Gs = X;
		Bs = C;
	}

	else if (H >= 240 && H < 300)
	{
		Rs = X;
		Gs = 0;
		Bs = C;
	}

	else
	{
		Rs = C;
		Gs = 0;
		Bs = X;
	}

	return { (Rs + m), (Gs + m), (Bs + m), 1.0f };
}

static bool point_in_triangle(const glm::vec2& p, const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2)
{
	float s = p0.y * p2.x - p0.x * p2.y + (p2.y - p0.y) * p.x + (p0.x - p2.x) * p.y;
	float t = p0.x * p1.y - p0.y * p1.x + (p0.y - p1.y) * p.x + (p1.x - p0.x) * p.y;

	if ((s < 0) != (t < 0))
		return false;

	float A = -p1.y * p2.x + p0.y * (p2.x - p1.x) + p0.x * (p1.y - p2.y) + p1.x * p2.y;

	return A < 0 ?
		(s <= 0 && s + t >= A) :
		(s >= 0 && s + t <= A);
}

void level::init()
{
	m_triangle_texture = whip::texture2D::create("C:\\Dev\\Whip\\F-Box\\assets\\textures\\new_triangle.png");
	m_player.load_assets();

	m_pillars.resize(5);
	for (int i = 0; i < 5; ++i)
	{
		create_pillar(i, i * 10.0f);
	}
}

void level::on_update(whip::timestep ts)
{
	m_player.on_update(ts);

	if (collision_test())
	{
		game_over();
		return;
	}

	m_pillar_HSV.x += 0.1f * ts;
	if (m_pillar_HSV.x > 1.0f)
	{
		m_pillar_HSV.x = 0.0f;
	}
	
	if (m_player.get_position().x > m_pillar_target)
	{
		create_pillar(m_pillar_index, m_pillar_target + 20.0f);
		m_pillar_index = ++m_pillar_index % m_pillars.size();
		m_pillar_target += 10.0f;
	}
}

void level::on_render()
{
	const auto& player_position = m_player.get_position();

	glm::vec4 x_color = HSVtoRGB(m_pillar_HSV);

	//background

	whip::renderer2D::draw_quad({ player_position.x, 0.0f, -0.8f }, { 50.0f, 50.0f }, { 0.3f, 0.3f, 0.3f, 1.0f });

	// floor and ceiling

	whip::renderer2D::draw_quad({ player_position.x,  34.0f }, { 50.0f, 50.0f }, x_color);
	whip::renderer2D::draw_quad({ player_position.x, -34.0f }, { 50.0f, 50.0f }, x_color);

	for (auto& x_pillar : m_pillars)
	{
		whip::renderer2D::draw_rotated_quad(x_pillar.top_position, x_pillar.top_scale, glm::radians(180.0f), m_triangle_texture, 1.0f, x_color);
		whip::renderer2D::draw_rotated_quad(x_pillar.bottom_position, x_pillar.bottom_scale, 0.0f, m_triangle_texture, 1.0f, x_color);
	}

	m_player.on_render();
}

void level::on_imgui_render()
{
	m_player.on_imgui_render();
}

void level::reset()
{
	m_game_over = false;

	m_player.reset();

	m_pillar_target = 30.0f;
	m_pillar_index = 0;
	for (int i = 0; i < 5; ++i)
		create_pillar(i, i * 10.0f);
}

void level::create_pillar(int index, float offset)
{
	pillar& x_pillar = m_pillars[index];
	x_pillar.top_position.x = offset;
	x_pillar.bottom_position.x = offset;

	x_pillar.top_position.z = index * 0.1f - 0.5f;
	x_pillar.bottom_position.z = index * 0.1f - 0.5f + 0.05f;

	float center = random::rfloat() * 35.0f - 17.5f;
	float gap = 2.0f + random::rfloat() * 5.0f;

	x_pillar.top_position.y = 10.0f - ((10.0f - center) * 0.2f) + gap * 0.5f;
	x_pillar.bottom_position.y = -10.0f - ((-10.0f - center) * 0.2f) - gap * 0.5f;
}

bool level::collision_test()
{
	if (glm::abs(m_player.get_position().y) > 8.5f)
		return true;

	glm::vec4 player_vertices[4] =
	{
		{ -0.5f, -0.5f, 0.0f, 1.0f },
		{  0.5f, -0.5f, 0.0f, 1.0f },
		{  0.5f,  0.5f, 0.0f, 1.0f },
		{ -0.5f,  0.5f, 0.0f, 1.0f }
	};

	const auto& pos = m_player.get_position();
	glm::vec4 player_transformed_verts[4];

	for (int i = 0; i < 4; ++i)
	{
		player_transformed_verts[i] = glm::translate(glm::mat4(1.0f), { pos.x, pos.y, 0.0f }) 
			* glm::rotate(glm::mat4(1.0f), glm::radians(m_player.get_rotation()), { 0.0f, 0.0f, 1.0f }) 
			* glm::scale(glm::mat4(1.0f), { 1.0f, 1.3f, 1.0f }) * player_vertices[i];
	}

	glm::vec4 pillar_vertices[3] =
	{
		{ -0.5f + 0.1f, -0.5f + 0.1f, 0.0f, 1.0f },
		{  0.5f - 0.1f, -0.5f + 0.1f, 0.0f, 1.0f },
		{  0.0f + 0.0f,  0.5f - 0.1f, 0.0f, 1.0f },
	};

	for (auto& p : m_pillars)
	{
		glm::vec2 triangle[3];

		// top pillars
		for (int i = 0; i < 3; ++i)
		{
			triangle[i] = glm::translate(glm::mat4(1.0f), { p.top_position.x, p.top_position.y, 0.0f })
				* glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), { 0.0f, 0.0f, 1.0f })
				* glm::scale(glm::mat4(1.0f), { p.top_scale.x, p.top_scale.y, 1.0f }) * pillar_vertices[i];
		}

		for (auto& vert : player_transformed_verts)
		{
			if (point_in_triangle({ vert.x, vert.y }, triangle[0], triangle[1], triangle[2]))
				return true;
		}

		// bottom pillars

		for (int i = 0; i < 3; ++i)
		{
			triangle[i] = glm::translate(glm::mat4(1.0f), { p.bottom_position.x, p.bottom_position.y, 0.0f })
				* glm::scale(glm::mat4(1.0f), { p.bottom_scale.x, p.bottom_scale.y, 1.0f }) * pillar_vertices[i];
		}

		for (auto& vert : player_transformed_verts)
		{
			if (point_in_triangle({ vert.x, vert.y }, triangle[0], triangle[1], triangle[2]))
				return true;
		}
	}

	return false;
}

void level::game_over()
{
	m_game_over = true;
}
