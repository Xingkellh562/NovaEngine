#include <windows.h>
#include "window/window.h"
#include "log/logger.h"

using namespace std;

int main()
{
	SetConsoleOutputCP(CP_UTF8);
	nova_engine::log::LogManager::Init();

	nova_engine::window::Window::GetInstance().Run();
	return 0;
}
