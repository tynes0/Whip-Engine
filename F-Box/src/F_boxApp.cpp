#include "F_boxApp2D.h"
//#include "game/game.h"

class f_box : public whip::application
{
public:
	f_box(const whip::application_specification& specification)
		: whip::application(specification)
	{
		push_layer(new fbox_app2D());
	}
	~f_box()
	{
		
	}
};

whip::application* whip::create_application(whip::application_command_line_args args)
{
	application_specification spec;
	spec.properties.m_title = "F-box";
	spec.working_directory = "../Whip-Editor";
	spec.command_line_args = args;
	return new f_box(spec);
}
