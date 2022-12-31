/*********************************************************************************
*
*  ===============================================================================
*  |                  GLDirect: Direct3D Device Driver for Mesa.                 |
*  |                                                                             |
*  |                Copyright (C) 1997-2007 SciTech Software, Inc.               |
*  |                                                                             |
*  |Permission is hereby granted, free of charge, to any person obtaining a copy |
*  |of this software and associated documentation files (the "Software"), to deal|
*  |in the Software without restriction, including without limitation the rights |
*  |to use, copy, modify, merge, publish, distribute, sublicense, and/or sell    |
*  |copies of the Software, and to permit persons to whom the Software is        |
*  |furnished to do so, subject to the following conditions:                     |
*  |                                                                             |
*  |The above copyright notice and this permission notice shall be included in   |
*  |all copies or substantial portions of the Software.                          |
*  |                                                                             |
*  |THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR   |
*  |IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,     |
*  |FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE  |
*  |AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER       |
*  |LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,|
*  |OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN    |
*  |THE SOFTWARE.                                                                |
*  ===============================================================================
*
*  ===============================================================================
*  |        Original Author: Keith Harrison <sio2@users.sourceforge.net>         |
*  ===============================================================================
*
* Language:     ANSI C
* Environment:  Windows 9x (Win32)
*
* Description:  Win32 DllMain functions.
*
*********************************************************************************/

#include "dll_main.h"

#include "gld_driver.h"

#include "mmsystem.h"

// ***********************************************************************

// Add your Quake1 engines to this list.
// NAMES MUST BE UPPERCASE!

static const char *g_szQuake1Engines[] = {
	"GLQUAKE.EXE",	// Quake1
	"GLH2.EXE",		// GL Hexen2
    "HL.EXE",       // HalfLife
    "HLDEMO.EXE"    // HalfLife demo
};

static const char *g_szUnrealEngines[] = {
    "DEUSEX.EXE",           // Deus Ex
    "RUNE.EXE",             // Rune
	"UNREALTOURNAMENT.EXE",	// Unreal Tournament
	"WOT.EXE"		        // Wheel of Time
};

// ***********************************************************************

BOOL bInitialized = FALSE;              // callback driver initialized?
BOOL bExited = FALSE;                   // callback driver exited this instance?
HINSTANCE hInstanceDll = NULL;          // DLL instance handle

// Licensing removed. KeithH
static BOOL bDriverValidated = TRUE;	// prior validation status
static BOOL bDriverLicensed = TRUE;	// license registration status

static BOOL bValidINIFound = FALSE;     // Have we found a valid INI file?

HHOOK 	hKeyHook = NULL;				// global keyboard handler hook

// Multi-threaded support needs to be reflected in Mesa code. (DaveM)
int _gld_bMultiThreaded = FALSE;

// ***********************************************************************

DWORD dwLogging = 0; 					// Logging flag
DWORD dwDebugLevel = 0;                 // Log debug level

char szLogPath[_MAX_PATH] = {"\0"};		// Log file path

// ***********************************************************************

typedef struct {
	DWORD	dwDriver;			// 0=SciTech SW, 1=Direct3D SW, 2=Direct3D HW
	BOOL	bMipmapping;		// 0=off, 1=on
	BOOL	bMultitexture;		// 0=off, 1=on
	BOOL	bWaitForRetrace;	// 0=off, 1=on
	BOOL	bFullscreenBlit;	// 0=off, 1=on
	BOOL	bFastFPU;			// 0=off, 1=on
	BOOL	bDirectDrawPersistant;// 0=off, 1=on
	BOOL	bPersistantBuffers; // 0=off, 1=on
	DWORD	dwLogging;			// 0=off, 1=normal, 2=crash-proof
	DWORD	dwLoggingSeverity;	// 0=all, 1=warnings+errors, 2=errors only
	BOOL	bMessageBoxWarnings;// 0=off, 1=on
	BOOL	bMultiThreaded;		// 0=off, 1=on
	BOOL	bAppCustomizations;	// 0=off, 1=on
	BOOL	bHotKeySupport;		// 0=off, 1=on
	BOOL	bSplashScreen;		// 0=off, 1=on

	DWORD	dwAdapter;			// DX8 adapter ordinal
	DWORD	dwTnL;				// Transform & Lighting type
	DWORD	dwMultisample;		// DX8 multisample type
} INI_settings;

