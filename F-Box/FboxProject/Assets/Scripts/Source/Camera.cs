using Whip;
using System;

namespace Fbox
{
	public class Camera : Entity
	{
		public float cameraDistance = 10.0f;
		public float moveSpeed = 5.0f;
		public float X = 5.0f;

		private Entity m_BluePlayer;
		private Entity m_GreenPlayer;
		private CameraComponent m_CameraComponent;

		public override void OnCreate()
		{
			// Karakterleri bul
			m_BluePlayer = FindEntityByName("Blue Character");
			m_GreenPlayer = FindEntityByName("Green Character");
			m_CameraComponent = GetComponent<CameraComponent>();
		}

		public override void OnUpdate(float ts)
		{
			if (m_BluePlayer == null || m_GreenPlayer == null || m_CameraComponent == null)
				return;

			Vector3 bluePosition = m_BluePlayer.Translation;
			Vector3 greenPosition = m_GreenPlayer.Translation;

			// Soldaki ve sa�daki karakteri kontrol et
			Vector3 leftMostCharacter = bluePosition.X < greenPosition.X ? bluePosition : greenPosition;
			Vector3 rightMostCharacter = bluePosition.X > greenPosition.X ? bluePosition : greenPosition;

			Vector3 cameraPosition = this.Translation;

			// Perspektif kamera FOV'sini al
			float fov = m_CameraComponent.PerspectiveVerticalFOV;

			// Aspect ratio (16:9 ekran varsay�yoruz)
			float aspectRatio = 16.0f / 9.0f;
			// Kamera g�r�� alan�n�n y�ksekli�ini hesapla
			float cameraHeight = (float)(Math.Tan(Math.PI * fov / 360) * 2 * cameraDistance);
			// Kamera geni�li�i
			float cameraWidth = cameraHeight * aspectRatio;

			// Kamera alan�n� soldan ve sa�dan takip etmek i�in sol ve sa� s�n�rlar� hesapla
			float cameraLeftEdge = cameraPosition.X - cameraWidth * 0.5f;
			float cameraRightEdge = cameraPosition.X + cameraWidth * 0.5f;

			// Buffer zone: karakterin kameradan �nce hareket etmesini engelleyen bo�luk
			float bufferZone = 1.0f;

			// Sol karakteri takip et
			if (leftMostCharacter.X < cameraLeftEdge - bufferZone)
			{
				cameraPosition.X = Lerp(cameraPosition.X, leftMostCharacter.X + cameraWidth * 0.5f, moveSpeed * ts);
			}
			// Sa� karakteri takip et
			if (rightMostCharacter.X > cameraRightEdge + bufferZone)
			{
				cameraPosition.X = Lerp(cameraPosition.X, rightMostCharacter.X - cameraWidth * 0.5f, moveSpeed * ts);
			}

			// Kamera Z mesafesini ayarla
			cameraPosition.Z = -cameraDistance;

			// Kameran�n pozisyonunu g�ncelle
			this.Translation = new Vector3(cameraPosition.XY, cameraDistance);
		}

		// Lerp fonksiyonu: A'dan B'ye lineer interpolasyon
		private float Lerp(float start, float end, float t)
		{
			return start + (end - start) * t;
		}
	}
}
