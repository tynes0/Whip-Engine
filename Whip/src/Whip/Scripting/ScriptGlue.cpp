#include "WhipPch.h"
#include <Whip/Scripting/ScriptGlue.h>
#include <Whip/Scripting/ScriptEngine.h>

#include <Whip/Core/UUID.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/KeyCodes.h>
#include <Whip/Core/Input.h>
#include <Whip/Helper/TimerManager.h>

#include <Whip/Scene/Entity.h>
#include <Whip/Scene/Components.h>
#include <Whip/Scene/Scene.h>

#include <Whip/Physics/ContactListener.h>
#include <Whip/Physics/Physics2D.h>

#include <Whip/Project/Project.h>

#include <Whip/Asset/AssetManager.h>

#include <Whip/Audio/AudioEngine.h>
#include <Whip/Animation/AnimationManager.h>

#include <cstring>

#include <glm/glm.hpp>

#include <mono/metadata/object.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/reflection.h>

#include <box2d/b2_body.h>

_WHIP_START

#define ADD_INTERNAL_CALL(name, func) mono_add_internal_call(WHP_CONCATENATE("Whip.InternalCalls::", WHP_STRINGIZE(name)), func)

namespace
{
	constexpr const char* RuntimeTimersGroupId = "0177f1a8-04e5-4340-a771-52fc1aac9440";
	std::unordered_map<MonoType*, std::function<bool(Entity)>> s_EntityHasComponentFuncs;
	Logger s_Logger;

	namespace detail
	{
		Scene* GetScene()
		{
			Scene* scne = ScriptEngine::GetSceneContext();
			WHP_CORE_ASSERT(scne);
			return scne;
		}

		Entity GetEntity(UUID id)
		{
			Scene* scne = GetScene();
			Entity ent = scne->FindEntityByUUID(id);
			WHP_CORE_ASSERT(ent);
			return ent;
		}

		std::string MonoStringToString(MonoString* string)
		{
			char* cstr = mono_string_to_utf8(string);
			std::string str(cstr);
			mono_free(cstr);
			return str;
		}

		std::wstring MonoStringToWstring(MonoString* string)
		{
			wchar_t* cstr = (wchar_t*)mono_string_to_utf16(string);
			std::wstring wstr(cstr);
			mono_free(cstr);
			return wstr;
		}

		AudioComponent::AudioData* FindAcAD(std::vector<AudioComponent::AudioData>& handleList, UUID32 id)
		{
			for (AudioComponent::AudioData& handle : handleList)
				if (handle.m_ID == id)
					return &handle;
			return nullptr;
		}

		b2Body* GetBody(UUID id)
		{
			Entity ent = GetEntity(id);
			auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
			b2Body* body = static_cast<b2Body*>(rb2d.m_RuntimeBody);
			WHP_CORE_ASSERT(body);
			return body;
		}

		AudioComponent::AudioData* GetAudioData(UUID id, UUID32 adId)
		{
			Entity ent = GetEntity(id);
			AudioComponent& ac = ent.GetComponent<AudioComponent>();
			AudioComponent::AudioData* audioData = detail::FindAcAD(ac.m_AudioDatas, adId);
			return audioData;
		}

		Ref<Animation2D> GetAnimation(UUID handle)
		{
			return std::static_pointer_cast<Animation2D>(Project::GetActive()->GetRuntimeAssetManager()->GetAsset(handle));
		}
	}

	MonoObject* GetScriptInstance(UUID entityId)
	{
		return ScriptEngine::GetManagedInstance(entityId);
	}

	bool EntityHasComponent(UUID entityId, MonoReflectionType* componentType)
	{
		Entity ent = detail::GetEntity(entityId);

		MonoType* managedType = mono_reflection_type_get_type(componentType);
		WHP_CORE_ASSERT(s_EntityHasComponentFuncs.find(managedType) != s_EntityHasComponentFuncs.end());
		return s_EntityHasComponentFuncs.at(managedType)(ent);
	}

	uint64_t EntityFindEntityByName(MonoString* name)
	{
		Scene* scne = detail::GetScene();
		Entity ent = scne->FindEntityByName(detail::MonoStringToString(name));
		if (!ent)
			return 0;
		return ent.GetUUID();
	}

	void LoggerInternalLog(MonoString* logMessage, Log::Level level)
	{
		Log::GetCoreLogger()->log(Log::WhipLogLevelToSpdlogLevel(level), detail::MonoStringToString(logMessage));
	}

	void LoggerInternalAssert(bool cond, MonoString* logMessage, MonoString* filepath, int line)
	{
		if (!(cond))
		{
			WHP_CORE_CRITICAL(
				"Whip Assertion failed! File: {0}, Line: {1}, Message: {2}",
				std::filesystem::path(detail::MonoStringToString(filepath)).filename().string(),
				line,
				detail::MonoStringToString(logMessage));
			WHP_DEBUGBREAK();
		}
	}

	void LoggerSetLogger(MonoString* loggerName)
	{
		Log::ResetLogger(s_Logger, detail::MonoStringToString(loggerName), Log::OutputTarget::Editor);
	}

	void LoggerPrintLog(MonoString* logMessage, Log::Level level)
	{
		s_Logger->log(Log::WhipLogLevelToSpdlogLevel(level), detail::MonoStringToString(logMessage));
		EditorLog::FileShouldReset().store(true);
	}

	void LoggerPrintLogNamed(MonoString* loggerName, MonoString* logMessage, Log::Level level)
	{
		std::string name = s_Logger->name();
		LoggerSetLogger(loggerName);
		LoggerPrintLog(logMessage, level);
		Log::ResetLogger(s_Logger, name);
	}

	bool TimerWaitFor(UUID tag, float ms)
	{
		TimerId id = 0;
		bool result = TimerManager::Get().WaitFor(tag, ms, 0, &id);
		if (id != 0)
			TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Add(id);
		return result;
	}

	uint64_t TimerSetTimeout(MonoObject* func, float delayMs, MonoObject* userData)
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

