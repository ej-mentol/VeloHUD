#ifndef PLUGINS_H
#define PLUGINS_H

#include <metahook.h>
#include <cvardef.h>
#include <pm_defs.h>
#include <Interface/VGUI/ISurface2.h>
#include <IVGUI2Extension.h>

extern cl_enginefunc_t gEngfuncs;
extern cl_exportfuncs_t gExportfuncs;
extern metahook_api_t* g_pMetaHookAPI;
extern vgui::ISurface2* g_pSurface;
extern IVGUI2Extension* g_pVGUI2Extension;

#endif
