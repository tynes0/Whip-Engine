#include "whippch.h"
#include "script_glue.h"
#include "script_engine.h"

#include <Whip/Core/UUID.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/Input.h>
#include <Whip/Core/timer_manager.h>

#include <Whip/Scene/entity.h>
#include <Whip/Scene/components.h>
#include <Whip/Scene/scene.h>

#include <Whip/Physics/contact_listener.h>
#include <Whip/Physics/physics2D.h>

#include <Whip/Project/project.h>

#include <Whip/Asset/asset_importer.h>
#include <Whip/Asset/asset_manager.h>

#include <Whip/Audio/audio_engine.h>
#include <Whip/Animation/animation_manager.h>

#include <cstring>

#include <glm/glm.hpp>

#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

#include <box2d/b2_body.h>

_WHIP_START

#define ADD_INTERNAL_CALL(name, func) mono_add_internal_call(WHP_CONCATENATE("Whip.InternalCalls::", WHP_STRINGIZE(name)), func)

static constexpr const char* runtime_timers_group_name = "--###RUNTIME-TIMER-TRACKER###--";
static std::unordered_map<MonoType*, std::function<bool(entity)>> s_entity_has_component_funcs;
static logger s_logger;

namespace utils
{
	namespace detail
	{
		static scene* get_scene()
		{
			scene* scne = script_engine::get_scene_context(); 
			WHP_CORE_ASSERT(scne);
			return scne;
		}

		static entity get_entity(UUID id)
		{
			scene* scne = get_scene();
			entity ent = scne->find_entity_by_UUID(id); 
			WHP_CORE_ASSERT(ent);
			return ent;
		}

		static std::string mono_string_to_string(MonoString* string)
		{
			char* cstr = mono_string_to_utf8(string);
			std::string str(cstr);
			mono_free(cstr);
			return str;
		}

		static std::wstring mono_string_to_wstring(MonoString* string)
		{
			wchar_t* cstr = (wchar_t*)mono_string_to_utf16(string);
			std::wstring wstr(cstr);
			mono_free(cstr);
			return wstr;
		}

		static audio_component::audio_data* find_ac_AD(std::vector<audio_component::audio_data>& handle_list, UUID32 id)
		{
			for (audio_component::audio_data& handle : handle_list)
				if (handle.ID == id)
					return &handle;
			return nullptr;
		}

		static audio_component::audio_data* find_ac_AD(std::vector<audio_component::audio_data>& handle_list, const std::string& tag)
		{
			for (audio_component::audio_data& handle : handle_list)
				if (handle.tag == tag)
					return &handle;
			return nullptr;
		}

		static b2Body* get_body(UUID id)
		{
			entity ent = get_entity(id);
			auto& rb2d = ent.get_component<rigidbody2D_component>();
			b2Body* body = (b2Body*)rb2d.runtime_body;
			WHP_CORE_ASSERT(body);
			return body;
		}

		static audio_component::audio_data* get_audio_data(UUID id, UUID32 ad_id)
		{
			entity ent = get_entity(id);
			audio_component& ac = ent.get_component<audio_component>(); 
			audio_component::audio_data* AD = detail::find_ac_AD(ac.audio_datas, ad_id);
			return AD;
		}

		static audio_component::audio_data* get_audio_data(UUID id, const std::string& tag)
		{
			entity ent = get_entity(id);
			audio_component& ac = ent.get_component<audio_component>();
			audio_component::audio_data* AD = detail::find_ac_AD(ac.audio_datas, tag);
			return AD;
		}

		static ref<animation2D> get_animation(UUID handle)
		{
			return std::static_pointer_cast<animation2D>(project::get_active()->get_runtime_asset_manager()->get_asset(handle));
		}
	}

	static MonoObject* get_script_instance(UUID entityID)
	{
		return script_engine::get_managed_instance(entityID);
	}

	static bool entity_has_component(UUID entityID, MonoReflectionType* component_type)
	{
		entity ent = detail::get_entity(entityID);

		MonoType* managed_type = mono_reflection_type_get_type(component_type);
		WHP_CORE_ASSERT(s_entity_has_component_funcs.find(managed_type) != s_entity_has_component_funcs.end());
		return s_entity_has_component_funcs.at(managed_type)(ent);
	}

	static uint64_t entity_find_entity_by_name(MonoString* name)
	{
		scene* scne = detail::get_scene();
		entity ent = scne->find_entity_by_name(detail::mono_string_to_string(name));
		if (!ent)
			return 0;
		return ent.get_UUID();
	}

	static void logger_internal_log(MonoString* log_message, log::level level)
	{
		log::get_core_logger()->log(log::whip_log_level_to_spdlog_level(level), detail::mono_string_to_string(log_message));
	}

	static void logger_internal_assert(bool cond, MonoString* log_message, MonoString* filepath, int line)
	{
		if (!(cond)) 
		{ 
			WHP_CORE_CRITICAL(
				"Whip Assertion failed! File: {0}, Line: {1}, Message: {2}", 
				std::filesystem::path(detail::mono_string_to_string(filepath)).filename().string(),
				line,
				detail::mono_string_to_string(log_message));
			WHP_DEBUGBREAK();
		}
	}

	static void logger_set_logger(MonoString* logger_name)
	{
		log::reset_logger(s_logger, detail::mono_string_to_string(logger_name), log::output_target::editor);
	}

	static void logger_print_log(MonoString* log_message, log::level level)
	{
		s_logger->log(log::whip_log_level_to_spdlog_level(level), detail::mono_string_to_string(log_message));
		editor_log::file_should_reset().store(true);
	}

	static void logger_print_log_named(MonoString* logger_name, MonoString* log_message, log::level level)
	{
		std::string name = s_logger->name();
		logger_set_logger(logger_name);
		logger_print_log(log_message, level);
		log::reset_logger(s_logger, name);
	}

	static bool timer_wait_for(UUID tag, float ms)
	{
		timer_id id = 0;
		bool result = timer_manager::get().wait_for(tag, ms, 0, &id);
		if (id != 0)
			timer_manager::get().get_group_map().get(runtime_timers_group_name).add(id);
		return result;
	}

