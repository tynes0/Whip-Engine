namespace Whip
{
	public class UIInputFieldComponent : Component
	{
		public string Text
		{
			get => InternalCalls.UIInputFieldComponent_GetText(entity.ID);
			set => InternalCalls.UIInputFieldComponent_SetText(entity.ID, value);
		}

		public string Placeholder
		{
			get => InternalCalls.UIInputFieldComponent_GetPlaceholder(entity.ID);
			set => InternalCalls.UIInputFieldComponent_SetPlaceholder(entity.ID, value);
		}

		public bool Interactable
		{
			get => InternalCalls.UIInputFieldComponent_IsInteractable(entity.ID);
			set => InternalCalls.UIInputFieldComponent_SetInteractable(entity.ID, value);
		}

		public float Radius
		{
			get => InternalCalls.UIInputFieldComponent_GetRadius(entity.ID);
			set => InternalCalls.UIInputFieldComponent_SetRadius(entity.ID, value);
		}

		public bool Hovered => InternalCalls.UIInputFieldComponent_IsHovered(entity.ID);
		public bool Focused => InternalCalls.UIInputFieldComponent_IsFocused(entity.ID);
		public bool ChangedThisFrame => InternalCalls.UIInputFieldComponent_WasChangedThisFrame(entity.ID);
		public bool SubmittedThisFrame => InternalCalls.UIInputFieldComponent_WasSubmittedThisFrame(entity.ID);
	}
}
