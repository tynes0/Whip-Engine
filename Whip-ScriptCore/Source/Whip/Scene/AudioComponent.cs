using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Whip
{
	public class AudioComponent : Component
	{
		internal List<AudioData> AudioDataList;
		internal void Ctor() 
		{
			int count = InternalCalls.AudioComponent_GetADCount(entity.ID);
			AudioDataList = new List<AudioData>(count);
			for (int i = 0; i < count; ++i)
			{
				uint id = InternalCalls.AudioComponent_GetAD(entity.ID, i);
				AudioDataList.Add(AudioData.CreateInstance(id, this));
			}
		}
		public AudioData CreateAudioData()
		{
			uint id = InternalCalls.AudioComponent_CreateAudioData(entity.ID);
			AudioData data = AudioData.CreateInstance(id, this);
			AudioDataList.Add(data);
			return data;
		}
		public AudioData GetAudioData(int index) 
		{ 
			return AudioDataList[index]; 
		}
		public int IndexOf(string tag) 
		{
			for (int i = 0; i < AudioDataList.Count; ++i)
				if (AudioDataList[i].Tag == tag) 
					return i;
			return -1;
		}
		public int IndexOf(AudioData audioData)
		{
			return AudioDataList.IndexOf(audioData);
		}
		public void AddAudioData(AudioData audioData)
		{
			if (audioData == null)
				return;
			if (audioData.BoundedComponent != this)
				return;
			
			AudioDataList.Add(audioData);
		}
		/// <summary>
		///  Only audios added at run time can be removed.
		/// </summary>
		public void RemoveAudioData(AudioData audioData) 
		{
			RemoveAudioData(AudioDataList.IndexOf(audioData));
		}
		/// <summary>
		///  Only audios added at run time can be removed.
		/// </summary>
		public void RemoveAudioData(string tag)
		{
			RemoveAudioData(IndexOf(tag));
		}
		/// <summary>
		///  Only audios added at run time can be removed.
		/// </summary>
		public void RemoveAudioData(int index)
		{
			if (index < 0)
			{
				Logger.LogInternal("[C# Audio Component] Index is out of the range!", Logger.LogLevel.Warn);
				return;
			}
			InternalCalls.AudioComponent_RemoveAudioData(entity.ID, AudioDataList[index].ID);
			try
			{
				AudioDataList.RemoveAt(index);
			}
			catch(ArgumentOutOfRangeException)
			{
				Logger.LogInternal("[C# Audio Component] Index is out of the range!", Logger.LogLevel.Warn);
				return;
			}
		}
	}
}
