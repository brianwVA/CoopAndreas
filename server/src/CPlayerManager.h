#pragma once

#ifndef _CPLAYERMANAGER_H_
	#define _CPLAYERMANAGER_H_
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "CControllerState.h"
#include "CPlayer.h"
#include "NetworkEntityType.h"
#include "PlayerDisconnectReason.h"
#include "../../shared/player_progress.h"

class CPlayerManager
{
	public:
		CPlayerManager();
		
		static std::vector<CPlayer*> m_pPlayers;
		static void Add(CPlayer* player);
		static void Remove(CPlayer* player);
		static CPlayer* GetPlayer(int playerid);
		static CPlayer* GetPlayer(ENetPeer* peer);
		static int GetFreeID();
		static CPlayer* GetHost();
		static void AssignHostToFirstPlayer();
		
		~CPlayerManager();
};


class CPlayerPackets
{
public:
	CPlayerPackets();
	struct DeathReviveState
	{
		bool active = false;
		CVector pos{};
		uint32_t deathTick = 0;
	};

	static inline std::array<DeathReviveState, MAX_SERVER_PLAYERS> ms_deathState{};

	struct DeathPickupSnapshot
	{
		bool active = false;
		uint32_t tick = 0;
		int playerid = -1;
		float x = 0.0f, y = 0.0f, z = 0.0f;
		int money = 0;
		unsigned char weaponCount = 0;
		struct WeaponEntry
		{
			unsigned int weaponType;
			unsigned int ammo;
		} weapons[13];
	};

	struct ItemDropSnapshot
	{
		bool active = false;
		uint32_t tick = 0;
		int playerid = -1;
		float x = 0.0f, y = 0.0f, z = 0.0f;
		unsigned char dropType = 0;
		unsigned int weaponType = 0;
		unsigned int ammo = 0;
		int money = 0;
		int16_t cx = 0, cy = 0, cz = 0;
	};

	static inline std::array<DeathPickupSnapshot, MAX_SERVER_PLAYERS> ms_deathPickupSnapshots{};
	static inline std::array<ItemDropSnapshot, 128> ms_itemDropSnapshots{};

	static void ReplayLateJoinState(ENetPeer* peer)
	{
		uint32_t now = enet_time_get();
		static constexpr uint32_t kReviveWindowMs = 60000;

		for (const auto& snapshot : ms_deathPickupSnapshots)
		{
			if (!snapshot.active)
			{
				continue;
			}

			if ((now - snapshot.tick) > kReviveWindowMs)
			{
				continue;
			}

			struct DeathPickupsPacket
			{
				int playerid;
				float x, y, z;
				int money;
				unsigned char weaponCount;
				struct WeaponEntry
				{
					unsigned int weaponType;
					unsigned int ammo;
				} weapons[13];
			} packet{};
			packet.playerid = snapshot.playerid;
			packet.x = snapshot.x;
			packet.y = snapshot.y;
			packet.z = snapshot.z;
			packet.money = snapshot.money;
			packet.weaponCount = snapshot.weaponCount;
			for (int i = 0; i < 13; i++)
			{
				packet.weapons[i].weaponType = snapshot.weapons[i].weaponType;
				packet.weapons[i].ammo = snapshot.weapons[i].ammo;
			}

			CNetwork::SendPacket(peer, CPacketsID::DEATH_PICKUPS, &packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
		}

		for (const auto& drop : ms_itemDropSnapshots)
		{
			if (!drop.active)
			{
				continue;
			}

			struct ItemDropPacket
			{
				int playerid;
				float x, y, z;
				unsigned char dropType;
				unsigned int weaponType;
				unsigned int ammo;
				int money;
			} packet{};
			packet.playerid = drop.playerid;
			packet.x = drop.x;
			packet.y = drop.y;
			packet.z = drop.z;
			packet.dropType = drop.dropType;
			packet.weaponType = drop.weaponType;
			packet.ammo = drop.ammo;
			packet.money = drop.money;
			CNetwork::SendPacket(peer, CPacketsID::ITEM_DROP, &packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
		}
	}

#pragma pack(1)
	struct PlayerConnected
	{
		int id;
		bool isAlreadyConnected; // prevents spam in the chat when connecting by distinguishing already connected players from newly joined ones
		char name[32 + 1];
		uint32_t version;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (CPlayerManager::GetPlayer(peer))
			{
				return;
			}

			CNetwork::HandlePlayerConnected(peer, data, size);
		}
	};

	struct PlayerDisconnected
	{
		int id;
		ePlayerDisconnectReason reason;
		uint32_t version;
	};

#pragma pack(1)
	struct PlayerOnFoot
	{
		int id = 0;
		CVector position = CVector();
		CVector velocity = CVector();
		float currentRotation = 0.0f;
		float aimingRotation = 0.0f;
		unsigned char health = 100;
		unsigned char armour = 0;
		unsigned char weapon = 0;
		unsigned char weaponState = 0;
		unsigned short ammo = 0;
		bool ducking = false;
		bool hasJetpack = false;
		char fightingStyle = 4;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				// create packet
				CPlayerPackets::PlayerOnFoot* packet = (CPlayerPackets::PlayerOnFoot*)data;

				// set packet`s playerid, cuz incoming packet has id = 0
				packet->id = player->m_iPlayerId;

				bool isValidWeapon = (packet->weapon >= 0 && packet->weapon <= 18) || (packet->weapon >= 22 && packet->weapon <= 46);
				if (!isValidWeapon)
				{
					packet->weapon = 0;
					packet->ammo = 0;
				}

				if (packet->fightingStyle < 4 || packet->fightingStyle > 16)
				{
					packet->fightingStyle = 4;
				}

				if (packet->velocity.x > 10.0f || packet->velocity.y > 10.0f || packet->velocity.z > 10.0f)
				{
					packet->velocity = CVector(0.0f, 0.0f, 0.0f);
				}

				if (player->m_nVehicleId >= 0)
				{
					player->RemoveFromVehicle();
				}

				CNetwork::SendPacketToAll(CPacketsID::PLAYER_ONFOOT, packet, sizeof * packet, (ENetPacketFlag)0, peer);
			}
		}
	};

