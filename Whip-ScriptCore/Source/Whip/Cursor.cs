namespace Whip
{
	public enum CursorMode
	{
		Normal = 0,
		Hidden,
		Locked,
		Confined
	}

	public enum CursorShape
	{
		Arrow = 0,
		IBeam,
		Crosshair,
		Hand,
		ResizeHorizontal,
		ResizeVertical,
		NotAllowed
	}

	public static class Cursor
	{
		public static CursorMode Mode
		{
			get => (CursorMode)InternalCalls.Cursor_GetMode();
			set => InternalCalls.Cursor_SetMode((int)value);
		}

		public static CursorShape Shape
		{
			get => (CursorShape)InternalCalls.Cursor_GetShape();
			set => InternalCalls.Cursor_SetShape((int)value);
		}

		public static bool Visible
		{
			get => InternalCalls.Cursor_IsVisible();
			set => InternalCalls.Cursor_SetVisible(value);
		}

		public static void Lock()
		{
			Mode = CursorMode.Locked;
		}

		public static void Unlock()
		{
			Mode = CursorMode.Normal;
		}
	}
}
