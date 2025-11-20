using Whip;

namespace Fbox
{
	public class Camera2 : Entity
	{
		public float cameraDistance = 10.0f;
		public float moveSpeed = 5.0f;

		private Entity m_BluePlayer;
		private CameraComponent m_CameraComponent;

		public override void OnCreate()
		{
			// Karakterleri bul
			m_BluePlayer = FindEntityByName("Blue Character");
			m_CameraComponent = GetComponent<CameraComponent>();
		}

		public override void OnUpdate(float ts)
		{
			if (m_BluePlayer == null || m_CameraComponent == null)
				return;

			this.Translation = new Vector3(m_BluePlayer.Translation.XY, cameraDistance);
		}

	}
}
