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

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gld_driver.h"
#include "gld_log.h"
#include "glheader.h"

#include "common_x86_asm.h"		// For glGetString().
#include "version.h"			// For MESA_VERSION_STRING

//---------------------------------------------------------------------------

static char *szDriverError = "Driver used before initialisation!";

// This holds our dynamically created OpenGL renderer string.
static char _gldRendererString[1024];

// Vendor string
static char *g_szGLDVendor		= "SciTech Software, Inc.";

// Based on mesa\src\mesa\main\get.c::_mesa_GetString
static char *g_szGLDVersion		= "1.1 Mesa " MESA_VERSION_STRING;

// extensions
// Quake3 is slower with GL_EXT_compiled_vertex_array !
static char *g_szGLDExtensions	=
//"GL_EXT_polygon_offset "
"GL_EXT_texture_env_add "
"GL_ARB_multitexture ";

//---------------------------------------------------------------------------

extern BOOL gldGetDXErrorString_DX(HRESULT hr, char *buf, int nBufSize);
extern BOOL gldCreateDrawable_DX(GLD_ctx *ctx, BOOL bPersistantInterface, BOOL bPersistantBuffers);
extern BOOL gldResizeDrawable_DX(GLD_ctx *ctx, BOOL bDefaultDriver, BOOL bPersistantInterface, BOOL bPersistantBuffers);
extern BOOL gldDestroyDrawable_DX(GLD_ctx *ctx);
extern BOOL gldCreatePrivateGlobals_DX(void);
extern BOOL gldDestroyPrivateGlobals_DX(void);
extern BOOL	gldBuildPixelformatList_DX(void);
extern BOOL gldInitialiseMesa_DX(GLD_ctx *ctx);
extern BOOL	gldSwapBuffers_DX(GLD_ctx *ctx, HDC hDC, HWND hWnd);
extern PROC	gldGetProcAddress_DX(LPCSTR a);
extern BOOL	gldGetDisplayMode_DX(GLD_ctx *ctx, GLD_displayMode *glddm);

//---------------------------------------------------------------------------
// NOP functions. Called if proper driver functions are not set.
//---------------------------------------------------------------------------

static BOOL _gldDriverError(void)
{
	gldLogMessage(GLDLOG_CRITICAL, szDriverError);
	return FALSE;
}

//---------------------------------------------------------------------------

static BOOL _GetDXErrorString_ERROR(
	HRESULT hr,
	char *buf,
	int nBufSize)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------

static BOOL _CreateDrawable_ERROR(
	GLD_ctx *ctx,
	BOOL bPersistantInterface,
	BOOL bPersistantBuffers)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------

static BOOL _ResizeDrawable_ERROR(
	GLD_ctx *ctx,
	BOOL bDefaultDriver,
	BOOL bPersistantInterface,
	BOOL bPersistantBuffers)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------

static BOOL _DestroyDrawable_ERROR(
	GLD_ctx *ctx)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------

static BOOL _CreatePrivateGlobals_ERROR(void)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------

static BOOL _DestroyPrivateGlobals_ERROR(void)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------

static BOOL _BuildPixelformatList_ERROR(void)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------


static BOOL _InitialiseMesa_ERROR(
	GLD_ctx *ctx)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------

static BOOL	_SwapBuffers_ERROR(
	GLD_ctx *ctx,
	HDC hDC,
	HWND hWnd)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------

static PROC _GetProcAddress_ERROR(
	LPCSTR a)
{
	_gldDriverError();
	return NULL;
}

//---------------------------------------------------------------------------

static BOOL	_GetDisplayMode_ERROR(
	GLD_ctx *ctx,
	GLD_displayMode *glddm)
{
	return _gldDriverError();
}

//---------------------------------------------------------------------------
// Functions useful to all drivers
//---------------------------------------------------------------------------

const GLubyte* _gldGetStringGeneric(
	GLcontext *ctx,
	GLenum name)
{
	if (!ctx)
		return NULL;

	switch (name) {
	case GL_VENDOR:
		return (const GLubyte *) g_szGLDVendor;
	case GL_VERSION:
		return (const GLubyte *) g_szGLDVersion;
	case GL_RENDERER:
		sprintf(_gldRendererString, "GLDirect 5.0 %s%s%s%s (%s %s)",
			_mesa_x86_cpu_features	? "x86"		: "",
			cpu_has_mmx				? "/MMX"		: "",
			cpu_has_3dnow			? "/3DNow!"		: "",
			cpu_has_xmm				? "/SSE"		: "",
			__DATE__, __TIME__);
		return (const GLubyte *) _gldRendererString;
	case GL_EXTENSIONS:
		return (const GLubyte *) g_szGLDExtensions;
	default:
		return NULL; // Mesa will "fill in blanks" if we return NULL.
	}
}

//---------------------------------------------------------------------------
// Global driver function pointers, initially set to functions that
// will report an error when called.
//---------------------------------------------------------------------------

GLD_driver _gldDriver = {
	_GetDXErrorString_ERROR,
	_CreateDrawable_ERROR,
	_ResizeDrawable_ERROR,
	_DestroyDrawable_ERROR,
	_CreatePrivateGlobals_ERROR,
	_DestroyPrivateGlobals_ERROR,
	_BuildPixelformatList_ERROR,
	_InitialiseMesa_ERROR,
	_SwapBuffers_ERROR,
	_GetProcAddress_ERROR,
	_GetDisplayMode_ERROR
};

//---------------------------------------------------------------------------
// FOR DEBUGGING!
//---------------------------------------------------------------------------

static BOOL _GetDXErrorString_NOP(
	HRESULT hr,
	char *buf,
	int nBufSize)
{
	buf[0] = 0;
	gldLogMessage(GLDLOG_SYSTEM, "_GetDXErrorString_NOP\n");
	return TRUE;
}

