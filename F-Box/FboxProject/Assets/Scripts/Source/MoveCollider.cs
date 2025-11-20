using Whip;

namespace Fbox
{
	public class MoveCollider : Entity
	{
		public bool BlueOrGreen = true;

		private string m_IgnoredColliderTag = "none";
		private Player m_Player;
		private uint m_ColliderCount = 0;
		private Rigidbody2DComponent m_Rigidbody;
		public override void OnCreate()
		{
			Entity ent;
			if (BlueOrGreen)
				ent = FindEntityByName("Blue Character");
			else
				ent = FindEntityByName("Green Character");

			if (ent != null)
			{
				m_Player = ent.As<Player>();
				BoxCollider2DComponent boxCollider = m_Player.GetComponent<BoxCollider2DComponent>();
				if (boxCollider != null)
					m_IgnoredColliderTag = boxCollider.Tag;
			}

			m_Rigidbody = GetComponent<Rigidbody2DComponent>();
		}

		public override void OnUpdate(float ts)
		{
			if (m_Player == null || m_Rigidbody == null)
				return;

			Translation = m_Player.Translation;
			m_Rigidbody.SetTransform(m_Player.Translation.XY, 0);
		}

		public override void OnColliderEnter(string tag)
		{
			if (tag != m_IgnoredColliderTag)
			{
				m_ColliderCount++;
				if (m_ColliderCount > 0)
					m_Player.SetJump(true);
			}
		}
		public override void OnColliderExit(string tag)
		{
			if (tag != m_IgnoredColliderTag)
			{
				if(m_ColliderCount > 0)
					m_ColliderCount--;
				if (m_ColliderCount == 0)
					m_Player.SetJump(false);
			}
		}
	}
}
