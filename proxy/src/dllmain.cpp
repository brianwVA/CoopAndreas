#include "pch.h"
#include <windows.h>

// ── Runtime forwarding to eax_orig.dll (graceful if missing) ──
// When eax_orig.dll is absent, EAXDirectSoundCreate falls back to
// regular DirectSoundCreate from dsound.dll so the game gets a valid
// IDirectSound* and doesn't crash.  EAX effects simply won't work.

static HMODULE hEaxOrig = NULL;
static HMODULE hDSound  = NULL;
static HMODULE hCoopAndreas = NULL;

typedef HRESULT (__stdcall *fn_NoArgs)();
typedef HRESULT (__stdcall *fn_GetClassObj)(const void*, const void*, void**);
typedef HRESULT (__stdcall *fn_DSCreate)(const void*, void**, void*);
typedef HRESULT (__stdcall *fn_GetVer)(void*);

static fn_NoArgs      p_DllCanUnloadNow;
static fn_GetClassObj p_DllGetClassObject;
static fn_NoArgs      p_DllRegisterServer;
static fn_NoArgs      p_DllUnregisterServer;
static fn_DSCreate    p_EAXDirectSoundCreate;
static fn_DSCreate    p_EAXDirectSoundCreate8;
static fn_GetVer      p_GetCurrentVersion;

// dsound.dll fallbacks
static fn_DSCreate    p_DirectSoundCreate;
static fn_DSCreate    p_DirectSoundCreate8;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // Load original EAX — not fatal if missing (falls back to DirectSound)
        hEaxOrig = LoadLibraryA("eax_orig.dll");
        if (hEaxOrig)
        {
            p_DllCanUnloadNow       = (fn_NoArgs)     GetProcAddress(hEaxOrig, "DllCanUnloadNow");
            p_DllGetClassObject     = (fn_GetClassObj) GetProcAddress(hEaxOrig, "DllGetClassObject");
            p_DllRegisterServer     = (fn_NoArgs)      GetProcAddress(hEaxOrig, "DllRegisterServer");
            p_DllUnregisterServer   = (fn_NoArgs)      GetProcAddress(hEaxOrig, "DllUnregisterServer");
            p_EAXDirectSoundCreate  = (fn_DSCreate)    GetProcAddress(hEaxOrig, "EAXDirectSoundCreate");
            p_EAXDirectSoundCreate8 = (fn_DSCreate)    GetProcAddress(hEaxOrig, "EAXDirectSoundCreate8");
            p_GetCurrentVersion     = (fn_GetVer)      GetProcAddress(hEaxOrig, "GetCurrentVersion");
        }
        else
        {
            // Fallback: resolve DirectSoundCreate from dsound.dll
            hDSound = LoadLibraryA("dsound.dll");
            if (hDSound)
            {
                p_DirectSoundCreate  = (fn_DSCreate) GetProcAddress(hDSound, "DirectSoundCreate");
                p_DirectSoundCreate8 = (fn_DSCreate) GetProcAddress(hDSound, "DirectSoundCreate8");
            }
        }

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
        if (hCoopAndreas) FreeLibrary(hCoopAndreas);
        if (hEaxOrig) FreeLibrary(hEaxOrig);
        if (hDSound) FreeLibrary(hDSound);
        break;
    }
    return TRUE;
}

// ── Forwarded EAX exports (return failure stubs if eax_orig.dll missing) ──

extern "C" HRESULT __stdcall proxy_DllCanUnloadNow()
{ return p_DllCanUnloadNow ? p_DllCanUnloadNow() : S_FALSE; }

extern "C" HRESULT __stdcall proxy_DllGetClassObject(const void* rclsid, const void* riid, void** ppv)
{
    if (p_DllGetClassObject) return p_DllGetClassObject(rclsid, riid, ppv);
    if (ppv) *ppv = NULL;
    return (HRESULT)0x80040111L; // CLASS_E_CLASSNOTAVAILABLE
}

extern "C" HRESULT __stdcall proxy_DllRegisterServer()
{ return p_DllRegisterServer ? p_DllRegisterServer() : E_FAIL; }

extern "C" HRESULT __stdcall proxy_DllUnregisterServer()
{ return p_DllUnregisterServer ? p_DllUnregisterServer() : E_FAIL; }

extern "C" HRESULT __stdcall proxy_EAXDirectSoundCreate(const void* g, void** pp, void* u)
{
    if (p_EAXDirectSoundCreate) return p_EAXDirectSoundCreate(g, pp, u);
    // Fallback to regular DirectSoundCreate — game works, just no EAX effects
    if (p_DirectSoundCreate) return p_DirectSoundCreate(g, pp, u);
    if (pp) *pp = NULL;
    return E_FAIL;
}

extern "C" HRESULT __stdcall proxy_EAXDirectSoundCreate8(const void* g, void** pp, void* u)
{
    if (p_EAXDirectSoundCreate8) return p_EAXDirectSoundCreate8(g, pp, u);
    if (p_DirectSoundCreate8) return p_DirectSoundCreate8(g, pp, u);
    if (pp) *pp = NULL;
    return E_FAIL;
}

extern "C" HRESULT __stdcall proxy_GetCurrentVersion(void* pv)
{ return p_GetCurrentVersion ? p_GetCurrentVersion(pv) : E_FAIL; }

// Export with undecorated names matching original eax.dll
#pragma comment(linker, "/export:DllCanUnloadNow=_proxy_DllCanUnloadNow@0,@1")
#pragma comment(linker, "/export:DllGetClassObject=_proxy_DllGetClassObject@12,@2")
#pragma comment(linker, "/export:DllRegisterServer=_proxy_DllRegisterServer@0,@3")
#pragma comment(linker, "/export:DllUnregisterServer=_proxy_DllUnregisterServer@0,@4")
#pragma comment(linker, "/export:EAXDirectSoundCreate=_proxy_EAXDirectSoundCreate@12,@5")
#pragma comment(linker, "/export:EAXDirectSoundCreate8=_proxy_EAXDirectSoundCreate8@12,@6")
#pragma comment(linker, "/export:GetCurrentVersion=_proxy_GetCurrentVersion@4,@7")
