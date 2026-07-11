using System;

namespace Whip
{
	public class UIEntity : Entity
	{
		public virtual void OnUIClick() {}
		public virtual void OnUIToggle(bool value) {}
		public virtual void OnUISlider(float value) {}
		public virtual void OnUIInputChanged(String value) {}
		public virtual void OnUIInputSubmit(String value) {}
	}
}
