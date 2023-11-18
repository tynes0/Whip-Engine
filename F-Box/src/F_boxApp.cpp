#include <Whip.h>

#include "F_boxApp2d.h"

class f_box : public whip::application
{
public:
	f_box()
	{
		push_layer(new fbox_app2D());
	}
	~f_box()
	{

	}
};

whip::application* whip::create_application()
{
	return new f_box();
}
