#include "stdafx.h"
#include "PlayerHooks.h"
#include "CKeySync.h"
#include "CAimSync.h"
#include <game_sa/CPedDamageResponseInfo.h>

// Stored weapon inventory for death pickup sync (updated each frame)
static CPackets::DeathPickups s_storedDeathPickups{};
static bool s_downedActive = false;
static bool s_downedAnnounced = false;
static bool s_downedTimedOutPendingDeath = false;
static uint32_t s_downedStartTick = 0;
static constexpr uint32_t kDownedDurationMs = 60000;
static constexpr float kDownedMinHealth = 5.0f;
static CVector s_downedFreezePos{};
static uint32_t s_downedAnimTick = 0;
static bool s_remoteDownedAnimApplied[MAX_SERVER_PLAYERS] = {};
static void SendReviveWindowMarker(CPlayerPed* ped);

static void ActivateDownedState(CPlayerPed* ped)
{
    if (!ped)
    {
        return;
    }

    s_downedActive = true;
    s_downedStartTick = GetTickCount();
    s_downedFreezePos = ped->GetPosition();
    s_downedAnimTick = 0;
    s_downedTimedOutPendingDeath = false;

    if (!s_downedAnnounced)
    {
        CChat::AddMessage("~b~Downed: you are immobilized for 60s, teammate can revive (J)");
        s_downedAnnounced = true;
    }

    SendReviveWindowMarker(ped);
}

static void ResetDownedInputLocks()
{
    if (CPad* pad = CPad::GetPad(0))
    {
        pad->bDisablePlayerEnterCar = 0;
        pad->bDisablePlayerDuck = 0;
        pad->bDisablePlayerJump = 0;
        pad->bDisablePlayerFireWeapon = 0;
        pad->bDisablePlayerFireWeaponWithL1 = 0;
        pad->bDisablePlayerCycleWeapon = 0;
    }
}

static void ApplyDownedAnimation(CPed* ped, bool alternate)
{
    if (!ped)
    {
        return;
    }

    int pedRef = CPools::GetPedRef(ped);
    if (pedRef < 0)
    {
        return;
    }

    // Use prone hit animations from PED so downed players look actually collapsed on the ground.
    const char* animName = alternate ? "KO_shot_stom" : "KO_shot_front";
    // Do not loop the downed animation - looping KO_* causes visual
    // "stand up / fall down" bouncing on some clients.
    plugin::Command<Commands::TASK_PLAY_ANIM_NON_INTERRUPTABLE>(
        pedRef,
        animName,
        "PED",
        4.0f,
        0,
        1,
        1,
        1,
        -1);
}

static void SendReviveWindowMarker(CPlayerPed* ped)
{
    if (!ped || !CNetwork::m_bConnected)
    {
        return;
    }

    CPackets::DeathPickups marker{};
    CVector pos = ped->GetPosition();
    marker.x = pos.x;
    marker.y = pos.y;
    marker.z = pos.z;
    marker.money = 0;
    marker.weaponCount = 0;
    CNetwork::SendPacket(CPacketsID::DEATH_PICKUPS, &marker, sizeof marker, ENET_PACKET_FLAG_RELIABLE);
}