static INI_settings ini;

// ***********************************************************************

BOOL APIENTRY GLD_initDriver(void)
{
    // Initialize the callback driver
    if (!gldInitDriver())
        return FALSE;

	return TRUE;
};

// ***********************************************************************

BOOL ReadINIFile(
	HINSTANCE hInstance)
{
	char		szModuleFilename[MAX_PATH];
	char		szSystemDirectory[MAX_PATH];
	const char	szSectionName[] = "Config";
	char		szINIFile[MAX_PATH];
	int			pos;

	// Now using the DLL module handle. KeithH, 24/May/2000.
	// Addendum: GetModuleFileName(NULL, ...    returns process filename,
	//           GetModuleFileName(hModule, ... returns DLL filename,

	// Get the dll path and filename.
	GetModuleFileName(hInstance, &szModuleFilename[0], MAX_PATH); // NULL for current process
	// Get the System directory.
	GetSystemDirectory(&szSystemDirectory[0], MAX_PATH);

	// Test to see if DLL is in system directory.
	if (_strnicmp(szModuleFilename, szSystemDirectory, strlen(szSystemDirectory))==0) {
		// DLL *is* in system directory.
		// Return FALSE to indicate that registry keys should be read.
		return FALSE;
	}

	// Compose filename of INI file
	strcpy(szINIFile, szModuleFilename);
	pos = strlen(szINIFile);
	while (szINIFile[pos] != '\\') {
		pos--;
	}
	szINIFile[pos+1] = '\0';
    // Use run-time DLL path for log file too
    strcpy(szLogPath, szINIFile);
    szLogPath[pos] = '\0';
    // Complete full INI file path
	strcat(szINIFile, "gldirect.ini");

	// Read settings from private INI file.
	// Note that defaults are contained in the calls.
	ini.dwDriver = GetPrivateProfileInt(szSectionName, "dwDriver", 2, szINIFile);
	ini.bMipmapping = GetPrivateProfileInt(szSectionName, "bMipmapping", 1, szINIFile);
	ini.bMultitexture = GetPrivateProfileInt(szSectionName, "bMultitexture", 1, szINIFile);
	ini.bWaitForRetrace = GetPrivateProfileInt(szSectionName, "bWaitForRetrace", 0, szINIFile);
	ini.bFullscreenBlit = GetPrivateProfileInt(szSectionName, "bFullscreenBlit", 0, szINIFile);
	ini.bFastFPU = GetPrivateProfileInt(szSectionName, "bFastFPU", 1, szINIFile);
	ini.bDirectDrawPersistant = GetPrivateProfileInt(szSectionName, "bPersistantDisplay", 0, szINIFile);
	ini.bPersistantBuffers = GetPrivateProfileInt(szSectionName, "bPersistantResources", 0, szINIFile);
//	ini.dwLogging = GetPrivateProfileInt(szSectionName, "dwLogging", 0, szINIFile);
	ini.dwLogging = GetPrivateProfileInt(szSectionName, "dwLogging", 1, szINIFile);
	ini.dwLoggingSeverity = GetPrivateProfileInt(szSectionName, "dwLoggingSeverity", 0, szINIFile);
	ini.bMessageBoxWarnings = GetPrivateProfileInt(szSectionName, "bMessageBoxWarnings", 0, szINIFile);
	ini.bMultiThreaded = GetPrivateProfileInt(szSectionName, "bMultiThreaded", 0, szINIFile);
	ini.bAppCustomizations = GetPrivateProfileInt(szSectionName, "bAppCustomizations", 1, szINIFile);
	ini.bHotKeySupport = GetPrivateProfileInt(szSectionName, "bHotKeySupport", 0, szINIFile);
	ini.bSplashScreen = GetPrivateProfileInt(szSectionName, "bSplashScreen", 1, szINIFile);

	// New for GLDirect 3.x
	ini.dwAdapter		= GetPrivateProfileInt(szSectionName, "dwAdapter", 0, szINIFile);
	// dwTnL now defaults to zero (chooses TnL at runtime). KeithH
	ini.dwTnL			= GetPrivateProfileInt(szSectionName, "dwTnL", 0, szINIFile);
	ini.dwMultisample	= GetPrivateProfileInt(szSectionName, "dwMultisample", 0, szINIFile);

	return TRUE;
}

