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
* Environment:  Windows 9x/2000/XP/XBox (Win32)
*
* Description:  GLDirect Direct3D 9.x WGL (WindowsGL)
*
*********************************************************************************/

#include "gld_context.h"
#include "gld_driver.h"
#include "gld_dxerr9.h"
#include "gldirect5.h"

// Copied from gld_context.c
#define GLDERR_NONE     0
#define GLDERR_MEM      1
#define GLDERR_DDRAW    2
#define GLDERR_D3D      3
#define GLDERR_BPP      4
#define GLDERR_LAME     5

// This external var keeps track of any error
extern int nContextError;
extern char *szLameWarning;

#define GLDLOG_CRITICAL_OR_WARN	GLDLOG_CRITICAL

extern void _gld_mesa_warning(GLcontext *, char *);
extern void _gld_mesa_fatal(GLcontext *, char *);

HRESULT _gldDrawPixels(
	GLcontext *ctx,
	BOOL bChromakey,	// Alpha test for glBitmap() images
	GLint x,			// GL x position
	GLint y,			// GL y position (needs flipping)
	GLsizei width,		// Width of input image
	GLsizei height,		// Height of input image
	IDirect3DSurface9 *pImage);

BOOL GLD_isLicensed(void);
BOOL GLD_isTimedOut(void);

//---------------------------------------------------------------------------

static char	szColorDepthWarning[] =
"GLDirect does not support the current desktop\n\
color depth.\n\n\
You may need to change the display resolution to\n\
16 bits per pixel or higher color depth using\n\
the Windows Display Settings control panel\n\
before running this OpenGL application.\n";

// The only depth-stencil formats currently supported by Direct3D
// Surface Format	Depth	Stencil		Total Bits
// D3DFMT_D32		32		-			32
// D3DFMT_D15S1		15		1			16
// D3DFMT_D24S8		24		8			32
// D3DFMT_D16		16		-			16
// D3DFMT_D24X8		24		-			32
// D3DFMT_D24X4S4	24		4			32

// This pixel format will be used as a template when compiling the list
// of pixel formats supported by the hardware. Many fields will be
// filled in at runtime.
// PFD flag defaults are upgraded to match ChoosePixelFormat() -- DaveM
static GLD_pixelFormat pfTemplateHW =
{
    {
	sizeof(PIXELFORMATDESCRIPTOR),	// Size of the data structure
		1,							// Structure version - should be 1
									// Flags:
		PFD_DRAW_TO_WINDOW |		// The buffer can draw to a window or device surface.
		PFD_DRAW_TO_BITMAP |		// The buffer can draw to a bitmap. (DaveM)
		PFD_SUPPORT_GDI |			// The buffer supports GDI drawing. (DaveM)
		PFD_SUPPORT_OPENGL |		// The buffer supports OpenGL drawing.
		PFD_DOUBLEBUFFER |			// The buffer is double-buffered.
		0,							// Placeholder for easy commenting of above flags
		PFD_TYPE_RGBA,				// Pixel type RGBA.
		16,							// Total colour bitplanes (excluding alpha bitplanes)
		5, 0,						// Red bits, shift
		5, 0,						// Green bits, shift
		5, 0,						// Blue bits, shift
		0, 0,						// Alpha bits, shift (destination alpha)
		0,							// Accumulator bits (total)
		0, 0, 0, 0,					// Accumulator bits: Red, Green, Blue, Alpha
		0,							// Depth bits
		0,							// Stencil bits
		0,							// Number of auxiliary buffers
		0,							// Layer type
		0,							// Specifies the number of overlay and underlay planes.
		0,							// Layer mask
		0,							// Specifies the transparent color or index of an underlay plane.
		0							// Damage mask
	},
	D3DFMT_UNKNOWN,	// No depth/stencil buffer
};

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

typedef struct {
	HINSTANCE			hD3D9DLL;			// Handle to d3d9.dll
	FNDIRECT3DCREATE9	fnDirect3DCreate9;	// Direct3DCreate9 function prototype
	BOOL				bDirect3D;			// Persistant Direct3D9 exists
	BOOL				bDirect3DDevice;	// Persistant Direct3DDevice9 exists
	IDirect3D9			*pD3D;				// Persistant Direct3D9
	IDirect3DDevice9	*pDev;				// Persistant Direct3DDevice9
} GLD_dx9_globals;

// These are "global" to all DX9 contexts. KeithH
static GLD_dx9_globals dx9Globals;

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

BOOL gldGetDXErrorString_DX(
	HRESULT hr,
	char *buf,
	int nBufSize)
{
	//
	// Return a string describing the input HRESULT error code
	//

	const char *pStr = DXGetErrorString(hr);

	if (pStr == NULL)
		return FALSE;

	if (strlen(pStr) > nBufSize)
		strncpy(buf, pStr, nBufSize);
	else
		strcpy(buf, pStr);

	return TRUE;
}

//---------------------------------------------------------------------------

static D3DMULTISAMPLE_TYPE _gldGetDeviceMultiSampleType(
	IDirect3D9 *pD3D9,
	D3DFORMAT SurfaceFormat,
	D3DDEVTYPE d3dDevType,
	BOOL Windowed)
{
	int			i;
	HRESULT		hr;

	if (glb.dwMultisample == GLDS_MULTISAMPLE_NONE)
		return D3DMULTISAMPLE_NONE;

	if (glb.dwMultisample == GLDS_MULTISAMPLE_FASTEST) {
		// Find fastest multisample
		for (i=2; i<17; i++) {
			hr = IDirect3D9_CheckDeviceMultiSampleType(
					pD3D9,
					glb.dwAdapter,
					d3dDevType,
					SurfaceFormat,
					Windowed,
					(D3DMULTISAMPLE_TYPE)i,
					NULL);
			if (SUCCEEDED(hr)) {
				return (D3DMULTISAMPLE_TYPE)i;
			}
		}
	} else {
		// Find nicest multisample
		for (i=16; i>1; i--) {
			hr = IDirect3D9_CheckDeviceMultiSampleType(
					pD3D9,
					glb.dwAdapter,
					d3dDevType,
					SurfaceFormat,
					Windowed,
					(D3DMULTISAMPLE_TYPE)i,
					NULL);
			if (SUCCEEDED(hr)) {
				return (D3DMULTISAMPLE_TYPE)i;
			}
		}
	}

	// Nothing found - return default
	return D3DMULTISAMPLE_NONE;
}

//---------------------------------------------------------------------------