	static uint64_t timer_set_timeout(MonoObject* func, float delay_ms, MonoObject* user_data)
	{
		if (!func)
		{
			WHP_CORE_ERROR("[Script Engine] Function object is null!");
			return 0;
		}
		MonoClass* klass = mono_object_get_class(func);

		if (!klass)
		{
			WHP_CORE_ERROR("[Script Engine] Failed to get class from object!");
			return 0;
		}
		MonoMethod* method = mono_class_get_method_from_name(klass, "Invoke", -1);

		if (!method)
		{
			WHP_CORE_ERROR("[Script Engine] Failed to get the method from delegate.");
			return 0;
		}

		timer_id id = timer_manager::get().set_timeout([func, method](void* u_data)
			{
				void* args[] = { u_data };
				script_class::invoke_method(func, method, args);
			}, delay_ms, static_cast<void*>(user_data));
		timer_manager::get().get_group_map().get(runtime_timers_group_name).add(id);
		return id;
	}

	static uint64_t timer_set_interval(MonoObject* func, float interval_ms, MonoObject* user_data)
	{
		if (!func)
		{
			WHP_CORE_ERROR("[Script Engine] Function object is null!");
			return 0;
		}
		MonoClass* klass = mono_object_get_class(func);

		if (!klass)
		{
			WHP_CORE_ERROR("[Script Engine] Failed to get class from object!");
			return 0;
		}
		MonoMethod* method = mono_class_get_method_from_name(klass, "Invoke", -1);

		if (!method)
		{
			WHP_CORE_ERROR("[Script Engine] Failed to get the method from delegate.");
			return 0;
		}

		timer_id id = timer_manager::get().set_interval([func, method](void* u_data)
			{
				void* args[] = { u_data };
				script_class::invoke_method(func, method, args);
			}, interval_ms, static_cast<void*>(user_data));
		timer_manager::get().get_group_map().get(runtime_timers_group_name).add(id);
		return id;
	}

	static void timer_pause_timer(uint64_t timer_id)
	{
		if(timer_manager::get().get_group_map().get(runtime_timers_group_name).exists(timer_id))
			timer_manager::get().pause_timer(timer_id);
	}

	static void timer_resume_timer(uint64_t timer_id)
	{
		if (timer_manager::get().get_group_map().get(runtime_timers_group_name).exists(timer_id))
			timer_manager::get().resume_timer(timer_id);
	}

	static void timer_stop_timer(uint64_t timer_id)
	{
		if (timer_manager::get().get_group_map().get(runtime_timers_group_name).exists(timer_id))
		{
			timer_manager::get().get_group_map().get(runtime_timers_group_name).remove(timer_id);
			timer_manager::get().stop_timer(timer_id);
		}
	}

	static void timer_clear()
	{
		timer_manager::get().get_group_map().get(runtime_timers_group_name).clear();
	}

	static bool timer_exists(uint64_t id)
	{
		return timer_manager::get().get_group_map().get(runtime_timers_group_name).exists(id);
	}

	static uint64_t asset_manager_import_asset(MonoString* path)
	{
		return project::get_active()->get_runtime_asset_manager()->import_asset(detail::mono_string_to_wstring(path));
	}

	static void asset_manager_delete_asset(asset_handle handle)
	{
		project::get_active()->get_runtime_asset_manager()->delete_asset(handle);
	}

	static bool asset_manager_is_asset_handle_valid(asset_handle handle)
	{
		return project::get_active()->get_runtime_asset_manager()->is_asset_handle_valid(handle);
	}

	static bool asset_manager_is_asset_loaded(asset_handle handle)
	{
		auto man = project::get_active()->get_runtime_asset_manager();
		return man->is_asset_loaded(handle);
	}

	static asset_type asset_manager_get_asset_type(asset_handle handle)
	{
		return project::get_active()->get_runtime_asset_manager()->get_asset_type(handle);
	}

	static MonoString* asset_manager_get_filepath(asset_handle handle)
	{
		auto& path = project::get_active()->get_runtime_asset_manager()->get_filepath(handle);
		return create_string(path.c_str());
	}

	static bool input_is_key_down(key_code keycode)
	{
		return input::is_key_down(keycode);
	}

	static bool input_is_key_up(key_code keycode)
	{
		return input::is_key_up(keycode);
	}

	static bool input_is_key_pressed(key_code keycode)
	{
		return input::is_key_pressed(keycode);
	}

	static bool input_is_key_released(key_code keycode)
	{
		return input::is_key_released(keycode);
	}

	static bool input_is_mouse_button_down(mouse_code button)
	{
		return input::is_mouse_button_down(button);
	}

	static bool input_is_mouse_button_up(mouse_code button)
	{
		return input::is_mouse_button_up(button);
	}

	static bool input_is_mouse_button_pressed(mouse_code button)
	{
		return input::is_mouse_button_pressed(button);
	}

	static bool input_is_mouse_button_released(mouse_code button)
	{
		return input::is_mouse_button_released(button);
	}

	static float input_get_mouse_x()
	{
		return input::get_mouse_X();
	}

	static float input_get_mouse_y()
	{
		return input::get_mouse_Y();
	}

	static void input_get_mouse_position(glm::vec2* position)
	{
		auto pos = input::get_mouse_position();
		position->x = pos.first;
		position->y = pos.second;
	}

	static bool AD_is_valid(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		return bool(AD);
	}

	static uint64_t AD_get_audio_handle(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		return AD ? ((uint64_t)AD->audio) : 0;
	}

	static void AD_set_audio_handle(UUID entityID, UUID32 adID, uint64_t new_handle)
	{
		if (!project::get_active()->get_runtime_asset_manager()->get_asset_registry().exist(asset_type::audio, new_handle))
		{
			WHP_CORE_WARN("[C# Method] The given handle is not bound to a audio!");
			return;
		}
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		AD->audio = new_handle;
	}

