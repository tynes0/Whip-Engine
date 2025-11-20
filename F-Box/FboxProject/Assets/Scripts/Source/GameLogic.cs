using System;
using Whip;

namespace Fbox
{
	public class GameLogic : Entity
	{
		private ulong m_id;
		public override void OnCreate()
		{
			m_id = Timer.SetInterval((data) => {
				Logger.Log("The data is: " + m_id);
			}, 1000, m_id);
		}

		public override void OnUpdate(float ts)
		{
			if(Input.IsKeyPressed(KeyCode.H)) 
				Timer.Stop(m_id);
		}
	}
}
