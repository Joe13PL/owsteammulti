// OWSteamNet - wsock32.dll proxy for Original War.
//
// Tunnels the game's UDP multiplayer traffic over Steam P2P (ISteamNetworking,
// NAT traversal + Steam relay), so hosting needs no port forwarding / VPN.
//
// How it works (no change to the game executable):
//  * Every Steam user gets a deterministic virtual IPv4 address
//    <FakeNetOctet>.<low 24 bits of the Steam account id>, identical on every machine.
//  * sendto() to a virtual address is sent as a Steam P2P packet instead of UDP.
//  * Packets arriving over Steam are re-injected into the game's socket through
//    127.0.0.1 and recvfrom() rewrites the sender to the peer's virtual address,
//    so the game's own WSAEventSelect/recvfrom loop keeps working untouched.
//  * LAN discovery broadcasts (to port 27963) are additionally forwarded to Steam
//    friends playing Original War and to hosts advertised in public Steam lobbies,
//    so their servers show up in the in-game LAN list.
//  * A hosted server (UDP bind on a fixed port) is advertised as a Steam lobby.
//  * gethostbyname("<SteamID64>") / "steam:<id>" / "@<friend name>" resolve to the
//    virtual address, so "Join by address" also works.
//
// Everything else in wsock32 is forwarded to ws2_32 / mswsock / the system wsock32.

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "common.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

// ---------------------------------------------------------------------------
// Export table: identical names/ordinals to the system wsock32.dll.
// ---------------------------------------------------------------------------
#define FWD(name, target, ord) __pragma(comment(linker, "/EXPORT:" #name "=" #target "," "@" #ord))
#define OWN(name, impl, ord) __pragma(comment(linker, "/EXPORT:" #name "=" #impl "," "@" #ord))

FWD(AcceptEx, MSWSOCK.AcceptEx, 1141)
FWD(EnumProtocolsA, MSWSOCK.EnumProtocolsA, 1111)
FWD(EnumProtocolsW, MSWSOCK.EnumProtocolsW, 1112)
FWD(GetAcceptExSockaddrs, MSWSOCK.GetAcceptExSockaddrs, 1142)
FWD(GetAddressByNameA, MSWSOCK.GetAddressByNameA, 1109)
FWD(GetAddressByNameW, MSWSOCK.GetAddressByNameW, 1110)
FWD(GetNameByTypeA, MSWSOCK.GetNameByTypeA, 1115)
FWD(GetNameByTypeW, MSWSOCK.GetNameByTypeW, 1116)
FWD(GetServiceA, MSWSOCK.GetServiceA, 1119)
FWD(GetServiceW, MSWSOCK.GetServiceW, 1120)
FWD(GetTypeByNameA, MSWSOCK.GetTypeByNameA, 1113)
FWD(GetTypeByNameW, MSWSOCK.GetTypeByNameW, 1114)
FWD(MigrateWinsockConfiguration, MSWSOCK.MigrateWinsockConfiguration, 24)
FWD(NPLoadNameSpaces, MSWSOCK.NPLoadNameSpaces, 1130)
FWD(SetServiceA, MSWSOCK.SetServiceA, 1117)
FWD(SetServiceW, MSWSOCK.SetServiceW, 1118)
FWD(TransmitFile, MSWSOCK.TransmitFile, 1140)
FWD(WEP, ws2_32.WEP, 500)
FWD(WSAAsyncGetHostByAddr, ws2_32.WSAAsyncGetHostByAddr, 102)
FWD(WSAAsyncGetHostByName, ws2_32.WSAAsyncGetHostByName, 103)
FWD(WSAAsyncGetProtoByName, ws2_32.WSAAsyncGetProtoByName, 105)
FWD(WSAAsyncGetProtoByNumber, ws2_32.WSAAsyncGetProtoByNumber, 104)
FWD(WSAAsyncGetServByName, ws2_32.WSAAsyncGetServByName, 107)
FWD(WSAAsyncGetServByPort, ws2_32.WSAAsyncGetServByPort, 106)
FWD(WSAAsyncSelect, ws2_32.WSAAsyncSelect, 101)
FWD(WSACancelAsyncRequest, ws2_32.WSACancelAsyncRequest, 108)
FWD(WSACancelBlockingCall, ws2_32.WSACancelBlockingCall, 113)
FWD(WSACleanup, ws2_32.WSACleanup, 116)
FWD(WSAGetLastError, ws2_32.WSAGetLastError, 111)
FWD(WSAIsBlocking, ws2_32.WSAIsBlocking, 114)
FWD(WSARecvEx, MSWSOCK.WSARecvEx, 1107)
FWD(WSASetBlockingHook, ws2_32.WSASetBlockingHook, 109)
FWD(WSASetLastError, ws2_32.WSASetLastError, 112)
FWD(WSAUnhookBlockingHook, ws2_32.WSAUnhookBlockingHook, 110)
FWD(WSApSetPostRoutine, ws2_32.WSApSetPostRoutine, 1000)
FWD(accept, ws2_32.accept, 1)
FWD(connect, ws2_32.connect, 4)
FWD(dn_expand, MSWSOCK.dn_expand, 1106)
FWD(gethostbyaddr, ws2_32.gethostbyaddr, 51)
FWD(gethostname, ws2_32.gethostname, 57)
FWD(getnetbyname, MSWSOCK.getnetbyname, 1101)
FWD(getpeername, ws2_32.getpeername, 5)
FWD(getprotobyname, ws2_32.getprotobyname, 53)
FWD(getprotobynumber, ws2_32.getprotobynumber, 54)
FWD(getservbyname, ws2_32.getservbyname, 55)
FWD(getservbyport, ws2_32.getservbyport, 56)
FWD(getsockname, ws2_32.getsockname, 6)
FWD(htonl, ws2_32.htonl, 8)
FWD(htons, ws2_32.htons, 9)
FWD(inet_addr, ws2_32.inet_addr, 10)
FWD(inet_network, MSWSOCK.inet_network, 1100)
FWD(inet_ntoa, ws2_32.inet_ntoa, 11)
FWD(ioctlsocket, ws2_32.ioctlsocket, 12)
FWD(listen, ws2_32.listen, 13)
FWD(ntohl, ws2_32.ntohl, 14)
FWD(ntohs, ws2_32.ntohs, 15)
FWD(rcmd, MSWSOCK.rcmd, 1102)
FWD(rexec, MSWSOCK.rexec, 1103)
FWD(rresvport, MSWSOCK.rresvport, 1104)
FWD(s_perror, MSWSOCK.s_perror, 1108)
FWD(select, ws2_32.select, 18)
FWD(send, ws2_32.send, 19)
FWD(sethostname, MSWSOCK.sethostname, 1105)
FWD(shutdown, ws2_32.shutdown, 22)
FWD(socket, ws2_32.socket, 23)

