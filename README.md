# owsteammulti

Multiplayer **Original War** przez Steam: hostowanie bez przekierowywania portów, bez Radmina/Hamachi, plus poprawki najczęstszych przyczyn desynców.

> **Pre-release / wersja testowa.** Mechanizm był testowany automatycznie (testy poniżej), ale nie został jeszcze sprawdzony w pełnym meczu wieloosobowym. Zgłaszajcie wyniki w Issues, dołączając `OWSteamNet.log`.

*English summary: a drop-in `wsock32.dll` proxy for Original War (Steam, OW Support build) that tunnels the game's UDP multiplayer over Steam P2P (NAT traversal + Steam relays) and applies in-memory desync fixes. The game executable is never modified.*

## Instalacja

1. Pobierz paczkę z zakładki **Releases**.
2. Skopiuj `wsock32.dll` i `OWSteamNet.ini` do głównego folderu gry (tam, gdzie `OwarOGL.exe`), np.
   `C:\Program Files (x86)\Steam\steamapps\common\Original War\`
3. Uruchamiaj grę normalnie ze Steama.

Moda muszą mieć **wszyscy gracze**. Deinstalacja: usuń `wsock32.dll` (albo ustaw `Enabled=0` w ini).

## Jak grać

- **Host:** Multiplayer → utwórz grę jak zwykle. Serwer jest automatycznie ogłaszany jako lobby Steam.
- **Gracze:** Multiplayer → lista gier **LAN**. Pojawią się na niej serwery znajomych ze Steam grających w Original War oraz serwery z publicznych lobby. Adresy `10.x.x.x` to wirtualne adresy graczy Steam.
- **Ręcznie:** w polu adresu można wpisać SteamID64 hosta (17 cyfr), `steam:<SteamID64>` albo `@NazwaZnajomego`.

Zwykły LAN i gra przez IP działają bez zmian.

## Jak to działa

### Sieć przez Steam
Gra (Delphi) używa wyłącznie UDP przez `wsock32.dll`: serwer na porcie 27963, klienci na porcie losowym, wyszukiwanie w LAN przez broadcast na 27963, topologia gwiazdy. Pakiety mają do ~674 bajtów, a serwer identyfikuje graczy po adresie nadawcy.

`wsock32.dll` z tego projektu to pośrednik systemowej biblioteki: 66 funkcji przekazuje do `ws2_32`/`mswsock`, a obsługuje sam tylko 9.
- Każdy gracz Steam dostaje deterministyczny wirtualny adres `10.<24 bity account id>`. Gra traktuje 10.x jako LAN, więc serwery trafiają na listę LAN.
- `sendto` na wirtualny adres → `ISteamNetworking::SendP2PPacket`.
- Pakiety ze Steama są wstrzykiwane do gniazda gry przez 127.0.0.1, a `recvfrom` podmienia nadawcę na wirtualny adres. Własna pętla `WSAEventSelect` gry działa bez zmian.
- Broadcasty LAN są przekazywane znajomym w grze i hostom z lobby. Hostowany serwer zakłada lobby Steam.
- Sesje P2P są przyjmowane przez callback `P2PSessionRequest_t`, pompowany przez własne `SteamAPI_RunCallbacks` gry.

### Poprawki desynców (`[SyncFix]`)
Nakładane w pamięci tylko w `OwarOGL_SGUI.exe`. Każde miejsce jest lokalizowane sygnaturą bajtową i weryfikowane; przy niezgodności poprawka jest pomijana.

| Opcja | Problem | Poprawka |
|---|---|---|
| `RngIsolation` | 21 strumieni `Urandom.rand_*` dzieli globalne `System.RandSeed` (wyścig wątków) | każdy strumień ma własne ziarno; wyniki bit w bit identyczne z oryginałem |
| `FpuGuard` | obrażenia liczone na x87 i zaokrąglane; słowo sterujące FPU ustawiane tylko raz | `0x133F` przywracane przed każdym `DoGameTick`, korekty logowane |
| `DrawRngFix` | rysowanie klatek zużywało strumień animacji `rand_mcanim`, który jest w CRC multiplayer | nowe animacje losuje tylko deterministyczny tick |
| `SelectEventMP` | zaznaczenie jednostki odpalało lokalnie zdarzenie skryptu `ActiveUnitChanged` | w multiplayerze zdarzenie nie jest wywoływane |

Gdy desync mimo to wystąpi, zachowajcie od **wszystkich** graczy pliki `debug\*_SyncLog_*.synclog` i `OWSteamNet.log`.

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
| `tests/net_selftest` | wzorzec gniazd gry (serwer 27963 + `WSAEventSelect`, klient, broadcast) w trybie `SelfTest=1` bez Steama: rozwiązywanie SteamID, wstrzykiwanie, podmiana nadawcy, broadcast, przezroczystość zwykłego UDP |
| `tests/steam_api_check` | odczytowe wywołania flat API dołączonego `steam_api.dll` (SDK ~1.32) oraz dispatch obiektu callbacku; wymaga działającego Steama i `steam_appid.txt` = `235320` |
| `tests/syncfix_test` | mapuje prawdziwy `OwarOGL_SGUI.exe` (bez uruchamiania), wykonuje kod gry przed i po poprawkach, sprawdza identyczność RNG, FPU i haki: `syncfix_test.exe "<folder gry>\OwarOGL_SGUI.exe"` |

## Narzędzia RE (`tools/re`)

- `td32.py` – parser informacji debugowych Borland TD32 (FB09) z `OwarOGL_SGUI_DEBUG.exe`: procedury, zmienne, typy, linie.
- `exemap.py`, `port.py` – parser nakładki `EXEMAP` i przenoszenie adresów z builda debug na release (moduł + linia).
- `owdis.py`, `xref.py`, `xrefs.py`, `reach.py` – deasembler z symbolami, odwołania i osiągalność w grafie wywołań.
- `ghidra_scripts/` – nakładanie symboli TD32 w Ghidrze i dekompilacja per moduł.

Repozytorium nie zawiera żadnych plików ani zdekompilowanego kodu gry; narzędzia działają na Twojej własnej instalacji.

## Licencja

MIT – patrz [LICENSE](LICENSE). Original War jest własnością jej twórców; projekt nie jest powiązany z Altar Games, OW Support ani Valve.
