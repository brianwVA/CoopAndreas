#pragma once
class GameHooks
{
public:
	static void InjectHooks();
	static void SetCheatsEnabled(bool enabled);
	static bool AreCheatsEnabled();
};
