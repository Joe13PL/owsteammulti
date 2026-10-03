// Mimics Original War's PSock usage against the proxy wsock32.dll (SelfTest mode).
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <stdio.h>
#pragma comment(lib, "wsock32.lib")   // imports resolve to ./wsock32.dll (the proxy)
#pragma comment(lib, "ws2_32.lib")    // WSAEventSelect, as the game loads it from ws2_32
static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok: %s\n", msg); } while (0)
static SOCKET mk(unsigned short port, WSAEVENT *ev) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    BOOL one = 1; setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&one, 4);
    sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_port = htons(port);
    if (bind(s, (sockaddr*)&a, sizeof a)) printf("bind err %d\n", WSAGetLastError());
    *ev = WSACreateEvent(); WSAEventSelect(s, *ev, FD_READ);
    return s;
}
static int rx(SOCKET s, WSAEVENT ev, char *buf, sockaddr_in *from) {
    if (WaitForSingleObject(ev, 2000) != WAIT_OBJECT_0) return -1;
    ResetEvent(ev);
    int fl = sizeof *from; int n = recvfrom(s, buf, 2000, 0, (sockaddr*)from, &fl);
    if (n >= 0) buf[n] = 0;
    return n;
}
int main() {
    WSADATA wd; WSAStartup(0x202, &wd);
    WSAEVENT es, ec;
    SOCKET srv = mk(37963, &es), cli = mk(0, &ec);
    sockaddr_in ca; int cl = sizeof ca; getsockname(cli, (sockaddr*)&ca, &cl);
    Sleep(300);  // let the worker start
    hostent *he = gethostbyname("76561198000000777");
    CHECK(he != NULL, "gethostbyname(SteamID64) resolves");
    in_addr peer = *(in_addr*)he->h_addr_list[0];
    printf("   peer virtual ip %s\n", inet_ntoa(peer));
    CHECK((ntohl(peer.s_addr) >> 24) == 10, "virtual ip in 10.0.0.0/8");
    hostent *real = gethostbyname("localhost");
    CHECK(real && ((in_addr*)real->h_addr_list[0])->s_addr == htonl(INADDR_LOOPBACK), "normal names still resolve");

    // client -> "remote server" over Steam (loops back to our server socket)
    sockaddr_in to = {}; to.sin_family = AF_INET; to.sin_addr = peer; to.sin_port = htons(37963);
    int n = sendto(cli, "hello", 5, 0, (sockaddr*)&to, sizeof to);
    CHECK(n == 5, "sendto virtual ip reports full length");
    char buf[2048]; sockaddr_in from;
    n = rx(srv, es, buf, &from);
    CHECK(n == 5 && !strcmp(buf, "hello"), "server got payload via injection (event fired)");
    CHECK(from.sin_addr.s_addr == peer.s_addr && from.sin_port == ca.sin_port, "server sees sender as virtual ip:client port");

    sockaddr_in clientAsSeen = from;
    // server replies to the address it saw
    n = sendto(srv, "welcome", 7, 0, (sockaddr*)&from, sizeof from);
    n = rx(cli, ec, buf, &from);
    CHECK(n == 7 && !strcmp(buf, "welcome"), "client got reply");
    CHECK(from.sin_addr.s_addr == peer.s_addr && ntohs(from.sin_port) == 37963, "client sees reply from virtual ip:37963");

    // join refusal is logged on both sides (server reply: type 5, reason 6 + mod name)
    unsigned char rej[13] = {0, 0, 0, 0, 0, 0, 0, 5, 6, 3, 'a', 'b', 'c'};
    sendto(srv, (char *)rej, sizeof rej, 0, (sockaddr *)&clientAsSeen, sizeof clientAsSeen);
    rx(cli, ec, buf, &from);
    Sleep(50);
    {
        FILE *lf = fopen("OWSteamNet.log", "r");
        char line[512]; int seenIn = 0, seenOut = 0;
        while (lf && fgets(line, sizeof line, lf)) {
            if (strstr(line, "refused join: different mod (code 6), server mod: abc")) seenIn = 1;
            if (strstr(line, "join refused for") && strstr(line, "different mod")) seenOut = 1;
        }
        if (lf) fclose(lf);
        CHECK(seenIn && seenOut, "join refusal reason logged on client and server");
    }

    // LAN discovery broadcast is forwarded over Steam to the server
    sockaddr_in bc = {}; bc.sin_family = AF_INET; bc.sin_addr.s_addr = INADDR_BROADCAST; bc.sin_port = htons(37963);
    sendto(cli, "query", 5, 0, (sockaddr*)&bc, sizeof bc);
    int got = 0;
    for (int i = 0; i < 3; i++) {  // real LAN broadcast copy may also arrive
        if (rx(srv, es, buf, &from) == 5 && !strcmp(buf, "query") && from.sin_addr.s_addr == peer.s_addr) got = 1;
    }
    CHECK(got, "broadcast forwarded over Steam reaches server");

    // ordinary LAN traffic untouched
    sockaddr_in lo = {}; lo.sin_family = AF_INET; lo.sin_addr.s_addr = htonl(INADDR_LOOPBACK); lo.sin_port = htons(37963);
    sendto(cli, "plain", 5, 0, (sockaddr*)&lo, sizeof lo);
    do { n = rx(srv, es, buf, &from); } while (n == 5 && strcmp(buf, "plain"));
    CHECK(n == 5 && from.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "plain UDP passes through unchanged");
    printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails;
}
