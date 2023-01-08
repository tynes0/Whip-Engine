#pragma once

#ifdef WHP_PLATFORM_WINDOWS

extern Whip::Application* Whip::CreateApplication();

//#include <iostream>

int main(int argc, char** argv)
{
	Whip::Log::Init();
	auto app = Whip::CreateApplication();
	app->Run();
	delete app;
}

#endif
