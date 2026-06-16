namespace Whip
{
	public class AnimatorComponent : Component
	{
		public AssetHandle Controller
		{
			get => new AssetHandle(InternalCalls.AnimatorComponent_GetController(entity.ID));
			set => InternalCalls.AnimatorComponent_SetController(entity.ID, value.ID);
		}

		public float Speed
		{
			get => InternalCalls.AnimatorComponent_GetSpeed(entity.ID);
			set => InternalCalls.AnimatorComponent_SetSpeed(entity.ID, value);
		}

		public bool IsPlaying => InternalCalls.AnimatorComponent_IsPlaying(entity.ID);
		public string CurrentState => InternalCalls.AnimatorComponent_GetCurrentState(entity.ID) ?? string.Empty;

		public void Play(string stateName = "")
		{
			InternalCalls.AnimatorComponent_Play(entity.ID, stateName);
		}

		public void Stop()
		{
			InternalCalls.AnimatorComponent_Stop(entity.ID);
		}

		public void SetBool(string name, bool value)
		{
			InternalCalls.AnimatorComponent_SetBool(entity.ID, name, value);
		}

		public void SetInt(string name, int value)
		{
			InternalCalls.AnimatorComponent_SetInt(entity.ID, name, value);
		}

		public void SetFloat(string name, float value)
		{
			InternalCalls.AnimatorComponent_SetFloat(entity.ID, name, value);
		}

		public void SetTrigger(string name)
		{
			InternalCalls.AnimatorComponent_SetTrigger(entity.ID, name);
		}

		public void ResetTrigger(string name)
		{
			InternalCalls.AnimatorComponent_ResetTrigger(entity.ID, name);
		}
	}
}
