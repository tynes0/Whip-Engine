#pragma once

#include <Whip/Core/Core.h>

#ifdef WHP_PLATFORM_WINDOWS

extern whip::application* whip::create_application();

int main(int argc, char** argv)
{
	INIT_WHP_LOG;
	auto app = whip::create_application();
	app->run();
	delete app;
}

#endif
