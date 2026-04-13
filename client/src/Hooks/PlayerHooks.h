#pragma once
class PlayerHooks
{
public:
	static void InjectHooks();
	static void NotifyLocalRevived();
	static bool IsLocalPlayerDowned();
	static void ClearRemoteDownedAnim(int playerId);
};

