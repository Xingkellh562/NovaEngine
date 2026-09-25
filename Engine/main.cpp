#include <windows.h>
#include "window/window.h"
#include "log/logger.h"

using namespace std;

int main()
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif 

	nova_engine::log::LogManager::Init();

	nova_engine::window::Window::GetInstance().Run();
	return 0;
}
