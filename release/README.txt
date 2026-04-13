CoopAndreas - Auto Updater (release)

Zawartosc:
- launcher/CoopAndreasUpdater.ps1
- launcher/Aktualizuj CoopAndreas.bat
- launcher/Aktualizuj i Uruchom CoopAndreas.bat
- launcher/CoopAndreasCleaner.ps1
- launcher/Odinstaluj CoopAndreas.bat
- old-X.Y.Z/* (paczki binarne)

Szybki start (dla gracza):
1. Skopiuj pliki z `release/launcher` do folderu GTA San Andreas.
2. Uruchom `Aktualizuj CoopAndreas.bat`.
3. Gotowe.

Instalacja na czystym GTA:
- updater sam dociagnie brakujacy ASI Loader (`dinput8.dll`) z oficjalnego GitHuba,
- updater sam dociagnie fix rozdzielczosci (`GTASA.WidescreenFix.asi`) i wrzuci do `scripts`.
- jesli chcesz pominac fix rozdzielczosci, uruchom:
  `CoopAndreasUpdater.ps1 -NoResolutionFix`

Co robi updater:
- pobiera branch `old-0.2.2` z GitHuba,
- wybiera najnowsza paczke z `release/old-*`,
- podmienia: `CoopAndreasSA.dll`, `server.exe`, `proxy.dll`, `VERSION.txt`,
- robi backup do `CoopAndreas_backup_YYYYMMDD_HHMMSS`,
- przed podmiana zamyka `server.exe` i `gta_sa.exe`.
- porownuje wersje lokalna i z GitHub i nie pobiera paczki, jesli wersja jest juz najnowsza.

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
- tworzy launchery:
  - `Uruchom CoopAndreas.bat`
  - `Uruchom CoopAndreas Server.bat`
  - `Odinstaluj CoopAndreas.bat`

Pelna odinstalacja (test "na swiezo"):
- uruchom `Odinstaluj CoopAndreas.bat`,
- skrypt usunie pliki CoopAndreas, fix rozdzielczosci i pliki launcherow,
- przywroci pliki SA:MP jesli byly tymczasowo wylaczone.