		TimerId id = TimerManager::Get().SetTimeout([func, method](void* userDataPtr)
			{
				void* args[] = { userDataPtr };
				ScriptClass::InvokeMethod(func, method, args);
			}, delayMs, static_cast<void*>(userData));
			TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Add(id);
		return id;
	}

	uint64_t TimerSetInterval(MonoObject* func, float intervalMs, MonoObject* userData)
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

		TimerId id = TimerManager::Get().SetInterval([func, method](void* userDataPtr)
			{
				void* args[] = { userDataPtr };
				ScriptClass::InvokeMethod(func, method, args);
			}, intervalMs, static_cast<void*>(userData));
		TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Add(id);
		return id;
	}

	void TimerPauseTimer(uint64_t timerId)
	{
		if(TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Exists(timerId))
			TimerManager::Get().PauseTimer(timerId);
	}

	void TimerResumeTimer(uint64_t timerId)
	{
		if (TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Exists(timerId))
			TimerManager::Get().ResumeTimer(timerId);
	}

	static void TimerStopTimer(uint64_t timerId)
	{
		if (TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Exists(timerId))
		{
			TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Remove(timerId);
			TimerManager::Get().StopTimer(timerId);
		}
	}

	static void TimerClear()
	{
		TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Clear();
	}

	static bool TimerExists(uint64_t id)
	{
		return TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Exists(id);
	}

	static uint64_t AssetManagerImportAsset(MonoString* path)
	{
		return Project::GetActive()->GetRuntimeAssetManager()->ImportAsset(detail::MonoStringToWstring(path));
	}

	static void AssetManagerDeleteAsset(AssetHandle handle)
	{
		Project::GetActive()->GetRuntimeAssetManager()->DeleteAsset(handle);
	}

	static bool AssetManagerIsAssetHandleValid(AssetHandle handle)
	{
		return Project::GetActive()->GetRuntimeAssetManager()->IsAssetHandleValid(handle);
	}

	static bool AssetManagerIsAsSetLoaded(AssetHandle handle)
	{
		auto man = Project::GetActive()->GetRuntimeAssetManager();
		return man->IsAssetLoaded(handle);
	}

	static AssetType AssetManagerGetAssetType(AssetHandle handle)
	{
		return Project::GetActive()->GetRuntimeAssetManager()->GetAssetType(handle);
	}

	static MonoString* AssetManagerGetFilepath(AssetHandle handle)
	{
		auto& path = Project::GetActive()->GetRuntimeAssetManager()->GetFilepath(handle);
		return Utils::CreateString(path.c_str());
	}

	static bool SceneManagerLoadScene(AssetHandle handle)
	{
		return ScriptEngine::RequestRuntimeSceneLoad(handle);
	}

	static bool SceneManagerLoadSceneByName(MonoString* sceneName)
	{
		if (!sceneName)
			return false;

		return ScriptEngine::RequestRuntimeSceneLoad(detail::MonoStringToString(sceneName));
	}

	static uint64_t SceneManagerFindSceneByName(MonoString* sceneName)
	{
		if (!sceneName)
			return 0;

		return ScriptEngine::FindRuntimeSceneHandle(detail::MonoStringToString(sceneName));
	}

	static bool SceneManagerLoadStartScene()
	{
		return ScriptEngine::RequestRuntimeStartSceneLoad();
	}

	static bool SceneManagerReloadScene()
	{
		return ScriptEngine::RequestRuntimeSceneReload();
	}

	static bool SceneManagerUnloadScene()
	{
		return ScriptEngine::RequestRuntimeSceneUnload();
	}

	static AssetHandle SceneManagerGetActiveSceneHandle()
	{
		return ScriptEngine::GetRuntimeActiveSceneHandle();
	}

	static bool InputIsKeyDown(KeyCode keyCode)
	{
		return Input::IsKeyDown(keyCode);
	}

	static bool InputIsKeyUp(KeyCode keyCode)
	{
		return Input::IsKeyUp(keyCode);
	}

	static bool InputIsKeyPressed(KeyCode keyCode)
	{
		return Input::IsKeyPressed(keyCode);
	}

	static bool InputIsKeyReleased(KeyCode keyCode)
	{
		return Input::IsKeyReleased(keyCode);
	}

	static bool InputIsMouseButtonDown(MouseCode button)
	{
		return Input::IsMouseButtonDown(button);
	}

	static bool InputIsMouseButtonUp(MouseCode button)
	{
		return Input::IsMouseButtonUp(button);
	}

	static bool InputIsMouseButtonPressed(MouseCode button)
	{
		return Input::IsMouseButtonPressed(button);
	}

	static bool InputIsMouseButtonReleased(MouseCode button)
	{
		return Input::IsMouseButtonReleased(button);
	}

	static float InputGetMouseX()
	{
		return Input::GetMouseX();
	}

	static float InputGetMouseY()
	{
		return Input::GetMouseY();
	}

	static void InputGetMousePosition(glm::vec2* position)
	{
		auto pos = Input::GetMousePosition();
		position->x = pos.first;
		position->y = pos.second;
	}

	static bool ADIsValid(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		return bool(audioData);
	}

	static uint64_t ADGetAudioHandle(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		return audioData ? ((uint64_t)audioData->m_Audio) : 0;
	}

	static void ADSetAudioHandle(UUID entityId, UUID32 audioDataId, uint64_t newHandle)
	{
		if (!Project::GetActive()->GetRuntimeAssetManager()->GetAssetRegistry().Exist(AssetType::Audio, newHandle))
		{
			WHP_CORE_WARN("[C# Method] The given handle is not bound to a audio!");
			return;
		}
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		audioData->m_Audio = newHandle;
	}

	static MonoString* ADGetTag(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return Utils::CreateString("");
		}
		return Utils::CreateString(audioData->m_Tag.c_str());
	}

	static void ADSetTag(UUID entityId, UUID32 audioDataId, MonoString* tag)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		audioData->m_Tag = detail::MonoStringToString(tag);
	}

	static void ADGetTranslation(UUID entityId, UUID32 audioDataId, glm::vec3* translation)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		*translation = audioData->m_Translation;
	}

	static void ADSetTranslation(UUID entityId, UUID32 audioDataId, glm::vec3* translation)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		audioData->m_Translation = *translation;
	}

	static bool ADIsSpitial(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return false;
		}
		return audioData->m_Spatial;
	}

	static void ADSetSpitial(UUID entityId, UUID32 audioDataId, bool spitial)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		audioData->m_Spatial = spitial;
	}

	static bool ADIsLoop(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return false;
		}
		return audioData->m_Loop;
	}

	static void ADSetLoop(UUID entityId, UUID32 audioDataId, bool loop)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		audioData->m_Loop = loop;
	}

	static float ADGetGain(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return audioData->m_Gain;
	}

	static void ADSetGain(UUID entityId, UUID32 audioDataId, float gain)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		audioData->m_Gain = gain;
	}

	static float ADGetPitch(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return audioData->m_Pitch;
	}

	static void ADSetPitch(UUID entityId, UUID32 audioDataId, float pitch)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		audioData->m_Pitch = pitch;
	}

	static float ADGetFullClipLength(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return audioData->m_FullClipLength;
	}

	static float ADGetClipStart(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return audioData->m_ClipStart;
	}

	static void ADSetClipStart(UUID entityId, UUID32 audioDataId, float clipStart)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		audioData->m_ClipStart = clipStart;
	}

	static float ADGetClipEnd(UUID entityId, UUID32 audioDataId)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return 0.0f;
		}
		return audioData->m_ClipEnd;
	}

	static void ADSetClipEnd(UUID entityId, UUID32 audioDataId, float clipEnd)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		audioData->m_ClipEnd = clipEnd;
	}

	static void AudioEngineUpdatePosition(UUID entityId, UUID32 audioDataId, glm::vec3* position)
	{
		auto* audioData = detail::GetAudioData(entityId, audioDataId);
		if (!audioData)
		{
			WHP_CORE_WARN("[C# Method] Audio Data not found!");
			return;
		}
		auto audioAsset = AssetManager::GetAsset<AudioSource>(audioData->m_Audio);
		audioAsset->UpdateSpatialPosition(position->x, position->y, position->z);
	}

	static uint64_t AnimationGetAnimByTag(MonoString* name)
	{
		uint64_t result = 0;
		std::string strName = detail::MonoStringToString(name);
		auto& assets = Project::GetActive()->GetEditorAssetManager()->GetAssetRegistry();
		assets.ForeachChecked(AssetType::Animation, [&result, strName](const AssetRegistry::ValueType& value)
			{
				auto anim = AssetManager::GetAsset<Animation2D>(value.first);
				if (anim->GetName() == strName)
				{
					auto copiedAnim = Animation2D::Copy(anim);
					Project::GetActive()->GetRuntimeAssetManager()->AddAssetCopy(anim->m_Handle, copiedAnim);
					AnimationManager::Get().AddAnimation(copiedAnim);
					result = copiedAnim->m_Handle;
					return AssetRegistry::LoopStop;
				}
				return AssetRegistry::LoopContinue;
			});
		return result;
	}

	static bool AnimationIsValid(UUID animationHandle)
	{
		return Project::GetActive()->GetRuntimeAssetManager()->GetAssetRegistry().Exist(AssetType::Animation, animationHandle);
	}

	static void AnimationBound(UUID animationHandle, UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->BindWithEntity(ent);
	}

	static void AnimationUnbound(UUID animationHandle)
	{
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->UnbindFromEntity();
	}

	static void AnimationPlay(UUID animationHandle)
	{
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->Play();
	}

	static void AnimationStop(UUID animationHandle)
	{
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->Stop();
	}

	static void AnimationPause(UUID animationHandle)
	{
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->Pause();
	}

	static void AnimationResume(UUID animationHandle)
	{
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if(!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->Resume();
	}

	static bool AnimationIsPlaying(UUID animationHandle)
	{
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return false;
		}
		return anim->IsPlaying();
	}

	static bool AnimationIsPaused(UUID animationHandle)
	{
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return false;
		}
		return anim->IsPaused();
	}

	static bool AnimationIsLooping(UUID animationHandle)
	{
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return false;
		}
		return anim->IsLooping();
	}

	static void AnimationSetLoop(UUID animationHandle, bool loop)
	{
		Ref<Animation2D> anim = detail::GetAnimation(animationHandle);
		if (!anim)
		{
			WHP_CORE_WARN("[C# Method] Animation is not valid!");
			return;
		}
		anim->SetLoop(loop);
	}

	static AnimatorRuntime* AnimatorComponentGetRuntime(UUID entityId, bool createIfMissing = true)
	{
		Entity ent = detail::GetEntity(entityId);
		if (!ent.HasComponent<AnimatorComponent>())
		{
			WHP_CORE_WARN("[C# Method] Entity does not have AnimatorComponent!");
			return nullptr;
		}

		return createIfMissing ? ent.GetScene()->GetOrCreateAnimatorRuntime(ent) : ent.GetScene()->GetAnimatorRuntime(entityId);
	}

	static uint64_t AnimatorComponentGetController(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.HasComponent<AnimatorComponent>() ? static_cast<uint64_t>(ent.GetComponent<AnimatorComponent>().m_Controller) : 0;
	}

	static void AnimatorComponentSetController(UUID entityId, AssetHandle controllerHandle)
	{
		Entity ent = detail::GetEntity(entityId);
		if (!ent.HasComponent<AnimatorComponent>())
			return;

		ent.GetComponent<AnimatorComponent>().m_Controller = controllerHandle;
		ent.GetScene()->GetOrCreateAnimatorRuntime(ent);
	}

	static float AnimatorComponentGetSpeed(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.HasComponent<AnimatorComponent>() ? ent.GetComponent<AnimatorComponent>().m_Speed : 0.0f;
	}

	static void AnimatorComponentSetSpeed(UUID entityId, float speed)
	{
		Entity ent = detail::GetEntity(entityId);
		if (ent.HasComponent<AnimatorComponent>())
			ent.GetComponent<AnimatorComponent>().m_Speed = speed;
	}

	static void AnimatorComponentPlay(UUID entityId, MonoString* stateName)
	{
		AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId);
		if (!runtime)
			return;

		const std::string state = stateName ? detail::MonoStringToString(stateName) : std::string{};
		runtime->Play(state);
	}

	static void AnimatorComponentStop(UUID entityId)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId, false))
			runtime->Stop();
	}

	static bool AnimatorComponentIsPlaying(UUID entityId)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId, false))
			return runtime->IsPlaying();
		return false;
	}

	static MonoString* AnimatorComponentGetCurrentState(UUID entityId)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId, false))
			return Utils::CreateString(runtime->GetCurrentStateName().c_str());
		return Utils::CreateString("");
	}

	static void AnimatorComponentSetBool(UUID entityId, MonoString* name, bool value)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId))
			runtime->SetBool(detail::MonoStringToString(name), value);
	}

	static void AnimatorComponentSetInt(UUID entityId, MonoString* name, int32_t value)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId))
			runtime->SetInt(detail::MonoStringToString(name), value);
	}

	static void AnimatorComponentSetFloat(UUID entityId, MonoString* name, float value)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId))
			runtime->SetFloat(detail::MonoStringToString(name), value);
	}

	static void AnimatorComponentSetTrigger(UUID entityId, MonoString* name)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId))
			runtime->SetTrigger(detail::MonoStringToString(name));
	}

	static void AnimatorComponentResetTrigger(UUID entityId, MonoString* name)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId))
			runtime->ResetTrigger(detail::MonoStringToString(name));
	}

	static bool AnimatorComponentGetBool(UUID entityId, MonoString* name)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId))
			return runtime->GetBool(detail::MonoStringToString(name));
		return false;
	}

	static int32_t AnimatorComponentGetInt(UUID entityId, MonoString* name)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId))
			return runtime->GetInt(detail::MonoStringToString(name));
		return 0;
	}

	static float AnimatorComponentGetFloat(UUID entityId, MonoString* name)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId))
			return runtime->GetFloat(detail::MonoStringToString(name));
		return 0.0f;
	}

	static bool AnimatorComponentIsTriggerSet(UUID entityId, MonoString* name)
	{
		if (AnimatorRuntime* runtime = AnimatorComponentGetRuntime(entityId))
			return runtime->IsTriggerSet(detail::MonoStringToString(name));
		return false;
	}

	static void CameraComponentSetPrimary(UUID entityId, bool primary)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Primary = primary;
	}

	static bool CameraComponentIsPrimary(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Primary;
	}

	static void CameraComponentSetFixedAspectRatio(UUID entityId, bool fixedAspectRatio)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_FixedAspectRatio = fixedAspectRatio;
	}

	static bool CameraComponentIsFixedAspectRatio(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_FixedAspectRatio;
	}

	static void CameraComponentSetPerspectiveVerticalFOV(UUID entityId, float perspectiveVerticalFOV)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetPerspectiveVerticalFOV(perspectiveVerticalFOV);
	}

	float CameraComponentGetPerspectiveVerticalFOV(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetPerspectiveVerticalFOV();
	}

	void CameraComponentSetPerspectiveNearClip(UUID entityId, float perspectiveNearClip)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetPerspectiveNearClip(perspectiveNearClip);
	}

	float CameraComponentGetPerspectiveNearClip(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetPerspectiveNearClip();
	}

	void CameraComponentSetPerspectiveFarClip(UUID entityId, float perspectiveFarClip)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetPerspectiveFarClip(perspectiveFarClip);
	}

	float CameraComponentGetPerspectiveFarClip(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetPerspectiveFarClip();
	}

	void CameraComponentSetOrthographicSize(UUID entityId, float orthographicSize)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetOrthographicSize(orthographicSize);
	}

	float CameraComponentGetOrthographicSize(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetOrthographicSize();
	}

	void CameraComponentSetOrthographicNearClip(UUID entityId, float orthographicNearClip)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetOrthographicNearClip(orthographicNearClip);
	}

	float CameraComponentGetOrthographicNearClip(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetOrthographicNearClip();
	}

	void CameraComponentSetOrthographicFarClip(UUID entityId, float orthographicFarClip)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetOrthographicFarClip(orthographicFarClip);
	}

	float CameraComponentGetOrthographicFarClip(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetOrthographicFarClip();
	}

	void TransformComponentGetTranslation(UUID entityId, glm::vec3* outTranslation)
	{
		Entity ent = detail::GetEntity(entityId);
		*outTranslation = ent.GetComponent<TransformComponent>().m_Translation;
	}

	void TransformComponentSetTranslation(UUID entityId, glm::vec3* translation)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<TransformComponent>().m_Translation = *translation;
	}

	void TransformComponentGetRotation(UUID entityId, glm::vec3* outRotation)
	{
		Entity ent = detail::GetEntity(entityId);
		*outRotation = glm::degrees(ent.GetComponent<TransformComponent>().m_Rotation);
	}

	void TransformComponentSetRotation(UUID entityId, glm::vec3* rotation)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<TransformComponent>().m_Rotation = glm::radians(*rotation);
	}

	void TransformComponentGetScale(UUID entityId, glm::vec3* outScale)
	{
		Entity ent = detail::GetEntity(entityId);
		*outScale = ent.GetComponent<TransformComponent>().m_Scale;
	}

	void TransformComponentSetScale(UUID entityId, glm::vec3* scale)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<TransformComponent>().m_Scale = *scale;
	}

	void Rigidbody2DComponentApplyForce(UUID entityId, glm::vec2* force, glm::vec2* point, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyForce(b2Vec2(force->x, force->y), b2Vec2(point->x, point->y), wake);
	}

	void Rigidbody2DComponentApplyForceToCenter(UUID entityId, glm::vec2* force, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyForceToCenter(b2Vec2(force->x, force->y), wake);
	}

	void Rigidbody2DComponentApplyLinearImpulse(UUID entityId, glm::vec2* impulse, glm::vec2* point, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyLinearImpulse(b2Vec2(impulse->x, impulse->y), b2Vec2(point->x, point->y), wake);
	}

	void Rigidbody2DComponentApplyLinearImpulseToCenter(UUID entityId, glm::vec2* impulse, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyLinearImpulseToCenter(b2Vec2(impulse->x, impulse->y), wake);
	}

	void Rigidbody2DComponentApplyAngularImpulse(UUID entityId, float impulse, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyAngularImpulse(impulse, wake);
	}

	void Rigidbody2DComponentApplyTorque(UUID entityId, float torque, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyTorque(torque, wake);
	}

	void Rigidbody2DComponentGetLinearVelocity(UUID entityId, glm::vec2* outLinearVelocity)
	{
		auto* body = detail::GetBody(entityId);
		const b2Vec2& linearVelocity = body->GetLinearVelocity();
		*outLinearVelocity = glm::vec2(linearVelocity.x, linearVelocity.y);
	}

	void Rigidbody2DComponentSetLinearVelocity(UUID entityId, glm::vec2* linearVelocity)
	{
		auto* body = detail::GetBody(entityId);
		body->SetLinearVelocity(b2Vec2(linearVelocity->x, linearVelocity->y));
	}

	float Rigidbody2DComponentGetAngularVelocity(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetAngularVelocity();
	}

	void Rigidbody2DComponentSetAngularVelocity(UUID entityId, float angularVelocity)
	{
		auto* body = detail::GetBody(entityId);
		body->SetAngularVelocity(angularVelocity);
	}

	Rigidbody2DComponent::BodyType Rigidbody2DComponentGetType(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return Physics2D::Rigidbody2DTypeFromBox2DBody(body->GetType());
	}

	void Rigidbody2DComponentSetType(UUID entityId, Rigidbody2DComponent::BodyType bodyType)
	{
		auto* body = detail::GetBody(entityId);
		body->SetType(Physics2D::Rigidbody2DTypeToBox2DBody(bodyType));
	}

	bool Rigidbody2DComponentIsFixedRotation(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->IsFixedRotation();
	}

	void Rigidbody2DComponentSetFixedRotation(UUID entityId, bool fixed)
	{
		auto* body = detail::GetBody(entityId);
		body->SetFixedRotation(fixed);
	}

	float Rigidbody2DComponentGetGravityScale(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetGravityScale();
	}

	void Rigidbody2DComponentSetGravityScale(UUID entityId, float scale)
	{
		auto* body = detail::GetBody(entityId);
		body->SetGravityScale(scale);
	}

	bool Rigidbody2DComponentIsEnabled(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->IsEnabled();
	}

	void Rigidbody2DComponentSetEnabled(UUID entityId, bool enabled)
	{
		auto* body = detail::GetBody(entityId);
		body->SetEnabled(enabled);
	}

	bool Rigidbody2DComponentIsAwake(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->IsAwake();
	}

	void Rigidbody2DComponentSetAwake(UUID entityId, bool awake)
	{
		auto* body = detail::GetBody(entityId);
		body->SetAwake(awake);
	}

	float Rigidbody2DComponentGetAngle(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetAngle();
	}

	float Rigidbody2DComponentGetMass(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetMass();
	}

	float Rigidbody2DComponentGetIntertia(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetInertia();
	}

	void Rigidbody2DComponentSetTransform(UUID entityId, glm::vec2* position, float angle)
	{
		auto* body = detail::GetBody(entityId);
		body->SetTransform(b2Vec2(position->x, position->y), angle);
	}

	MonoString* TextComponentGetData(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		return Utils::CreateString(tc.m_TextString.c_str());
	}

	void TextComponentSetData(UUID entityId, MonoString* textString)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		tc.m_TextString = detail::MonoStringToString(textString);
	}

	void TextComponentGetColor(UUID entityId, glm::vec4* color)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		*color = tc.m_Color;
	}

	void TextComponentSetColor(UUID entityId, glm::vec4* color)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		tc.m_Color = *color;
	}

	float TextComponentGetKerning(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		return tc.m_Kerning;
	}

	void TextComponentSetKerning(UUID entityId, float kerning)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		tc.m_Kerning = kerning;
	}

	float TextComponentGetLineSpacing(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		return tc.m_LineSpacing;
	}

	void TextComponentSetLineSpacing(UUID entityId, float lineSpacing)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		tc.m_LineSpacing = lineSpacing;
	}

	void BoxCollider2DComponentGetOffset(UUID entityId, glm::vec2* offset)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		*offset = bc2dc.m_Offset;
	}

	void BoxCollider2DComponentSetOffset(UUID entityId, glm::vec2* offset)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Offset = *offset;
	}

	void BoxCollider2DComponentGetSize(UUID entityId, glm::vec2* size)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		*size = bc2dc.m_Size;
	}

	void BoxCollider2DComponentSetSize(UUID entityId, glm::vec2* size)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Size = *size;
	}

	MonoString* BoxCollider2DComponentGetTag(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return Utils::CreateString(bc2dc.m_Tag.c_str());
	}

	void BoxCollider2DComponentSetTag(UUID entityId, MonoString* tag)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Tag = detail::MonoStringToString(tag);
	}

	float BoxCollider2DComponentGetDensity(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_Density;
	}

	void BoxCollider2DComponentSetDensity(UUID entityId, float density)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Density = density;
	}

	float BoxCollider2DComponentGetFriction(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_Friction;
	}

	void BoxCollider2DComponentSetFriction(UUID entityId, float friction)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Friction = friction;
	}

	float BoxCollider2DComponentGetRestitution(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_Restitution;
	}

	void BoxCollider2DComponentSetRestitution(UUID entityId, float restitution)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Restitution = restitution;
	}

	float BoxCollider2DComponentGetRestitutionThreshold(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_RestitutionThreshold;
	}

	void BoxCollider2DComponentSetRestitutionThreshold(UUID entityId, float restitutionThreshold)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_RestitutionThreshold = restitutionThreshold;
	}

	float BoxCollider2DComponentIsSensor(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_Sensor;
	}

	void BoxCollider2DComponentSetSensor(UUID entityId, bool sensor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Sensor = sensor;
	}

	void CircleCollider2DComponentGetOffset(UUID entityId, glm::vec2* offset)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		*offset = cc2dc.m_Offset;
	}

	void CircleCollider2DComponentSetOffset(UUID entityId, glm::vec2* offset)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Offset = *offset;
	}

	float CircleCollider2DComponentGetRadius(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Radius;
	}

	void CircleCollider2DComponentSetRadius(UUID entityId, float radius)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Radius = radius;
	}

	MonoString* CircleCollider2DComponentGetTag(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return Utils::CreateString(cc2dc.m_Tag.c_str());
	}

	void CircleCollider2DComponentSetTag(UUID entityId, MonoString* tag)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Tag = detail::MonoStringToString(tag);
	}

	float CircleCollider2DComponentGetDensity(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Density;
	}

	void CircleCollider2DComponentSetDensity(UUID entityId, float density)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Density = density;
	}

	float CircleCollider2DComponentGetFriction(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Friction;
	}

	void CircleCollider2DComponentSetFriction(UUID entityId, float friction)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Friction = friction;
	}

	float CircleCollider2DComponentGetRestitution(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Restitution;
	}

	void CircleCollider2DComponentSetRestitution(UUID entityId, float restitution)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Restitution = restitution;
	}

	float CircleCollider2DComponentGetRestitutionThreshold(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_RestitutionThreshold;
	}

	void CircleCollider2DComponentSetRestitutionThreshold(UUID entityId, float restitutionThreshold)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_RestitutionThreshold = restitutionThreshold;
	}

	float CircleCollider2DComponentIsSensor(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Sensor;
	}

	void CircleCollider2DComponentSetSensor(UUID entityId, bool sensor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Sensor = sensor;
	}

	void SpriteRendererComponentGetColor(UUID entityId, glm::vec4* outColor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		*outColor = src.m_Color;
	}

	void SpriteRendererComponentSetColor(UUID entityId, glm::vec4* color)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		src.m_Color = *color;
	}

	float SpriteRendererComponentGetTilingFactor(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		return src.m_TilingFactor;
	}

	void SpriteRendererComponentSetTilingFactor(UUID entityId, float tilingFactor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		src.m_TilingFactor = tilingFactor;
	}

	uint64_t SpriteRendererComponentGetTextureHandle(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		return static_cast<uint64_t>(src.m_Texture);
	}

	void SpriteRendererComponentSetTextureHandle(UUID entityId, UUID textureHandle)
	{
		if (!Project::GetActive()->GetRuntimeAssetManager()->GetAssetRegistry().Exist(AssetType::Texture2D, textureHandle))
		{
			WHP_CORE_WARN("[C# Method] The given handle is not bound to a Texture!");
			return;
		}
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		src.m_Texture = textureHandle;
	}

	void CircleRendererComponentGetColor(UUID entityId, glm::vec4* outColor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		*outColor = crc.m_Color;
	}

	void CircleRendererComponentSetColor(UUID entityId, glm::vec4* color)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		crc.m_Color = *color;
	}

	float CircleRendererComponentGetThickness(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		return crc.m_Thickness;
	}

	void CircleRendererComponentSetThickness(UUID entityId, float thickness)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		crc.m_Thickness = thickness;
	}

	float CircleRendererComponentGetFade(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		return crc.m_Fade;
	}

	void CircleRendererComponentSetFade(UUID entityId, float fade)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		crc.m_Fade = fade;
	}

	int AudioComponentGetADCount(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& ac = ent.GetComponent<AudioComponent>();
		return static_cast<int>(ac.m_AudioDatas.size());
	}

	uint32_t AudioComponentGetAD(UUID entityId, int index)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& ac = ent.GetComponent<AudioComponent>();
		if (size_t idx = static_cast<size_t>(index); idx >= ac.m_AudioDatas.size())
			return 0;
		return ac.m_AudioDatas[index].m_ID;
	}

	uint32_t AudioComponentCreateAudioData(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& ac = ent.GetComponent<AudioComponent>();
		AudioComponent::AudioData data;
		data.m_ID = UUID32();
		data.m_LoadedInRuntime = true;
		ac.m_AudioDatas.push_back(data);
		return data.m_ID;
	}

	void AudioComponentRemoveAudioData(UUID entityId, UUID32 audioDataId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& ac = ent.GetComponent<AudioComponent>();
		for (size_t i = 0; i < ac.m_AudioDatas.size(); ++i)
		{
			if (ac.m_AudioDatas[i].m_ID == audioDataId && ac.m_AudioDatas[i].m_LoadedInRuntime)
			{
				ac.m_AudioDatas.erase(ac.m_AudioDatas.begin() + static_cast<std::ptrdiff_t>(i));
				break;
			}
		}
	}

	template<class... Component>
	void RegisterComponent()
	{
		([]()
			{
				std::string managedTypeName = std::string("Whip.") + Component::ScriptStructName;

				MonoType* managedType = mono_reflection_type_from_name(managedTypeName.data(), AssemblyManager::GetCoreAssemblyImage());
				if (!managedType)
				{
					WHP_CORE_ERROR("[Script Engine] Could not find component type {0}", managedTypeName);
					return;
				}
				s_EntityHasComponentFuncs[managedType] = [](Entity ent) { return ent.HasComponent<Component>(); };
			}(), ...);
	}

	template<class... Component>
	void RegisterComponent(ComponentGroup<Component...>)
	{
		RegisterComponent<Component...>();
	}
}

