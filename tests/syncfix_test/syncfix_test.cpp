// Runs the real OwarOGL_SGUI.exe code (mapped, not started) before/after SyncFix patches.
// Usage: syncfix_test.exe "<game dir>\OwarOGL_SGUI.exe"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
struct CodeRange { uint8_t *begin, *end; };
CodeRange ExeCode();
int PatchRng(const CodeRange &c);
void PatchFpu(const CodeRange &c);
void PatchDrawRng(const CodeRange &c);
void PatchSelectEvent(const CodeRange &c);
extern void *g_origDoGameTick, *g_origSailEvent1;
extern uint8_t **g_ppMultiplayer;
void DoGameTickHook();
void SelectEventHook();
void Log(const char *fmt, ...) { va_list ap; va_start(ap, fmt); printf("   [log] "); vprintf(fmt, ap); printf("\n"); va_end(ap); }

static HMODULE g_exe;
static intptr_t g_delta;
#define RB(a) ((uint8_t *)((intptr_t)(a) + g_delta))
// Map the exe image ourselves (sections + base relocations), without running it.
static HMODULE MapImage(const char *path) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return nullptr;
    DWORD sz = GetFileSize(f, nullptr), rd;
    std::vector<uint8_t> file(sz);
    ReadFile(f, file.data(), sz, &rd, nullptr);
    CloseHandle(f);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(file.data() + ((IMAGE_DOS_HEADER *)file.data())->e_lfanew);
    uint8_t *img = (uint8_t *)VirtualAlloc((void *)nt->OptionalHeader.ImageBase, nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!img) img = (uint8_t *)VirtualAlloc(nullptr, nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    memcpy(img, file.data(), nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (s[i].SizeOfRawData) memcpy(img + s[i].VirtualAddress, file.data() + s[i].PointerToRawData, s[i].SizeOfRawData);
    g_delta = (intptr_t)img - (intptr_t)nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY rel = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (g_delta && rel.Size) {
        uint8_t *p = img + rel.VirtualAddress, *e = p + rel.Size;
        while (p < e) {
            IMAGE_BASE_RELOCATION *b = (IMAGE_BASE_RELOCATION *)p;
            if (!b->SizeOfBlock) break;
            WORD *w = (WORD *)(b + 1);
            for (DWORD k = 0; k < (b->SizeOfBlock - sizeof *b) / 2; k++)
                if ((w[k] >> 12) == IMAGE_REL_BASED_HIGHLOW) *(uint32_t *)(img + b->VirtualAddress + (w[k] & 0xFFF)) += (uint32_t)g_delta;
            p += b->SizeOfBlock;
        }
    }
    return (HMODULE)img;
}
static int fails;
#define CHECK(c, ...) do { if (c) printf("ok: "); else { printf("FAIL: "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static CodeRange Code() {
    uint8_t *base = (uint8_t *)g_exe;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    return {base + s->VirtualAddress, base + s->VirtualAddress + s->Misc.VirtualSize};
}
struct Stream { uint8_t *fn; uint32_t *seed; };
static uint32_t *g_randSeed;
static std::vector<Stream> FindStreams(const CodeRange &c) {
    std::vector<Stream> v;
    for (uint8_t *p = c.begin; p + 38 <= c.end; p++)
        if (p[0] == 0x55 && p[1] == 0x8B && p[2] == 0xEC && p[3] == 0x8B && p[4] == 0x15 && p[9] == 0x8B && p[10] == 0x0D &&
            p[17] == 0xE8 && p[36] == 0x5D && p[37] == 0xC3) {
            v.push_back({p, *(uint32_t **)(p + 11)});
            g_randSeed = **(uint32_t ***)(p + 5);
        }
    return v;
}
static uint32_t CallRand(uint8_t *fn, uint32_t range) {
    uint32_t r;
    __asm { mov eax, range
            call fn
            mov r, eax }
    return r;
}
// Run a deterministic interleaved sequence over all streams, return a trace.
static std::vector<uint32_t> Run(const std::vector<Stream> &s) {
    for (size_t i = 0; i < s.size(); i++) *s[i].seed = 0x1234567u * (uint32_t)(i + 1);
    *g_randSeed = 0xDEADBEEF;
    std::vector<uint32_t> t;
    uint32_t x = 1;
    for (int k = 0; k < 20000; k++) {
        x = x * 1103515245u + 12345u;
        const Stream &st = s[(x >> 16) % s.size()];
        uint32_t range = 1 + (x >> 8) % 1000;
        t.push_back(CallRand(st.fn, range));
        t.push_back(*g_randSeed);  // legacy side effect must match too
    }
    for (auto &st : s) t.push_back(*st.seed);
    return t;
}

