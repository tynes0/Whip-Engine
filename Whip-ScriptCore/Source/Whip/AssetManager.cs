using System;

namespace Whip
{
	public class AssetManager
	{
		public static AssetHandle ImportAsset(string path)
		{
			ulong id =  InternalCalls.AssetManager_ImportAsset(path);
			return new AssetHandle(id);
		}

		public static void DeleteAsset(ulong id)
		{
			InternalCalls.AssetManager_DeleteAsset(id);
		}

		public static bool IsAssetHandleValid(AssetHandle handle)
		{
			return InternalCalls.AssetManager_IsAssetHandleValid(handle.ID);
		}

		public static bool IsAssetLoaded(AssetHandle handle)
		{
			return InternalCalls.AssetManager_IsAssetLoaded(handle.ID);
		}

		public static AssetType GetAssetType(AssetHandle handle)
		{
			return InternalCalls.AssetManager_GetAssetType(handle.ID);
		}
	}

}
