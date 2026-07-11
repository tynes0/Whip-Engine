namespace Whip
{
	public class UICanvasComponent : Component
	{
		public enum CanvasScaleMode
		{
			ConstantPixelSize = 0,
			ScaleWithScreenSize = 1
		}

		public bool Visible
		{
			get => InternalCalls.UICanvasComponent_IsVisible(entity.ID);
			set => InternalCalls.UICanvasComponent_SetVisible(entity.ID, value);
		}

		public bool ShowInEditor
		{
			get => InternalCalls.UICanvasComponent_IsShownInEditor(entity.ID);
			set => InternalCalls.UICanvasComponent_SetShownInEditor(entity.ID, value);
		}

		public bool ShowSafeAreaInEditor
		{
			get => InternalCalls.UICanvasComponent_IsSafeAreaShownInEditor(entity.ID);
			set => InternalCalls.UICanvasComponent_SetSafeAreaShownInEditor(entity.ID, value);
		}

		public CanvasScaleMode ScaleMode
		{
			get => (CanvasScaleMode)InternalCalls.UICanvasComponent_GetScaleMode(entity.ID);
			set => InternalCalls.UICanvasComponent_SetScaleMode(entity.ID, (int)value);
		}

		public Vector2 ReferenceResolution
		{
			get
			{
				InternalCalls.UICanvasComponent_GetReferenceResolution(entity.ID, out Vector2 resolution);
				return resolution;
			}
			set => InternalCalls.UICanvasComponent_SetReferenceResolution(entity.ID, value);
		}

		public float MatchWidthOrHeight
		{
			get => InternalCalls.UICanvasComponent_GetMatchWidthOrHeight(entity.ID);
			set => InternalCalls.UICanvasComponent_SetMatchWidthOrHeight(entity.ID, value);
		}

		public float ScaleFactor
		{
			get => InternalCalls.UICanvasComponent_GetScaleFactor(entity.ID);
			set => InternalCalls.UICanvasComponent_SetScaleFactor(entity.ID, value);
		}

		public Vector4 SafeAreaInsets
		{
			get
			{
				InternalCalls.UICanvasComponent_GetSafeAreaInsets(entity.ID, out Vector4 insets);
				return insets;
			}
			set => InternalCalls.UICanvasComponent_SetSafeAreaInsets(entity.ID, value);
		}
	}
}
