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
* Description:  Context handling.
*
*********************************************************************************/

#include "gld_context.h"

#include "gld_driver.h"

extern void _gld_mesa_warning(GLcontext *, char *);
extern void _gld_mesa_fatal(GLcontext *, char *);
void gldExitDriver(void);
BOOL gldValidate();

extern HHOOK 	hKeyHook;				// global keyboard handler hook

// TODO: Clean out old DX6-specific code from GLD 2.x CAD driver
// if it is no longer being built as part of GLDirect. (DaveM)

// ***********************************************************************

#define GLDERR_NONE     0
#define GLDERR_MEM      1
#define GLDERR_DDRAW    2
#define GLDERR_D3D      3
#define GLDERR_BPP      4
#define GLDERR_LAME     5

const char *szResourceWarning =
"GLDirect does not have enough video memory resources\n"
"to support the requested OpenGL rendering context.\n\n"
"You may have to reduce the current display resolution\n"
"to obtain satisfactory OpenGL performance.\n";

const char *szDDrawWarning =
"GLDirect is unable to initialize DirectDraw for the\n"
"requested OpenGL rendering context.\n\n"
"You will have to check the DirectX control panel\n"
"for further information.\n";

const char *szD3DWarning =
"GLDirect is unable to initialize Direct3D for the\n"
"requested OpenGL rendering context.\n\n"
"You may have to change the display mode resolution\n"
"color depth or check the DirectX control panel for\n"
"further information.\n";

const char *szBPPWarning =
"GLDirect is unable to use the selected color depth for\n"
"the requested OpenGL rendering context.\n\n"
"You will have to change the display mode resolution\n"
"color depth with the Display Settings control panel.\n";

const char *szLameWarning = 
"Your graphics adapter does not meet the\n"
"minimum specification for GLDirect.\n";

int nContextError = GLDERR_NONE;

// ***********************************************************************

extern DWORD dwLogging;

#ifdef GLD_THREADS
#pragma message("compiling GLD_CONTEXT.C vars for multi-threaded support")
CRITICAL_SECTION CriticalSection;		// for serialized access
DWORD		dwTLSCurrentContext = 0xFFFFFFFF;	// TLS index for current context
DWORD		dwTLSPixelFormat = 0xFFFFFFFF;		// TLS index for current pixel format
#endif
HGLRC		iCurrentContext = 0;		// Index of current context (static)
BOOL		bContextReady = FALSE;		// Context state ready ?

GLD_ctx		ctxlist[GLD_MAX_CONTEXTS];	// Context list

// ***********************************************************************

static BOOL bHaveWin95 = FALSE;
static BOOL bHaveWinNT = FALSE;
static BOOL bHaveWin2K = FALSE;

/****************************************************************************
REMARKS:
Detect the installed OS type.
****************************************************************************/
static void DetectOS(void)
{
    OSVERSIONINFO VersionInformation;
    LPOSVERSIONINFO lpVersionInformation = &VersionInformation;

    VersionInformation.dwOSVersionInfoSize = sizeof(VersionInformation);

	GetVersionEx(lpVersionInformation);

    switch (VersionInformation.dwPlatformId) {
    	case VER_PLATFORM_WIN32_WINDOWS:
			bHaveWin95 = TRUE;
			bHaveWinNT = FALSE;
			bHaveWin2K = FALSE;
            break;
    	case VER_PLATFORM_WIN32_NT:
			bHaveWin95 = FALSE;
			if (VersionInformation.dwMajorVersion <= 4) {
				bHaveWinNT = TRUE;
				bHaveWin2K = FALSE;
                }
            else {
				bHaveWinNT = FALSE;
				bHaveWin2K = TRUE;
                }
			break;
		case VER_PLATFORM_WIN32s:
			bHaveWin95 = FALSE;
			bHaveWinNT = FALSE;
			bHaveWin2K = FALSE;
			break;
        }
}

// ***********************************************************************

HWND hWndEvent = NULL;					// event monitor window
HWND hWndLastActive = NULL;				// last active client window
LONG __stdcall GLD_EventWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

// ***********************************************************************

// Checks if the HGLRC is valid in range of context list.
BOOL gldIsValidContext(
	HGLRC a)
{
	return ((int)a > 0 && (int)a <= GLD_MAX_CONTEXTS);
}

// ***********************************************************************

// Convert a HGLRC to a pointer into the context list.
GLD_ctx* gldGetContextAddress(
	const HGLRC a)
{
	if (gldIsValidContext(a))
		return &ctxlist[(int)a-1];
	return NULL;
}

// ***********************************************************************

// Return the current HGLRC (however it may be stored for multi-threading).
HGLRC gldGetCurrentContext(void)
{
#ifdef GLD_THREADS
	HGLRC hGLRC;
	// load from thread-specific instance
	if (glb.bMultiThreaded) {
		// protect against calls from arbitrary threads
		__try {
			hGLRC = (HGLRC)TlsGetValue(dwTLSCurrentContext);
		}
		__except(EXCEPTION_EXECUTE_HANDLER) {
			hGLRC = iCurrentContext;
		}
	}
	// load from global static var
	else {
		hGLRC = iCurrentContext;
	}
	return hGLRC;
#else
	return iCurrentContext;
#endif
}

// ***********************************************************************

