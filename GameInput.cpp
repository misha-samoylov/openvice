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
	DIMOUSESTATE mouseCurrState;

	DIKeyboard->Acquire();
	DIMouse->Acquire();

	DIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseCurrState);
	DIKeyboard->GetDeviceState(sizeof(keyboardState), (LPVOID)&keyboardState);

	// if (keyboardState[DIK_ESCAPE] & 0x80)
	//    PostMessage(hwnd, WM_DESTROY, 0, 0);

	//float speed = 10.0f * time;

	//if (keyboardState[DIK_W] & 0x80) {
		//moveBackForward += speed;
	//}

	//if (keyboardState[DIK_A] & 0x80) {
		//moveLeftRight -= speed;
	//}

	//if (keyboardState[DIK_D] & 0x80) {
		//moveLeftRight += speed;
	//}

	//if (keyboardState[DIK_S] & 0x80) {
		//moveBackForward -= speed;
	//}

	//if ((mouseCurrState.lX != mouseLastState.lX)
	//	|| (mouseCurrState.lY != mouseLastState.lY)) {

		//camYaw += mouseLastState.lX * 0.001f;
		//camPitch += mouseCurrState.lY * 0.001f;

	//	mouseLastState = mouseCurrState;
	//}

	//UpdateCamera();

	//return;
}

void GameInput::Cleanup()
{
	DIKeyboard->Unacquire();
	DIMouse->Unacquire();

	DirectInput->Release();
}