void _gldDestroyPrimitiveBuffer(
	GLD_driver_dx9 *gld)
{
	if (gld == NULL)
		return;

	SAFE_RELEASE(gld->pVB);
	SAFE_FREE(gld->pPrim);

	// Init vars
	gld->dwMaxVBVerts = gld->dwMaxPrimVerts = 0;
	gld->dwFirstVBVert = gld->dwNextVBVert = 0;
	gld->dwPrimVert = 0;
}

//---------------------------------------------------------------------------

void gldResetPrimitiveBuffer(
	GLD_driver_dx9 *gld)
{
	gld->dwFirstVBVert = gld->dwNextVBVert = 0;
	gld->dwPrimVert = 0;
}

//---------------------------------------------------------------------------

HRESULT _gldCreatePrimitiveBuffer(
	GLD_driver_dx9 *gld)
{
	HRESULT		hr;
	char		*szCreateVertexBufferFailed = "CreateVertexBuffer failed";
	DWORD		dwUsage;

	if (gld == NULL)
		return E_FAIL;

	// Create a system-memory buffer to hold the vertices of the current primitive.
	gld->dwMaxPrimVerts	= GLD_PRIM_BLOCK_SIZE;
	gld->pPrim = malloc(GLD_4D_VERTEX_SIZE * gld->dwMaxPrimVerts);
	if (gld->pPrim == NULL)
		return E_OUTOFMEMORY;
	gld->dwPrimVert = 0;

	// Create a Direct3D Vertex Buffer to hold vertices to be passed to hardware.
	gld->dwMaxVBVerts	= 65535;
	dwUsage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;	// We will lock frequently and never read from buffer (write only).
	if (!gld->bHasHWTnL)
		dwUsage	|= D3DUSAGE_SOFTWAREPROCESSING;
	hr = IDirect3DDevice9_CreateVertexBuffer(
		gld->pDev,
		GLD_4D_VERTEX_SIZE * gld->dwMaxVBVerts,
		dwUsage,
		0, // Non-FVF buffer
		D3DPOOL_DEFAULT,
		&gld->pVB,
		NULL);
	if (FAILED(hr)) {
		gldLogMessage(GLDLOG_CRITICAL_OR_WARN, szCreateVertexBufferFailed);
		SAFE_FREE(gld->pPrim); // Clean up before bail
		return hr; // FAILED
	}

	gldResetPrimitiveBuffer(gld);

	return S_OK; // SUCCEEDED
}

//---------------------------------------------------------------------------

BOOL IsDX9DriverLame(
	IDirect3D9 *pD3D,
	UINT uiAdapter,
	D3DDEVTYPE d3dDevType)
{
	HRESULT		hr;
	D3DCAPS9	d3dCaps;
	DWORD		dwMinVSVer = D3DVS_VERSION(1,0); // DX8.0 support
	DWORD		dwMinPSVer = D3DPS_VERSION(1,0); // DX8.0 support

	hr = IDirect3D9_GetDeviceCaps(pD3D, uiAdapter, d3dDevType, &d3dCaps);
	if (FAILED(hr))
		return TRUE; // Can't get caps? Must be lame...

	{
		// Dump out some stats.
		// We do this before the lameness test so if we bail we can what VS and PS values caused us to bail.
		gldLogPrintf(GLDLOG_SYSTEM, "Vertex Shader    : %d.%d", D3DSHADER_VERSION_MAJOR(d3dCaps.VertexShaderVersion), D3DSHADER_VERSION_MINOR(d3dCaps.VertexShaderVersion));
		gldLogPrintf(GLDLOG_SYSTEM, "Pixel Shader     : %d.%d", D3DSHADER_VERSION_MAJOR(d3dCaps.PixelShaderVersion), D3DSHADER_VERSION_MINOR(d3dCaps.PixelShaderVersion));
	}

	if (d3dCaps.VertexShaderVersion < dwMinVSVer)
		return TRUE; // LAME!

	if (d3dCaps.PixelShaderVersion < dwMinPSVer)
		return TRUE; // LAME!

	// Meets bare minimum specification, at least.
	return FALSE;
}

//---------------------------------------------------------------------------

