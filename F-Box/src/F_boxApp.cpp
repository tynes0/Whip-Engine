#include <Whip.h>

class F_Box : public Whip::Application
{
public:
	F_Box()
	{

	}
	~F_Box()
	{

	}
};

Whip::Application* Whip::CreateApplication()
{
	return new F_Box();
}
