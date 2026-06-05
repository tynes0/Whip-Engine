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
			if (handle == null)
				return false;
			return InternalCalls.AssetManager_IsAssetHandleValid(handle.ID);
		}

		public static bool IsAssetLoaded(AssetHandle handle)
		{
			if (handle == null)
				return false;
			return InternalCalls.AssetManager_IsAssetLoaded(handle.ID);
		}

		public static AssetType GetAssetType(AssetHandle handle)
		{
			if (handle == null)
				return AssetType.None;
			return InternalCalls.AssetManager_GetAssetType(handle.ID);
		}

		public static string GetFilepath(AssetHandle handle)
		{
			if (handle == null || handle.ID == 0)
				return string.Empty;
			return InternalCalls.AssetManager_GetFilepath(handle.ID);
		}

		public static bool IsScene(AssetHandle handle)
		{
			return IsAssetHandleValid(handle) && GetAssetType(handle) == AssetType.Scene;
		}
	}

}