#pragma pack(1)
	struct PlayerBulletShot
	{
		int playerid;
		int targetid;
		CVector startPos;
		CVector endPos;
		unsigned char colPoint[44]; // padding
		int incrementalHit;
		unsigned char entityType;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			// create packet
			CPlayerPackets::PlayerBulletShot* packet = (CPlayerPackets::PlayerBulletShot*)data;

			// set packet`s playerid, cuz incoming packet has id = 0
			packet->playerid = CPlayerManager::GetPlayer(peer)->m_iPlayerId;

			CNetwork::SendPacketToAll(CPacketsID::PLAYER_BULLET_SHOT, packet, sizeof * packet, (ENetPacketFlag)0, peer);
		}
	};

	struct PlayerHandshake
	{
		int yourid;
	};

	struct PlayerPlaceWaypoint
	{
		int playerid;
		bool place;
		CVector position;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				CPlayerPackets::PlayerPlaceWaypoint* packet = (CPlayerPackets::PlayerPlaceWaypoint*)data;
				packet->playerid = player->m_iPlayerId;
				packet->position.x = std::clamp(packet->position.x, -3000.0f, 3000.0f);
				packet->position.y = std::clamp(packet->position.y, -3000.0f, 3000.0f);
				player->m_ucSyncFlags.bWaypointModified = packet->place != 0;
				player->m_vecWaypointPos = packet->position;
				CNetwork::SendPacketToAll(CPacketsID::PLAYER_PLACE_WAYPOINT, packet, sizeof * packet, ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct PlayerSetHost
	{
		int playerid;
	};

	struct AddExplosion
	{
		unsigned char type;
		CVector pos;
		int time;
		bool usesSound;
		float cameraShake;
		bool isVisible;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			CPlayerPackets::AddExplosion* packet = (CPlayerPackets::AddExplosion*)data;
			CNetwork::SendPacketToAll(CPacketsID::ADD_EXPLOSION, packet, sizeof * packet, ENET_PACKET_FLAG_RELIABLE, peer);
		}
	};

	struct PlayerChatMessage
	{
		int playerid;
		wchar_t message[128 + 1];

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			CPlayerPackets::PlayerChatMessage* packet = (CPlayerPackets::PlayerChatMessage*)data;
			packet->playerid = CPlayerManager::GetPlayer(peer)->m_iPlayerId;
			packet->message[128] = 0;
			CNetwork::SendPacketToAll(CPacketsID::PLAYER_CHAT_MESSAGE, packet, sizeof * packet, ENET_PACKET_FLAG_RELIABLE, peer);
		}
	};

	struct PlayerKeySync
	{
		int playerid;
		CCompressedControllerState newState;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			CPlayerPackets::PlayerKeySync* packet = (CPlayerPackets::PlayerKeySync*)data;
			packet->playerid = CPlayerManager::GetPlayer(peer)->m_iPlayerId;
			CNetwork::SendPacketToAll(CPacketsID::PLAYER_KEY_SYNC, packet, sizeof * packet, (ENetPacketFlag)0, peer);
		}
	};

	struct GameWeatherTime
	{
		unsigned char newWeather;
		unsigned char oldWeather;
		unsigned char forcedWeather;
		unsigned char currentMonth;
		unsigned char currentDay;
		unsigned char currentHour;
		unsigned char currentMinute;
		unsigned int gameTickCount;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (!CPlayerManager::GetPlayer(peer)->m_bIsHost)
				return;

			CPlayerPackets::GameWeatherTime* packet = (CPlayerPackets::GameWeatherTime*)data;
			CNetwork::SendPacketToAll(CPacketsID::GAME_WEATHER_TIME, packet, sizeof * packet, ENET_PACKET_FLAG_RELIABLE, peer);
		}
	};

	struct PlayerAimSync
	{
		int playerid;
		unsigned char cameraMode;
		unsigned char weaponCameraMode;
		float cameraFov;
		CVector front;
		CVector	source;
		CVector	up;
		float lookPitch;
		float orientation;
		CVector sniperDotPos;
		unsigned char sniperDotActive;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			CPlayerPackets::PlayerAimSync* packet = (CPlayerPackets::PlayerAimSync*)data;
			packet->playerid = CPlayerManager::GetPlayer(peer)->m_iPlayerId;
			CNetwork::SendPacketToAll(CPacketsID::PLAYER_AIM_SYNC, packet, sizeof * packet, (ENetPacketFlag)0, peer);
		}
	};

	struct PlayerStats
	{
		int playerid;
		PlayerProgressState progress;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				CPlayerPackets::PlayerStats* packet = (CPlayerPackets::PlayerStats*)data;
				packet->playerid = player->m_iPlayerId;
				CNetwork::SendPacketToAll(CPacketsID::PLAYER_STATS, packet, sizeof * packet, ENET_PACKET_FLAG_RELIABLE, peer);

				memcpy(&player->m_progress, &packet->progress, sizeof(packet->progress));
				player->m_ucSyncFlags.bProgressModified = true;
			}
		}
	};

	struct RebuildPlayer
	{
		int playerid;
		// CPedClothesDesc inlined
		unsigned int m_anModelKeys[10];
		unsigned int m_anTextureKeys[18];
		float m_fFatStat;
		float m_fMuscleStat;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				CPlayerPackets::RebuildPlayer* packet = (CPlayerPackets::RebuildPlayer*)data;
				packet->playerid = player->m_iPlayerId;
				CNetwork::SendPacketToAll(CPacketsID::REBUILD_PLAYER, packet, sizeof * packet, ENET_PACKET_FLAG_RELIABLE, peer);

				memcpy(player->m_anModelKeys, packet->m_anModelKeys, sizeof(player->m_anModelKeys));
				memcpy(player->m_anTextureKeys, packet->m_anTextureKeys, sizeof(player->m_anTextureKeys));
				player->m_fFatStat = packet->m_fFatStat;
				player->m_fMuscleStat = packet->m_fMuscleStat;
				player->m_ucSyncFlags.bClothesModified = true;
			}
		}
	};

	struct RespawnPlayer
	{
		int playerid;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			CPlayerPackets::RespawnPlayer* packet = (CPlayerPackets::RespawnPlayer*)data;
			packet->playerid = CPlayerManager::GetPlayer(peer)->m_iPlayerId;
			if (packet->playerid >= 0 && packet->playerid < MAX_SERVER_PLAYERS)
			{
				ms_deathState[packet->playerid].active = false;
				ms_deathPickupSnapshots[packet->playerid].active = false;
			}
			CNetwork::SendPacketToAll(CPacketsID::RESPAWN_PLAYER, packet, sizeof * packet, ENET_PACKET_FLAG_RELIABLE, peer);
		}
	};

	struct StartCutscene
	{
		char name[8];
		uint8_t currArea; // AKA interior

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					CPlayerPackets::StartCutscene* packet = (CPlayerPackets::StartCutscene*)data;
					CNetwork::SendPacketToAll(CPacketsID::START_CUTSCENE, packet, sizeof * packet, ENET_PACKET_FLAG_RELIABLE, peer);
				}
			}
		}
	};

	struct OpCodeSync
	{
		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				/*if (player->m_bIsHost)
				{*/
					CNetwork::SendPacketToAll(CPacketsID::OPCODE_SYNC, data, size, ENET_PACKET_FLAG_RELIABLE, peer);
				//}
			}
		}
	};

	struct SkipCutscene
	{
		int playerid;
		int votes; // temporary unused

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				CNetwork::SendPacketToAll(CPacketsID::SKIP_CUTSCENE, data, size, ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct OnMissionFlagSync
	{
		uint8_t bOnMission : 1;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					CNetwork::SendPacketToAll(CPacketsID::ON_MISSION_FLAG_SYNC, data, sizeof(OnMissionFlagSync), ENET_PACKET_FLAG_RELIABLE, peer);
				}
			}
		}
	};

	struct UpdateEntityBlip
	{
		int playerid;
		eNetworkEntityType entityType;
		int entityId;
		bool isFriendly;
		uint8_t color;
		uint8_t display;
		uint8_t scale;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					UpdateEntityBlip* packet = (UpdateEntityBlip*)data;

					if (auto targetPlayer = CPlayerManager::GetPlayer(packet->playerid))
					{
						CNetwork::SendPacket(targetPlayer->m_pPeer, CPacketsID::UPDATE_ENTITY_BLIP, data, sizeof(UpdateEntityBlip), ENET_PACKET_FLAG_RELIABLE);
					}
				}
			}
		}
	};

	struct RemoveEntityBlip
	{
		int playerid;
		eNetworkEntityType entityType;
		int entityId;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					RemoveEntityBlip* packet = (RemoveEntityBlip*)data;

					if (auto targetPlayer = CPlayerManager::GetPlayer(packet->playerid))
					{
						CNetwork::SendPacket(targetPlayer->m_pPeer, CPacketsID::REMOVE_ENTITY_BLIP, data, sizeof(RemoveEntityBlip), ENET_PACKET_FLAG_RELIABLE);
					}
				}
			}
		}
	};

	struct AddMessageGXT
	{
		int playerid;
		// 0 - COMMAND_PRINT
		// 1 - COMMAND_PRINT_BIG
		// 2 - COMMAND_PRINT_NOW
		// 3 - COMMAND_PRINT_HELP
		uint8_t type;
		uint32_t time;
		uint8_t flag;
		char gxt[8];

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					AddMessageGXT* packet = (AddMessageGXT*)data;

					if (auto targetPlayer = CPlayerManager::GetPlayer(packet->playerid))
					{
						CNetwork::SendPacket(targetPlayer->m_pPeer, CPacketsID::ADD_MESSAGE_GXT, data, sizeof(AddMessageGXT), ENET_PACKET_FLAG_RELIABLE);
					}
				}
			}
		}
	};

	struct RemoveMessageGXT
	{
		int playerid;
		char gxt[8];

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					RemoveMessageGXT* packet = (RemoveMessageGXT*)data;

					if (auto targetPlayer = CPlayerManager::GetPlayer(packet->playerid))
					{
						CNetwork::SendPacket(targetPlayer->m_pPeer, CPacketsID::REMOVE_MESSAGE_GXT, data, sizeof(RemoveMessageGXT), ENET_PACKET_FLAG_RELIABLE);
					}
				}
			}
		}
	};

	struct ClearEntityBlips
	{
		int playerid;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					ClearEntityBlips* packet = (ClearEntityBlips*)data;

					if (auto targetPlayer = CPlayerManager::GetPlayer(packet->playerid))
					{
						CNetwork::SendPacket(targetPlayer->m_pPeer, CPacketsID::CLEAR_ENTITY_BLIPS, data, sizeof(ClearEntityBlips), ENET_PACKET_FLAG_RELIABLE);
					}
				}
			}
		}
	};

	struct PlayMissionAudio
	{
		uint8_t slotid;
		int audioid;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					PlayMissionAudio* packet = (PlayMissionAudio*)data;
					CNetwork::SendPacketToAll(CPacketsID::PLAY_MISSION_AUDIO, packet, sizeof(PlayMissionAudio), ENET_PACKET_FLAG_RELIABLE, peer);
				}
			}
		}
	};

	struct UpdateCheckpoint
	{
		int playerid;
		CVector position;
		CVector radius;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					UpdateCheckpoint* packet = (UpdateCheckpoint*)data;

					if (auto targetPlayer = CPlayerManager::GetPlayer(packet->playerid))
					{
						CNetwork::SendPacket(targetPlayer->m_pPeer, CPacketsID::UPDATE_CHECKPOINT, data, sizeof(UpdateCheckpoint), ENET_PACKET_FLAG_RELIABLE);
					}
				}
			}
		}
	};

	struct RemoveCheckpoint
	{
		int playerid;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					RemoveCheckpoint* packet = (RemoveCheckpoint*)data;

					if (auto targetPlayer = CPlayerManager::GetPlayer(packet->playerid))
					{
						CNetwork::SendPacket(targetPlayer->m_pPeer, CPacketsID::REMOVE_CHECKPOINT, data, sizeof(RemoveCheckpoint), ENET_PACKET_FLAG_RELIABLE);
					}
				}
			}
		}
	};

	struct EnExSync
	{
		static inline std::vector<uint8_t> ms_vLastData;
		static inline CPlayer* ms_pLastPlayerOwner;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					ms_vLastData.assign((uint8_t*)data, (uint8_t*)data + size);
					ms_pLastPlayerOwner = player;

					CNetwork::SendPacketToAll(CPacketsID::ENEX_SYNC, data, size, ENET_PACKET_FLAG_RELIABLE, peer);
				}
			}
		}
	};

	struct CreateStaticBlip
	{
		CVector position;
		int8_t sprite;
		uint8_t display : 2;
		uint8_t type : 1; // 0 - BLIP_CONTACT_POINT, 1 - BLIP_COORD
		uint8_t trackingBlip : 1;
		uint8_t shortRange : 1;
		uint8_t friendly : 1; // It is affected by BLIP_COLOUR_THREAT.   
		uint8_t coordBlipAppearance : 2; // see eBlipAppearance
		uint8_t size : 3;
		uint8_t color : 4;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					CNetwork::SendPacketToAll(CPacketsID::CREATE_STATIC_BLIP, data, sizeof(CreateStaticBlip), ENET_PACKET_FLAG_RELIABLE, peer);
				}
			}
		}
	};

	struct SetPlayerTask
	{
		int playerid;
		int taskType;
		CVector position;
		float rotation;
		bool toggle;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				SetPlayerTask* packet = (SetPlayerTask*)data;
				packet->playerid = player->m_iPlayerId;
				CNetwork::SendPacketToAll(CPacketsID::SET_PLAYER_TASK, packet, sizeof(SetPlayerTask), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct PedSay
	{
		int entityid : 31;
		int isPlayer : 1;
		int16_t phraseId;
		int startTimeDelay;
		uint8_t overrideSilence : 1;
		uint8_t isForceAudible : 1;
		uint8_t isFrontEnd : 1;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				PedSay* packet = (PedSay*)data;

				if (packet->isPlayer)
				{
					packet->entityid = player->m_iPlayerId;
				}

				CNetwork::SendPacketToAll(CPacketsID::PED_SAY, packet, sizeof(PedSay), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct AddProjectile
	{
		uint8_t creatorType;
		int creatorId;
		uint8_t projectileType; // eWeaponType
		CVector origin;
		float force;
		CVector dir;
		uint8_t targetType;
		int targetId;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				CNetwork::SendPacketToAll(CPacketsID::ADD_PROJECTILE, data, sizeof(AddProjectile), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct TagUpdate
	{
		int16_t pos_x;
		int16_t pos_y;
		int16_t pos_z;
		uint8_t alpha;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				CNetwork::SendPacketToAll(CPacketsID::TAG_UPDATE, data, sizeof(TagUpdate), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct UpdateAllTags
	{
		TagUpdate tags[150];

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					CNetwork::SendPacketToAll(CPacketsID::UPDATE_ALL_TAGS, data, sizeof(UpdateAllTags), ENET_PACKET_FLAG_RELIABLE, peer);
				}
			}
		}
	};

	struct TeleportPlayerScripted
	{
		int playerid;
		CVector pos;
		float heading;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				if (player->m_bIsHost)
				{
					TeleportPlayerScripted* packet = (TeleportPlayerScripted*)data;
					if (auto targetPlayer = CPlayerManager::GetPlayer(packet->playerid))
					{
						CNetwork::SendPacket(targetPlayer->m_pPeer, TELEPORT_PLAYER_SCRIPTED, packet, sizeof(*packet), ENET_PACKET_FLAG_RELIABLE);
					}
				}
			}
		}
	};

	struct WantedLevelSync
	{
		int playerid;
		uint8_t wantedLevel;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				WantedLevelSync* packet = (WantedLevelSync*)data;
				packet->playerid = player->m_iPlayerId;
				player->m_progress.wantedLevel = packet->wantedLevel;
				player->m_ucSyncFlags.bProgressModified = true;
				CNetwork::SendPacketToAll(CPacketsID::WANTED_LEVEL_SYNC, packet, sizeof(*packet), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct MoneySync
	{
		int playerid;
		int32_t money;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				MoneySync* packet = (MoneySync*)data;
				packet->playerid = player->m_iPlayerId;
				player->m_progress.money = packet->money;
				player->m_ucSyncFlags.bProgressModified = true;
				CNetwork::SendPacketToAll(CPacketsID::MONEY_SYNC, packet, sizeof(*packet), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct CheatCodeSync
	{
		int playerid;
		uint16_t cheatId;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				CheatCodeSync* packet = (CheatCodeSync*)data;
				packet->playerid = player->m_iPlayerId;
				CNetwork::SendPacketToAll(CPacketsID::CHEAT_CODE_SYNC, packet, sizeof(*packet), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct FireSync
	{
		int playerid;
		CVector position;
		uint32_t timeToBurn;
		int8_t numGenerations;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				FireSync* packet = (FireSync*)data;
				packet->playerid = player->m_iPlayerId;
				CNetwork::SendPacketToAll(CPacketsID::FIRE_SYNC, packet, sizeof(*packet), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};
	struct PickupRemove
	{
		int16_t pos_x;
		int16_t pos_y;
		int16_t pos_z;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				PickupRemove* packet = (PickupRemove*)data;

				for (auto& drop : ms_itemDropSnapshots)
				{
					if (!drop.active)
						continue;

					if (drop.cx == packet->pos_x && drop.cy == packet->pos_y && drop.cz == packet->pos_z)
					{
						drop.active = false;
						break;
					}
				}

				CNetwork::SendPacketToAll(CPacketsID::PICKUP_REMOVE, data, sizeof(PickupRemove), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct DeathPickups
	{
		int playerid;
		float x, y, z;
		int money;
		unsigned char weaponCount;
		struct WeaponEntry
		{
			unsigned int weaponType;
			unsigned int ammo;
		} weapons[13];

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				DeathPickups* packet = (DeathPickups*)data;
				packet->playerid = player->m_iPlayerId;

				if (player->m_iPlayerId >= 0 && player->m_iPlayerId < MAX_SERVER_PLAYERS)
				{
					auto& state = ms_deathState[player->m_iPlayerId];
					state.active = true;
					state.pos = CVector(packet->x, packet->y, packet->z);
					state.deathTick = enet_time_get();

					auto& snapshot = ms_deathPickupSnapshots[player->m_iPlayerId];
					snapshot.active = true;
					snapshot.tick = state.deathTick;
					snapshot.playerid = packet->playerid;
					snapshot.x = packet->x;
					snapshot.y = packet->y;
					snapshot.z = packet->z;
					snapshot.money = packet->money;
					snapshot.weaponCount = packet->weaponCount;
					for (int i = 0; i < 13; i++)
					{
						snapshot.weapons[i].weaponType = packet->weapons[i].weaponType;
						snapshot.weapons[i].ammo = packet->weapons[i].ammo;
					}
				}

				CNetwork::SendPacketToAll(CPacketsID::DEATH_PICKUPS, packet, sizeof(DeathPickups), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct ItemDrop
	{
		int playerid;
		float x, y, z;
		unsigned char dropType;
		unsigned int weaponType;
		unsigned int ammo;
		int money;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			if (auto player = CPlayerManager::GetPlayer(peer))
			{
				ItemDrop* packet = (ItemDrop*)data;
				packet->playerid = player->m_iPlayerId;

				int16_t cx = (int16_t)(packet->x * 8.0f);
				int16_t cy = (int16_t)(packet->y * 8.0f);
				int16_t cz = (int16_t)(packet->z * 8.0f);

				ItemDropSnapshot* slot = nullptr;
				for (auto& drop : ms_itemDropSnapshots)
				{
					if (!drop.active)
					{
						slot = &drop;
						break;
					}
				}
				if (!slot)
				{
					slot = &ms_itemDropSnapshots[0];
					for (auto& drop : ms_itemDropSnapshots)
					{
						if (drop.tick < slot->tick)
						{
							slot = &drop;
						}
					}
				}

				slot->active = true;
				slot->tick = enet_time_get();
				slot->playerid = packet->playerid;
				slot->x = packet->x;
				slot->y = packet->y;
				slot->z = packet->z;
				slot->dropType = packet->dropType;
				slot->weaponType = packet->weaponType;
				slot->ammo = packet->ammo;
				slot->money = packet->money;
				slot->cx = cx;
				slot->cy = cy;
				slot->cz = cz;

				CNetwork::SendPacketToAll(CPacketsID::ITEM_DROP, packet, sizeof(ItemDrop), ENET_PACKET_FLAG_RELIABLE, peer);
			}
		}
	};

	struct ReviveRequest
	{
		int targetPlayerId;
		CVector rescuerPos;

		static void Handle(ENetPeer* peer, void* data, int size)
		{
			auto* rescuer = CPlayerManager::GetPlayer(peer);
			if (!rescuer)
			{
				return;
			}

			auto* packet = (ReviveRequest*)data;
			if (packet->targetPlayerId < 0 || packet->targetPlayerId >= MAX_SERVER_PLAYERS)
			{
				return;
			}

			if (packet->targetPlayerId == rescuer->m_iPlayerId)
			{
				return;
			}

			auto* target = CPlayerManager::GetPlayer(packet->targetPlayerId);
			if (!target)
			{
				return;
			}

			auto& state = ms_deathState[packet->targetPlayerId];
			if (!state.active)
			{
				return;
			}

			static constexpr uint32_t kReviveWindowMs = 60000;
			static constexpr float kReviveDistance = 1.0f;

			uint32_t now = enet_time_get();
			if ((now - state.deathTick) > kReviveWindowMs)
			{
				state.active = false;
				return;
			}

			CVector rescuerPos = packet->rescuerPos;
			rescuerPos.x = std::clamp(rescuerPos.x, -3000.0f, 3000.0f);
			rescuerPos.y = std::clamp(rescuerPos.y, -3000.0f, 3000.0f);
			rescuerPos.z = std::clamp(rescuerPos.z, -300.0f, 1200.0f);

			float dx = rescuerPos.x - state.pos.x;
			float dy = rescuerPos.y - state.pos.y;
			float dz = rescuerPos.z - state.pos.z;
			float distSq = dx * dx + dy * dy + dz * dz;
			if (distSq > (kReviveDistance * kReviveDistance))
			{
				return;
			}

			struct ReviveApply
			{
				int targetPlayerId;
				int rescuerPlayerId;
				CVector revivePos;
				uint8_t success;
			};

			ReviveApply out{};
			out.targetPlayerId = packet->targetPlayerId;
			out.rescuerPlayerId = rescuer->m_iPlayerId;
			out.revivePos = state.pos;
			out.success = 1;

			state.active = false;
			ms_deathPickupSnapshots[packet->targetPlayerId].active = false;

			CNetwork::SendPacketToAll(CPacketsID::REVIVE_APPLY, &out, sizeof(out), ENET_PACKET_FLAG_RELIABLE);
		}
	};
};
#endif
