# Runtime Checklist (A/B) - 2026-04-08

Version target:
- client DLL: `0.2.13`
- server EXE: `0.2.13`

Test environment:
- Player A: `C:\GTA San Andreas`
- Player B: `C:\Users\brian\Downloads\bwcoop\GTA San Andreas`
- Server: `C:\CoopAndreas\build\windows\x86\release\server.exe`

How to mark:
- `PASS`: behavior matches expected.
- `FAIL`: behavior does not match expected.
- `BLOCKED`: cannot test due to setup issue.

---

## 1) Ambient/NPC vehicle visibility (OPEN -> target RUNTIME_OK)

### 1.1 Driver enter (A enters, B observes)
- Steps:
  1. A and B join same session.
  2. A enters an ambient/NPC street vehicle (not spawned by code/mission command).
  3. B observes A.
- Expected:
  - A remains visible to B.
  - Vehicle remains visible to B.
- Result: `PENDING`
- Notes:

### 1.2 Driver enter (B enters, A observes)
- Steps:
  1. B enters another ambient/NPC street vehicle.
  2. A observes B.
- Expected:
  - B remains visible to A.
  - Vehicle remains visible to A.
- Result: `PENDING`
- Notes:

### 1.3 Passenger enter
- Steps:
  1. One player drives ambient vehicle.
  2. Second player enters as passenger.
- Expected:
  - Both players see same vehicle occupancy.
  - No disappear/desync on either client.
- Result: `PENDING`
- Notes:

---

## 2) Late join occupied vehicle (OPEN -> target RUNTIME_OK)

### 2.1 Rejoin visibility snapshot
- Steps:
  1. A sits as driver in ambient/NPC vehicle.
  2. B disconnects/reconnects (or joins after A already sits in vehicle).
  3. B observes A vehicle seat state.
- Expected:
  - B sees A in correct seat.
  - Vehicle/occupant does not pop out/despawn.
- Result: `PENDING`
- Notes:

---

## 3) School progress and medals independence (OPEN -> target RUNTIME_OK)

### 3.1 School progress isolation
- Steps:
  1. B completes or advances one school exercise.
  2. A does not run that exercise.
  3. Compare school progress values/state for A vs B.
- Expected:
  - B progress changes.
  - A progress remains unchanged.
- Result: `PENDING`
- Notes:

### 3.2 Medal isolation
- Steps:
  1. B earns/changes medal result in school.
  2. A does not perform that school run.
  3. Compare medal state for A vs B.
- Expected:
  - B medal changes.
  - A medal remains unchanged.
- Result: `PENDING`
- Notes:

---

## 4) Host migration money/wanted/stats snapshot (OPEN -> target RUNTIME_OK)

### 4.1 Host handover integrity
- Steps:
  1. A and B have different money/wanted/stats.
  2. Current host leaves so host migrates.
  3. Verify both players after migration.
- Expected:
  - Money/wanted/stats remain per-player.
  - No overwrite from remote player state.
- Result: `PENDING`
- Notes:

---

## Quick report format (send after run)

Use this compact format in chat:

`1.1 PASS`
`1.2 PASS`
`1.3 FAIL - <short reason>`
`2.1 PASS`
`3.1 PASS`
`3.2 FAIL - <short reason>`
`4.1 PASS`

If any FAIL appears, include:
- which side observed issue (A or B),
- exact moment (enter/rejoin/host change),
- 5-15 lines of in-game network/chat log around the event.
