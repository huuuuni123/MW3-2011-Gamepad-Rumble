// MW3KoreanBridge.asi
// Lets the Korean patch (d3d9.dll proxy) and MW3GamepadFix (MinHook ASI) hook the
// same two game functions (FindAsset, CountChars) without either refusing.
//
// Order of events:
//   1. MW3GamepadFix hooks the functions at ASI load (MinHook: E9 jmp -> relay).
//   2. The game calls Direct3DCreate9 -> we intercept it (IAT patch).
//   3. prep(): temporarily put the original prologue back so the Korean patch's
//      byte check passes.
//   4. Korean d3d9.dll Direct3DCreate9 runs and installs its hooks (FF25 abs jmp).
//   5. post(): put MinHook's E9 back at the function start and redirect MinHook's
//      trampoline to the Korean hook, giving the chain
//      game -> gamepad detour -> (trampoline) -> Korean hook -> original.
// Neither mod's files are modified.

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

typedef void *(WINAPI *D3DCreate9_t)(UINT);

typedef struct {
    const char *name;
    uint32_t rva;
    uint8_t orig[16];
    size_t len;          // bytes the Korean patch compares
    // runtime
    int state;           // 0 none, 1 restored (gamepad hook removed temporarily), 2 unknown hook
    uint8_t cur[16];     // bytes present before we touched it (gamepad hook)
    uint8_t *tramp;      // MinHook trampoline
    size_t copyLen;      // bytes MinHook copied into trampoline
} Target;

static Target g_t[] = {
    {"FindAsset", 0xA5A20,
     {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18}, 15},
    {"CountChars", 0x289E80,
     {0x48,0x83,0xec,0x08,0x48,0x8b,0xd1,0x48,0x85,0xc9,0x75,0x07,0x33,0xc0,0x48,0x83}, 16},
};
#define NT (sizeof(g_t) / sizeof(g_t[0]))

static D3DCreate9_t g_real;
static volatile LONG g_done;
static uint8_t *g_exportAddr;
static uint8_t g_exportSaved[14];
static char g_logPath[MAX_PATH];

static void logf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 3) n = sizeof(buf) - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';
    HANDLE h = CreateFileA(g_logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w;
    WriteFile(h, buf, (DWORD)n, &w, NULL);
    CloseHandle(h);
}

static int readMem(const void *addr, void *out, size_t n)
{
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), addr, out, n, &got) && got == n;
}

static int writeCode(void *addr, const void *src, size_t n)
{
    DWORD old;
    if (!VirtualProtect(addr, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(addr, src, n);
    VirtualProtect(addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), addr, n);
    return 1;
}

static const uint8_t kAbsJmp[6] = {0xFF, 0x25, 0, 0, 0, 0};

// Recognise a MinHook x64 hook at T and find its trampoline.
// MinHook layout: T: E9 rel32 -> relay; relay = tramp + copyLen + 14;
// tramp = original instructions (copyLen bytes) + FF25 00000000 <T+copyLen>.
static int analyseMinHook(Target *t, uint8_t *T)
{
    if (t->cur[0] != 0xE9) return 0;
    int32_t rel;
    memcpy(&rel, t->cur + 1, 4);
    uint8_t *relay = T + 5 + rel;
    uint8_t r[14];
    if (!readMem(relay, r, 14) || memcmp(r, kAbsJmp, 6) != 0) return 0;
    for (size_t L = 5; L <= 16; L++) {
        uint8_t *tr = relay - (L + 14);
        uint8_t buf[16 + 14];
        if (!readMem(tr, buf, L + 14)) continue;
        if (L <= t->len && memcmp(buf, t->orig, L) != 0) continue;
        if (L > t->len && memcmp(buf, t->orig, t->len) != 0) continue;
        if (memcmp(buf + L, kAbsJmp, 6) != 0) continue;
        uint64_t back;
        memcpy(&back, buf + L + 6, 8);
        if (back != (uint64_t)(uintptr_t)(T + L)) continue;
        t->tramp = tr;
        t->copyLen = L;
        return 1;
    }
    return 0;
}

static void prep(void)
{
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    for (size_t i = 0; i < NT; i++) {
        Target *t = &g_t[i];
        uint8_t *T = base + t->rva;
        if (!readMem(T, t->cur, t->len)) {
            logf("%s: unreadable, skipped", t->name);
            t->state = 2;
            continue;
        }
        if (memcmp(t->cur, t->orig, t->len) == 0) {
            t->state = 0;
            logf("%s: original prologue (no other hook)", t->name);
            continue;
        }
        if (!analyseMinHook(t, T)) {
            t->state = 2;
            logf("%s: unknown hook present, left alone", t->name);
            continue;
        }
        if (!writeCode(T, t->orig, t->len)) {
            t->state = 2;
            logf("%s: could not restore prologue", t->name);
            continue;
        }
        t->state = 1;
        logf("%s: gamepad hook found (trampoline %p, %u bytes), prologue restored for Korean patch",
             t->name, (void *)t->tramp, (unsigned)t->copyLen);
    }
}

static void post(void)
{
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    for (size_t i = 0; i < NT; i++) {
        Target *t = &g_t[i];
        if (t->state != 1) continue;
        uint8_t *T = base + t->rva;
        uint8_t now[14];
        if (!readMem(T, now, 14) || memcmp(now, kAbsJmp, 6) != 0) {
            // Korean patch did not hook it: give the gamepad hook back as it was.
            writeCode(T, t->cur, t->len);
            logf("%s: Korean patch did not hook; gamepad hook restored", t->name);
            continue;
        }
        uint64_t korean;
        memcpy(&korean, now + 6, 8);
        // Trampoline -> Korean hook (absolute jmp, 14 bytes; trampoline is copyLen+14 long).
        uint8_t j[14];
        memcpy(j, kAbsJmp, 6);
        memcpy(j + 6, &korean, 8);
        if (!writeCode(t->tramp, j, 14)) {
            writeCode(T, t->cur, t->len);
            logf("%s: could not patch trampoline; gamepad hook restored (Korean hook dropped)", t->name);
            continue;
        }
        // Function start -> gamepad relay again. Write the whole saved prefix
        // (E9 + untouched original bytes) so that if MinHook ever disables its hook
        // (restoring its 5-byte backup) the function is intact original code
        // rather than the tail of the Korean jmp. The Korean trampoline returns to
        // T+len, which is unaffected.
        writeCode(T, t->cur, t->len);
        logf("%s: chained gamepad -> Korean (%p)", t->name, (void *)(uintptr_t)korean);
    }
}

static void *WINAPI Hook_Direct3DCreate9(UINT sdk)
{
    if (InterlockedCompareExchange(&g_done, 1, 0) == 0) {
        logf("Direct3DCreate9 intercepted");
        prep();
        void *r = g_real(sdk);
        post();
        return r;
    }
    return g_real(sdk);
}

// Fallback when the IAT entry is not found: one-shot patch of the export itself.
static void *WINAPI Hook_Export(UINT sdk)
{
    writeCode(g_exportAddr, g_exportSaved, 14);
    return Hook_Direct3DCreate9(sdk);
}

static int patchIAT(uint8_t *exe, void *target, void *repl)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)exe;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(exe + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return 0;
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(exe + dir.VirtualAddress);
    for (; d->Name; d++) {
        if (_stricmp((char *)(exe + d->Name), "d3d9.dll") != 0) continue;
        void **iat = (void **)(exe + d->FirstThunk);
        for (; *iat; iat++) {
            if (*iat == target) {
                DWORD old;
                if (!VirtualProtect(iat, sizeof(void *), PAGE_READWRITE, &old)) return 0;
                *iat = repl;
                VirtualProtect(iat, sizeof(void *), old, &old);
                return 1;
            }
        }
    }
    return 0;
}

