#include <windows.h>

#include "app/App.h"

int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	(void)hPrevInstance;
	(void)lpCmdLine;

	App app;
	if (!app.Initialize(hInstance, nCmdShow))
		return 1;

	int code = app.Run();
	app.Shutdown();
	return code;
}