// Set the current HGLRC (however it may be stored for multi-threading).
void gldSetCurrentContext(HGLRC hGLRC)
{
#ifdef GLD_THREADS
	// store in thread-specific instance
	if (glb.bMultiThreaded) {
		// protect against calls from arbitrary threads
		__try {
			TlsSetValue(dwTLSCurrentContext, (LPVOID)hGLRC);
		}
		__except(EXCEPTION_EXECUTE_HANDLER) {
			iCurrentContext = hGLRC;
		}
	}
	// store in global static var
	else {
		iCurrentContext = hGLRC;
	}
#else
	iCurrentContext = hGLRC;
#endif
}

// ***********************************************************************

// Return the current HDC only for a currently active HGLRC.
HDC gldGetCurrentDC(void)
{
	HGLRC hGLRC;
	GLD_ctx* lpCtx;

	hGLRC = gldGetCurrentContext();
	if (hGLRC) {
		lpCtx = gldGetContextAddress(hGLRC);
		return lpCtx->hDC;
	}
	return 0;
}

// ***********************************************************************

void gldInitContextState()
{
	int i;

#ifdef GLD_THREADS
	// Allocate thread local storage indexes for current context and pixel format
	dwTLSCurrentContext = TlsAlloc();
	dwTLSPixelFormat = TlsAlloc();
#endif

	gldSetCurrentContext(NULL); // No current rendering context

	 // Clear all context data
	ZeroMemory(ctxlist, sizeof(ctxlist[0]) * GLD_MAX_CONTEXTS);

	for (i=0; i<GLD_MAX_CONTEXTS; i++)
		ctxlist[i].bAllocated = FALSE; // Flag context as unused

	// This section of code crashes the dll in circumstances where the app
	// creates and destroys contexts.
/*
	// Register the class for our event monitor window
	wc.style = 0;
	wc.lpfnWndProc = GLD_EventWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "GLDIRECT";
	RegisterClass(&wc);

	// Create the non-visible window to monitor all broadcast messages
	hWndEvent = CreateWindowEx(
		WS_EX_TOOLWINDOW,"GLDIRECT","GLDIRECT",WS_POPUP,
		0,0,0,0,
		NULL,NULL,GetModuleHandle(NULL),NULL);
*/

#ifdef GLD_THREADS
	// Create a critical section object for serializing access to
	// DirectDraw and DDStereo create/destroy functions in multiple threads
	if (glb.bMultiThreaded)
		InitializeCriticalSection(&CriticalSection);
#endif

	// Context state is now initialized and ready
	bContextReady = TRUE;
}

// ***********************************************************************

void gldDeleteContextState()
{
	int i;
	static BOOL bOnceIsEnough = FALSE;

	// Only call once, from either GLD_exitDriver(), or DLL_PROCESS_DETACH
	if (bOnceIsEnough)
		return;
	bOnceIsEnough = TRUE;

	for (i=0; i<GLD_MAX_CONTEXTS; i++) {
		if (ctxlist[i].bAllocated == TRUE) {
			gldLogPrintf(GLDLOG_WARN, "** Context %i not deleted - cleaning up.", (i+1));
			gldDeleteContext((HGLRC)(i+1));
		}
	}

	// Context state is no longer ready
	bContextReady = FALSE;

    // If executed when DLL unloads, DDraw objects may be invalid.
    // So catch any page faults with this exception handler.
__try {

	// Release final DirectDraw interfaces
	if (glb.bDirectDrawPersistant) {
//		RELEASE(glb.lpGlobalPalette);
//		RELEASE(glb.lpDepth4);
//		RELEASE(glb.lpBack4);
//		RELEASE(glb.lpPrimary4);
//	    RELEASE(glb.lpDD4);
    }
}
__except(EXCEPTION_EXECUTE_HANDLER) {
    gldLogPrintf(GLDLOG_WARN, "Exception raised in gldDeleteContextState.");
}

	// Destroy our event monitor window
	if (hWndEvent) {
		DestroyWindow(hWndEvent);
		hWndEvent = hWndLastActive = NULL;
	}

#ifdef GLD_THREADS
	// Destroy the critical section object
	if (glb.bMultiThreaded)
		DeleteCriticalSection(&CriticalSection);

	// Release thread local storage indexes for current HGLRC and pixel format
	TlsFree(dwTLSPixelFormat);
	TlsFree(dwTLSCurrentContext);
#endif
}

// ***********************************************************************

