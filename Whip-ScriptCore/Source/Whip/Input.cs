using static System.Runtime.CompilerServices.RuntimeHelpers;

namespace Whip
{
	public class Input
	{
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
	}
}
