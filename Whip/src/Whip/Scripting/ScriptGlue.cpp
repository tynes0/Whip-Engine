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

#include <Whip/Asset/AssetImporter.h>
#include <Whip/Asset/AssetManager.h>

#include <Whip/Audio/AudioEngine.h>
#include <Whip/Animation/AnimationManager.h>

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

static constexpr const char* RuntimeTimersGroupId = "0177f1a8-04e5-4340-a771-52fc1aac9440";
static std::unordered_map<MonoType*, std::function<bool(Entity)>> s_EntityHasComponentFuncs;
static Logger s_Logger;

namespace Utils
{
	namespace detail
	{
		static Scene* GetScene()
		{
			Scene* scne = ScriptEngine::GetSceneContext(); 
			WHP_CORE_ASSERT(scne);
			return scne;
		}

		static Entity GetEntity(UUID id)
		{
			Scene* scne = GetScene();
			Entity ent = scne->FindEntityByUUID(id);
			WHP_CORE_ASSERT(ent);
			return ent;
		}

		static std::string MonoStringToString(MonoString* string)
		{
			char* cstr = mono_string_to_utf8(string);
			std::string str(cstr);
			mono_free(cstr);
			return str;
		}

		static std::wstring MonoStringToWstring(MonoString* string)
		{
			wchar_t* cstr = (wchar_t*)mono_string_to_utf16(string);
			std::wstring wstr(cstr);
			mono_free(cstr);
			return wstr;
		}

		static AudioComponent::AudioData* FindAcAD(std::vector<AudioComponent::AudioData>& handleList, UUID32 id)
		{
			for (AudioComponent::AudioData& handle : handleList)
				if (handle.m_ID == id)
					return &handle;
			return nullptr;
		}

		static AudioComponent::AudioData* FindAcAD(std::vector<AudioComponent::AudioData>& handleList, const std::string& tag)
		{
			for (AudioComponent::AudioData& handle : handleList)
				if (handle.m_Tag == tag)
					return &handle;
			return nullptr;
		}

		static b2Body* GetBody(UUID id)
		{
			Entity ent = GetEntity(id);
			auto& rb2d = ent.GetComponent<Rigidbody2DComponent>();
			b2Body* body = (b2Body*)rb2d.m_RuntimeBody;
			WHP_CORE_ASSERT(body);
			return body;
		}

		static AudioComponent::AudioData* GetAudioData(UUID id, UUID32 adId)
		{
			Entity ent = GetEntity(id);
			AudioComponent& ac = ent.GetComponent<AudioComponent>();
			AudioComponent::AudioData* audioData = detail::FindAcAD(ac.m_AudioDatas, adId);
			return audioData;
		}

		static AudioComponent::AudioData* GetAudioData(UUID id, const std::string& tag)
		{
			Entity ent = GetEntity(id);
			AudioComponent& ac = ent.GetComponent<AudioComponent>();
			AudioComponent::AudioData* audioData = detail::FindAcAD(ac.m_AudioDatas, tag);
			return audioData;
		}

		static Ref<Animation2D> GetAnimation(UUID handle)
		{
			return std::static_pointer_cast<Animation2D>(Project::GetActive()->GetRuntimeAssetManager()->GetAsset(handle));
		}
	}

	static MonoObject* GetScriptInstance(UUID entityId)
	{
		return ScriptEngine::GetManagedInstance(entityId);
	}

	static bool EntityHasComponent(UUID entityId, MonoReflectionType* componentType)
	{
		Entity ent = detail::GetEntity(entityId);

		MonoType* managedType = mono_reflection_type_get_type(componentType);
		WHP_CORE_ASSERT(s_EntityHasComponentFuncs.find(managedType) != s_EntityHasComponentFuncs.end());
		return s_EntityHasComponentFuncs.at(managedType)(ent);
	}

	static uint64_t EntityFindEntityByName(MonoString* name)
	{
		Scene* scne = detail::GetScene();
		Entity ent = scne->FindEntityByName(detail::MonoStringToString(name));
		if (!ent)
			return 0;
		return ent.GetUUID();
	}

	static void LoggerInternalLog(MonoString* logMessage, Log::Level level)
	{
		Log::GetCoreLogger()->log(Log::WhipLogLevelToSpdlogLevel(level), detail::MonoStringToString(logMessage));
	}

	static void LoggerInternalAssert(bool cond, MonoString* logMessage, MonoString* filepath, int line)
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

	static void LoggerSetLogger(MonoString* loggerName)
	{
		Log::ResetLogger(s_Logger, detail::MonoStringToString(loggerName), Log::OutputTarget::Editor);
	}

	static void LoggerPrintLog(MonoString* logMessage, Log::Level level)
	{
		s_Logger->log(Log::WhipLogLevelToSpdlogLevel(level), detail::MonoStringToString(logMessage));
		EditorLog::FileShouldReset().store(true);
	}