// Application Window message handler interception
static LONG __stdcall gldWndProc(
	HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	GLD_ctx* 	lpCtx = NULL;
	LONG 		lpfnWndProc = 0L;
	int  		i;
//	HGLRC 		hGLRC;
	RECT 		rect;
	PAINTSTRUCT	ps;
    BOOL        bQuit = FALSE;
    BOOL        bMain = FALSE;
    LONG        rc;

    // Get the window's message handler *before* it is unhooked in WM_DESTROY

    // Is this the main window?
    if (hwnd == glb.hWndActive) {
        bMain = TRUE;
        lpfnWndProc = glb.lpfnWndProc;
    }
    // Search for GLD context matching window handle
    for (i=0; i<GLD_MAX_CONTEXTS; i++) {
	    if (ctxlist[i].hWnd == hwnd) {
	        lpCtx = &ctxlist[i];
	        lpfnWndProc = lpCtx->lpfnWndProc;
		    break;
        }
    }
	// Not one of ours...
	if (!lpfnWndProc)
	    return DefWindowProc(hwnd, msg, wParam, lParam);

    // Intercept messages amd process *before* passing on to window
	switch (msg) {
	case WM_DISPLAYCHANGE:
		glb.bPixelformatsDirty = TRUE;
		break;
	case WM_ACTIVATEAPP:
		glb.bAppActive = (BOOL)wParam;
		gldLogPrintf(GLDLOG_INFO, "Calling app has been %s", glb.bAppActive ? "activated" : "de-activated");
		break;
	case WM_ERASEBKGND:
		// Eat the GDI erase event for the GL window
        if (!lpCtx || !lpCtx->bHasBeenCurrent)
            break;
		lpCtx->bGDIEraseBkgnd = TRUE;
		return TRUE;
	case WM_PAINT:
		// Eat the invalidated update region if render scene is in progress
        if (!lpCtx || !lpCtx->bHasBeenCurrent)
            break;
		if (lpCtx->bFrameStarted) {
			if (GetUpdateRect(hwnd, &rect, FALSE)) {
				BeginPaint(hwnd, &ps);
				EndPaint(hwnd, &ps);
				ValidateRect(hwnd, &rect);
				return TRUE;
				}
			}
		break;
	}
	// Call the appropriate window message handler
	rc = CallWindowProc((WNDPROC)lpfnWndProc, hwnd, msg, wParam, lParam);

    // Intercept messages and process *after* passing on to window
	switch (msg) {
    case WM_QUIT:
	case WM_DESTROY:
        bQuit = TRUE;
		if (lpCtx && lpCtx->bAllocated) {
			gldLogPrintf(GLDLOG_WARN, "WM_DESTROY detected for HWND=%X, HDC=%X, HGLRC=%d", hwnd, lpCtx->hDC, i+1);
			gldDeleteContext((HGLRC)(i+1));
		}
		break;
#if 0
	case WM_SIZE:
		// Resize surfaces to fit window but not viewport (in case app did not bother)
        if (!lpCtx || !lpCtx->bHasBeenCurrent)
            break;
		w = LOWORD(lParam);
		h = HIWORD(lParam);
		if (lpCtx->dwWidth < w || lpCtx->dwHeight < h) {
			if (!gldWglResizeBuffers(lpCtx->glCtx, TRUE))
                 gldWglResizeBuffers(lpCtx->glCtx, FALSE);
        }
		break;
#endif
    }

    // If the main window is quitting, then so should we...
    if (bMain && bQuit) {
		gldLogPrintf(GLDLOG_SYSTEM, "shutting down after WM_DESTROY detected for main HWND=%X", hwnd);
        gldDeleteContextState();
        gldExitDriver();
    }

    return rc;
}

// ***********************************************************************

