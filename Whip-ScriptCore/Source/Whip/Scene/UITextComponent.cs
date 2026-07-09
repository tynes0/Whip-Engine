namespace Whip
{
	public class UITextComponent : Component
	{
		public string Text
		{
			get => InternalCalls.UITextComponent_GetText(entity.ID);
			set => InternalCalls.UITextComponent_SetText(entity.ID, value);
		}

		public Vector4 Color
		{
			get
			{
				InternalCalls.UITextComponent_GetColor(entity.ID, out Vector4 color);
				return color;
			}
			set => InternalCalls.UITextComponent_SetColor(entity.ID, value);
		}

		public float FontSize
		{
			get => InternalCalls.UITextComponent_GetFontSize(entity.ID);
			set => InternalCalls.UITextComponent_SetFontSize(entity.ID, value);
		}
	}
}
