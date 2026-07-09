namespace Whip
{
	public class UIImageComponent : Component
	{
		public AssetHandle TextureHandle
		{
			get => new AssetHandle(InternalCalls.UIImageComponent_GetTextureHandle(entity.ID));
			set => InternalCalls.UIImageComponent_SetTextureHandle(entity.ID, value.ID);
		}

		public int TextureSpriteIndex
		{
			get => InternalCalls.UIImageComponent_GetTextureSpriteIndex(entity.ID);
			set => InternalCalls.UIImageComponent_SetTextureSpriteIndex(entity.ID, value);
		}

		public Vector4 Color
		{
			get
			{
				InternalCalls.UIImageComponent_GetColor(entity.ID, out Vector4 color);
				return color;
			}
			set => InternalCalls.UIImageComponent_SetColor(entity.ID, value);
		}

		public bool RaycastTarget
		{
			get => InternalCalls.UIImageComponent_IsRaycastTarget(entity.ID);
			set => InternalCalls.UIImageComponent_SetRaycastTarget(entity.ID, value);
		}
	}
}