// ***********************************************************************

BOOL dllReadRegistry(
	HINSTANCE hInstance)
{
	// Read settings from INI file, if available
    bValidINIFound = FALSE;
	if (ReadINIFile(hInstance)) {
		const char *szRendering[3] = {
			"SciTech Software Renderer",
			"Direct3D MMX Software Renderer",
			"Direct3D Hardware Renderer"
		};
		// Set globals
		glb.bPrimary = 1;
		glb.bHardware = (ini.dwDriver == 2) ? 1 : 0;
		strcpy(glb.szDDName, "Primary");
		strcpy(glb.szD3DName, szRendering[ini.dwDriver]);
		glb.dwRendering = ini.dwDriver;
		glb.bUseMipmaps = ini.bMipmapping;
		glb.bMultitexture = ini.bMultitexture;
		glb.bWaitForRetrace = ini.bWaitForRetrace;
		glb.bFullscreenBlit = ini.bFullscreenBlit;
		glb.bFastFPU = ini.bFastFPU;
		glb.bDirectDrawPersistant = ini.bDirectDrawPersistant;
		glb.bPersistantBuffers = ini.bPersistantBuffers;
		dwLogging = ini.dwLogging;
		dwDebugLevel = ini.dwLoggingSeverity;
		glb.bMessageBoxWarnings = ini.bMessageBoxWarnings;
		glb.bMultiThreaded = ini.bMultiThreaded;
		glb.bAppCustomizations = ini.bAppCustomizations;
        glb.bHotKeySupport = ini.bHotKeySupport;
//		bSplashScreen = ini.bSplashScreen;

		// New for GLDirect 3.x
		glb.dwAdapter		= ini.dwAdapter;
		glb.dwDriver		= ini.dwDriver;
		glb.dwTnL			= ini.dwTnL;
		glb.dwMultisample	= ini.dwMultisample;
        bValidINIFound = TRUE;
		return TRUE;
	}
	return FALSE;
#if 0
	// Read settings from registry
	else {
	HKEY	hReg;
	DWORD	cbValSize;
	DWORD	dwType = REG_SZ; // Registry data type for strings
	BOOL	bRegistryError;
	BOOL	bSuccess;

#define REG_READ_DWORD(a, b)							\
	cbValSize = sizeof(b);								\
	if (ERROR_SUCCESS != RegQueryValueEx( hReg, (a),	\
		NULL, NULL, (LPBYTE)&(b), &cbValSize ))			\
		bRegistryError = TRUE;

#define REG_READ_DEVICEID(a, b)									\
	cbValSize = MAX_DDDEVICEID_STRING;							\
	if(ERROR_SUCCESS != RegQueryValueEx(hReg, (a), 0, &dwType,	\
					(LPBYTE)&(b), &cbValSize))					\
		bRegistryError = TRUE;

#define REG_READ_STRING(a, b)									\
	cbValSize = sizeof((b));									\
	if(ERROR_SUCCESS != RegQueryValueEx(hReg, (a), 0, &dwType,	\
					(LPBYTE)&(b), &cbValSize))					\
		bRegistryError = TRUE;

	// Read settings from the registry.

	// Open the registry key for the current user if it exists.
	bSuccess = (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER,
									  GLDIRECT_REG_SETTINGS_KEY,
									  0,
									  KEY_READ,
									  &hReg));
    // Otherwise open the registry key for the local machine.
    if (!bSuccess)
        bSuccess = (ERROR_SUCCESS == RegOpenKeyEx(GLDIRECT_REG_KEY_ROOT,
									  GLDIRECT_REG_SETTINGS_KEY,
									  0,
									  KEY_READ,
									  &hReg));
    if (!bSuccess)
        return FALSE;

	bRegistryError = FALSE;

	REG_READ_DWORD(GLDIRECT_REG_SETTING_PRIMARY, glb.bPrimary);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_D3D_HW, glb.bHardware);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_LOGGING, dwLogging);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_DEBUGLEVEL, dwDebugLevel);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_RENDERING, glb.dwRendering);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_MULTITEXTURE, glb.bMultitexture);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_WAITFORRETRACE, glb.bWaitForRetrace);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_FULLSCREENBLIT, glb.bFullscreenBlit);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_USEMIPMAPS, glb.bUseMipmaps);

	REG_READ_DEVICEID(GLDIRECT_REG_SETTING_DD_NAME, glb.szDDName);
	REG_READ_DEVICEID(GLDIRECT_REG_SETTING_D3D_NAME, glb.szD3DName);

	REG_READ_DWORD(GLDIRECT_REG_SETTING_MSGBOXWARNINGS, glb.bMessageBoxWarnings);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_PERSISTDISPLAY, glb.bDirectDrawPersistant);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_PERSISTBUFFERS, glb.bPersistantBuffers);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_FASTFPU, glb.bFastFPU);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_HOTKEYS, glb.bHotKeySupport);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_MULTITHREAD, glb.bMultiThreaded);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_APPCUSTOM, glb.bAppCustomizations);
    REG_READ_DWORD(GLDIRECT_REG_SETTING_SPLASHSCREEN, bSplashScreen);

	// New for GLDirect 3.x
	glb.dwDriver = glb.dwRendering;
	REG_READ_DWORD(GLDIRECT_REG_SETTING_ADAPTER, glb.dwAdapter);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_TNL, glb.dwTnL);
	REG_READ_DWORD(GLDIRECT_REG_SETTING_MULTISAMPLE, glb.dwMultisample);

	RegCloseKey(hReg);

	// Open the global registry key for GLDirect
	bSuccess = (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE,
									  GLDIRECT_REG_SETTINGS_KEY,
									  0,
									  KEY_READ,
									  &hReg));
    if (bSuccess) {
	    // Read the installation path for GLDirect
	    REG_READ_STRING("InstallLocation",szLogPath);
	    RegCloseKey(hReg);
        }

	if (bRegistryError || !bSuccess)
		return FALSE;
	else
		return TRUE;