static void __fastcall CPlayerPed__ProcessControl_Hook(CPlayerPed* This)
{
    CPlayerPed* localPlayer = FindPlayerPed(0);

    if (This == localPlayer)
    {
        if (CNetwork::m_bConnected && !s_downedActive && !s_downedTimedOutPendingDeath && This->m_fHealth <= kDownedMinHealth)
        {
            ActivateDownedState(This);
            This->m_fHealth = kDownedMinHealth;
        }

        if (CNetwork::m_bConnected && s_downedActive)
        {
            if (GetTickCount() - s_downedStartTick >= kDownedDurationMs)
            {
                s_downedActive = false;
                s_downedAnnounced = false;
                s_downedTimedOutPendingDeath = true;
                ResetDownedInputLocks();
                This->m_fHealth = 0.0f;
                int pedRef = CPools::GetPedRef(This);
                if (pedRef >= 0)
                {
                    // Force an actual death transition when revive timeout ends.
                    // Some game states ignore direct m_fHealth writes.
                    plugin::Command<0x0223>(pedRef, 0);
                }
                return;
            }

            if (This->m_fHealth < kDownedMinHealth)
            {
                This->m_fHealth = kDownedMinHealth;
            }

            // Hard downed: immobilize and disable actions while waiting for revive.
            This->m_matrix->pos = s_downedFreezePos;
            This->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
            This->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);

            if (This->m_pVehicle)
            {
                This->m_pVehicle->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
                This->m_pVehicle->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
                This->m_pVehicle->m_fGasPedal = 0.0f;
                This->m_pVehicle->m_fBreakPedal = 1.0f;
            }

            if (CPad* pad = CPad::GetPad(0))
            {
                pad->bDisablePlayerEnterCar = 1;
                pad->bDisablePlayerDuck = 1;
                pad->bDisablePlayerJump = 1;
                pad->bDisablePlayerFireWeapon = 1;
                pad->bDisablePlayerFireWeaponWithL1 = 1;
                pad->bDisablePlayerCycleWeapon = 1;
            }

            uint32_t now = GetTickCount();
            if (s_downedAnimTick == 0)
            {
                ApplyDownedAnimation(This, false);
                s_downedAnimTick = now;
            }
        }
        else
        {
            ResetDownedInputLocks();
        }

        patch::SetRaw(0x6884C4, (void*)"\xD9\x96\x5C\x05\x00\x00", 6, false);
        plugin::CallMethod<0x60EA90, CPlayerPed*>(This);
        patch::Nop(0x6884C4, 6, false);

        // Capture weapon inventory for death pickup sync
        {
            s_storedDeathPickups = {};
            CVector pos = This->GetPosition();
            s_storedDeathPickups.x = pos.x;
            s_storedDeathPickups.y = pos.y;
            s_storedDeathPickups.z = pos.z;

            s_storedDeathPickups.money = CWorld::Players[0].m_nMoney;

            unsigned char count = 0;
            for (int i = 0; i < 13; i++)
            {
                auto& wep = This->m_aWeapons[i];
                if (wep.m_eWeaponType != WEAPON_UNARMED && wep.m_nTotalAmmo > 0)
                {
                    s_storedDeathPickups.weapons[count].weaponType = wep.m_eWeaponType;
                    s_storedDeathPickups.weapons[count].ammo = wep.m_nTotalAmmo;
                    count++;
                }
            }
            s_storedDeathPickups.weaponCount = count;
        }

        return;
    }

    CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(This);

    if (player == nullptr)
    {
        plugin::CallMethod<0x60EA90, CPlayerPed*>(This);
        return;
    }

    int playerNum = player->GetInternalId();

    if (playerNum == -1)
    {
        plugin::CallMethod<0x60EA90, CPlayerPed*>(This);
        return;
    }

    CWorld::PlayerInFocus = playerNum;

    CKeySync::ApplyNetworkPlayerContext(player);
    CAimSync::ApplyNetworkPlayerContext(player);
    //CStatsSync::ApplyNetworkPlayerContext(player);

    //if (CPad::GetPad(0)->NewState.RightShoulder1) // is aiming
    //{
    //    player->m_pPed->m_fCurrentRotation = player->m_playerOnFoot.currentRotation;
    //}
    //player->m_pPed->m_fAimingRotation = player->m_playerOnFoot.aimingRotation;

    player->m_pPed->m_fHealth = player->m_playerOnFoot.health;
    player->m_pPed->m_fArmour = player->m_playerOnFoot.armour;

    CTask* activeTask = player->m_pPed->m_pIntelligence->m_TaskMgr.GetActiveTask();

    if (activeTask && activeTask->GetId() != eTaskType::TASK_COMPLEX_JUMP)
    {
        player->m_pPed->m_vecMoveSpeed = player->m_playerOnFoot.velocity;
    }

    plugin::CallMethod<0x60EA90, CPlayerPed*>(This);

    // Smooth position interpolation towards last synced position
    {
        CVector& currentPos = player->m_pPed->m_matrix->pos;
        const CVector& targetPos = player->m_playerOnFoot.position;
        float dx = targetPos.x - currentPos.x;
        float dy = targetPos.y - currentPos.y;
        float dz = targetPos.z - currentPos.z;
        float distSq = dx * dx + dy * dy + dz * dz;

        if (distSq > 0.0001f && distSq < 50.0f * 50.0f)
        {
            float alpha = 0.25f;
            currentPos.x += dx * alpha;
            currentPos.y += dy * alpha;
            currentPos.z += dz * alpha;
        }
        else if (distSq >= 50.0f * 50.0f)
        {
            currentPos = targetPos;
        }
    }

    // Smooth rotation interpolation
    {
        float currentRot = player->m_pPed->m_fCurrentRotation;
        float targetRot = player->m_playerOnFoot.currentRotation;
        float diff = targetRot - currentRot;
        // Wrap angle to [-PI, PI]
        while (diff > 3.14159f) diff -= 6.28318f;
        while (diff < -3.14159f) diff += 6.28318f;
        if (diff * diff > 0.0001f)
        {
            player->m_pPed->m_fCurrentRotation = currentRot + diff * 0.25f;
        }
    }

    CWorld::PlayerInFocus = 0;

    // Render remote downed animation when that player is in downed-like synced state.
    {
        int remoteId = player->m_iPlayerId;
        bool remoteDowned = player->m_playerOnFoot.health <= (unsigned char)kDownedMinHealth
            && !player->m_pPed->m_nPedFlags.bInVehicle;

        if (remoteId >= 0 && remoteId < MAX_SERVER_PLAYERS)
        {
            if (remoteDowned)
            {
                if (!s_remoteDownedAnimApplied[remoteId])
                {
                    ApplyDownedAnimation(player->m_pPed, false);
                    s_remoteDownedAnimApplied[remoteId] = true;
                }
            }
            else
            {
                s_remoteDownedAnimApplied[remoteId] = false;
            }
        }
    }

    CKeySync::ApplyLocalContext();
    CAimSync::ApplyLocalContext();
    //CStatsSync::ApplyLocalContext();
}