OWN(WSAStartup, _hk_WSAStartup@8, 115)
OWN(bind, _hk_bind@12, 2)
OWN(closesocket, _hk_closesocket@4, 3)
OWN(gethostbyname, _hk_gethostbyname@4, 52)
OWN(getsockopt, _hk_getsockopt@20, 7)
OWN(recv, _hk_recv@16, 16)
OWN(recvfrom, _hk_recvfrom@24, 17)
OWN(sendto, _hk_sendto@24, 20)
OWN(setsockopt, _hk_setsockopt@20, 21)

// ---------------------------------------------------------------------------
// Real system wsock32 (for the functions it implements itself, not forwarders)
// ---------------------------------------------------------------------------
typedef int(WSAAPI *getsockopt_t)(SOCKET, int, int, char *, int *);
typedef int(WSAAPI *setsockopt_t)(SOCKET, int, int, const char *, int);
typedef int(WSAAPI *recv_t)(SOCKET, char *, int, int);
typedef int(WSAAPI *recvfrom_t)(SOCKET, char *, int, int, struct sockaddr *, int *);
static getsockopt_t real_getsockopt;
static setsockopt_t real_setsockopt;
static recv_t real_recv;
static recvfrom_t real_recvfrom;

static void LoadRealWsock32() {
    static volatile LONG done;
    if (done) return;
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);  // WOW64 redirects System32 -> SysWOW64 for 32-bit
    strcat(path, "\\wsock32.dll");
    HMODULE h = LoadLibraryA(path);
    if (!h) return;
    real_getsockopt = (getsockopt_t)GetProcAddress(h, "getsockopt");
    real_setsockopt = (setsockopt_t)GetProcAddress(h, "setsockopt");
    real_recv = (recv_t)GetProcAddress(h, "recv");
    real_recvfrom = (recvfrom_t)GetProcAddress(h, "recvfrom");
    done = 1;
}

// ---------------------------------------------------------------------------
// Config + logging
// ---------------------------------------------------------------------------
static char g_dir[MAX_PATH];
static int cfg_enabled = 1;
static int cfg_log = 1;
static int cfg_fakeOctet = 10;
static int cfg_lobbyType = 2;  // 0 off, 1 friends only, 2 public
static int cfg_acceptOnlyHosting = 1;
static int cfg_friends = 1;
static int cfg_lobbies = 1;
static int cfg_selfTest = 0;  // loopback test mode: P2P sends are delivered locally

static CRITICAL_SECTION g_logCs;
static FILE *g_logf;

void Log(const char *fmt, ...) {
    if (!cfg_log) return;
    EnterCriticalSection(&g_logCs);
    if (!g_logf) {
        char p[MAX_PATH];
        sprintf(p, "%sOWSteamNet.log", g_dir);
        g_logf = fopen(p, "a");
    }
    if (g_logf) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(g_logf, "%02d:%02d:%02d.%03d [%5lu] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentThreadId());
        va_list ap;
        va_start(ap, fmt);
        vfprintf(g_logf, fmt, ap);
        va_end(ap);
        fputc('\n', g_logf);
        fflush(g_logf);
    }
    LeaveCriticalSection(&g_logCs);
}

static void LoadConfig() {
    char ini[MAX_PATH];
    sprintf(ini, "%sOWSteamNet.ini", g_dir);
    if (GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES) {
        WritePrivateProfileStringA("OWSteamNet", "Enabled", "1", ini);
        WritePrivateProfileStringA("OWSteamNet", "Lobby", "public", ini);
        WritePrivateProfileStringA("OWSteamNet", "DiscoverFriends", "1", ini);
        WritePrivateProfileStringA("OWSteamNet", "DiscoverLobbies", "1", ini);
        WritePrivateProfileStringA("OWSteamNet", "AcceptOnlyWhenHosting", "1", ini);
        WritePrivateProfileStringA("OWSteamNet", "FakeNetOctet", "10", ini);
        WritePrivateProfileStringA("OWSteamNet", "Log", "1", ini);
    }
    cfg_enabled = GetPrivateProfileIntA("OWSteamNet", "Enabled", 1, ini);
    cfg_log = GetPrivateProfileIntA("OWSteamNet", "Log", 1, ini);
    cfg_fakeOctet = GetPrivateProfileIntA("OWSteamNet", "FakeNetOctet", 10, ini) & 0xFF;
    cfg_friends = GetPrivateProfileIntA("OWSteamNet", "DiscoverFriends", 1, ini);
    cfg_lobbies = GetPrivateProfileIntA("OWSteamNet", "DiscoverLobbies", 1, ini);
    cfg_acceptOnlyHosting = GetPrivateProfileIntA("OWSteamNet", "AcceptOnlyWhenHosting", 1, ini);
    cfg_selfTest = GetPrivateProfileIntA("OWSteamNet", "SelfTest", 0, ini);
    char lt[32];
    GetPrivateProfileStringA("OWSteamNet", "Lobby", "public", lt, sizeof lt, ini);
    cfg_lobbyType = !_stricmp(lt, "off") ? 0 : !_stricmp(lt, "friends") ? 1 : 2;
}