#undef REG_READ_DWORD
#undef REG_READ_DEVICEID
#undef REG_READ_STRING
	}
#endif
}

// ***********************************************************************
#if 0
BOOL dllWriteRegistry(
	void )
{
	HKEY 	hReg;
	DWORD 	dwCreateDisposition;
	BOOL 	bRegistryError = FALSE;

#define REG_WRITE_DWORD(a, b)							\
	cbValSize = sizeof(b);								\
	if (ERROR_SUCCESS != RegSetValueEx( hReg, (a),		\
		0, REG_DWORD, (LPBYTE)&(b), cbValSize ))		\
		bRegistryError = TRUE;

	if (ERROR_SUCCESS == RegCreateKeyEx( GLDIRECT_REG_KEY_ROOT, GLDIRECT_REG_SETTINGS_KEY,
										0, NULL, 0, KEY_WRITE, NULL, &hReg,
										&dwCreateDisposition ))
	{
		RegFlushKey(hReg); // Make sure keys are written to disk
		RegCloseKey(hReg);
		hReg = NULL;
	}

	if (bRegistryError)
		return FALSE;
	else
		return TRUE;

#undef REG_WRITE_DWORD
}
#endif
// ***********************************************************************

void gldInitHotKeys(HINSTANCE hInstance)
{
	// Hot-Key support at all?
	if (!glb.bHotKeySupport)
		return;

	// Install global keyboard interceptor
	hKeyHook = SetWindowsHookEx(WH_KEYBOARD, gldKeyProc, hInstance, 0);
}

