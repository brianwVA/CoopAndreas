#pragma once
#include <array>
#include "../../shared/player_progress.h"
class CNetworkPlayer
{
public:
	CPlayerPed* m_pPed = nullptr;
	int m_iPlayerId;

	CPackets::PlayerOnFoot m_playerOnFoot{};
	
	signed short m_oShockButtonL;
	signed short m_lShockButtonL;

	CVector* m_vecWaypointPos = nullptr;
	bool m_bWaypointPlaced = false;

	char m_Name[32 + 1] = { 0 };

	CControllerState m_oldControllerState{};
	CControllerState m_newControllerState{};
	CCompressedControllerState m_compressedControllerState{};

	CPackets::PlayerAimSync m_aimSyncData;

	CNetworkPlayerStats m_stats{};
	int32_t m_nMoney = 0;
	uint8_t m_nWantedLevel = 0;
	std::array<uint8_t, COOPANDREAS_PROPERTY_COUNT> m_ownedProperties{};
	std::array<uint8_t, COOPANDREAS_SCHOOL_PROGRESS_COUNT> m_schoolProgress{};
	std::array<uint8_t, COOPANDREAS_SCHOOL_MEDAL_COUNT> m_schoolMedals{};
	CPedClothesDesc m_pPedClothesDesc{};
	bool m_bHasBeenConnectedBeforeMe = false;

	CNetworkPlayer::~CNetworkPlayer();
	CNetworkPlayer::CNetworkPlayer(int id, CVector position);

	void CreatePed(int id, CVector position);
	void DestroyPed();
	void Respawn();
	int GetInternalId();
	std::string GetName();
	char GetWeaponSkill(eWeaponType weaponType);
	void WarpIntoVehicleDriver(CVehicle* vehicle);
	void RemoveFromVehicle(CVehicle* vehicle);
	void WarpIntoVehiclePassenger(CVehicle* vehicle, int seatid);
	void EnterVehiclePassenger(CVehicle* vehicle, int seatid);
	void HandleTask(CPackets::SetPlayerTask& packet);
};

