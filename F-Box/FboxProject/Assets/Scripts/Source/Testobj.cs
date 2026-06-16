using Whip;
using System;
namespace Fbox
{
	public class Testobj : Entity
	{
		public float[] TestArray = { 1,7,8,12,5 };

		public override void OnCreate()
		{
			Logger.Log(TestArray[0].ToString());
			Logger.Log(TestArray[1].ToString());
			Logger.Log(TestArray[2].ToString());
			Logger.Log(TestArray[3].ToString());
			Logger.Log(TestArray[4].ToString());
		}

		public override void OnUpdate(float ts)
		{
			if (Input.IsKeyPressed(KeyCode.C))
			{
				Logger.Log(TestArray[0].ToString());
				Logger.Log(TestArray[1].ToString());
				Logger.Log(TestArray[2].ToString());
				Logger.Log(TestArray[3].ToString());
				Logger.Log(TestArray[4].ToString());
			}
		}
	}
}
