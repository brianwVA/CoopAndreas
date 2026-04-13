#include "stdafx.h"
#include "CrashfixHooks.h"

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
// Original code at 0x5D12D1:
//   mov edx, [0xC16EE8]   ; animation buffer base pointer
//   lea edi, [edx+eax]    ; dest = base + offset
//   ...
//   rep movsd              ; crashes when edx (buffer base) is NULL
// We hook at 0x5D12D1 (6 bytes), check edx for NULL, and skip the
// memcpy block if the buffer is not allocated.
static void __declspec(naked) AnimBuf_NullGuard_Hook()
{
    __asm
    {
        // Replaced instruction: mov edx, [0xC16EE8]
        mov edx, dword ptr ds:[0x00C16EE8]
        test edx, edx
        jnz buf_ok

        // Buffer is NULL — skip the copy and the offset advance.
        // Jump to the function epilogue: pop edi; pop esi; mov al,1; pop ebx; ret
        push 0x005D12F0
        ret

    buf_ok:
        // Continue with original code: lea edi, [edx+eax]
        lea edi, [edx + eax]
        push 0x005D12DA  // return to mov ecx, ebx (next instruction after lea)
        ret
    }
}

void CrashfixHooks::InjectHooks()
{
    patch::RedirectJump(0x6E3D10, CVehicleAnimGroup__ComputeAnimDoorOffsets_Hook);
    patch::RedirectJump(0x4D4610, CAnimManager__BlendAnimation_Hook);
    // Guard against NULL animation buffer at 0x5D12D1 (covers crash at 0x5D12DD)
    patch::RedirectJump(0x5D12D1, AnimBuf_NullGuard_Hook);
}
