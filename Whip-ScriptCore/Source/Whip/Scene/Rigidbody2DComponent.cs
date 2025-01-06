namespace Whip
{
	public class Rigidbody2DComponent : Component
	{
		public enum BodyType { Static = 0, Dynamic, Kinematic }

		public Vector2 LinearVelocity
		{
			get
			{
				InternalCalls.Rigidbody2DComponent_GetLinearVelocity(entity.ID, out Vector2 velocity);
				return velocity;
			}
			set
			{
				InternalCalls.Rigidbody2DComponent_SetLinearVelocity(entity.ID, ref value);
			}
		}

		public float AngularVelocity
		{
			get => InternalCalls.Rigidbody2DComponent_GetAngularVelocity(entity.ID);
			set => InternalCalls.Rigidbody2DComponent_SetAngularVelocity(entity.ID, value);
		}

		public BodyType Type
		{
			get => InternalCalls.Rigidbody2DComponent_GetType(entity.ID);
			set => InternalCalls.Rigidbody2DComponent_SetType(entity.ID, value);
		}

		public bool FixedRotation
		{
			get => InternalCalls.Rigidbody2DComponent_IsFixedRotation(entity.ID);
			set => InternalCalls.Rigidbody2DComponent_SetFixedRotation(entity.ID, value);
		}

		public float GravityScale
		{
			get => InternalCalls.Rigidbody2DComponent_GetGravityScale(entity.ID);
			set => InternalCalls.Rigidbody2DComponent_SetGravityScale(entity.ID, value);
		}

		public bool Enabled
		{
			get => InternalCalls.Rigidbody2DComponent_IsEnabled(entity.ID);
			set => InternalCalls.Rigidbody2DComponent_SetEnabled(entity.ID, value);
		}

		public bool Awake
		{
			get => InternalCalls.Rigidbody2DComponent_IsAwake(entity.ID);
			set => InternalCalls.Rigidbody2DComponent_SetAwake(entity.ID, value);
		}

		public void ApplyForce(Vector2 force, Vector2 worldPosition, bool wake)
		{
			InternalCalls.Rigidbody2DComponent_ApplyForce(entity.ID, ref force, ref worldPosition, wake);
		}
		public void ApplyForce(Vector2 force, bool wake)
		{
			InternalCalls.Rigidbody2DComponent_ApplyForceToCenter(entity.ID, ref force, wake);
		}
		public void ApplyLinearImpulse(Vector2 impulse, Vector2 worldPosition, bool wake)
		{
			InternalCalls.Rigidbody2DComponent_ApplyLinearImpulse(entity.ID, ref impulse, ref worldPosition, wake);
		}
		public void ApplyLinearImpulse(Vector2 impulse, bool wake)
		{
			InternalCalls.Rigidbody2DComponent_ApplyLinearImpulseToCenter(entity.ID, ref impulse, wake);
		}
		public void ApplyAngularImpulse(float impulse, bool wake)
		{
			InternalCalls.Rigidbody2DComponent_ApplyAngularImpulse(entity.ID, impulse, wake);
		}
		public void ApplyTorque(float torque, bool wake)
		{
			InternalCalls.Rigidbody2DComponent_ApplyTorque(entity.ID, torque, wake);
		}
		public float GetAngle()
		{
			return InternalCalls.Rigidbody2DComponent_GetAngle(entity.ID);
		}
		public float GetMass()
		{
			return InternalCalls.Rigidbody2DComponent_GetMass(entity.ID);
		}
		public float GetIntertia()
		{
			return InternalCalls.Rigidbody2DComponent_GetIntertia(entity.ID);
		}
		public void SetTransform(Vector2 position, float angle)
		{
			InternalCalls.Rigidbody2DComponent_SetTransform(entity.ID, ref position, angle);
		}
	}
}
