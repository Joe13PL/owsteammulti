// Read-only check of the flat Steam API signatures used by OWSteamNet against the
// game's own steam_api.dll, with SteamAPI_RunCallbacks pumped on another thread
// (as the game does) while results are polled.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
typedef uint64_t u64;
#define F(ret, name, ...) typedef ret(__cdecl *name##_t)(__VA_ARGS__); static name##_t name;
F(bool, SteamAPI_Init)
F(void, SteamAPI_RunCallbacks)
F(void, SteamAPI_Shutdown)
F(void*, SteamUser) F(void*, SteamFriends) F(void*, SteamMatchmaking) F(void*, SteamUtils) F(void*, SteamNetworking)
F(u64, SteamAPI_ISteamUser_GetSteamID, void*)
F(uint32_t, SteamAPI_ISteamUtils_GetAppID, void*)
F(int, SteamAPI_ISteamFriends_GetFriendCount, void*, int)
F(u64, SteamAPI_ISteamFriends_GetFriendByIndex, void*, int, int)
F(bool, SteamAPI_ISteamFriends_GetFriendGamePlayed, void*, u64, void*)
F(const char*, SteamAPI_ISteamFriends_GetPersonaName, void*)
F(const char*, SteamAPI_ISteamFriends_GetFriendPersonaName, void*, u64)
F(u64, SteamAPI_ISteamMatchmaking_RequestLobbyList, void*)
F(void, SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter, void*, int)
F(u64, SteamAPI_ISteamMatchmaking_GetLobbyByIndex, void*, int)
F(const char*, SteamAPI_ISteamMatchmaking_GetLobbyData, void*, u64, const char*)
F(bool, SteamAPI_ISteamUtils_IsAPICallCompleted, void*, u64, bool*)
F(bool, SteamAPI_ISteamUtils_GetAPICallResult, void*, u64, void*, int, int, bool*)
F(bool, SteamAPI_ISteamNetworking_IsP2PPacketAvailable, void*, uint32_t*, int)
F(bool, SteamAPI_ISteamNetworking_AllowP2PPacketRelay, void*, bool)
#pragma pack(push, 8)
struct FriendGameInfo_t { u64 m_gameID; uint32_t ip; uint16_t gp, qp; u64 lobby; };
#pragma pack(pop)
class CCallbackBase {
public:
    CCallbackBase() : m_nCallbackFlags(0), m_iCallback(0) {}
    virtual void Run(void *pvParam) = 0;
    virtual void Run(void *pvParam, bool bIOFailure, u64 hSteamAPICall) = 0;
    virtual int GetCallbackSizeBytes() = 0;
    uint8_t m_nCallbackFlags;
    int m_iCallback;
};
static volatile int cbHits; static volatile u64 cbCall;
class CallDoneCb : public CCallbackBase {
public:
    void Run(void *p) override { cbHits++; cbCall = *(u64*)p; }
    void Run(void *p, bool, u64) override { Run(p); }
    int GetCallbackSizeBytes() override { return 16; }
} g_cb;
typedef void(__cdecl *Reg_t)(CCallbackBase*, int);
static volatile bool stop;
static DWORD WINAPI Pump(LPVOID) { while (!stop) { SteamAPI_RunCallbacks(); Sleep(16); } return 0; }
int main() {
    HMODULE h = LoadLibraryA("steam_api.dll");
    if (!h) { printf("no steam_api.dll\n"); return 1; }
#define L(name) name = (name##_t)GetProcAddress(h, #name); if (!name) { printf("missing %s\n", #name); return 1; }
    L(SteamAPI_Init) L(SteamAPI_RunCallbacks) L(SteamAPI_Shutdown) L(SteamUser) L(SteamFriends) L(SteamMatchmaking) L(SteamUtils) L(SteamNetworking)
    L(SteamAPI_ISteamUser_GetSteamID) L(SteamAPI_ISteamUtils_GetAppID) L(SteamAPI_ISteamFriends_GetFriendCount) L(SteamAPI_ISteamFriends_GetFriendByIndex)
    L(SteamAPI_ISteamFriends_GetFriendGamePlayed) L(SteamAPI_ISteamFriends_GetPersonaName) L(SteamAPI_ISteamFriends_GetFriendPersonaName)
    L(SteamAPI_ISteamMatchmaking_RequestLobbyList) L(SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter) L(SteamAPI_ISteamMatchmaking_GetLobbyByIndex)
    L(SteamAPI_ISteamMatchmaking_GetLobbyData) L(SteamAPI_ISteamUtils_IsAPICallCompleted) L(SteamAPI_ISteamUtils_GetAPICallResult)
    L(SteamAPI_ISteamNetworking_IsP2PPacketAvailable) L(SteamAPI_ISteamNetworking_AllowP2PPacketRelay)
    Reg_t reg = (Reg_t)GetProcAddress(h, "SteamAPI_RegisterCallback");
    g_cb.m_iCallback = 703; reg(&g_cb, 703);   // registered before Init, as OWSteamNet does
    if (!SteamAPI_Init()) { printf("SteamAPI_Init failed\n"); return 1; }
    HANDLE th = CreateThread(0, 0, Pump, 0, 0, 0);
    u64 me = SteamAPI_ISteamUser_GetSteamID(SteamUser());
    printf("steamid ok: %d (%s)\n", (me >> 52) == 0x011, (me>>32)==0x01100001 ? "individual/public" : "?");
    printf("appid: %u\n", SteamAPI_ISteamUtils_GetAppID(SteamUtils()));
    printf("persona name readable: %d\n", SteamAPI_ISteamFriends_GetPersonaName(SteamFriends()) != 0);
    int n = SteamAPI_ISteamFriends_GetFriendCount(SteamFriends(), 4), inGame = 0, anyGame = 0;
    for (int i = 0; i < n; i++) {
        u64 f = SteamAPI_ISteamFriends_GetFriendByIndex(SteamFriends(), i, 4);
        FriendGameInfo_t gi = {};
        if (SteamAPI_ISteamFriends_GetFriendGamePlayed(SteamFriends(), f, &gi)) { anyGame++; if ((gi.m_gameID & 0xFFFFFF) == 235320) inGame++; }
        if ((f >> 32) != 0x01100001) printf("  odd friend id at %d\n", i);
    }
    printf("friends: %d, in some game: %d, in Original War: %d\n", n, anyGame, inGame);
    uint32_t sz; printf("P2P relay allowed: %d, packet avail call ok: %d\n",
        SteamAPI_ISteamNetworking_AllowP2PPacketRelay(SteamNetworking(), true),
        (int)!SteamAPI_ISteamNetworking_IsP2PPacketAvailable(SteamNetworking(), &sz, 0));
    SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter(SteamMatchmaking(), 3);
    u64 call = SteamAPI_ISteamMatchmaking_RequestLobbyList(SteamMatchmaking());
    printf("RequestLobbyList handle nonzero: %d\n", call != 0);
    bool failed = false; int waited = 0;
    while (!SteamAPI_ISteamUtils_IsAPICallCompleted(SteamUtils(), call, &failed) && waited < 10000) { Sleep(50); waited += 50; }
    uint32_t count = 0xFFFFFFFF;
    bool ok = SteamAPI_ISteamUtils_GetAPICallResult(SteamUtils(), call, &count, 4, 510, &failed);
    printf("lobby list polled with callbacks pumping: completed=%d ok=%d failed=%d lobbies=%u (%d ms)\n", waited < 10000, ok, failed, count, waited);
    for (uint32_t i = 0; ok && i < count && i < 5; i++) {
        u64 l = SteamAPI_ISteamMatchmaking_GetLobbyByIndex(SteamMatchmaking(), i);
        const char *d = SteamAPI_ISteamMatchmaking_GetLobbyData(SteamMatchmaking(), l, "owsteamnet");
        printf("  lobby %d: id-type-ok=%d owsteamnet='%s'\n", i, (l >> 52) == 0x018 || (l>>56)==0x01, d ? d : "");
    }
    Sleep(300);
    printf("callback object dispatched: hits=%d, matches our call=%d\n", cbHits, cbCall == call);
    stop = true; WaitForSingleObject(th, 1000);
    SteamAPI_Shutdown();
    return 0;
}
