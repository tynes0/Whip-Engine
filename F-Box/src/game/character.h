#pragma once

#include <Whip.h>

class character
{
public:
	enum class rotation
	{
		none,
		left,
		right,
		up,
		down
	};

	character();
	character(const glm::vec3& position);
	character(const std::initializer_list<std::string>& still_frames_filepaths, const std::initializer_list<std::string>& attack_frames_filepaths, 
		const std::initializer_list<std::string>& default_movement_frames_filepaths, const std::initializer_list<std::string>& changed_movement_frames_filepaths = {});
	character(const glm::vec3& position, const std::initializer_list<std::string>& still_frames_filepaths, const std::initializer_list<std::string>& attack_frames_filepaths,
		const std::initializer_list<std::string>& default_movement_frames_filepaths, const std::initializer_list<std::string>& changed_movement_frames_filepaths = {});
private:
	void display_default_quad() const;
public:
	void display(whip::timestep ts);

	size_t get_id() const;
	bool is_moving() const;
	bool is_attacking() const;
	bool is_changed() const;
	float get_speed() const;
	float get_delay_divider() const;
	glm::vec3 get_position() const;
	glm::vec3 get_default_position() const;
	glm::vec2 get_scale() const;
	rotation get_last_x_face() const;
	rotation get_last_y_face() const;
	whip::vector<whip::ref<whip::texture2D>> get_still_frames() const;
	whip::vector<whip::ref<whip::texture2D>> get_attack_frames() const;
	whip::vector<whip::ref<whip::texture2D>> get_default_movement_frames() const;
	whip::vector<whip::ref<whip::texture2D>> get_changed_movement_frames() const;

	void set_speed(float new_speed);
	void set_x_face(rotation rot);
	void set_y_face(rotation rot);
	void set_movement_state(bool move);
	void set_attack_state(bool attack);
	void set_change_state(bool change);
	void set_delay_divider(float divider);
	void set_position(const glm::vec3& position);
	void set_default_position(const glm::vec3& position);
	void set_x_position(float x);
	void set_y_position(float y);
	void set_z_position(float z);
	void set_scale(const glm::vec2& scale);
	void set_x_scale(float x);
	void set_y_scale(float y);
	size_t add_still_frame(const std::string& filepath);
	size_t add_attack_frame(const std::string& filepath);
	size_t add_default_movement_frame(const std::string& filepath);
	size_t add_changed_movement_frame(const std::string& filepath);

	void move(rotation rot);
	void move_x();
	void move_y();
private:
	float last_ts = 0.0f;
	size_t id;
	bool moving;
	bool attacking;
	bool changed;
	float delay_divider = 0.1f;
	float speed = 0.0f;
	glm::vec3 m_position;
	glm::vec3 m_default_position;
	glm::vec2 m_scale = { 1.0f, 1.0f };
	whip::array<float, 4> delays{ 0,0,0,0 };
	whip::array<size_t, 4> idxs{ 0,0,0,0 };
	whip::pair<rotation> m_faces{ rotation::none, rotation::none };
	whip::vector<whip::ref<whip::texture2D>> m_still_frames;
	whip::vector<whip::ref<whip::texture2D>> m_attack_frames;
	whip::vector<whip::ref<whip::texture2D>> m_default_movement_frames;
	whip::vector<whip::ref<whip::texture2D>> m_changed_movement_frames;
};