void ScriptGlue::RegisterComponents()
{
	s_EntityHasComponentFuncs.clear();
	RegisterComponent(AllComponentsNoIDNoTagNoScript{});
}

void ScriptGlue::RegisterFunctions()
{
	//NOLINTBEGIN(clang-diagnostic-microsoft-cast)

	// entity
	ADD_INTERNAL_CALL(GetScriptInstance, GetScriptInstance);
	ADD_INTERNAL_CALL(Entity_HasComponent, EntityHasComponent);
	ADD_INTERNAL_CALL(Entity_FindEntityByName, EntityFindEntityByName);

	// Logger
	ADD_INTERNAL_CALL(Logger_InternalLog, LoggerInternalLog);
	ADD_INTERNAL_CALL(Logger_InternalAssert, LoggerInternalAssert);
	ADD_INTERNAL_CALL(Logger_SetLogger, LoggerSetLogger);
	ADD_INTERNAL_CALL(Logger_PrintLog, LoggerPrintLog);
	ADD_INTERNAL_CALL(Logger_PrintLogNamed, LoggerPrintLogNamed);

	// timer
	ADD_INTERNAL_CALL(Timer_WaitFor, TimerWaitFor);
	ADD_INTERNAL_CALL(Timer_SetTimeout, TimerSetTimeout);
	ADD_INTERNAL_CALL(Timer_SetInterval, TimerSetInterval);
	ADD_INTERNAL_CALL(Timer_PauseTimer, TimerPauseTimer);
	ADD_INTERNAL_CALL(Timer_ResumeTimer, TimerResumeTimer);
	ADD_INTERNAL_CALL(Timer_StopTimer, TimerStopTimer);
	ADD_INTERNAL_CALL(Timer_Clear, TimerClear);
	ADD_INTERNAL_CALL(Timer_Exists, TimerExists);

	// Asset manager
	ADD_INTERNAL_CALL(AssetManager_ImportAsset, AssetManagerImportAsset);
	ADD_INTERNAL_CALL(AssetManager_DeleteAsset, AssetManagerDeleteAsset);
	ADD_INTERNAL_CALL(AssetManager_IsAssetHandleValid, AssetManagerIsAssetHandleValid);
	ADD_INTERNAL_CALL(AssetManager_IsAssetLoaded, AssetManagerIsAsSetLoaded);
	ADD_INTERNAL_CALL(AssetManager_GetAssetType, AssetManagerGetAssetType);
	ADD_INTERNAL_CALL(AssetManager_GetFilepath, AssetManagerGetFilepath);

	// scene manager
	ADD_INTERNAL_CALL(SceneManager_LoadScene, SceneManagerLoadScene);
	ADD_INTERNAL_CALL(SceneManager_LoadSceneByName, SceneManagerLoadSceneByName);
	ADD_INTERNAL_CALL(SceneManager_FindSceneByName, SceneManagerFindSceneByName);
	ADD_INTERNAL_CALL(SceneManager_LoadStartScene, SceneManagerLoadStartScene);
	ADD_INTERNAL_CALL(SceneManager_ReloadScene, SceneManagerReloadScene);
	ADD_INTERNAL_CALL(SceneManager_UnloadScene, SceneManagerUnloadScene);
	ADD_INTERNAL_CALL(SceneManager_GetActiveSceneHandle, SceneManagerGetActiveSceneHandle);

	// Input
	ADD_INTERNAL_CALL(Input_IsKeyDown, InputIsKeyDown);
	ADD_INTERNAL_CALL(Input_IsKeyUp, InputIsKeyUp);
	ADD_INTERNAL_CALL(Input_IsKeyPressed, InputIsKeyPressed);
	ADD_INTERNAL_CALL(Input_IsKeyReleased, InputIsKeyReleased);
	ADD_INTERNAL_CALL(Input_IsMouseButtonDown, InputIsMouseButtonDown);
	ADD_INTERNAL_CALL(Input_IsMouseButtonUp, InputIsMouseButtonUp);
	ADD_INTERNAL_CALL(Input_IsMouseButtonPressed, InputIsMouseButtonPressed);
	ADD_INTERNAL_CALL(Input_IsMouseButtonReleased, InputIsMouseButtonReleased);
	ADD_INTERNAL_CALL(Input_GetMouseX, InputGetMouseX);
	ADD_INTERNAL_CALL(Input_GetMouseY, InputGetMouseY);
	ADD_INTERNAL_CALL(Input_GetMousePosition, InputGetMousePosition);

	// audio data
	ADD_INTERNAL_CALL(AD_IsValid, ADIsValid);
	ADD_INTERNAL_CALL(AD_GetAudioHandle, ADGetAudioHandle);
	ADD_INTERNAL_CALL(AD_SetAudioHandle, ADSetAudioHandle);
	ADD_INTERNAL_CALL(AD_GetTag, ADGetTag);
	ADD_INTERNAL_CALL(AD_SetTag, ADSetTag);
	ADD_INTERNAL_CALL(AD_GetTranslation, ADGetTranslation);
	ADD_INTERNAL_CALL(AD_SetTranslation, ADSetTranslation);
	ADD_INTERNAL_CALL(ADIsSpitial, ADIsSpitial);
	ADD_INTERNAL_CALL(ADSetSpitial, ADSetSpitial);
	ADD_INTERNAL_CALL(ADIsLoop, ADIsLoop);
	ADD_INTERNAL_CALL(ADSetLoop, ADSetLoop);
	ADD_INTERNAL_CALL(ADGetGain, ADGetGain);
	ADD_INTERNAL_CALL(ADSetGain, ADSetGain);
	ADD_INTERNAL_CALL(AD_GetPitch, ADGetPitch);
	ADD_INTERNAL_CALL(ADSetPitch, ADSetPitch);
	ADD_INTERNAL_CALL(AD_GetFullClipLength, ADGetFullClipLength);
	ADD_INTERNAL_CALL(AD_GetClipStart, ADGetClipStart);
	ADD_INTERNAL_CALL(AD_SetClipStart, ADSetClipStart);
	ADD_INTERNAL_CALL(AD_GetClipEnd, ADGetClipEnd);
	ADD_INTERNAL_CALL(AD_SetClipEnd, ADSetClipEnd);

	// audio Engine or Manager
	ADD_INTERNAL_CALL(AudioEngine_UpdatePosition, AudioEngineUpdatePosition);

	// Animation
	ADD_INTERNAL_CALL(Animation_GetAnimationByName, AnimationGetAnimByTag);
	ADD_INTERNAL_CALL(Animation_IsValid, AnimationIsValid);
	ADD_INTERNAL_CALL(Animation_Bound, AnimationBound);
	ADD_INTERNAL_CALL(Animation_Unbound, AnimationUnbound);
	ADD_INTERNAL_CALL(Animation_Play, AnimationPlay);
	ADD_INTERNAL_CALL(Animation_Stop, AnimationStop);
	ADD_INTERNAL_CALL(Animation_Pause, AnimationPause);
	ADD_INTERNAL_CALL(Animation_Resume, AnimationResume);
	ADD_INTERNAL_CALL(Animation_IsPlaying, AnimationIsPlaying);
	ADD_INTERNAL_CALL(Animation_IsPaused, AnimationIsPaused);
	ADD_INTERNAL_CALL(Animation_IsLooping, AnimationIsLooping);
	ADD_INTERNAL_CALL(Animation_SetLoop, AnimationSetLoop);

	// Animator component
	ADD_INTERNAL_CALL(AnimatorComponent_GetController, AnimatorComponentGetController);
	ADD_INTERNAL_CALL(AnimatorComponent_SetController, AnimatorComponentSetController);
	ADD_INTERNAL_CALL(AnimatorComponent_GetSpeed, AnimatorComponentGetSpeed);
	ADD_INTERNAL_CALL(AnimatorComponent_SetSpeed, AnimatorComponentSetSpeed);
	ADD_INTERNAL_CALL(AnimatorComponent_Play, AnimatorComponentPlay);
	ADD_INTERNAL_CALL(AnimatorComponent_Stop, AnimatorComponentStop);
	ADD_INTERNAL_CALL(AnimatorComponent_IsPlaying, AnimatorComponentIsPlaying);
	ADD_INTERNAL_CALL(AnimatorComponent_GetCurrentState, AnimatorComponentGetCurrentState);
	ADD_INTERNAL_CALL(AnimatorComponent_SetBool, AnimatorComponentSetBool);
	ADD_INTERNAL_CALL(AnimatorComponent_SetInt, AnimatorComponentSetInt);
	ADD_INTERNAL_CALL(AnimatorComponent_SetFloat, AnimatorComponentSetFloat);
	ADD_INTERNAL_CALL(AnimatorComponent_SetTrigger, AnimatorComponentSetTrigger);
	ADD_INTERNAL_CALL(AnimatorComponent_ResetTrigger, AnimatorComponentResetTrigger);
	ADD_INTERNAL_CALL(AnimatorComponent_GetBool, AnimatorComponentGetBool);
	ADD_INTERNAL_CALL(AnimatorComponent_GetInt, AnimatorComponentGetInt);
	ADD_INTERNAL_CALL(AnimatorComponent_GetFloat, AnimatorComponentGetFloat);
	ADD_INTERNAL_CALL(AnimatorComponent_IsTriggerSet, AnimatorComponentIsTriggerSet);

	// Camera component
	ADD_INTERNAL_CALL(CameraComponent_IsPrimary, CameraComponentIsPrimary);
	ADD_INTERNAL_CALL(CameraComponent_SetPrimary, CameraComponentSetPrimary);
	ADD_INTERNAL_CALL(CameraComponent_IsFixedAspectRatio, CameraComponentIsFixedAspectRatio);
	ADD_INTERNAL_CALL(CameraComponent_SetFixedAspectRatio, CameraComponentSetFixedAspectRatio);
	ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveVerticalFOV, CameraComponentGetPerspectiveVerticalFOV);
	ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveVerticalFOV, CameraComponentSetPerspectiveVerticalFOV);
	ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveNearClip, CameraComponentGetPerspectiveNearClip);
	ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveNearClip, CameraComponentSetPerspectiveNearClip);
	ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveFarClip, CameraComponentGetPerspectiveFarClip);
	ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveFarClip, CameraComponentSetPerspectiveFarClip);
	ADD_INTERNAL_CALL(CameraComponent_GetOrthographicSize, CameraComponentGetOrthographicSize);
	ADD_INTERNAL_CALL(CameraComponent_SetOrthographicSize, CameraComponentSetOrthographicSize);
	ADD_INTERNAL_CALL(CameraComponent_GetOrthographicNearClip, CameraComponentGetOrthographicNearClip);
	ADD_INTERNAL_CALL(CameraComponent_SetOrthographicNearClip, CameraComponentSetOrthographicNearClip);
	ADD_INTERNAL_CALL(CameraComponent_GetOrthographicFarClip, CameraComponentGetOrthographicFarClip);
	ADD_INTERNAL_CALL(CameraComponent_SetOrthographicFarClip, CameraComponentSetOrthographicFarClip);

	// transform component
	ADD_INTERNAL_CALL(TransformComponent_GetTranslation, TransformComponentGetTranslation);
	ADD_INTERNAL_CALL(TransformComponent_SetTranslation, TransformComponentSetTranslation);
	ADD_INTERNAL_CALL(TransformComponent_GetRotation, TransformComponentGetRotation);
	ADD_INTERNAL_CALL(TransformComponent_SetRotation, TransformComponentSetRotation);
	ADD_INTERNAL_CALL(TransformComponent_GetScale, TransformComponentGetScale);
	ADD_INTERNAL_CALL(TransformComponent_SetScale, TransformComponentSetScale);

	// text component
	ADD_INTERNAL_CALL(TextComponent_GetText, TextComponentGetData);
	ADD_INTERNAL_CALL(TextComponent_SetText, TextComponentSetData);
	ADD_INTERNAL_CALL(TextComponent_GetColor, TextComponentGetColor);
	ADD_INTERNAL_CALL(TextComponent_SetColor, TextComponentSetColor);
	ADD_INTERNAL_CALL(TextComponent_GetKerning, TextComponentGetKerning);
	ADD_INTERNAL_CALL(TextComponent_SetKerning, TextComponentSetKerning);
	ADD_INTERNAL_CALL(TextComponent_GetLineSpacing, TextComponentGetLineSpacing);
	ADD_INTERNAL_CALL(TextComponent_SetLineSpacing, TextComponentSetLineSpacing);

	// rigidbody2D component
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyForce, Rigidbody2DComponentApplyForce);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyForceToCenter, Rigidbody2DComponentApplyForceToCenter);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulse, Rigidbody2DComponentApplyLinearImpulse);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulseToCenter, Rigidbody2DComponentApplyLinearImpulseToCenter);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyAngularImpulse, Rigidbody2DComponentApplyAngularImpulse);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyTorque, Rigidbody2DComponentApplyTorque);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetLinearVelocity, Rigidbody2DComponentGetLinearVelocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetLinearVelocity, Rigidbody2DComponentSetLinearVelocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetAngularVelocity, Rigidbody2DComponentGetAngularVelocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetAngularVelocity, Rigidbody2DComponentSetAngularVelocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetType, Rigidbody2DComponentGetType);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetType, Rigidbody2DComponentSetType);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_IsFixedRotation, Rigidbody2DComponentIsFixedRotation);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetFixedRotation, Rigidbody2DComponentSetFixedRotation);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetGravityScale, Rigidbody2DComponentGetGravityScale);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetGravityScale, Rigidbody2DComponentSetGravityScale);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_IsEnabled, Rigidbody2DComponentIsEnabled);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetEnabled, Rigidbody2DComponentSetEnabled);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_IsAwake, Rigidbody2DComponentIsAwake);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetAwake, Rigidbody2DComponentSetAwake);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetAngle, Rigidbody2DComponentGetAngle);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetMass, Rigidbody2DComponentGetMass);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetIntertia, Rigidbody2DComponentGetIntertia);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetTransform, Rigidbody2DComponentSetTransform);

	// box_collider2D_component
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetOffset, BoxCollider2DComponentGetOffset);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetOffset, BoxCollider2DComponentSetOffset);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetSize, BoxCollider2DComponentGetSize);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetSize, BoxCollider2DComponentSetSize);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetTag, BoxCollider2DComponentGetTag);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetTag, BoxCollider2DComponentSetTag);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetDensity, BoxCollider2DComponentGetDensity);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetDensity, BoxCollider2DComponentSetDensity);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetFriction, BoxCollider2DComponentGetFriction);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetFriction, BoxCollider2DComponentSetFriction);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetRestitution, BoxCollider2DComponentGetRestitution);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetRestitution, BoxCollider2DComponentSetRestitution);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetRestitutionThreshold, BoxCollider2DComponentGetRestitutionThreshold);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetRestitutionThreshold, BoxCollider2DComponentSetRestitutionThreshold);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_IsSensor, BoxCollider2DComponentIsSensor);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetSensor, BoxCollider2DComponentSetSensor);

	// circle_collider2D_component
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetOffset, CircleCollider2DComponentGetOffset);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetOffset, CircleCollider2DComponentSetOffset);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetSize, CircleCollider2DComponentGetRadius);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetSize, CircleCollider2DComponentSetRadius);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetTag, CircleCollider2DComponentGetTag);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetTag, CircleCollider2DComponentSetTag);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetDensity, CircleCollider2DComponentGetDensity);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetDensity, CircleCollider2DComponentSetDensity);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetFriction, CircleCollider2DComponentGetFriction);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetFriction, CircleCollider2DComponentSetFriction);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetRestitution, CircleCollider2DComponentGetRestitution);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetRestitution, CircleCollider2DComponentSetRestitution);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetRestitutionThreshold, CircleCollider2DComponentGetRestitutionThreshold);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetRestitutionThreshold, CircleCollider2DComponentSetRestitutionThreshold);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_IsSensor, CircleCollider2DComponentIsSensor);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetSensor, CircleCollider2DComponentSetSensor);

	// sprite_renderer_component
	ADD_INTERNAL_CALL(SpriteRendererComponent_GetColor, SpriteRendererComponentGetColor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_SetColor, SpriteRendererComponentSetColor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_GetTilingFactor, SpriteRendererComponentGetTilingFactor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_SetTilingFactor, SpriteRendererComponentSetTilingFactor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_GetTextureHandle, SpriteRendererComponentGetTextureHandle);
	ADD_INTERNAL_CALL(SpriteRendererComponent_SetTextureHandle, SpriteRendererComponentSetTextureHandle);
	// todo Texture jobs

	// circle_renderer_component
	ADD_INTERNAL_CALL(CircleRendererComponent_GetColor, CircleRendererComponentGetColor);
	ADD_INTERNAL_CALL(CircleRendererComponent_SetColor, CircleRendererComponentSetColor);
	ADD_INTERNAL_CALL(CircleRendererComponent_GetThickness, CircleRendererComponentGetThickness);
	ADD_INTERNAL_CALL(CircleRendererComponent_SetThickness, CircleRendererComponentSetThickness);
	ADD_INTERNAL_CALL(CircleRendererComponent_GetFade, CircleRendererComponentGetFade);
	ADD_INTERNAL_CALL(CircleRendererComponent_SetFade, CircleRendererComponentSetFade);

	// audio_component
	ADD_INTERNAL_CALL(AudioComponent_GetADCount, AudioComponentGetADCount);
	ADD_INTERNAL_CALL(AudioComponent_GetAD, AudioComponentGetAD);
	ADD_INTERNAL_CALL(AudioComponent_CreateAudioData, AudioComponentCreateAudioData);
	ADD_INTERNAL_CALL(AudioComponent_RemoveAudioData, AudioComponentRemoveAudioData);

	//NOLINTEND(clang-diagnostic-microsoft-cast)
}

void ScriptGlue::OnRuntimeStart()
{
}

void ScriptGlue::OnRuntimeStop()
{
	TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Clear();
	TimerManager::Get().GetGroupMap(ApplicationMode::Runtime).Clear();
}

_WHIP_END
