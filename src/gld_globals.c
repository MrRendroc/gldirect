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
* Description:  Global variables.
*
*********************************************************************************/

#include "gld_globals.h"

// =======================================================================
// Global Variables
// =======================================================================

char szCopyright[]	= "Copyright (C) 1997-2007 SciTech Software, Inc.";
char szDllName[]	= "Scitech GLDirect";
char szErrorTitle[]	= "GLDirect Error";

GLD_globals glb;

// Shared result variable
HRESULT hResult;

// ***********************************************************************

// Patch function for missing function in Mesa
int finite(
	double x)
{
	return _finite(x);
};

// ***********************************************************************

void gldInitGlobals()
{
    // Zero all fields just in case
    memset(&glb, 0, sizeof(glb));

	// Set the global defaults
	glb.bPrimary			= FALSE;		// Not the primary device
	glb.bHardware			= FALSE;		// Not a hardware device
//	glb.bFullscreen			= FALSE;		// Not running fullscreen
	glb.bSquareTextures		= FALSE;		// Device does not need sq
	glb.bPAL8				= FALSE;		// Device cannot do 8bit
	glb.dwMemoryType		= DDSCAPS_SYSTEMMEMORY;
	glb.dwRendering			= GLD_RENDER_D3D;

	glb.bWaitForRetrace		= TRUE;			// Sync to vertical retrace
	glb.bFullscreenBlit		= FALSE;

	glb.nPixelFormatCount	= 0;
	glb.lpPF				= NULL;			// Pixel format list

	glb.wMaxSimultaneousTextures = 1;

	// Enable support for multitexture, if available.
	glb.bMultitexture		= TRUE;

	// Enable support for mipmapping
	glb.bUseMipmaps			= TRUE;

	// Alpha emulation via chroma key
	glb.bEmulateAlphaTest	= FALSE;

	// Use Mesa pipeline always (for compatibility)
	glb.bForceMesaPipeline	= FALSE;

	// Init support for multiple GLRCs
	glb.bDirectDraw			= FALSE;
	glb.bDirectDrawPrimary	= FALSE;
	glb.bDirect3D			= FALSE;
	glb.bDirect3DDevice		= FALSE;
	glb.hWndActive			= NULL;

	// Init special support options
	glb.bMessageBoxWarnings = TRUE;
	glb.bDirectDrawPersistant = FALSE;
	glb.bPersistantBuffers	= FALSE;

	// Do not assume single-precision-only FPU (for compatibility)
	glb.bFastFPU			= FALSE;

	// Allow hot-key support
	glb.bHotKeySupport		= TRUE;

	// Default to single-threaded support (for simplicity)
	glb.bMultiThreaded		= FALSE;

	// Use application-specific customizations (for end-user convenience)
	glb.bAppCustomizations	= TRUE;

	// Registry/ini-file settings for GLDirect
	glb.dwAdapter				= 0;	// Primary DX9 adapter
	glb.dwTnL					= 0;	// TnL setting
	glb.dwMultisample			= 0;	// Multisample Auto
	glb.dwDriver				= 2;	// Direct3D HW

	// Signal a pixelformat list rebuild
	glb.bPixelformatsDirty		= TRUE;

	// Quake1 engines can set this to allow GLDirect to disable ztrick.
	glb.bDisableZTrick			= FALSE;

	// hacks for various games
    glb.bFAKK2					= FALSE;
	glb.bBugdom					= FALSE;
    glb.bGL13Needed             = FALSE;
	
	// bUseMesaDisplayLists:
	// If TRUE, Mesa handles all display list functionality (less performance, more stability).
	// If FALSE, GLDirect will optimise display lists (more performance, less stability?).
	// This can be enabled for any app, as required, as an app-customisation.
	glb.bUseMesaDisplayLists	= FALSE;

	glb.iAppCustomisation			= -1; // Not yet detected
}

// ***********************************************************************
