CoopAndreas - Auto Updater (release)

Zawartosc:
- launcher/CoopAndreasUpdater.ps1
- launcher/Aktualizuj CoopAndreas.bat
- launcher/Aktualizuj i Uruchom CoopAndreas.bat
- old-X.Y.Z/* (paczki binarne)

Szybki start (dla gracza):
1. Skopiuj pliki z `release/launcher` do folderu GTA San Andreas.
2. Uruchom `Aktualizuj CoopAndreas.bat`.
3. Gotowe.

Co robi updater:
- pobiera branch `old-0.2.2` z GitHuba,
- wybiera najnowsza paczke z `release/old-*`,
- podmienia: `CoopAndreasSA.dll`, `server.exe`, `proxy.dll`, `VERSION.txt`,
- robi backup do `CoopAndreas_backup_YYYYMMDD_HHMMSS`,
- przed podmiana zamyka `server.exe` i `gta_sa.exe`.

Walidacja i bezpieczenstwo:
- wykrywa folder gry po `gta_sa.exe` (moze byc uruchomiony takze z podkatalogu),
- sprawdza SA:MP i pokazuje wybor:
  1) anuluj,
  2) wylacz SA:MP i kontynuuj,
  3) kontynuuj mimo ryzyka.
- zapisuje instrukcje do `CoopAndreas_SAMP_INFO.txt`,
- tworzy przelaczniki:
  - `Przelacz na CoopAndreas.bat`
  - `Przelacz na SA-MP.bat`

Wymaganie:
- potrzebny `dinput8.dll` (ASI Loader). Jesli jest w paczce - skrypt go skopiuje.
