# owsteammulti

**[Polski](#polski) | [English](#english)**

---

## Polski

Multiplayer **Original War** przez Steam: hostowanie gry bez przekierowywania portów i bez Radmina/Hamachi.

### Wymagania i ograniczenia

- **Tylko wersja Original War ze Steam.** Wersje z GOG, z płyty i inne **nie będą działać**. Połączenie odbywa się przez Steam, a Steam wpuszcza tylko konta, które mają Original War w swojej bibliotece Steam. Na innej wersji mod jest po prostu nieaktywny: nic nie psuje, ale nie łączy przez Steam.
- **Tylko Windows.** Na **Linuksie** (także przez Proton/Wine) mod **nie będzie działać**.
- Klient Steam musi być uruchomiony i zalogowany.
- Moda muszą mieć **wszyscy gracze**, przy **tej samej wersji gry** (te same pliki i patche). Przy różnych wersjach gra odrzuci dołączenie, a powód odmowy trafi do `OWSteamNet.log`.

### Instalacja

1. Pobierz paczkę z zakładki **Releases**.
2. Skopiuj `wsock32.dll` i `OWSteamNet.ini` do głównego folderu gry (tam, gdzie `OwarOGL.exe`), np.
   `C:\Program Files (x86)\Steam\steamapps\common\Original War\`
3. Uruchamiaj grę normalnie ze Steama („Graj”).

Deinstalacja: usuń `wsock32.dll` albo ustaw `Enabled=0` w `OWSteamNet.ini`.

### Jak grać

- **Host:** Multiplayer → utwórz grę jak zwykle. Serwer jest automatycznie ogłaszany jako lobby Steam.
- **Gracze:** Multiplayer → lista gier **LAN**. Pojawią się na niej serwery znajomych ze Steam grających w Original War oraz serwery z publicznych lobby. Adresy `10.x.x.x` to wirtualne adresy graczy Steam.
- **Ręcznie:** w polu adresu można wpisać SteamID64 hosta (17 cyfr), `steam:<SteamID64>` albo `@NazwaZnajomego`.

Zwykły LAN i gra przez IP działają bez zmian.

### Ustawienia (`OWSteamNet.ini`)

| Opcja | Znaczenie |
|---|---|
| `Enabled` | `1` = gra przez Steam włączona, `0` = mod całkowicie nieaktywny |
| `Lobby` | `public` / `friends` / `off`: jak hostowany serwer jest widoczny na Steam |
| `DiscoverFriends` | serwery znajomych grających w OW na liście LAN |
| `DiscoverLobbies` | serwery z publicznych lobby Steam na liście LAN |
| `AcceptOnlyWhenHosting` | przychodzące połączenia Steam przyjmowane tylko podczas hostowania |
| `FakeNetOctet` | pierwszy oktet wirtualnych adresów; zostaw `10`, bo gra traktuje 10.x jako LAN |
| `Log` | dziennik w `OWSteamNet.log`; pierwsza linia podaje wersję moda |

Zgłaszając problem w Issues, dołącz `OWSteamNet.log`. Log zawiera Twój SteamID, więc możesz go zamazać.

### Jak to działa

Gra używa wyłącznie UDP przez `wsock32.dll`:
- serwer działa na porcie 27963, klienci na porcie losowym;
- wyszukiwanie w LAN to broadcast na 27963.

`wsock32.dll` z tego projektu to pośrednik systemowej biblioteki: większość funkcji przekazuje do systemu, a obsługuje sam tylko ruch gry.
- Każdy gracz Steam dostaje stały wirtualny adres `10.x.x.x`.
- Pakiety wysyłane na taki adres idą przez Steam P2P (przebijanie NAT, a gdy się nie da – przekaźniki Steama).
- Pakiety ze Steama są wstrzykiwane do gniazda gry przez 127.0.0.1 z wirtualnym adresem nadawcy.
- Plik wykonywalny gry nie jest modyfikowany.

### Budowanie

Wymagany Visual Studio / Build Tools z narzędziami C++ **x86**:

```bat
build.bat
```

albo z Git Bash (ścieżki ustawia się zmiennymi `VC`, `SDK`, `SDKV`):

```bash
sh build.sh
```

Wynik: `dist\wsock32.dll`.

### Testy

| Test | Co sprawdza |
|---|---|
| `tests/net_selftest` | wzorzec gniazd gry w trybie `SelfTest=1` bez Steama: wirtualne adresy, wstrzykiwanie pakietów, broadcast, logowanie odmów dołączenia, przezroczystość zwykłego UDP |
| `tests/steam_api_check` | wywołania Steam API z dołączonego `steam_api.dll` (tylko odczyt); wymaga działającego Steama i `steam_appid.txt` = `235320` |

---

## English

**Original War** multiplayer over Steam: host games without port forwarding and without Radmin/Hamachi.

### Requirements and limitations

- **Steam version of Original War only.** GOG, retail/CD and other versions **will not work**. The connection goes through Steam, and Steam only lets in accounts that own Original War on Steam. On other versions the mod simply stays inactive: it breaks nothing, but it does not connect through Steam.
- **Windows only.** It **will not work on Linux** (including Proton/Wine).
- The Steam client must be running and logged in.
- **All players** need the mod and **the same game version** (same files and patches). With different versions the game refuses the join, and the reason is written to `OWSteamNet.log`.

### Installation

1. Download the package from **Releases**.
2. Copy `wsock32.dll` and `OWSteamNet.ini` into the game's main folder (next to `OwarOGL.exe`), e.g.
   `C:\Program Files (x86)\Steam\steamapps\common\Original War\`
3. Start the game from Steam as usual ("Play").

Uninstall: delete `wsock32.dll`, or set `Enabled=0` in `OWSteamNet.ini`.

### How to play

- **Host:** Multiplayer → create a game as usual. The server is advertised as a Steam lobby automatically.
- **Players:** Multiplayer → **LAN** game list. Servers of Steam friends playing Original War and servers from public lobbies show up there. `10.x.x.x` addresses are the virtual addresses of Steam players.
- **Manually:** in the address field you can type the host's SteamID64 (17 digits), `steam:<SteamID64>` or `@FriendName`.

Regular LAN and direct IP games keep working as before.

### Settings (`OWSteamNet.ini`)

| Option | Meaning |
|---|---|
| `Enabled` | `1` = play over Steam, `0` = mod fully inactive |
| `Lobby` | `public` / `friends` / `off`: how a hosted server is visible on Steam |
| `DiscoverFriends` | servers of friends playing OW appear in the LAN list |
| `DiscoverLobbies` | servers from public Steam lobbies appear in the LAN list |
| `AcceptOnlyWhenHosting` | accept incoming Steam connections only while hosting |
| `FakeNetOctet` | first octet of virtual addresses; keep `10`, the game treats 10.x as LAN |
| `Log` | log file `OWSteamNet.log`; the first line shows the mod version |

When reporting a problem in Issues, attach `OWSteamNet.log`. It contains your SteamID, so you may want to blank it out.

### How it works

The game uses only UDP through `wsock32.dll`:
- the server runs on port 27963, clients use a random port;
- LAN discovery is a broadcast to port 27963.

This project's `wsock32.dll` is a proxy for the system library: most functions are forwarded to the system, and only the game's traffic is handled.
- Every Steam player gets a stable virtual `10.x.x.x` address.
- Packets sent to such an address go over Steam P2P (NAT traversal, or Steam relays when that fails).
- Packets arriving from Steam are injected into the game's socket via 127.0.0.1 with the sender's virtual address.
- The game executable is not modified.

### Building

Requires Visual Studio / Build Tools with the **x86** C++ toolset:

```bat
build.bat
```

or from Git Bash (set paths with the `VC`, `SDK`, `SDKV` variables):

```bash
sh build.sh
```

Output: `dist\wsock32.dll`.

### Tests

| Test | What it checks |
|---|---|
| `tests/net_selftest` | the game's socket pattern in `SelfTest=1` mode without Steam: virtual addresses, packet injection, broadcast, join-refusal logging, plain UDP passthrough |
| `tests/steam_api_check` | read-only Steam API calls through the bundled `steam_api.dll`; needs Steam running and `steam_appid.txt` = `235320` |

---

## Licencja / License

MIT – [LICENSE](LICENSE). Original War należy do jej twórców; projekt nie jest powiązany z Altar Games, OW Support ani Valve. / Original War belongs to its creators; this project is not affiliated with Altar Games, OW Support or Valve.