// ***********************************************************************

void gldExitHotKeys(void)
{
	// Hot-Key support at all?
	if (!glb.bHotKeySupport)
		return;

	// Remove global keyboard interceptor
	if (hKeyHook)
		UnhookWindowsHookEx(hKeyHook);
	hKeyHook = NULL;
}

// ***********************************************************************

static void _gldDumpAppCust(int iCase)
{
	// Dump the Case detected in gldSetAppCustomizations() to the log.
	// This is private to SciTech, so we'll just dump a number.
	//gldLogPrintf(GLDLOG_SYSTEM, "-- Case %d", iCase);
	// Logging hasn't started yet, so keep for later, when we can dump it...
	glb.iAppCustomisation = iCase;
}

// ***********************************************************************

// Note: This app-customization step must be performed in both the main
// OpenGL32 driver and the callback driver DLLs for multithreading option.
void gldSetAppCustomizations(void)
{
	char		szModuleFileName[MAX_PATH];
	int			iSize = MAX_PATH;
	int			i;

	// Get the currently loaded EXE filename.
	GetModuleFileName(NULL, &szModuleFileName[0], MAX_PATH); // NULL for current process
	_strupr(szModuleFileName);
	iSize = strlen(szModuleFileName);

	// Check for specific EXEs and adjust global settings accordingly

	// NOTE: In GLD3.x "bDirectDrawPersistant" corresponds to IDirect3D8 and
	//       "bPersistantBuffers" corresponds to IDirect3DDevice8. KeithH

	// Case 1: 3DStudio must be multi-threaded
	// Added: Discreet GMAX (3DStudio MAX 4 for gamers. KeithH)
	if (strstr(szModuleFileName, "3DSMAX.EXE")
		|| strstr(szModuleFileName, "3DSVIZ.EXE")
		|| strstr(szModuleFileName, "GMAX.EXE")) {
		glb.bMultiThreaded = TRUE;
		glb.bDirectDrawPersistant = FALSE;
		glb.bPersistantBuffers = FALSE;
		_gldDumpAppCust(1);
		return;
	}

	// Case 2: Solid Edge must use pre-allocated resources for all GLRCs
	if (strstr(szModuleFileName, "PART.EXE")
		|| strstr(szModuleFileName, "ASSEMBL.EXE")
		|| strstr(szModuleFileName, "DRAFT.EXE")
		|| strstr(szModuleFileName, "SMARTVW.EXE")
		|| strstr(szModuleFileName, "SMETAL.EXE")) {
		glb.bMultiThreaded = FALSE;
		glb.bDirectDrawPersistant = TRUE;
		glb.bPersistantBuffers = FALSE;
		_gldDumpAppCust(2);
		return;
	}

	// Case 3: Sudden Depth creates and destroys GLRCs on paint commands
	if (strstr(szModuleFileName, "SUDDEPTH.EXE")
		|| strstr(szModuleFileName, "SUDDEMO.EXE")) {
		glb.bMultiThreaded = FALSE;
		glb.bDirectDrawPersistant = TRUE;
		glb.bPersistantBuffers = TRUE;
		glb.bFullscreenBlit = TRUE;
		_gldDumpAppCust(3);
		return;
	}

	// Case 4: StereoGraphics test apps create and destroy GLRCs on paint commands
	if (strstr(szModuleFileName, "REDBLUE.EXE")
		|| strstr(szModuleFileName, "DIAGNOSE.EXE")) {
		glb.bMultiThreaded = FALSE;
		glb.bDirectDrawPersistant = TRUE;
		glb.bPersistantBuffers = TRUE;
		_gldDumpAppCust(4);
		return;
	}

	// Case 5: Pipes screen savers share multiple GLRCs for same window
	if (strstr(szModuleFileName, "PIPES.SCR")
		|| (strstr(szModuleFileName, "PIPES") && strstr(szModuleFileName, ".SCR"))) {
		glb.bMultiThreaded = FALSE;
		glb.bDirectDrawPersistant = TRUE;
		glb.bPersistantBuffers = TRUE;
		_gldDumpAppCust(5);
		return;
	}

	// Case 6: AutoVue uses sub-viewport ops which are temporarily broken in stereo window
	if (strstr(szModuleFileName, "AVWIN.EXE")) {
		glb.bMultiThreaded = FALSE;
		glb.bDirectDrawPersistant = TRUE;
		glb.bPersistantBuffers = TRUE;
		_gldDumpAppCust(6);
		return;
	}
	// Case 7: Quake3 is waiting for DDraw objects to be released at exit
	if (strstr(szModuleFileName, "QUAKE")) {
		glb.bMultiThreaded = FALSE;
		glb.bDirectDrawPersistant = FALSE;
		glb.bPersistantBuffers = FALSE;
        glb.bFullscreenBlit = FALSE;
		// Quake1 engine
		glb.bFastFPU		= FALSE;
		glb.bDisableZTrick	= TRUE;
		// FALL THROUGH
		//return;
		_gldDumpAppCust(7);
	}
	// Case 8: Quake1 Engine Games (GLQuake)
	for (i=0; i<(sizeof(g_szQuake1Engines)/sizeof(g_szQuake1Engines[0])); i++) {
		if (strstr(szModuleFileName, g_szQuake1Engines[i])) {
			// These apps contain asm and mess with the floating point control word.
			// Flag GLD to tell D3D to preserve the FPCW when the device is created.
			glb.bFastFPU		= FALSE;
			// Disable the horrid z-trick
			glb.bDisableZTrick	= TRUE;
			gldLogMessage(GLDLOG_SYSTEM, "Quake1 Engine\n");
			_gldDumpAppCust(8);
			return;
		}
	}
	// Case 9: Reflection GLX server is unable to switch contexts at run-time
	if (strstr(szModuleFileName, "RX.EXE")) {
		glb.bMultiThreaded = FALSE;
        glb.bMessageBoxWarnings = FALSE;
		_gldDumpAppCust(9);
		return;
	}
	// Case 10: Original AutoCAD 2000 must share DDraw objects across GLRCs
	if (strstr(szModuleFileName, "ACAD.EXE")) {
		glb.bFastFPU = FALSE;
        if (GetModuleHandle("wopengl6.hdi") != NULL) {
		glb.bMultiThreaded = FALSE;
		glb.bDirectDrawPersistant = TRUE;
		glb.bPersistantBuffers = FALSE;
		}
		// Test for AutoCAD 2002. KeithH
        if (GetModuleHandle("wopengl7.hdi") != NULL) {
			glb.bMultiThreaded = TRUE;
		}
		_gldDumpAppCust(10);
		return;
	}
	// Case 11: Game "Heavy Metal: FAKK2" sets invalid viewport dimensions.
	if (strstr(szModuleFileName, "FAKK2.EXE")) {
		// Use hack for invalid viewport
		glb.bFAKK2 = TRUE;
		_gldDumpAppCust(11);
		return;
	}
	// Case 12: Serious Sam AND Serious Sam 2 AND other games need fullscreen.
	//
	// WARNING: These apps share the same EXE name!
	//          Any app customisations made for one of them will effect the other!
	//
	// When running fullscreen, this app does not pass in a top-level
	// window; D3D9 refuses to create a device that will render to it.
	//
    // Games from IFD also need FullscreenBlit enabled.
	if (strstr(szModuleFileName, "SERIOUSSAM.EXE") ||
	    strstr(szModuleFileName, "BUGDOM.EXE") ||
        strstr(szModuleFileName, "NANOSAUR.EXE"))
	{
		// Force fullscreen blit - D3D9 will then accept the window.
		glb.bFullscreenBlit = TRUE;
		_gldDumpAppCust(12);
		// Bugdom needs an extra hack for wrapping textures
        if (strstr(szModuleFileName, "BUGDOM.EXE")) {
			glb.bBugdom = TRUE;
			// An attempt to stop GeForce3 Ti et. al. crashing.
			glb.bUseMesaDisplayLists = TRUE;
		}
		return;
	}
    // Case 13: Newer games from IFD need OpenGL 1.3 enabled.
	if (strstr(szModuleFileName, "BUGDOM 2.EXE") ||
        strstr(szModuleFileName, "NANOSAUR 2.EXE") ||
        strstr(szModuleFileName, "OTTO MATIC.EXE"))
	{
		//glb.bFullscreenBlit = TRUE;
        glb.bGL13Needed = TRUE;
		_gldDumpAppCust(13);
        return;
    }
	// Case 14: Unreal engine uses a z-trick
	for (i=0; i<(sizeof(g_szUnrealEngines)/sizeof(g_szUnrealEngines[0])); i++) {
		if (strstr(szModuleFileName, g_szUnrealEngines[i])) {
            glb.bDisableZTrick	= TRUE;
			gldLogMessage(GLDLOG_SYSTEM, "Unreal Engine\n");
            _gldDumpAppCust(14);
            return;
        }
	}
}

