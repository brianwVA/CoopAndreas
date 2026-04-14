#pragma once
class PlayerHooks
{
public:
	static void InjectHooks();
	static void NotifyLocalRevived();
	static bool IsLocalPlayerDowned();
	static void ClearRemoteDownedAnim(int playerId);
	static uint32_t GetDownedStartTick();
	static uint32_t GetDownedDurationMs();
	static void ForceDownedDeath();
};

