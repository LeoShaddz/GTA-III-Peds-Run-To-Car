// PedsRunToCarIII - GTA III 1.0 (PC) (ASI) - NOT COMPILED, NOT TESTED
// Peds that walk to a car to get in (mission peds told to enter your car) run instead of walking.
//
// What it does: CPed::SetMoveState (0x4C5A30, a 13-byte function: mov eax,[esp+4]; mov [ecx+0x22C],eax; ret 4) is replaced.
// When a ped that is NOT the player is in the "seek car" state (m_nPedState == 0x18) and is asked to WALK (2), it is asked to RUN (3).
// All other calls behave exactly as before.
//
// Offsets (read from the user's gta3.exe): m_nPedState = ped+0x224, m_nMoveState = ped+0x22C, model index = entity+0x5C (player = 0).
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cstdlib>

static const uintptr_t FN_SETMOVESTATE = 0x4C5A30;
static const uint8_t   ORIG_BYTES[13]  = { 0x8B, 0x44, 0x24, 0x04, 0x89, 0x81, 0x2C, 0x02, 0x00, 0x00, 0xC2, 0x04, 0x00 };
static const int  OFF_PEDSTATE = 0x224, OFF_MOVESTATE = 0x22C, OFF_MODEL = 0x5C;
static const int  STATE_SEEK_CAR = 0x18;

static bool g_log = false;
static int  g_runState = 3;           // 3 = run, 4 = sprint
static bool g_alsoSprint = false;
static char g_iniPath[MAX_PATH];
static volatile LONG g_logged = 0;

static void Log(const char* fmt, ...)
{
    if (!g_log) return;
    FILE* f = fopen("PedsRunToCarIII.log", "a");
    if (!f) return;
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f); fclose(f);
}

static void __fastcall HookSetMoveState(uint8_t* ped, void* /*edx*/, int state)
{
    const int pedState = *(const int*)(ped + OFF_PEDSTATE);
    if (pedState == STATE_SEEK_CAR && *(const uint16_t*)(ped + OFF_MODEL) != 0)      // seeking a car, not the player
    {
        if (g_log && g_logged < 60) { InterlockedIncrement(&g_logged); Log("ped %p seeking car: SetMoveState(%d) from 0x%08X%s", ped, state, (unsigned)(uintptr_t)_ReturnAddress(), state == 2 ? " -> run" : ""); }
        if (state == 2) state = g_runState;
    }
    *(int*)(ped + OFF_MOVESTATE) = state;
}

BOOL APIENTRY DllMain(HMODULE hm, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(hm);
    GetModuleFileNameA(hm, g_iniPath, MAX_PATH);
    char* dot = strrchr(g_iniPath, '.'); if (dot) strcpy(dot, ".ini");
    g_log = GetPrivateProfileIntA("Settings", "EnableLog", 0, g_iniPath) != 0;
    g_alsoSprint = GetPrivateProfileIntA("Settings", "Sprint", 0, g_iniPath) != 0;
    g_runState = g_alsoSprint ? 4 : 3;
    if (g_log) { FILE* f = fopen("PedsRunToCarIII.log", "w"); if (f) fclose(f); }

    uint8_t* fn = (uint8_t*)FN_SETMOVESTATE;
    if (memcmp(fn, ORIG_BYTES, sizeof(ORIG_BYTES)) != 0)
    {
        Log("Unexpected bytes at 0x4C5A30 (needs GTA III 1.0 PC, or another plugin already patched it). Mod disabled.");
        return TRUE;
    }
    DWORD old;
    if (!VirtualProtect(fn, 5, PAGE_EXECUTE_READWRITE, &old)) { Log("VirtualProtect failed"); return TRUE; }
    fn[0] = 0xE9; *(int32_t*)(fn + 1) = (int32_t)((uintptr_t)&HookSetMoveState - ((uintptr_t)fn + 5));
    VirtualProtect(fn, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 5);
    Log("OK. run state = %d", g_runState);
    return TRUE;
}
