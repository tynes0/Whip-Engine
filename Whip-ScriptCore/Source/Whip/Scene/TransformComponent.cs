using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Whip
{
	public class TransformComponent : Component
	{
		public Vector3 Translation
		{
			get
			{
				InternalCalls.TransformComponent_GetTranslation(entity.ID, out Vector3 result);
				return result;
			}
			set
			{
				InternalCalls.TransformComponent_SetTranslation(entity.ID, ref value);
			}
		}

		public Vector3 Rotation
		{
			get
			{
				InternalCalls.TransformComponent_GetRotation(entity.ID, out Vector3 result);
				return result;
			}
			set
			{
				InternalCalls.TransformComponent_SetRotation(entity.ID, ref value);
			}
		}


		public Vector3 Scale
		{
			get
			{
				InternalCalls.TransformComponent_GetScale(entity.ID, out Vector3 result);
				return result;
			}
			set
			{
				InternalCalls.TransformComponent_SetScale(entity.ID, ref value);
			}
		}
	}
}
