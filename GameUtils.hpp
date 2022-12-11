#pragma once

#include <stdio.h>

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

static double g_countsPerSecond;
static __int64 g_counterStart;
static __int64 g_frameTimeOld;

class GameUtils
{
public:
	static void StartTimer();
	static double GetTime();
	static double GetFrameTime();
	HRESULT CompileShaderFromFile(LPCWSTR szFileName, 
		LPCSTR szEntryPoint, LPCSTR szShaderModel, 
		ID3DBlob** ppBlobOut);
};