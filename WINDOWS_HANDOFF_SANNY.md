# Windows Handoff For Codex

## Current Baseline (2026-04-08)
- Repo path: `C:\CoopAndreas`
- Branch: `main`
- Version source of truth: `version.json`
- Current synced version: `0.2.12`
- Status model used in docs: `CODE_OK` / `RUNTIME_OK` / `OPEN`

## What Was Re-Checked In This Pass

### Build and Tooling
- `xmake --build client` -> pass
- `xmake --build server` -> pass
- `python tools/sync_version.py --check` -> pass
- `tools/build-scm.ps1` with `C:\Tools\SannyBuilder4\sanny.exe` -> produced `scm/main.compiled.scm`

### Code Audit Coverage
- Item drops (`G` / `H`):
  - `client/src/Main.cpp`
  - `client/src/CPacketHandler.cpp`
  - `client/src/CPackets.h`
- Per-player progress model:
  - `shared/player_progress.h`
  - `client/src/CStatsSync.cpp`
  - `server/src/CPlayerManager.h`
- Vehicle enter/occupancy flow:
  - `client/src/CPacketHandler.cpp` (`VehicleEnter`, `VehicleConfirm`, `VehicleOccupants`)
  - `client/src/Hooks/WorldHooks.cpp`
  - `server/src/CNetwork.cpp` (late-join occupant snapshot)
- Property/school opcodes:
  - `client/src/Commands/CCustomCommandRegistrar.h` (`0x1D1D`..`0x1D22`)
  - `client/src/Commands/Commands/CCommandSet/GetPlayerPropertyState*`
  - `client/src/Commands/Commands/CCommandSet/GetPlayerSchool*`

## Runtime Status Snapshot

| Feature | CODE_OK | RUNTIME_OK | Status |
|---|---|---|---|
| Weapon drop (`G`) | Yes | Yes | `RUNTIME_OK` |
| Money drop (`H`) | Yes | Yes | `RUNTIME_OK` |
| Independent skills/stats | Yes | Yes | `RUNTIME_OK` |
| Independent property ownership | Yes | Yes | `RUNTIME_OK` |
| School progress/medals independent | Yes | Partial | `OPEN` |
| Vehicle visibility in ambient/NPC enter cases | Yes | Partial | `OPEN` |
| Late-join occupant restoration | Yes | Partial | `OPEN` |

## Immediate Next Validation Targets
1. Re-test ambient/NPC vehicle enter visibility in both directions (A sees B, B sees A), including passenger case.
2. Re-test late join while another player is already inside an ambient vehicle.
3. Run full school matrix (driving/boat/bike/flight progress and medals) to promote school state from partial to `RUNTIME_OK`.
4. If any test fails: capture chat logs from both clients around the exact event and patch before moving to next item.

## Rule For Updating Status
- Never mark `RUNTIME_OK` without explicit 2-session runtime evidence.
- If code exists but runtime is not fully proven, keep `OPEN` with a short blocker note.
