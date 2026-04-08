#pragma once
class CStatsSync
{
public:
	static void ApplyNetworkPlayerContext(CNetworkPlayer* player);
	static void ApplyLocalContext();
	static void NotifyChanged();
	static bool IsSupportedStat(eStats stat);
};

