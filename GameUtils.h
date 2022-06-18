#pragma once

#include <Windows.h>

static double countsPerSecond;
static __int64 CounterStart;
static __int64 frameTimeOld;

class GameUtils
{
public:
	static void StartTimer();
	static double GetTime();
	static double GetFrameTime();
};