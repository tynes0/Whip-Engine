using Whip;

namespace Fbox
{
	public class TestObject : Entity
	{


		public override void OnCreate()
		{
			Logger.Level = Logger.LogLevel.Debug;
		}

		public override void OnUpdate(float ts)
		{
			if (Input.IsKeyPressed(KeyCode.O))
			{
				if (true)
				{
					Timer.SetTimeout((object obj) => { Logger.Log("TestObject Log Called!"); }, 1000);
				}
			}
		}
	}
}
