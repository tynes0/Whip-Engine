#include "F_boxApp2D.h"

class f_box : public whip::application
{
public:
	f_box(const whip::application_specification& specification)
		: whip::application(specification)
	{
		push_layer(new fbox_app2D());
	}
};

whip::application* whip::create_application(whip::application_command_line_args args)
{
	whip::application_specification spec;
	spec.properties.title = "F-box";
	spec.properties.fullscreen = true;
	spec.command_line_args = args;
	return new f_box(spec);
}