// ***********************************************************************

BOOL gldInitDriver(void)
{
	char szExeName[MAX_PATH];
	const char *szRendering[] = {
		"Mesa Software",
		"Direct3D RGB SW",
		"Direct3D HW",
	};
    static BOOL bWarnOnce = FALSE;

    // Already initialized?
    if (bInitialized)
        return TRUE;

    // Moved from DllMain DLL_PROCESS_ATTACH:

	// (Re-)Init defaults
	gldInitGlobals();

	// Read registry or INI file settings
	if (!dllReadRegistry(hInstanceDll)) {
        if (!bWarnOnce)
			MessageBox( NULL, "GLDirect has not been configured.\n\n"
						  "Please run the configuration program\n"
                          "before using GLDirect with applications.\n",
						  "GLDirect", MB_OK | MB_ICONWARNING);
        bWarnOnce = TRUE;
        return FALSE;
	}

	// Must do this as early as possible.
	// Need to read regkeys/ini-file first though.
	gldInitDriverPointers(glb.dwDriver);

	// Create private driver globals
	_gldDriver.CreatePrivateGlobals();

	// Overide settings with application customizations
	if (glb.bAppCustomizations)
		gldSetAppCustomizations();

	// Set the global memory type to either sysmem or vidmem
	glb.dwMemoryType = glb.bHardware ? DDSCAPS_VIDEOMEMORY : DDSCAPS_SYSTEMMEMORY;

	// Multi-threaded support overides persistant display support
	if (glb.bMultiThreaded)
		glb.bDirectDrawPersistant = glb.bPersistantBuffers = FALSE;

    // Multi-threaded support needs to be reflected in Mesa code. (DaveM)
    _gld_bMultiThreaded = glb.bMultiThreaded;

	// Start logging
    gldLogPathOption(szLogPath);
	gldLogWarnOption(glb.bMessageBoxWarnings);
	gldLogOpen((GLDLOG_loggingMethodType)dwLogging,
			  (GLDLOG_severityType)dwDebugLevel);

	// Obtain the name of the calling app
	gldLogMessage(GLDLOG_SYSTEM, "Driver           : SciTech GLDirect 5.0\n");
	GetModuleFileName(NULL, szExeName, sizeof(szExeName));
	gldLogPrintf(GLDLOG_SYSTEM, "Executable       : %s", szExeName);

	gldLogPrintf(GLDLOG_SYSTEM, "DirectDraw device: %s", glb.szDDName);
	gldLogPrintf(GLDLOG_SYSTEM, "Direct3D driver  : %s", glb.szD3DName);

	gldLogPrintf(GLDLOG_SYSTEM, "Rendering type   : %s", szRendering[glb.dwRendering]);

	gldLogPrintf(GLDLOG_SYSTEM, "Multithreaded    : %s", glb.bMultiThreaded ? "Enabled" : "Disabled");
	gldLogPrintf(GLDLOG_SYSTEM, "Display resources: %s", glb.bDirectDrawPersistant ? "Persistant" : "Instanced");
	gldLogPrintf(GLDLOG_SYSTEM, "Buffer resources : %s", glb.bPersistantBuffers ? "Persistant" : "Instanced");

	if (glb.iAppCustomisation != -1) {
		gldLogPrintf(GLDLOG_SYSTEM, "-- AppCust       : %d", glb.iAppCustomisation);
	}

	gldInitContextState();
	if (!gldBuildPixelFormatList())
		return FALSE;

    // D3D callback driver is now successfully initialized
    bInitialized = TRUE;
    // D3D callback driver is now ready to be exited
    bExited = FALSE;

    return TRUE;
}

