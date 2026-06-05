namespace Whip
{
	public class SceneReference : AssetHandle
	{
		public SceneReference() : base(0)
		{
		}

		public SceneReference(ulong id) : base(id)
		{
		}

		public bool IsSet
		{
			get => ID != 0;
		}

		public bool IsValid
		{
			get => AssetManager.IsScene(this);
		}

		public string Path
		{
			get => AssetManager.GetFilepath(this);
		}

		public string Name
		{
			get => string.IsNullOrEmpty(Path) ? string.Empty : System.IO.Path.GetFileNameWithoutExtension(Path);
		}

		public bool Load()
		{
			return SceneManager.LoadScene(this);
		}
	}
}