	static MonoString* AD_get_tag(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return create_string("");
		}
		return create_string(AD->tag.c_str());
	}

	static void AD_set_tag(UUID entityID, UUID32 adID, MonoString* tag)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		AD->tag = detail::mono_string_to_string(tag);
	}

	static void AD_get_translation(UUID entityID, UUID32 adID, glm::vec3* translation)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		*translation = AD->translation;
	}

	static void AD_set_translation(UUID entityID, UUID32 adID, glm::vec3* translation)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		AD->translation = *translation;
	}

	static bool AD_is_spitial(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return false;
		}
		return AD->spitial;
	}

	static void AD_set_spitial(UUID entityID, UUID32 adID, bool spitial)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		AD->spitial = spitial;
	}

	static bool AD_is_loop(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return false;
		}
		return AD->loop;
	}

	static void AD_set_loop(UUID entityID, UUID32 adID, bool loop)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		AD->loop = loop;
	}

	static float AD_get_gain(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return AD->gain;
	}

	static void AD_set_gain(UUID entityID, UUID32 adID, float gain)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		AD->gain = gain;
	}

	static float AD_get_pitch(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return AD->pitch;
	}

	static void AD_set_pitch(UUID entityID, UUID32 adID, float pitch)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		AD->pitch = pitch;
	}

	static float AD_get_full_clip_length(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return AD->full_clip_length;
	}

	static float AD_get_clip_start(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return AD->clip_start;
	}

	static void AD_set_clip_start(UUID entityID, UUID32 adID, float clip_start)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		AD->clip_start = clip_start;
	}

	static float AD_get_clip_end(UUID entityID, UUID32 adID)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return AD->clip_end;
	}

	static void AD_set_clip_end(UUID entityID, UUID32 adID, float clip_end)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		AD->clip_end = clip_end;
	}

	static void audio_engine_update_position(UUID entityID, UUID32 adID, glm::vec3* position)
	{
		auto* AD = detail::get_audio_data(entityID, adID);
		if (!AD)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		auto aud_asset = asset_manager::get_asset<audio_source>(AD->audio);
		aud_asset->update_spatial_position(position->x, position->y, position->z);
	}

	static uint64_t animation_get_anim_by_tag(MonoString* name)
	{
		uint64_t result = 0;
		std::string str_name = detail::mono_string_to_string(name);
		auto& assets = project::get_active()->get_editor_asset_manager()->get_asset_registry();
		assets.foreach_checked(asset_type::animation, [&result, str_name](const asset_registry::value_type& value)
			{
				auto anim = asset_manager::get_asset<animation2D>(value.first);
				if (anim->get_name() == str_name)
				{
					auto copied_anim = animation2D::copy(anim);
					project::get_active()->get_runtime_asset_manager()->add_asset_copy(anim->handle, copied_anim);
					animation_manager::get().add_animation(copied_anim);
					result = copied_anim->handle;
					return asset_registry::loop_stop;
				}
				return asset_registry::loop_continue;
			});
		return result;
	}

	static bool animation_is_valid(UUID animation_handle)
	{
		return project::get_active()->get_runtime_asset_manager()->get_asset_registry().exist(asset_type::animation, animation_handle);
	}

	static void animation_bound(UUID animation_handle, UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->bound_with_entity(ent);
	}

	static void animation_unbound(UUID animation_handle)
	{
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->unbound_from_entity();
	}

	static void animation_play(UUID animation_handle)
	{
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->play();
	}

	static void animation_stop(UUID animation_handle)
	{
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->stop();
	}

	static void animation_pause(UUID animation_handle)
	{
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->pause();
	}

	static void animation_resume(UUID animation_handle)
	{
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if(!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->resume();
	}

	static bool animation_is_playing(UUID animation_handle)
	{
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return false;
		}
		return anim->is_playing();
	}

	static bool animation_is_paused(UUID animation_handle)
	{
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return false;
		}
		return anim->is_paused();
	}

	static bool animation_is_looping(UUID animation_handle)
	{
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return false;
		}
		return anim->is_looping();
	}

	static void animation_set_loop(UUID animation_handle, bool loop)
	{
		ref<animation2D> anim = detail::get_animation(animation_handle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->set_loop(loop);
	}

	static void camera_component_set_primary(UUID entityID, bool primary)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<camera_component>().primary = primary;
	}

	static bool camera_component_is_primary(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		return ent.get_component<camera_component>().primary;
	}

	static void camera_component_set_fixed_aspect_ratio(UUID entityID, bool fixed_aspect_ratio)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<camera_component>().fixed_aspect_ratio = fixed_aspect_ratio;
	}

	static bool camera_component_is_fixed_aspect_ratio(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		return ent.get_component<camera_component>().fixed_aspect_ratio;
	}

	static void camera_component_set_perspective_vertical_FOV(UUID entityID, float perspective_vertical_FOV)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<camera_component>().camera.set_perspective_vertical_FOV(perspective_vertical_FOV);
	}

	static float camera_component_get_perspective_vertical_FOV(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		return ent.get_component<camera_component>().camera.get_perspective_vertical_FOV();
	}

	static void camera_component_set_perspective_near_clip(UUID entityID, float perspective_near_clip)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<camera_component>().camera.set_perspective_near_clip(perspective_near_clip);
	}

	static float camera_component_get_perspective_near_clip(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		return ent.get_component<camera_component>().camera.get_perspective_near_clip();
	}

	static void camera_component_set_perspective_far_clip(UUID entityID, float perspective_far_clip)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<camera_component>().camera.set_perspective_far_clip(perspective_far_clip);
	}

	static float camera_component_get_perspective_far_clip(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		return ent.get_component<camera_component>().camera.get_perspective_far_clip();
	}

	static void camera_component_set_orthographic_size(UUID entityID, float orthographic_size)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<camera_component>().camera.set_orthographic_size(orthographic_size);
	}

	static float camera_component_get_orthographic_size(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		return ent.get_component<camera_component>().camera.get_orthographic_size();
	}

	static void camera_component_set_orthographic_near_clip(UUID entityID, float orthographic_near_clip)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<camera_component>().camera.set_orthographic_near_clip(orthographic_near_clip);
	}

	static float camera_component_get_orthographic_near_clip(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		return ent.get_component<camera_component>().camera.get_orthographic_near_clip();
	}

	static void camera_component_set_orthographic_far_clip(UUID entityID, float orthographic_far_clip)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<camera_component>().camera.set_orthographic_far_clip(orthographic_far_clip);
	}

	static float camera_component_get_orthographic_far_clip(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		return ent.get_component<camera_component>().camera.get_orthographic_far_clip();
	}

	static void transform_component_get_translation(UUID entityID, glm::vec3* out_translation)
	{
		entity ent = detail::get_entity(entityID);
		*out_translation = ent.get_component<transform_component>().translation;
	}

	static void transform_component_set_translation(UUID entityID, glm::vec3* translation)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<transform_component>().translation = *translation;
	}

	static void transform_component_get_rotation(UUID entityID, glm::vec3* out_rotation)
	{
		entity ent = detail::get_entity(entityID);
		*out_rotation = glm::degrees(ent.get_component<transform_component>().rotation);
	}

	static void transform_component_set_rotation(UUID entityID, glm::vec3* rotation)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<transform_component>().rotation = glm::radians(*rotation);
	}

	static void transform_component_get_scale(UUID entityID, glm::vec3* out_scale)
	{
		entity ent = detail::get_entity(entityID);
		*out_scale = ent.get_component<transform_component>().scale;
	}

	static void transform_component_set_scale(UUID entityID, glm::vec3* scale)
	{
		entity ent = detail::get_entity(entityID);
		ent.get_component<transform_component>().scale = *scale;
	}

	static void rigidbody2D_component_apply_force(UUID entityID, glm::vec2* force, glm::vec2* point, bool wake)
	{
		auto* body = detail::get_body(entityID);
		body->ApplyForce(b2Vec2(force->x, force->y), b2Vec2(point->x, point->y), wake);
	}

	static void rigidbody2D_component_apply_force_to_center(UUID entityID, glm::vec2* force, bool wake)
	{
		auto* body = detail::get_body(entityID);
		body->ApplyForceToCenter(b2Vec2(force->x, force->y), wake);
	}

	static void rigidbody2D_component_apply_linear_impulse(UUID entityID, glm::vec2* impulse, glm::vec2* point, bool wake)
	{
		auto* body = detail::get_body(entityID);
		body->ApplyLinearImpulse(b2Vec2(impulse->x, impulse->y), b2Vec2(point->x, point->y), wake);
	}

	static void rigidbody2D_component_apply_linear_impulse_to_center(UUID entityID, glm::vec2* impulse, bool wake)
	{
		auto* body = detail::get_body(entityID);
		body->ApplyLinearImpulseToCenter(b2Vec2(impulse->x, impulse->y), wake);
	}

	static void rigidbody2D_component_apply_angular_impulse(UUID entityID, float impulse, bool wake)
	{
		auto* body = detail::get_body(entityID);
		body->ApplyAngularImpulse(impulse, wake);
	}

	static void rigidbody2D_component_apply_torque(UUID entityID, float torque, bool wake)
	{
		auto* body = detail::get_body(entityID);
		body->ApplyTorque(torque, wake);
	}

	static void rigidbody2D_component_get_linear_velocity(UUID entityID, glm::vec2* outLinearVelocity)
	{
		auto* body = detail::get_body(entityID);
		const b2Vec2& linearVelocity = body->GetLinearVelocity();
		*outLinearVelocity = glm::vec2(linearVelocity.x, linearVelocity.y);
	}

	static void rigidbody2D_component_set_linear_velocity(UUID entityID, glm::vec2* linearVelocity)
	{
		auto* body = detail::get_body(entityID);
		body->SetLinearVelocity(b2Vec2(linearVelocity->x, linearVelocity->y));
	}

	static float rigidbody2D_component_get_angular_velocity(UUID entityID)
	{
		auto* body = detail::get_body(entityID);
		return body->GetAngularVelocity();
	}

	static void rigidbody2D_component_set_angular_velocity(UUID entityID, float angularVelocity)
	{
		auto* body = detail::get_body(entityID);
		body->SetAngularVelocity(angularVelocity);
	}

	static rigidbody2D_component::body_type rigidbody2D_component_get_type(UUID entityID)
	{
		auto* body = detail::get_body(entityID);
		return physics2D::rigidbody2D_type_from_box2D_body(body->GetType());
	}

	static void rigidbody2D_component_set_type(UUID entityID, rigidbody2D_component::body_type body_type)
	{
		auto* body = detail::get_body(entityID);
		body->SetType(physics2D::rigidbody2D_type_to_box2D_body(body_type));
	}

	static bool rigidbody2D_component_is_fixed_rotation(UUID entityID)
	{
		auto* body = detail::get_body(entityID);
		return body->IsFixedRotation();
	}

	static void rigidbody2D_component_set_fixed_rotation(UUID entityID, bool fixed)
	{
		auto* body = detail::get_body(entityID);
		body->SetFixedRotation(fixed);
	}

	static float rigidbody2D_component_get_gravity_scale(UUID entityID)
	{
		auto* body = detail::get_body(entityID);
		return body->GetGravityScale();
	}

	static void rigidbody2D_component_set_gravity_scale(UUID entityID, float scale)
	{
		auto* body = detail::get_body(entityID);
		body->SetGravityScale(scale);
	}

	static bool rigidbody2D_component_is_enabled(UUID entityID)
	{
		auto* body = detail::get_body(entityID);
		return body->IsEnabled();
	}

	static void rigidbody2D_component_set_enabled(UUID entityID, bool enabled)
	{
		auto* body = detail::get_body(entityID);
		body->SetEnabled(enabled);
	}

	static bool rigidbody2D_component_is_awake(UUID entityID)
	{
		auto* body = detail::get_body(entityID);
		return body->IsAwake();
	}

	static void rigidbody2D_component_set_awake(UUID entityID, bool awake)
	{
		auto* body = detail::get_body(entityID);
		body->SetAwake(awake);
	}

	static float rigidbody2D_component_get_angle(UUID entityID)
	{
		auto* body = detail::get_body(entityID);
		return body->GetAngle();
	}

	static float rigidbody2D_component_get_mass(UUID entityID)
	{
		auto* body = detail::get_body(entityID);
		return body->GetMass();
	}

	static float rigidbody2D_component_get_intertia(UUID entityID)
	{
		auto* body = detail::get_body(entityID);
		return body->GetInertia();
	}

	static void rigidbody2D_component_set_transform(UUID entityID, glm::vec2* position, float angle)
	{
		auto* body = detail::get_body(entityID);
		body->SetTransform(b2Vec2(position->x, position->y), angle);
	}

	static MonoString* text_component_get_data(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& tc = ent.get_component<text_component>();
		return create_string(tc.text_string.c_str());
	}

	static void text_component_set_data(UUID entityID, MonoString* textString)
	{
		entity ent = detail::get_entity(entityID);
		auto& tc = ent.get_component<text_component>();
		tc.text_string = detail::mono_string_to_string(textString);
	}

	static void text_component_get_color(UUID entityID, glm::vec4* color)
	{
		entity ent = detail::get_entity(entityID);
		auto& tc = ent.get_component<text_component>();
		*color = tc.color;
	}

	static void text_component_set_color(UUID entityID, glm::vec4* color)
	{
		entity ent = detail::get_entity(entityID);
		auto& tc = ent.get_component<text_component>();
		tc.color = *color;
	}

	static float text_component_get_kerning(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& tc = ent.get_component<text_component>();
		return tc.kerning;
	}

	static void text_component_set_kerning(UUID entityID, float kerning)
	{
		entity ent = detail::get_entity(entityID);
		auto& tc = ent.get_component<text_component>();
		tc.kerning = kerning;
	}

	static float text_component_get_line_spacing(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& tc = ent.get_component<text_component>();
		return tc.line_spacing;
	}

	static void text_component_set_line_spacing(UUID entityID, float lineSpacing)
	{
		entity ent = detail::get_entity(entityID);
		auto& tc = ent.get_component<text_component>();
		tc.line_spacing = lineSpacing;
	}

	static void box_collider2D_component_get_offset(UUID entityID, glm::vec2* offset)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		*offset = bc2dc.offset;
	}

	static void box_collider2D_component_set_offset(UUID entityID, glm::vec2* offset)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		bc2dc.offset = *offset;
	}

	static void box_collider2D_component_get_size(UUID entityID, glm::vec2* size)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		*size = bc2dc.size;
	}

	static void box_collider2D_component_set_size(UUID entityID, glm::vec2* size)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		bc2dc.size = *size;
	}

	static MonoString* box_collider2D_component_get_tag(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		return create_string(bc2dc.tag.c_str());
	}

	static void box_collider2D_component_set_tag(UUID entityID, MonoString* tag)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		bc2dc.tag = detail::mono_string_to_string(tag);
	}

	static float box_collider2D_component_get_density(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		return bc2dc.density;
	}

	static void box_collider2D_component_set_density(UUID entityID, float density)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		bc2dc.density = density;
	}

	static float box_collider2D_component_get_friction(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		return bc2dc.friction;
	}

	static void box_collider2D_component_set_friction(UUID entityID, float friction)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		bc2dc.friction = friction;
	}

	static float box_collider2D_component_get_restitution(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		return bc2dc.restitution;
	}

	static void box_collider2D_component_set_restitution(UUID entityID, float restitution)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		bc2dc.restitution = restitution;
	}

	static float box_collider2D_component_get_restitution_threshold(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		return bc2dc.restitution_threshold;
	}

	static void box_collider2D_component_set_restitution_threshold(UUID entityID, float restitution_threshold)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		bc2dc.restitution_threshold = restitution_threshold;
	}

	static float box_collider2D_component_is_sensor(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		return bc2dc.sensor;
	}

	static void box_collider2D_component_set_sensor(UUID entityID, bool sensor)
	{
		entity ent = detail::get_entity(entityID);
		auto& bc2dc = ent.get_component<box_collider2D_component>();
		bc2dc.sensor = sensor;
	}

	static void circle_collider2D_component_get_offset(UUID entityID, glm::vec2* offset)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		*offset = cc2dc.offset;
	}

	static void circle_collider2D_component_set_offset(UUID entityID, glm::vec2* offset)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		cc2dc.offset = *offset;
	}

	static float circle_collider2D_component_get_radius(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		return cc2dc.radius;
	}

	static void circle_collider2D_component_set_radius(UUID entityID, float radius)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		cc2dc.radius = radius;
	}

	static MonoString* circle_collider2D_component_get_tag(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		return create_string(cc2dc.tag.c_str());
	}

	static void circle_collider2D_component_set_tag(UUID entityID, MonoString* tag)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		cc2dc.tag = detail::mono_string_to_string(tag);
	}

	static float circle_collider2D_component_get_density(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		return cc2dc.density;
	}

	static void circle_collider2D_component_set_density(UUID entityID, float density)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		cc2dc.density = density;
	}

	static float circle_collider2D_component_get_friction(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		return cc2dc.friction;
	}

	static void circle_collider2D_component_set_friction(UUID entityID, float friction)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		cc2dc.friction = friction;
	}

	static float circle_collider2D_component_get_restitution(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		return cc2dc.restitution;
	}

	static void circle_collider2D_component_set_restitution(UUID entityID, float restitution)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		cc2dc.restitution = restitution;
	}

	static float circle_collider2D_component_get_restitution_threshold(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		return cc2dc.restitution_threshold;
	}

	static void circle_collider2D_component_set_restitution_threshold(UUID entityID, float restitution_threshold)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		cc2dc.restitution_threshold = restitution_threshold;
	}

	static float circle_collider2D_component_is_sensor(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		return cc2dc.sensor;
	}

	static void circle_collider2D_component_set_sensor(UUID entityID, bool sensor)
	{
		entity ent = detail::get_entity(entityID);
		auto& cc2dc = ent.get_component<circle_collider2D_component>();
		cc2dc.sensor = sensor;
	}

	static void sprite_renderer_component_get_color(UUID entityID, glm::vec4* out_color)
	{
		entity ent = detail::get_entity(entityID);
		auto& src = ent.get_component<sprite_renderer_component>();
		*out_color = src.color;
	}

	static void sprite_renderer_component_set_color(UUID entityID, glm::vec4* color)
	{
		entity ent = detail::get_entity(entityID);
		auto& src = ent.get_component<sprite_renderer_component>();
		src.color = *color;
	}

	static float sprite_renderer_component_get_tiling_factor(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& src = ent.get_component<sprite_renderer_component>();
		return src.tiling_factor;
	}

	static void sprite_renderer_component_set_tiling_factor(UUID entityID, float tiling_factor)
	{
		entity ent = detail::get_entity(entityID);
		auto& src = ent.get_component<sprite_renderer_component>();
		src.tiling_factor = tiling_factor;
	}

	static uint64_t sprite_renderer_component_get_texture_handle(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& src = ent.get_component<sprite_renderer_component>();
		return static_cast<uint64_t>(src.texture);
	}

	static void sprite_renderer_component_set_texture_handle(UUID entityID, UUID texture_handle)
	{
		if (!project::get_active()->get_runtime_asset_manager()->get_asset_registry().exist(asset_type::texture2D, texture_handle))
		{
			WHP_CORE_WARN("[C# Method] The given handle is not bound to a texture!");
			return;
		}
		entity ent = detail::get_entity(entityID);
		auto& src = ent.get_component<sprite_renderer_component>();
		src.texture = texture_handle;
	}
	
	static void circle_renderer_component_get_color(UUID entityID, glm::vec4* out_color)
	{
		entity ent = detail::get_entity(entityID);
		auto& crc = ent.get_component<circle_renderer_component>();
		*out_color = crc.color;
	}

	static void circle_renderer_component_set_color(UUID entityID, glm::vec4* color)
	{
		entity ent = detail::get_entity(entityID);
		auto& crc = ent.get_component<circle_renderer_component>();
		crc.color = *color;
	}

	static float circle_renderer_component_get_thickness(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& crc = ent.get_component<circle_renderer_component>();
		return crc.thickness;
	}

	static void circle_renderer_component_set_thickness(UUID entityID, float thickness)
	{
		entity ent = detail::get_entity(entityID);
		auto& crc = ent.get_component<circle_renderer_component>();
		crc.thickness = thickness;
	}

	static float circle_renderer_component_get_fade(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& crc = ent.get_component<circle_renderer_component>();
		return crc.fade;
	}

	static void circle_renderer_component_set_fade(UUID entityID, float fade)
	{
		entity ent = detail::get_entity(entityID);
		auto& crc = ent.get_component<circle_renderer_component>();
		crc.fade = fade;
	}

	static int audio_component_get_AD_count(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& ac = ent.get_component<audio_component>();
		return static_cast<int>(ac.audio_datas.size());
	}

	static uint32_t audio_component_get_AD(UUID entityID, int index)
	{
		entity ent = detail::get_entity(entityID);
		auto& ac = ent.get_component<audio_component>();
		if (index >= ac.audio_datas.size())
			return 0;
		return ac.audio_datas[index].ID;
	}

	static uint32_t audio_component_create_audio_data(UUID entityID)
	{
		entity ent = detail::get_entity(entityID);
		auto& ac = ent.get_component<audio_component>();
		audio_component::audio_data data;
		data.ID = UUID32();
		data.loaded_in_runtime = true;
		ac.audio_datas.push_back(data);
		return data.ID;
	}

	static void audio_component_remove_audio_data(UUID entityID, UUID32 adID)
	{
		entity ent = detail::get_entity(entityID);
		auto& ac = ent.get_component<audio_component>();
		for (size_t i = 0; i < ac.audio_datas.size(); ++i)
		{
			if (ac.audio_datas[i].ID == adID && ac.audio_datas[i].loaded_in_runtime)
			{
				ac.audio_datas.erase(ac.audio_datas.begin() + i);
				break;
			}
		}
	}

	template<class... Component>
	static void register_component()
	{
		([]()
			{
				std::string managed_type_name = std::string("Whip.") + Component::script_struct_name;

				MonoType* managed_type = mono_reflection_type_from_name(managed_type_name.data(), assembly_manager::get_core_assembly_image());
				if (!managed_type)
				{
					WHP_CORE_ERROR("[Script Engine] Could not find component type {0}", managed_type_name);
					return;
				}
				s_entity_has_component_funcs[managed_type] = [](entity ent) { return ent.has_component<Component>(); };
			}(), ...);
	}

	template<class... Component>
	static void register_component(component_group<Component...>)
	{
		register_component<Component...>();
	}
}

