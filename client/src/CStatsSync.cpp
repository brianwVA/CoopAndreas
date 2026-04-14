#include "stdafx.h"
#include "../../shared/player_progress.h"
#include <array>
#include <algorithm>
#include <cmath>

int m_anStoredIntStats[COOPANDREAS_INT_STATS_COUNT];
float m_afStoredFloatStats[COOPANDREAS_FLOAT_STATS_COUNT];
static std::array<uint8_t, COOPANDREAS_PROPERTY_COUNT> g_ownedProperties{};
static std::array<uint8_t, COOPANDREAS_SCHOOL_PROGRESS_COUNT> g_schoolProgress{};
static std::array<uint8_t, COOPANDREAS_SCHOOL_MEDAL_COUNT> g_schoolMedals{};
static constexpr std::size_t kScmAlreadyBoughtHouseBase = 728;

constexpr int MAX_INT_STATS = sizeof(m_anStoredIntStats) / sizeof(int);
constexpr int MAX_FLOAT_STATS = sizeof(m_afStoredFloatStats) / sizeof(float);

// ── Stat categorization for v0.2.11 ──
// PERSONAL float stats: body, weapon skills — each player keeps their own
static constexpr int kPersonalFloatStats[] = {
	21,  // STAT_FAT
	22,  // STAT_STAMINA
	23,  // STAT_MUSCLE
	24,  // STAT_MAX_HEALTH
	25,  // STAT_SEX_APPEAL
	69,  // STAT_SKILL_PISTOL
	70,  // STAT_SKILL_PISTOL_SILENCED
	71,  // STAT_SKILL_DESERT_EAGLE
	72,  // STAT_SKILL_SHOTGUN
	73,  // STAT_SKILL_SAWNOFF_SHOTGUN
	74,  // STAT_SKILL_COMBAT_SHOTGUN
	75,  // STAT_SKILL_UZI
	76,  // STAT_SKILL_MP5
	77,  // STAT_SKILL_AK47
	78,  // STAT_SKILL_M4
	79,  // STAT_SKILL_SNIPER
	80,  // STAT_SEX_APPEAL_CLOTHES
	81,  // STAT_GAMBLING
};

// PERSONAL int stats (array offset = stat_id - 120): driving, flying, etc.
static constexpr int kPersonalIntStatOffsets[] = {
	40,   // STAT_DRIVING_SKILL (160-120)
	103,  // STAT_FLYING_SKILL (223-120)
	105,  // STAT_LUNG_CAPACITY (225-120)
	109,  // STAT_BIKE_SKILL (229-120)
	110,  // STAT_CYCLE_SKILL (230-120)
};

// SHARED int stats (MAX-merged): side mission progress
static constexpr int kSharedMaxIntStatOffsets[] = {
	29,   // STAT_TAXI_MONEY_MADE (149-120)
	32,   // STAT_TOTAL_FIRES_EXTINGUISHED (152-120)
	37,   // STAT_HIGHEST_VIGILANTE_MISSION_LEVEL (157-120)
	38,   // STAT_HIGHEST_PARAMEDIC_MISSION_LEVEL (158-120)
	39,   // STAT_HIGHEST_FIREFIGHTER_MISSION_LEVEL (159-120)
};

static bool IsPersonalFloatStat(int index)
{
	for (int s : kPersonalFloatStats)
		if (s == index) return true;
	return false;
}

static bool IsPersonalIntStat(int offset)
{
	for (int s : kPersonalIntStatOffsets)
		if (s == offset) return true;
	return false;
}

static bool IsSharedMaxIntStat(int offset)
{
	for (int s : kSharedMaxIntStatOffsets)
		if (s == offset) return true;
	return false;
}

static void SyncLocalNetworkPlayerProgressCache()
{
	if (auto localNetworkPlayer = CNetworkPlayerManager::GetPlayer(CNetworkPlayerManager::m_nMyId))
	{
		localNetworkPlayer->m_ownedProperties = g_ownedProperties;
		localNetworkPlayer->m_schoolProgress = g_schoolProgress;
		localNetworkPlayer->m_schoolMedals = g_schoolMedals;
	}
}

static int32_t ReadScmGlobalInt(std::size_t variableIndex)
{
	return *reinterpret_cast<int32_t*>(&CTheScripts::ScriptSpace[variableIndex * sizeof(int32_t)]);
}

static void WriteScmGlobalInt(std::size_t variableIndex, int32_t value)
{
	*reinterpret_cast<int32_t*>(&CTheScripts::ScriptSpace[variableIndex * sizeof(int32_t)]) = value;
}

