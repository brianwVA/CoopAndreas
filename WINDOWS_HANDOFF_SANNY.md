# Windows Handoff For Codex

## Goal
- Continue the CoopAndreas work on a Windows machine with Sanny Builder available.
- Validate the current gameplay/networking changes in a real toolchain and in-game environment.
- Finish the next stage: per-player properties and school progression with proper SCM/world-state projection.

## What Is Already Implemented
- Unified project versioning around `0.2.7`.
- Single source of truth for version in [`version.json`](/Users/bewu/Documents/CoopAndreas/version.json).
- Version sync script in [`tools/sync_version.py`](/Users/bewu/Documents/CoopAndreas/tools/sync_version.py).
- Shared version header in [`shared/version.h`](/Users/bewu/Documents/CoopAndreas/shared/version.h).
- GitHub Actions version validation in [`/.github/workflows/ci.yml`](/Users/bewu/Documents/CoopAndreas/.github/workflows/ci.yml).
- Auto patch bump on push to `main` in [`/.github/workflows/version-bump.yml`](/Users/bewu/Documents/CoopAndreas/.github/workflows/version-bump.yml).
- `H` key money drop on client in [`client/src/Main.cpp`](/Users/bewu/Documents/CoopAndreas/client/src/Main.cpp).
- Money drop pickup sync/removal in [`client/src/CPacketHandler.cpp`](/Users/bewu/Documents/CoopAndreas/client/src/CPacketHandler.cpp).
- Fixed remote money/wanted packets so they do not overwrite the local player blindly in [`client/src/CPacketHandler.cpp`](/Users/bewu/Documents/CoopAndreas/client/src/CPacketHandler.cpp).
- Added late-join vehicle occupancy snapshot packet flow:
  - [`server/src/CNetwork.cpp`](/Users/bewu/Documents/CoopAndreas/server/src/CNetwork.cpp)
  - [`server/src/CVehicleManager.h`](/Users/bewu/Documents/CoopAndreas/server/src/CVehicleManager.h)
  - [`client/src/CPackets.h`](/Users/bewu/Documents/CoopAndreas/client/src/CPackets.h)
  - [`client/src/CPacketHandler.cpp`](/Users/bewu/Documents/CoopAndreas/client/src/CPacketHandler.cpp)
- Added a broader per-player progress data model in [`shared/player_progress.h`](/Users/bewu/Documents/CoopAndreas/shared/player_progress.h).
- Expanded stats sync packet from a 14-float subset to a fuller progress snapshot.

## What Was Verified Locally
- `python3 -m py_compile tools/sync_version.py`
- `python3 tools/sync_version.py --check`
- Syntax-only checks for:
  - [`shared/version.h`](/Users/bewu/Documents/CoopAndreas/shared/version.h)
  - [`shared/player_progress.h`](/Users/bewu/Documents/CoopAndreas/shared/player_progress.h)

## What Was Not Verified Yet
- Full Windows build with `xmake`.
- Runtime validation in GTA SA.
- SCM compilation/behavior through Sanny Builder.
- End-to-end behavior of vehicle late join occupancy in live gameplay.
- End-to-end behavior of `H` money drop in multiplayer gameplay.
- Per-player properties and school progression are not finished yet.

## Highest-Risk Areas To Check First On Windows
- `MoneySync` and `WantedLevelSync` behavior after join/host migration.
- `PlayerStats` packet serialization/deserialization on both client and server after widening the payload.
- `VehicleOccupants` packet layout and late-join seat restoration.
- `H` money drop pickup creation/removal in multiplayer.
- Any compile break caused by plugin SDK / Windows headers / packing assumptions.

## Recommended Validation Order
1. Run the normal Windows build for client and server.
2. Confirm version strings/resources show `0.2.7`.
3. Compile any affected SCM pieces with Sanny Builder if the next work touches SCM.
4. Launch a 2-player test session and verify:
   - money drop under `H`
   - passenger/driver vehicle sync
   - late join into an occupied vehicle
   - money/stat packets do not overwrite the wrong player
5. Only after that, start the next implementation stage for properties and schools.

## Next Implementation Target
- Finish per-player properties and per-player schools.
- This likely requires a real projection layer between per-player network state and the shared GTA SCM/global script state.
- Do not assume packet/data-model work alone is enough; verify actual pickup/blip/unlock behavior.

## Suggested Prompt For Codex On Windows
```text
Work in /Users/bewu/Documents/CoopAndreas.

Start by reviewing the latest local changes related to:
- version.json / tools/sync_version.py / shared/version.h
- H money drop
- PlayerProgressState and widened PlayerStats packet
- VehicleOccupants late-join sync

Then do this in order:
1. Build the project on Windows with the intended toolchain.
2. Verify the current changes actually compile and run.
3. Use Sanny Builder to validate any SCM-related behavior before making deeper progression changes.
4. Test in a 2-player scenario:
   - H money drop and pickup removal
   - entering/exiting vehicles
   - late join while someone is inside a vehicle
   - local money and wanted level are not overwritten by remote packets
5. Fix any compile/runtime/network regressions you find.
6. After the current foundation is confirmed stable, implement per-player properties and school progression properly.

Important constraints:
- Do not revert unrelated local changes.
- Treat shared missions/world state as host-authoritative.
- Treat money, stats, owned properties, school progress, and school medals as per-player state.
- Verify behavior with real Windows/Sanny tooling, not just static inspection.
```
