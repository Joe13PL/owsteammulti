OWSteamNet - Original War multiplayer over Steam (no port forwarding, no Radmin/Hamachi)
=========================================================================================

REQUIREMENTS AND LIMITATIONS
  - Steam version of Original War only. GOG, retail/CD and other versions WILL
    NOT WORK (Steam only lets in accounts that own the game on Steam).
  - Windows only. It WILL NOT WORK on Linux (including Proton/Wine).
  - The Steam client must be running and logged in.

INSTALLATION (every player)
  Copy wsock32.dll and OWSteamNet.ini into the game's main folder
  (where OwarOGL.exe is), e.g.:
  C:\Program Files (x86)\Steam\steamapps\common\Original War\
  Start the game from Steam as usual.

UNINSTALL
  Delete wsock32.dll (and optionally OWSteamNet.ini, OWSteamNet.log) from the
  game folder, or set Enabled=0 in OWSteamNet.ini.

HOW TO PLAY
  Host:    Multiplayer -> create a game as usual (no port forwarding).
           The server is advertised as a Steam lobby automatically.
  Players: Multiplayer -> LAN game list. Servers of Steam friends playing
           Original War and servers from public lobbies appear there
           (10.x.x.x addresses are virtual addresses of Steam players).
  Manual:  type the host's SteamID64 (17 digits), "steam:<SteamID64>" or
           "@FriendName" into the address field.

  All players need OWSteamNet AND the same game version (same patches).
  With different versions the game refuses the join - the reason
  (e.g. "different game/protocol version") is in OWSteamNet.log.
  The first log line shows the mod version - compare it between players.
  Regular LAN and direct IP games keep working.

SETTINGS (OWSteamNet.ini)
  Lobby=public|friends|off  - visibility of a hosted server on Steam
  DiscoverFriends / DiscoverLobbies - where the LAN list gets Steam servers from
  AcceptOnlyWhenHosting=1  - accept incoming Steam connections only while hosting
  FakeNetOctet=10  - do not change (the game treats 10.x as LAN)
  Log=1  - log file OWSteamNet.log (useful for bug reports)