	static void LoggerPrintLogNamed(MonoString* loggerName, MonoString* logMessage, Log::Level level)
	{
		std::string name = s_Logger->name();
		LoggerSetLogger(loggerName);
		LoggerPrintLog(logMessage, level);
		Log::ResetLogger(s_Logger, name);
	}

	static bool TimerWaitFor(UUID tag, float ms)
	{
		TimerId id = 0;
		bool result = TimerManager::Get().WaitFor(tag, ms, 0, &id);
		if (id != 0)
			TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Add(id);
		return result;
	}

	static uint64_t TimerSetTimeout(MonoObject* func, float delayMs, MonoObject* userData)
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

	static uint64_t TimerSetInterval(MonoObject* func, float intervalMs, MonoObject* userData)
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

	static void TimerPauseTimer(uint64_t timerId)
	{
		if(TimerManager::Get().GetGroupMap().Get(RuntimeTimersGroupId).Exists(timerId))
			TimerManager::Get().PauseTimer(timerId);
	}

	static void TimerResumeTimer(uint64_t timerId)
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
		return CreateString(path.c_str());
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
			return CreateString("");
		}
		return CreateString(audioData->m_Tag.c_str());
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

	static float CameraComponentGetPerspectiveVerticalFOV(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetPerspectiveVerticalFOV();
	}

	static void CameraComponentSetPerspectiveNearClip(UUID entityId, float perspectiveNearClip)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetPerspectiveNearClip(perspectiveNearClip);
	}

	static float CameraComponentGetPerspectiveNearClip(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetPerspectiveNearClip();
	}

	static void CameraComponentSetPerspectiveFarClip(UUID entityId, float perspectiveFarClip)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetPerspectiveFarClip(perspectiveFarClip);
	}

	static float CameraComponentGetPerspectiveFarClip(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetPerspectiveFarClip();
	}

	static void CameraComponentSetOrthographicSize(UUID entityId, float orthographicSize)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetOrthographicSize(orthographicSize);
	}

	static float CameraComponentGetOrthographicSize(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetOrthographicSize();
	}

	static void CameraComponentSetOrthographicNearClip(UUID entityId, float orthographicNearClip)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetOrthographicNearClip(orthographicNearClip);
	}

	static float CameraComponentGetOrthographicNearClip(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetOrthographicNearClip();
	}

	static void CameraComponentSetOrthographicFarClip(UUID entityId, float orthographicFarClip)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<CameraComponent>().m_Camera.SetOrthographicFarClip(orthographicFarClip);
	}

	static float CameraComponentGetOrthographicFarClip(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		return ent.GetComponent<CameraComponent>().m_Camera.GetOrthographicFarClip();
	}

	static void TransformComponentGetTranslation(UUID entityId, glm::vec3* outTranslation)
	{
		Entity ent = detail::GetEntity(entityId);
		*outTranslation = ent.GetComponent<TransformComponent>().m_Translation;
	}

	static void TransformComponentSetTranslation(UUID entityId, glm::vec3* translation)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<TransformComponent>().m_Translation = *translation;
	}

	static void TransformComponentGetRotation(UUID entityId, glm::vec3* outRotation)
	{
		Entity ent = detail::GetEntity(entityId);
		*outRotation = glm::degrees(ent.GetComponent<TransformComponent>().m_Rotation);
	}

	static void TransformComponentSetRotation(UUID entityId, glm::vec3* rotation)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<TransformComponent>().m_Rotation = glm::radians(*rotation);
	}

	static void TransformComponentGetScale(UUID entityId, glm::vec3* outScale)
	{
		Entity ent = detail::GetEntity(entityId);
		*outScale = ent.GetComponent<TransformComponent>().m_Scale;
	}

	static void TransformComponentSetScale(UUID entityId, glm::vec3* scale)
	{
		Entity ent = detail::GetEntity(entityId);
		ent.GetComponent<TransformComponent>().m_Scale = *scale;
	}

	static void Rigidbody2DComponentApplyForce(UUID entityId, glm::vec2* force, glm::vec2* point, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyForce(b2Vec2(force->x, force->y), b2Vec2(point->x, point->y), wake);
	}

	static void Rigidbody2DComponentApplyForceToCenter(UUID entityId, glm::vec2* force, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyForceToCenter(b2Vec2(force->x, force->y), wake);
	}

	static void Rigidbody2DComponentApplyLinearImpulse(UUID entityId, glm::vec2* impulse, glm::vec2* point, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyLinearImpulse(b2Vec2(impulse->x, impulse->y), b2Vec2(point->x, point->y), wake);
	}

	static void Rigidbody2DComponentApplyLinearImpulseToCenter(UUID entityId, glm::vec2* impulse, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyLinearImpulseToCenter(b2Vec2(impulse->x, impulse->y), wake);
	}

	static void Rigidbody2DComponentApplyAngularImpulse(UUID entityId, float impulse, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyAngularImpulse(impulse, wake);
	}

	static void Rigidbody2DComponentApplyTorque(UUID entityId, float torque, bool wake)
	{
		auto* body = detail::GetBody(entityId);
		body->ApplyTorque(torque, wake);
	}

	static void Rigidbody2DComponentGetLinearVelocity(UUID entityId, glm::vec2* outLinearVelocity)
	{
		auto* body = detail::GetBody(entityId);
		const b2Vec2& linearVelocity = body->GetLinearVelocity();
		*outLinearVelocity = glm::vec2(linearVelocity.x, linearVelocity.y);
	}

	static void Rigidbody2DComponentSetLinearVelocity(UUID entityId, glm::vec2* linearVelocity)
	{
		auto* body = detail::GetBody(entityId);
		body->SetLinearVelocity(b2Vec2(linearVelocity->x, linearVelocity->y));
	}

	static float Rigidbody2DComponentGetAngularVelocity(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetAngularVelocity();
	}

	static void Rigidbody2DComponentSetAngularVelocity(UUID entityId, float angularVelocity)
	{
		auto* body = detail::GetBody(entityId);
		body->SetAngularVelocity(angularVelocity);
	}

	static Rigidbody2DComponent::BodyType Rigidbody2DComponentGetType(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return Physics2D::Rigidbody2DTypeFromBox2DBody(body->GetType());
	}

	static void Rigidbody2DComponentSetType(UUID entityId, Rigidbody2DComponent::BodyType bodyType)
	{
		auto* body = detail::GetBody(entityId);
		body->SetType(Physics2D::Rigidbody2DTypeToBox2DBody(bodyType));
	}

	static bool Rigidbody2DComponentIsFixedRotation(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->IsFixedRotation();
	}

	static void Rigidbody2DComponentSetFixedRotation(UUID entityId, bool fixed)
	{
		auto* body = detail::GetBody(entityId);
		body->SetFixedRotation(fixed);
	}

	static float Rigidbody2DComponentGetGravityScale(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetGravityScale();
	}

	static void Rigidbody2DComponentSetGravityScale(UUID entityId, float scale)
	{
		auto* body = detail::GetBody(entityId);
		body->SetGravityScale(scale);
	}

	static bool Rigidbody2DComponentIsEnabled(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->IsEnabled();
	}

	static void Rigidbody2DComponentSetEnabled(UUID entityId, bool enabled)
	{
		auto* body = detail::GetBody(entityId);
		body->SetEnabled(enabled);
	}

	static bool Rigidbody2DComponentIsAwake(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->IsAwake();
	}

	static void Rigidbody2DComponentSetAwake(UUID entityId, bool awake)
	{
		auto* body = detail::GetBody(entityId);
		body->SetAwake(awake);
	}

	static float Rigidbody2DComponentGetAngle(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetAngle();
	}

	static float Rigidbody2DComponentGetMass(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetMass();
	}

	static float Rigidbody2DComponentGetIntertia(UUID entityId)
	{
		auto* body = detail::GetBody(entityId);
		return body->GetInertia();
	}

	static void Rigidbody2DComponentSetTransform(UUID entityId, glm::vec2* position, float angle)
	{
		auto* body = detail::GetBody(entityId);
		body->SetTransform(b2Vec2(position->x, position->y), angle);
	}

	static MonoString* TextComponentGetData(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		return CreateString(tc.m_TextString.c_str());
	}

	static void TextComponentSetData(UUID entityId, MonoString* textString)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		tc.m_TextString = detail::MonoStringToString(textString);
	}

	static void TextComponentGetColor(UUID entityId, glm::vec4* color)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		*color = tc.m_Color;
	}

	static void TextComponentSetColor(UUID entityId, glm::vec4* color)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		tc.m_Color = *color;
	}

	static float TextComponentGetKerning(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		return tc.m_Kerning;
	}

	static void TextComponentSetKerning(UUID entityId, float kerning)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		tc.m_Kerning = kerning;
	}

	static float TextComponentGetLineSpacing(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		return tc.m_LineSpacing;
	}

	static void TextComponentSetLineSpacing(UUID entityId, float lineSpacing)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& tc = ent.GetComponent<TextComponent>();
		tc.m_LineSpacing = lineSpacing;
	}

	static void BoxCollider2DComponentGetOffset(UUID entityId, glm::vec2* offset)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		*offset = bc2dc.m_Offset;
	}

	static void BoxCollider2DComponentSetOffset(UUID entityId, glm::vec2* offset)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Offset = *offset;
	}

	static void BoxCollider2DComponentGetSize(UUID entityId, glm::vec2* size)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		*size = bc2dc.m_Size;
	}

	static void BoxCollider2DComponentSetSize(UUID entityId, glm::vec2* size)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Size = *size;
	}

	static MonoString* BoxCollider2DComponentGetTag(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return CreateString(bc2dc.m_Tag.c_str());
	}

	static void BoxCollider2DComponentSetTag(UUID entityId, MonoString* tag)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Tag = detail::MonoStringToString(tag);
	}

	static float BoxCollider2DComponentGetDensity(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_Density;
	}

	static void BoxCollider2DComponentSetDensity(UUID entityId, float density)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Density = density;
	}

	static float BoxCollider2DComponentGetFriction(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_Friction;
	}

	static void BoxCollider2DComponentSetFriction(UUID entityId, float friction)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Friction = friction;
	}

	static float BoxCollider2DComponentGetRestitution(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_Restitution;
	}

	static void BoxCollider2DComponentSetRestitution(UUID entityId, float restitution)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Restitution = restitution;
	}

	static float BoxCollider2DComponentGetRestitutionThreshold(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_RestitutionThreshold;
	}

	static void BoxCollider2DComponentSetRestitutionThreshold(UUID entityId, float restitutionThreshold)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_RestitutionThreshold = restitutionThreshold;
	}

	static float BoxCollider2DComponentIsSensor(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		return bc2dc.m_Sensor;
	}

	static void BoxCollider2DComponentSetSensor(UUID entityId, bool sensor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& bc2dc = ent.GetComponent<BoxCollider2DComponent>();
		bc2dc.m_Sensor = sensor;
	}

	static void CircleCollider2DComponentGetOffset(UUID entityId, glm::vec2* offset)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		*offset = cc2dc.m_Offset;
	}

	static void CircleCollider2DComponentSetOffset(UUID entityId, glm::vec2* offset)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Offset = *offset;
	}

	static float CircleCollider2DComponentGetRadius(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Radius;
	}

	static void CircleCollider2DComponentSetRadius(UUID entityId, float radius)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Radius = radius;
	}

	static MonoString* CircleCollider2DComponentGetTag(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return CreateString(cc2dc.m_Tag.c_str());
	}

	static void CircleCollider2DComponentSetTag(UUID entityId, MonoString* tag)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Tag = detail::MonoStringToString(tag);
	}

	static float CircleCollider2DComponentGetDensity(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Density;
	}

	static void CircleCollider2DComponentSetDensity(UUID entityId, float density)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Density = density;
	}

	static float CircleCollider2DComponentGetFriction(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Friction;
	}

	static void CircleCollider2DComponentSetFriction(UUID entityId, float friction)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Friction = friction;
	}

	static float CircleCollider2DComponentGetRestitution(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Restitution;
	}

	static void CircleCollider2DComponentSetRestitution(UUID entityId, float restitution)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Restitution = restitution;
	}

	static float CircleCollider2DComponentGetRestitutionThreshold(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_RestitutionThreshold;
	}

	static void CircleCollider2DComponentSetRestitutionThreshold(UUID entityId, float restitutionThreshold)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_RestitutionThreshold = restitutionThreshold;
	}

	static float CircleCollider2DComponentIsSensor(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		return cc2dc.m_Sensor;
	}

	static void CircleCollider2DComponentSetSensor(UUID entityId, bool sensor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& cc2dc = ent.GetComponent<CircleCollider2DComponent>();
		cc2dc.m_Sensor = sensor;
	}

	static void SpriteRendererComponentGetColor(UUID entityId, glm::vec4* outColor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		*outColor = src.m_Color;
	}

	static void SpriteRendererComponentSetColor(UUID entityId, glm::vec4* color)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		src.m_Color = *color;
	}

	static float SpriteRendererComponentGetTilingFactor(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		return src.m_TilingFactor;
	}

	static void SpriteRendererComponentSetTilingFactor(UUID entityId, float tilingFactor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		src.m_TilingFactor = tilingFactor;
	}

	static uint64_t SpriteRendererComponentGetTextureHandle(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& src = ent.GetComponent<SpriteRendererComponent>();
		return static_cast<uint64_t>(src.m_Texture);
	}

	static void SpriteRendererComponentSetTextureHandle(UUID entityId, UUID textureHandle)
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
	
	static void CircleRendererComponentGetColor(UUID entityId, glm::vec4* outColor)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		*outColor = crc.m_Color;
	}

	static void CircleRendererComponentSetColor(UUID entityId, glm::vec4* color)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		crc.m_Color = *color;
	}

	static float CircleRendererComponentGetThickness(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		return crc.m_Thickness;
	}

	static void CircleRendererComponentSetThickness(UUID entityId, float thickness)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		crc.m_Thickness = thickness;
	}

	static float CircleRendererComponentGetFade(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		return crc.m_Fade;
	}

	static void CircleRendererComponentSetFade(UUID entityId, float fade)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& crc = ent.GetComponent<CircleRendererComponent>();
		crc.m_Fade = fade;
	}

	static int AudioComponentGetADCount(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& ac = ent.GetComponent<AudioComponent>();
		return static_cast<int>(ac.m_AudioDatas.size());
	}

	static uint32_t AudioComponentGetAD(UUID entityId, int index)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& ac = ent.GetComponent<AudioComponent>();
		if (index >= ac.m_AudioDatas.size())
			return 0;
		return ac.m_AudioDatas[index].m_ID;
	}

	static uint32_t AudioComponentCreateAudioData(UUID entityId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& ac = ent.GetComponent<AudioComponent>();
		AudioComponent::AudioData data;
		data.m_ID = UUID32();
		data.m_LoadedInRuntime = true;
		ac.m_AudioDatas.push_back(data);
		return data.m_ID;
	}

	static void AudioComponentRemoveAudioData(UUID entityId, UUID32 audioDataId)
	{
		Entity ent = detail::GetEntity(entityId);
		auto& ac = ent.GetComponent<AudioComponent>();
		for (size_t i = 0; i < ac.m_AudioDatas.size(); ++i)
		{
			if (ac.m_AudioDatas[i].m_ID == audioDataId && ac.m_AudioDatas[i].m_LoadedInRuntime)
			{
				ac.m_AudioDatas.erase(ac.m_AudioDatas.begin() + i);
				break;
			}
		}
	}

	template<class... Component>
	static void RegisterComponent()
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
	static void RegisterComponent(ComponentGroup<Component...>)
	{
		RegisterComponent<Component...>();
	}
}

