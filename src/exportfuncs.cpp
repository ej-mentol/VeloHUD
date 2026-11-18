#include <metahook.h>
#include "plugins.h"
#include "exportfuncs.h"
#include "VelocityFilter.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <cmath>

cl_exportfuncs_t gExportfuncs = { 0 };
CVelocityFilter g_Filter;

float g_flRawVelocity = 0.0f;
float g_flEngineVelocity = 0.0f;
float g_flFilteredVelocity = 0.0f;
float g_flMaxVelocity = 0.0f;
float g_flVerticalVelocity = 0.0f;
float g_flMaxVertical = 0.0f;
float g_flMinVertical = 0.0f;

// Diagnostic for the "how often does the engine call HUD_PlayerMove" question.
// Compiled only with VELOHUD_DIAG; see the README diagnostics section.
#ifdef VELOHUD_DIAG
static int g_iPlayerMoveCalls = 0;
static long long g_llPlayerMoveCallsTotal = 0;
static int g_iPlayerMoveCallsPeak = 0;
static int g_iRedrawFrames = 0;
static int g_iRedrawZeroMoveFrames = 0;
static double g_flLastRedrawTime = -1.0;
static double g_flPlayerMoveFrametimeSum = 0.0;
static double g_flPlayerMoveFrametimeTotal = 0.0;
static double g_flRedrawTimeTotal = 0.0;

static void ResetDiagnostics()
{
	g_iPlayerMoveCalls = 0;
	g_llPlayerMoveCallsTotal = 0;
	g_iPlayerMoveCallsPeak = 0;
	g_iRedrawFrames = 0;
	g_iRedrawZeroMoveFrames = 0;
	g_flLastRedrawTime = -1.0;
	g_flPlayerMoveFrametimeSum = 0.0;
	g_flPlayerMoveFrametimeTotal = 0.0;
	g_flRedrawTimeTotal = 0.0;
}
#endif

// VGUI2 resources
vgui::HFont g_hFont = 0;
int g_iLastFontSize = -1;
int g_iCurR = 180, g_iCurG = 180, g_iCurB = 180, g_iCurA = 255;
static char g_szLastColor[64] = { 0 };

// Data source for the displayed speed.
// 0 = client prediction, raw          2 = engine (interpolated curstate)
// 1 = client prediction, filtered     3 = hybrid of engine and prediction, raw
//                                     4 = hybrid of engine and prediction, filtered
enum VelocitySource
{
	kSourcePredictionRaw = 0,
	kSourcePredictionFiltered = 1,
	kSourceEngine = 2,
	kSourceHybridRaw = 3,
	kSourceHybridFiltered = 4
};

// CVars
cvar_t* velo_mode = nullptr;
cvar_t* velo_marker = nullptr;
cvar_t* velo_pos_x = nullptr;
cvar_t* velo_pos_y = nullptr;
cvar_t* velo_font_size = nullptr;
cvar_t* velo_color = nullptr;
cvar_t* velo_hide_max = nullptr;
cvar_t* velo_hide_vertical = nullptr;

