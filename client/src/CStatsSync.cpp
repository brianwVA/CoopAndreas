#include "stdafx.h"
#include "../../shared/player_progress.h"

int m_anStoredIntStats[COOPANDREAS_INT_STATS_COUNT];
float m_afStoredFloatStats[COOPANDREAS_FLOAT_STATS_COUNT];

constexpr int MAX_INT_STATS = sizeof(m_anStoredIntStats) / sizeof(int);
constexpr int MAX_FLOAT_STATS = sizeof(m_afStoredFloatStats) / sizeof(float);

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

    CNetwork::SendPacket(CPacketsID::PLAYER_STATS, &packet, sizeof packet, ENET_PACKET_FLAG_RELIABLE);
}

bool CStatsSync::IsSupportedStat(eStats stat)
{
    return stat >= 0 && stat < MAX_FLOAT_STATS + MAX_INT_STATS;
}
