#pragma once

#ifdef WHP_PLATFORM_WINDOWS

#define RUN_APP 			auto app = Whip::CreateApplication();\
							app->Run();\
							delete app

extern Whip::Application* Whip::CreateApplication();

int main(int argc, char** argv)
{
	INIT_WHP_LOG;
	RUN_APP;
}

#endif
