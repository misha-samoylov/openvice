#include "GameWindow.hpp"

HWND GameWindow::GetHandleWindow()
{
	return mHWnd;
}

HRESULT GameWindow::Init(HINSTANCE hInstance, int nCmdShow, 
	int width, int height, LPCWSTR windowTitle)
{
	HRESULT hr;

	hr = CreateWindowApp(hInstance, nCmdShow, width, height, windowTitle);
	if (FAILED(hr)) {
		printf("Error: cannot create window\n");
		return hr;
	}

	ShowConsole();

	return hr;
}

void GameWindow::Cleanup()
{
}

HRESULT GameWindow::CreateWindowApp(HINSTANCE hInstance, int nCmdShow, 
	int width, int height, LPCWSTR windowTitle)
{
	LPCWSTR CLASS_NAME = L"OpenViceWndClass";

	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = GameWindow::WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.style = CS_HREDRAW | CS_VREDRAW;

	if (!RegisterClass(&wc)) {
		printf("Error: cannot register window\n");
		return E_FAIL;
	}

	RECT rc = { 0, 0, width, height };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	mHWnd = CreateWindowEx(
		0, /* extended styles */
		CLASS_NAME,
		windowTitle,
		WS_OVERLAPPEDWINDOW, /* style */
		CW_USEDEFAULT, CW_USEDEFAULT, /* position: x, y */
		rc.right - rc.left, rc.bottom - rc.top, /* size: weight, height */
		NULL, /* parent window */
		NULL, /* menu */
		hInstance,
		NULL /* extended app info */
	);

	if (mHWnd == NULL) {
		MessageBox(NULL, L"Cannot create window", L"Error", MB_OK);
		return E_FAIL;
	}

	ShowWindow(mHWnd, nCmdShow);
	UpdateWindow(mHWnd);

	ShowCursor(FALSE);

	return S_OK;
}

void GameWindow::ShowConsole()
{
	FILE* conin = stdin;
	FILE* conout = stdout;
	FILE* conerr = stderr;
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	freopen_s(&conin, "CONIN$", "r", stdin);
	freopen_s(&conout, "CONOUT$", "w", stdout);
	freopen_s(&conerr, "CONOUT$", "w", stderr);
	SetConsoleTitle(L"appconsole");
}

LRESULT CALLBACK GameWindow::WndProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, message, wp, lp);
	}

	return 0;
}