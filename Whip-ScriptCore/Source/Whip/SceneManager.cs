namespace Whip
{
	public static class SceneManager
	{
		public static SceneReference ActiveScene
		{
			get => new SceneReference(InternalCalls.SceneManager_GetActiveSceneHandle());
		}

		public static bool LoadScene(AssetHandle scene)
		{
			if (scene == null)
				return false;
			return InternalCalls.SceneManager_LoadScene(scene.ID);
		}

		public static bool LoadScene(SceneReference scene)
		{
			if (scene == null)
				return false;
			return InternalCalls.SceneManager_LoadScene(scene.ID);
		}

		public static bool LoadScene(ulong sceneHandle)
		{
			return InternalCalls.SceneManager_LoadScene(sceneHandle);
		}

		public static bool LoadScene(string sceneName)
		{
			if (string.IsNullOrWhiteSpace(sceneName))
				return false;
			return InternalCalls.SceneManager_LoadSceneByName(sceneName);
		}

		public static SceneReference FindScene(string sceneName)
		{
			if (string.IsNullOrWhiteSpace(sceneName))
				return new SceneReference();
			return new SceneReference(InternalCalls.SceneManager_FindSceneByName(sceneName));
		}

		public static bool LoadStartScene()
		{
			return InternalCalls.SceneManager_LoadStartScene();
		}

		public static bool ReloadScene()
		{
			return InternalCalls.SceneManager_ReloadScene();
		}

		public static bool UnloadScene()
		{
			return InternalCalls.SceneManager_UnloadScene();
		}
	}
}
