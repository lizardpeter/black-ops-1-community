// Complete movie requests immediately during local bring-up.
#include <gfx_d3d/r_cinematic.h>

void R_CinematicInitSound(const _GUID *) {}
void R_Cinematic_BeginLostDevice() {}
void R_Cinematic_EndLostDevice() {}
void R_Cinematic_Init() {}
void R_Cinematic_Shutdown() {}
void R_Cinematic_StartPlayback_Internal(const char *, unsigned int, int) {}
void R_Cinematic_StartNextPlayback() {}
void R_Cinematic_StopPlayback() {}
void R_Cinematic_UpdateFrame(bool) {}
void R_Cinematic_ForceRelinquishIO() {}
bool R_Cinematic_IsFinished() { return true; }
bool R_Cinematic_IsStarted() { return false; }
bool R_Cinematic_IsNextReady() { return false; }
char R_Cinematic_IsUnderrun() { return false; }

char R_Cinematic_GetFilenameAndTimeInMsec(char *name, int size, unsigned int *time)
{
    if (name && size > 0)
        name[0] = '\0';
    if (time)
        *time = 0;
    return false;
}
