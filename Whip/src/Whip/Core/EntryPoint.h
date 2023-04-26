#pragma once

#include <Whip/Core/Core.h>

#ifdef WHP_PLATFORM_WINDOWS

extern whip::application* whip::create_application();


int main(int argc, char** argv)
{
	INIT_WHP_LOG;
	WHP_PROFILE_BEGIN_SESSION("Startup", "WhipProfile-Startup.json");
	auto app = whip::create_application();
	WHP_PROFILE_END_SESSION();

	WHP_PROFILE_BEGIN_SESSION("Runtime", "WhipProfile-Runtime.json");
	app->run();
	WHP_PROFILE_END_SESSION();

	WHP_PROFILE_BEGIN_SESSION("Shutdown", "WhipProfile-Shutdown.json");
	delete app;
	WHP_PROFILE_END_SESSION();
}


#endif