void script_glue::register_components()
{
	s_entity_has_component_funcs.clear();
	utils::register_component(all_components_no_id_no_tag_no_script{});
}

void script_glue::register_functions()
{
	// entity
	ADD_INTERNAL_CALL(GetScriptInstance, utils::get_script_instance);
	ADD_INTERNAL_CALL(Entity_HasComponent, utils::entity_has_component);
	ADD_INTERNAL_CALL(Entity_FindEntityByName, utils::entity_find_entity_by_name);

	// logger
	ADD_INTERNAL_CALL(Logger_InternalLog, utils::logger_internal_log);
	ADD_INTERNAL_CALL(Logger_InternalAssert, utils::logger_internal_assert);
	ADD_INTERNAL_CALL(Logger_SetLogger, utils::logger_set_logger);
	ADD_INTERNAL_CALL(Logger_PrintLog, utils::logger_print_log);
	ADD_INTERNAL_CALL(Logger_PrintLogNamed, utils::logger_print_log_named);

	// timer
	ADD_INTERNAL_CALL(Timer_WaitFor, utils::timer_wait_for);
	ADD_INTERNAL_CALL(Timer_SetTimeout, utils::timer_set_timeout);
	ADD_INTERNAL_CALL(Timer_SetInterval, utils::timer_set_interval);
	ADD_INTERNAL_CALL(Timer_PauseTimer, utils::timer_pause_timer);
	ADD_INTERNAL_CALL(Timer_ResumeTimer, utils::timer_resume_timer);
	ADD_INTERNAL_CALL(Timer_StopTimer, utils::timer_stop_timer);
	ADD_INTERNAL_CALL(Timer_Clear, utils::timer_clear);
	ADD_INTERNAL_CALL(Timer_Exists, utils::timer_exists);

	// asset manager
	ADD_INTERNAL_CALL(AssetManager_ImportAsset, utils::asset_manager_import_asset);
	ADD_INTERNAL_CALL(AssetManager_DeleteAsset, utils::asset_manager_delete_asset);
	ADD_INTERNAL_CALL(AssetManager_IsAssetHandleValid, utils::asset_manager_is_asset_handle_valid);
	ADD_INTERNAL_CALL(AssetManager_IsAssetLoaded, utils::asset_manager_is_asset_loaded);
	ADD_INTERNAL_CALL(AssetManager_GetAssetType, utils::asset_manager_get_asset_type);
	ADD_INTERNAL_CALL(AssetManager_GetFilepath, utils::asset_manager_get_filepath);

	// input
	ADD_INTERNAL_CALL(Input_IsKeyDown, utils::input_is_key_down);
	ADD_INTERNAL_CALL(Input_IsKeyUp, utils::input_is_key_up);
	ADD_INTERNAL_CALL(Input_IsKeyPressed, utils::input_is_key_pressed);
	ADD_INTERNAL_CALL(Input_IsKeyReleased, utils::input_is_key_released);
	ADD_INTERNAL_CALL(Input_IsMouseButtonDown, utils::input_is_mouse_button_down);
	ADD_INTERNAL_CALL(Input_IsMouseButtonUp, utils::input_is_mouse_button_up);
	ADD_INTERNAL_CALL(Input_IsMouseButtonPressed, utils::input_is_mouse_button_pressed);
	ADD_INTERNAL_CALL(Input_IsMouseButtonReleased, utils::input_is_mouse_button_released);
	ADD_INTERNAL_CALL(Input_GetMouseX, utils::input_get_mouse_x);
	ADD_INTERNAL_CALL(Input_GetMouseY, utils::input_get_mouse_y);
	ADD_INTERNAL_CALL(Input_GetMousePosition, utils::input_get_mouse_position);

	// audio data
	ADD_INTERNAL_CALL(AD_IsValid, utils::AD_is_valid);
	ADD_INTERNAL_CALL(AD_GetAudioHandle, utils::AD_get_audio_handle);
	ADD_INTERNAL_CALL(AD_SetAudioHandle, utils::AD_set_audio_handle);
	ADD_INTERNAL_CALL(AD_GetTag, utils::AD_get_tag);
	ADD_INTERNAL_CALL(AD_SetTag, utils::AD_set_tag);
	ADD_INTERNAL_CALL(AD_GetTranslation, utils::AD_get_translation);
	ADD_INTERNAL_CALL(AD_SetTranslation, utils::AD_set_translation);
	ADD_INTERNAL_CALL(AD_IsSpitial, utils::AD_is_spitial);
	ADD_INTERNAL_CALL(AD_SetSpitial, utils::AD_set_spitial);
	ADD_INTERNAL_CALL(AD_IsLoop, utils::AD_is_loop);
	ADD_INTERNAL_CALL(AD_SetLoop, utils::AD_set_loop);
	ADD_INTERNAL_CALL(AD_GetGain, utils::AD_get_gain);
	ADD_INTERNAL_CALL(AD_SetGain, utils::AD_set_gain);
	ADD_INTERNAL_CALL(AD_GetPitch, utils::AD_get_pitch);
	ADD_INTERNAL_CALL(AD_SetPitch, utils::AD_set_pitch);
	ADD_INTERNAL_CALL(AD_GetFullClipLength, utils::AD_get_full_clip_length);
	ADD_INTERNAL_CALL(AD_GetClipStart, utils::AD_get_clip_start);
	ADD_INTERNAL_CALL(AD_SetClipStart, utils::AD_set_clip_start);
	ADD_INTERNAL_CALL(AD_GetClipEnd, utils::AD_get_clip_end);
	ADD_INTERNAL_CALL(AD_SetClipEnd, utils::AD_set_clip_end);

	// audio Engine or Manager
	ADD_INTERNAL_CALL(AudioEngine_UpdatePosition, utils::audio_engine_update_position);

	// Animation
	ADD_INTERNAL_CALL(Animation_GetAnimationByName, utils::animation_get_anim_by_tag);
	ADD_INTERNAL_CALL(Animation_IsValid, utils::animation_is_valid);
	ADD_INTERNAL_CALL(Animation_Bound, utils::animation_bound);
	ADD_INTERNAL_CALL(Animation_Unbound, utils::animation_unbound);
	ADD_INTERNAL_CALL(Animation_Play, utils::animation_play);
	ADD_INTERNAL_CALL(Animation_Stop, utils::animation_stop);
	ADD_INTERNAL_CALL(Animation_Pause, utils::animation_pause);
	ADD_INTERNAL_CALL(Animation_Resume, utils::animation_resume);
	ADD_INTERNAL_CALL(Animation_IsPlaying, utils::animation_is_playing);
	ADD_INTERNAL_CALL(Animation_IsPaused, utils::animation_is_paused);
	ADD_INTERNAL_CALL(Animation_IsLooping, utils::animation_is_looping);
	ADD_INTERNAL_CALL(Animation_SetLoop, utils::animation_set_loop);

	// camera component
	ADD_INTERNAL_CALL(CameraComponent_IsPrimary, utils::camera_component_is_primary);
	ADD_INTERNAL_CALL(CameraComponent_SetPrimary, utils::camera_component_set_primary);
	ADD_INTERNAL_CALL(CameraComponent_IsFixedAspectRatio, utils::camera_component_is_fixed_aspect_ratio);
	ADD_INTERNAL_CALL(CameraComponent_SetFixedAspectRatio, utils::camera_component_set_fixed_aspect_ratio);
	ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveVerticalFOV, utils::camera_component_get_perspective_vertical_FOV);
	ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveVerticalFOV, utils::camera_component_set_perspective_vertical_FOV);
	ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveNearClip, utils::camera_component_get_perspective_near_clip);
	ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveNearClip, utils::camera_component_set_perspective_near_clip);
	ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveFarClip, utils::camera_component_get_perspective_far_clip);
	ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveFarClip, utils::camera_component_set_perspective_far_clip);
	ADD_INTERNAL_CALL(CameraComponent_GetOrthographicSize, utils::camera_component_get_orthographic_size);
	ADD_INTERNAL_CALL(CameraComponent_SetOrthographicSize, utils::camera_component_set_orthographic_size);
	ADD_INTERNAL_CALL(CameraComponent_GetOrthographicNearClip, utils::camera_component_get_orthographic_near_clip);
	ADD_INTERNAL_CALL(CameraComponent_SetOrthographicNearClip, utils::camera_component_set_orthographic_near_clip);
	ADD_INTERNAL_CALL(CameraComponent_GetOrthographicFarClip, utils::camera_component_get_orthographic_far_clip);
	ADD_INTERNAL_CALL(CameraComponent_SetOrthographicFarClip, utils::camera_component_set_orthographic_far_clip);

	// transform component
	ADD_INTERNAL_CALL(TransformComponent_GetTranslation, utils::transform_component_get_translation);
	ADD_INTERNAL_CALL(TransformComponent_SetTranslation, utils::transform_component_set_translation);
	ADD_INTERNAL_CALL(TransformComponent_GetRotation, utils::transform_component_get_rotation);
	ADD_INTERNAL_CALL(TransformComponent_SetRotation, utils::transform_component_set_rotation);
	ADD_INTERNAL_CALL(TransformComponent_GetScale, utils::transform_component_get_scale);
	ADD_INTERNAL_CALL(TransformComponent_SetScale, utils::transform_component_set_scale);

	// text component
	ADD_INTERNAL_CALL(TextComponent_GetText, utils::text_component_get_data);
	ADD_INTERNAL_CALL(TextComponent_SetText, utils::text_component_set_data);
	ADD_INTERNAL_CALL(TextComponent_GetColor, utils::text_component_get_color);
	ADD_INTERNAL_CALL(TextComponent_SetColor, utils::text_component_set_color);
	ADD_INTERNAL_CALL(TextComponent_GetKerning, utils::text_component_get_kerning);
	ADD_INTERNAL_CALL(TextComponent_SetKerning, utils::text_component_set_kerning);
	ADD_INTERNAL_CALL(TextComponent_GetLineSpacing, utils::text_component_get_line_spacing);
	ADD_INTERNAL_CALL(TextComponent_SetLineSpacing, utils::text_component_set_line_spacing);

	// rigidbody2D component
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyForce, utils::rigidbody2D_component_apply_force);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyForceToCenter, utils::rigidbody2D_component_apply_force_to_center);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulse, utils::rigidbody2D_component_apply_linear_impulse);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulseToCenter, utils::rigidbody2D_component_apply_linear_impulse_to_center);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyAngularImpulse, utils::rigidbody2D_component_apply_angular_impulse);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyTorque, utils::rigidbody2D_component_apply_torque);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetLinearVelocity, utils::rigidbody2D_component_get_linear_velocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetLinearVelocity, utils::rigidbody2D_component_set_linear_velocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetAngularVelocity, utils::rigidbody2D_component_get_angular_velocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetAngularVelocity, utils::rigidbody2D_component_set_angular_velocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetType, utils::rigidbody2D_component_get_type);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetType, utils::rigidbody2D_component_set_type);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_IsFixedRotation, utils::rigidbody2D_component_is_fixed_rotation);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetFixedRotation, utils::rigidbody2D_component_set_fixed_rotation);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetGravityScale, utils::rigidbody2D_component_get_gravity_scale);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetGravityScale, utils::rigidbody2D_component_set_gravity_scale);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_IsEnabled, utils::rigidbody2D_component_is_enabled);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetEnabled, utils::rigidbody2D_component_set_enabled);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_IsAwake, utils::rigidbody2D_component_is_awake);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetAwake, utils::rigidbody2D_component_set_awake);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetAngle, utils::rigidbody2D_component_get_angle);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetMass, utils::rigidbody2D_component_get_mass);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetIntertia, utils::rigidbody2D_component_get_intertia);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetTransform, utils::rigidbody2D_component_set_transform);

	// box_collider2D_component
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetOffset, utils::box_collider2D_component_get_offset);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetOffset, utils::box_collider2D_component_set_offset);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetSize, utils::box_collider2D_component_get_size);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetSize, utils::box_collider2D_component_set_size);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetTag, utils::box_collider2D_component_get_tag);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetTag, utils::box_collider2D_component_set_tag);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetDensity, utils::box_collider2D_component_get_density);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetDensity, utils::box_collider2D_component_set_density);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetFriction, utils::box_collider2D_component_get_friction);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetFriction, utils::box_collider2D_component_set_friction);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetRestitution, utils::box_collider2D_component_get_restitution);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetRestitution, utils::box_collider2D_component_set_restitution);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetRestitutionThreshold, utils::box_collider2D_component_get_restitution_threshold);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetRestitutionThreshold, utils::box_collider2D_component_set_restitution_threshold);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_IsSensor, utils::box_collider2D_component_is_sensor);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetSensor, utils::box_collider2D_component_set_sensor);

	// circle_collider2D_component
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetOffset, utils::circle_collider2D_component_get_offset);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetOffset, utils::circle_collider2D_component_set_offset);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetSize, utils::circle_collider2D_component_get_radius);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetSize, utils::circle_collider2D_component_set_radius);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetTag, utils::circle_collider2D_component_get_tag);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetTag, utils::circle_collider2D_component_set_tag);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetDensity, utils::circle_collider2D_component_get_density);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetDensity, utils::circle_collider2D_component_set_density);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetFriction, utils::circle_collider2D_component_get_friction);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetFriction, utils::circle_collider2D_component_set_friction);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetRestitution, utils::circle_collider2D_component_get_restitution);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetRestitution, utils::circle_collider2D_component_set_restitution);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetRestitutionThreshold, utils::circle_collider2D_component_get_restitution_threshold);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetRestitutionThreshold, utils::circle_collider2D_component_set_restitution_threshold);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_IsSensor, utils::circle_collider2D_component_is_sensor);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetSensor, utils::circle_collider2D_component_set_sensor);

	// sprite_renderer_component
	ADD_INTERNAL_CALL(SpriteRendererComponent_GetColor, utils::sprite_renderer_component_get_color);
	ADD_INTERNAL_CALL(SpriteRendererComponent_SetColor, utils::sprite_renderer_component_set_color);
	ADD_INTERNAL_CALL(SpriteRendererComponent_GetTilingFactor, utils::sprite_renderer_component_get_tiling_factor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_SetTilingFactor, utils::sprite_renderer_component_set_tiling_factor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_GetTextureHandle, utils::sprite_renderer_component_get_texture_handle);
	ADD_INTERNAL_CALL(SpriteRendererComponent_SetTextureHandle, utils::sprite_renderer_component_set_texture_handle);
	// todo texture jobs

	// circle_renderer_component
	ADD_INTERNAL_CALL(CircleRendererComponent_GetColor, utils::circle_renderer_component_get_color);
	ADD_INTERNAL_CALL(CircleRendererComponent_SetColor, utils::circle_renderer_component_set_color);
	ADD_INTERNAL_CALL(CircleRendererComponent_GetThickness, utils::circle_renderer_component_get_thickness);
	ADD_INTERNAL_CALL(CircleRendererComponent_SetThickness, utils::circle_renderer_component_set_thickness);
	ADD_INTERNAL_CALL(CircleRendererComponent_GetFade, utils::circle_renderer_component_get_fade);
	ADD_INTERNAL_CALL(CircleRendererComponent_SetFade, utils::circle_renderer_component_set_fade);

	// audio_component
	ADD_INTERNAL_CALL(AudioComponent_GetADCount, utils::audio_component_get_AD_count);
	ADD_INTERNAL_CALL(AudioComponent_GetAD, utils::audio_component_get_AD);
	ADD_INTERNAL_CALL(AudioComponent_CreateAudioData, utils::audio_component_create_audio_data);
	ADD_INTERNAL_CALL(AudioComponent_RemoveAudioData, utils::audio_component_remove_audio_data);
}

void script_glue::on_runtime_start()
{
}

void script_glue::on_runtime_stop()
{
	timer_manager::get().get_group_map().get(runtime_timers_group_name).clear();
	timer_manager::get().get_group_map(application_mode::runtime).clear();
}

_WHIP_END
