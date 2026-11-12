// Compatibility entry points for the engine's local-only platform backend.
#include <live/live_steam.h>
#include <live/live_friends_pc.h>
#include <win32/win_steam.h>
#include <universal/dvar.h>

namespace
{
constexpr unsigned long long localProfileId = 1;

void ClearName(char *buffer, int size)
{
    if (buffer && size > 0)
        buffer[0] = '\0';
}
}

bool g_steamInitialized = false;
const dvar_t *livesteam_sv_vac = nullptr;

void Steam_Init() {}
void Steam_Shutdown() {}
void Steam_CancelClientTicket() {}
void Steam_CheckClients() {}
void Steam_OnClientDropped(uint64_t) {}
void Steam_SV_AddTestCommands() {}
uint64_t Steam_GetClientSteamID64() { return 0; }
bool Steam_UpdateClientAuthTicket(netadr_t) { return false; }
bool Steam_CheckClientTicket(const void *, uint32, uint64_t) { return false; }

bool Steam_GetRawClientTicket(unsigned char **buffer, uint32 *size)
{
    if (buffer)
        *buffer = nullptr;
    if (size)
        *size = 0;
    return false;
}

void LiveSteam_Init()
{
    livesteam_sv_vac = _Dvar_RegisterBool("sv_vac", false, 0x40u, "VAC unavailable in local play");
}

void LiveSteam_CheckAccess() {}
void LiveSteam_Shutdown() {}
void LiveSteam_Frame() {}
void LiveSteam_RunCallbacks() {}
bool LiveSteam_IsInitialized() { return false; }
char LiveSteam_IsClientSignedInLocally() { return true; }
bool LiveSteam_IsClientSignedInOnline() { return false; }
unsigned __int64 LiveSteam_GetClientIDAsXUID() { return localProfileId; }
unsigned __int64 LiveSteam_GetUid() { return localProfileId; }

const char *LiveSteam_GetCountryCode()
{
    return Dvar_GetString("cl_forceloc");
}

const char *LiveSteam_GetClientPersonaName(bool) { return "Player"; }
bool LiveSteam_LaunchOtherApp(const char *) { return false; }
void LiveSteam_ShowStore() {}
void LiveSteam_PopOverlayForSteamID(unsigned __int64) {}
void LiveSteam_SendP2PMessage(unsigned __int64, unsigned __int8 *, unsigned int) {}
int LiveSteam_Client_ConnectToSteamServer(unsigned __int64, void *, unsigned int) { return 0; }
void LiveSteam_Client_SteamDisconnect() {}
void LiveSteam_GiveAchievement(const char *) {}

void LiveSteam_Server_Init() {}
void LiveSteam_Server_Shutdown() {}
void LiveSteam_Server_Frame() {}
void LiveSteam_Server_RunCallbacks() {}
void LiveSteam_Server_ClientSteamAuthentication(unsigned __int64, netadr_t, void *, unsigned int) {}
void LiveSteam_Server_ClientSteamDisconnect(unsigned __int64) {}

int LiveSteam_GetNumFriends() { return 0; }
int LiveSteam_GetNumFriendsOnTheServer() { return 0; }
unsigned __int64 LiveSteam_GetFriendXuid(int) { return 0; }
void LiveSteam_GetFriendPersonaName(int, char *buffer, int size) { ClearName(buffer, size); }
void LiveSteam_GetFriendOnTheServerPersonaName(int, char *buffer, int size) { ClearName(buffer, size); }
int LiveSteam_GetFriendPersonaState(unsigned __int64) { return LIVE_STEAM_PERSONA_STATE_OFFLINE; }
int LiveSteam_GetFriendPersonaStateByIndex(int) { return LIVE_STEAM_PERSONA_STATE_OFFLINE; }
char Friends_Update() { return 0; }
int Friends_GetCount(int, int) { return 0; }
char Friends_GetByIndex(int, int, int, FriendInfo *) { return 0; }
char Friends_GetByID(int, unsigned __int64, FriendInfo *) { return 0; }
bool Friend_GetByName(int, const char *, unsigned __int64 *) { return false; }
void Friends_GetPeopleOnServer() {}
void Friend_PrintNamesOnServer_f() {}
void Friends_Init() {}
void Live_GetRecentPlayerName(unsigned __int64, char *buffer, int size) { ClearName(buffer, size); }
