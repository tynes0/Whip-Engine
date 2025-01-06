namespace Whip
{
	public class BoxCollider2DComponent : Component
	{

		public string Tag
		{
			get => InternalCalls.BoxCollider2DComponent_GetTag(entity.ID) ?? string.Empty;
			set => InternalCalls.BoxCollider2DComponent_SetTag(entity.ID, value);
		}
		public Vector2 Size
		{
			get
			{
				InternalCalls.BoxCollider2DComponent_GetSize(entity.ID, out Vector2 size);
				return size;
			}
			set
			{
				InternalCalls.BoxCollider2DComponent_SetSize(entity.ID, value);
			}
		}

		public Vector2 Offset
		{
			get
			{
				InternalCalls.BoxCollider2DComponent_GetOffset(entity.ID, out Vector2 offset);
				return offset;
			}
			set 
			{
				InternalCalls.BoxCollider2DComponent_SetOffset(entity.ID, value);
			}
		}

		public float Density
		{
			get => InternalCalls.BoxCollider2DComponent_GetDensity(entity.ID);
			set => InternalCalls.BoxCollider2DComponent_SetDensity(entity.ID, value);
		}

		public float Friction
		{
			get => InternalCalls.BoxCollider2DComponent_GetFriction(entity.ID);
			set => InternalCalls.BoxCollider2DComponent_SetFriction(entity.ID, value);
		}

		public float Restitution
		{
			get => InternalCalls.BoxCollider2DComponent_GetRestitution(entity.ID);
			set => InternalCalls.BoxCollider2DComponent_SetRestitution(entity.ID, value);
		}

		public float RestitutionThreshold
		{
			get => InternalCalls.BoxCollider2DComponent_GetRestitutionThreshold(entity.ID);
			set => InternalCalls.BoxCollider2DComponent_SetRestitutionThreshold(entity.ID, value);
		}
	}
}
