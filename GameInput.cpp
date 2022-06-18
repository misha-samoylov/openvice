#include "GameInput.h"


HRESULT GameInput::Init(HINSTANCE hInstance, HWND hwnd)
{
	HRESULT hr;

	hr = DirectInput8Create(hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&m_pDirectInput,
		NULL);

	hr = m_pDirectInput->CreateDevice(GUID_SysKeyboard,
		&m_pDIKeyboard,
		NULL);

	hr = m_pDirectInput->CreateDevice(GUID_SysMouse,
		&m_pDIMouse,
		NULL);

	hr = m_pDIKeyboard->SetDataFormat(&c_dfDIKeyboard);
	hr = m_pDIKeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	hr = m_pDIMouse->SetDataFormat(&c_dfDIMouse);
	hr = m_pDIMouse->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

	return hr;
}

bool GameInput::IsKey(BYTE key)
{
	if (mKeyboardState[key] & 0x80)
		return true;

	return false;
}

void GameInput::Detect()
{
	m_pDIKeyboard->Acquire();
	m_pDIMouse->Acquire();

	m_pDIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mMouseCurrState);
	m_pDIKeyboard->GetDeviceState(sizeof(mKeyboardState), (LPVOID)&mKeyboardState);
}

float GameInput::GetMouseSpeedX()
{
	return (float)mMouseCurrState.lX;
}

float GameInput::GetMouseSpeedY()
{
	return (float)mMouseCurrState.lY;
}

void GameInput::Cleanup()
{
	m_pDIKeyboard->Unacquire();
	m_pDIMouse->Unacquire();

	m_pDirectInput->Release();
}