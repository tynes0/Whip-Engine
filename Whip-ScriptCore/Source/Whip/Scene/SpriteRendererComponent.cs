namespace Whip
{
	public class SpriteRendererComponent : Component
	{
		public AssetHandle TextureHandle
		{
			get 
			{
				ulong id = InternalCalls.SpriteRendererComponent_GetTextureHandle(entity.ID);
				return new AssetHandle(id);
			}
			set
			{
				InternalCalls.SpriteRendererComponent_SetTextureHandle(entity.ID, value.ID);
			}
		}

		public Vector4 Color
		{
			get
			{
				InternalCalls.SpriteRendererComponent_GetColor(entity.ID, out Vector4 color);
				return color;
			}
			set
			{
				InternalCalls.SpriteRendererComponent_SetColor(entity.ID, value);
			}
		}

		public float TilingFactor
		{
			get => InternalCalls.SpriteRendererComponent_GetTilingFactor(entity.ID);
			set => InternalCalls.SpriteRendererComponent_SetTilingFactor(entity.ID, value);
		}
	}
}