static void PushOwnedPropertiesToScm()
{
	for (std::size_t i = 0; i < g_ownedProperties.size(); ++i)
	{
		WriteScmGlobalInt(kScmAlreadyBoughtHouseBase + i, g_ownedProperties[i] ? 1 : 0);
	}
}

static bool PullOwnedPropertiesFromScm(bool sendUpdateIfChanged)
{
	bool changed = false;
	for (std::size_t i = 0; i < g_ownedProperties.size(); ++i)
	{
		const uint8_t next = ReadScmGlobalInt(kScmAlreadyBoughtHouseBase + i) != 0 ? 1 : 0;
		if (g_ownedProperties[i] != next)
		{
			g_ownedProperties[i] = next;
			changed = true;
		}
	}

	if (changed)
	{
		SyncLocalNetworkPlayerProgressCache();
		if (sendUpdateIfChanged && CNetwork::m_bConnected)
		{
			CStatsSync::NotifyChanged();
		}
	}

	return changed;
}

void CStatsSync::ApplyNetworkPlayerContext(CNetworkPlayer* player)
{
    for (int i = 0; i < MAX_INT_STATS; ++i)
        m_anStoredIntStats[i] = CStats::StatTypesInt[i];

    for (int i = 0; i < MAX_FLOAT_STATS; ++i)
        m_afStoredFloatStats[i] = CStats::StatTypesFloat[i];

    for (int i = 0; i < MAX_INT_STATS; ++i)
        CStats::StatTypesInt[i] = player->m_stats.m_aStatsInt[i];

    for (int i = 0; i < MAX_FLOAT_STATS; ++i)
        CStats::StatTypesFloat[i] = player->m_stats.m_aStatsFloat[i];
}

void CStatsSync::ApplyLocalContext()
{
    for (int i = 0; i < MAX_INT_STATS; ++i)
        CStats::StatTypesInt[i] = m_anStoredIntStats[i];

    for (int i = 0; i < MAX_FLOAT_STATS; ++i)
        CStats::StatTypesFloat[i] = m_afStoredFloatStats[i];
}


void CStatsSync::NotifyChanged()
{
    CPackets::PlayerStats packet{};

	PullOwnedPropertiesFromScm(false);

    CPlayerInfo* playerInfo = &CWorld::Players[0];
    if (playerInfo)
    {
        packet.progress.money = playerInfo->m_nMoney;
    }

    CPlayerPed* localPlayer = FindPlayerPed(0);
    if (localPlayer && localPlayer->GetWanted())
    {
        packet.progress.wantedLevel = static_cast<uint8_t>(localPlayer->GetWanted()->m_nWantedLevel);
    }

    memcpy(packet.progress.floatStats, CStats::StatTypesFloat, sizeof(packet.progress.floatStats));
    memcpy(packet.progress.intStats, CStats::StatTypesInt, sizeof(packet.progress.intStats));
	memcpy(packet.progress.ownedProperties, g_ownedProperties.data(), sizeof(packet.progress.ownedProperties));
	memcpy(packet.progress.schoolProgress, g_schoolProgress.data(), sizeof(packet.progress.schoolProgress));
	memcpy(packet.progress.schoolMedals, g_schoolMedals.data(), sizeof(packet.progress.schoolMedals));

    CNetwork::SendPacket(CPacketsID::PLAYER_STATS, &packet, sizeof packet, ENET_PACKET_FLAG_RELIABLE);
}

bool CStatsSync::IsSupportedStat(eStats stat)
{
    return stat >= 0 && stat < MAX_FLOAT_STATS + MAX_INT_STATS;
}

void CStatsSync::ApplyProgressSnapshot(const PlayerProgressState& progress)
{
	// v0.2.11: Selective stat application
	// FLOAT STATS: only apply non-personal stats
	for (int i = 0; i < MAX_FLOAT_STATS; ++i)
	{
		if (!IsPersonalFloatStat(i))
			CStats::StatTypesFloat[i] = progress.floatStats[i];
	}

	// INT STATS: personal stats stay local, shared MAX stats use max, rest overwrite
	for (int i = 0; i < MAX_INT_STATS; ++i)
	{
		if (IsPersonalIntStat(i))
			continue; // keep local value

		if (IsSharedMaxIntStat(i))
		{
			// MAX-merge: side mission progress — take the higher value
			if (progress.intStats[i] > CStats::StatTypesInt[i])
				CStats::StatTypesInt[i] = progress.intStats[i];
		}
		else
		{
			CStats::StatTypesInt[i] = progress.intStats[i];
		}
	}

	// v0.2.11: Money is SEPARATE — don't overwrite local money

	// v0.2.11: Wanted level uses MAX of both players
	if (auto localPlayer = FindPlayerPed(0))
	{
		if (auto wanted = localPlayer->GetWanted())
		{
			uint8_t localLevel = static_cast<uint8_t>(wanted->m_nWantedLevel);
			uint8_t remoteLevel = progress.wantedLevel;
			uint8_t maxLevel = (remoteLevel > localLevel) ? remoteLevel : localLevel;
			if (maxLevel != localLevel)
				wanted->SetWantedLevelNoDrop(maxLevel);
		}
	}

	// v0.2.11: Properties are SEPARATE — don't overwrite local properties

	// Schools still shared
	memcpy(g_schoolProgress.data(), progress.schoolProgress, sizeof(progress.schoolProgress));
	memcpy(g_schoolMedals.data(), progress.schoolMedals, sizeof(progress.schoolMedals));
	SyncLocalNetworkPlayerProgressCache();
}

