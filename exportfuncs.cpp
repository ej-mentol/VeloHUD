#include <metahook.h>
#include "plugins.h"
#include "exportfuncs.h"
#include "VelocityFilter.h"
#include <math.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

cl_exportfuncs_t gExportfuncs = { 0 };
CVelocityFilter g_Filter;

float g_flRawVelocity = 0.0f;
float g_flEngineVelocity = 0.0f;
float g_flFilteredVelocity = 0.0f;
float g_flMaxVelocity = 0.0f;

int g_iScreenWidth = 0;
int g_iScreenHeight = 0;

// VGUI2 Resources
vgui::HFont g_hFont = 0;
int g_iLastFontSize = -1;
int g_iCurR = 180, g_iCurG = 180, g_iCurB = 180, g_iCurA = 255;
static char g_szLastColor[64] = { 0 };

// CVars
cvar_t* cl_velocity_enabled = nullptr;
cvar_t* cl_velocity_mode = nullptr;
cvar_t* cl_velocity_source = nullptr;
cvar_t* cl_velocity_x = nullptr; 
cvar_t* cl_velocity_y = nullptr; 
cvar_t* cl_velocity_font_size = nullptr;
cvar_t* cl_velocity_color = nullptr;
cvar_t* cl_velocity_show_max = nullptr; 

void cl_velocity_reset_max_f(void) {
    g_flMaxVelocity = 0;
}

void cl_velocity_help_f(void) {
    gEngfuncs.Con_Printf("\n--- VeloHUD Status & Help ---\n");
    if (cl_velocity_enabled) gEngfuncs.Con_Printf("cl_velocity_enabled : %.0f (1=On, 0=Off)\n", cl_velocity_enabled->value);
    if (cl_velocity_source)  gEngfuncs.Con_Printf("cl_velocity_source  : %.0f (0:Pred, 1:Eng, 2:Hybrid)\n", cl_velocity_source->value);
    if (cl_velocity_mode)    gEngfuncs.Con_Printf("cl_velocity_mode    : %.0f (0:Raw, 1:Filtered)\n", cl_velocity_mode->value);
    if (cl_velocity_x && cl_velocity_y) gEngfuncs.Con_Printf("cl_velocity_pos_x/y : %.2f, %.2f (Range 0.0 - 1.0)\n", cl_velocity_x->value, cl_velocity_y->value);
    if (cl_velocity_font_size) gEngfuncs.Con_Printf("cl_velocity_font_size: %.0f px\n", cl_velocity_font_size->value);
    
    if (cl_velocity_color && cl_velocity_color->string) gEngfuncs.Con_Printf("cl_velocity_color   : \"%s\" (Format: \"R G B A\")\n", cl_velocity_color->string);
    
    gEngfuncs.Con_Printf("Commands: cl_velocity_reset_max, cl_velocity_help\n");
    gEngfuncs.Con_Printf("----------------------------\n");
}

