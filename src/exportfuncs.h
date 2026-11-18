#pragma once
#include <metahook.h>

void HUD_Init(void);
int HUD_VidInit(void);
int HUD_Redraw(float time, int intermission);
void HUD_PlayerMove(struct playermove_s *ppmove, int server);

extern cl_exportfuncs_t gExportfuncs;
extern float g_flRawVelocity;
extern float g_flFilteredVelocity;

extern int g_iScreenWidth;
extern int g_iScreenHeight;
