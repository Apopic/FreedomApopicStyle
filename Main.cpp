#include "System.h"

#ifndef __ANDROID__
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
	// 想定していない例外で落ちる時も、原因をダイアログに出してから終わる
	try {
		std::unique_ptr<Game> game = std::make_unique<Game>();
		game->Main();
	}
	catch (const std::exception& e) {
		ShowErrorDialog(std::string("エラーのため終了します。\n原因: ") + DescribeException(e));
		return 1;
	}
	return 0;
}
#else
int android_main(void) {
	try {
		std::unique_ptr<Game> game = std::make_unique<Game>();
		game->Main();
	}
	catch (const std::exception& e) {
		ShowErrorDialog(std::string("エラーのため終了します。\n原因: ") + DescribeException(e));
		return 1;
	}
	return 0;
}
#endif
