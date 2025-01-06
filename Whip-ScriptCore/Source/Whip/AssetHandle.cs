using System;

namespace Whip
{
	public class AssetHandle
	{
		public AssetHandle() { ID = 0; }

		public AssetHandle(ulong id)
		{
			ID = id;
		}

		public readonly ulong ID;
	}

}