static void __fastcall CWeapon__DoBulletImpact_Hook(CWeapon* weapon, SKIP_EDX, CEntity* owner, CEntity* victim, CVector* startPoint, CVector* endPoint, CColPoint* colPoint, int incrementalHit)
{
    if (owner == FindPlayerPed(0) && victim)
    {
        CPackets::PlayerBulletShot packet{};
        packet.entityType = victim->m_nType + 1;

        packet.targetid = -1;

        if (victim != nullptr)
        {
            switch (victim->m_nType)
            {
            case eEntityType::ENTITY_TYPE_PED: // ped or player
            {
                if (auto playerTarget = CNetworkPlayerManager::GetPlayer(victim))
                {
                    packet.targetid = playerTarget->m_iPlayerId;
                    packet.entityType = eNetworkEntityType::NETWORK_ENTITY_TYPE_PLAYER;
                }
                else if (auto pedTarget = CNetworkPedManager::GetPed(victim))
                {
                    packet.targetid = pedTarget->m_nPedId;
                }
                break;
            }
            case eEntityType::ENTITY_TYPE_VEHICLE:
            {
                if (auto vehicleTarget = CNetworkVehicleManager::GetVehicle(victim))
                    packet.targetid = vehicleTarget->m_nVehicleId;
                break;
            }
            }
        }

        packet.startPos = *startPoint;
        packet.endPos = *endPoint;
        packet.colPoint = *colPoint;
        packet.incrementalHit = incrementalHit;

        CNetwork::SendPacket(CPacketsID::PLAYER_BULLET_SHOT, &packet, sizeof(packet));

        weapon->DoBulletImpact(owner, victim, startPoint, endPoint, colPoint, incrementalHit);

        return;
    }
    else if (owner->m_nType == eEntityType::ENTITY_TYPE_PED)
    {
        CPed* ped = (CPed*)owner;

        if (ped->m_nPedType > PED_TYPE_PLAYER1)
        {
            weapon->DoBulletImpact(owner, victim, startPoint, endPoint, colPoint, incrementalHit);
        }
    }
}

