#pragma once

#include <stdio.h>
#include <windows.h>

class GameWindow
{
public:
	HRESULT Init(HINSTANCE hInstance, int nCmdShow, int width, int height, LPCWSTR windowTitle);
	void Cleanup();

	HWND GetHandleWindow();

private:
	HWND mHWnd;

	HRESULT CreateWindowApp(HINSTANCE hInstance, int nCmdShow, int width, int height, LPCWSTR windowTitle);
	void ShowConsole();
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
};