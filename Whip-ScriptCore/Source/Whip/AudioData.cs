namespace Whip
{
	public class AudioData
	{
		private AudioData(uint id, AudioComponent component) { ID = id; BoundedComponent = component; }
		internal static AudioData CreateInstance(uint id, AudioComponent component)
		{
			return new AudioData(id, component);
		}

		public AssetHandle Audio
		{
			get
			{
				ulong id = InternalCalls.AD_GetAudioHandle(BoundedComponent.entity.ID, ID);
				return new AssetHandle(id);
			}
			set => InternalCalls.AD_SetAudioHandle(BoundedComponent.entity.ID, ID, value.ID);
		}
		public string Tag
		{
			get => InternalCalls.AD_GetTag(BoundedComponent.entity.ID, ID) ?? string.Empty;
			set => InternalCalls.AD_SetTag(BoundedComponent.entity.ID, ID, value);
		}
		public Vector3 Translation
		{
			get
			{
				InternalCalls.AD_GetTranslation(BoundedComponent.entity.ID, ID, out Vector3 vector3);
				return vector3;
			}
			set => InternalCalls.AD_SetTranslation(BoundedComponent.entity.ID, ID, ref value);
		}
		public bool Spitial
		{
			get => InternalCalls.AD_IsSpitial(BoundedComponent.entity.ID, ID);
			set => InternalCalls.AD_SetSpitial(BoundedComponent.entity.ID, ID, value);
		}
		public bool Loop
		{
			get => InternalCalls.AD_IsLoop(BoundedComponent.entity.ID, ID);
			set => InternalCalls.AD_SetLoop(BoundedComponent.entity.ID, ID, value);
		}
		public float Gain
		{
			get => InternalCalls.AD_GetGain(BoundedComponent.entity.ID, ID);
			set => InternalCalls.AD_SetGain(BoundedComponent.entity.ID, ID, value);
		}
		public float Pitch
		{
			get => InternalCalls.AD_GetPitch(BoundedComponent.entity.ID, ID);
			set => InternalCalls.AD_SetPitch(BoundedComponent.entity.ID, ID, value);
		}
		public float FullClipLength
		{
			get => InternalCalls.AD_GetFullClipLength(BoundedComponent.entity.ID, ID);
		}
		public float ClipStart
		{
			get => InternalCalls.AD_GetClipStart(BoundedComponent.entity.ID, ID);
			set => InternalCalls.AD_SetClipStart(BoundedComponent.entity.ID, ID, value);
		}
		public float ClipEnd
		{
			get => InternalCalls.AD_GetClipEnd(BoundedComponent.entity.ID, ID);
			set => InternalCalls.AD_SetClipEnd(BoundedComponent.entity.ID, ID, value);
		}
		public bool Valid
		{
			get
			{
				if (ID == 0)
					return false;
				return InternalCalls.AD_IsValid(BoundedComponent.entity.ID, ID);
			}
		}

		public readonly uint ID = 0;

		internal readonly AudioComponent BoundedComponent;

		public void IncreaseGain(float incrementValue)
		{
			Gain += incrementValue;
		}
		public void DecreaseGain(float decrementValue)
		{
			if (Gain - decrementValue < 0.0f)
				Gain = 0;
			else
				Gain -= decrementValue;
		}
		public void IncreasePitch(float incrementValue)
		{
			if (Pitch + incrementValue > 2.0f)
				Pitch = 2.0f;
			else
				Pitch += incrementValue;
		}
		public void DecreasePitch(float decrementValue)
		{
			if (Pitch - decrementValue < 0.5f)
				Pitch = 0.5f;
			else
				Pitch -= decrementValue;
		}
		public (float, float) GetMinutesAndSeconds()
		{
			float length = FullClipLength;
			return (length / 60.0f, length % 60.0f);
		}
	}
}