void HUD_Init(void) {
    gExportfuncs.HUD_Init();

    cl_velocity_enabled = gEngfuncs.pfnRegisterVariable("cl_velocity_enabled", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
    cl_velocity_mode = gEngfuncs.pfnRegisterVariable("cl_velocity_mode", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
    cl_velocity_source = gEngfuncs.pfnRegisterVariable("cl_velocity_source", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
    cl_velocity_x = gEngfuncs.pfnRegisterVariable("cl_velocity_pos_x", "0.3", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
    cl_velocity_y = gEngfuncs.pfnRegisterVariable("cl_velocity_pos_y", "0.85", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
    cl_velocity_font_size = gEngfuncs.pfnRegisterVariable("cl_velocity_font_size", "24", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
    // Явно задаем 4 параметра по умолчанию
    cl_velocity_color = gEngfuncs.pfnRegisterVariable("cl_velocity_color", "180 180 180 255", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
    cl_velocity_show_max = gEngfuncs.pfnRegisterVariable("cl_velocity_show_max", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);

    gEngfuncs.pfnAddCommand("cl_velocity_reset_max", cl_velocity_reset_max_f);
    gEngfuncs.pfnAddCommand("cl_velocity_help", cl_velocity_help_f);
}

void UpdateHUDParams() {
    if (!g_pSurface) return;
    
    int size = cl_velocity_font_size ? (int)cl_velocity_font_size->value : 24;
    if (size != g_iLastFontSize || g_hFont == 0) {
        if (g_hFont == 0) g_hFont = g_pSurface->CreateFont();
        g_pSurface->AddGlyphSetToFont(g_hFont, "Tahoma", size, 700, 0, 0, vgui::ISurface2::FONTFLAG_ANTIALIAS | vgui::ISurface2::FONTFLAG_DROPSHADOW);
        g_iLastFontSize = size;
    }

    if (cl_velocity_color && cl_velocity_color->string && strcmp(cl_velocity_color->string, g_szLastColor) != 0) {
        int r = 180, g = 180, b = 180, a = 255;
        int count = sscanf(cl_velocity_color->string, "%d %d %d %d", &r, &g, &b, &a);
        
        if (count >= 1) {
            g_iCurR = r;
            g_iCurG = (count >= 2) ? g : 180;
            g_iCurB = (count >= 3) ? b : 180;
            g_iCurA = (count >= 4) ? a : 255;
        } 
        strncpy(g_szLastColor, cl_velocity_color->string, sizeof(g_szLastColor) - 1);
    }
}

int HUD_VidInit(void) {
    int result = gExportfuncs.HUD_VidInit();
    SCREENINFO si;
    si.iSize = sizeof(si);
    gEngfuncs.pfnGetScreenInfo(&si);
    g_iScreenWidth = si.iWidth;
    g_iScreenHeight = si.iHeight;

    g_flMaxVelocity = 0;
    UpdateHUDParams();
    return result;
}

void HUD_PlayerMove(struct playermove_s* ppmove, int server) {
    gExportfuncs.HUD_PlayerMove(ppmove, server);
    if (ppmove) {
        g_flRawVelocity = sqrtf(ppmove->velocity[0] * ppmove->velocity[0] + ppmove->velocity[1] * ppmove->velocity[1]);
        g_flFilteredVelocity = g_Filter.Update(g_flRawVelocity, ppmove->frametime);
    }
}

int HUD_Redraw(float time, int intermission) {
    int result = gExportfuncs.HUD_Redraw(time, intermission);

    if (intermission || !cl_velocity_enabled || cl_velocity_enabled->value <= 0)
        return result;

    UpdateHUDParams();

    if (!g_pSurface || !g_hFont) return result;

    auto local = gEngfuncs.GetLocalPlayer();
    if (local) {
        g_flEngineVelocity = sqrtf(local->curstate.velocity[0] * local->curstate.velocity[0] + local->curstate.velocity[1] * local->curstate.velocity[1]);
    }

    float flPred = (cl_velocity_mode && cl_velocity_mode->value > 0) ? g_flFilteredVelocity : g_flRawVelocity;

    float flCurrent;
    if (cl_velocity_source) {
        int src = (int)cl_velocity_source->value;
        if (src == 1) flCurrent = g_flEngineVelocity;
        else if (src == 2) flCurrent = (g_flEngineVelocity * 0.5f) + (flPred * 0.5f);
        else flCurrent = flPred;
    } else flCurrent = flPred;

    if (flCurrent > g_flMaxVelocity) g_flMaxVelocity = flCurrent;

    wchar_t wszVelocity[128];
    if (cl_velocity_show_max && cl_velocity_show_max->value > 0) 
        swprintf_s(wszVelocity, L"%.0f (Max: %.0f)", flCurrent, g_flMaxVelocity);
    else 
        swprintf_s(wszVelocity, L"%.0f", flCurrent);

    int wide = 0, tall = 0;
    g_pSurface->GetTextSize(g_hFont, wszVelocity, wide, tall);

    float posX = cl_velocity_x ? cl_velocity_x->value : 0.3f;
    float posY = cl_velocity_y ? cl_velocity_y->value : 0.85f;

    if (posX < 0) posX = 0; else if (posX > 1.0f) posX = 1.0f;
    if (posY < 0) posY = 0; else if (posY > 1.0f) posY = 1.0f;

    int finalX = (int)(g_iScreenWidth * posX) - (wide / 2);
    int finalY = (int)(g_iScreenHeight * posY);

    g_pSurface->DrawSetTextFont(g_hFont);
    g_pSurface->DrawSetTextColor(g_iCurR, g_iCurG, g_iCurB, g_iCurA);
    g_pSurface->DrawSetTextPos(finalX, finalY);
    g_pSurface->DrawPrintText(wszVelocity, wcslen(wszVelocity));

    return result;
}