static void __fastcall CPedIK__PointGunInDirection_Hook(CPedIK* This, SKIP_EDX, float dirX, float dirY, char flag, float float1)
{
    //if (This->m_pPed == FindPlayerPed(0))
    //{
    //    // save last aim data for syncing
    //    CLocalPlayer::m_vecLastAimX = dirX;
    //    CLocalPlayer::m_vecLastAimY = dirY;

    //    This->PointGunInDirection(dirX, dirY, flag, float1);
    //    return;
    //}

    //CNetworkPlayer* player = CNetworkPlayerManager::GetPlayer(This->m_pPed);

    //if (player == nullptr)
    //{
    //    This->PointGunInDirection(dirX, dirY, flag, float1);
    //    return;
    //}

    //if (player->m_playerOnFoot == nullptr)
    //    return;

    //player->m_pPed->m_fAimingRotation = player->m_playerOnFoot.aimingRotation;
    //
    //eWeaponType weapon = player->m_pPed->m_aWeapons[player->m_pPed->m_nActiveWeaponSlot].m_eWeaponType;

    ///*if (weapon != WEAPON_SNIPERRIFLE)
    //    dirY = player->m_aimSyncData.aimY;*/

    This->PointGunInDirection(dirX, dirY, flag, float1);
}

static void __fastcall CPlayerPed__dctor_Hook(CPlayerPed* This, SKIP_EDX)
{
    This->m_pPlayerData->m_nMeleeWeaponAnimReferencedExtra = 0;
    plugin::CallMethod<0x6093B0, CPlayerPed*>(This);
}

void CReferences__RemoveReferencesToPlayer_Hook()
{
    if (s_downedActive)
    {
        return;
    }

    s_downedTimedOutPendingDeath = false;

    plugin::Call<0x571AD0>(); // CReferences::RemoveReferencesToPlayer();

    if (CNetwork::m_bConnected)
    {
        if (s_storedDeathPickups.weaponCount > 0 || s_storedDeathPickups.money > 0)
        {
            CNetwork::SendPacket(CPacketsID::DEATH_PICKUPS, &s_storedDeathPickups, sizeof s_storedDeathPickups, ENET_PACKET_FLAG_RELIABLE);
        }

        CPackets::RespawnPlayer packet{};
        CNetwork::SendPacket(CPacketsID::RESPAWN_PLAYER, &packet, sizeof packet, ENET_PACKET_FLAG_RELIABLE);
    }
}

bool __fastcall CWeapon__TakePhotograph_Hook(CWeapon* This, SKIP_EDX, CEntity* entity, CVector* point)
{
    if (entity == FindPlayerPed(0))
    {
        return This->TakePhotograph(entity, point);
    }

    return false;
}

void __fastcall CTaskSimpleJetPack__DropJetPack_Hook(CTaskSimpleJetPack* This, SKIP_EDX, CPed* ped)
{
    if (ped != FindPlayerPed(0) && ped->IsPlayer())
    {
        // dont create a pickup if the player is network
        This->m_bIsFinished = true;
    }

    plugin::CallMethod<0x67B660, CTaskSimpleJetPack*>(This, ped); // CTaskSimpleJetPack::DropJetPack
}

