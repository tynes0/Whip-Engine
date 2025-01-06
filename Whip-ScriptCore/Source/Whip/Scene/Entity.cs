using System;
using System.Reflection;

namespace Whip
{
	public class Entity
	{
		protected Entity() { ID = 0; }

		internal Entity(ulong id)
		{
			ID = id;
		}

		public readonly ulong ID;

		public Vector3 Translation
		{
			get
			{
				InternalCalls.TransformComponent_GetTranslation(ID, out Vector3 result);
				return result;
			}
			set
			{
				InternalCalls.TransformComponent_SetTranslation(ID, ref value);
			}
		}

		public Vector3 Rotation
		{
			get
			{
				InternalCalls.TransformComponent_GetRotation(ID, out Vector3 result);
				return result;
			}
			set
			{
				InternalCalls.TransformComponent_SetRotation(ID, ref value);
			}
		}


		public Vector3 Scale
		{
			get
			{
				InternalCalls.TransformComponent_GetScale(ID, out Vector3 result);
				return result;
			}
			set
			{
				InternalCalls.TransformComponent_SetScale(ID, ref value);
			}
		}
		public virtual void OnCreate() { }
		public virtual void OnUpdate(float ts) {}
		public virtual void OnColliderEnter(String tag) {}
		public virtual void OnColliderExit(String tag) {}
		public bool HasComponent<T>() where T : Component, new()
		{
			Type componentType = typeof(T);
			return InternalCalls.Entity_HasComponent(ID, componentType);
		}

		public T GetComponent<T>() where T : Component, new()
		{
			if (!HasComponent<T>())
				return null;

			T component = new T() { entity = this };

			MethodInfo method = typeof(T).GetMethod("Ctor", BindingFlags.Instance | BindingFlags.NonPublic);
			method?.Invoke(component, null);

			return component;
		}

		public Entity FindEntityByName(string name)
		{
			ulong entityID = InternalCalls.Entity_FindEntityByName(name);
			if (entityID == 0)
				return null;

			return new Entity(entityID);
		}

		public T As<T>() where T : Entity, new()
		{
			object instance = InternalCalls.GetScriptInstance(ID);
			return instance as T;
		}
	}

}
