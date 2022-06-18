#pragma once

#include <dinput.h>

#pragma comment (lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")

class GameInput
{
private:
	IDirectInputDevice8* DIKeyboard;
	IDirectInputDevice8* DIMouse;

	LPDIRECTINPUT8 DirectInput;
	DIMOUSESTATE mouseLastState;

	BYTE keyboardState[256];

public:
	HRESULT Init(HINSTANCE hInstance, HWND hWnd);
	void Cleanup();
	void Detect();
	bool IsKey(BYTE key);
};

