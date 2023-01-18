#pragma once

#ifdef WHP_PLATFORM_WINDOWS

extern Whip::Application* Whip::CreateApplication();

int main(int argc, char** argv)
{
	INIT_WHP_LOG;
	auto app = Whip::CreateApplication();
	app->Run();
	delete app;
}

#endif
