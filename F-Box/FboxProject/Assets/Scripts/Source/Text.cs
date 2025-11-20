using System;
using Whip;

namespace Fbox
{
	public class Text : Entity
	{
		private TextComponent textComponent;
		public override void OnCreate()
		{
			textComponent = GetComponent<TextComponent>();
		}

		public override void OnUpdate(float ts)
		{
			if(Input.IsKeyPressed(KeyCode.U))
			{
				textComponent.Color = new Vector4(0.2f, 0.1f, 0.9f, 1.0f);
			}
		}
	}
}
