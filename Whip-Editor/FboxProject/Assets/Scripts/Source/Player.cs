using System;
using Whip;

namespace Fbox
{
	public class Player : Entity
	{
		public float Speed = 3.0f;
		public bool EnableInputs = false;
		public float JumpPower = 1.0f;
		public KeyCode jumpKey = KeyCode.W;
		public KeyCode leftKey = KeyCode.A;
		public KeyCode rightKey = KeyCode.D;
		public KeyCode interactionKey = KeyCode.E;
		public bool EnableAnimation = false;
		
		private Animation Anim;

		bool canJump = false;
		private Rigidbody2DComponent m_Rigidbody;
		private bool m_FaceToLeft = true;
		public Camera camera1 = null;
		public override void OnCreate()
		{
			m_Rigidbody = GetComponent<Rigidbody2DComponent>();
			Anim = Animation.Get("CharacterMove");
			if(Anim != null )
			{
				Anim.IsLooping = true;
				Anim.Bound(this);
			}
		}

		public override void OnUpdate(float ts)
		{
			Vector3 velocity = Vector3.Zero;
			bool OldFaceToLeft = m_FaceToLeft;
			if(EnableInputs)
			{
				if (Input.IsKeyDown(leftKey))
				{
					m_FaceToLeft = true;
					velocity.X = -1.0f;
				}
				else if (Input.IsKeyDown(rightKey))
				{
					m_FaceToLeft = false;
					velocity.X = 1.0f;
				}

				if (Input.IsKeyPressed(jumpKey) && canJump)
					velocity.Y = JumpPower / (ts * 5);

				if (velocity.X != 0.0f && EnableAnimation && Anim != null && !Anim.IsPlaying)
				{	Logger.Log("Animation is Playing");
					Anim.Play();
				}
				else if(velocity.X == 0.0f && EnableAnimation && Anim != null && Anim.IsPlaying)
				{
					Anim.Stop();
				}
				velocity *= Speed * ts;
				m_Rigidbody.ApplyLinearImpulse(velocity.XY, true);
				Vector3 translation = Translation;
				translation += velocity * ts;
				Translation = translation;
				Vector3 rotation = Rotation;
				if (OldFaceToLeft && !m_FaceToLeft)
					rotation.Y = 180.0f;
				else if (!OldFaceToLeft && m_FaceToLeft)
					rotation.Y = 0.0f;
				Rotation = rotation;
			}
		}
		public void SetJump(bool jumpState)
		{
			canJump = jumpState;
		}
	}
}
