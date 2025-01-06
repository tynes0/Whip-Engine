namespace Whip
{
	public class CircleCollider2DComponent : Component
	{

		public string Tag
		{
			get => InternalCalls.CircleCollider2DComponent_GetTag(entity.ID) ?? string.Empty;
			set => InternalCalls.CircleCollider2DComponent_SetTag(entity.ID, value);
		}
		public Vector2 Size
		{
			get
			{
				InternalCalls.CircleCollider2DComponent_GetSize(entity.ID, out Vector2 size);
				return size;
			}
			set
			{
				InternalCalls.CircleCollider2DComponent_SetSize(entity.ID, value);
			}
		}

		public Vector2 Offset
		{
			get
			{
				InternalCalls.CircleCollider2DComponent_GetOffset(entity.ID, out Vector2 offset);
				return offset;
			}
			set
			{
				InternalCalls.CircleCollider2DComponent_SetOffset(entity.ID, value);
			}
		}

		public float Density
		{
			get => InternalCalls.CircleCollider2DComponent_GetDensity(entity.ID);
			set => InternalCalls.CircleCollider2DComponent_SetDensity(entity.ID, value);
		}

		public float Friction
		{
			get => InternalCalls.CircleCollider2DComponent_GetFriction(entity.ID);
			set => InternalCalls.CircleCollider2DComponent_SetFriction(entity.ID, value);
		}

		public float Restitution
		{
			get => InternalCalls.CircleCollider2DComponent_GetRestitution(entity.ID);
			set => InternalCalls.CircleCollider2DComponent_SetRestitution(entity.ID, value);
		}

		public float RestitutionThreshold
		{
			get => InternalCalls.CircleCollider2DComponent_GetRestitutionThreshold(entity.ID);
			set => InternalCalls.CircleCollider2DComponent_SetRestitutionThreshold(entity.ID, value);
		}
	}
}