static void install(void)
{
    HMODULE d3d = GetModuleHandleA("d3d9.dll");
    if (!d3d) {
        logf("d3d9.dll not loaded; nothing to do");
        return;
    }
    // The Korean patch is a d3d9.dll proxy in the game folder. If d3d9 came from
    // anywhere else (System32), there is no Korean patch to chain: stay out of the way.
    char exeDir[MAX_PATH], d3dPath[MAX_PATH];
    GetModuleFileNameA(NULL, exeDir, MAX_PATH);
    GetModuleFileNameA(d3d, d3dPath, MAX_PATH);
    char *e = strrchr(exeDir, '\\'), *d = strrchr(d3dPath, '\\');
    if (!e || !d || (e - exeDir) != (d - d3dPath) || _strnicmp(exeDir, d3dPath, (size_t)(e - exeDir)) != 0) {
        logf("no Korean patch (d3d9 from %s); nothing to do", d3dPath);
        return;
    }
    g_real = (D3DCreate9_t)GetProcAddress(d3d, "Direct3DCreate9");
    if (!g_real) {
        logf("Direct3DCreate9 not found");
        return;
    }
    if (patchIAT((uint8_t *)GetModuleHandleA(NULL), (void *)g_real, (void *)Hook_Direct3DCreate9)) {
        logf("installed (IAT)");
        return;
    }
    g_exportAddr = (uint8_t *)g_real;
    if (!readMem(g_exportAddr, g_exportSaved, 14)) return;
    uint8_t j[14];
    uint64_t a = (uint64_t)(uintptr_t)Hook_Export;
    memcpy(j, kAbsJmp, 6);
    memcpy(j + 6, &a, 8);
    // g_real must not be called while patched; Hook_Export restores it first.
    if (writeCode(g_exportAddr, j, 14)) logf("installed (export patch)");
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        GetModuleFileNameA(NULL, g_logPath, MAX_PATH);
        // Single-player only: never touch any other process (e.g. iw5mp.exe, which is VAC-protected).
        const char *nm = strrchr(g_logPath, '\\');
        nm = nm ? nm + 1 : g_logPath;
        if (_stricmp(nm, "iw5sp.exe") != 0) return TRUE;
        char *s = strrchr(g_logPath, '\\');
        if (s) strcpy(s + 1, "MW3KoreanBridge.log");
        else strcpy(g_logPath, "MW3KoreanBridge.log");
        DeleteFileA(g_logPath);
        logf("MW3KoreanBridge 1.0 loaded");
        install();
    }
    return TRUE;
}
