#pragma once

#include <stdio.h>
#include <windows.h>

class GameWindow
{
public:
	void Init(HINSTANCE hInstance, int nCmdShow, int width, int height, LPCWSTR windowTitle);
	void Cleanup();

	HWND GetHandleWindow();

private:
	HWND mHWnd;

	HWND CreateWindowApp(HINSTANCE hInstance, int nCmdShow, int width, int height, LPCWSTR windowTitle);
	void ShowConsole();
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
};