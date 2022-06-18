#pragma once

#include <dinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class GameInput
{
private:
	LPDIRECTINPUT8 m_pDirectInput;

	IDirectInputDevice8 *m_pDIKeyboard;
	IDirectInputDevice8 *m_pDIMouse;

	BYTE mKeyboardState[256];
	DIMOUSESTATE mMouseCurrState;

public:
	HRESULT Init(HINSTANCE hInstance, HWND hWnd);
	void Cleanup();
	void Detect();

	bool IsKey(BYTE key);
	float GetMouseSpeedX();
	float GetMouseSpeedY();
};

