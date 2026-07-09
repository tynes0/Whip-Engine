namespace Whip
{
	public class UITransformComponent : Component
	{
		public Vector2 AnchoredPosition
		{
			get
			{
				InternalCalls.UITransformComponent_GetAnchoredPosition(entity.ID, out Vector2 position);
				return position;
			}
			set => InternalCalls.UITransformComponent_SetAnchoredPosition(entity.ID, value);
		}

		public Vector2 Size
		{
			get
			{
				InternalCalls.UITransformComponent_GetSize(entity.ID, out Vector2 size);
				return size;
			}
			set => InternalCalls.UITransformComponent_SetSize(entity.ID, value);
		}

		public Vector2 AnchorMin
		{
			get
			{
				InternalCalls.UITransformComponent_GetAnchorMin(entity.ID, out Vector2 anchor);
				return anchor;
			}
			set => InternalCalls.UITransformComponent_SetAnchorMin(entity.ID, value);
		}

		public Vector2 AnchorMax
		{
			get
			{
				InternalCalls.UITransformComponent_GetAnchorMax(entity.ID, out Vector2 anchor);
				return anchor;
			}
			set => InternalCalls.UITransformComponent_SetAnchorMax(entity.ID, value);
		}

		public Vector2 Pivot
		{
			get
			{
				InternalCalls.UITransformComponent_GetPivot(entity.ID, out Vector2 pivot);
				return pivot;
			}
			set => InternalCalls.UITransformComponent_SetPivot(entity.ID, value);
		}

		public bool Visible
		{
			get => InternalCalls.UITransformComponent_IsVisible(entity.ID);
			set => InternalCalls.UITransformComponent_SetVisible(entity.ID, value);
		}

		public float Rotation
		{
			get => InternalCalls.UITransformComponent_GetRotation(entity.ID);
			set => InternalCalls.UITransformComponent_SetRotation(entity.ID, value);
		}

		public int SortOrder
		{
			get => InternalCalls.UITransformComponent_GetSortOrder(entity.ID);
			set => InternalCalls.UITransformComponent_SetSortOrder(entity.ID, value);
		}
	}
}
