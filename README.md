# owsteammulti

Multiplayer **Original War** przez Steam: hostowanie bez przekierowywania portów i bez Radmina/Hamachi.

*English summary: a drop-in `wsock32.dll` proxy for Original War (Steam version) that tunnels the game's UDP multiplayer over Steam P2P (NAT traversal + Steam relays). The game executable is never modified.*

## Instalacja

1. Pobierz paczkę z zakładki **Releases**.
2. Skopiuj `wsock32.dll` i `OWSteamNet.ini` do głównego folderu gry (tam, gdzie `OwarOGL.exe`), np.
   `C:\Program Files (x86)\Steam\steamapps\common\Original War\`
3. Uruchamiaj grę normalnie ze Steama.

Moda muszą mieć **wszyscy gracze**, i to przy **tej samej wersji gry** (te same pliki i patche Original War). Przy różnych wersjach gra odrzuci dołączenie, a powód odmowy trafi do `OWSteamNet.log`.

Deinstalacja: usuń `wsock32.dll` albo ustaw `Enabled=0` w ini.

## Jak grać

- **Host:** Multiplayer → utwórz grę jak zwykle. Serwer jest automatycznie ogłaszany jako lobby Steam.
- **Gracze:** Multiplayer → lista gier **LAN**. Pojawią się na niej serwery znajomych ze Steam grających w Original War oraz serwery z publicznych lobby. Adresy `10.x.x.x` to wirtualne adresy graczy Steam.
- **Ręcznie:** w polu adresu można wpisać SteamID64 hosta (17 cyfr), `steam:<SteamID64>` albo `@NazwaZnajomego`.

Zwykły LAN i gra przez IP działają bez zmian.

## Ustawienia (`OWSteamNet.ini`)

| Opcja | Znaczenie |
|---|---|
| `Enabled` | `1` = gra przez Steam włączona, `0` = biblioteka całkowicie bierna |
| `Lobby` | `public` / `friends` / `off`: jak hostowany serwer jest widoczny na Steam |
| `DiscoverFriends` | serwery znajomych grających w OW na liście LAN |
| `DiscoverLobbies` | serwery z publicznych lobby Steam na liście LAN |
| `AcceptOnlyWhenHosting` | przychodzące połączenia Steam przyjmowane tylko podczas hostowania |
| `FakeNetOctet` | pierwszy oktet wirtualnych adresów; zostaw `10`, bo gra traktuje 10.x jako LAN |
| `Log` | dziennik w `OWSteamNet.log`; pierwsza linia podaje wersję moda |

Zgłaszając problem w Issues, dołącz `OWSteamNet.log`. Log zawiera Twój SteamID, więc możesz go zamazać.

## Jak to działa

Gra używa wyłącznie UDP przez `wsock32.dll`:
- serwer działa na porcie 27963, klienci na porcie losowym;
- wyszukiwanie w LAN to broadcast na 27963;
- topologia jest gwiazdą, pakiety mają do ok. 674 bajtów, a serwer rozpoznaje graczy po adresie nadawcy.

`wsock32.dll` z tego projektu to pośrednik systemowej biblioteki: 66 funkcji przekazuje do `ws2_32`/`mswsock`, a obsługuje sam tylko 7.
- Każdy gracz Steam dostaje deterministyczny wirtualny adres `10.<24 bity account id>`. Gra traktuje 10.x jako LAN, więc serwery trafiają na listę LAN.
- `sendto` na wirtualny adres → `ISteamNetworking::SendP2PPacket` (przebijanie NAT, a gdy się nie da – przekaźniki Steama).
- Pakiety ze Steama są wstrzykiwane do gniazda gry przez 127.0.0.1, a `recvfrom` podmienia nadawcę na wirtualny adres. Własna pętla `WSAEventSelect` gry działa bez zmian.
- Broadcasty LAN są przekazywane znajomym w grze i hostom z lobby. Hostowany serwer zakłada lobby Steam.
- Sesje P2P są przyjmowane przez callback `P2PSessionRequest_t`, pompowany przez własne `SteamAPI_RunCallbacks` gry.

## Budowanie

Wymagany Visual Studio / Build Tools z narzędziami C++ **x86**.

```bat
build.bat
```

albo z Git Bash, gdy brak `vcvars32.bat` (ścieżki ustawia się zmiennymi `VC`, `SDK`, `SDKV`):

```bash
sh build.sh
```

Wynik: `dist\wsock32.dll`.

## Testy

| Test | Co sprawdza |
|---|---|
| `tests/net_selftest` | wzorzec gniazd gry (serwer + `WSAEventSelect`, klient, broadcast) w trybie `SelfTest=1` bez Steama: rozwiązywanie SteamID, wstrzykiwanie, podmiana nadawcy, logowanie odmów dołączenia, broadcast, przezroczystość zwykłego UDP |
| `tests/steam_api_check` | odczytowe wywołania flat API dołączonego `steam_api.dll` oraz dispatch obiektu callbacku; wymaga działającego Steama i `steam_appid.txt` = `235320` |

## Licencja

MIT – patrz [LICENSE](LICENSE). Original War jest własnością jej twórców; projekt nie jest powiązany z Altar Games, OW Support ani Valve.
