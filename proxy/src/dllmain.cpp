#include "pch.h"
#include <windows.h>

#pragma comment(linker,"/export:DllCanUnloadNow=eax_orig.DllCanUnloadNow,@1")
#pragma comment(linker,"/export:DllGetClassObject=eax_orig.DllGetClassObject,@2")
#pragma comment(linker,"/export:DllRegisterServer=eax_orig.DllRegisterServer,@3")
#pragma comment(linker,"/export:DllUnregisterServer=eax_orig.DllUnregisterServer,@4")
#pragma comment(linker,"/export:EAXDirectSoundCreate=eax_orig.EAXDirectSoundCreate,@5")
#pragma comment(linker,"/export:EAXDirectSoundCreate8=eax_orig.EAXDirectSoundCreate8,@6")
#pragma comment(linker,"/export:GetCurrentVersion=eax_orig.GetCurrentVersion,@7")

HMODULE hCoopAndreas = NULL;

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        hCoopAndreas = LoadLibraryA("CoopAndreasSA.dll");
        if (!hCoopAndreas)
        {
            DWORD err = GetLastError();
            char msg[512];
            wsprintfA(msg,
                "Failed to load CoopAndreasSA.dll (error %lu).\n\n"
                "Mozliwe przyczyny:\n"
                "1) Brak DirectX 9 Runtime (d3dx9_43.dll)\n"
                "   Pobierz: https://www.microsoft.com/en-us/download/details.aspx?id=8109\n"
                "2) Brak Visual C++ Redistributable x86\n"
                "   Pobierz: https://aka.ms/vs/17/release/vc_redist.x86.exe\n"
                "3) Windows zablokował plik (PPM na DLL -> Wlasciwosci -> Odblokuj)\n\n"
                "Lub uruchom: Uruchom CoopAndreas.bat\n"
                "(updater zainstaluje brakujace automatycznie)",
                err);
            MessageBoxA(0, msg, "CoopAndreas Loader", MB_OK | MB_ICONERROR);
        }
        break;
    case DLL_PROCESS_DETACH:
        if (hCoopAndreas)
        {
            FreeLibrary(hCoopAndreas);
        }
        break;
    }
    return TRUE;
}
