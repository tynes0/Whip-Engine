namespace Whip
{
	public class UIButtonComponent : Component
	{
		public string Text
		{
			get => InternalCalls.UIButtonComponent_GetText(entity.ID);
			set => InternalCalls.UIButtonComponent_SetText(entity.ID, value);
		}

		public bool Interactable
		{
			get => InternalCalls.UIButtonComponent_IsInteractable(entity.ID);
			set => InternalCalls.UIButtonComponent_SetInteractable(entity.ID, value);
		}

		public bool Hovered => InternalCalls.UIButtonComponent_IsHovered(entity.ID);
		public bool Pressed => InternalCalls.UIButtonComponent_IsPressed(entity.ID);
		public bool Focused => InternalCalls.UIButtonComponent_IsFocused(entity.ID);
		public bool ClickedThisFrame => InternalCalls.UIButtonComponent_WasClickedThisFrame(entity.ID);
		public bool SubmittedThisFrame => InternalCalls.UIButtonComponent_WasSubmittedThisFrame(entity.ID);
	}
}
