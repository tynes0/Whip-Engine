#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/Application.h>
#include <Whip/Debug/Instrumentor.h>

#ifdef WHP_PLATFORM_WINDOWS

extern whip::application* whip::create_application(whip::application_command_line_args args);

int main(int argc, char** argv)
{
	whip::log::init();
	WHP_PROFILE_BEGIN_SESSION("Startup", "WhipProfile-Startup.json");
	auto app = whip::create_application({ argc, argv });
	WHP_PROFILE_END_SESSION();

	WHP_PROFILE_BEGIN_SESSION("Runtime", "WhipProfile-Runtime.json");
	app->run();
	WHP_PROFILE_END_SESSION();

	WHP_PROFILE_BEGIN_SESSION("Shutdown", "WhipProfile-Shutdown.json");
	delete app;
	WHP_PROFILE_END_SESSION();
}


#endif
