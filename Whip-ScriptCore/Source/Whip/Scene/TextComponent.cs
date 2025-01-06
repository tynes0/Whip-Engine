namespace Whip
{
	public class TextComponent : Component
	{

		public string Text
		{
			get => InternalCalls.TextComponent_GetText(entity.ID);
			set => InternalCalls.TextComponent_SetText(entity.ID, value);
		}

		public Vector4 Color
		{
			get
			{
				InternalCalls.TextComponent_GetColor(entity.ID, out Vector4 color);
				return color;
			}

			set
			{
				InternalCalls.TextComponent_SetColor(entity.ID, value);
			}
		}

		public float Kerning
		{
			get => InternalCalls.TextComponent_GetKerning(entity.ID);
			set => InternalCalls.TextComponent_SetKerning(entity.ID, value);
		}

		public float LineSpacing
		{
			get => InternalCalls.TextComponent_GetLineSpacing(entity.ID);
			set => InternalCalls.TextComponent_SetLineSpacing(entity.ID, value);
		}

	}
}
