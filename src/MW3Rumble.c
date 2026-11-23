// MW3Rumble.asi - controller vibration for Call of Duty: Modern Warfare 3 (SP, iw5sp.exe)
//
// Sources of rumble:
//   * CG_EntityEvent hook (iw5sp+0x42540): weapon fire, melee, explosions, landing.
//   * Polling (read-only, no hook): damage feedback (playerState damageEvent/damageCount)
//     and camera shake slots (earthquakes / script shakes).
// Output: XInputSetState on the first connected pad. Settings in MW3Rumble.ini.

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

// ---------------------------------------------------------------- game layout (iw5sp.exe, Steam)
#define RVA_ENTITY_EVENT 0x42540
#define RVA_CG           0x514580        // cg_t for local client 0
#define CG_TIME          0x15af4         // int
#define CG_VIEWORG       0x208e0         // float[3]
#define RVA_PS           0x52a084        // cg.predictedPlayerState
#define PS_CLIENTNUM     0x44
#define PS_DAMAGEEVENT   0x14c
#define PS_DAMAGECOUNT   0x158
#define RVA_SHAKES       0x4e9870        // 4 slots * 0x24
#define CENT_ORIGIN      0x18
#define CENT_ES          0xdc
#define ES_EVENTPARM     0x08
#define ES_ENTNUM        0x84            // uint16

static const uint8_t kEventPrologue[16] = {
    0x45, 0x85, 0xc0,                         // test r8d, r8d
    0x0f, 0x84, 0x4a, 0x21, 0x00, 0x00,       // je   ret
    0x48, 0x8b, 0xc4,                         // mov  rax, rsp
    0x44, 0x89, 0x40, 0x18                    // mov  [rax+18h], r8d
};

enum {
    EV_FIRE_WEAPON = 36, EV_FIRE_WEAPON_LASTSHOT = 37,
    EV_FIRE_WEAPON_LEFT = 41, EV_FIRE_WEAPON_LASTSHOT_LEFT = 42,
    EV_FIRE_MELEE = 45, EV_MELEE_HIT = 49, EV_FIRE_TURRET = 52,
    EV_GRENADE_EXPLODE = 71, EV_ROCKET_EXPLODE = 74, EV_ROCKET_EXPLODE_NOMARKS = 75,
    EV_FLASHBANG_EXPLODE = 76, EV_CUSTOM_EXPLODE = 77, EV_CUSTOM_EXPLODE_NOMARKS = 78,
    EV_DUD_EXPLODE = 80, EV_PHYS_EXPLOSION_JOLT = 90,
    EV_LANDING_PAIN_FIRST = 143, EV_LANDING_PAIN_LAST = 173,
};

// ---------------------------------------------------------------- settings
static float s_strength = 1.0f, s_fire = 1.0f, s_explosion = 1.0f, s_damage = 1.0f,
             s_shake = 1.0f, s_land = 1.0f, s_explRadius = 1600.0f;
static int s_padIndex = -1, s_debug = 0;

// ---------------------------------------------------------------- utils
static char g_dir[MAX_PATH];
static uint8_t *g_base;

static void rlog(const char *fmt, ...)
{
    char path[MAX_PATH], buf[512];
    snprintf(path, sizeof(path), "%sMW3Rumble.log", g_dir);
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 3) n = sizeof(buf) - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w;
    WriteFile(h, buf, (DWORD)n, &w, NULL);
    CloseHandle(h);
}

static float iniF(const char *path, const char *key, float def)
{
    char b[32], d[32];
    snprintf(d, sizeof(d), "%g", def);
    GetPrivateProfileStringA("Rumble", key, d, b, sizeof(b), path);
    return (float)atof(b);
}

static void loadSettings(void)
{
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%sMW3Rumble.ini", g_dir);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        const char *def =
            "[Rumble]\r\n"
            "; Overall strength (0 = off, 1 = default, up to 2)\r\n"
            "Strength=1.0\r\n"
            "; Per-source multipliers\r\n"
            "Fire=1.0\r\n"
            "Explosion=1.0\r\n"
            "Damage=1.0\r\n"
            "Shake=1.0\r\n"
            "Landing=1.0\r\n"
            "; Explosions farther than this (game units, ~40 per metre) do not rumble\r\n"
            "ExplosionRadius=1600\r\n"
            "; XInput pad 0-3, or -1 = first connected\r\n"
            "Pad=-1\r\n"
            "; 1 = log every rumble event to MW3Rumble.log\r\n"
            "Debug=0\r\n";
        HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD w;
            WriteFile(h, def, (DWORD)strlen(def), &w, NULL);
            CloseHandle(h);
        }
    }
    s_strength = iniF(path, "Strength", 1.0f);
    s_fire = iniF(path, "Fire", 1.0f);
    s_explosion = iniF(path, "Explosion", 1.0f);
    s_damage = iniF(path, "Damage", 1.0f);
    s_shake = iniF(path, "Shake", 1.0f);
    s_land = iniF(path, "Landing", 1.0f);
    s_explRadius = iniF(path, "ExplosionRadius", 1600.0f);
    s_padIndex = GetPrivateProfileIntA("Rumble", "Pad", -1, path);
    s_debug = GetPrivateProfileIntA("Rumble", "Debug", 0, path);
}