static void VeloConPrintf(const char* fmt, ...)
{
	if (!gEngfuncs.Con_Printf)
		return;
	char buffer[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	gEngfuncs.Con_Printf("%s", buffer);
}

void VeloHUD_ResetFont()
{
	// CFontManager refuses AddGlyphSetToFont on a handle that already owns glyph
	// sets, so a new size needs a new handle. The old one is intentionally left
	// alone: VGUI2 has no DeleteFont, so one handle leaks per distinct size.
	g_hFont = 0;
	g_iLastFontSize = -1;
}

void velo_reset_max_f(void)
{
	g_flMaxVelocity = 0.0f;
	g_flMaxVertical = 0.0f;
	g_flMinVertical = 0.0f;
#ifdef VELOHUD_DIAG
	// Also start a fresh measurement window without a map change.
	ResetDiagnostics();
#endif
}

static bool CvarIsOn(cvar_t* cvar)
{
	return cvar && cvar->value != 0.0f;
}

// GoldSrc cvars have no min/max, so every value is clamped where it is read.
// Nothing is written back: the archived config keeps exactly what the user
// typed and the HUD does not touch the cvar list while it runs.
static int GetVelocityMode()
{
	if (!velo_mode || !std::isfinite(velo_mode->value))
		return 2;
	return static_cast<int>(std::clamp(velo_mode->value, 0.0f, 5.0f));
}

// Index into VelocitySource; -1 means the HUD is off.
static int GetVelocitySource()
{
	return GetVelocityMode() - 1;
}

static int GetFontSize()
{
	if (!velo_font_size || !std::isfinite(velo_font_size->value))
		return 24;
	return static_cast<int>(std::clamp(velo_font_size->value, 6.0f, 512.0f));
}

static float GetPosition(cvar_t* cvar, float fallback)
{
	if (!cvar || !std::isfinite(cvar->value))
		return fallback;
	return std::clamp(cvar->value, 0.0f, 1.0f);
}

static float GetMarker()
{
	if (!velo_marker || !std::isfinite(velo_marker->value) || velo_marker->value <= 0.0f)
		return 0.0f;
	return velo_marker->value;
}

static void PrintValue(const char* name, int value, const char* meaning)
{
	VeloConPrintf("%-22s : %d  %s\n", name, value, meaning);
}

static void PrintFlag(cvar_t* cvar, const char* name, const char* meaning)
{
	if (cvar)
		PrintValue(name, CvarIsOn(cvar) ? 1 : 0, meaning);
}

static void PrintFloatValue(const char* name, float value, const char* meaning)
{
	VeloConPrintf("%-22s : %.2f  %s\n", name, value, meaning);
}

void velo_help_f(void)
{
	VeloConPrintf("\n--- VeloHUD %s ---\n", VELOHUD_VERSION);
	PrintValue("velo_mode", GetVelocityMode(), "(0 off, 1 pred raw, 2 pred filtered, 3 engine, 4 hybrid raw, 5 hybrid filtered)");
	PrintFloatValue("velo_marker", GetMarker(), "(0 = off, otherwise a fixed benchmark shown as (N))");
	PrintFloatValue("velo_pos_x", GetPosition(velo_pos_x, 0.5f), "(0.00 .. 1.00 screen ratio)");
	PrintFloatValue("velo_pos_y", GetPosition(velo_pos_y, 0.85f), "(0.00 .. 1.00 screen ratio)");
	PrintValue("velo_font_size", GetFontSize(), "(pixels, 6..512)");
	PrintFlag(velo_hide_max, "velo_hide_max", "(0/1, hide the max speed)");
	PrintFlag(velo_hide_vertical, "velo_hide_vertical", "(0/1, hide the vertical speed line)");
	if (velo_color && velo_color->string)
		VeloConPrintf("%-22s : \"%s\"  (R G B A, missing channels default to 180 180 180 255)\n", "velo_color", velo_color->string);
#ifdef VELOHUD_DIAG
	const double averagePlayerMoves = g_iRedrawFrames > 0
		? static_cast<double>(g_llPlayerMoveCallsTotal) / g_iRedrawFrames : 0.0;
	const double averagePlayerMoveDt = g_iRedrawFrames > 0
		? g_flPlayerMoveFrametimeTotal / g_iRedrawFrames : 0.0;
	const double averageRedrawDt = g_iRedrawFrames > 0
		? g_flRedrawTimeTotal / g_iRedrawFrames : 0.0;
	const double dtRatio = averageRedrawDt > 0.0 ? averagePlayerMoveDt / averageRedrawDt : 0.0;
	VeloConPrintf("HUD_PlayerMove / frame   : avg %.2f, peak %d, zero-move %d/%d\n",
		averagePlayerMoves, g_iPlayerMoveCallsPeak, g_iRedrawZeroMoveFrames, g_iRedrawFrames);
	VeloConPrintf("HUD_PlayerMove dt/frame  : %.4f vs redraw %.4f (ratio %.2f)\n",
		averagePlayerMoveDt, averageRedrawDt, dtRatio);
#endif
	VeloConPrintf("commands                 : velo_reset_max, velo_help\n");
	VeloConPrintf("------------------------\n");
}

void HUD_Init(void)
{
	gExportfuncs.HUD_Init();

	velo_mode = gEngfuncs.pfnRegisterVariable("velo_mode", "2", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	velo_marker = gEngfuncs.pfnRegisterVariable("velo_marker", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	velo_pos_x = gEngfuncs.pfnRegisterVariable("velo_pos_x", "0.5", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	velo_pos_y = gEngfuncs.pfnRegisterVariable("velo_pos_y", "0.85", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	velo_font_size = gEngfuncs.pfnRegisterVariable("velo_font_size", "24", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	velo_color = gEngfuncs.pfnRegisterVariable("velo_color", "180 180 180 255", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	velo_hide_max = gEngfuncs.pfnRegisterVariable("velo_hide_max", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	velo_hide_vertical = gEngfuncs.pfnRegisterVariable("velo_hide_vertical", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);

	gEngfuncs.pfnAddCommand("velo_reset_max", velo_reset_max_f);
	gEngfuncs.pfnAddCommand("velo_help", velo_help_f);
}

static int ClampColorChannel(int value)
{
	return std::clamp(value, 0, 255);
}

static void UpdateHUDParams()
{
	if (!g_pSurface)
		return;

	const int size = GetFontSize();
	if (size != g_iLastFontSize || g_hFont == 0)
	{
		g_hFont = g_pSurface->CreateFont();
		g_pSurface->AddGlyphSetToFont(g_hFont, "Tahoma", size, 700, 0, 0,
			vgui::ISurface2::FONTFLAG_ANTIALIAS | vgui::ISurface2::FONTFLAG_DROPSHADOW);
		g_iLastFontSize = size;
	}

	if (velo_color && velo_color->string && strcmp(velo_color->string, g_szLastColor) != 0)
	{
		int r = 180, g = 180, b = 180, a = 255;
		const int count = sscanf(velo_color->string, "%d %d %d %d", &r, &g, &b, &a);
		if (count >= 1)
		{
			g_iCurR = ClampColorChannel(r);
			g_iCurG = ClampColorChannel(count >= 2 ? g : 180);
			g_iCurB = ClampColorChannel(count >= 3 ? b : 180);
			g_iCurA = ClampColorChannel(count >= 4 ? a : 255);
		}
		snprintf(g_szLastColor, sizeof(g_szLastColor), "%s", velo_color->string);
	}
}

int HUD_VidInit(void)
{
	const int result = gExportfuncs.HUD_VidInit();

	// Map change / reconnect: drop the per-map state and the font handle.
	g_flMaxVelocity      = 0.0f;
	g_flRawVelocity      = 0.0f;
	g_flEngineVelocity   = 0.0f;
	g_flFilteredVelocity = 0.0f;
	g_flVerticalVelocity = 0.0f;
	g_flMaxVertical      = 0.0f;
	g_flMinVertical      = 0.0f;
	g_Filter.Reset();
#ifdef VELOHUD_DIAG
	ResetDiagnostics();
#endif
	// The font handle is deliberately kept across VidInit: a video mode change
	// restarts the game, so the module-unload callback already drops the handle
	// together with the VGUI2 module, while a map change leaves the surface and
	// its fonts alive. A missing handle is recreated lazily in HUD_Redraw.
	return result;
}

void HUD_PlayerMove(struct playermove_s* ppmove, int server)
{
#ifdef VELOHUD_DIAG
	++g_iPlayerMoveCalls;
#endif
	gExportfuncs.HUD_PlayerMove(ppmove, server);
	if (!ppmove)
		return;

#ifdef VELOHUD_DIAG
	if (std::isfinite(ppmove->frametime) && ppmove->frametime > 0.0f)
		g_flPlayerMoveFrametimeSum += ppmove->frametime;
#endif

	// A non-finite sample is ignored instead of being turned into 0: the filter
	// already keeps its previous value, and the raw modes must not dip for one
	// frame either.
	const float velocityX = ppmove->velocity[0];
	const float velocityY = ppmove->velocity[1];
	if (std::isfinite(velocityX) && std::isfinite(velocityY))
	{
		const float rawVelocity = sqrtf(velocityX * velocityX + velocityY * velocityY);
		if (std::isfinite(rawVelocity))
		{
			g_flRawVelocity = rawVelocity;
			// Prediction replay can call this several times between two redraws,
			// but ppmove->frametime is split across those calls: measured
			// sum(frametime) equals the redraw delta, so the filter is
			// time-correct here and does not need to move to HUD_Redraw.
			g_flFilteredVelocity = g_Filter.Update(g_flRawVelocity, ppmove->frametime);
		}
	}

	// Vertical speed is only sampled when the second line is enabled and the
	// HUD is not off.
	const int source = GetVelocitySource();
	const float verticalVelocity = ppmove->velocity[2];
	if (!CvarIsOn(velo_hide_vertical) && source != kSourceEngine && source >= kSourcePredictionRaw &&
		std::isfinite(verticalVelocity))
		g_flVerticalVelocity = verticalVelocity;
}

static float ResolveDisplayedSpeed(float predictedRaw, float predictedFiltered, float engineSpeed, int source)
{
	switch (source)
	{
	case kSourcePredictionRaw:      return predictedRaw;
	case kSourcePredictionFiltered: return predictedFiltered;
	case kSourceEngine:             return engineSpeed;
	case kSourceHybridRaw:          return engineSpeed * 0.5f + predictedRaw * 0.5f;
	case kSourceHybridFiltered:     return engineSpeed * 0.5f + predictedFiltered * 0.5f;
	default:                        return predictedFiltered;
	}
}

int HUD_Redraw(float time, int intermission)
{
#ifdef VELOHUD_DIAG
	// Snapshot before any return: a skipped frame must not leave calls behind
	// for the next redraw to count, or the peak would be meaningless.
	const int playerMoveCalls = g_iPlayerMoveCalls;
	g_iPlayerMoveCalls = 0;
	const double playerMoveFrametime = g_flPlayerMoveFrametimeSum;
	g_flPlayerMoveFrametimeSum = 0.0;
	const double now = time;
	const double frameDt = g_flLastRedrawTime >= 0.0 ? now - g_flLastRedrawTime : 0.0;
	const bool continuousFrame = frameDt > 0.0 && frameDt <= 0.25;
	g_flLastRedrawTime = now;
#endif

	const int result = gExportfuncs.HUD_Redraw(time, intermission);

	if (intermission || GetVelocityMode() == 0)
		return result;

	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	if (!local || local->index <= 0)
		return result;

#ifdef VELOHUD_DIAG
	// Only continuous frames that actually render the HUD: a console pause or a
	// loading gap must not turn into one huge "peak".
	if (continuousFrame)
	{
		g_llPlayerMoveCallsTotal += playerMoveCalls;
		g_flPlayerMoveFrametimeTotal += playerMoveFrametime;
		g_flRedrawTimeTotal += frameDt;
		++g_iRedrawFrames;
		if (playerMoveCalls == 0)
			++g_iRedrawZeroMoveFrames;
		else if (playerMoveCalls > g_iPlayerMoveCallsPeak)
			g_iPlayerMoveCallsPeak = playerMoveCalls;
	}
#endif

	UpdateHUDParams();
	if (!g_pSurface || !g_hFont)
		return result;

	// Screen size can change without a video mode restart (windowed resize).
	int screenWidth = 0;
	int screenHeight = 0;
	g_pSurface->GetScreenSize(screenWidth, screenHeight);
	if (screenWidth <= 0 || screenHeight <= 0)
		return result;

	const int source = GetVelocitySource();

	const float engineX = local->curstate.velocity[0];
	const float engineY = local->curstate.velocity[1];
	if (std::isfinite(engineX) && std::isfinite(engineY))
	{
		const float engineSpeed = sqrtf(engineX * engineX + engineY * engineY);
		if (std::isfinite(engineSpeed))
			g_flEngineVelocity = engineSpeed;
	}

	if (!CvarIsOn(velo_hide_vertical) && source == kSourceEngine && std::isfinite(local->curstate.velocity[2]))
		g_flVerticalVelocity = local->curstate.velocity[2];

	const float current = ResolveDisplayedSpeed(
		g_flRawVelocity, g_flFilteredVelocity, g_flEngineVelocity, source);

	if (current > g_flMaxVelocity)
		g_flMaxVelocity = current;

	wchar_t speedText[128];
	const bool showMax = !CvarIsOn(velo_hide_max);
	const float marker = GetMarker();
	const bool haveMarker = marker > 0.0f;
	if (showMax && haveMarker)
		swprintf_s(speedText, L"%.0f (%.0f)", current, marker);
	else if (showMax)
		swprintf_s(speedText, L"%.0f (Max: %.0f)", current, g_flMaxVelocity);
	else
		swprintf_s(speedText, L"%.0f", current);

	const float posX = GetPosition(velo_pos_x, 0.5f);
	const float posY = GetPosition(velo_pos_y, 0.85f);

	int wide = 0, tall = 0;
	g_pSurface->GetTextSize(g_hFont, speedText, wide, tall);

	g_pSurface->DrawSetTextFont(g_hFont);
	g_pSurface->DrawSetTextColor(g_iCurR, g_iCurG, g_iCurB, g_iCurA);

	const int lineX = static_cast<int>(screenWidth * posX) - (wide / 2);
	const int lineY = static_cast<int>(screenHeight * posY);
	g_pSurface->DrawSetTextPos(lineX, lineY);
	g_pSurface->DrawPrintText(speedText, static_cast<int>(wcslen(speedText)));

	if (!CvarIsOn(velo_hide_vertical))
	{
		if (g_flVerticalVelocity > g_flMaxVertical)
			g_flMaxVertical = g_flVerticalVelocity;
		if (g_flVerticalVelocity < g_flMinVertical)
			g_flMinVertical = g_flVerticalVelocity;

		wchar_t verticalText[160];
		if (showMax)
			swprintf_s(verticalText, L"%.0f (Max: %.0f, Min: %.0f)", g_flVerticalVelocity, g_flMaxVertical, g_flMinVertical);
		else
			swprintf_s(verticalText, L"%.0f", g_flVerticalVelocity);

		int vWide = 0, vTall = 0;
		g_pSurface->GetTextSize(g_hFont, verticalText, vWide, vTall);
		g_pSurface->DrawSetTextPos(static_cast<int>(screenWidth * posX) - (vWide / 2), lineY + vTall + 2);
		g_pSurface->DrawPrintText(verticalText, static_cast<int>(wcslen(verticalText)));
	}

	return result;
}