BOOL gldCreateDrawable_DX(
	GLD_ctx *ctx,
//	BOOL bDefaultDriver,
	BOOL bDirectDrawPersistant,
	BOOL bPersistantBuffers)
{
	//
	// bDirectDrawPersistant:	applies to IDirect3D9
	// bPersistantBuffers:		applies to IDirect3DDevice9
	//

	HRESULT					hResult;
	GLD_driver_dx9			*lpCtx = NULL;
	D3DDEVTYPE				d3dDevType;
	D3DPRESENT_PARAMETERS	d3dpp;
	D3DDISPLAYMODE			d3ddm;
	DWORD					dwBehaviourFlags;
	D3DADAPTER_IDENTIFIER9	d3dIdent;
	UINT					uiDriverLevel;

	// Error if context is NULL.
	if (ctx == NULL)
		return FALSE;

	if (ctx->glPriv) {
		lpCtx = (GLD_driver_dx9*)ctx->glPriv;
		// Release any existing interfaces
		SAFE_RELEASE(lpCtx->pDev);
		SAFE_RELEASE(lpCtx->pD3D);
	} else {
		lpCtx = (GLD_driver_dx9*)calloc(1, sizeof(GLD_driver_dx9));
//		ZeroMemory(lpCtx, sizeof(lpCtx));
	}

	d3dDevType = (glb.dwDriver == GLDS_DRIVER_HAL) ? D3DDEVTYPE_HAL : D3DDEVTYPE_REF;
	// TODO: Check this
//	if (bDefaultDriver)
//		d3dDevType = D3DDEVTYPE_REF;

	// Use persistant interface if needed
	if (bDirectDrawPersistant && dx9Globals.bDirect3D) {
		lpCtx->pD3D = dx9Globals.pD3D;
		IDirect3D9_AddRef(lpCtx->pD3D);
		goto SkipDirectDrawCreate;
	}

	// Create Direct3D9 object
	lpCtx->pD3D = dx9Globals.fnDirect3DCreate9(D3D_SDK_VERSION);
	if (lpCtx->pD3D == NULL) {
		MessageBox(NULL, "Unable to initialize Direct3D9", "GLDirect", MB_OK);
		gldLogMessage(GLDLOG_CRITICAL_OR_WARN, "Unable to create Direct3D9 interface");
        nContextError = GLDERR_D3D;
		goto return_with_error;
	}

	// Cache Direct3D interface for subsequent GLRCs
	if (bDirectDrawPersistant && !dx9Globals.bDirect3D) {
		dx9Globals.pD3D = lpCtx->pD3D;
		IDirect3D9_AddRef(dx9Globals.pD3D);
		dx9Globals.bDirect3D = TRUE;
	}
SkipDirectDrawCreate:

	// Get the display mode so we can make a compatible backbuffer
	hResult = IDirect3D9_GetAdapterDisplayMode(lpCtx->pD3D, glb.dwAdapter, &d3ddm);
	if (FAILED(hResult)) {
        nContextError = GLDERR_D3D;
		goto return_with_error;
	}

	// Check if fullscreen window for page-flipping option. (DaveM)
	if (!glb.bFullscreenBlit) {
		RECT r;
		GetClientRect(ctx->hWnd, &r);
		if ((r.right - r.left == d3ddm.Width) &&
			(r.bottom - r.top == d3ddm.Height))
			ctx->bFullscreen = TRUE;
	}

	// Get device caps
	hResult = IDirect3D9_GetDeviceCaps(lpCtx->pD3D, glb.dwAdapter, d3dDevType, &lpCtx->d3dCaps9);
	if (FAILED(hResult)) {
		gldLogError(GLDLOG_CRITICAL_OR_WARN, "IDirect3D9_GetDeviceCaps failed", hResult);
        nContextError = GLDERR_D3D;
		goto return_with_error;
	}

	// Check for hardware transform & lighting
	lpCtx->bHasHWTnL = lpCtx->d3dCaps9.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ? TRUE : FALSE;

	//
	//	Create the Direct3D context
	//

	// Re-use original IDirect3DDevice if persistant buffers exist.
	// Note that we test for persistant IDirect3D9 as well
	// bDirectDrawPersistant == persistant IDirect3D9 (DirectDraw9 does not exist)
	if (bDirectDrawPersistant && bPersistantBuffers && dx9Globals.pD3D && dx9Globals.pDev) {
		lpCtx->pDev = dx9Globals.pDev;
		IDirect3DDevice9_AddRef(dx9Globals.pDev);
		goto skip_direct3ddevice_create;
	}

	// Clear the presentation parameters (sets all members to zero)
	ZeroMemory(&d3dpp, sizeof(d3dpp));

	// Recommended by MS; needed for MultiSample.
	// Be careful if altering this for FullScreenBlit
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

	d3dpp.BackBufferFormat	= d3ddm.Format;
	d3dpp.BackBufferCount	= (!glb.bWaitForRetrace) ? 2 : 1;
	d3dpp.MultiSampleType	= _gldGetDeviceMultiSampleType(lpCtx->pD3D, d3ddm.Format, d3dDevType, !ctx->bFullscreen);
	d3dpp.AutoDepthStencilFormat	= (D3DFORMAT)ctx->lpPF->dwDriverData;
	d3dpp.EnableAutoDepthStencil	= (d3dpp.AutoDepthStencilFormat == D3DFMT_UNKNOWN) ? FALSE : TRUE;

	if (ctx->bFullscreen) {
		gldLogWarnOption(FALSE); // Don't popup any messages in fullscreen
		d3dpp.Windowed							= FALSE;
		d3dpp.BackBufferWidth					= d3ddm.Width;
		d3dpp.BackBufferHeight					= d3ddm.Height;
		d3dpp.hDeviceWindow						= ctx->hWnd;
		d3dpp.FullScreen_RefreshRateInHz		= D3DPRESENT_RATE_DEFAULT;
		// Support for vertical retrace synchronisation.
		// Set default presentation interval in case caps bits are missing
		d3dpp.PresentationInterval	= D3DPRESENT_INTERVAL_DEFAULT;
		if (glb.bWaitForRetrace) {
			if (lpCtx->d3dCaps9.PresentationIntervals & D3DPRESENT_INTERVAL_ONE)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		} else {
			if (lpCtx->d3dCaps9.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}
		// Fullscreen blit option. (DaveM)
		if ((glb.bFullscreenBlit || ctx->EmulateSingle) &&
			d3dpp.MultiSampleType == D3DMULTISAMPLE_NONE) {
			d3dpp.BackBufferCount = 1;
			d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
		}
	} else {
		gldLogWarnOption(glb.bMessageBoxWarnings); // OK to popup messages
		d3dpp.Windowed							= TRUE;
		d3dpp.BackBufferWidth					= ctx->dwWidth;
		d3dpp.BackBufferHeight					= ctx->dwHeight;
		d3dpp.hDeviceWindow						= ctx->hWnd;
		d3dpp.FullScreen_RefreshRateInHz		= 0;
		// PresentationInterval Windowed mode is optional now in DX9 (DaveM)
		d3dpp.PresentationInterval	= D3DPRESENT_INTERVAL_DEFAULT;
		if (glb.bWaitForRetrace) {
			if (lpCtx->d3dCaps9.PresentationIntervals & D3DPRESENT_INTERVAL_ONE)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		} else {
			if (lpCtx->d3dCaps9.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}
		// Emulated front buffering option. (DaveM)
		if (ctx->EmulateSingle &&
			d3dpp.MultiSampleType == D3DMULTISAMPLE_NONE) {
			d3dpp.BackBufferCount = 1;
			d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
		}
	}

	// Decide if we can use hardware TnL
	// Changed D3DCREATE_MIXED_VERTEXPROCESSING to D3DCREATE_HARDWARE_VERTEXPROCESSING. KeithH
	dwBehaviourFlags = (lpCtx->bHasHWTnL) ?
		D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	// Add flag to tell D3D to be thread-safe
	if (glb.bMultiThreaded)
		dwBehaviourFlags |= D3DCREATE_MULTITHREADED;
	// Add flag to tell D3D to be FPU-safe
	if (!glb.bFastFPU)
		dwBehaviourFlags |= D3DCREATE_FPU_PRESERVE;

	// Add flag for Pure device.
	// This is currently slower. Need to implement state manager before enabling this.
//	dwBehaviourFlags |= D3DCREATE_PUREDEVICE;

	hResult = IDirect3D9_CreateDevice(lpCtx->pD3D,
								glb.dwAdapter,
								d3dDevType,
								ctx->hWnd ? ctx->hWnd : GetDesktopWindow(),
								dwBehaviourFlags,
								&d3dpp,
								&lpCtx->pDev);
    if (FAILED(hResult)) {
		gldLogError(GLDLOG_CRITICAL_OR_WARN, "IDirect3D9_CreateDevice failed", hResult);
        nContextError = GLDERR_D3D;
		goto return_with_error;
	}

	if (bDirectDrawPersistant && bPersistantBuffers && dx9Globals.pD3D) {
		dx9Globals.pDev = lpCtx->pDev;
		dx9Globals.bDirect3DDevice = TRUE;
	}

	// Dump the IHV driver level
	// This should return:
	// 700 - DirectX 7.0 level driver
	// 800 - DirectX 8.0 level driver
	// 900 - DirectX 9.0 level driver
	uiDriverLevel = D3DXGetDriverLevel(lpCtx->pDev);
	gldLogPrintf(GLDLOG_SYSTEM, "[DDI Driver Level : %d]", uiDriverLevel);

	// Dump some useful stats
	hResult = IDirect3D9_GetAdapterIdentifier(
		lpCtx->pD3D,
		glb.dwAdapter,
		0, // No WHQL detection (avoid few seconds delay)
		&d3dIdent);
	if (SUCCEEDED(hResult)) {
		gldLogPrintf(GLDLOG_INFO, "[Driver Description: %s]", &d3dIdent.Description);
		gldLogPrintf(GLDLOG_INFO, "[Driver file: %s %d.%d.%02d.%d]",
			d3dIdent.Driver,
			HIWORD(d3dIdent.DriverVersion.HighPart),
			LOWORD(d3dIdent.DriverVersion.HighPart),
			HIWORD(d3dIdent.DriverVersion.LowPart),
			LOWORD(d3dIdent.DriverVersion.LowPart));
		gldLogPrintf(GLDLOG_INFO, "[VendorId: 0x%X, DeviceId: 0x%X, SubSysId: 0x%X, Revision: 0x%X]",
			d3dIdent.VendorId, d3dIdent.DeviceId, d3dIdent.SubSysId, d3dIdent.Revision);
	}

	// Test to see if IHV driver exposes Scissor Test (new for DX9)
	lpCtx->bCanScissor = lpCtx->d3dCaps9.RasterCaps & D3DPRASTERCAPS_SCISSORTEST;
	gldLogPrintf(GLDLOG_INFO, "Can Scissor: %s", lpCtx->bCanScissor ? "Yes" : "No");

	// Init projection matrix for D3D TnL
	D3DXMatrixIdentity(&lpCtx->matProjection);
	lpCtx->matModelView = lpCtx->matProjection;

skip_direct3ddevice_create:

	// Create buffers to hold primitives
	hResult = _gldCreatePrimitiveBuffer(lpCtx);
	if (FAILED(hResult))
		goto return_with_error;

	// Create Vertex Declaration for GLD_4D_VERTEX
	lpCtx->pVertDecl = NULL;
	hResult = IDirect3DDevice9_CreateVertexDeclaration(lpCtx->pDev, (D3DVERTEXELEMENT9*)&GLD_vertDecl, &lpCtx->pVertDecl);
	if (FAILED(hResult))
		goto return_with_error;

	// Assign drawable to GL private
	ctx->glPriv = lpCtx;
	return TRUE;

return_with_error:
	// Clean up and bail
	_gldDestroyPrimitiveBuffer(lpCtx);
	SAFE_RELEASE(lpCtx->pDev);
	SAFE_RELEASE(lpCtx->pD3D);
	return FALSE;
}

//---------------------------------------------------------------------------

BOOL gldResizeDrawable_DX(
	GLD_ctx *ctx,
	BOOL bDefaultDriver,
	BOOL bPersistantInterface,
	BOOL bPersistantBuffers)
{
	GLD_driver_dx9			*gld = NULL;
	D3DDEVTYPE				d3dDevType;
	D3DPRESENT_PARAMETERS	d3dpp;
	D3DDISPLAYMODE			d3ddm;
	HRESULT					hResult;
	int						i;

	// Error if context is NULL.
	if (ctx == NULL)
		return FALSE;

	gld = (GLD_driver_dx9*)ctx->glPriv;
	if (gld == NULL)
		return FALSE;

	// Check if resize might have caused device suspension. (DaveM)
	hResult = IDirect3DDevice9_TestCooperativeLevel(gld->pDev);	// DaveM
	if (FAILED(hResult))
		gldLogPrintf(GLDLOG_WARN, "gldResizeDrawable_DX: IDirect3DDevice9_TestCooperativeLevel");

	// End the current Effect
	gldEndEffect(gld, gld->iCurEffect);

	// End the scene
	if (ctx->bSceneStarted) {
		IDirect3DDevice9_EndScene(gld->pDev);
		ctx->bSceneStarted = FALSE;
	}

	d3dDevType = (glb.dwDriver == GLDS_DRIVER_HAL) ? D3DDEVTYPE_HAL : D3DDEVTYPE_REF;
	if (!bDefaultDriver)
		d3dDevType = D3DDEVTYPE_REF; // Force Direct3D Reference Rasterise (software)

	// Get the display mode so we can make a compatible backbuffer
	hResult = IDirect3D9_GetAdapterDisplayMode(gld->pD3D, glb.dwAdapter, &d3ddm);
	if (FAILED(hResult)) {
        nContextError = GLDERR_D3D;
//		goto return_with_error;
		return FALSE;
	}

	// Release POOL_DEFAULT objects before Reset()
	_gldDestroyPrimitiveBuffer(gld);

	// Notify Effects of impending Reset
	for (i=0; i<gld->nEffects; i++) {
		ID3DXEffect_OnLostDevice(gld->Effects[i].pEffect);
	}

	// Clear the presentation parameters (sets all members to zero)
	ZeroMemory(&d3dpp, sizeof(d3dpp));

	// Recommended by MS; needed for MultiSample.
	// Be careful if altering this for FullScreenBlit
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

	d3dpp.BackBufferFormat	= d3ddm.Format;
	d3dpp.BackBufferCount	= (!glb.bWaitForRetrace) ? 2 : 1;
	d3dpp.MultiSampleType	= _gldGetDeviceMultiSampleType(gld->pD3D, d3ddm.Format, d3dDevType, !ctx->bFullscreen);
	d3dpp.AutoDepthStencilFormat	= (D3DFORMAT)ctx->lpPF->dwDriverData;
	d3dpp.EnableAutoDepthStencil	= (d3dpp.AutoDepthStencilFormat == D3DFMT_UNKNOWN) ? FALSE : TRUE;

	// TODO: Sync to refresh

	if (ctx->bFullscreen) {
		gldLogWarnOption(FALSE); // Don't popup any messages in fullscreen
		d3dpp.Windowed						= FALSE;
		d3dpp.BackBufferWidth				= d3ddm.Width;
		d3dpp.BackBufferHeight				= d3ddm.Height;
		d3dpp.hDeviceWindow					= ctx->hWnd;
		d3dpp.FullScreen_RefreshRateInHz	= D3DPRESENT_RATE_DEFAULT;
		d3dpp.PresentationInterval			= D3DPRESENT_INTERVAL_DEFAULT;
		// Get better benchmark results? KeithH
//		d3dpp.FullScreen_RefreshRateInHz	= D3DPRESENT_RATE_UNLIMITED;
		// Support for vertical retrace synchronisation.
		// Set default presentation interval in case caps bits are missing
		d3dpp.PresentationInterval	= D3DPRESENT_INTERVAL_DEFAULT;
		if (glb.bWaitForRetrace) {
			if (gld->d3dCaps9.PresentationIntervals & D3DPRESENT_INTERVAL_ONE)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		} else {
			if (gld->d3dCaps9.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}
		// Fullscreen blit option. (DaveM)
		if ((glb.bFullscreenBlit || ctx->EmulateSingle) &&
			d3dpp.MultiSampleType == D3DMULTISAMPLE_NONE) {
			d3dpp.BackBufferCount = 1;
			d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
		}
	} else {
		gldLogWarnOption(glb.bMessageBoxWarnings); // OK to popup messages
		d3dpp.Windowed						= TRUE;
		d3dpp.BackBufferWidth				= ctx->dwWidth;
		d3dpp.BackBufferHeight				= ctx->dwHeight;
		d3dpp.hDeviceWindow					= ctx->hWnd;
		d3dpp.FullScreen_RefreshRateInHz	= 0;
		d3dpp.PresentationInterval			= D3DPRESENT_INTERVAL_DEFAULT;
		// PresentationInterval Windowed mode is optional now in DX9 (DaveM)
		d3dpp.PresentationInterval	= D3DPRESENT_INTERVAL_DEFAULT;
		if (glb.bWaitForRetrace) {
			if (gld->d3dCaps9.PresentationIntervals & D3DPRESENT_INTERVAL_ONE)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		} else {
			if (gld->d3dCaps9.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE)
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}
		// Emulated front buffering option. (DaveM)
		if (ctx->EmulateSingle &&
			d3dpp.MultiSampleType == D3DMULTISAMPLE_NONE) {
			d3dpp.BackBufferCount = 1;
			d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
		}
	}
	// Reset() will fail if switching from windowed to fullscreen
	// surfaces, so ResizeDrawable() really should not be called
	// in that case. (DaveM)
	hResult = IDirect3DDevice9_Reset(gld->pDev, &d3dpp);
	if (FAILED(hResult)) {
		gldLogError(GLDLOG_CRITICAL_OR_WARN, "gldResize: Reset failed", hResult);
		return FALSE;
		//goto cleanup_and_return_with_error;
	}

    // Explicitly Clear resized surfaces (DaveM)
	{
		D3DVIEWPORT9 d3dvp1, d3dvp2;
		IDirect3DDevice9_GetViewport(gld->pDev, &d3dvp1);
		IDirect3DDevice9_GetViewport(gld->pDev, &d3dvp2);
		d3dvp1.X = 0;
		d3dvp1.Y = 0;
		d3dvp1.Width = ctx->dwWidth;
		d3dvp1.Height = ctx->dwHeight;
		IDirect3DDevice9_SetViewport(gld->pDev, &d3dvp1);
		IDirect3DDevice9_Clear(gld->pDev,0,NULL,D3DCLEAR_TARGET,0,1.0f,0);
		IDirect3DDevice9_SetViewport(gld->pDev, &d3dvp2);
	}

	// Set Dither back on again. Some drivers switch this off in the Reset() call...
	IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_DITHERENABLE, TRUE);

	//
	// Recreate POOL_DEFAULT objects
	//
	_gldCreatePrimitiveBuffer(gld);

	// Notify Effects that Reset has been called
	for (i=0; i<gld->nEffects; i++) {
		ID3DXEffect_OnResetDevice(gld->Effects[i].pEffect);
	}

	// Necessary for D3D HW TnL resize when normals not present.(DaveM)
	if (gld->bHasHWTnL)
		IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_LIGHTING, FALSE);
	
	// Signal a complete state update
	ctx->glCtx->Driver.UpdateState(ctx->glCtx, _NEW_ALL);

	// Begin a new scene
	hResult = IDirect3DDevice9_BeginScene(gld->pDev);
	ctx->bSceneStarted = TRUE;

	if (FAILED(hResult))
		gldLogPrintf(GLDLOG_WARN, "gldResizeDrawable_DX: IDirect3DDevice9_BeginScene");

	// Start the current Effect
	gldBeginEffect(gld, gld->iCurEffect);

	// Reset stream
	_GLD_DX9_DEV(SetStreamSource(gld->pDev, 0, gld->pVB, 0, GLD_4D_VERTEX_SIZE));
	_GLD_DX9_DEV(SetVertexDeclaration(gld->pDev, gld->pVertDecl));

	return TRUE;
}

//---------------------------------------------------------------------------

BOOL gldDestroyDrawable_DX(
	GLD_ctx *ctx)
{
	GLD_driver_dx9			*lpCtx = NULL;

	// Error if context is NULL.
	if (!ctx)
		return FALSE;

	// Error if the drawable does not exist.
	if (!ctx->glPriv)
		return FALSE;

	lpCtx = (GLD_driver_dx9*)ctx->glPriv;

	// Release buffers used to build up and render primitives
	_gldDestroyPrimitiveBuffer(lpCtx);

	// Release vertex declaration
	SAFE_RELEASE(lpCtx->pVertDecl);

	// Hack for exiting DX9 D3D fullscreen page-flipping mode.
	// Otherwise Quake3 crashes on exit. (DaveM)
	if (ctx->bFullscreen && ctx->lpfnWndProc) {
		SetWindowLong(ctx->hWnd, GWL_WNDPROC, (LONG)ctx->lpfnWndProc);
		ctx->lpfnWndProc = (LONG)NULL;
		}

	// Ensure device isn't holding onto any interfaces before we release it.
	gldReleaseShaders(lpCtx);
	_GLD_DX9_DEV(SetTexture(lpCtx->pDev, 0, NULL));
	_GLD_DX9_DEV(SetTexture(lpCtx->pDev, 1, NULL));
	_GLD_DX9_DEV(SetVertexShader(lpCtx->pDev, NULL));
	_GLD_DX9_DEV(SetPixelShader(lpCtx->pDev, NULL));
	_GLD_DX9_DEV(SetStreamSource(lpCtx->pDev, 0, NULL, 0, 0));
	_GLD_DX9_DEV(SetVertexDeclaration(lpCtx->pDev, NULL));

	SAFE_RELEASE(lpCtx->pDev);
	SAFE_RELEASE(lpCtx->pD3D);

	// Free the private drawable data
	free(ctx->glPriv);
	ctx->glPriv = NULL;

	return TRUE;
}

//---------------------------------------------------------------------------

BOOL gldCreatePrivateGlobals_DX(void)
{
	ZeroMemory(&dx9Globals, sizeof(dx9Globals));

	// Load d3d9.dll
	dx9Globals.hD3D9DLL = LoadLibrary("D3D9.DLL");
	if (dx9Globals.hD3D9DLL == NULL)
		return FALSE;

	// Now try and obtain Direct3DCreate9
	dx9Globals.fnDirect3DCreate9 = (FNDIRECT3DCREATE9)GetProcAddress(dx9Globals.hD3D9DLL, "Direct3DCreate9");
	if (dx9Globals.fnDirect3DCreate9 == NULL) {
		FreeLibrary(dx9Globals.hD3D9DLL);
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------

BOOL gldDestroyPrivateGlobals_DX(void)
{
	if (dx9Globals.bDirect3DDevice) {
		SAFE_RELEASE(dx9Globals.pDev);
		dx9Globals.bDirect3DDevice = FALSE;
	}
	if (dx9Globals.bDirect3D) {
		SAFE_RELEASE(dx9Globals.pD3D);
		dx9Globals.bDirect3D = FALSE;
	}

	FreeLibrary(dx9Globals.hD3D9DLL);
	dx9Globals.hD3D9DLL = NULL;
	dx9Globals.fnDirect3DCreate9 = NULL;

	return TRUE;
}

//---------------------------------------------------------------------------

static void _BitsFromDisplayFormat(
	D3DFORMAT fmt,
	BYTE *cColorBits,
	BYTE *cRedBits,
	BYTE *cGreenBits,
	BYTE *cBlueBits,
	BYTE *cAlphaBits)
{
	switch (fmt) {
	case D3DFMT_X1R5G5B5:
		*cColorBits = 16;
		*cRedBits = 5;
		*cGreenBits = 5;
		*cBlueBits = 5;
		*cAlphaBits = 0;
		return;
	case D3DFMT_R5G6B5:
		*cColorBits = 16;
		*cRedBits = 5;
		*cGreenBits = 6;
		*cBlueBits = 5;
		*cAlphaBits = 0;
		return;
	case D3DFMT_X8R8G8B8:
		*cColorBits = 32;
		*cRedBits = 8;
		*cGreenBits = 8;
		*cBlueBits = 8;
		*cAlphaBits = 0;
		return;
	case D3DFMT_A8R8G8B8:
		*cColorBits = 32;
		*cRedBits = 8;
		*cGreenBits = 8;
		*cBlueBits = 8;
		*cAlphaBits = 8;
		return;
	}

	// Should not get here!
	*cColorBits = 32;
	*cRedBits = 8;
	*cGreenBits = 8;
	*cBlueBits = 8;
	*cAlphaBits = 0;
}

//---------------------------------------------------------------------------

static void _BitsFromDepthStencilFormat(
	D3DFORMAT fmt,
	BYTE *cDepthBits,
	BYTE *cStencilBits)
{
	// NOTE: GL expects either 32 or 16 as depth bits.
	switch (fmt) {
	case D3DFMT_D32:
		*cDepthBits = 32;
		*cStencilBits = 0;
		return;
	case D3DFMT_D15S1:
		*cDepthBits = 16;
		*cStencilBits = 1;
		return;
	case D3DFMT_D24S8:
		*cDepthBits = 32;
		*cStencilBits = 8;
		return;
	case D3DFMT_D16:
		*cDepthBits = 16;
		*cStencilBits = 0;
		return;
	case D3DFMT_D24X8:
		*cDepthBits = 32;
		*cStencilBits = 0;
		return;
	case D3DFMT_D24X4S4:
		*cDepthBits = 32;
		*cStencilBits = 4;
		return;
	}
}

//---------------------------------------------------------------------------

BOOL gldBuildPixelformatList_DX(void)
{
	D3DDISPLAYMODE		d3ddm;
	D3DFORMAT			fmt[6];
	IDirect3D9			*pD3D = NULL;
	HRESULT				hr;
	int					nSupportedFormats = 0;
	int					i;
	GLD_pixelFormat		*pPF;
	BYTE				cColorBits, cRedBits, cGreenBits, cBlueBits, cAlphaBits;
	D3DDEVTYPE			d3dDevType; // D3D device type

	// Direct3D (SW or HW)
	// These are arranged so that 'best' pixelformat
	// is higher in the list (for ChoosePixelFormat).
	const D3DFORMAT DepthStencil[6] = {
// New order: increaing Z, then increasing stencil
		D3DFMT_D15S1,
		D3DFMT_D16,
		D3DFMT_D24X4S4,
		D3DFMT_D24X8,
		D3DFMT_D24S8,
		D3DFMT_D32,
	};

	// Release any existing pixelformat list
	if (glb.lpPF) {
		free(glb.lpPF);
	}

	glb.nPixelFormatCount	= 0;
	glb.lpPF				= NULL;

	//
	// Pixelformats for Direct3D (SW or HW) rendering
	//

	// Get a Direct3D 9.0 interface
	pD3D = dx9Globals.fnDirect3DCreate9(D3D_SDK_VERSION);
	if (!pD3D) {
		gldLogMessage(GLDLOG_CRITICAL, "Installed DirectX runtime is incompatible with this GLDirect build\n");
		MessageBox(NULL, "Your DirectX runtime is too old, please update to a newer version", "GLDirect Error", MB_OK | MB_ICONSTOP);
		return FALSE;
	}

	// Work out which D3D device we eant to test with
	d3dDevType = (glb.dwDriver == GLDS_DRIVER_HAL) ? D3DDEVTYPE_HAL : D3DDEVTYPE_REF;

	// Test for Lame device. This is as early as we dare test for this.
	if (IsDX9DriverLame(pD3D, glb.dwAdapter, d3dDevType)) {
		IDirect3D9_Release(pD3D);
		gldLogMessage(GLDLOG_CRITICAL, "Graphics adapter does not meet minimum specification for GLDirect\n");
		MessageBox(NULL, szLameWarning, "GLDirect Error", MB_OK | MB_ICONSTOP);
		return FALSE;
	}

	// We will use the display mode format when finding compliant
	// rendertarget/depth-stencil surfaces.
	hr = IDirect3D9_GetAdapterDisplayMode(pD3D, glb.dwAdapter, &d3ddm);
	if (FAILED(hr)) {
		IDirect3D9_Release(pD3D);
		return FALSE;
	}
	
	// Run through the possible formats and detect supported formats
	for (i=0; i<6; i++) {
		hr = IDirect3D9_CheckDeviceFormat(
			pD3D,
			glb.dwAdapter,
			glb.dwDriver==GLDS_DRIVER_HAL ? D3DDEVTYPE_HAL : D3DDEVTYPE_REF,
            d3ddm.Format,
			D3DUSAGE_DEPTHSTENCIL,
			D3DRTYPE_SURFACE,
			DepthStencil[i]);
		if (FAILED(hr))
			// A failure here is not fatal.
			continue;

	    // Verify that the depth format is compatible.
	    hr = IDirect3D9_CheckDepthStencilMatch(
				pD3D,
				glb.dwAdapter,
                glb.dwDriver==GLDS_DRIVER_HAL ? D3DDEVTYPE_HAL : D3DDEVTYPE_REF,
                d3ddm.Format,
                d3ddm.Format,
                DepthStencil[i]);
		if (FAILED(hr))
			// A failure here is not fatal, just means depth-stencil
			// format is not compatible with this display mode.
			continue;

		fmt[nSupportedFormats++] = DepthStencil[i];
	}

	IDirect3D9_Release(pD3D);

	if (nSupportedFormats == 0)
		return FALSE; // Bail: no compliant pixelformats

	// Dump DX version
	gldLogMessage(GLDLOG_SYSTEM, "DirectX Version  : 9.0 or above\n");

	// Total count of pixelformats is:
	// (nSupportedFormats+1)*2
	// UPDATED: nSupportedFormats*2
	glb.lpPF = (GLD_pixelFormat *)calloc(nSupportedFormats*2, sizeof(GLD_pixelFormat));
	glb.nPixelFormatCount = nSupportedFormats*2;
	if (glb.lpPF == NULL) {
		glb.nPixelFormatCount = 0;
		return FALSE;
	}

	// Get a copy of pointer that we can alter
	pPF = glb.lpPF;

	// Cache colour bits from display format
	_BitsFromDisplayFormat(d3ddm.Format, &cColorBits, &cRedBits, &cGreenBits, &cBlueBits, &cAlphaBits);

	//
	// Add single-buffer formats
	//
	for (i=0; i<nSupportedFormats; i++, pPF++) {
		memcpy(pPF, &pfTemplateHW, sizeof(GLD_pixelFormat));
		pPF->pfd.dwFlags &= ~PFD_DOUBLEBUFFER; // Remove doublebuffer flag
		pPF->pfd.cColorBits		= cColorBits;
		pPF->pfd.cRedBits		= cRedBits;
		pPF->pfd.cGreenBits		= cGreenBits;
		pPF->pfd.cBlueBits		= cBlueBits;
		pPF->pfd.cAlphaBits		= cAlphaBits;
		_BitsFromDepthStencilFormat(fmt[i], &pPF->pfd.cDepthBits, &pPF->pfd.cStencilBits);
		pPF->dwDriverData		= fmt[i];
	}

	//
	// Add double-buffer formats
	//

	// NOTE: No longer returning pixelformats that don't contain depth
	for (i=0; i<nSupportedFormats; i++, pPF++) {
		memcpy(pPF, &pfTemplateHW, sizeof(GLD_pixelFormat));
		pPF->pfd.cColorBits		= cColorBits;
		pPF->pfd.cRedBits		= cRedBits;
		pPF->pfd.cGreenBits		= cGreenBits;
		pPF->pfd.cBlueBits		= cBlueBits;
		pPF->pfd.cAlphaBits		= cAlphaBits;
		_BitsFromDepthStencilFormat(fmt[i], &pPF->pfd.cDepthBits, &pPF->pfd.cStencilBits);
		pPF->dwDriverData		= fmt[i];
	}

	// Popup warning message if non RGB color mode
	{
		// This is a hack. KeithH
		HDC hdcDesktop = GetDC(NULL);
		DWORD dwDisplayBitDepth = GetDeviceCaps(hdcDesktop, BITSPIXEL);
		ReleaseDC(0, hdcDesktop);
		if (dwDisplayBitDepth <= 8) {
			gldLogPrintf(GLDLOG_WARN, "Current Color Depth %d bpp is not supported", dwDisplayBitDepth);
			MessageBox(NULL, szColorDepthWarning, "GLDirect", MB_OK | MB_ICONWARNING);
		}
	}

	// Mark list as 'current'
	glb.bPixelformatsDirty = FALSE;

	return TRUE;
}

//---------------------------------------------------------------------------

BOOL gldInitialiseMesa_DX(
	GLD_ctx *lpCtx)
{
	GLD_driver_dx9	*gld = NULL;
	int				MaxTextureSize, TextureLevels;
	BOOL			bSoftwareTnL;
	D3DXMATRIX		vm;

	if (lpCtx == NULL)
		return FALSE;

	gld = (GLD_driver_dx9*)lpCtx->glPriv;
	if (gld == NULL)
		return FALSE;

	if (glb.bMultitexture) {
		lpCtx->glCtx->Const.MaxTextureUnits = gld->d3dCaps9.MaxSimultaneousTextures;
		// Only support MAX_TEXTURE_UNITS texture units.
		// ** If this is altered then the FVF formats must be reviewed **.
		if (lpCtx->glCtx->Const.MaxTextureUnits > GLD_MAX_TEXTURE_UNITS_DX9)
			lpCtx->glCtx->Const.MaxTextureUnits = GLD_MAX_TEXTURE_UNITS_DX9;
	} else {
		// Multitexture override
		lpCtx->glCtx->Const.MaxTextureUnits = 1;
	}
	lpCtx->glCtx->Const.MaxTextureCoordUnits = lpCtx->glCtx->Const.MaxTextureUnits;

	// max texture size
	MaxTextureSize = min(gld->d3dCaps9.MaxTextureHeight, gld->d3dCaps9.MaxTextureWidth);
	if (MaxTextureSize == 0)
		MaxTextureSize = 256; // Sanity check

	// Clamp texture size to Mesa limits
	if (MaxTextureSize > __min(MAX_WIDTH, MAX_HEIGHT))
		MaxTextureSize = __min(MAX_WIDTH, MAX_HEIGHT);

	// Got to set MAX_TEXTURE_SIZE as max levels.
	// Who thought this stupid idea up? ;)
	TextureLevels = 0;
	// Calculate power-of-two.
	while (MaxTextureSize) {
		TextureLevels++;
		MaxTextureSize >>= 1;
	}
	lpCtx->glCtx->Const.MaxTextureLevels = (TextureLevels) ? TextureLevels : 8;

	IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_LIGHTING, FALSE);
	IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_CULLMODE, D3DCULL_NONE);
	IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_DITHERENABLE, TRUE);
	IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_CLIPPING, TRUE); // KeithH

	IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_ZENABLE,
		(lpCtx->lpPF->dwDriverData!=D3DFMT_UNKNOWN) ? D3DZB_TRUE : D3DZB_FALSE);

	// Set the view matrix
	D3DXMatrixIdentity(&vm);
	IDirect3DDevice9_SetTransform(gld->pDev, D3DTS_VIEW, &vm);

	if (gld->bHasHWTnL) {
		if (glb.dwTnL == GLDS_TNL_DEFAULT)
			bSoftwareTnL = FALSE; // HW TnL
		else {
			bSoftwareTnL = (glb.dwTnL == GLDS_TNL_D3DSW) ? TRUE : FALSE;
		}
	} else {
		// No HW TnL, so no choice possible
		bSoftwareTnL = TRUE;
	}
	IDirect3DDevice9_SetSoftwareVertexProcessing(gld->pDev, bSoftwareTnL);

