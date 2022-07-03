#pragma once

#include <Windows.h>

static double g_countsPerSecond;
static __int64 g_counterStart;
static __int64 g_frameTimeOld;

class GameUtils
{
public:
	static void StartTimer();
	static double GetTime();
	static double GetFrameTime();
};