#include "stdafx.h"
#include "CrashfixHooks.h"

// VEH crash recovery globals from COpCodeSync.cpp
extern volatile bool g_bInNativeHandlerCall;
extern volatile DWORD g_savedNativeHandlerEsp;
extern volatile DWORD g_nativeHandlerRecoveryEip;

// Vectored Exception Handler for crash recovery.
//
// Priority 1: Native opcode handler crash recovery.
//   During OpcodeProcessingWellDone_Hook's `call edx`, if the handler crashes
//   (e.g. EIP lands in data section 0xA8E99E), we restore ESP and jump to a
//   recovery stub that safely continues the game loop.
//
// Priority 2: Corrupted function pointer recovery.
//   If EIP < 0x10000 (e.g. EIP=0x1 from corrupted vtable/callback), pop the
//   return address from the stack and resume execution with EAX=0.
static LONG CALLBACK InvalidCallGuard(PEXCEPTION_POINTERS ex)
{
    if (ex->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;

    // Priority 1: If we're inside the native handler call, recover via the
    // pre-built recovery stub. This handles crashes at any EIP (data section,
    // wild jumps, etc.) that happen during opcode processing.
    if (g_bInNativeHandlerCall && g_nativeHandlerRecoveryEip != 0)
    {
        g_bInNativeHandlerCall = false;
        ex->ContextRecord->Esp = g_savedNativeHandlerEsp;
        ex->ContextRecord->Eip = g_nativeHandlerRecoveryEip;
        ex->ContextRecord->Eax = 0;
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    DWORD eip = ex->ContextRecord->Eip;

    // Priority 2: Corrupted function pointer (EIP in first 64KB)
    if (eip < 0x10000)
    {
        DWORD* esp = (DWORD*)ex->ContextRecord->Esp;
        DWORD returnAddr = *esp;

        if (returnAddr < 0x10000)
            return EXCEPTION_CONTINUE_SEARCH;

        ex->ContextRecord->Eip = returnAddr;
        ex->ContextRecord->Esp += 4;
        ex->ContextRecord->Eax = 0;

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

// i hope it will work
void __declspec(naked) CVehicleAnimGroup__ComputeAnimDoorOffsets_Hook()
{
    __asm
    {
        // ensure door id < 16
        mov edx, [esp + 8] // get door id
        cmp edx, 16 // compare
        jb cont // if lower than 16, good, exit the hook 

        // zero if out of range
        mov edx, 0
        mov[esp + 8], edx

        cont:
        // code cave
        mov     edx, [esp + 8]
        lea     eax, [edx + edx * 2]

        push 0x6E3D17
        ret
    }
}

static void __declspec(naked) CAnimManager__BlendAnimation_Hook()
{
    __asm
    {
        mov eax, [esp + 4] // get RpClump*
        test eax, eax // check for nullptr

        jnz cont // jump if ok

        retn // exit if not ok

        cont:
        // code cave
        sub esp, 0x14
        mov ecx, [esp + 0x14 + 0x4]

        push 0x4D4617
        ret
    }
}

// Crashfix for 0x005D12DD — rep movsd writes to NULL destination.
// Disassembly around the crash site:
//   0x5D12C8: A1 EC6EC100        mov eax, [0xC16EEC]   ; buffer write-offset
//   0x5D12CD: 8B15 E86EC100      mov edx, [0xC16EE8]   ; buffer base pointer
//   0x5D12D3: 8D3C02             lea edi, [edx+eax]     ; dest = base + offset
//   0x5D12D6: 8BCB               mov ecx, ebx           ; byte count
//   ...
//   0x5D12DD: F3A5               rep movsd              ; ← crashes when edx is NULL
//   ...
//   0x5D12EC: 5F                 pop edi                ; epilogue
//   0x5D12ED: 5E                 pop esi
//   0x5D12EE: B001               mov al, 1
//   0x5D12F0: 5B                 pop ebx
//   0x5D12F1: C3                 ret
//
// Hook at 0x5D12CD (6-byte mov edx instruction), check for NULL buffer,
// skip the copy block if not allocated.
static void __declspec(naked) AnimBuf_NullGuard_Hook()
{
    __asm
    {
        // Replaced instruction: mov edx, [0xC16EE8]  (6 bytes at 0x5D12CD)
        mov edx, dword ptr ds:[0x00C16EE8]
        test edx, edx
        jnz buf_ok

        // Buffer is NULL — skip copy + offset advance, jump to epilogue.
        // 0x5D12EC = pop edi; pop esi; mov al,1; pop ebx; ret
        push 0x005D12EC
        ret

    buf_ok:
        // Buffer valid — continue to lea edi,[edx+eax] at 0x5D12D3
        push 0x005D12D3
        ret
    }
}

void CrashfixHooks::InjectHooks()
{
    patch::RedirectJump(0x6E3D10, CVehicleAnimGroup__ComputeAnimDoorOffsets_Hook);
    patch::RedirectJump(0x4D4610, CAnimManager__BlendAnimation_Hook);
    // Guard against NULL animation buffer at 0x5D12CD (covers crash at 0x5D12DD)
    patch::RedirectJump(0x5D12CD, AnimBuf_NullGuard_Hook);
    // Catch calls through corrupted function pointers (EIP < 0x10000)
    AddVectoredExceptionHandler(1, InvalidCallGuard);
}
