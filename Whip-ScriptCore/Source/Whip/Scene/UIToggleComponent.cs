namespace Whip
{
	public class UIToggleComponent : Component
	{
		public string Label
		{
			get => InternalCalls.UIToggleComponent_GetLabel(entity.ID);
			set => InternalCalls.UIToggleComponent_SetLabel(entity.ID, value);
		}

		public bool Checked
		{
			get => InternalCalls.UIToggleComponent_IsChecked(entity.ID);
			set => InternalCalls.UIToggleComponent_SetChecked(entity.ID, value);
		}

		public bool Interactable
		{
			get => InternalCalls.UIToggleComponent_IsInteractable(entity.ID);
			set => InternalCalls.UIToggleComponent_SetInteractable(entity.ID, value);
		}

		public bool Hovered => InternalCalls.UIToggleComponent_IsHovered(entity.ID);
		public bool Pressed => InternalCalls.UIToggleComponent_IsPressed(entity.ID);
		public bool Focused => InternalCalls.UIToggleComponent_IsFocused(entity.ID);
		public bool ChangedThisFrame => InternalCalls.UIToggleComponent_WasChangedThisFrame(entity.ID);
	}
}
