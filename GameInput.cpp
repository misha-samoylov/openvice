#include "GameInput.h"


HRESULT GameInput::Init(HINSTANCE hInstance, HWND hwnd)
{
	HRESULT hr;

	hr = DirectInput8Create(hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&DirectInput,
		NULL);

	hr = DirectInput->CreateDevice(GUID_SysKeyboard,
		&DIKeyboard,
		NULL);

	hr = DirectInput->CreateDevice(GUID_SysMouse,
		&DIMouse,
		NULL);

	hr = DIKeyboard->SetDataFormat(&c_dfDIKeyboard);
	hr = DIKeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	hr = DIMouse->SetDataFormat(&c_dfDIMouse);
	hr = DIMouse->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

	return hr;
}

bool GameInput::IsKey(BYTE key)
{
	if (keyboardState[key] & 0x80)
		return true;

	return false;
}

void GameInput::Detect()
{
	DIKeyboard->Acquire();
	DIMouse->Acquire();

	DIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseCurrState);
	DIKeyboard->GetDeviceState(sizeof(keyboardState), (LPVOID)&keyboardState);
}

float GameInput::GetMouseSpeedX()
{
	return (float)mouseCurrState.lX;
}

float GameInput::GetMouseSpeedY()
{
	return (float)mouseCurrState.lY;
}

void GameInput::Cleanup()
{
	DIKeyboard->Unacquire();
	DIMouse->Unacquire();

	DirectInput->Release();
}