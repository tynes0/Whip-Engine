#include "character.h"

static size_t id_counter = 0;

character::character() : id(whip::hash<size_t>{}(id_counter++)), moving(false), attacking(false), changed(false),
	m_position(glm::vec3(0)), m_default_position(glm::vec3(0)), m_still_frames(), m_attack_frames(), m_default_movement_frames(), m_changed_movement_frames()
{
}

character::character(const glm::vec3& default_position) : id(whip::hash<size_t>{}(id_counter++)), moving(false), attacking(false), changed(false),
m_position(glm::vec3(0)), m_default_position(default_position), m_still_frames(), m_attack_frames(), m_default_movement_frames(), m_changed_movement_frames()
{
}

character::character(const std::initializer_list<std::string>& still_frames_filepaths, const std::initializer_list<std::string>& attack_frames_filepaths, 
	const std::initializer_list<std::string>& default_movement_frames_filepaths, const std::initializer_list<std::string>& changed_movement_frames_filepaths)
	: id(whip::hash<size_t>{}(id_counter++)), moving(false), attacking(false), changed(false), m_position(glm::vec3(0)), m_default_position(glm::vec3(0))
{
	for (const auto& filepath : still_frames_filepaths)
		m_still_frames.push_back(whip::texture2D::create(filepath));
	for (const auto& filepath : attack_frames_filepaths)
		m_attack_frames.push_back(whip::texture2D::create(filepath));
	for (const auto& filepath : default_movement_frames_filepaths)
		m_default_movement_frames.push_back(whip::texture2D::create(filepath));
	for (const auto& filepath : changed_movement_frames_filepaths)
		m_changed_movement_frames.push_back(whip::texture2D::create(filepath));
}

character::character(const glm::vec3& default_position, const std::initializer_list<std::string>& still_frames_filepaths, const std::initializer_list<std::string>& attack_frames_filepaths,
	const std::initializer_list<std::string>& default_movement_frames_filepaths, const std::initializer_list<std::string>& changed_movement_frames_filepaths)
	: id(whip::hash<size_t>{}(id_counter++)), moving(false), attacking(false), changed(false), m_position(glm::vec3(0)), m_default_position(default_position)
{
	for (const auto& filepath : still_frames_filepaths)
		m_still_frames.push_back(whip::texture2D::create(filepath));
	for (const auto& filepath : attack_frames_filepaths)
		m_attack_frames.push_back(whip::texture2D::create(filepath));
	for (const auto& filepath : default_movement_frames_filepaths)
		m_default_movement_frames.push_back(whip::texture2D::create(filepath));
	for (const auto& filepath : changed_movement_frames_filepaths)
		m_changed_movement_frames.push_back(whip::texture2D::create(filepath));
}

void character::display_default_quad() const
{
	glm::vec4 default_quad = { 0.5f, 0.5f, 0.5f, 1.0f };
	whip::renderer2D::draw_quad(m_position, m_scale, default_quad);
}

// this should be between begin and end scenes
void character::display(whip::timestep ts)
{
	last_ts = ts;
	if (!moving && !attacking)
	{
		if (m_still_frames.size() != 0)
		{
			whip::renderer2D::draw_quad(m_position, m_scale, m_still_frames[idxs[0]]);
			if (delays[0]-- <= 0)
			{
				delays[0] = delay_divider / ts;
				idxs[0]++;
				if (idxs[0] >= m_still_frames.size())
					idxs[0] = 0;
			}
		}
		else
			display_default_quad();
	}
	else if (!moving && attacking)
	{
		if (m_attack_frames.size() != 0)
		{
			whip::renderer2D::draw_quad(m_position, m_scale, m_attack_frames[idxs[1]]);
			if (delays[1]-- <= 0)
			{
				delays[1] = delay_divider / ts;
				idxs[1]++;
				if (idxs[1] >= m_attack_frames.size())
					idxs[1] = 0;
			}
		}
		else
			display_default_quad();
	}
	else if (moving && !changed)
	{
		if (m_default_movement_frames.size() != 0)
		{
			whip::renderer2D::draw_quad(m_position, m_scale, m_default_movement_frames[idxs[2]]);
			if (delays[2]-- <= 0)
			{
				delays[2] = delay_divider / ts;
				idxs[2]++;
				if (idxs[2] >= m_default_movement_frames.size())
					idxs[2] = 0;
			}
		}
		else
			display_default_quad();
	}
	else if (moving && changed)
	{
		if (m_changed_movement_frames.size() != 0)
		{
			whip::renderer2D::draw_quad(m_position, m_scale, m_changed_movement_frames[idxs[3]]);
			if (delays[3]-- <= 0)
			{
				delays[3] = delay_divider / ts;
				idxs[3]++;
				if (idxs[3] >= m_changed_movement_frames.size())
					idxs[3] = 0;
			}
		}
		else
			display_default_quad();
	}
}

