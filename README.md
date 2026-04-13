# CoopAndreas

## Quick Install (Windows, easiest)

If you just want to **play** (not build from source), use the ready updater from branch `old-0.2.2`.

1. Open: [old-0.2.2 / release / launcher](https://github.com/brianwVA/CoopAndreas/tree/old-0.2.2/release/launcher)
2. Download all files from that folder into your GTA San Andreas root directory (where `gta_sa.exe` is).
3. Run `Aktualizuj CoopAndreas.bat`.
4. After update, run `Uruchom CoopAndreas.bat`.

What this installer does automatically:
- detects local version vs latest GitHub version and updates only when needed,
- installs required files (`CoopAndreasSA.dll`, `server.exe`, `proxy.dll`, `VERSION.txt`),
- installs ASI loader (`dinput8.dll`) if missing,
- installs resolution fix (`scripts/GTASA.WidescreenFix.asi`) if missing,
- detects SA:MP and offers a safe switch mode to avoid conflicts.

Fresh reinstall / cleanup:
- run `Wyczysc CoopAndreas.bat` from the same launcher package to remove CoopAndreas files and install again from scratch.

## Disclaimer
This mod is an unofficial modification for **Grand Theft Auto: San Andreas** and requires a legitimate copy of the game to function. No original game files or assets from Rockstar Games are included in this repository, and all content provided is independently developed. The project is not affiliated with Rockstar Games or Take-Two Interactive. All rights to the original game, its assets, and intellectual property belong to Rockstar Games and Take-Two Interactive. This mod is created solely for educational and non-commercial purposes. Users must comply with the terms of service and license agreements of Rockstar Games.


## Building (Windows)

### Client & Server

1. Make sure you have the **C++ package** installed in **Visual Studio 2022** and **[xmake](https://xmake.io/)** (needed to build the project).

2. Download **[this version of plugin-sdk](https://github.com/DK22Pac/plugin-sdk/tree/050d18b6e1770477deab81a40028a40277583d97)** and install it using **[this instruction](https://github.com/DK22Pac/plugin-sdk/wiki/Set-up-plugin-sdk)**.  
   Set up your GTA-SA and plugin-sdk folders, and make sure `GTA_SA_DIR` and `PLUGIN_SDK_DIR` environment variables are set.

3. Open a terminal in the repository root and build the projects:

```bash
# Build client DLL
xmake --build client

# Build server binary
xmake --build server
```
If `GTA_SA_DIR` is set, the client DLL (`CoopAndreasSA.dll`) will automatically be placed in your game directory.

---

### Proxy (DLL Loader)

The proxy is required to load (inject) the main DLL (`CoopAndreasSA.dll`) into the game executable.

1. Go to your game directory and rename `eax.dll` to `eax_orig.dll`.

2. Build the **Proxy** project:

```bash
xmake --build proxy
```

3. Rename the output DLL to `eax.dll` and copy it to your game folder.

---

### `main.scm`

1. Download and install **[Sanny Builder 4](https://github.com/sannybuilder/dev/releases)**.

2. Copy all files from the `sdk/Sanny Builder 4/` directory to the **Sanny Builder 4 installation folder**.  
   This will add **CoopAndreas opcodes** to the compiler.

3. Open `scm/main.txt` in **Sanny Builder 4**, compile it, and copy all generated files to  
   `${GTA_SA_DIR}/CoopAndreas/`.

---

### Running

1. Run `server.exe` it will start accepting client connections.
2. Run `gta_sa.exe`, press **Start Game**, enter your nickname, then provide the server IP and port.
   - If the server is running on the same machine, use `127.0.0.1`
   - Default port: `6767`


## Building (GNU/Linux)

### Server

TODO

## TODO And Verification Matrix (Source Of Truth)

Audit date: `2026-04-08`
Status model:
- `CODE_OK`: implemented and verified in code.
- `RUNTIME_OK`: validated in a 2-player GTA runtime flow (or explicit Sanny compile check where noted).
- `OPEN`: missing implementation, regression, or runtime validation still pending.

### Core Matrix

| Subsystem | CODE_OK | RUNTIME_OK | Status | Evidence |
|---|---|---|---|---|
| Drop weapon (`G`) | Yes | Yes | `RUNTIME_OK` | `client/src/Main.cpp` (G key drop packet), `client/src/CPacketHandler.cpp` (`ItemDrop__Handle`, pickup tracking/removal); runtime verified in 2-session test |
| Drop money (`H`) | Yes | Yes | `RUNTIME_OK` | `client/src/Main.cpp` (H key, money decrement + packet), `client/src/CPacketHandler.cpp` (`dropType == 1`); runtime verified in 2-session test |
| Per-player stats/skills | Yes | Yes | `RUNTIME_OK` | `shared/player_progress.h`, `client/src/CStatsSync.cpp`, `server/src/CPlayerManager.h` (`PlayerStats`); runtime verified (skills/stat changes isolated per player) |
| Per-player property ownership | Yes | Yes | `RUNTIME_OK` | `client/src/CStatsSync.cpp` + custom opcodes `0x1D1D/0x1D1E`; runtime verified (independent house purchase state) |
| Per-player school progress/medals | Yes | Partial | `OPEN` | Code path present (`0x1D1F..0x1D22`, `CStatsSync` arrays), but full runtime school medal/progress suite still pending |
| Vehicle enter/occupancy (general) | Yes | Partial | `OPEN` | `VehicleEnter`, `VehicleConfirm`, `VehicleOccupants` and `WorldHooks` audited and patched; ambient/NPC vehicle visibility still requires clean pass in runtime retest |
| Late-join vehicle seat restore | Yes | Partial | `OPEN` | server snapshot send in `server/src/CNetwork.cpp` + apply in `client/src/CPacketHandler.cpp::VehicleOccupants__Handle`; needs fresh runtime confirmation after latest fixes |
| Version single source and auto-bump | Yes | Yes | `RUNTIME_OK` | `version.json`, `tools/sync_version.py`, CI workflows; observed auto bump sequence on `main` |
| Sanny SCM compile path | Yes | Yes* | `RUNTIME_OK` | `tools/build-scm.ps1` executed with `C:\Tools\SannyBuilder4\sanny.exe`; `scm/main.compiled.scm` regenerated (`*compiler/tooling check, not gameplay runtime`) |

### Focused Re-Audit (Claim Vs Reality)

| Area | Status | Notes |
|---|---|---|
| Item drops (`Main.cpp`, `CPacketHandler.cpp`, `CPackets.h`) | `RUNTIME_OK` | Weapon and money drop paths exist and were runtime tested in 2 sessions |
| Progress sync (`player_progress.h`, `CStatsSync`, client/server `PlayerStats`) | `RUNTIME_OK` (stats/properties), `OPEN` (schools full suite) | Data model and packet flow are widened and active |
| Vehicle flow (`VehicleEnter`, `VehicleConfirm`, `VehicleOccupants`, `WorldHooks`) | `OPEN` | Code fixes are in; ambient/NPC vehicle disappearance needs a final green runtime pass |
| Property/school custom opcodes and caches | `CODE_OK` / `RUNTIME_OK` partial | Property runtime confirmed; school runtime coverage not fully closed |

### Current Priority TODO (Maintained)

- [ ] Close runtime validation for ambient/NPC vehicle visibility after enter (driver and passenger both directions).
- [ ] Close runtime validation for late join into already occupied ambient vehicle.
- [ ] Execute full school progression and medal runtime matrix (driving/bike/boat/flight school independent state).
- [ ] Re-verify host migration edge cases for money/wanted/stats snapshots.
- [ ] Confirm launcher-side server control remains in backlog (`OPEN`).

Runtime session checklist used to close these items:
- `RUNTIME_CHECKLIST_AB_2026-04-08.md`

### Notes

- Legacy checklist blocks were intentionally replaced because they mixed historical claims with current status and were no longer reliable as a source of truth.
- Mission conversion details remain available in `COOP_PROGRESS.md` and `COOP_AUDIT_REPORT.md`.