// ---------------------------------------------------------------------------
// Steam flat API (SDK ~1.32: SteamNetworking005, SteamMatchmaking009, SteamFriends015)
// ---------------------------------------------------------------------------
typedef uint64_t SteamAPICall_t;
typedef void *(__cdecl *Accessor_t)();
typedef bool(__cdecl *SendP2P_t)(void *, uint64_t, const void *, uint32_t, int, int);
typedef bool(__cdecl *IsP2PAvail_t)(void *, uint32_t *, int);
typedef bool(__cdecl *ReadP2P_t)(void *, void *, uint32_t, uint32_t *, uint64_t *, int);
typedef bool(__cdecl *AcceptP2P_t)(void *, uint64_t);
typedef bool(__cdecl *AllowRelay_t)(void *, bool);
typedef uint64_t(__cdecl *GetSteamID_t)(void *);
typedef uint32_t(__cdecl *GetAppID_t)(void *);
typedef int(__cdecl *GetFriendCount_t)(void *, int);
typedef uint64_t(__cdecl *GetFriendByIndex_t)(void *, int, int);
typedef bool(__cdecl *GetFriendGamePlayed_t)(void *, uint64_t, void *);
typedef const char *(__cdecl *GetPersonaName_t)(void *);
typedef const char *(__cdecl *GetFriendPersonaName_t)(void *, uint64_t);
typedef SteamAPICall_t(__cdecl *CreateLobby_t)(void *, int, int);
typedef SteamAPICall_t(__cdecl *RequestLobbyList_t)(void *);
typedef void(__cdecl *AddStrFilter_t)(void *, const char *, const char *, int);
typedef void(__cdecl *AddDistFilter_t)(void *, int);
typedef uint64_t(__cdecl *GetLobbyByIndex_t)(void *, int);
typedef const char *(__cdecl *GetLobbyData_t)(void *, uint64_t, const char *);
typedef bool(__cdecl *SetLobbyData_t)(void *, uint64_t, const char *, const char *);
typedef void(__cdecl *LeaveLobby_t)(void *, uint64_t);
typedef bool(__cdecl *IsCallDone_t)(void *, SteamAPICall_t, bool *);
typedef bool(__cdecl *GetCallResult_t)(void *, SteamAPICall_t, void *, int, int, bool *);

class CCallbackBase;
typedef void(__cdecl *RegisterCallback_t)(CCallbackBase *, int);

static struct {
    Accessor_t SteamNetworking, SteamUser, SteamFriends, SteamMatchmaking, SteamUtils;
    SendP2P_t SendP2PPacket;
    IsP2PAvail_t IsP2PPacketAvailable;
    ReadP2P_t ReadP2PPacket;
    AcceptP2P_t AcceptP2PSessionWithUser;
    AllowRelay_t AllowP2PPacketRelay;
    GetSteamID_t GetSteamID;
    GetAppID_t GetAppID;
    GetFriendCount_t GetFriendCount;
    GetFriendByIndex_t GetFriendByIndex;
    GetFriendGamePlayed_t GetFriendGamePlayed;
    GetPersonaName_t GetPersonaName;
    GetFriendPersonaName_t GetFriendPersonaName;
    CreateLobby_t CreateLobby;
    RequestLobbyList_t RequestLobbyList;
    AddStrFilter_t AddRequestLobbyListStringFilter;
    AddDistFilter_t AddRequestLobbyListDistanceFilter;
    GetLobbyByIndex_t GetLobbyByIndex;
    GetLobbyData_t GetLobbyData;
    SetLobbyData_t SetLobbyData;
    LeaveLobby_t LeaveLobby;
    IsCallDone_t IsAPICallCompleted;
    GetCallResult_t GetAPICallResult;
    RegisterCallback_t RegisterCallback;
} S;

static HMODULE g_steamApi;
static volatile bool g_ready;  // Steam initialised and all functions resolved
static void *g_net, *g_friends, *g_mm, *g_utils;
static uint64_t g_myId;
static uint32_t g_appId;

