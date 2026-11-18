#pragma once
#include <metahook.h>

#define VELOHUD_VERSION "1.3.0"

void HUD_Init(void);
int HUD_VidInit(void);
int HUD_Redraw(float time, int intermission);
void HUD_PlayerMove(struct playermove_s* ppmove, int server);

// Drops the cached VGUI2 font handle; called when the VGUI2Extension module
// unloads and its font manager releases the fonts behind that handle.
void VeloHUD_ResetFont(void);

extern cl_exportfuncs_t gExportfuncs;
extern float g_flRawVelocity;
extern float g_flFilteredVelocity;