// ***********************************************************************

void gldExitDriver(void)
{

	// Only need to clean up once per instance:
	// May be called implicitly from DLL_PROCESS_DETACH,
	// or explicitly from GLD_exitDriver().
	if (bExited)
		return;
	bExited = TRUE;

    // DDraw objects may be invalid when DLL unloads.
__try {

	// Clean-up sequence (moved from DLL_PROCESS_DETACH)
	gldReleasePixelFormatList();
	gldDeleteContextState();

	_gldDriver.DestroyPrivateGlobals();
}
__except(EXCEPTION_EXECUTE_HANDLER) {
	    gldLogPrintf(GLDLOG_WARN, "Exception raised in gldExitDriver.");
}

	// Close the log file
	gldLogClose();
}

// ***********************************************************************

int WINAPI DllMain(
	HINSTANCE hInstance,
	DWORD fdwReason,
	PVOID pvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
        // Cache DLL instance handle
        hInstanceDll = hInstance;

        // Flag that callback driver has yet to be initialized
        bInitialized = bExited = FALSE;

		// Init defaults
		gldInitGlobals();

        // Defer rest of DLL initialization to 1st WGL function call
		break;

	case DLL_PROCESS_DETACH:
		// Call exit clean-up sequence
		gldExitDriver();
		break;
	}

	return TRUE;
}

