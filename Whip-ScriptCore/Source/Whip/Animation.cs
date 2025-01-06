namespace Whip
{
	public class Animation
	{
		private Animation(AssetHandle assetHandle) { handle = assetHandle; }
		public void Bound(Entity entity) { InternalCalls.Animation_Bound(handle.ID, entity.ID); }
		public void Unbound() { InternalCalls.Animation_Unbound(handle.ID); }
		public void Play() { InternalCalls.Animation_Play(handle.ID); }
		public void Stop() { InternalCalls.Animation_Stop(handle.ID); }
		public void Pause() { InternalCalls.Animation_Pause(handle.ID); }
		public void Resume() { InternalCalls.Animation_Stop(handle.ID); }

		public readonly AssetHandle handle;
		public bool Valid { get => InternalCalls.Animation_IsValid(handle.ID); }
		public bool IsPlaying { get => InternalCalls.Animation_IsPlaying(handle.ID); }
		public bool IsPaused { get => InternalCalls.Animation_IsPaused(handle.ID); }
		public bool IsLooping
		{ 
			get => InternalCalls.Animation_IsLooping(handle.ID);
			set => InternalCalls.Animation_SetLoop(handle.ID, value);
		}
		public static Animation Get(string name)
		{
			ulong handleID = InternalCalls.Animation_GetAnimationByName(name);
			if (handleID == 0)
				return null;
			return new Animation(new AssetHandle(handleID));
		}
	}
}
