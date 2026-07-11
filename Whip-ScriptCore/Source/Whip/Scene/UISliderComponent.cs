namespace Whip
{
	public class UISliderComponent : Component
	{
		public float Value
		{
			get => InternalCalls.UISliderComponent_GetValue(entity.ID);
			set => InternalCalls.UISliderComponent_SetValue(entity.ID, value);
		}

		public float MinValue
		{
			get => InternalCalls.UISliderComponent_GetMinValue(entity.ID);
			set => InternalCalls.UISliderComponent_SetMinValue(entity.ID, value);
		}

		public float MaxValue
		{
			get => InternalCalls.UISliderComponent_GetMaxValue(entity.ID);
			set => InternalCalls.UISliderComponent_SetMaxValue(entity.ID, value);
		}

		public float TrackRadius
		{
			get => InternalCalls.UISliderComponent_GetTrackRadius(entity.ID);
			set => InternalCalls.UISliderComponent_SetTrackRadius(entity.ID, value);
		}

		public float HandleRadius
		{
			get => InternalCalls.UISliderComponent_GetHandleRadius(entity.ID);
			set => InternalCalls.UISliderComponent_SetHandleRadius(entity.ID, value);
		}

		public bool Interactable
		{
			get => InternalCalls.UISliderComponent_IsInteractable(entity.ID);
			set => InternalCalls.UISliderComponent_SetInteractable(entity.ID, value);
		}

		public bool Hovered => InternalCalls.UISliderComponent_IsHovered(entity.ID);
		public bool Pressed => InternalCalls.UISliderComponent_IsPressed(entity.ID);
		public bool Focused => InternalCalls.UISliderComponent_IsFocused(entity.ID);
		public bool ChangedThisFrame => InternalCalls.UISliderComponent_WasChangedThisFrame(entity.ID);
	}
}
