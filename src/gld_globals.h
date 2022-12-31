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
* Description:  Globals.
*
*********************************************************************************/

#ifndef __GLD_GLOBALS_H
#define __GLD_GLOBALS_H

#include "gld_context.h"
#include "gld_pf.h"		// Pixel format

/*---------------------- Macros and type definitions ----------------------*/

typedef enum {
	GLD_RENDER_MESASW		= 0,
	GLD_RENDER_D3D			= 1,
	GLD_RENDER_FORCE_DWORD	= 0x7ffffff,
} GLD_renderType;

// Same as GLD_renderType? KeithH
typedef enum {
	GLDS_DRIVER_MESA_SW			= 0,	// Mesa SW rendering
	GLDS_DRIVER_REF				= 1,	// Direct3D Reference Rasteriser
	GLDS_DRIVER_HAL				= 2,	// Direct3D HW rendering
} GLDS_DRIVER;

typedef enum {
	GLDS_TNL_DEFAULT			= 0,	// Choose best TnL method
	GLDS_TNL_D3DSW				= 1,	// D3D Software TnL
	GLDS_TNL_D3DHW				= 2,	// D3D Hardware TnL
} GLDS_TNL;

typedef enum {
	GLDS_MULTISAMPLE_NONE		= 0,
	GLDS_MULTISAMPLE_FASTEST	= 1,
	GLDS_MULTISAMPLE_NICEST		= 2,
} GLDS_MULTISAMPLE;

typedef struct {
	// Registry settings
	char				szDDName[MAX_DDDEVICEID_STRING]; // DirectDraw device name
	char				szD3DName[MAX_DDDEVICEID_STRING]; // Direct3D driver name
	BOOL				bPrimary; // Is ddraw device the Primary device?
	BOOL				bHardware; // Is the d3d driver a Hardware driver?
//	BOOL				bFullscreen; // Force fullscreen - only useful for primary adaptors.
	BOOL				bSquareTextures; // Does this driver require square textures?
	DWORD               dwRendering; // Type of rendering required

	BOOL				bWaitForRetrace; // Sync to vertical retrace
	BOOL				bFullscreenBlit; // Use Blt instead of Flip in fullscreen modes

	// Multitexture
	BOOL				bMultitexture;

	BOOL				bUseMipmaps;

	DWORD				dwMemoryType; // Sysmem or Vidmem

	// Global palette
	BOOL				bPAL8;
	DDPIXELFORMAT		ddpfPAL8;

	// Multitexture
	WORD				wMaxSimultaneousTextures;

	// Win32 internals
	BOOL				bAppActive; // Keep track of Alt-Tab
	LONG				lpfnWndProc; // WndProc of calling app

	// Pixel Format Descriptior list.
	int					nPixelFormatCount;
	GLD_pixelFormat		*lpPF;
	// Alpha emulation via chroma key
	BOOL				bEmulateAlphaTest;

	// Geom pipeline override.
	// If this is set TRUE then the D3D pipeline will never be used,
	// and the Mesa pipline will always be used.
	BOOL				bForceMesaPipeline;

	BOOL				bPixelformatsDirty;	// Signal a list rebuild

	// Additional globals to support multiple GL rendering contexts, GLRCs
	BOOL				bDirectDraw;			// DirectDraw interface exists ?
	BOOL				bDirectDrawPrimary;		// DirectDraw primary surface exists ?
	BOOL				bDirect3D;				// Direct3D interface exists ?
	BOOL				bDirect3DDevice;		// Direct3D device exists ?

	HWND				hWndActive;				// copy of active window

	// Aids for heavy-duty MFC-windowed OGL apps, like AutoCAD
	BOOL				bMessageBoxWarnings;	// popup message box warning
	BOOL				bDirectDrawPersistant;  // DirectDraw is persisitant
	BOOL				bPersistantBuffers;  	// DirectDraw buffers persisitant

	// FPU setup option for CAD precision (AutoCAD) vs GAME speed (Quake)
	BOOL				bFastFPU;				// single-precision-only FPU ?

	// Hot-Key support, like for real-time stereo parallax adjustments
	BOOL				bHotKeySupport;			// hot-key support ?

	// Multi-threaded support, for apps like 3DStudio
	BOOL				bMultiThreaded;			// multi-threaded ?

	// Detect and use app-specific customizations for apps like 3DStudio
	BOOL				bAppCustomizations;		// app customizations ?

	// Quake1 engines can set this to allow GLDirect to disable ztrick.
	BOOL				bDisableZTrick;

	// Viewport is invalid in game Heavy Metal: FAKK2
	BOOL				bFAKK2;

	// Bugdom: Issue with Mesa not updating Wrap/Clamp mode
	BOOL				bBugdom;

	// Bugdom 2 and Otto Matic require matrix multiplication stuff from OpenGL 1.2.1
	BOOL				bGL13Needed;
	
	// bUseMesaDisplayLists:
	// If TRUE, Mesa handles all display list functionality (less performance, more stability).
	// If FALSE, GLDirect will optimise display lists (more performance, less stability?).
	// This can be enabled for any app, as required, as an app-customisation.
	// Default value: FALSE
	BOOL				bUseMesaDisplayLists;

    DWORD				dwAdapter;				// Primary DX8 adapter
	DWORD				dwTnL;					// TnL setting
	DWORD				dwMultisample;			// Multisample Off
	DWORD				dwDriver;				// Direct3D HW
	void				*pDrvPrivate;			// Driver-specific data

	int					iAppCustomisation;		// Index of AppCust detected.
} GLD_globals;

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

extern GLD_globals	glb;

void	gldInitGlobals();

#ifdef  __cplusplus
}
#endif

#endif