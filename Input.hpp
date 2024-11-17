#pragma once

#include <dinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input
{
public:
	HRESULT Init(HINSTANCE hInstance, HWND hWnd);
	void Cleanup();
	HRESULT Detect();

	bool IsKey(BYTE key);
	float GetMouseSpeedX();
	float GetMouseSpeedY();

private:
	LPDIRECTINPUT8 m_pDirectInput;

	IDirectInputDevice8 *m_pDIKeyboard;
	IDirectInputDevice8 *m_pDIMouse;

	BYTE m_keyboardState[256];
	DIMOUSESTATE m_mouseCurrState;
};