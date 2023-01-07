#pragma once

#ifdef WH_PLATFORM_WINDOWS

extern Whip::Application* Whip::CreateApplication();

int main(int argc, char** argv)
{
	auto app = Whip::CreateApplication();
	app->Run();
	delete app;
}

#endif
