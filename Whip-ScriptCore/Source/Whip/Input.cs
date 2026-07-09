using static System.Runtime.CompilerServices.RuntimeHelpers;

namespace Whip
{
	public class Input
	{
		public static Vector2 MousePosition => GetMousePosition();
		public static Vector2 MouseDelta => GetMouseDelta();
		public static Vector2 MouseViewportPosition => GetMouseViewportPosition();
		public static Vector2 ScrollDelta => GetScrollDelta();
		public static bool RuntimeInputActive => InternalCalls.Input_IsRuntimeInputActive();
		public static bool RuntimeInputCapturedByUI => InternalCalls.Input_IsRuntimeInputCapturedByUI();
		public static bool GameplayInputActive => InternalCalls.Input_IsRuntimeGameplayInputActive();
		public static bool MouseInsideViewport => InternalCalls.Input_IsMouseInsideViewport();

		public static bool IsKeyDown(KeyCode keycode)
		{
			return InternalCalls.Input_IsKeyDown(keycode);
		}
		public static bool IsKeyUp(KeyCode keycode)
		{
			return InternalCalls.Input_IsKeyUp(keycode);
		}
		public static bool IsKeyPressed(KeyCode keycode)
		{
			return InternalCalls.Input_IsKeyPressed(keycode);
		}
		public static bool IsKeyReleased(KeyCode keycode)
		{
			return InternalCalls.Input_IsKeyReleased(keycode);
		}
		public static bool IsMouseButtonDown(MouseCode button)
		{
			return InternalCalls.Input_IsMouseButtonDown(button);
		}
		public static bool IsMouseButtonUp(MouseCode button)
		{
			return InternalCalls.Input_IsMouseButtonUp(button);
		}
		public static bool IsMouseButtonPressed(MouseCode button)
		{
			return InternalCalls.Input_IsMouseButtonPressed(button);
		}
		public static bool IsMouseButtonReleased(MouseCode button)
		{
			return InternalCalls.Input_IsMouseButtonReleased(button);
		}
		public static float GetMouseX()
		{
			return InternalCalls.Input_GetMouseX();
		}
		public static float GetMouseY()
		{
			return InternalCalls.Input_GetMouseY();
		}
		public static Vector2 GetMousePosition()
		{
			InternalCalls.Input_GetMousePosition(out Vector2 position);
			return position;
		}
		public static Vector2 GetMouseDelta()
		{
			InternalCalls.Input_GetMouseDelta(out Vector2 delta);
			return delta;
		}
		public static Vector2 GetMouseViewportPosition()
		{
			InternalCalls.Input_GetMouseViewportPosition(out Vector2 position);
			return position;
		}
		public static Vector2 GetScrollDelta()
		{
			return new Vector2(InternalCalls.Input_GetScrollDeltaX(), InternalCalls.Input_GetScrollDeltaY());
		}
		public static float GetScrollDeltaX()
		{
			return InternalCalls.Input_GetScrollDeltaX();
		}
		public static float GetScrollDeltaY()
		{
			return InternalCalls.Input_GetScrollDeltaY();
		}
		public static float GetAxisRaw(KeyCode negative, KeyCode positive)
		{
			float value = 0.0f;
			if (IsKeyDown(negative))
				value -= 1.0f;
			if (IsKeyDown(positive))
				value += 1.0f;
			return value;
		}
		public static Vector2 GetKeyboardVector(KeyCode left, KeyCode right, KeyCode down, KeyCode up, bool normalized = true)
		{
			Vector2 vector = new Vector2(GetAxisRaw(left, right), GetAxisRaw(down, up));
			return normalized ? vector.Normalized() : vector;
		}
		public static Vector2 GetWASDVector(bool normalized = true)
		{
			return GetKeyboardVector(KeyCode.A, KeyCode.D, KeyCode.S, KeyCode.W, normalized);
		}
	}
}
