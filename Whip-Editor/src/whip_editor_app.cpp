#include "editor_layer.h"

_WHIP_START

class whip_editor : public application
{
public:
	whip_editor(const application_specification& spec) : application(spec)
	{
		push_layer(new editor_layer());
	}
};

application* create_application(application_command_line_args args)
{
	application_specification spec;
	spec.properties.title = "Whip Editor";
	spec.properties.fullscreen = true;
	spec.command_line_args = args;
	return new whip_editor(spec);
}

_WHIP_END
