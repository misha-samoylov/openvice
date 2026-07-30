#include "Input.hpp"


HRESULT Input::Init(HINSTANCE hInstance, HWND hwnd)
{
	HRESULT hr;

	hr = DirectInput8Create(hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&m_pDirectInput,
		NULL);
	if (FAILED(hr))
		return hr;

	hr = m_pDirectInput->CreateDevice(GUID_SysKeyboard,
		&m_pDIKeyboard,
		NULL);
	if (FAILED(hr))
		return hr;

	hr = m_pDirectInput->CreateDevice(GUID_SysMouse,
		&m_pDIMouse,
		NULL);
	if (FAILED(hr))
		return hr;

	hr = m_pDIKeyboard->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(hr))
		return hr;
	hr = m_pDIKeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(hr))
		return hr;

	hr = m_pDIMouse->SetDataFormat(&c_dfDIMouse);
	if (FAILED(hr))
		return hr;
	hr = m_pDIMouse->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

	return hr;
}

bool Input::IsKey(BYTE key)
{
	if (m_keyboardState[key] & 0x80)
		return true;

	return false;
}

HRESULT Input::Detect()
{
	/* Don't abort the whole frame if keyboard Acquire fails (unfocused) —
	 * still try to read mouse so free-cam look keeps working. */
	m_pDIKeyboard->Acquire();
	m_pDIMouse->Acquire();

	m_pDIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &m_mouseCurrState);
	m_pDIKeyboard->GetDeviceState(sizeof(m_keyboardState), (LPVOID)&m_keyboardState);
	return S_OK;
}

float Input::GetMouseSpeedX()
{
	return (float)m_mouseCurrState.lX;
}

float Input::GetMouseSpeedY()
{
	return (float)m_mouseCurrState.lY;
}

float Input::GetMouseWheel()
{
	return (float)m_mouseCurrState.lZ;
}

void Input::Cleanup()
{
	m_pDIKeyboard->Unacquire();
	m_pDIMouse->Unacquire();

	m_pDirectInput->Release();
}