//---------------------------------------------------------------------------

static BOOL _CreateDrawable_NOP(
	GLD_ctx *ctx,
	BOOL bPersistantInterface,
	BOOL bPersistantBuffers)
{
	gldLogMessage(GLDLOG_SYSTEM, "_CreateDrawable_NOP\n");
	return TRUE;
}

//---------------------------------------------------------------------------

static BOOL _ResizeDrawable_NOP(
	GLD_ctx *ctx,
	BOOL bDefaultDriver,
	BOOL bPersistantInterface,
	BOOL bPersistantBuffers)
{
	gldLogMessage(GLDLOG_SYSTEM, "_ResizeDrawable_NOP\n");
	return TRUE;
}

//---------------------------------------------------------------------------

static BOOL _DestroyDrawable_NOP(
	GLD_ctx *ctx)
{
	gldLogMessage(GLDLOG_SYSTEM, "_DestroyDrawable_NOP\n");
	return TRUE;
}

//---------------------------------------------------------------------------

static BOOL _CreatePrivateGlobals_NOP(void)
{
	gldLogMessage(GLDLOG_SYSTEM, "_CreatePrivateGlobals_NOP\n");
	return TRUE;
}

//---------------------------------------------------------------------------

static BOOL _DestroyPrivateGlobals_NOP(void)
{
	gldLogMessage(GLDLOG_SYSTEM, "_DestroyPrivateGlobals_NOP\n");
	return TRUE;
}

//---------------------------------------------------------------------------

static GLD_pixelFormat pfNOP =
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
	0,	// Driver data
};

static BOOL _BuildPixelformatList_NOP(void)
{
	gldLogMessage(GLDLOG_SYSTEM, "_BuildPixelformatList_NOP\n");

	glb.nPixelFormatCount	= 1;
	glb.lpPF				= (GLD_pixelFormat *)calloc(1, sizeof(GLD_pixelFormat));
	glb.lpPF[0]				= pfNOP;

	return TRUE;
}

//---------------------------------------------------------------------------


static BOOL _InitialiseMesa_NOP(
	GLD_ctx *ctx)
{
	gldLogMessage(GLDLOG_SYSTEM, "_InitialiseMesa_NOP\n");
	return TRUE;
}

//---------------------------------------------------------------------------

static BOOL	_SwapBuffers_NOP(
	GLD_ctx *ctx,
	HDC hDC,
	HWND hWnd)
{
	gldLogMessage(GLDLOG_SYSTEM, "_SwapBuffers_NOP\n");
	return TRUE;
}

//---------------------------------------------------------------------------

static PROC _GetProcAddress_NOP(
	LPCSTR a)
{
	gldLogMessage(GLDLOG_SYSTEM, "_GetProcAddress_NOP\n");
	return NULL;
}

//---------------------------------------------------------------------------

static BOOL	_GetDisplayMode_NOP(
	GLD_ctx *ctx,
	GLD_displayMode *glddm)
{
	// Fill with something...
	glddm->Width	= 1024;
	glddm->Height	= 768;
	glddm->BPP		= 16;
	glddm->Refresh	= 100;

	gldLogMessage(GLDLOG_SYSTEM, "_GetDisplayMode_NOP\n");
	return TRUE;
}

//---------------------------------------------------------------------------
// Init function. Should be called as soon as regkeys/ini-settings are read.
//---------------------------------------------------------------------------

BOOL gldInitDriverPointers(
	DWORD dwDriver)
{
	_gldDriver.GetDXErrorString	= gldGetDXErrorString_DX;

#if 0
	// FOR DEBUGGING!
	_gldDriver.CreateDrawable			= _CreateDrawable_NOP;
	_gldDriver.ResizeDrawable			= _ResizeDrawable_NOP;
	_gldDriver.DestroyDrawable			= _DestroyDrawable_NOP;
	_gldDriver.CreatePrivateGlobals		= _CreatePrivateGlobals_NOP;
	_gldDriver.DestroyPrivateGlobals	= _DestroyPrivateGlobals_NOP;
	_gldDriver.BuildPixelformatList		= _BuildPixelformatList_NOP;
	_gldDriver.InitialiseMesa			= _InitialiseMesa_NOP;
	_gldDriver.SwapBuffers				= _SwapBuffers_NOP;
	_gldDriver.wglGetProcAddress		= _GetProcAddress_NOP;
	_gldDriver.GetDisplayMode			= _GetDisplayMode_NOP;
	return TRUE;
#endif

	if (dwDriver == GLDS_DRIVER_MESA_SW) {
		// Mesa Software driver
		return FALSE;
	}
	
	if ((dwDriver == GLDS_DRIVER_REF) || (dwDriver == GLDS_DRIVER_HAL)) {
		// Direct3D driver, either HW or SW
		_gldDriver.CreateDrawable			= gldCreateDrawable_DX;
		_gldDriver.ResizeDrawable			= gldResizeDrawable_DX;
		_gldDriver.DestroyDrawable			= gldDestroyDrawable_DX;
		_gldDriver.CreatePrivateGlobals		= gldCreatePrivateGlobals_DX;
		_gldDriver.DestroyPrivateGlobals	= gldDestroyPrivateGlobals_DX;
		_gldDriver.BuildPixelformatList		= gldBuildPixelformatList_DX;
		_gldDriver.InitialiseMesa			= gldInitialiseMesa_DX;
		_gldDriver.SwapBuffers				= gldSwapBuffers_DX;
		_gldDriver.wglGetProcAddress		= gldGetProcAddress_DX;
		_gldDriver.GetDisplayMode			= gldGetDisplayMode_DX;
		return TRUE;
	};

	return FALSE;
}

//---------------------------------------------------------------------------
