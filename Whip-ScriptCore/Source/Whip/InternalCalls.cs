using System;
using System.Runtime.CompilerServices;

// implamented in C++ Whip
namespace Whip
{
	public delegate void TimerCallback(object userData);

	public static class InternalCalls
	{
		#region entity
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static object GetScriptInstance(ulong entityID);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Entity_HasComponent(ulong entityID, Type componentType);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong Entity_FindEntityByName(string name);
		#endregion

		#region AssetManager
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong AssetManager_ImportAsset(string path);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AssetManager_DeleteAsset(ulong assetHandle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool AssetManager_IsAssetHandleValid(ulong assetHandle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool AssetManager_IsAssetLoaded(ulong assetHandle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static AssetType AssetManager_GetAssetType(ulong assetHandle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string AssetManager_GetFilepath(ulong assetHandle);
		#endregion

		#region SceneManager
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool SceneManager_LoadScene(ulong sceneHandle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool SceneManager_LoadSceneByName(string sceneName);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong SceneManager_FindSceneByName(string sceneName);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool SceneManager_LoadStartScene();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool SceneManager_ReloadScene();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool SceneManager_UnloadScene();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong SceneManager_GetActiveSceneHandle();
		#endregion

		#region Input
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsKeyDown(KeyCode keycode);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsKeyUp(KeyCode keycode);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsKeyPressed(KeyCode keycode);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsKeyReleased(KeyCode keycode);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsMouseButtonDown(MouseCode button);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsMouseButtonUp(MouseCode button);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsMouseButtonPressed(MouseCode button);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsMouseButtonReleased(MouseCode button);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float Input_GetMouseX();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float Input_GetMouseY();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Input_GetMousePosition(out Vector2 position);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Input_GetMouseDelta(out Vector2 delta);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Input_GetMouseViewportPosition(out Vector2 position);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsMouseInsideViewport();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float Input_GetScrollDeltaX();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float Input_GetScrollDeltaY();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsRuntimeInputActive();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsRuntimeInputCapturedByUI();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Input_IsRuntimeGameplayInputActive();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Cursor_SetMode(int mode);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static int Cursor_GetMode();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Cursor_SetVisible(bool visible);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Cursor_IsVisible();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Cursor_SetShape(int shape);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static int Cursor_GetShape();
		#endregion

		#region AudioData
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool AD_IsValid(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong AD_GetAudioHandle(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_SetAudioHandle(ulong entityID, uint ID, ulong assetHandle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string AD_GetTag(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_SetTag(ulong entityID, uint ID, string tag);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_GetTranslation(ulong entityID, uint ID, out Vector3 translation);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_SetTranslation(ulong entityID, uint ID, ref Vector3 translation);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool AD_IsSpitial(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_SetSpitial(ulong entityID, uint ID, bool spitial);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool AD_IsLoop(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_SetLoop(ulong entityID, uint ID, bool loop);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float AD_GetGain(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_SetGain(ulong entityID, uint ID, float gain);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float AD_GetPitch(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_SetPitch(ulong entityID, uint ID, float pitch);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float AD_GetClipStart(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_SetClipStart(ulong entityID, uint ID, float clipStart);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float AD_GetClipEnd(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AD_SetClipEnd(ulong entityID, uint ID, float clipEnd);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float AD_GetFullClipLength(ulong entityID, uint ID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static uint AudioComponent_CreateAudioData(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AudioComponent_RemoveAudioData(ulong entityID, uint ID);
		#endregion

		#region Audio
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Audio_UpdatePosition(ulong entityID, uint ID, ref Vector3 newPosition);


		#endregion

		#region Logger
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Logger_InternalLog(string logMessage, Logger.LogLevel level);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Logger_InternalAssert(bool condition, string logMessage, string filepath, int line);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Logger_SetLogger(string loggerName);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Logger_PrintLog(string logMessage, Logger.LogLevel level);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Logger_PrintLogNamed(string loggerName, string logMessage, Logger.LogLevel level);
		#endregion

		#region Timer
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Timer_WaitFor(ulong tag, float delayMs);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Timer_SubmitToNextTick(TimerCallback callback);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong Timer_SetTimeout(TimerCallback callback, float delayMs, object userData);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong Timer_SetInterval(TimerCallback callback, float intervalMs, object userData);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Timer_PauseTimer(ulong id);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Timer_ResumeTimer(ulong id);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Timer_StopTimer(ulong id);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Timer_Clear();
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Timer_Exists(ulong id);

		#endregion

		#region Animation
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong Animation_GetAnimationByName(string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Animation_IsValid(ulong handle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Animation_Bound(ulong handle, ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Animation_Unbound(ulong handle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Animation_Play(ulong handle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Animation_Stop(ulong handle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Animation_Pause(ulong handle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Animation_Resume(ulong handle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Animation_IsPlaying(ulong handle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Animation_IsPaused(ulong handle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Animation_IsLooping(ulong handle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Animation_SetLoop(ulong handle, bool loop);

		#endregion

		#region AnimatorComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong AnimatorComponent_GetController(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AnimatorComponent_SetController(ulong entityID, ulong controllerHandle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float AnimatorComponent_GetSpeed(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AnimatorComponent_SetSpeed(ulong entityID, float speed);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AnimatorComponent_Play(ulong entityID, string stateName);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AnimatorComponent_Stop(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool AnimatorComponent_IsPlaying(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string AnimatorComponent_GetCurrentState(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AnimatorComponent_SetBool(ulong entityID, string name, bool value);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AnimatorComponent_SetInt(ulong entityID, string name, int value);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AnimatorComponent_SetFloat(ulong entityID, string name, float value);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AnimatorComponent_SetTrigger(ulong entityID, string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void AnimatorComponent_ResetTrigger(ulong entityID, string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool AnimatorComponent_GetBool(ulong entityID, string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static int AnimatorComponent_GetInt(ulong entityID, string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float AnimatorComponent_GetFloat(ulong entityID, string name);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool AnimatorComponent_IsTriggerSet(ulong entityID, string name);

		#endregion

		#region CameraComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool CameraComponent_IsPrimary(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CameraComponent_SetPrimary(ulong entityID, bool primary);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool CameraComponent_IsFixedAspectRatio(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CameraComponent_SetFixedAspectRatio(ulong entityID, bool fixedAspectRatio);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CameraComponent_GetPerspectiveVerticalFOV(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CameraComponent_SetPerspectiveVerticalFOV(ulong entityID, float perspectiveVertivalFOV);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CameraComponent_GetPerspectiveNearClip(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CameraComponent_SetPerspectiveNearClip(ulong entityID, float perspectiveNearClip);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CameraComponent_GetPerspectiveFarClip(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CameraComponent_SetPerspectiveFarClip(ulong entityID, float perspectiveFarClip);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CameraComponent_GetOrthographicSize(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CameraComponent_SetOrthographicSize(ulong entityID, float orthographicSize);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CameraComponent_GetOrthographicNearClip(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CameraComponent_SetOrthographicNearClip(ulong entityID, float orthographicNearClip);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CameraComponent_GetOrthographicFarClip(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CameraComponent_SetOrthographicFarClip(ulong entityID, float orthographicFarClip);


		#endregion

		#region TransformComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_GetTranslation(ulong entityID, out Vector3 translation);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_SetTranslation(ulong entityID, ref Vector3 translation);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_GetRotation(ulong entityID, out Vector3 rotation);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_SetRotation(ulong entityID, ref Vector3 rotation);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_GetScale(ulong entityID, out Vector3 scale);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TransformComponent_SetScale(ulong entityID, ref Vector3 scale);
		#endregion

		#region Rigidbody2DComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_ApplyForce(ulong entityID, ref Vector2 force, ref Vector2 point, bool wake);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_ApplyForceToCenter(ulong entityID, ref Vector2 force, bool wake);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_ApplyLinearImpulse(ulong entityID, ref Vector2 impulse, ref Vector2 point, bool wake);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(ulong entityID, ref Vector2 impulse, bool wake);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_ApplyAngularImpulse(ulong entityID, float impulse, bool wake);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_ApplyTorque(ulong entityID, float torque, bool wake);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_GetLinearVelocity(ulong entityID, out Vector2 linearVelocity);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_SetLinearVelocity(ulong entityID, ref Vector2 linearVelocity);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float Rigidbody2DComponent_GetAngularVelocity(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_SetAngularVelocity(ulong entityID, float angularVelocity);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static Rigidbody2DComponent.BodyType Rigidbody2DComponent_GetType(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_SetType(ulong entityID, Rigidbody2DComponent.BodyType type);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Rigidbody2DComponent_IsFixedRotation(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_SetFixedRotation(ulong entityID, bool fixedRotation);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float Rigidbody2DComponent_GetGravityScale(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_SetGravityScale(ulong entityID, float gravityScale);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Rigidbody2DComponent_IsEnabled(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_SetEnabled(ulong entityID, bool enabled);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool Rigidbody2DComponent_IsAwake(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_SetAwake(ulong entityID, bool awake);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float Rigidbody2DComponent_GetAngle(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float Rigidbody2DComponent_GetMass(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float Rigidbody2DComponent_GetIntertia(ulong entityID);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void Rigidbody2DComponent_SetTransform(ulong entityID, ref Vector2 position, float angle);
		#endregion

		#region TextComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string TextComponent_GetText(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_SetText(ulong entityID, string text);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_GetColor(ulong entityID, out Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_SetColor(ulong entityID, Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float TextComponent_GetKerning(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_SetKerning(ulong entityID, float kerning);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float TextComponent_GetLineSpacing(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void TextComponent_SetLineSpacing(ulong entityID, float lineSpacing);
		#endregion

		#region BoxCollider2DComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string BoxCollider2DComponent_GetTag(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_SetTag(ulong entityID, string tag);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_GetOffset(ulong entityID, out Vector2 offset);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_SetOffset(ulong entityID, Vector2 offset);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_GetSize(ulong entityID, out Vector2 size);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_SetSize(ulong entityID, Vector2 size);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float BoxCollider2DComponent_GetDensity(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_SetDensity(ulong entityID, float density);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float BoxCollider2DComponent_GetFriction(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_SetFriction(ulong entityID, float friction);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float BoxCollider2DComponent_GetRestitution(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_SetRestitution(ulong entityID, float restitution);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float BoxCollider2DComponent_GetRestitutionThreshold(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_SetRestitutionThreshold(ulong entityID, float restitutionThreshold);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool BoxCollider2DComponent_IsSensor(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void BoxCollider2DComponent_SetSensor(ulong entityID, bool sensor);

		#endregion

		#region CircleCollider2DComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string CircleCollider2DComponent_GetTag(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_SetTag(ulong entityID, string tag);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_GetOffset(ulong entityID, out Vector2 offset);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_SetOffset(ulong entityID, Vector2 offset);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_GetSize(ulong entityID, out Vector2 size);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_SetSize(ulong entityID, Vector2 size);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CircleCollider2DComponent_GetDensity(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_SetDensity(ulong entityID, float density);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CircleCollider2DComponent_GetFriction(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_SetFriction(ulong entityID, float friction);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CircleCollider2DComponent_GetRestitution(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_SetRestitution(ulong entityID, float restitution);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CircleCollider2DComponent_GetRestitutionThreshold(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_SetRestitutionThreshold(ulong entityID, float restitutionThreshold);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool CircleCollider2DComponent_IsSensor(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleCollider2DComponent_SetSensor(ulong entityID, bool sensor);

		#endregion

		#region SpriteRendererComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void SpriteRendererComponent_GetColor(ulong entityID, out Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void SpriteRendererComponent_SetColor(ulong entityID, Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float SpriteRendererComponent_GetTilingFactor(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void SpriteRendererComponent_SetTilingFactor(ulong entityID, float tilingFactor);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong SpriteRendererComponent_GetTextureHandle(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void SpriteRendererComponent_SetTextureHandle(ulong entityID, ulong textureHandle);

		#endregion

		#region RuntimeUI
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_GetAnchoredPosition(ulong entityID, out Vector2 position);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_SetAnchoredPosition(ulong entityID, Vector2 position);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_GetSize(ulong entityID, out Vector2 size);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_SetSize(ulong entityID, Vector2 size);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_GetScale(ulong entityID, out Vector2 scale);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_SetScale(ulong entityID, Vector2 scale);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_GetAnchorMin(ulong entityID, out Vector2 anchor);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_SetAnchorMin(ulong entityID, Vector2 anchor);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_GetAnchorMax(ulong entityID, out Vector2 anchor);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_SetAnchorMax(ulong entityID, Vector2 anchor);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_GetPivot(ulong entityID, out Vector2 pivot);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_SetPivot(ulong entityID, Vector2 pivot);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UITransformComponent_IsVisible(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_SetVisible(ulong entityID, bool visible);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float UITransformComponent_GetRotation(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_SetRotation(ulong entityID, float rotation);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static int UITransformComponent_GetSortOrder(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITransformComponent_SetSortOrder(ulong entityID, int sortOrder);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UICanvasComponent_IsVisible(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_SetVisible(ulong entityID, bool visible);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UICanvasComponent_IsShownInEditor(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_SetShownInEditor(ulong entityID, bool shown);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UICanvasComponent_IsSafeAreaShownInEditor(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_SetSafeAreaShownInEditor(ulong entityID, bool shown);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static int UICanvasComponent_GetScaleMode(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_SetScaleMode(ulong entityID, int scaleMode);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_GetReferenceResolution(ulong entityID, out Vector2 resolution);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_SetReferenceResolution(ulong entityID, Vector2 resolution);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float UICanvasComponent_GetMatchWidthOrHeight(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_SetMatchWidthOrHeight(ulong entityID, float value);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float UICanvasComponent_GetScaleFactor(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_SetScaleFactor(ulong entityID, float value);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_GetSafeAreaInsets(ulong entityID, out Vector4 insets);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UICanvasComponent_SetSafeAreaInsets(ulong entityID, Vector4 insets);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIImageComponent_GetColor(ulong entityID, out Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIImageComponent_SetColor(ulong entityID, Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static ulong UIImageComponent_GetTextureHandle(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIImageComponent_SetTextureHandle(ulong entityID, ulong textureHandle);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static int UIImageComponent_GetTextureSpriteIndex(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIImageComponent_SetTextureSpriteIndex(ulong entityID, int spriteIndex);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIImageComponent_IsRaycastTarget(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIImageComponent_SetRaycastTarget(ulong entityID, bool raycastTarget);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIPanelComponent_GetColor(ulong entityID, out Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIPanelComponent_SetColor(ulong entityID, Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIPanelComponent_IsRaycastTarget(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIPanelComponent_SetRaycastTarget(ulong entityID, bool raycastTarget);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string UITextComponent_GetText(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITextComponent_SetText(ulong entityID, string text);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITextComponent_GetColor(ulong entityID, out Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITextComponent_SetColor(ulong entityID, Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float UITextComponent_GetFontSize(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UITextComponent_SetFontSize(ulong entityID, float fontSize);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string UIButtonComponent_GetText(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIButtonComponent_SetText(ulong entityID, string text);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIButtonComponent_IsInteractable(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIButtonComponent_SetInteractable(ulong entityID, bool interactable);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIButtonComponent_IsHovered(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIButtonComponent_IsPressed(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIButtonComponent_IsFocused(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIButtonComponent_WasClickedThisFrame(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIButtonComponent_WasSubmittedThisFrame(ulong entityID);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string UIToggleComponent_GetLabel(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIToggleComponent_SetLabel(ulong entityID, string label);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIToggleComponent_IsChecked(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIToggleComponent_SetChecked(ulong entityID, bool value);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIToggleComponent_IsInteractable(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIToggleComponent_SetInteractable(ulong entityID, bool interactable);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIToggleComponent_IsHovered(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIToggleComponent_IsPressed(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIToggleComponent_IsFocused(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIToggleComponent_WasChangedThisFrame(ulong entityID);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float UISliderComponent_GetValue(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UISliderComponent_SetValue(ulong entityID, float value);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float UISliderComponent_GetMinValue(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UISliderComponent_SetMinValue(ulong entityID, float value);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float UISliderComponent_GetMaxValue(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UISliderComponent_SetMaxValue(ulong entityID, float value);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UISliderComponent_IsInteractable(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UISliderComponent_SetInteractable(ulong entityID, bool interactable);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UISliderComponent_IsHovered(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UISliderComponent_IsPressed(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UISliderComponent_IsFocused(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UISliderComponent_WasChangedThisFrame(ulong entityID);

		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string UIInputFieldComponent_GetText(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIInputFieldComponent_SetText(ulong entityID, string text);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static string UIInputFieldComponent_GetPlaceholder(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIInputFieldComponent_SetPlaceholder(ulong entityID, string placeholder);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIInputFieldComponent_IsInteractable(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void UIInputFieldComponent_SetInteractable(ulong entityID, bool interactable);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIInputFieldComponent_IsHovered(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIInputFieldComponent_IsFocused(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIInputFieldComponent_WasChangedThisFrame(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static bool UIInputFieldComponent_WasSubmittedThisFrame(ulong entityID);
		#endregion

		#region CircleRendererComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleRendererComponent_GetColor(ulong entityID, out Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleRendererComponent_SetColor(ulong entityID, Vector4 color);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CircleRendererComponent_GetThickness(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleRendererComponent_SetThickness(ulong entityID, float thickness);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static float CircleRendererComponent_GetFade(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static void CircleRendererComponent_SetFade(ulong entityID, float fade);
		#endregion

		#region AudioComponent
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static int AudioComponent_GetADCount(ulong entityID);
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		internal extern static uint AudioComponent_GetAD(ulong entityID, int index);


		#endregion
	}
}