void __fastcall CPedDamageResponseCalculator__ComputeWillKillPed_Hook(uintptr_t This, SKIP_EDX, CPed* ped, CPedDamageResponseInfo* dmgResponse, bool speak)
{
    plugin::CallMethod<0x4B3210, uintptr_t>(This, ped, dmgResponse, speak);

    if (ped == FindPlayerPed(0))
    {
        if (CNetwork::m_bConnected)
        {
            if (s_downedTimedOutPendingDeath)
            {
                return;
            }

            float projectedHealth = ped->m_fHealth - dmgResponse->m_fDamageHealth;
            bool shouldEnterDowned = !s_downedActive && projectedHealth <= kDownedMinHealth;

            if (shouldEnterDowned)
            {
                ActivateDownedState((CPlayerPed*)ped);
            }

            if (s_downedActive && projectedHealth <= kDownedMinHealth)
            {
                ped->m_fHealth = kDownedMinHealth;
                dmgResponse->m_bHealthZero = false;
                dmgResponse->m_bForceDeath = false;
                dmgResponse->m_fDamageHealth = 0.0f;
                dmgResponse->m_fDamageArmor = 0.0f;
            }
        }
        return;
    }

    if (ped->m_nPedType > 3) // peds
    {
        if (auto networkPed = CNetworkPedManager::GetPed(ped))
        {
            if (!networkPed->m_bSyncing)
            {
                if (networkPed->m_fHealth >= 1.0f)
                {
                    ped->m_fHealth = networkPed->m_fHealth;
                    dmgResponse->m_bHealthZero = false;
                    dmgResponse->m_bForceDeath = false;
                    dmgResponse->m_fDamageHealth = 0.0f;
                    dmgResponse->m_fDamageArmor = 0.0f;
                }
            }
        }

        return;
    }

    // players
    if (auto networkPlayer = CNetworkPlayerManager::GetPlayer(ped))
    {
        if (networkPlayer->m_playerOnFoot.health >= 1.0f)
        {
            ped->m_fHealth = networkPlayer->m_playerOnFoot.health;
            dmgResponse->m_bHealthZero = false;
            dmgResponse->m_bForceDeath = false;
            dmgResponse->m_fDamageHealth = 0.0f;
            dmgResponse->m_fDamageArmor = 0.0f;
        }
    }
}

bool __fastcall CPlayerPed__CanPlayerStartMission_Hook(CPlayerPed* This, SKIP_EDX)
{
    return This->CanPlayerStartMission()/* && CLocalPlayer::m_bIsHost*/;
}

void __fastcall CPlayerPed__ProcessWeaponSwitch_Hook(CPlayerPed* This, SKIP_EDX, CPad* pad)
{
    if (CWorld::PlayerInFocus == 0)
    {
        This->ProcessWeaponSwitch(pad);
    }
}

void __fastcall CTaskSimplePlayerOnFoot__PlayIdleAnimations_Hook(CTaskSimplePlayerOnFoot* This, SKIP_EDX, CPlayerPed* playerPed)
{
    // Allow idle animations for all players (local + remote).
    // Each client independently picks random idles — not synced across
    // players, but looks much better than remote players never idling.
    plugin::CallMethod<0x6872C0>(This, playerPed);
}

bool __fastcall CPad__JumpJustDown_Hook(CPad* This)
{
    if (CWorld::PlayerInFocus == 0)
    {
        return This->JumpJustDown();
    }

    return false;
}

void __fastcall CTaskComplexJump_CTaskManager__SetTask_Hook(CTaskManager* This, SKIP_EDX, CTask* task, int tasksId, bool a4)
{
    This->SetTask(task, tasksId, a4);

    if (CWorld::PlayerInFocus == 0)
    {
        CLocalPlayer::BuildTaskPacket(TASK_COMPLEX_JUMP);
    }
}

void __fastcall CRunningScript__DoDeatharrestCheck_Hook(CRunningScript* This, SKIP_EDX)
{
    if (!This->m_bWastedBustedCheck) 
    {
        return;
    }

    if (!CTheScripts::IsPlayerOnAMission()) 
    {
        return;
    }

    if (CNetwork::m_bConnected)
    {
        CPlayerPed* localPed = FindPlayerPed(0);
        if (s_downedActive || (localPed && localPed->m_fHealth <= kDownedMinHealth && !s_downedTimedOutPendingDeath))
        {
            // While downed, suppress mission wasted flow and let revive/timeout logic decide.
            This->m_bWastedOrBusted = false;
            return;
        }
    }

    bool wastedOrBusted = false;
    
    auto* playerInfo = &CWorld::Players[CWorld::PlayerInFocus];

    if (playerInfo->m_nPlayerState == ePlayerState::PLAYERSTATE_HASDIED ||
        playerInfo->m_nPlayerState == ePlayerState::PLAYERSTATE_HASBEENARRESTED)
    {
        wastedOrBusted = true;
    }

    if (!wastedOrBusted)
    {
        for (auto networkPlayer : CNetworkPlayerManager::m_pPlayers)
        {
            if (auto ped = networkPlayer->m_pPed)
            {
                if (ped->m_ePedState == PEDSTATE_ARRESTED ||
                    ped->m_ePedState == PEDSTATE_DEAD ||
                    (ped->m_ePedState == PEDSTATE_DIE && ped->m_nPedFlags.bIsDyingStuck))
                {
                    wastedOrBusted = true;
                }
            }
        }
    }

    if (wastedOrBusted)
    {
        CMessages::ClearSmallMessagesOnly();
        memset(&CTheScripts::ScriptSpace[CTheScripts::OnAMissionFlag], 0, sizeof(uint32_t));
        
        if (This->m_nSP > 1u)
        {
            uint16_t nsp = This->m_nSP;
            do
            {
                --nsp;
            } while (nsp > 1u);
            This->m_nSP = nsp;
        }
        This->m_pCurrentIP = This->m_apStack[--This->m_nSP];

        This->m_bWastedOrBusted = true;
        This->m_nWakeTime = 0;
    }
}

