#pragma once
#include <cstddef>
#include <cstdint>

class CStatsSync
{
public:
	static void ApplyNetworkPlayerContext(CNetworkPlayer* player);
	static void ApplyLocalContext();
	static void NotifyChanged();
	static bool IsSupportedStat(eStats stat);

	static void ApplyProgressSnapshot(const PlayerProgressState& progress);

	static bool SetOwnedProperty(std::size_t index, bool isOwned);
	static uint8_t GetOwnedProperty(std::size_t index);

	static bool SetSchoolProgress(std::size_t index, uint8_t value);
	static uint8_t GetSchoolProgress(std::size_t index);

	static bool SetSchoolMedal(std::size_t index, uint8_t value);
	static uint8_t GetSchoolMedal(std::size_t index);
};

