#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/Application.h>
#include <Whip/Debug/Instrumentor.h>
#include <Whip/Utils/PlatformUtils.h>

#ifdef WHP_PLATFORM_WINDOWS

extern whip::Application* whip::CreateApplication(whip::ApplicationCommandLineArgs args);

inline int main(int argc, char** argv)
{
	whip::Log::Init();
	whip::Utils::WaitForRestartParentIfNeeded();
	WHP_PROFILE_BEGIN_SESSION("Startup", "WhipProfile-Startup.json");
	auto app = whip::CreateApplication({
		.m_Count = argc,
		.m_Args = argv
	});
	WHP_PROFILE_END_SESSION();

	WHP_PROFILE_BEGIN_SESSION("Runtime", "WhipProfile-Runtime.json");
	app->Run();
	WHP_PROFILE_END_SESSION();

	WHP_PROFILE_BEGIN_SESSION("Shutdown", "WhipProfile-Shutdown.json");
	delete app;
	WHP_PROFILE_END_SESSION();
}


#endif