void ScriptGlue::RegisterComponents()
{
	s_EntityHasComponentFuncs.clear();
	Utils::RegisterComponent(AllComponentsNoIDNoTagNoScript{});
}

void ScriptGlue::RegisterFunctions()
{
	// entity
	ADD_INTERNAL_CALL(GetScriptInstance, Utils::GetScriptInstance);
	ADD_INTERNAL_CALL(Entity_HasComponent, Utils::EntityHasComponent);
	ADD_INTERNAL_CALL(Entity_FindEntityByName, Utils::EntityFindEntityByName);

	// Logger
	ADD_INTERNAL_CALL(Logger_InternalLog, Utils::LoggerInternalLog);
	ADD_INTERNAL_CALL(Logger_InternalAssert, Utils::LoggerInternalAssert);
	ADD_INTERNAL_CALL(Logger_SetLogger, Utils::LoggerSetLogger);
	ADD_INTERNAL_CALL(Logger_PrintLog, Utils::LoggerPrintLog);
	ADD_INTERNAL_CALL(Logger_PrintLogNamed, Utils::LoggerPrintLogNamed);

	// timer
	ADD_INTERNAL_CALL(Timer_WaitFor, Utils::TimerWaitFor);
	ADD_INTERNAL_CALL(Timer_SetTimeout, Utils::TimerSetTimeout);
	ADD_INTERNAL_CALL(Timer_SetInterval, Utils::TimerSetInterval);
	ADD_INTERNAL_CALL(Timer_PauseTimer, Utils::TimerPauseTimer);
	ADD_INTERNAL_CALL(Timer_ResumeTimer, Utils::TimerResumeTimer);
	ADD_INTERNAL_CALL(Timer_StopTimer, Utils::TimerStopTimer);
	ADD_INTERNAL_CALL(Timer_Clear, Utils::TimerClear);
	ADD_INTERNAL_CALL(Timer_Exists, Utils::TimerExists);

	// Asset manager
	ADD_INTERNAL_CALL(AssetManager_ImportAsset, Utils::AssetManagerImportAsset);
	ADD_INTERNAL_CALL(AssetManager_DeleteAsset, Utils::AssetManagerDeleteAsset);
	ADD_INTERNAL_CALL(AssetManager_IsAssetHandleValid, Utils::AssetManagerIsAssetHandleValid);
	ADD_INTERNAL_CALL(AssetManager_IsAssetLoaded, Utils::AssetManagerIsAsSetLoaded);
	ADD_INTERNAL_CALL(AssetManager_GetAssetType, Utils::AssetManagerGetAssetType);
	ADD_INTERNAL_CALL(AssetManager_GetFilepath, Utils::AssetManagerGetFilepath);

	// scene manager
	ADD_INTERNAL_CALL(SceneManager_LoadScene, Utils::SceneManagerLoadScene);
	ADD_INTERNAL_CALL(SceneManager_LoadSceneByName, Utils::SceneManagerLoadSceneByName);
	ADD_INTERNAL_CALL(SceneManager_FindSceneByName, Utils::SceneManagerFindSceneByName);
	ADD_INTERNAL_CALL(SceneManager_LoadStartScene, Utils::SceneManagerLoadStartScene);
	ADD_INTERNAL_CALL(SceneManager_ReloadScene, Utils::SceneManagerReloadScene);
	ADD_INTERNAL_CALL(SceneManager_UnloadScene, Utils::SceneManagerUnloadScene);
	ADD_INTERNAL_CALL(SceneManager_GetActiveSceneHandle, Utils::SceneManagerGetActiveSceneHandle);

	// Input
	ADD_INTERNAL_CALL(Input_IsKeyDown, Utils::InputIsKeyDown);
	ADD_INTERNAL_CALL(Input_IsKeyUp, Utils::InputIsKeyUp);
	ADD_INTERNAL_CALL(Input_IsKeyPressed, Utils::InputIsKeyPressed);
	ADD_INTERNAL_CALL(Input_IsKeyReleased, Utils::InputIsKeyReleased);
	ADD_INTERNAL_CALL(Input_IsMouseButtonDown, Utils::InputIsMouseButtonDown);
	ADD_INTERNAL_CALL(Input_IsMouseButtonUp, Utils::InputIsMouseButtonUp);
	ADD_INTERNAL_CALL(Input_IsMouseButtonPressed, Utils::InputIsMouseButtonPressed);
	ADD_INTERNAL_CALL(Input_IsMouseButtonReleased, Utils::InputIsMouseButtonReleased);
	ADD_INTERNAL_CALL(Input_GetMouseX, Utils::InputGetMouseX);
	ADD_INTERNAL_CALL(Input_GetMouseY, Utils::InputGetMouseY);
	ADD_INTERNAL_CALL(Input_GetMousePosition, Utils::InputGetMousePosition);

	// audio data
	ADD_INTERNAL_CALL(AD_IsValid, Utils::ADIsValid);
	ADD_INTERNAL_CALL(AD_GetAudioHandle, Utils::ADGetAudioHandle);
	ADD_INTERNAL_CALL(AD_SetAudioHandle, Utils::ADSetAudioHandle);
	ADD_INTERNAL_CALL(AD_GetTag, Utils::ADGetTag);
	ADD_INTERNAL_CALL(AD_SetTag, Utils::ADSetTag);
	ADD_INTERNAL_CALL(AD_GetTranslation, Utils::ADGetTranslation);
	ADD_INTERNAL_CALL(AD_SetTranslation, Utils::ADSetTranslation);
	ADD_INTERNAL_CALL(ADIsSpitial, Utils::ADIsSpitial);
	ADD_INTERNAL_CALL(ADSetSpitial, Utils::ADSetSpitial);
	ADD_INTERNAL_CALL(ADIsLoop, Utils::ADIsLoop);
	ADD_INTERNAL_CALL(ADSetLoop, Utils::ADSetLoop);
	ADD_INTERNAL_CALL(ADGetGain, Utils::ADGetGain);
	ADD_INTERNAL_CALL(ADSetGain, Utils::ADSetGain);
	ADD_INTERNAL_CALL(AD_GetPitch, Utils::ADGetPitch);
	ADD_INTERNAL_CALL(ADSetPitch, Utils::ADSetPitch);
	ADD_INTERNAL_CALL(AD_GetFullClipLength, Utils::ADGetFullClipLength);
	ADD_INTERNAL_CALL(AD_GetClipStart, Utils::ADGetClipStart);
	ADD_INTERNAL_CALL(AD_SetClipStart, Utils::ADSetClipStart);
	ADD_INTERNAL_CALL(AD_GetClipEnd, Utils::ADGetClipEnd);
	ADD_INTERNAL_CALL(AD_SetClipEnd, Utils::ADSetClipEnd);

	// audio Engine or Manager
	ADD_INTERNAL_CALL(AudioEngine_UpdatePosition, Utils::AudioEngineUpdatePosition);

	// Animation
	ADD_INTERNAL_CALL(Animation_GetAnimationByName, Utils::AnimationGetAnimByTag);
	ADD_INTERNAL_CALL(Animation_IsValid, Utils::AnimationIsValid);
	ADD_INTERNAL_CALL(Animation_Bound, Utils::AnimationBound);
	ADD_INTERNAL_CALL(Animation_Unbound, Utils::AnimationUnbound);
	ADD_INTERNAL_CALL(Animation_Play, Utils::AnimationPlay);
	ADD_INTERNAL_CALL(Animation_Stop, Utils::AnimationStop);
	ADD_INTERNAL_CALL(Animation_Pause, Utils::AnimationPause);
	ADD_INTERNAL_CALL(Animation_Resume, Utils::AnimationResume);
	ADD_INTERNAL_CALL(Animation_IsPlaying, Utils::AnimationIsPlaying);
	ADD_INTERNAL_CALL(Animation_IsPaused, Utils::AnimationIsPaused);
	ADD_INTERNAL_CALL(Animation_IsLooping, Utils::AnimationIsLooping);
	ADD_INTERNAL_CALL(Animation_SetLoop, Utils::AnimationSetLoop);

	// Camera component
	ADD_INTERNAL_CALL(CameraComponent_IsPrimary, Utils::CameraComponentIsPrimary);
	ADD_INTERNAL_CALL(CameraComponent_SetPrimary, Utils::CameraComponentSetPrimary);
	ADD_INTERNAL_CALL(CameraComponent_IsFixedAspectRatio, Utils::CameraComponentIsFixedAspectRatio);
	ADD_INTERNAL_CALL(CameraComponent_SetFixedAspectRatio, Utils::CameraComponentSetFixedAspectRatio);
	ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveVerticalFOV, Utils::CameraComponentGetPerspectiveVerticalFOV);
	ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveVerticalFOV, Utils::CameraComponentSetPerspectiveVerticalFOV);
	ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveNearClip, Utils::CameraComponentGetPerspectiveNearClip);
	ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveNearClip, Utils::CameraComponentSetPerspectiveNearClip);
	ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveFarClip, Utils::CameraComponentGetPerspectiveFarClip);
	ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveFarClip, Utils::CameraComponentSetPerspectiveFarClip);
	ADD_INTERNAL_CALL(CameraComponent_GetOrthographicSize, Utils::CameraComponentGetOrthographicSize);
	ADD_INTERNAL_CALL(CameraComponent_SetOrthographicSize, Utils::CameraComponentSetOrthographicSize);
	ADD_INTERNAL_CALL(CameraComponent_GetOrthographicNearClip, Utils::CameraComponentGetOrthographicNearClip);
	ADD_INTERNAL_CALL(CameraComponent_SetOrthographicNearClip, Utils::CameraComponentSetOrthographicNearClip);
	ADD_INTERNAL_CALL(CameraComponent_GetOrthographicFarClip, Utils::CameraComponentGetOrthographicFarClip);
	ADD_INTERNAL_CALL(CameraComponent_SetOrthographicFarClip, Utils::CameraComponentSetOrthographicFarClip);

	// transform component
	ADD_INTERNAL_CALL(TransformComponent_GetTranslation, Utils::TransformComponentGetTranslation);
	ADD_INTERNAL_CALL(TransformComponent_SetTranslation, Utils::TransformComponentSetTranslation);
	ADD_INTERNAL_CALL(TransformComponent_GetRotation, Utils::TransformComponentGetRotation);
	ADD_INTERNAL_CALL(TransformComponent_SetRotation, Utils::TransformComponentSetRotation);
	ADD_INTERNAL_CALL(TransformComponent_GetScale, Utils::TransformComponentGetScale);
	ADD_INTERNAL_CALL(TransformComponent_SetScale, Utils::TransformComponentSetScale);

	// text component
	ADD_INTERNAL_CALL(TextComponent_GetText, Utils::TextComponentGetData);
	ADD_INTERNAL_CALL(TextComponent_SetText, Utils::TextComponentSetData);
	ADD_INTERNAL_CALL(TextComponent_GetColor, Utils::TextComponentGetColor);
	ADD_INTERNAL_CALL(TextComponent_SetColor, Utils::TextComponentSetColor);
	ADD_INTERNAL_CALL(TextComponent_GetKerning, Utils::TextComponentGetKerning);
	ADD_INTERNAL_CALL(TextComponent_SetKerning, Utils::TextComponentSetKerning);
	ADD_INTERNAL_CALL(TextComponent_GetLineSpacing, Utils::TextComponentGetLineSpacing);
	ADD_INTERNAL_CALL(TextComponent_SetLineSpacing, Utils::TextComponentSetLineSpacing);

	// rigidbody2D component
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyForce, Utils::Rigidbody2DComponentApplyForce);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyForceToCenter, Utils::Rigidbody2DComponentApplyForceToCenter);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulse, Utils::Rigidbody2DComponentApplyLinearImpulse);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulseToCenter, Utils::Rigidbody2DComponentApplyLinearImpulseToCenter);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyAngularImpulse, Utils::Rigidbody2DComponentApplyAngularImpulse);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyTorque, Utils::Rigidbody2DComponentApplyTorque);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetLinearVelocity, Utils::Rigidbody2DComponentGetLinearVelocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetLinearVelocity, Utils::Rigidbody2DComponentSetLinearVelocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetAngularVelocity, Utils::Rigidbody2DComponentGetAngularVelocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetAngularVelocity, Utils::Rigidbody2DComponentSetAngularVelocity);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetType, Utils::Rigidbody2DComponentGetType);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetType, Utils::Rigidbody2DComponentSetType);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_IsFixedRotation, Utils::Rigidbody2DComponentIsFixedRotation);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetFixedRotation, Utils::Rigidbody2DComponentSetFixedRotation);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetGravityScale, Utils::Rigidbody2DComponentGetGravityScale);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetGravityScale, Utils::Rigidbody2DComponentSetGravityScale);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_IsEnabled, Utils::Rigidbody2DComponentIsEnabled);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetEnabled, Utils::Rigidbody2DComponentSetEnabled);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_IsAwake, Utils::Rigidbody2DComponentIsAwake);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetAwake, Utils::Rigidbody2DComponentSetAwake);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetAngle, Utils::Rigidbody2DComponentGetAngle);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetMass, Utils::Rigidbody2DComponentGetMass);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_GetIntertia, Utils::Rigidbody2DComponentGetIntertia);
	ADD_INTERNAL_CALL(Rigidbody2DComponent_SetTransform, Utils::Rigidbody2DComponentSetTransform);

	// box_collider2D_component
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetOffset, Utils::BoxCollider2DComponentGetOffset);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetOffset, Utils::BoxCollider2DComponentSetOffset);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetSize, Utils::BoxCollider2DComponentGetSize);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetSize, Utils::BoxCollider2DComponentSetSize);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetTag, Utils::BoxCollider2DComponentGetTag);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetTag, Utils::BoxCollider2DComponentSetTag);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetDensity, Utils::BoxCollider2DComponentGetDensity);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetDensity, Utils::BoxCollider2DComponentSetDensity);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetFriction, Utils::BoxCollider2DComponentGetFriction);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetFriction, Utils::BoxCollider2DComponentSetFriction);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetRestitution, Utils::BoxCollider2DComponentGetRestitution);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetRestitution, Utils::BoxCollider2DComponentSetRestitution);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_GetRestitutionThreshold, Utils::BoxCollider2DComponentGetRestitutionThreshold);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetRestitutionThreshold, Utils::BoxCollider2DComponentSetRestitutionThreshold);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_IsSensor, Utils::BoxCollider2DComponentIsSensor);
	ADD_INTERNAL_CALL(BoxCollider2DComponent_SetSensor, Utils::BoxCollider2DComponentSetSensor);

	// circle_collider2D_component
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetOffset, Utils::CircleCollider2DComponentGetOffset);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetOffset, Utils::CircleCollider2DComponentSetOffset);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetSize, Utils::CircleCollider2DComponentGetRadius);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetSize, Utils::CircleCollider2DComponentSetRadius);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetTag, Utils::CircleCollider2DComponentGetTag);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetTag, Utils::CircleCollider2DComponentSetTag);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetDensity, Utils::CircleCollider2DComponentGetDensity);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetDensity, Utils::CircleCollider2DComponentSetDensity);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetFriction, Utils::CircleCollider2DComponentGetFriction);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetFriction, Utils::CircleCollider2DComponentSetFriction);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetRestitution, Utils::CircleCollider2DComponentGetRestitution);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetRestitution, Utils::CircleCollider2DComponentSetRestitution);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_GetRestitutionThreshold, Utils::CircleCollider2DComponentGetRestitutionThreshold);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetRestitutionThreshold, Utils::CircleCollider2DComponentSetRestitutionThreshold);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_IsSensor, Utils::CircleCollider2DComponentIsSensor);
	ADD_INTERNAL_CALL(CircleCollider2DComponent_SetSensor, Utils::CircleCollider2DComponentSetSensor);

	// sprite_renderer_component
	ADD_INTERNAL_CALL(SpriteRendererComponent_GetColor, Utils::SpriteRendererComponentGetColor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_SetColor, Utils::SpriteRendererComponentSetColor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_GetTilingFactor, Utils::SpriteRendererComponentGetTilingFactor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_SetTilingFactor, Utils::SpriteRendererComponentSetTilingFactor);
	ADD_INTERNAL_CALL(SpriteRendererComponent_GetTextureHandle, Utils::SpriteRendererComponentGetTextureHandle);
	ADD_INTERNAL_CALL(SpriteRendererComponent_SetTextureHandle, Utils::SpriteRendererComponentSetTextureHandle);
	// todo Texture jobs

	// circle_renderer_component
	ADD_INTERNAL_CALL(CircleRendererComponent_GetColor, Utils::CircleRendererComponentGetColor);
	ADD_INTERNAL_CALL(CircleRendererComponent_SetColor, Utils::CircleRendererComponentSetColor);
	ADD_INTERNAL_CALL(CircleRendererComponent_GetThickness, Utils::CircleRendererComponentGetThickness);
	ADD_INTERNAL_CALL(CircleRendererComponent_SetThickness, Utils::CircleRendererComponentSetThickness);
	ADD_INTERNAL_CALL(CircleRendererComponent_GetFade, Utils::CircleRendererComponentGetFade);
	ADD_INTERNAL_CALL(CircleRendererComponent_SetFade, Utils::CircleRendererComponentSetFade);

	// audio_component
	ADD_INTERNAL_CALL(AudioComponent_GetADCount, Utils::AudioComponentGetADCount);
	ADD_INTERNAL_CALL(AudioComponent_GetAD, Utils::AudioComponentGetAD);
	ADD_INTERNAL_CALL(AudioComponent_CreateAudioData, Utils::AudioComponentCreateAudioData);
	ADD_INTERNAL_CALL(AudioComponent_RemoveAudioData, Utils::AudioComponentRemoveAudioData);
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