// Dump this in a Release build as well, now.
//#ifdef _DEBUG
	gldLogPrintf(GLDLOG_INFO, "HW TnL: %s",
		gld->bHasHWTnL ? (bSoftwareTnL ? "Disabled" : "Enabled") : "Unavailable");
//#endif

	gldEnableExtensions_DX9(lpCtx->glCtx);
	gldInstallD3DTnl(lpCtx->glCtx); // Our own D3D TnL module
	gldSetupDriverPointers_DX9(lpCtx->glCtx);

	// Signal a complete state update
	lpCtx->glCtx->Driver.UpdateState(lpCtx->glCtx, _NEW_ALL);

	// Start a scene
	IDirect3DDevice9_BeginScene(gld->pDev);
	lpCtx->bSceneStarted = TRUE;

	return TRUE;
}

//---------------------------------------------------------------------------

BOOL gldSwapBuffers_DX(
	GLD_ctx *ctx,
	HDC hDC,
	HWND hWnd)
{
	HRESULT			hr;
	GLD_driver_dx9	*gld = NULL;

	if (ctx == NULL)
		return FALSE;

	gld = (GLD_driver_dx9*)ctx->glPriv;
	if (gld == NULL)
		return FALSE;

	// Flush any outstanding data
	FLUSH_VERTICES(ctx->glCtx, 0);
	// Ensure that scene variables are reset, regardless of whether Driver.FlushVertices is NULL.
	gldResetPrimitiveBuffer(gld);
	//gld->GLPrim			= PRIM_UNKNOWN;
	ctx->glCtx->Driver.CurrentExecPrimitive = PRIM_OUTSIDE_BEGIN_END;
	gld->GLReducedPrim	= PRIM_UNKNOWN;

	// End any Effect currently set
	gldEndEffect(gld, gld->iCurEffect);

	// Notify Direct3D of the scene end
	if (ctx->bSceneStarted) {
		IDirect3DDevice9_EndScene(gld->pDev);
		ctx->bSceneStarted = FALSE;
	}

	// Determine if device is valid
	hr = IDirect3DDevice9_TestCooperativeLevel(gld->pDev);
	if (SUCCEEDED(hr)) {
		// Swap the buffers. hWnd may override the hWnd used for CreateDevice()
		hr = IDirect3DDevice9_Present(gld->pDev, NULL, NULL, hWnd, NULL);
		if (FAILED(hr))
			gldLogError(GLDLOG_WARN, "gldSwapBuffers_DX: IDirect3DDevice9_Present", hr);
	} else {
		// Device is in an invalid state; either "lost" or "lost: not reset"
		if (hr == D3DERR_DEVICENOTRESET) {
			// Reset the device
			_gldDriver.ResizeDrawable(ctx, TRUE, glb.bDirectDrawPersistant, glb.bPersistantBuffers);
		}
	}

	IDirect3DDevice9_BeginScene(gld->pDev);
	ctx->bSceneStarted = TRUE;

	if (glb.bDisableZTrick) {
		// Force clear of target and depth every frame
//		IDirect3DDevice9_Clear(gld->pDev, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
		ctx->glCtx->Driver.Clear(ctx->glCtx, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_TRUE, ctx->glCtx->Viewport.X, ctx->glCtx->Viewport.Y, ctx->glCtx->Viewport.Width, ctx->glCtx->Viewport.Height);
	}

	// Restart the current Effect
	gldBeginEffect(gld, gld->iCurEffect);

	return (FAILED(hr)) ? FALSE : TRUE;
}

