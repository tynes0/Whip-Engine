namespace Whip
{
	public class CircleRendererComponent : Component
	{
		public Vector4 Color
		{
			get
			{
				InternalCalls.CircleRendererComponent_GetColor(entity.ID, out Vector4 color);
				return color;
			}
			set
			{
				InternalCalls.CircleRendererComponent_SetColor(entity.ID, value);
			}
		}

		public float Thickness
		{
			get => InternalCalls.CircleRendererComponent_GetThickness(entity.ID);
			set => InternalCalls.CircleRendererComponent_SetThickness(entity.ID, value);
		}

		public float Fade
		{
			get => InternalCalls.CircleRendererComponent_GetFade(entity.ID);
			set => InternalCalls.CircleRendererComponent_SetFade(entity.ID, value);
		}
	}
}
