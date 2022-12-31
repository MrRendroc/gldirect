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
* Description:  OpenGL context handling.
*
*********************************************************************************/

#ifndef __GLD_CONTEXT_H
#define __GLD_CONTEXT_H

// Disable compiler complaints about DLL linkage
#pragma warning (disable:4273)

// Macros to control compilation
#ifndef STRICT
#define STRICT
#endif // STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL\gl.h>

#include "gld_macros.h"
#include "gld_globals.h"
#include "gld_log.h"
#include "gld_pf.h"

#include "context.h"	// Mesa context

/*---------------------- Macros and type definitions ----------------------*/

// TODO: Use a list instead of this constant!
#define GLD_MAX_CONTEXTS 32

// Structure for describing an OpenGL context
typedef struct {
	BOOL				bHasBeenCurrent;
	GLD_pixelFormat		*lpPF;

	// Pointer to private driver data (this also contains the drawable).
	void				*glPriv;

	// Mesa vars:
	GLcontext			*glCtx;			// The core Mesa context
	GLvisual			*glVis;			// Describes the color buffer
	GLframebuffer		*glBuffer;		// Ancillary buffers

	GLuint				ClearIndex;
	GLuint				CurrentIndex;
	GLubyte				ClearColor[4];
	GLubyte				CurrentColor[4];

	BOOL				EmulateSingle;	// Emulate single-buffering
	BOOL				bDoubleBuffer;
	BOOL				bDepthBuffer;

	// Shared driver vars:
	BOOL				bAllocated;
    BOOL				bFullscreen;	// Is this a fullscreen context?
    BOOL				bSceneStarted;	// Has a lpDev->BeginScene been issued?
    BOOL				bCanRender;		// Flag: states whether rendering is OK
	BOOL				bFrameStarted;	// Has frame update started at all?
	BOOL				bStencil;		// TRUE if this context has stencil
	BOOL				bGDIEraseBkgnd; // GDI Erase Background command

	// Window information
	HWND				hWnd;			// Window handle
	HDC					hDC;			// Windows' Device Context of the window
	DWORD				dwWidth;		// Window width
	DWORD				dwHeight;		// Window height
	DWORD				dwBPP;			// Window bits-per-pixel;
	RECT				rcScreenRect;	// Screen rectangle
	DWORD				dwModeWidth;	// Display mode width
	DWORD				dwModeHeight;	// Display mode height
	float				dvClipX;
	float				dvClipY;
	LONG				lpfnWndProc;	// window message handler function
} GLD_ctx;

typedef GLD_ctx GLD_context;

#define GLD_GET_CONTEXT(c)	(GLD_context*)(c)->DriverCtx

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

LRESULT CALLBACK	gldKeyProc(int code,WPARAM wParam,LPARAM lParam);

void				gldInitContextState();
void				gldDeleteContextState();
BOOL 				gldIsValidContext(HGLRC a);
GLD_ctx*			gldGetContextAddress(const HGLRC a);
HDC 				gldGetCurrentDC(void);
HGLRC 				gldGetCurrentContext(void);
HGLRC				gldCreateContext(HDC a, const GLD_pixelFormat *lpPF);
BOOL				gldMakeCurrent(HDC a, HGLRC b);
BOOL				gldDeleteContext(HGLRC a);
BOOL				gldSwapBuffers(HDC hDC);

BOOL				gldDestroyDrawable_DX(GLD_ctx *ctx);
BOOL				gldCreateDrawable_DX(GLD_ctx *ctx, BOOL bDirectDrawPersistant, BOOL bPersistantBuffers);

#ifdef  __cplusplus
}
#endif

#endif
