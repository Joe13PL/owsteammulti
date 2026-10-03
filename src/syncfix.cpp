// Desync fixes for Original War (OwarOGL_SGUI.exe), applied in memory at load time.
//
// Every patch locates its target by a byte signature, checks it is unique and
// leaves the game untouched if anything does not match.
//
//  1. RngIsolation  - the 21 Urandom::rand_* streams all went through Delphi's
//     global System.RandSeed (RandSeed := seed; Random(n); seed := RandSeed), so
//     any other thread calling Random() (e.g. the master-server thread) or any
//     interleaving could corrupt a simulation stream. Each stream now advances
//     only its own seed with the identical LCG, so results are bit-identical to
//     the original whenever no race happens (compatible with unpatched players).
//  2. FpuGuard      - the simulation relies on x87 rounding (damage formulas are
//     float -> Round). The game sets the control word 0x133F once in InitGame;
//     drivers/DLLs may change it later. It is now re-asserted before every game
//     tick, and every correction is logged.
//  3. DrawRngFix    - per-frame drawing (prep_clovek_/prep_auto_) consumed the
//     animation stream rand_mcanim, which is part of the multiplayer CRC, so the
//     stream could depend on the frame rate. New animations are now picked only by
//     the deterministic per-tick pass (objdraw_tick); at most one tick later.
//  4. SelectEventMP - selecting a unit fired the SAIL script event
//     ActiveUnitChanged on the local machine only; in multiplayer it is no longer
//     raised, so map scripts can't diverge on it.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "common.h"

#ifdef SYNCFIX_TEST
#define SF_NS_BEGIN
#define SF_NS_END
#else
#define SF_NS_BEGIN namespace {
#define SF_NS_END }
#endif
SF_NS_BEGIN

struct CodeRange { uint8_t *begin, *end; };

CodeRange ExeCode() {
    uint8_t *base = (uint8_t *)GetModuleHandleA(nullptr);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);  // Delphi: first section is CODE
    return {base + sec->VirtualAddress, base + sec->VirtualAddress + sec->Misc.VirtualSize};
}

// Pattern: bytes with -1 as wildcard.
bool Match(const uint8_t *p, const int *pat, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (pat[i] >= 0 && p[i] != (uint8_t)pat[i]) return false;
    return true;
}

// Returns the single match, or nullptr if there are none or several.
uint8_t *FindUnique(const CodeRange &c, const int *pat, size_t n) {
    uint8_t *hit = nullptr;
    for (uint8_t *p = c.begin; p + n <= c.end; p++) {
        if (Match(p, pat, n)) {
            if (hit) return nullptr;
            hit = p;
        }
    }
    return hit;
}

