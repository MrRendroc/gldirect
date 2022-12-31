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
* Description:  Driver functions and interfaces
*
*********************************************************************************/

#ifndef _GLD_DRIVER_H
#define _GLD_DRIVER_H

#include "gld_context.h"

//---------------------------------------------------------------------------

// Same as DX8 D3DDISPLAYMODE
typedef struct {
	DWORD	Width;
	DWORD	Height;
	DWORD	Refresh;
	DWORD	BPP;
} GLD_displayMode;

//---------------------------------------------------------------------------

typedef struct {
	// Returns a string for a given HRESULT error code.
	BOOL	(*GetDXErrorString)(HRESULT hr, char *buf, int nBufSize);

	// Driver functions for managing drawables.
	// Functions must respect persistant buffers / persistant interface.
	// NOTE: Persistant interface is: DirectDraw, pre-DX8; Direct3D, DX8 and above.
	BOOL	(*CreateDrawable)(GLD_ctx *ctx, BOOL bPersistantInterface, BOOL bPersistantBuffers);
	BOOL	(*ResizeDrawable)(GLD_ctx *ctx, BOOL bDefaultDriver, BOOL bPersistantInterface, BOOL bPersistantBuffers);
	BOOL	(*DestroyDrawable)(GLD_ctx *ctx);

	// Create/Destroy private globals belonging to driver
	BOOL	(*CreatePrivateGlobals)(void);
	BOOL	(*DestroyPrivateGlobals)(void);

	// Build pixelformat list
	BOOL	(*BuildPixelformatList)(void);

	// Initialise Mesa's driver pointers
	BOOL	(*InitialiseMesa)(GLD_ctx *ctx);

	// Swap buffers
	BOOL	(*SwapBuffers)(GLD_ctx *ctx, HDC hDC, HWND hWnd);

	// wglGetProcAddress()
	PROC	(*wglGetProcAddress)(LPCSTR a);

	BOOL	(*GetDisplayMode)(GLD_ctx *ctx, GLD_displayMode *glddm);
} GLD_driver;

//---------------------------------------------------------------------------

#ifdef  __cplusplus
extern "C" {
#endif

extern GLD_driver _gldDriver;

BOOL gldInitDriverPointers(DWORD dwDriver);
const GLubyte* _gldGetStringGeneric(GLcontext *ctx, GLenum name);
BOOL gldValidate();

#ifdef  __cplusplus
}
#endif

//---------------------------------------------------------------------------

#endif // _GLD_DRIVER_H