// ***********************************************************************

void APIENTRY GLD_exitDriver(void)
{
	// Call exit clean-up sequence
	gldExitDriver();
}

// ***********************************************************************

void APIENTRY GLD_reinitDriver(void)
{
	// Force init sequence again
    bInitialized = bExited = FALSE;
	gldInitDriver();
}

// ***********************************************************************

int WINAPI DllInitialize(
	HINSTANCE hInstance,
	DWORD fdwReason,
	PVOID pvReserved)
{
	// Some Watcom compiled executables require this.
	return DllMain(hInstance, fdwReason, pvReserved);
}

// ***********************************************************************

void GLD_LoadSplashScreen(int piReg, char* pszUser)
{
}

// ***********************************************************************

int APIENTRY GLD_registerGLDLicense(
	unsigned char *license)
{
	return 1;
}

// ***********************************************************************

static int GLD_validateLicense(void)
{
	return 1;
}

// ***********************************************************************

BOOL gldValidate()
{
	char *szCaption = "SciTech GLDirect Driver";
	UINT uType = MB_OK | MB_ICONEXCLAMATION;

	// (Re)build pixelformat list
	if (glb.bPixelformatsDirty)
		_gldDriver.BuildPixelformatList();

	// Check to see if we have already validated
	if (bDriverValidated && bInitialized)
		return TRUE;

    // Since all (most) the WGL functions must be validated at this point,
    // this also insure that the callback driver is completely initialized.
    if (!bInitialized)
        if (!gldInitDriver()) {
			MessageBox(NULL,
				"The GLDirect driver could not initialize.\n\n"
				"Please run the configuration program to\n"
				"properly configure the driver, or else\n"
                "re-run the installation program.", szCaption, uType);
			_exit(1); // Bail
        }

	return TRUE;
}

// ***********************************************************************

BOOL GLD_isLicensed(void)
{
    // Check if GLD is licensed to user
    if (!bDriverValidated)
        gldValidate();
    return bDriverLicensed;
}

// ***********************************************************************

