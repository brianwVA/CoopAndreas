#include "stdafx.h"
#include "../../shared/player_progress.h"
#include <array>
#include <algorithm>

int m_anStoredIntStats[COOPANDREAS_INT_STATS_COUNT];
float m_afStoredFloatStats[COOPANDREAS_FLOAT_STATS_COUNT];
static std::array<uint8_t, COOPANDREAS_PROPERTY_COUNT> g_ownedProperties{};
static std::array<uint8_t, COOPANDREAS_SCHOOL_PROGRESS_COUNT> g_schoolProgress{};
static std::array<uint8_t, COOPANDREAS_SCHOOL_MEDAL_COUNT> g_schoolMedals{};

constexpr int MAX_INT_STATS = sizeof(m_anStoredIntStats) / sizeof(int);
constexpr int MAX_FLOAT_STATS = sizeof(m_afStoredFloatStats) / sizeof(float);

static void SyncLocalNetworkPlayerProgressCache()
{
	if (auto localNetworkPlayer = CNetworkPlayerManager::GetPlayer(CNetworkPlayerManager::m_nMyId))
	{
		localNetworkPlayer->m_ownedProperties = g_ownedProperties;
		localNetworkPlayer->m_schoolProgress = g_schoolProgress;
		localNetworkPlayer->m_schoolMedals = g_schoolMedals;
	}
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
	memcpy(CStats::StatTypesFloat, progress.floatStats, sizeof(progress.floatStats));
	memcpy(CStats::StatTypesInt, progress.intStats, sizeof(progress.intStats));

	if (auto playerInfo = &CWorld::Players[0])
	{
		playerInfo->m_nMoney = progress.money;
		playerInfo->m_nDisplayMoney = progress.money;
	}

	if (auto localPlayer = FindPlayerPed(0))
	{
		if (auto wanted = localPlayer->GetWanted())
		{
			wanted->SetWantedLevelNoDrop(progress.wantedLevel);
		}
	}

	memcpy(g_ownedProperties.data(), progress.ownedProperties, sizeof(progress.ownedProperties));
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
