namespace Whip
{
	public class UIPanelComponent : Component
	{
		public Vector4 Color
		{
			get
			{
				InternalCalls.UIPanelComponent_GetColor(entity.ID, out Vector4 color);
				return color;
			}
			set => InternalCalls.UIPanelComponent_SetColor(entity.ID, value);
		}

		public bool RaycastTarget
		{
			get => InternalCalls.UIPanelComponent_IsRaycastTarget(entity.ID);
			set => InternalCalls.UIPanelComponent_SetRaycastTarget(entity.ID, value);
		}

		public float Radius
		{
			get => InternalCalls.UIPanelComponent_GetRadius(entity.ID);
			set => InternalCalls.UIPanelComponent_SetRadius(entity.ID, value);
		}
	}
}