// ---------------------------------------------------------------- impulses
typedef struct { ULONGLONG start; DWORD dur; float lo, hi; } Impulse;
#define NIMP 16
static Impulse g_imp[NIMP];
static CRITICAL_SECTION g_cs;

static ULONGLONG nowMs(void) { return GetTickCount64(); }

static void addImpulse(float lo, float hi, DWORD dur)
{
    if (lo <= 0.0f && hi <= 0.0f) return;
    ULONGLONG t = nowMs();
    EnterCriticalSection(&g_cs);
    int slot = 0;
    float weakest = 1e9f;
    for (int i = 0; i < NIMP; i++) {
        if (t >= g_imp[i].start + g_imp[i].dur) { slot = i; weakest = -1; break; }
        float w = g_imp[i].lo + g_imp[i].hi;
        if (w < weakest) { weakest = w; slot = i; }
    }
    g_imp[slot].start = t;
    g_imp[slot].dur = dur;
    g_imp[slot].lo = lo;
    g_imp[slot].hi = hi;
    LeaveCriticalSection(&g_cs);
}

static void clearImpulses(void)
{
    EnterCriticalSection(&g_cs);
    memset(g_imp, 0, sizeof(g_imp));
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------- game access
static inline uint8_t *cg(void) { return g_base + RVA_CG; }
static inline uint8_t *ps(void) { return g_base + RVA_PS; }

static float distToView(const float *o)
{
    const float *v = (const float *)(cg() + CG_VIEWORG);
    float dx = o[0] - v[0], dy = o[1] - v[1], dz = o[2] - v[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// ---------------------------------------------------------------- CG_EntityEvent hook
typedef void (*EntityEvent_t)(int, uint8_t *, int, int);
static EntityEvent_t g_tramp;
static int g_lastEv;
static ULONGLONG g_lastEvT;

static void onEvent(uint8_t *cent, int ev, int flags)
{
    int entnum = *(uint16_t *)(cent + CENT_ES + ES_ENTNUM);
    int me = *(int *)(ps() + PS_CLIENTNUM);
    int local = (flags & 1) || entnum == me;
    float k;

    switch (ev) {
    case EV_FIRE_WEAPON:
    case EV_FIRE_WEAPON_LASTSHOT:
    case EV_FIRE_WEAPON_LEFT:
    case EV_FIRE_WEAPON_LASTSHOT_LEFT:
    case EV_FIRE_TURRET:
        if (!local) return;
        // The same shot can arrive from the player state and the entity: dedupe.
        if (ev == g_lastEv && nowMs() - g_lastEvT < 20) return;
        k = s_fire;
        if (ev == EV_FIRE_TURRET) addImpulse(0.55f * k, 0.75f * k, 110);
        else addImpulse(0.30f * k, 0.65f * k, 85);
        break;
    case EV_FIRE_MELEE:
        if (!local) return;
        addImpulse(0.10f * s_fire, 0.35f * s_fire, 80);
        break;
    case EV_MELEE_HIT:
        if (!local) return;
        addImpulse(0.60f * s_fire, 0.50f * s_fire, 160);
        break;
    case EV_GRENADE_EXPLODE:
    case EV_ROCKET_EXPLODE:
    case EV_ROCKET_EXPLODE_NOMARKS:
    case EV_FLASHBANG_EXPLODE:
    case EV_CUSTOM_EXPLODE:
    case EV_CUSTOM_EXPLODE_NOMARKS:
    case EV_DUD_EXPLODE:
    case EV_PHYS_EXPLOSION_JOLT: {
        float r = s_explRadius > 1.0f ? s_explRadius : 1.0f;
        if (ev == EV_DUD_EXPLODE || ev == EV_PHYS_EXPLOSION_JOLT) r *= 0.4f;
        float d = distToView((const float *)(cent + CENT_ORIGIN));
        float f = 1.0f - d / r;
        if (f <= 0.0f) return;
        f = f * f;                               // fall off quickly with distance
        k = s_explosion;
        addImpulse(1.0f * f * k, 0.7f * f * k, (DWORD)(250 + 450 * f));
        if (s_debug) rlog("explosion ev %d dist %.0f -> %.2f", ev, d, f);
        break;
    }
    default:
        if (ev >= EV_LANDING_PAIN_FIRST && ev <= EV_LANDING_PAIN_LAST) {
            if (!local) return;
            int dmg = *(int *)(cent + CENT_ES + ES_EVENTPARM) & 0x7f;
            float f = 0.45f + dmg / 100.0f;
            if (f > 1.0f) f = 1.0f;
            addImpulse(f * s_land, 0.3f * f * s_land, 220);
            break;
        }
        return;
    }
    g_lastEv = ev;
    g_lastEvT = nowMs();
    if (s_debug && ev != EV_GRENADE_EXPLODE) rlog("event %d (local %d, flags %d)", ev, local, flags);
}

static void Hook_EntityEvent(int lc, uint8_t *cent, int ev, int flags)
{
    if (!ev) return;
    if (lc == 0 && cent) onEvent(cent, ev, flags);
    g_tramp(lc, cent, ev, flags);
}

static int installEventHook(void)
{
    uint8_t *T = g_base + RVA_ENTITY_EVENT;
    if (memcmp(T, kEventPrologue, sizeof(kEventPrologue)) != 0) {
        rlog("CG_EntityEvent prologue differs (other hook or other game version): fire/explosion rumble off");
        return 0;
    }
    // Trampoline: the two instructions after the zero check, then jump back to T+16.
    uint8_t *tr = (uint8_t *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) return 0;
    uint8_t *p = tr;
    memcpy(p, kEventPrologue + 9, 7); p += 7;
    *p++ = 0xFF; *p++ = 0x25; memset(p, 0, 4); p += 4;
    uint64_t back = (uint64_t)(uintptr_t)(T + 16);
    memcpy(p, &back, 8);
    g_tramp = (EntityEvent_t)tr;

    uint8_t j[16];
    j[0] = 0xFF; j[1] = 0x25; memset(j + 2, 0, 4);
    uint64_t det = (uint64_t)(uintptr_t)Hook_EntityEvent;
    memcpy(j + 6, &det, 8);
    j[14] = 0x90; j[15] = 0x90;
    DWORD old;
    if (!VirtualProtect(T, 16, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(T, j, 16);
    VirtualProtect(T, 16, old, &old);
    FlushInstructionCache(GetCurrentProcess(), T, 16);
    return 1;
}

// ---------------------------------------------------------------- XInput
typedef struct { WORD l, r; } XVib;
typedef struct { DWORD packet; BYTE pad[12]; } XState;
static DWORD (WINAPI *pXSet)(DWORD, XVib *);
static DWORD (WINAPI *pXGet)(DWORD, XState *);

static int loadXInput(void)
{
    const char *names[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    for (int i = 0; i < 3; i++) {
        HMODULE m = LoadLibraryA(names[i]);
        if (!m) continue;
        pXSet = (void *)GetProcAddress(m, "XInputSetState");
        pXGet = (void *)GetProcAddress(m, "XInputGetState");
        if (pXSet && pXGet) {
            rlog("using %s", names[i]);
            return 1;
        }
    }
    return 0;
}

static int g_pad = -1;
static ULONGLONG g_padCheck;

static int findPad(void)
{
    ULONGLONG t = nowMs();
    if (s_padIndex >= 0 && s_padIndex <= 3) return s_padIndex;
    if (g_pad >= 0 && t - g_padCheck < 2000) return g_pad;
    g_padCheck = t;
    XState st;
    for (int i = 0; i < 4; i++)
        if (pXGet(i, &st) == 0) {
            if (g_pad != i) rlog("pad %d connected", i);
            return g_pad = i;
        }
    return g_pad = -1;
}

// ---------------------------------------------------------------- worker
static volatile LONG g_quit;
static WORD g_sentL = 0xffff, g_sentR = 0xffff;
static int g_sentPad = -1;

static void setMotors(int pad, float lo, float hi)
{
    if (lo > 1.0f) lo = 1.0f;
    if (hi > 1.0f) hi = 1.0f;
    if (lo < 0.0f) lo = 0.0f;
    if (hi < 0.0f) hi = 0.0f;
    XVib v = {(WORD)(lo * 65535.0f), (WORD)(hi * 65535.0f)};
    if (pad == g_sentPad && v.l == g_sentL && v.r == g_sentR) return;
    if (g_sentPad >= 0 && g_sentPad != pad) {
        XVib z = {0, 0};
        pXSet(g_sentPad, &z);
    }
    if (pad >= 0) pXSet(pad, &v);
    g_sentPad = pad;
    g_sentL = v.l;
    g_sentR = v.r;
}

static float shakeAmount(int cgTime)
{
    float best = 0.0f;
    for (int i = 0; i < 4; i++) {
        uint8_t *s = g_base + RVA_SHAKES + i * 0x24;
        int start = *(int *)s;
        float scale = *(float *)(s + 4), dur = *(float *)(s + 8), radius = *(float *)(s + 0xc);
        int dt = cgTime - start;
        if (dt < 0 || dur <= 0.0f || (float)dt >= dur || scale <= 0.0f) continue;
        float fall = 1.0f;
        if (radius > 0.0f) {
            fall = 1.0f - distToView((const float *)(s + 0x10)) / radius;
            if (fall <= 0.0f) continue;
        }
        float a = scale * (1.0f - (float)dt / dur) * fall;
        if (a > best) best = a;
    }
    return best;
}

static DWORD WINAPI worker(LPVOID arg)
{
    (void)arg;
    int lastTime = 0, lastDmgEv = 0, first = 1;
    ULONGLONG lastAdvance = 0;
    if (!loadXInput()) {          // not in DllMain: loader lock
        rlog("XInput not available; disabled");
        return 0;
    }
    while (!g_quit) {
        Sleep(8);
        ULONGLONG t = nowMs();
        int cgTime = *(volatile int *)(cg() + CG_TIME);
        if (cgTime != lastTime) {
            lastTime = cgTime;
            lastAdvance = t;
        }
        int running = cgTime > 0 && t - lastAdvance < 150;
        int pad = findPad();
        if (!running || pad < 0 || s_strength <= 0.0f) {
            if (!running) clearImpulses();
            first = 1;
            setMotors(pad, 0.0f, 0.0f);
            continue;
        }

        // damage feedback
        int dmgEv = *(volatile int *)(ps() + PS_DAMAGEEVENT);
        int dmgCount = *(volatile int *)(ps() + PS_DAMAGECOUNT);
        if (first) {
            lastDmgEv = dmgEv;
            first = 0;
        } else if (dmgEv != lastDmgEv) {
            lastDmgEv = dmgEv;
            if (dmgCount > 0) {
                float f = 0.35f + dmgCount / 50.0f;
                if (f > 1.0f) f = 1.0f;
                addImpulse(f * s_damage, 0.6f * f * s_damage, (DWORD)(150 + 200 * f));
                if (s_debug) rlog("damage %d -> %.2f", dmgCount, f);
            }
        }

        float lo = 0.0f, hi = 0.0f;
        EnterCriticalSection(&g_cs);
        for (int i = 0; i < NIMP; i++) {
            Impulse *m = &g_imp[i];
            if (!m->dur || t >= m->start + m->dur) continue;
            float k = 1.0f - (float)(t - m->start) / (float)m->dur;
            k = 0.35f + 0.65f * k;               // keep a solid hit, then fade
            if (m->lo * k > lo) lo = m->lo * k;
            if (m->hi * k > hi) hi = m->hi * k;
        }
        LeaveCriticalSection(&g_cs);

        float sh = shakeAmount(cgTime) * s_shake;
        if (sh > 0.0f) {
            float sl = sh * 2.0f, shh = sh * 1.0f;
            if (sl > lo) lo = sl;
            if (shh > hi) hi = shh;
        }
        setMotors(pad, lo * s_strength, hi * s_strength);
    }
    setMotors(g_sentPad, 0.0f, 0.0f);
    return 0;
}

// ---------------------------------------------------------------- entry
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        GetModuleFileNameA(NULL, g_dir, MAX_PATH);
        char *s = strrchr(g_dir, '\\');
        if (s) s[1] = 0; else g_dir[0] = 0;
        char logp[MAX_PATH];
        snprintf(logp, sizeof(logp), "%sMW3Rumble.log", g_dir);
        DeleteFileA(logp);
        rlog("MW3Rumble 1.0 loaded");

        char exe[MAX_PATH];
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        const char *nm = strrchr(exe, '\\');
        nm = nm ? nm + 1 : exe;
        if (_stricmp(nm, "iw5sp.exe") != 0) {
            rlog("not iw5sp.exe (%s); disabled", nm);
            return TRUE;
        }
        g_base = (uint8_t *)GetModuleHandleA(NULL);
        InitializeCriticalSection(&g_cs);
        loadSettings();
        rlog(installEventHook() ? "event hook installed" : "event hook NOT installed");
        HANDLE th = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (th) CloseHandle(th);
        rlog("strength %.2f, pad %d", s_strength, s_padIndex);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_quit = 1;
        if (pXSet && g_sentPad >= 0) {
            XVib z = {0, 0};
            pXSet(g_sentPad, &z);
        }
    }
    return TRUE;
}
