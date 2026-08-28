#include "render/window/program.h"
#include <windows.h>
#include "log/logger.h"

using namespace std;

int main()
{
	SetConsoleOutputCP(CP_UTF8);
	nova_engine::log::LogManager::Init();
	return render();
	//cout << "1111";
}