void PlayerHooks::InjectHooks()
{
    patch::SetPointer(0x86D190, CPlayerPed__ProcessControl_Hook);

    patch::RedirectCall(0x73CD92, CWeapon__DoBulletImpact_Hook);
    patch::RedirectCall(0x741199, CWeapon__DoBulletImpact_Hook);
    patch::RedirectCall(0x7411DF, CWeapon__DoBulletImpact_Hook);
    patch::RedirectCall(0x7412DF, CWeapon__DoBulletImpact_Hook);
    patch::RedirectCall(0x741E30, CWeapon__DoBulletImpact_Hook);

    patch::RedirectCall(0x5FDF7A, CPedIK__PointGunInDirection_Hook);
    patch::RedirectCall(0x61F351, CPedIK__PointGunInDirection_Hook);
    patch::RedirectCall(0x62876D, CPedIK__PointGunInDirection_Hook);

    // fix CPlayerPed dctor crash
    patch::RedirectCall(0x60A9A3, CPlayerPed__dctor_Hook);

    // called when the player respawns after being busted or wasted
    patch::RedirectCall(0x443082, CReferences__RemoveReferencesToPlayer_Hook);

    patch::RedirectCall(0x74278B, CWeapon__TakePhotograph_Hook);

    patch::RedirectCall(0x5707AE, CTaskSimpleJetPack__DropJetPack_Hook);
    patch::RedirectCall(0x67E840, CTaskSimpleJetPack__DropJetPack_Hook);

    patch::RedirectCall(0x4B5B27, CPedDamageResponseCalculator__ComputeWillKillPed_Hook);

    patch::RedirectCall(0x4577E6, CPlayerPed__CanPlayerStartMission_Hook);
    patch::RedirectCall(0x4895B0, CPlayerPed__CanPlayerStartMission_Hook);

    patch::RedirectCall(0x60F2E0, CPlayerPed__ProcessWeaponSwitch_Hook); // disable switching weapon for network players

    patch::RedirectCall(0x6887E2, CTaskSimplePlayerOnFoot__PlayIdleAnimations_Hook);

    patch::RedirectCall(0x688700, CPad__JumpJustDown_Hook);
    patch::RedirectCall(0x6887D8, CTaskComplexJump_CTaskManager__SetTask_Hook);

    patch::ReplaceFunction(0x485A50, CRunningScript__DoDeatharrestCheck_Hook);
}

void PlayerHooks::NotifyLocalRevived()
{
    s_downedActive = false;
    s_downedAnnounced = false;
    s_downedTimedOutPendingDeath = false;
    s_downedAnimTick = 0;
    ResetDownedInputLocks();
}

bool PlayerHooks::IsLocalPlayerDowned()
{
    if (s_downedActive)
    {
        return true;
    }

    CPlayerPed* localPed = FindPlayerPed(0);
    if (!localPed)
    {
        return false;
    }

    return (localPed->m_fHealth <= kDownedMinHealth) && !s_downedTimedOutPendingDeath;
}
