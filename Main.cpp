#include "System.h"

#ifndef __ANDROID__
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
	std::unique_ptr<Game> game = std::make_unique<Game>();
	game->Main();
	return 0;
}
#else
int android_main(void) {
	std::unique_ptr<Game> game = std::make_unique<Game>();
	game->Main();
	return 0;
}
#endif