OWSteamNet - multiplayer Original War przez Steam (bez otwierania portów, bez Radmina/Hamachi)
=====================================================================================

INSTALACJA (każdy gracz)
  Skopiuj wsock32.dll i OWSteamNet.ini do głównego katalogu gry
  (tam, gdzie jest OwarOGL.exe), np.:
  C:\Program Files (x86)\Steam\steamapps\common\Original War\
  Grę uruchamiaj normalnie ze Steama.

DEINSTALACJA
  Usuń wsock32.dll (i opcjonalnie OWSteamNet.ini, OWSteamNet.log) z katalogu gry.
  Albo ustaw Enabled=0 w OWSteamNet.ini.

JAK GRAĆ
  Host:   Multiplayer -> utwórz grę jak zwykle (bez przekierowania portów).
          Serwer jest automatycznie ogłaszany jako lobby Steam.
  Gracze: Multiplayer -> lista gier LAN. Serwery znajomych ze Steam, którzy grają
          w Original War, oraz serwery z publicznych lobby pojawią się na liście
          (adresy 10.x.x.x to wirtualne adresy graczy Steam). Dołącz jak w LAN.
  Ręcznie: w polu adresu można wpisać SteamID64 hosta (17 cyfr, np. 7656119...),
          "steam:<SteamID64>" albo "@NazwaZnajomego".

  Wszyscy gracze muszą mieć zainstalowany OWSteamNet. Zwykły LAN i gra przez
  IP działają dalej bez zmian.

USTAWIENIA (OWSteamNet.ini)
  Lobby=public|friends|off  - widoczność hostowanego serwera na Steam
  DiscoverFriends / DiscoverLobbies - skąd lista LAN bierze serwery Steam
  AcceptOnlyWhenHosting=1  - przychodzące połączenia Steam tylko gdy hostujesz
  FakeNetOctet=10  - nie zmieniaj (gra traktuje 10.x jako LAN)
  Log=1  - dziennik w OWSteamNet.log (przydatny przy zgłaszaniu problemów)

JAK TO DZIAŁA
  wsock32.dll to pośrednik systemowej biblioteki sieciowej. Ruch UDP gry
  kierowany do graczy Steam idzie przez Steam P2P (przebijanie NAT, a gdy się
  nie da - przekaźniki Steama). Plik wykonywalny gry nie jest modyfikowany.

POPRAWKI DESYNCÓW (sekcja [SyncFix] w OWSteamNet.ini)
  Działają tylko w OwarOGL_SGUI.exe, nakładane w pamięci przy starcie gry
  (plik exe nie jest zmieniany). Najlepiej, żeby mieli je wszyscy gracze.
  RngIsolation  - strumienie losowe gry nie dzielą już jednej globalnej zmiennej
                  (wyścig między wątkami psuł losowanie symulacji). Wyniki są
                  identyczne jak w oryginale, więc działa też z graczami bez moda.
  FpuGuard      - przed każdym tickiem przywracana jest precyzja obliczeń FPU,
                  od której zależą m.in. obrażenia. Każda korekta trafia do logu.
  DrawRngFix    - rysowanie klatek nie zużywa już losowania sprawdzanego w CRC
                  (wynik nie zależy od FPS).
  SelectEventMP - zaznaczenie jednostki nie odpala w multiplayerze lokalnego
                  zdarzenia skryptu ActiveUnitChanged.
  Każdą poprawkę można wyłączyć wpisując 0.

  Gdy mimo to wystąpi desync: zachowaj pliki *_SyncLog_* z folderu debug\
  OD WSZYSTKICH graczy oraz OWSteamNet.log - porównanie pokaże przyczynę.