bool Write(void *addr, const void *bytes, size_t n) {
    DWORD old;
    if (!VirtualProtect(addr, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(addr, bytes, n);
    VirtualProtect(addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), addr, n);
    return true;
}

uint8_t *CallTarget(uint8_t *call) { return call + 5 + *(int32_t *)(call + 1); }

bool SetCallTarget(uint8_t *call, void *target) {
    int32_t rel = (int32_t)((uint8_t *)target - (call + 5));
    return Write(call + 1, &rel, 4);
}

// ---------------------------------------------------------------- 1. RNG
const int kRandInt[] = {0x53, 0x31, 0xDB, 0x69, 0x93, -1, -1, -1, -1, 0x05, 0x84, 0x08, 0x08, 0x42,
                        0x89, 0x93, -1, -1, -1, -1, 0xF7, 0xE2, 0x89, 0xD0, 0x5B, 0xC3};

int PatchRng(const CodeRange &c) {
    uint8_t *randInt = FindUnique(c, kRandInt, sizeof kRandInt / sizeof *kRandInt);
    if (!randInt) { Log("SyncFix: RandInt not found - RngIsolation skipped"); return 0; }
    // rand_X: push ebp; mov ebp,esp; mov edx,[P]; mov ecx,[S]; mov [edx],ecx; call RandInt;
    //         mov edx,[P]; mov edx,[edx]; mov [S],edx; pop ebp; ret        (38 bytes)
    int n = 0;
    for (uint8_t *p = c.begin; p + 38 <= c.end; p++) {
        if (p[0] != 0x55 || p[1] != 0x8B || p[2] != 0xEC || p[3] != 0x8B || p[4] != 0x15 || p[9] != 0x8B ||
            p[10] != 0x0D || p[15] != 0x89 || p[16] != 0x0A || p[17] != 0xE8 || p[22] != 0x8B || p[23] != 0x15 ||
            p[28] != 0x8B || p[29] != 0x12 || p[30] != 0x89 || p[31] != 0x15 || p[36] != 0x5D || p[37] != 0xC3)
            continue;
        uint32_t ptr = *(uint32_t *)(p + 5), seed = *(uint32_t *)(p + 11);
        if (*(uint32_t *)(p + 24) != ptr || *(uint32_t *)(p + 32) != seed || CallTarget(p + 17) != randInt) continue;
        // Same LCG as Delphi 7 RandInt, on the stream's own seed:
        //   mov ecx,[S]; imul ecx,ecx,08088405h; inc ecx; mov [S],ecx
        //   mov edx,[P]; mov [edx],ecx      (keep RandSeed side effect for legacy raw Random() callers)
        //   mul ecx; mov eax,edx; ret
        uint8_t b[38];
        memset(b, 0xCC, sizeof b);
        uint8_t *q = b;
        *q++ = 0x8B; *q++ = 0x0D; memcpy(q, &seed, 4); q += 4;
        *q++ = 0x69; *q++ = 0xC9; *q++ = 0x05; *q++ = 0x84; *q++ = 0x08; *q++ = 0x08;
        *q++ = 0x41;
        *q++ = 0x89; *q++ = 0x0D; memcpy(q, &seed, 4); q += 4;
        *q++ = 0x8B; *q++ = 0x15; memcpy(q, &ptr, 4); q += 4;
        *q++ = 0x89; *q++ = 0x0A;
        *q++ = 0xF7; *q++ = 0xE1;
        *q++ = 0x8B; *q++ = 0xC2;
        *q++ = 0xC3;
        if (Write(p, b, sizeof b)) n++;
        p += 37;
    }
    Log("SyncFix: RngIsolation patched %d random streams", n);
    return n;
}

// ---------------------------------------------------------------- 2. FPU
const WORD kGameCW = 0x133F;  // value set by Ugame.InitGame
const WORD kCWMask = 0x1F3F;  // exception masks, precision, rounding, infinity; bit 6 is reserved (reads as 1)
void *g_origDoGameTick;
volatile LONG g_fpuFixes;

void __cdecl FpuGuard() {
    WORD cw;
    __asm fnstcw cw
    if ((cw & kCWMask) != (kGameCW & kCWMask)) {
        LONG n = InterlockedIncrement(&g_fpuFixes);
        if (n <= 20 || (n & (n - 1)) == 0)
            Log("SyncFix: FPU control word was %04X, restored %04X before game tick (correction #%ld)", cw, kGameCW, n);
        WORD want = kGameCW;
        __asm fldcw want
    }
}

__declspec(naked) void DoGameTickHook() {
    __asm {
        push eax
        push ecx
        push edx
        call FpuGuard
        pop edx
        pop ecx
        pop eax
        jmp [g_origDoGameTick]
    }
}

const int kDoGameTick[] = {0x55, 0x8B, 0xEC, 0x53, 0xA1, -1, -1, -1, -1, 0x83, 0x38, 0x64, 0x7D, -1, 0xA1, -1, -1, -1,
                           -1, 0x8B, 0x00, 0xB9, 0x0A, 0x00, 0x00, 0x00, 0x99, 0xF7, 0xF9, 0x4A};

void PatchFpu(const CodeRange &c) {
    uint8_t *fn = FindUnique(c, kDoGameTick, sizeof kDoGameTick / sizeof *kDoGameTick);
    if (!fn) { Log("SyncFix: DoGameTick not found - FpuGuard skipped"); return; }
    g_origDoGameTick = fn;
    int n = 0;
    for (uint8_t *p = c.begin; p + 5 <= c.end; p++)
        if (*p == 0xE8 && CallTarget(p) == fn && SetCallTarget(p, (void *)DoGameTickHook)) n++;
    Log("SyncFix: FpuGuard installed at %d call site(s) of DoGameTick", n);
}

// ---------------------------------------------------------------- 3. draw RNG
// cmp byte ptr [edi+0D0h],0; je +7; mov eax,edi; call draw_*newcin
const int kNewCinSite[] = {0x80, 0xBF, 0xD0, 0x00, 0x00, 0x00, 0x00, 0x74, 0x07, 0x8B, 0xC7, 0xE8};

void PatchDrawRng(const CodeRange &c) {
    const size_t n = sizeof kNewCinSite / sizeof *kNewCinSite;
    int found = 0, patched = 0;
    for (uint8_t *p = c.begin; p + n + 4 <= c.end; p++) {
        if (!Match(p, kNewCinSite, n)) continue;
        found++;
        static const uint8_t nop5[5] = {0x0F, 0x1F, 0x44, 0x00, 0x00};
        if (Write(p + 11, nop5, 5)) patched++;
    }
    if (found != 2) Log("SyncFix: DrawRngFix expected 2 sites, found %d", found);
    Log("SyncFix: DrawRngFix removed %d per-frame animation RNG call(s)", patched);
}

// ---------------------------------------------------------------- 4. select event
void *g_origSailEvent1;
uint8_t **g_ppMultiplayer;  // PTR_Multiplayer -> Boolean

__declspec(naked) void SelectEventHook() {
    __asm {
        push ecx
        mov ecx, g_ppMultiplayer
        mov ecx, [ecx]
        cmp byte ptr [ecx], 0
        pop ecx
        jne skip
        jmp [g_origSailEvent1]
    skip:
        ret
    }
}

const int kSelectSite[] = {0x0F, 0xBF, 0x50, 0x14, 0xB8, 0x1F, 0x00, 0x00, 0x00, 0xE8};
const int kDoMultiplayerLog[] = {0x55, 0x8B, 0xEC, 0xA1, -1, -1, -1, -1, 0x80, 0x38, 0x00, 0x74, -1, 0xA1, -1, -1,
                                 -1, -1, 0x8B, 0x00, 0xB9, 0x14, 0x00, 0x00, 0x00, 0x99, 0xF7, 0xF9, 0x83, 0xFA, 0x05};

void PatchSelectEvent(const CodeRange &c) {
    uint8_t *site = FindUnique(c, kSelectSite, sizeof kSelectSite / sizeof *kSelectSite);
    uint8_t *mlog = FindUnique(c, kDoMultiplayerLog, sizeof kDoMultiplayerLog / sizeof *kDoMultiplayerLog);
    if (!site || !mlog) { Log("SyncFix: ActiveUnitChanged site not found - SelectEventMP skipped"); return; }
    g_ppMultiplayer = *(uint8_t ***)(mlog + 4);
    uint8_t *call = site + 9;
    g_origSailEvent1 = CallTarget(call);
    if (SetCallTarget(call, (void *)SelectEventHook)) Log("SyncFix: SelectEventMP installed");
}

SF_NS_END  // namespace

void SyncFix_Apply(const char *ini) {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    const char *name = strrchr(exe, '\\');
    name = name ? name + 1 : exe;
    if (_stricmp(name, "OwarOGL_SGUI.exe") != 0) return;  // other tools in the folder: leave alone
    if (!GetPrivateProfileIntA("SyncFix", "Enabled", 1, ini)) { Log("SyncFix: disabled"); return; }
    CodeRange c = ExeCode();
    if (GetPrivateProfileIntA("SyncFix", "RngIsolation", 1, ini)) PatchRng(c);
    if (GetPrivateProfileIntA("SyncFix", "FpuGuard", 1, ini)) PatchFpu(c);
    if (GetPrivateProfileIntA("SyncFix", "DrawRngFix", 1, ini)) PatchDrawRng(c);
    if (GetPrivateProfileIntA("SyncFix", "SelectEventMP", 1, ini)) PatchSelectEvent(c);
}
