using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Whip;

namespace Whip
{
	public class CameraComponent : Component
	{
		public bool Primary
		{
			get => InternalCalls.CameraComponent_IsPrimary(entity.ID);
			set => InternalCalls.CameraComponent_SetPrimary(entity.ID, value);
		}
		public bool FixedAspectRatio
		{
			get => InternalCalls.CameraComponent_IsFixedAspectRatio(entity.ID);
			set => InternalCalls.CameraComponent_SetFixedAspectRatio(entity.ID, value);
		}
		public float PerspectiveVerticalFOV
		{
			get => InternalCalls.CameraComponent_GetPerspectiveVerticalFOV(entity.ID);
			set => InternalCalls.CameraComponent_SetPerspectiveVerticalFOV(entity.ID, value);
		}
		public float PerspectiveNearClip
		{
			get => InternalCalls.CameraComponent_GetPerspectiveNearClip(entity.ID);
			set => InternalCalls.CameraComponent_SetPerspectiveNearClip(entity.ID, value);
		}
		public float PerspectiveFarClip
		{
			get => InternalCalls.CameraComponent_GetPerspectiveFarClip(entity.ID);
			set => InternalCalls.CameraComponent_SetPerspectiveFarClip(entity.ID, value);
		}

		public float OrthographicSize
		{
			get => InternalCalls.CameraComponent_GetOrthographicSize(entity.ID);
			set => InternalCalls.CameraComponent_SetOrthographicSize(entity.ID, value);
		}
		public float OrthographicNearClip
		{
			get => InternalCalls.CameraComponent_GetOrthographicNearClip(entity.ID);
			set => InternalCalls.CameraComponent_SetOrthographicNearClip(entity.ID, value);
		}
		public float OrthographicFarClip
		{
			get => InternalCalls.CameraComponent_GetOrthographicFarClip(entity.ID);
			set => InternalCalls.CameraComponent_SetOrthographicFarClip(entity.ID, value);
		}
	}
}