bool CStatsSync::SetFloatStat(std::size_t index, float value)
{
	if (index >= COOPANDREAS_FLOAT_STATS_COUNT)
		return false;

	const eStats statId = static_cast<eStats>(index);
	const float oldValue = CStats::GetStatValue(statId);
	if (oldValue == value)
		return false;

	CStats::SetStatValue(statId, value);

	if (CNetwork::m_bConnected)
		NotifyChanged();

	return true;
}

float CStatsSync::GetFloatStat(std::size_t index)
{
	if (index >= COOPANDREAS_FLOAT_STATS_COUNT)
		return 0.0f;

	return CStats::GetStatValue(static_cast<eStats>(index));
}

bool CStatsSync::AddFloatStat(std::size_t index, float delta)
{
	if (index >= COOPANDREAS_FLOAT_STATS_COUNT)
		return false;

	return SetFloatStat(index, GetFloatStat(index) + delta);
}

bool CStatsSync::SetIntStat(std::size_t index, int32_t value)
{
	if (index >= COOPANDREAS_INT_STATS_COUNT)
		return false;

	if (CStats::StatTypesInt[index] == value)
		return false;

	CStats::StatTypesInt[index] = value;

	if (CNetwork::m_bConnected)
		NotifyChanged();

	return true;
}

int32_t CStatsSync::GetIntStat(std::size_t index)
{
	if (index >= COOPANDREAS_INT_STATS_COUNT)
		return 0;

	return CStats::StatTypesInt[index];
}

bool CStatsSync::AddIntStat(std::size_t index, int32_t delta)
{
	if (index >= COOPANDREAS_INT_STATS_COUNT)
		return false;

	return SetIntStat(index, GetIntStat(index) + delta);
}

bool CStatsSync::SetOwnedProperty(std::size_t index, bool isOwned)
{
	if (index >= g_ownedProperties.size())
		return false;

	const uint8_t next = isOwned ? 1 : 0;
	if (g_ownedProperties[index] == next)
		return false;

	g_ownedProperties[index] = next;
	PushOwnedPropertiesToScm();
	SyncLocalNetworkPlayerProgressCache();

	if (CNetwork::m_bConnected)
		NotifyChanged();

	return true;
}

uint8_t CStatsSync::GetOwnedProperty(std::size_t index)
{
	if (index >= g_ownedProperties.size())
		return 0;
	return g_ownedProperties[index];
}

bool CStatsSync::SetSchoolProgress(std::size_t index, uint8_t value)
{
	if (index >= g_schoolProgress.size())
		return false;

	if (g_schoolProgress[index] == value)
		return false;

	g_schoolProgress[index] = value;
	SyncLocalNetworkPlayerProgressCache();

	if (CNetwork::m_bConnected)
		NotifyChanged();

	return true;
}

uint8_t CStatsSync::GetSchoolProgress(std::size_t index)
{
	if (index >= g_schoolProgress.size())
		return 0;
	return g_schoolProgress[index];
}

bool CStatsSync::SetSchoolMedal(std::size_t index, uint8_t value)
{
	if (index >= g_schoolMedals.size())
		return false;

	if (g_schoolMedals[index] == value)
		return false;

	g_schoolMedals[index] = value;
	SyncLocalNetworkPlayerProgressCache();

	if (CNetwork::m_bConnected)
		NotifyChanged();

	return true;
}

uint8_t CStatsSync::GetSchoolMedal(std::size_t index)
{
	if (index >= g_schoolMedals.size())
		return 0;
	return g_schoolMedals[index];
}

void CStatsSync::PollScriptProgress()
{
	PullOwnedPropertiesFromScm(true);
}
