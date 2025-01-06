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