static bool ResolveSteam(HMODULE h) {
#define R(field, name)                                   \
    *(FARPROC *)&S.field = GetProcAddress(h, name);      \
    if (!S.field) { Log("missing steam_api export %s", name); return false; }
    R(SteamNetworking, "SteamNetworking");
    R(SteamUser, "SteamUser");
    R(SteamFriends, "SteamFriends");
    R(SteamMatchmaking, "SteamMatchmaking");
    R(SteamUtils, "SteamUtils");
    R(SendP2PPacket, "SteamAPI_ISteamNetworking_SendP2PPacket");
    R(IsP2PPacketAvailable, "SteamAPI_ISteamNetworking_IsP2PPacketAvailable");
    R(ReadP2PPacket, "SteamAPI_ISteamNetworking_ReadP2PPacket");
    R(AcceptP2PSessionWithUser, "SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser");
    R(AllowP2PPacketRelay, "SteamAPI_ISteamNetworking_AllowP2PPacketRelay");
    R(GetSteamID, "SteamAPI_ISteamUser_GetSteamID");
    R(GetAppID, "SteamAPI_ISteamUtils_GetAppID");
    R(GetFriendCount, "SteamAPI_ISteamFriends_GetFriendCount");
    R(GetFriendByIndex, "SteamAPI_ISteamFriends_GetFriendByIndex");
    R(GetFriendGamePlayed, "SteamAPI_ISteamFriends_GetFriendGamePlayed");
    R(GetPersonaName, "SteamAPI_ISteamFriends_GetPersonaName");
    R(GetFriendPersonaName, "SteamAPI_ISteamFriends_GetFriendPersonaName");
    R(CreateLobby, "SteamAPI_ISteamMatchmaking_CreateLobby");
    R(RequestLobbyList, "SteamAPI_ISteamMatchmaking_RequestLobbyList");
    R(AddRequestLobbyListStringFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListStringFilter");
    R(AddRequestLobbyListDistanceFilter, "SteamAPI_ISteamMatchmaking_AddRequestLobbyListDistanceFilter");
    R(GetLobbyByIndex, "SteamAPI_ISteamMatchmaking_GetLobbyByIndex");
    R(GetLobbyData, "SteamAPI_ISteamMatchmaking_GetLobbyData");
    R(SetLobbyData, "SteamAPI_ISteamMatchmaking_SetLobbyData");
    R(LeaveLobby, "SteamAPI_ISteamMatchmaking_LeaveLobby");
    R(IsAPICallCompleted, "SteamAPI_ISteamUtils_IsAPICallCompleted");
    R(GetAPICallResult, "SteamAPI_ISteamUtils_GetAPICallResult");
    R(RegisterCallback, "SteamAPI_RegisterCallback");
#undef R
    return true;
}

// Steam's callback base class (same declaration as the SDK so MSVC lays out the
// vtable identically, including its reversed order of the overloaded Run()).
class CCallbackBase {
public:
    CCallbackBase() : m_nCallbackFlags(0), m_iCallback(0) {}
    virtual void Run(void *pvParam) = 0;
    virtual void Run(void *pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall) = 0;
    virtual int GetCallbackSizeBytes() = 0;
    uint8_t m_nCallbackFlags;
    int m_iCallback;
};

// ---------------------------------------------------------------------------
// Peer table (Steam id <-> virtual IPv4)
// ---------------------------------------------------------------------------
static CRITICAL_SECTION g_cs;
static std::map<uint64_t, uint32_t> g_idToIp;  // ip in network byte order
static std::map<uint32_t, uint64_t> g_ipToId;
static std::map<uint64_t, DWORD> g_lastHeard;  // peers that sent us data
static std::map<SOCKET, uint16_t> g_sockPort;  // tracked UDP game sockets -> local port (host order)
static std::set<uint64_t> g_lobbyHosts;
static SOCKET g_hostSock = INVALID_SOCKET;
static uint16_t g_hostPort;

static uint32_t VirtualIp(uint64_t id) {
    uint32_t acc = (uint32_t)(id & 0xFFFFFFFFu) & 0xFFFFFF;
    if ((acc & 0xFF) == 0 || (acc & 0xFF) == 0xFF) acc ^= 0x80;  // avoid .0 / .255 host parts
    return htonl(((uint32_t)cfg_fakeOctet << 24) | acc);
}

static uint32_t AddPeer(uint64_t id) {
    uint32_t ip = VirtualIp(id);
    EnterCriticalSection(&g_cs);
    if (!g_idToIp.count(id)) {
        g_idToIp[id] = ip;
        g_ipToId[ip] = id;
        in_addr a;
        a.s_addr = ip;
        LeaveCriticalSection(&g_cs);
        Log("peer %llu -> %s", id, inet_ntoa(a));
        return ip;
    }
    LeaveCriticalSection(&g_cs);
    return ip;
}

static bool LookupIp(uint32_t ip, uint64_t *id) {
    EnterCriticalSection(&g_cs);
    auto it = g_ipToId.find(ip);
    bool ok = it != g_ipToId.end();
    if (ok) *id = it->second;
    LeaveCriticalSection(&g_cs);
    return ok;
}

static bool TrackedPort(SOCKET s, uint16_t *port) {
    EnterCriticalSection(&g_cs);
    auto it = g_sockPort.find(s);
    bool ok = it != g_sockPort.end();
    if (ok && port) *port = it->second;
    LeaveCriticalSection(&g_cs);
    return ok;
}

static bool LocalPortOpen(uint16_t port) {
    EnterCriticalSection(&g_cs);
    bool ok = false;
    for (auto &kv : g_sockPort)
        if (kv.second == port) { ok = true; break; }
    LeaveCriticalSection(&g_cs);
    return ok;
}

// ---------------------------------------------------------------------------
// Wire format
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct WireHdr {          // Steam P2P payload header
    uint16_t magic;       // 'OW'
    uint8_t ver;          // 1
    uint8_t type;         // PKT_*
    uint16_t srcPort;     // sender's local UDP port (host order)
    uint16_t dstPort;     // destination UDP port on receiver (host order)
};
struct InjHdr {           // loopback injection header
    uint32_t magic;       // 'OWSI'
    uint32_t ip;          // virtual IP of the sender (network order)
    uint16_t port;        // sender port (network order)
};
#pragma pack(pop)
enum { WIRE_MAGIC = 0x574F, WIRE_VER = 1, INJ_MAGIC = 0x4953574F };
enum { PKT_DATA = 0, PKT_BCAST = 1, PKT_DIR = 2 };
enum { k_EP2PSendUnreliable = 0, k_EP2PSendReliable = 2 };
enum { MAX_UNRELIABLE = 1200 };

static SOCKET g_injSock = INVALID_SOCKET;
static uint16_t g_injPortNet;

static void HandleWire(uint64_t from, const uint8_t *p, uint32_t n);

static bool SteamSend(uint64_t to, uint8_t type, uint16_t srcPort, uint16_t dstPort, const void *data, uint32_t len) {
    uint8_t buf[4096];
    if (len + sizeof(WireHdr) > sizeof buf) return false;
    WireHdr *h = (WireHdr *)buf;
    h->magic = WIRE_MAGIC;
    h->ver = WIRE_VER;
    h->type = type;
    h->srcPort = srcPort;
    h->dstPort = dstPort;
    memcpy(buf + sizeof(WireHdr), data, len);
    uint32_t total = len + sizeof(WireHdr);
    if (cfg_selfTest) {  // deliver to ourselves as if `to` had sent it
        HandleWire(to, buf, total);
        return true;
    }
    int mode = total <= MAX_UNRELIABLE ? k_EP2PSendUnreliable : k_EP2PSendReliable;
    bool ok = S.SendP2PPacket(g_net, to, buf, total, mode, 0);
    if (!ok) Log("SendP2PPacket to %llu failed (len %u)", to, total);
    return ok;
}

static void Inject(uint32_t fromIp, uint16_t fromPort, uint16_t dstPort, const uint8_t *data, uint32_t len) {
    if (g_injSock == INVALID_SOCKET) return;
    uint8_t buf[4096];
    if (len + sizeof(InjHdr) > sizeof buf) return;
    InjHdr *h = (InjHdr *)buf;
    h->magic = INJ_MAGIC;
    h->ip = fromIp;
    h->port = htons(fromPort);
    memcpy(buf + sizeof(InjHdr), data, len);
    sockaddr_in to = {};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    to.sin_port = htons(dstPort);
    sendto(g_injSock, (const char *)buf, (int)(len + sizeof(InjHdr)), 0, (sockaddr *)&to, sizeof to);  // ws2_32
}

static void HandleWire(uint64_t from, const uint8_t *p, uint32_t n) {
    if (n < sizeof(WireHdr)) return;
    const WireHdr *h = (const WireHdr *)p;
    if (h->magic != WIRE_MAGIC || h->ver != WIRE_VER) return;
    const uint8_t *d = p + sizeof(WireHdr);
    uint32_t dn = n - sizeof(WireHdr);
    uint32_t ip = AddPeer(from);
    EnterCriticalSection(&g_cs);
    g_lastHeard[from] = GetTickCount();
    LeaveCriticalSection(&g_cs);
    switch (h->type) {
    case PKT_DATA:
        if (LocalPortOpen(h->dstPort)) Inject(ip, h->srcPort, h->dstPort, d, dn);
        break;
    case PKT_BCAST:
        // Only a running server answers discovery broadcasts.
        if (g_hostSock != INVALID_SOCKET && h->dstPort == g_hostPort) Inject(ip, h->srcPort, h->dstPort, d, dn);
        break;
    case PKT_DIR:  // host tells us about the other players (needed for host migration)
        for (uint32_t i = 0; i + 8 <= dn; i += 8) {
            uint64_t id;
            memcpy(&id, d + i, 8);
            if (id != g_myId) AddPeer(id);
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// Steam callbacks (dispatched by the game's own SteamAPI_RunCallbacks)
// ---------------------------------------------------------------------------
class P2PSessionRequestCb : public CCallbackBase {
public:
    void Run(void *p) override {
        uint64_t remote;
        memcpy(&remote, p, 8);
        bool hosting = g_hostSock != INVALID_SOCKET;
        bool known;
        EnterCriticalSection(&g_cs);
        known = g_idToIp.count(remote) != 0;
        LeaveCriticalSection(&g_cs);
        if (!cfg_acceptOnlyHosting || hosting || known) {
            void *net = S.SteamNetworking();
            if (net) S.AcceptP2PSessionWithUser(net, remote);
            Log("accepted P2P session from %llu", remote);
        } else {
            Log("ignored P2P session request from %llu (not hosting)", remote);
        }
    }
    void Run(void *p, bool, SteamAPICall_t) override { Run(p); }
    int GetCallbackSizeBytes() override { return 8; }
};

class P2PSessionFailCb : public CCallbackBase {
public:
    void Run(void *p) override {
        uint64_t remote;
        memcpy(&remote, p, 8);
        Log("P2P session with %llu failed, error %u", remote, (unsigned)((uint8_t *)p)[8]);
    }
    void Run(void *p, bool, SteamAPICall_t) override { Run(p); }
    int GetCallbackSizeBytes() override { return 9; }
};

static P2PSessionRequestCb g_cbReq;
static P2PSessionFailCb g_cbFail;
static bool g_cbRegistered;

static void RegisterCallbacksEarly() {
    // Called from the game's first WSAStartup, which runs on the main thread during
    // unit initialisation, before anything pumps Steam callbacks.
    if (g_cbRegistered) return;
    HMODULE h = LoadLibraryA("steam_api.dll");
    if (!h) { Log("steam_api.dll not found"); return; }
    g_steamApi = h;
    *(FARPROC *)&S.RegisterCallback = GetProcAddress(h, "SteamAPI_RegisterCallback");
    if (!S.RegisterCallback) return;
    g_cbReq.m_iCallback = 1202;   // P2PSessionRequest_t
    g_cbFail.m_iCallback = 1203;  // P2PSessionConnectFail_t
    S.RegisterCallback(&g_cbReq, 1202);
    S.RegisterCallback(&g_cbFail, 1203);
    g_cbRegistered = true;
    Log("Steam callbacks registered");
}

// ---------------------------------------------------------------------------
// Lobby advertising / discovery
// ---------------------------------------------------------------------------
static SteamAPICall_t g_createCall, g_listCall;
static uint64_t g_lobby;
static volatile bool g_wantLobby;
static DWORD g_lastListReq;
static DWORD g_lastBroadcast;

#pragma pack(push, 8)
struct LobbyCreated_t { int m_eResult; uint64_t m_ulSteamIDLobby; };
struct LobbyMatchList_t { uint32_t m_nLobbiesMatching; };
struct FriendGameInfo_t { uint64_t m_gameID; uint32_t m_unGameIP; uint16_t m_usGamePort; uint16_t m_usQueryPort; uint64_t m_steamIDLobby; };
#pragma pack(pop)

static void LobbyTick() {
    if (!cfg_lobbyType) return;
    bool failed = false;
    if (g_wantLobby && !g_lobby && !g_createCall) {
        g_createCall = S.CreateLobby(g_mm, cfg_lobbyType == 1 ? 1 : 2, 16);
        Log("CreateLobby requested");
    }
    if (g_createCall && S.IsAPICallCompleted(g_utils, g_createCall, &failed)) {
        LobbyCreated_t r = {};
        if (S.GetAPICallResult(g_utils, g_createCall, &r, sizeof r, 513, &failed) && !failed && r.m_eResult == 1) {
            g_lobby = r.m_ulSteamIDLobby;
            char id[32], port[16];
            sprintf(id, "%llu", g_myId);
            sprintf(port, "%u", g_hostPort);
            S.SetLobbyData(g_mm, g_lobby, "owsteamnet", "1");
            S.SetLobbyData(g_mm, g_lobby, "host", id);
            S.SetLobbyData(g_mm, g_lobby, "port", port);
            S.SetLobbyData(g_mm, g_lobby, "name", S.GetPersonaName(g_friends));
            Log("lobby %llu created", g_lobby);
            if (!g_wantLobby) { S.LeaveLobby(g_mm, g_lobby); g_lobby = 0; }
        } else {
            Log("CreateLobby failed (result %d)", r.m_eResult);
        }
        g_createCall = 0;
    }
    if (!g_wantLobby && g_lobby) {
        S.LeaveLobby(g_mm, g_lobby);
        Log("lobby %llu closed", g_lobby);
        g_lobby = 0;
    }
}

static void DiscoveryTick() {
    if (!cfg_lobbies) return;
    DWORD now = GetTickCount();
    bool browsing = now - g_lastBroadcast < 15000;
    bool failed = false;
    if (browsing && !g_listCall && now - g_lastListReq > 8000) {
        S.AddRequestLobbyListStringFilter(g_mm, "owsteamnet", "1", 0);
        S.AddRequestLobbyListDistanceFilter(g_mm, 3);  // worldwide
        g_listCall = S.RequestLobbyList(g_mm);
        g_lastListReq = now;
    }
    if (g_listCall && S.IsAPICallCompleted(g_utils, g_listCall, &failed)) {
        LobbyMatchList_t r = {};
        std::set<uint64_t> hosts;
        if (S.GetAPICallResult(g_utils, g_listCall, &r, sizeof r, 510, &failed) && !failed) {
            for (uint32_t i = 0; i < r.m_nLobbiesMatching; i++) {
                uint64_t lobby = S.GetLobbyByIndex(g_mm, (int)i);
                const char *h = S.GetLobbyData(g_mm, lobby, "host");
                uint64_t id = h ? _strtoui64(h, nullptr, 10) : 0;
                if (id && id != g_myId) hosts.insert(id);
            }
        }
        EnterCriticalSection(&g_cs);
        if (hosts != g_lobbyHosts) Log("lobby search: %u Original War host(s) on Steam", (unsigned)hosts.size());
        g_lobbyHosts.swap(hosts);
        LeaveCriticalSection(&g_cs);
        g_listCall = 0;
    }
}

// Sends the list of active peers to each active peer (lets clients resolve each
// other's virtual addresses, used by the game's host migration).
static void DirectoryTick() {
    static DWORD last;
    DWORD now = GetTickCount();
    if (g_hostSock == INVALID_SOCKET || now - last < 3000) return;
    last = now;
    std::vector<uint64_t> act;
    EnterCriticalSection(&g_cs);
    for (auto &kv : g_lastHeard)
        if (now - kv.second < 30000) act.push_back(kv.first);
    LeaveCriticalSection(&g_cs);
    if (act.size() < 2) return;
    for (uint64_t to : act) SteamSend(to, PKT_DIR, 0, 0, act.data(), (uint32_t)(act.size() * 8));
}

static std::vector<uint64_t> BroadcastTargets() {
    std::set<uint64_t> t;
    if (cfg_selfTest) return std::vector<uint64_t>(1, 76561198000000777ull);
    if (cfg_friends) {
        int n = S.GetFriendCount(g_friends, 4 /*k_EFriendFlagImmediate*/);
        for (int i = 0; i < n; i++) {
            uint64_t f = S.GetFriendByIndex(g_friends, i, 4);
            FriendGameInfo_t gi = {};
            if (S.GetFriendGamePlayed(g_friends, f, &gi) && (uint32_t)(gi.m_gameID & 0xFFFFFF) == g_appId) t.insert(f);
        }
    }
    EnterCriticalSection(&g_cs);
    t.insert(g_lobbyHosts.begin(), g_lobbyHosts.end());
    LeaveCriticalSection(&g_cs);
    t.erase(g_myId);
    return std::vector<uint64_t>(t.begin(), t.end());
}

// ---------------------------------------------------------------------------
// Worker thread: Steam init, packet pump, lobby maintenance
// ---------------------------------------------------------------------------
static HANDLE g_worker;

static bool TrySteamInit() {
    if (!g_steamApi) g_steamApi = GetModuleHandleA("steam_api.dll");
    if (!g_steamApi) return false;
    static bool resolved;
    if (!resolved) {
        if (!ResolveSteam(g_steamApi)) return false;
        resolved = true;
    }
    void *user = S.SteamUser();
    g_net = S.SteamNetworking();
    g_friends = S.SteamFriends();
    g_mm = S.SteamMatchmaking();
    g_utils = S.SteamUtils();
    if (!user || !g_net || !g_friends || !g_mm || !g_utils) return false;
    g_myId = S.GetSteamID(user);
    if (!g_myId) return false;
    g_appId = S.GetAppID(g_utils);
    S.AllowP2PPacketRelay(g_net, true);
    if (!g_cbRegistered) Log("warning: P2P session callbacks not registered, incoming sessions can't be accepted");
    in_addr a;
    a.s_addr = VirtualIp(g_myId);
    Log("Steam ready: me=%llu app=%u my virtual IP=%s", g_myId, g_appId, inet_ntoa(a));
    return true;
}

static DWORD WINAPI Worker(LPVOID) {
    timeBeginPeriod(1);
    // loopback injection socket
    g_injSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(g_injSock, (sockaddr *)&a, sizeof a);
    int al = sizeof a;
    getsockname(g_injSock, (sockaddr *)&a, &al);
    g_injPortNet = a.sin_port;
    Log("injector on 127.0.0.1:%u", ntohs(g_injPortNet));

    static uint8_t buf[65536];
    for (;;) {
        if (cfg_selfTest) {  // no Steam: pretend to be a user, P2P loops back locally
            if (!g_ready) {
                g_myId = 76561198000000001ull;
                g_ready = true;
                Log("SelfTest mode active");
            }
            Sleep(5);
            continue;
        }
        if (!g_ready) {
            if (TrySteamInit()) g_ready = true;
            else { Sleep(250); continue; }
        }
        for (int ch = 0; ch < 2; ch++) {
            uint32_t sz;
            while (S.IsP2PPacketAvailable(g_net, &sz, ch)) {
                uint64_t from = 0;
                if (!S.ReadP2PPacket(g_net, buf, sizeof buf, &sz, &from, ch)) break;
                HandleWire(from, buf, sz);
            }
        }
        LobbyTick();
        DiscoveryTick();
        DirectoryTick();
        Sleep(1);
    }
}

static void EnsureWorker() {
    if (g_worker || !cfg_enabled) return;
    EnterCriticalSection(&g_cs);
    if (!g_worker) g_worker = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    LeaveCriticalSection(&g_cs);
}

// ---------------------------------------------------------------------------
// Hooked wsock32 exports
// ---------------------------------------------------------------------------
extern "C" int WSAAPI hk_WSAStartup(WORD ver, LPWSADATA data) {
    static LONG once;
    if (cfg_enabled && InterlockedExchange(&once, 1) == 0) RegisterCallbacksEarly();
    return WSAStartup(ver, data);
}

static bool IsUdp(SOCKET s) {
    int type = 0, len = sizeof type;
    return getsockopt(s, SOL_SOCKET, SO_TYPE, (char *)&type, &len) == 0 && type == SOCK_DGRAM;
}

extern "C" int WSAAPI hk_bind(SOCKET s, const struct sockaddr *name, int namelen) {
    int r = bind(s, name, namelen);
    if (r != 0 || !cfg_enabled || !name || name->sa_family != AF_INET || !IsUdp(s)) return r;
    uint16_t requested = ntohs(((const sockaddr_in *)name)->sin_port);
    sockaddr_in a;
    int al = sizeof a;
    if (getsockname(s, (sockaddr *)&a, &al) != 0) return r;
    uint16_t port = ntohs(a.sin_port);
    EnsureWorker();
    EnterCriticalSection(&g_cs);
    g_sockPort[s] = port;
    LeaveCriticalSection(&g_cs);
    if (requested != 0 && g_hostSock == INVALID_SOCKET) {
        g_hostSock = s;
        g_hostPort = port;
        g_wantLobby = true;
        Log("game server socket on UDP %u - advertising on Steam", port);
    } else {
        Log("game client socket on UDP %u", port);
    }
    return r;
}

extern "C" int WSAAPI hk_closesocket(SOCKET s) {
    EnterCriticalSection(&g_cs);
    bool tracked = g_sockPort.erase(s) != 0;
    LeaveCriticalSection(&g_cs);
    if (tracked && s == g_hostSock) {
        g_hostSock = INVALID_SOCKET;
        g_wantLobby = false;
        Log("game server socket closed");
    }
    return closesocket(s);
}

extern "C" int WSAAPI hk_sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen) {
    if (g_ready && to && to->sa_family == AF_INET && len >= 0) {
        const sockaddr_in *a = (const sockaddr_in *)to;
        uint16_t srcPort = 0;
        uint64_t id;
        if (LookupIp(a->sin_addr.s_addr, &id)) {
            TrackedPort(s, &srcPort);
            SteamSend(id, PKT_DATA, srcPort, ntohs(a->sin_port), buf, (uint32_t)len);
            return len;
        }
        uint32_t ip = ntohl(a->sin_addr.s_addr);
        if ((ip == 0xFFFFFFFF || (ip & 0xFF) == 0xFF) && TrackedPort(s, &srcPort)) {
            // The game sends one copy per network interface: forward only one.
            static DWORD lastTick;
            static uint32_t lastHash;
            uint32_t hsh = 2166136261u;
            for (int i = 0; i < len; i++) hsh = (hsh ^ (uint8_t)buf[i]) * 16777619u;
            DWORD now = GetTickCount();
            if (hsh != lastHash || now - lastTick > 250) {
                lastHash = hsh;
                lastTick = now;
                g_lastBroadcast = now;
                for (uint64_t t : BroadcastTargets()) SteamSend(t, PKT_BCAST, srcPort, ntohs(a->sin_port), buf, (uint32_t)len);
            }
        }
    }
    return sendto(s, buf, len, flags, to, tolen);
}

extern "C" int WSAAPI hk_recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen) {
    LoadRealWsock32();
    if (g_injSock == INVALID_SOCKET || !TrackedPort(s, nullptr)) return real_recvfrom(s, buf, len, flags, from, fromlen);
    char tmp[8192];
    sockaddr_in f = {};
    int fl = sizeof f;
    int n = real_recvfrom(s, tmp, sizeof tmp, flags, (sockaddr *)&f, &fl);
    if (n == SOCKET_ERROR) return n;
    const char *payload = tmp;
    if (f.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && f.sin_port == g_injPortNet && n >= (int)sizeof(InjHdr) &&
        ((InjHdr *)tmp)->magic == INJ_MAGIC) {
        InjHdr *h = (InjHdr *)tmp;
        f.sin_addr.s_addr = h->ip;
        f.sin_port = h->port;
        payload += sizeof(InjHdr);
        n -= sizeof(InjHdr);
    }
    if (from && fromlen) {
        int c = *fromlen < (int)sizeof f ? *fromlen : (int)sizeof f;
        memcpy(from, &f, c);
        *fromlen = sizeof f;
    }
    if (n > len) {
        memcpy(buf, payload, len);
        WSASetLastError(WSAEMSGSIZE);
        return SOCKET_ERROR;
    }
    memcpy(buf, payload, n);
    return n;
}

extern "C" int WSAAPI hk_recv(SOCKET s, char *buf, int len, int flags) { LoadRealWsock32(); return real_recv(s, buf, len, flags); }
extern "C" int WSAAPI hk_getsockopt(SOCKET s, int level, int opt, char *val, int *len) { LoadRealWsock32(); return real_getsockopt(s, level, opt, val, len); }
extern "C" int WSAAPI hk_setsockopt(SOCKET s, int level, int opt, const char *val, int len) { LoadRealWsock32(); return real_setsockopt(s, level, opt, val, len); }

// gethostbyname: "<SteamID64>", "steam:<id>", "steam-<id>" or "@<friend name>"
extern "C" struct hostent *WSAAPI hk_gethostbyname(const char *name) {
    if (cfg_enabled && name) {
        uint64_t id = 0;
        const char *p = name;
        if (!_strnicmp(p, "steam:", 6) || !_strnicmp(p, "steam-", 6)) p += 6;
        size_t l = strlen(p);
        if (l == 17 && !strncmp(p, "7656119", 7) && strspn(p, "0123456789") == 17) id = _strtoui64(p, nullptr, 10);
        if (!id && name[0] == '@' && g_ready) {
            int n = S.GetFriendCount(g_friends, 4);
            for (int i = 0; i < n && !id; i++) {
                uint64_t f = S.GetFriendByIndex(g_friends, i, 4);
                const char *pn = S.GetFriendPersonaName(g_friends, f);
                if (pn && !_stricmp(pn, name + 1)) id = f;
            }
        }
        if (id) {
            static __declspec(thread) hostent he;
            static __declspec(thread) in_addr addr;
            static __declspec(thread) char *list[2];
            addr.s_addr = AddPeer(id);
            list[0] = (char *)&addr;
            list[1] = nullptr;
            he.h_name = (char *)name;
            he.h_aliases = nullptr;
            he.h_addrtype = AF_INET;
            he.h_length = 4;
            he.h_addr_list = list;
            EnsureWorker();
            Log("resolved '%s' to Steam user %llu", name, id);
            return &he;
        }
    }
    return gethostbyname(name);
}

// ---------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&g_cs);
        InitializeCriticalSection(&g_logCs);
        GetModuleFileNameA(inst, g_dir, MAX_PATH);
        char *slash = strrchr(g_dir, '\\');
        if (slash) slash[1] = 0;
        LoadConfig();
        Log("OWSteamNet loaded (enabled=%d lobby=%d)", cfg_enabled, cfg_lobbyType);
        char ini[MAX_PATH];
        sprintf(ini, "%sOWSteamNet.ini", g_dir);
        SyncFix_Apply(ini);
    }
    return TRUE;
}
