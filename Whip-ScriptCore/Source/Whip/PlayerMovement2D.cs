namespace Whip
{
	public struct PlatformerInput2D
	{
		public float Horizontal;
		public bool JumpPressed;
		public bool JumpHeld;
		public bool CrouchHeld;

		public Vector2 Direction => new Vector2(Horizontal, 0.0f);
		public bool HasHorizontalInput => Horizontal != 0.0f;
	}

	public static class PlayerMovement2D
	{
		public static PlatformerInput2D ReadPlatformer(
			KeyCode left = KeyCode.A,
			KeyCode right = KeyCode.D,
			KeyCode jump = KeyCode.Space,
			KeyCode crouch = KeyCode.S)
		{
			return new PlatformerInput2D
			{
				Horizontal = Input.GetAxisRaw(left, right),
				JumpPressed = Input.IsKeyPressed(jump),
				JumpHeld = Input.IsKeyDown(jump),
				CrouchHeld = Input.IsKeyDown(crouch)
			};
		}

		public static void ApplyHorizontalVelocity(Rigidbody2DComponent body, float horizontal, float speed)
		{
			if (body == null)
				return;

			Vector2 velocity = body.LinearVelocity;
			velocity.X = horizontal * speed;
			body.LinearVelocity = velocity;
		}

		public static void ApplyJumpVelocity(Rigidbody2DComponent body, float jumpVelocity)
		{
			if (body == null)
				return;

			Vector2 velocity = body.LinearVelocity;
			velocity.Y = jumpVelocity;
			body.LinearVelocity = velocity;
		}

		public static void ApplyPlatformerVelocity(Rigidbody2DComponent body, PlatformerInput2D input, float moveSpeed, float jumpVelocity, bool canJump)
		{
			if (body == null)
				return;

			Vector2 velocity = body.LinearVelocity;
			velocity.X = input.Horizontal * moveSpeed;
			if (canJump && input.JumpPressed)
				velocity.Y = jumpVelocity;
			body.LinearVelocity = velocity;
		}
	}
}