//---------------------------------------------------------------------------

BOOL gldGetDisplayMode_DX(
	GLD_ctx *ctx,
	GLD_displayMode *glddm)
{
	D3DDISPLAYMODE	d3ddm;
	HRESULT			hr;
	GLD_driver_dx9	*lpCtx = NULL;
	BYTE cColorBits, cRedBits, cGreenBits, cBlueBits, cAlphaBits;

	if ((glddm == NULL) || (ctx == NULL))
		return FALSE;

	lpCtx = (GLD_driver_dx9*)ctx->glPriv;
	if (lpCtx == NULL)
		return FALSE;

	if (lpCtx->pD3D == NULL)
		return FALSE;

	hr = IDirect3D9_GetAdapterDisplayMode(lpCtx->pD3D, glb.dwAdapter, &d3ddm);
	if (FAILED(hr))
		return FALSE;

	// Get info from the display format
	_BitsFromDisplayFormat(d3ddm.Format,
		&cColorBits, &cRedBits, &cGreenBits, &cBlueBits, &cAlphaBits);

	glddm->Width	= d3ddm.Width;
	glddm->Height	= d3ddm.Height;
	glddm->BPP		= cColorBits;
	glddm->Refresh	= d3ddm.RefreshRate;

	return TRUE;
}

//---------------------------------------------------------------------------