static WORD g_seenCW; static uint32_t g_seenEAX, g_seenEDX;
static __declspec(naked) void FakeTick() {
    __asm { mov g_seenEAX, eax
            fnstcw g_seenCW
            ret }
}
static int g_eventCalls; static uint32_t g_evA, g_evD, g_evC;
static __declspec(naked) void FakeSailEvent() {
    __asm { mov g_evA, eax
            mov g_evD, edx
            mov g_evC, ecx
            inc g_eventCalls
            ret }
}

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char *exe = argc > 1 ? argv[1] : "OwarOGL_SGUI.exe";
    g_exe = MapImage(exe);
    if (!g_exe) { printf("cannot map exe (%lu)\n", GetLastError()); return 1; }
    printf("exe mapped at %p (delta %+ld)\n", g_exe, (long)g_delta);
    CodeRange c = Code();
    DWORD old;
    VirtualProtect(c.begin, c.end - c.begin, PAGE_EXECUTE_READWRITE, &old);

    std::vector<Stream> streams = FindStreams(c);
    CHECK(streams.size() == 21, "found %u rand streams", (unsigned)streams.size());
    std::vector<uint32_t> before = Run(streams);
    int n = PatchRng(c);
    CHECK(n == 21, "patched %d streams", n);
    std::vector<uint32_t> after = Run(streams);
    CHECK(before == after, "patched RNG bit-identical to original over %u values (incl. RandSeed side effect)", (unsigned)before.size());
    // Isolation: clobbering the global RandSeed between calls no longer changes stream output.
    for (size_t i = 0; i < streams.size(); i++) *streams[i].seed = 777u + (uint32_t)i;
    uint32_t a1 = CallRand(streams[3].fn, 1000), a2 = CallRand(streams[3].fn, 1000);
    *streams[3].seed = 777u + 3;
    uint32_t b1 = CallRand(streams[3].fn, 1000);
    *g_randSeed = 0x55555555;  // simulate another thread calling Random()
    uint32_t b2 = CallRand(streams[3].fn, 1000);
    CHECK(a1 == b1 && a2 == b2, "stream unaffected by foreign RandSeed writes");

    PatchFpu(c);
    CHECK(g_origDoGameTick != nullptr, "DoGameTick located at %p", g_origDoGameTick);
    void *realTick = g_origDoGameTick;
    g_origDoGameTick = (void *)FakeTick;
    WORD bad = 0x027F;
    __asm fldcw bad
    __asm { mov eax, 0x11223344
            call DoGameTickHook }
    CHECK((g_seenCW & 0x1F3F) == 0x133F && g_seenEAX == 0x11223344, "FpuGuard restores CW (%04X) and keeps EAX (%08X)", g_seenCW, g_seenEAX);
    // already-correct CW must not be "corrected" (no log spam every tick)
    WORD good = 0x133F; __asm fldcw good
    extern volatile LONG g_fpuFixes;
    LONG fixesBefore = g_fpuFixes;
    __asm { mov eax, 1
            call DoGameTickHook }
    CHECK(g_fpuFixes == fixesBefore, "correct CW left alone (fixes %ld)", g_fpuFixes);
    WORD def = 0x027F; __asm fldcw def
    g_origDoGameTick = realTick;

    uint8_t s1[5], s2[5];
    memcpy(s1, RB(0x4E8227) + 11, 5); memcpy(s2, RB(0x4E8928) + 11, 5);
    PatchDrawRng(c);
    CHECK(s1[0] == 0xE8 && s2[0] == 0xE8 && !memcmp(RB(0x4E8227) + 11, "\x0F\x1F\x44\x00\x00", 5) &&
          !memcmp(RB(0x4E8928) + 11, "\x0F\x1F\x44\x00\x00", 5), "both per-frame newcin calls replaced by NOP");

    PatchSelectEvent(c);
    CHECK(g_origSailEvent1 == (void *)RB(0x62CC54), "SAIL_event1 resolved to %p", g_origSailEvent1);
    void *realEv = g_origSailEvent1; uint8_t **realMp = g_ppMultiplayer;
    static uint8_t mp; static uint8_t *pmp = &mp;
    g_ppMultiplayer = &pmp; g_origSailEvent1 = (void *)FakeSailEvent;
    mp = 0;
    __asm { mov eax, 0x1F
            mov edx, 42
            mov ecx, 7
            call SelectEventHook }
    CHECK(g_eventCalls == 1 && g_evA == 0x1F && g_evD == 42 && g_evC == 7, "single player: event forwarded with registers intact");
    mp = 1;
    __asm { mov eax, 0x1F
            mov edx, 42
            call SelectEventHook }
    CHECK(g_eventCalls == 1, "multiplayer: ActiveUnitChanged suppressed");
    CHECK(realMp && *realMp && (uintptr_t)*realMp > (uintptr_t)g_exe, "real Multiplayer flag pointer %p -> %p", realMp, realMp ? *realMp : 0);
    printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails;
}
