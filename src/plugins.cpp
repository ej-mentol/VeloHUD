#include <metahook.h>
#include <interface.h>
#include "plugins.h"
#include "exportfuncs.h"

cl_enginefunc_t gEngfuncs;
metahook_api_t *g_pMetaHookAPI;
mh_interface_t *g_pInterface;
mh_enginesave_t *g_pMetaSave;

vgui::ISurface2* g_pSurface = nullptr;
IVGUI2Extension* g_pVGUI2Extension = nullptr;

IVGUI2Extension* VGUI2Extension() {
    return g_pVGUI2Extension;
}

void VGUI2Extension_Init(HMODULE hModule) {
    if (!hModule) hModule = (HMODULE)g_pMetaHookAPI->GetPluginModuleHandleByBaseFileName("VGUI2Extension");
    if (!hModule) hModule = GetModuleHandleA("VGUI2Extension.dll");
    
    if (!hModule) return;

    auto factory = (CreateInterfaceFn)GetProcAddress(hModule, "CreateInterface");
    if (factory) {
        g_pVGUI2Extension = (IVGUI2Extension*)factory(VGUI2_EXTENSION_INTERFACE_VERSION, NULL);
        g_pSurface = (vgui::ISurface2*)factory(VGUI_SURFACE2_INTERFACE_VERSION, NULL);
    }
}

void DllLoadNotification(mh_load_dll_notification_context_t* ctx) {
    if (!ctx->BaseDllName || _wcsicmp(ctx->BaseDllName, L"VGUI2Extension.dll") != 0)
        return;

    if (ctx->flags & LOAD_DLL_NOTIFICATION_IS_LOAD) {
        VGUI2Extension_Init(ctx->hModule);
    }
    else if (ctx->flags & LOAD_DLL_NOTIFICATION_IS_UNLOAD) {
        // The module is detaching: drop every pointer into it. The surface is
        // re-acquired on the next load, and the font handle belongs to the old
        // VGUI2 instance, so it must not survive either.
        g_pVGUI2Extension = nullptr;
        g_pSurface = nullptr;
        VeloHUD_ResetFont();
    }
}

class CPlugins : public IPluginsV4 {
public:
    void Init(metahook_api_t *pAPI, mh_interface_t *pInterface, mh_enginesave_t *pMetaSave) override {
        g_pMetaHookAPI = pAPI;
        g_pInterface = pInterface;
        g_pMetaSave = pMetaSave;
        g_pMetaHookAPI->RegisterLoadDllNotificationCallback(DllLoadNotification);
    }

    void Shutdown(void) override {
        g_pMetaHookAPI->UnregisterLoadDllNotificationCallback(DllLoadNotification);
    }

    void LoadEngine(cl_enginefunc_t *pEngineFuncs) override {
        memcpy(&gEngfuncs, pEngineFuncs, sizeof(gEngfuncs));
    }

    void LoadClient(cl_exportfuncs_t *pExportFuncs) override {
        memcpy(&gExportfuncs, pExportFuncs, sizeof(gExportfuncs));

        pExportFuncs->HUD_Init = HUD_Init;
        pExportFuncs->HUD_VidInit = HUD_VidInit;
        pExportFuncs->HUD_Redraw = HUD_Redraw;
        pExportFuncs->HUD_PlayerMove = HUD_PlayerMove;

        VGUI2Extension_Init(NULL);
    }

    void ExitGame(int iResult) override {
    }

    const char *GetVersion(void) override {
        return VELOHUD_VERSION;
    }
};

static CPlugins s_Plugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CPlugins, IPluginsV4, METAHOOK_PLUGIN_API_VERSION_V4, s_Plugin);