// Driver Window message handler
static LONG __stdcall GLD_EventWndProc(
	HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (msg) {
        // May be sent by splash screen dialog on exit
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_ACTIVE && glb.hWndActive) {
                SetForegroundWindow(glb.hWndActive);
                return 0;
                }
            break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ***********************************************************************

// Intercepted Keyboard handler for detecting hot keys.
LRESULT CALLBACK gldKeyProc(
	int code,
	WPARAM wParam,
	LPARAM lParam)
{
	return CallNextHookEx(hKeyHook, code, wParam, lParam);
}

// ***********************************************************************

HWND hWndMatch;

// Window handle enumeration procedure.
BOOL CALLBACK gldEnumChildProc(
    HWND hWnd,
    LPARAM lParam)
{
    RECT rect;

    // Find window handle with matching client rect.
    GetClientRect(hWnd, &rect);
    if (EqualRect(&rect, (RECT*)lParam)) {
        hWndMatch = hWnd;
        return FALSE;
        }
    // Continue with next child window.
    return TRUE;
}

// ***********************************************************************

// Find window handle with matching client rect.
HWND gldFindWindowRect(
    RECT* pRect)
{
    hWndMatch = NULL;
    EnumChildWindows(GetForegroundWindow(), gldEnumChildProc, (LPARAM)pRect);
    return hWndMatch;
}

// ***********************************************************************

static BOOL IsDevice(
	DWORD *lpDeviceIdList,
	DWORD dwDeviceId,
	int count)
{
	int i;

	for (i=0; i<count; i++)
		if (dwDeviceId == lpDeviceIdList[i])
			return TRUE;

	return FALSE;
}

// ***********************************************************************

BOOL gldCreateContextBuffers(
	HDC a,
	GLD_ctx *lpCtx,
	BOOL bFallback)
{
	GLenum				bDoubleBuffer;	// TRUE if double buffer required
	GLenum				bDepthBuffer;	// TRUE if depth buffer required

	const PIXELFORMATDESCRIPTOR	*lpPFD = &lpCtx->lpPF->pfd;

	// Vars for Mesa visual
	DWORD				dwDepthBits		= 0;
	DWORD				dwStencilBits	= 0;
	DWORD				dwAlphaBits		= 0;
	DWORD				bAlphaSW		= GL_FALSE;
	DWORD				bDouble			= GL_FALSE;

	BOOL				bFullScrnWin	= FALSE;	// fullscreen-size window ?
	DWORD				dwMemoryType 	= (bFallback) ? DDSCAPS_SYSTEMMEMORY : glb.dwMemoryType;
	BOOL				bBogusWindow	= FALSE;	// non-drawable window ?
	DWORD               dwColorRef      = 0;        // GDI background color

#define GLDLOG_CRITICAL_OR_WARN	(bFallback ? GLDLOG_CRITICAL : GLDLOG_WARN)

	gldLogPrintf(GLDLOG_SYSTEM, "gldCreateContextBuffers for HDC=%X", a);
    nContextError = GLDERR_NONE;

#ifdef GLD_THREADS
	// Serialize access to DirectDraw object creation or DDS start
	if (glb.bMultiThreaded)
		EnterCriticalSection(&CriticalSection);
#endif

	// Check for back buffer
	bDoubleBuffer = GL_TRUE; //(lpPFD->dwFlags & PFD_DOUBLEBUFFER) ? GL_TRUE : GL_FALSE;
	// Since we always do back buffering, check if we emulate front buffering
	lpCtx->EmulateSingle =
		(lpPFD->dwFlags & PFD_DOUBLEBUFFER) ? FALSE : TRUE;
#if 0	// Don't have to mimic MS OpenGL behavior for front-buffering (DaveM)
	lpCtx->EmulateSingle |=
		(lpPFD->dwFlags & PFD_SUPPORT_GDI) ? TRUE : FALSE;
#endif

	// Check for depth buffer
	bDepthBuffer = (lpPFD->cDepthBits) ? GL_TRUE : GL_FALSE;

	lpCtx->bDoubleBuffer = bDoubleBuffer;
	lpCtx->bDepthBuffer = bDepthBuffer;

	// Set the Fullscreen flag for the context.
//	lpCtx->bFullscreen = glb.bFullscreen;

	// Obtain the dimensions of the rendering window
	lpCtx->hDC = a; // Cache DC
	lpCtx->hWnd = WindowFromDC(lpCtx->hDC);
	// Check for non-window DC = memory DC ?
	if (lpCtx->hWnd == NULL) {
        // bitmap memory contexts are always single-buffered
        lpCtx->EmulateSingle = TRUE;
		bBogusWindow = TRUE;
		gldLogPrintf(GLDLOG_INFO, "Non-Window Memory Device Context");
		if (GetClipBox(lpCtx->hDC, &lpCtx->rcScreenRect) == ERROR) {
			gldLogMessage(GLDLOG_WARN, "GetClipBox failed in gldCreateContext\n");
			SetRect(&lpCtx->rcScreenRect, 0, 0, 0, 0);
		}
	}
	else if (!GetClientRect(lpCtx->hWnd, &lpCtx->rcScreenRect)) {
		bBogusWindow = TRUE;
		gldLogMessage(GLDLOG_WARN, "GetClientRect failed in gldCreateContext\n");
		SetRect(&lpCtx->rcScreenRect, 0, 0, 0, 0);
	}
	lpCtx->dwWidth = lpCtx->rcScreenRect.right - lpCtx->rcScreenRect.left;
	lpCtx->dwHeight = lpCtx->rcScreenRect.bottom - lpCtx->rcScreenRect.top;

	gldLogPrintf(GLDLOG_INFO, "Input window %X: w=%i, h=%i",
							lpCtx->hWnd, lpCtx->dwWidth, lpCtx->dwHeight);

	// What if app only zeroes one dimension instead of both? (DaveM)
	if ( (lpCtx->dwWidth == 0) || (lpCtx->dwHeight == 0) ) {
		// Make the buffer size something sensible
		lpCtx->dwWidth = 8;
		lpCtx->dwHeight = 8;
	}

	// Set defaults
	lpCtx->dwModeWidth = lpCtx->dwWidth;
	lpCtx->dwModeHeight = lpCtx->dwHeight;
/*
	// Find best display mode for fullscreen
	if (glb.bFullscreen || !glb.bPrimary) {
		gldChooseDisplayMode(lpCtx);
	}
*/
	// Misc initialisation
	lpCtx->bCanRender = FALSE; // No rendering allowed yet
	lpCtx->bSceneStarted = FALSE;
	lpCtx->bFrameStarted = FALSE;

	// Detect OS (specifically 'Windows 2000' or 'Windows XP')
	DetectOS();

	// NOTE: WinNT not supported
	gldLogPrintf(GLDLOG_INFO, "OS: %s", bHaveWin95 ? "Win9x" : (bHaveWin2K ? "Win2000/XP" : "Unsupported") );

	// Test for Fullscreen
	if (bHaveWin95) { // Problems with fullscreen on Win2K/XP
		if ((GetSystemMetrics(SM_CXSCREEN) == lpCtx->dwWidth) &&
			(GetSystemMetrics(SM_CYSCREEN) == lpCtx->dwHeight))
		{
			// Workaround for some apps that crash when going fullscreen.
			//lpCtx->bFullscreen = TRUE;
		}
		
	}

	_gldDriver.CreateDrawable(lpCtx, glb.bDirectDrawPersistant, glb.bPersistantBuffers);

	gldLogPrintf(GLDLOG_INFO, "Window: w=%i, h=%i (%s)",
							lpCtx->dwWidth,
							lpCtx->dwHeight,
							lpCtx->bFullscreen ? "fullscreen" : "windowed");

	//
	//	Now create the Mesa context
	//

	// Create the Mesa visual
	if (lpPFD->cDepthBits)
		dwDepthBits = 16;
	if (lpPFD->cStencilBits)
		dwStencilBits = 8;
	if (lpPFD->cAlphaBits) {
		dwAlphaBits = 8;
		bAlphaSW = GL_TRUE;
	}
	if (lpPFD->dwFlags & PFD_DOUBLEBUFFER)
		bDouble = GL_TRUE;
//	lpCtx->EmulateSingle =
//		(lpPFD->dwFlags & PFD_DOUBLEBUFFER) ? FALSE : TRUE;

	lpCtx->glVis = _mesa_create_visual(
		GL_TRUE,				// RGB mode
		bDouble,				// double buffer
		GL_FALSE,				// stereo
		lpPFD->cRedBits,
		lpPFD->cGreenBits,
		lpPFD->cBlueBits,
		dwAlphaBits,
		0,						// index bits
		dwDepthBits,
		dwStencilBits,
		lpPFD->cAccumRedBits,	// accum bits
		lpPFD->cAccumGreenBits,	// accum bits
		lpPFD->cAccumBlueBits,	// accum bits
		lpPFD->cAccumAlphaBits,	// accum alpha bits
		1						// num samples
		);

	if (lpCtx->glVis == NULL) {
		gldLogMessage(GLDLOG_CRITICAL_OR_WARN, "gl_create_visual failed\n");
		goto return_with_error;
	}

	lpCtx->glCtx = _mesa_create_context(lpCtx->glVis, NULL, (void *)lpCtx, GL_TRUE);

	if (lpCtx->glCtx == NULL) {
		gldLogMessage(GLDLOG_CRITICAL_OR_WARN, "gl_create_context failed\n");
		goto return_with_error;
	}

	// Initialise Scissor rectangle.
	// Mesa should do this, but does not!
	// Mesa sets scissor:x,y,width,height all to zero,
	// GL spec states that scissor rect should be same as newly created render buffer.
	lpCtx->glCtx->Scissor.Width		= lpCtx->dwWidth;
	lpCtx->glCtx->Scissor.Height	= lpCtx->dwHeight;


	// Create the Mesa framebuffer
	lpCtx->glBuffer = _mesa_create_framebuffer(
		lpCtx->glVis,
		lpCtx->glVis->depthBits > 0,
		lpCtx->glVis->stencilBits > 0,
		lpCtx->glVis->accumRedBits > 0,
		GL_FALSE //swalpha
		);

	if (lpCtx->glBuffer == NULL) {
		gldLogMessage(GLDLOG_CRITICAL_OR_WARN, "gl_create_framebuffer failed\n");
		goto return_with_error;
	}

	// Disable Mesa TnL codepath
/*
	// Init Mesa internals
	_swrast_CreateContext( lpCtx->glCtx );
	_ac_CreateContext( lpCtx->glCtx );
	_tnl_CreateContext( lpCtx->glCtx );
	_swsetup_CreateContext( lpCtx->glCtx );
*/
	_gldDriver.InitialiseMesa(lpCtx);
	
	lpCtx->glCtx->imports.warning	= _gld_mesa_warning;
	lpCtx->glCtx->imports.fatal		= _gld_mesa_fatal;

	// ** If we have made it to here then we can enable rendering **
	lpCtx->bCanRender = TRUE;

//	gldLogMessage(GLDLOG_SYSTEM, "gldCreateContextBuffers succeded\n");

#ifdef GLD_THREADS
	// Release serialized access
	if (glb.bMultiThreaded)
		LeaveCriticalSection(&CriticalSection);
#endif

	return TRUE;

return_with_error:
	// Clean up before returning.
	// This is critical for secondary devices.

	lpCtx->bCanRender = FALSE;

	// Destroy the Mesa context
	if (lpCtx->glBuffer)
		_mesa_destroy_framebuffer(lpCtx->glBuffer);
	if (lpCtx->glCtx)
		_mesa_destroy_context(lpCtx->glCtx);
	if (lpCtx->glVis)
		_mesa_destroy_visual(lpCtx->glVis);

	// Destroy driver data
	_gldDriver.DestroyDrawable(lpCtx);

	lpCtx->bAllocated = FALSE;

#ifdef GLD_THREADS
	// Release serialized access
	if (glb.bMultiThreaded)
		LeaveCriticalSection(&CriticalSection);
#endif

	return FALSE;

#undef GLDLOG_CRITICAL_OR_WARN
}

// ***********************************************************************

HGLRC gldCreateContext(
	HDC a,
	const GLD_pixelFormat *lpPF)
{
	int i;
	HGLRC				hGLRC;
	GLD_ctx*			lpCtx;
	static BOOL			bWarnOnce = TRUE;
	DWORD				dwThreadId = GetCurrentThreadId();
    char                szMsg[256];
    HWND                hWnd;
    LONG                lpfnWndProc;

	// Validate license
	if (!gldValidate())
		return NULL;

	// Is context state ready ?
	if (!bContextReady)
		return NULL;

	gldLogPrintf(GLDLOG_SYSTEM, "gldCreateContext for HDC=%X, ThreadId=%X", a, dwThreadId);

	// Find next free context.
	// Also ensure that only one Fullscreen context is created at any one time.
	hGLRC = 0; // Default to Not Found
	for (i=0; i<GLD_MAX_CONTEXTS; i++) {
		if (ctxlist[i].bAllocated) {
			if (/*glb.bFullscreen && */ctxlist[i].bFullscreen)
				break;
		} else {
			hGLRC = (HGLRC)(i+1);
			break;
		}
	}

	// Bail if no GLRC was found
	if (!hGLRC)
		return NULL;

	// Set the context pointer
	lpCtx = gldGetContextAddress(hGLRC);
	// Make sure that context is zeroed before we do anything.
	// MFC and C++ apps call wglCreateContext() and wglDeleteContext() multiple times,
	// even though only one context is ever used by the app, so keep it clean. (DaveM)
	ZeroMemory(lpCtx, sizeof(GLD_ctx));
	lpCtx->bAllocated = TRUE;
	// Flag that buffers need creating on next wglMakeCurrent call.
	lpCtx->bHasBeenCurrent = FALSE;
	lpCtx->lpPF = (GLD_pixelFormat *)lpPF;	// cache pixel format
	lpCtx->bCanRender = FALSE;

	// Create all the internal resources here, not in gldMakeCurrent().
	// We do a re-size check in gldMakeCurrent in case of re-allocations. (DaveM)
	// We now try context allocations twice, first with video memory,
	// then again with system memory. This is similar to technique
	// used for gldWglResizeBuffers(). (DaveM)
	if (lpCtx->bHasBeenCurrent == FALSE) {
		if (!gldCreateContextBuffers(a, lpCtx, FALSE)) {
			if (glb.bMessageBoxWarnings && bWarnOnce && dwLogging) {
				bWarnOnce = FALSE;
                switch (nContextError) {
                   case GLDERR_DDRAW: strcpy(szMsg, szDDrawWarning); break;
                   case GLDERR_D3D: strcpy(szMsg, szD3DWarning); break;
                   case GLDERR_MEM: strcpy(szMsg, szResourceWarning); break;
                   case GLDERR_BPP: strcpy(szMsg, szBPPWarning); break;
                   case GLDERR_LAME: strcpy(szMsg, szLameWarning); break;
                   default: strcpy(szMsg, "");
                }
                if (strlen(szMsg))
                    MessageBox(NULL, szMsg, "GLDirect", MB_OK | MB_ICONWARNING);
			}
            // Only need to try again if memory error
            if (nContextError == GLDERR_MEM) {
			    gldLogPrintf(GLDLOG_WARN, "gldCreateContext failed 1st time with video memory");
			    if (!gldCreateContextBuffers(a, lpCtx, TRUE)) {
				    gldLogPrintf(GLDLOG_ERROR, "gldCreateContext failed 2nd time with system memory");
				    return NULL;
			    }
            }
            else {
			    gldLogPrintf(GLDLOG_ERROR, "gldCreateContext failed");
                return NULL;
            }
		}
	}

	// Now that we have a hWnd, we can intercept the WindowProc.
    hWnd = lpCtx->hWnd;
    if (hWnd) {
		// Only hook individual window handler once if not hooked before.
		lpfnWndProc = GetWindowLong(hWnd, GWL_WNDPROC);
		if (lpfnWndProc != (LONG)gldWndProc) {
			lpCtx->lpfnWndProc = lpfnWndProc;
			SetWindowLong(hWnd, GWL_WNDPROC, (LONG)gldWndProc);
			}
        // Find the parent window of the app too.
        if (glb.hWndActive == NULL) {
            while (hWnd != NULL) {
                glb.hWndActive = hWnd;
                hWnd = GetParent(hWnd);
            }
            // Hook the parent window too.
            lpfnWndProc = GetWindowLong(glb.hWndActive, GWL_WNDPROC);
            if (glb.hWndActive == lpCtx->hWnd)
                glb.lpfnWndProc = lpCtx->lpfnWndProc;
            else if (lpfnWndProc != (LONG)gldWndProc)
                glb.lpfnWndProc = lpfnWndProc;
            if (glb.lpfnWndProc)
                SetWindowLong(glb.hWndActive, GWL_WNDPROC, (LONG)gldWndProc);
        }
    }

	gldLogPrintf(GLDLOG_SYSTEM, "gldCreateContext succeeded for HGLRC=%d", (int)hGLRC);

	return hGLRC;
}

// ***********************************************************************
// Make a GLDirect context current
// Used by wgl functions and gld functions
BOOL gldMakeCurrent(
	HDC a,
	HGLRC b)
{
	int context;
	GLD_ctx* lpCtx;
	HWND hWnd;
	BOOL bNeedResize = FALSE;
	BOOL bWindowChanged, bContextChanged;
	DWORD dwThreadId = GetCurrentThreadId();

	context = (int)b; // This is as a result of STRICT!

	// Workaround for Unreal engine games.
	if (!bContextReady && (context == 0) && (a == 0)) {
		return TRUE;
	}

	// Validate license
	if (!gldValidate())
		return FALSE;

	// Is context state ready ?
	if (!bContextReady)
		return FALSE;

#if DEBUG
#if 0
	// PITA
	gldLogPrintf(GLDLOG_SYSTEM, "gldMakeCurrent: HDC=%X, HGLRC=%d, ThreadId=%X", a, context, dwThreadId);
#endif
#endif

	// If the HGLRC is NULL then make no context current;
	// Ditto if the HDC is NULL either. (DaveM)
	if (context == 0 || a == 0) {
		// Corresponding Mesa operation
		_mesa_make_current(NULL, NULL);
		gldSetCurrentContext(0);
		return TRUE;
	}

	// Make sure the HGLRC is in range
	if ((context > GLD_MAX_CONTEXTS) || (context < 0)) {
		gldLogMessage(GLDLOG_ERROR, "gldMakeCurrent: HGLRC out of range\n");
		return FALSE;
	}

	// Find address of context and make sure that it has been allocated
	lpCtx = gldGetContextAddress(b);
	if (!lpCtx->bAllocated) {
		gldLogMessage(GLDLOG_ERROR, "gldMakeCurrent: Context not allocated\n");
//		return FALSE;
		return TRUE; // HACK: Shuts up "WebLab Viewer Pro". KeithH
	}

#ifdef GLD_THREADS
	// Serialize access to DirectDraw or DDS operations
	if (glb.bMultiThreaded)
		EnterCriticalSection(&CriticalSection);
#endif

	// Check if window has changed
	hWnd = (a != lpCtx->hDC) ? WindowFromDC(a) : lpCtx->hWnd;
	bWindowChanged = (hWnd != lpCtx->hWnd) ? TRUE : FALSE;
	bContextChanged = (b != gldGetCurrentContext()) ? TRUE : FALSE;

	// If the window has changed, make sure the clipper is updated. (DaveM)
	if (glb.bDirectDrawPersistant && !lpCtx->bFullscreen && (bWindowChanged || bContextChanged)) {
		lpCtx->hWnd = hWnd;
	}

	// Make sure hDC and hWnd is current. (DaveM)
	// Obtain the dimensions of the rendering window
	lpCtx->hDC = a; // Cache DC
	lpCtx->hWnd = hWnd;
	hWndLastActive = hWnd;

	// Check for non-window DC = memory DC ?
	if (hWnd == NULL) {
		if (GetClipBox(a, &lpCtx->rcScreenRect) == ERROR) {
			gldLogMessage(GLDLOG_WARN, "GetClipBox failed in gldMakeCurrent\n");
			SetRect(&lpCtx->rcScreenRect, 0, 0, 0, 0);
		}
	}
	else if (!GetClientRect(lpCtx->hWnd, &lpCtx->rcScreenRect)) {
		gldLogMessage(GLDLOG_WARN, "GetClientRect failed in gldMakeCurrent\n");
		SetRect(&lpCtx->rcScreenRect, 0, 0, 0, 0);
	}
	// Check if buffers need to be re-sized;
	// If so, wait until Mesa GL stuff is setup before re-sizing;
	if (lpCtx->dwWidth != lpCtx->rcScreenRect.right - lpCtx->rcScreenRect.left ||
		lpCtx->dwHeight != lpCtx->rcScreenRect.bottom - lpCtx->rcScreenRect.top)
		bNeedResize = TRUE;

	// Now we can update our globals
	gldSetCurrentContext(b);

	// Corresponding Mesa operation
	_mesa_make_current(lpCtx->glCtx, lpCtx->glBuffer);
	lpCtx->glCtx->Driver.UpdateState(lpCtx->glCtx, _NEW_ALL);
	if (bNeedResize) {
		// Resize buffers (Note Mesa GL needs to be setup beforehand);
		// Resize Mesa internal buffer too via glViewport() command,
		// which subsequently calls gldWglResizeBuffers() too.
		lpCtx->glCtx->Driver.Viewport(lpCtx->glCtx, 0, 0, lpCtx->dwWidth, lpCtx->dwHeight);
		lpCtx->bHasBeenCurrent = TRUE;
	}

	// Don't want to spit this out *every* frame...
	//gldLogPrintf(GLDLOG_SYSTEM, "gldMakeCurrent: width = %d, height = %d", lpCtx->dwWidth, lpCtx->dwHeight);

	// We have to clear D3D back buffer and render state if emulated front buffering
	// for different window (but not context) like in Solid Edge.
	if (glb.bDirectDrawPersistant && glb.bPersistantBuffers
		&& (bWindowChanged /* || bContextChanged */) && lpCtx->EmulateSingle) {
//		IDirect3DDevice8_EndScene(lpCtx->pDev);
//		lpCtx->bSceneStarted = FALSE;
		lpCtx->glCtx->Driver.Clear(lpCtx->glCtx, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
			GL_TRUE, 0, 0, lpCtx->dwWidth, lpCtx->dwHeight);
	}

	// The first time we call MakeCurrent we set the initial viewport size
	if (lpCtx->bHasBeenCurrent == FALSE) {
		lpCtx->glCtx->Driver.Viewport(lpCtx->glCtx, 0, 0, lpCtx->dwWidth, lpCtx->dwHeight);
		// Send a Paint message to the app by invalidating and updating the window
		InvalidateRect(lpCtx->hWnd, NULL, TRUE);
		UpdateWindow(lpCtx->hWnd);
	}

	lpCtx->bHasBeenCurrent = TRUE;

#ifdef GLD_THREADS
	// Release serialized access
	if (glb.bMultiThreaded)
		LeaveCriticalSection(&CriticalSection);
#endif

	return TRUE;
}

// ***********************************************************************

BOOL gldDeleteContext(
	HGLRC a)
{
	GLD_ctx* lpCtx;
	DWORD dwThreadId = GetCurrentThreadId();
    char argstr[256];

	// Is context state ready ?
	if (!bContextReady)
//		return FALSE;
		return TRUE; // Workaround for Unreal engine games.

	gldLogPrintf(GLDLOG_SYSTEM, "gldDeleteContext: Deleting context HGLRC=%d, ThreadId=%X", (int)a, dwThreadId);

	// Make sure the HGLRC is in range
	if (((int) a> GLD_MAX_CONTEXTS) || ((int)a < 0)) {
		gldLogMessage(GLDLOG_ERROR, "gldDeleteCurrent: HGLRC out of range\n");
		return FALSE;
	}

	// Make sure context is valid
	lpCtx = gldGetContextAddress(a);
	if (!lpCtx->bAllocated) {
		gldLogPrintf(GLDLOG_WARN, "Tried to delete unallocated context HGLRC=%d", (int)a);
//		return FALSE;
		return TRUE; // HACK: Shuts up "WebLab Viewer Pro". KeithH
	}

	// Make sure context is de-activated
	if (a == gldGetCurrentContext()) {
		gldLogPrintf(GLDLOG_WARN, "gldDeleteContext: context HGLRC=%d still active", (int)a);
		gldMakeCurrent(NULL, NULL);
	}

#ifdef GLD_THREADS
	// Serialize access to DirectDraw or DDS operations
	if (glb.bMultiThreaded)
		EnterCriticalSection(&CriticalSection);
#endif

	// We are about to destroy all Direct3D objects.
	// Therefore we must disable rendering
	lpCtx->bCanRender = FALSE;

	// This exception handler was installed to catch some
	// particularly nasty apps. Console apps that call exit()
	// fall into this catagory (i.e. Win32 Glut).

    // VC cannot successfully implement multiple exception handlers
    // if more than one exception occurs. Therefore reverting back to
    // single exception handler as Keith originally had it. (DaveM)

#define WARN_MESSAGE(p) strcpy(argstr, (#p));
#define SAFE_RELEASE(p) WARN_MESSAGE(p); RELEASE(p);

__try {
    WARN_MESSAGE(gl_destroy_framebuffer);
	if (lpCtx->glBuffer)
		_mesa_destroy_framebuffer(lpCtx->glBuffer);
    WARN_MESSAGE(gl_destroy_context);
	if (lpCtx->glCtx)
		_mesa_destroy_context(lpCtx->glCtx);
    WARN_MESSAGE(gl_destroy_visual);
	if (lpCtx->glVis)
		_mesa_destroy_visual(lpCtx->glVis);

	_gldDriver.DestroyDrawable(lpCtx);
}
__except(EXCEPTION_EXECUTE_HANDLER) {
    gldLogPrintf(GLDLOG_WARN, "Exception raised in gldDeleteContext: %s", argstr);
}

	// Restore the window message handler because this context may be used
	// again by another window with a *different* message handler. (DaveM)
	if (lpCtx->lpfnWndProc) {
		SetWindowLong(lpCtx->hWnd, GWL_WNDPROC, (LONG)lpCtx->lpfnWndProc);
		lpCtx->lpfnWndProc = (LONG)NULL;
		}

	lpCtx->bAllocated = FALSE; // This context is now free for use

#ifdef GLD_THREADS
	// Release serialized access
	if (glb.bMultiThreaded)
		LeaveCriticalSection(&CriticalSection);
#endif

	return TRUE;
}

// ***********************************************************************

BOOL gldSwapBuffers(
	HDC hDC)
{
	RECT		rSrcRect;	// Source rectangle

//	DWORD		dwThreadId = GetCurrentThreadId();
	HGLRC		hGLRC	= gldGetCurrentContext();
	GLD_ctx		*lpCtx	= gldGetContextAddress(hGLRC);
	HWND		hWnd;

	int 		x,y,w,h;	// for memory DC BitBlt

	if (!lpCtx) {
		return TRUE; //FALSE; // No current context
	}

	if (!lpCtx->bCanRender) {
		// Don't return false else some apps will bail.
		return TRUE;
	}

	hWnd = lpCtx->hWnd;
	if (hDC != lpCtx->hDC) {
		gldLogPrintf(GLDLOG_WARN, "gldSwapBuffers: HDC=%X does not match HDC=%X for HGLRC=%d", hDC, lpCtx->hDC, hGLRC);
		hWnd = WindowFromDC(hDC);
	}

	// Check for non-window DC = memory DC ?
	if (hWnd == NULL) {
		if (GetClipBox(hDC, &rSrcRect) == ERROR)
			return TRUE;
		// Use GDI BitBlt instead from compatible DirectDraw DC
		x = rSrcRect.left;
		y = rSrcRect.top;
		w = rSrcRect.right - rSrcRect.left;
		h = rSrcRect.bottom - rSrcRect.top;
		return TRUE;
	}

	// Bail if window client region is not drawable, like in Solid Edge
	if (!IsWindow(hWnd) /* || !IsWindowVisible(hWnd) */ || !GetClientRect(hWnd, &rSrcRect))
		return TRUE;

#ifdef GLD_THREADS
	// Serialize access to DirectDraw or DDS operations
	if (glb.bMultiThreaded)
		EnterCriticalSection(&CriticalSection);
#endif

	// Notify Mesa of impending swap, so Mesa can flush internal buffers.
	_mesa_notifySwapBuffers(lpCtx->glCtx);
	// Now perform driver buffer swap
	_gldDriver.SwapBuffers(lpCtx, hDC, hWnd);

#ifdef GLD_THREADS
	// Release serialized access
	if (glb.bMultiThreaded)
		LeaveCriticalSection(&CriticalSection);
#endif

	// Render frame is completed
	ValidateRect(hWnd, NULL);
	lpCtx->bFrameStarted = FALSE;

	return TRUE;
}

// ***********************************************************************