size_t character::get_id() const
{
	return id;
}

bool character::is_moving() const
{
	return moving;
}

bool character::is_attacking() const
{
	return attacking;
}

bool character::is_changed() const
{
	return changed;
}

float character::get_speed() const
{
	return speed;
}

float character::get_delay_divider() const
{
	return delay_divider;
}

glm::vec3 character::get_position() const
{
	return m_position;
}

glm::vec3 character::get_default_position() const
{
	return m_default_position;
}

glm::vec2 character::get_scale() const
{
	return m_scale;
}

character::rotation character::get_last_x_face() const
{
	return m_faces.first;
}

character::rotation character::get_last_y_face() const
{
	return m_faces.second;
}

whip::vector<whip::ref<whip::texture2D>> character::get_still_frames() const
{
	return m_still_frames;
}

whip::vector<whip::ref<whip::texture2D>> character::get_attack_frames() const
{
	return m_attack_frames;
}

whip::vector<whip::ref<whip::texture2D>> character::get_default_movement_frames() const
{
	return m_default_movement_frames;
}

whip::vector<whip::ref<whip::texture2D>> character::get_changed_movement_frames() const
{
	return m_changed_movement_frames;
}

void character::set_speed(float new_speed)
{
	speed = new_speed;
}

void character::set_x_face(rotation rot)
{
	switch (rot)
	{
	case character::rotation::left:
	case character::rotation::right:
		m_faces.first = rot;
		break;
	case character::rotation::up:
	case character::rotation::down:
	case character::rotation::none:
	default:
		break;
	}
}

void character::set_y_face(rotation rot)
{
	switch (rot)
	{
	case character::rotation::up:
	case character::rotation::down:
		m_faces.second = rot;
		break;
	case character::rotation::left:
	case character::rotation::right:
	case character::rotation::none:
	default:
		break;
	}
}

void character::set_movement_state(bool move)
{
	if (!changed && move) changed = true;
	moving = move;
}

void character::set_attack_state(bool attack)
{
	if (moving && attack) moving = false;
	attacking = attack;
}

void character::set_change_state(bool change)
{
	if (!moving && change) moving = true;
	changed = change;
}

void character::set_delay_divider(float divider)
{
	delay_divider = divider;
}

void character::set_position(const glm::vec3& position)
{
	m_position = position;
}

void character::set_default_position(const glm::vec3& position)
{
	m_default_position = position;
}

void character::set_x_position(float x)
{
	m_position.x = x;
}

void character::set_y_position(float y)
{
	m_position.y = y;
}

void character::set_z_position(float z)
{
	m_position.z = z;
}

void character::set_scale(const glm::vec2& scale)
{
	m_scale = scale;
}

void character::set_x_scale(float x)
{
	m_scale.x = x;
}

void character::set_y_scale(float y)
{
	m_scale.y = y;
}

size_t character::add_still_frame(const std::string& filepath)
{
	m_still_frames.push_back(whip::texture2D::create(filepath));
	return m_still_frames.size() - 1;
}

size_t character::add_attack_frame(const std::string& filepath)
{
	m_attack_frames.push_back(whip::texture2D::create(filepath));
	return m_attack_frames.size() - 1;
}

size_t character::add_default_movement_frame(const std::string& filepath)
{
	m_default_movement_frames.push_back(whip::texture2D::create(filepath));
	return m_default_movement_frames.size() - 1;
}

size_t character::add_changed_movement_frame(const std::string& filepath)
{
	m_changed_movement_frames.push_back(whip::texture2D::create(filepath));
	return m_changed_movement_frames.size() - 1;
}

void character::move(rotation rot)
{
	switch (rot)
	{
	case character::rotation::left:
		m_position.x -= speed * last_ts;
		m_faces.first = rotation::left;
		break;
	case character::rotation::right:
		m_position.x += speed * last_ts;
		m_faces.first = rotation::right;
		break;
	case character::rotation::up:
		m_position.y += speed * last_ts;
		m_faces.second = rotation::up;
		break;
	case character::rotation::down:
		m_position.y -= speed * last_ts;
		m_faces.second = rotation::down;
		break;
	case character::rotation::none:
	default:
		break;
	}
}

void character::move_x()
{
	switch (m_faces.first)
	{
	case character::rotation::left:
		m_position.x -= speed * last_ts;
		break;
	case character::rotation::right:
		m_position.x += speed * last_ts;
		break;
	case character::rotation::none:
	default:
		break;
	}
}

void character::move_y()
{
	switch (m_faces.second)
	{
	case character::rotation::up:
		m_position.y += speed * last_ts;
		break;
	case character::rotation::down:
		m_position.y -= speed * last_ts;
		break;
	case character::rotation::none:
	default:
		break;
	}
}