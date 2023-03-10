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
* Description:  OpenGL window  functions (wgl*).
*
*********************************************************************************/

#include "gld_wgl.h"
#include "gld_driver.h"

#include "glu.h"	// MUST USE MICROSOFT'S GLU32!

// Need to export wgl* functions if using GLD3,
// otherwise export GLD2 GLD_* functions.
#define _GLD_WGL_EXPORT(a) wgl##a

// Calls into Mesa 4.x are different
#include "dlist.h"
#include "drawpix.h"
#include "get.h"
#include "matrix.h"
// NOTE: All the _GLD* macros now call the gl* functions direct.
//       This ensures that the correct internal pathway is taken. KeithH
#define _GLD_glNewList		glNewList
#define _GLD_glBitmap		glBitmap
#define _GLD_glEndList		glEndList
#define _GLD_glDeleteLists	glDeleteLists
#define _GLD_glGetError		glGetError
#define _GLD_glTranslatef	glTranslatef
#define _GLD_glBegin		glBegin
#define _GLD_glVertex2fv	glVertex2fv
#define _GLD_glEnd			glEnd
#define _GLD_glNormal3f		glNormal3f
#define _GLD_glVertex3f		glVertex3f
#define _GLD_glVertex3fv	glVertex3fv

// ***********************************************************************

#define __wglMalloc(a) GlobalAlloc(GPTR, (a))
#define __wglFree(a) GlobalFree((a))

// ***********************************************************************

// Mesa glu.h and MS glu.h call these different things...
//#define GLUtesselator GLUtriangulatorObj
//#define GLU_TESS_VERTEX_DATA GLU_VERTEX_DATA

// For wglFontOutlines

typedef GLUtesselator *(APIENTRY *gluNewTessProto)(void);
typedef void (APIENTRY *gluDeleteTessProto)(GLUtesselator *tess);
typedef void (APIENTRY *gluTessBeginPolygonProto)(GLUtesselator *tess, void *polygon_data);
typedef void (APIENTRY *gluTessBeginContourProto)(GLUtesselator *tess);
typedef void (APIENTRY *gluTessVertexProto)(GLUtesselator *tess, GLdouble coords[3], void *data);
typedef void (APIENTRY *gluTessEndContourProto)(GLUtesselator *tess);
typedef void (APIENTRY *gluTessEndPolygonProto)(GLUtesselator *tess);
typedef void (APIENTRY *gluTessPropertyProto)(GLUtesselator *tess, GLenum which, GLdouble value);
typedef void (APIENTRY *gluTessNormalProto)(GLUtesselator *tess, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY *gluTessCallbackProto)(GLUtesselator *tess, GLenum which, void (CALLBACK *)());

static HINSTANCE		gluModuleHandle;
static gluNewTessProto		gluNewTessProc;
static gluDeleteTessProto	gluDeleteTessProc;
static gluTessBeginPolygonProto	gluTessBeginPolygonProc;
static gluTessBeginContourProto	gluTessBeginContourProc;
static gluTessVertexProto	gluTessVertexProc;
static gluTessEndContourProto	gluTessEndContourProc;
static gluTessEndPolygonProto	gluTessEndPolygonProc;
static gluTessPropertyProto	gluTessPropertyProc;
static gluTessNormalProto	gluTessNormalProc;
static gluTessCallbackProto	gluTessCallbackProc;

static HFONT	hNewFont, hOldFont;
static FLOAT	ScaleFactor;

#define LINE_BUF_QUANT 4000
#define VERT_BUF_QUANT 4000

static FLOAT*	LineBuf;
static DWORD	LineBufSize;
static DWORD	LineBufIndex;
static FLOAT*	VertBuf;
static DWORD	VertBufSize;
static DWORD	VertBufIndex;
static GLenum	TessErrorOccurred;

static int AppendToLineBuf(
	FLOAT value);

static int AppendToVertBuf(
	FLOAT value);

static int DrawGlyph(
	UCHAR*		glyphBuf,
	DWORD		glyphSize,
	FLOAT		chordalDeviation,
	FLOAT		extrusion,
	INT		format);

static void FreeLineBuf(void);

static void FreeVertBuf(void);

static long GetWord(
	UCHAR**		p);

static long GetDWord(
	UCHAR**		p);

static double GetFixed(
	UCHAR**		p);

static int InitLineBuf(void);

static int InitVertBuf(void);

static HFONT CreateHighResolutionFont(
	HDC		hDC);

static int MakeDisplayListFromGlyph(
	DWORD			listName,
	UCHAR*			glyphBuf,
	DWORD			glyphSize,
	LPGLYPHMETRICSFLOAT	glyphMetricsFloat,
	FLOAT			chordalDeviation,
	FLOAT			extrusion,
	INT			format);

static BOOL LoadGLUTesselator(void);
static BOOL UnloadGLUTesselator(void);

static int MakeLinesFromArc(
	FLOAT		x0,
	FLOAT		y0,
	FLOAT		x1,
	FLOAT		y1,
	FLOAT		x2,
	FLOAT		y2,
	DWORD		vertexCountIndex,
	FLOAT		chordalDeviationSquared);

static int MakeLinesFromGlyph(		UCHAR*		glyphBuf,
					DWORD		glyphSize,
					FLOAT		chordalDeviation);

static int MakeLinesFromTTLine(		UCHAR**		pp,
					DWORD		vertexCountIndex,
					WORD		pointCount);

static int MakeLinesFromTTPolycurve(	UCHAR**		pp,
					DWORD		vertexCountIndex,
					FLOAT		chordalDeviation);

static int MakeLinesFromTTPolygon(	UCHAR**		pp,
					FLOAT		chordalDeviation);

static int MakeLinesFromTTQSpline(	UCHAR**		pp,
					DWORD		vertexCountIndex,
					WORD		pointCount,
					FLOAT		chordalDeviation);

static void CALLBACK TessCombine(	double		coords[3],
					void*		vertex_data[4],
					FLOAT		weight[4],
					void**		outData);

static void CALLBACK TessError(		GLenum		error);

static void CALLBACK TessVertexOutData(	FLOAT		p[3],
					GLfloat 	z);

// ***********************************************************************

#ifdef GLD_THREADS
#pragma message("compiling GLD_WGL.C vars for multi-threaded support")
extern CRITICAL_SECTION CriticalSection;
extern DWORD dwTLSPixelFormat;			// TLS index for current pixel format
#endif
int curPFD = 0;							// Current PFD (static)

// ***********************************************************************

int gldGetPixelFormat(void)
{
#ifdef GLD_THREADS
	int iPixelFormat;
	// get thread-specific instance
	if (glb.bMultiThreaded) {
		__try {
			iPixelFormat = (int)TlsGetValue(dwTLSPixelFormat);
		}
		__except(EXCEPTION_EXECUTE_HANDLER) {
			iPixelFormat = curPFD;
		}
	}
	// get global static var
	else {
		iPixelFormat = curPFD;
	}
	return iPixelFormat;
#else
	return curPFD;
#endif
}

// ***********************************************************************

void gldSetPixelFormat(int iPixelFormat)
{
#ifdef GLD_THREADS
	// set thread-specific instance
	if (glb.bMultiThreaded) {
		__try {
			TlsSetValue(dwTLSPixelFormat, (LPVOID)iPixelFormat);
		}
		__except(EXCEPTION_EXECUTE_HANDLER) {
			curPFD = iPixelFormat;
		}
	}
	// set global static var
	else {
		curPFD = iPixelFormat;
	}
#else
	curPFD = iPixelFormat;
#endif
}

// ***********************************************************************

int APIENTRY _GLD_WGL_EXPORT(ChoosePixelFormat)(
	HDC a,
	CONST PIXELFORMATDESCRIPTOR *ppfd)
{
	GLD_pixelFormat			*lpPF = glb.lpPF;

    PIXELFORMATDESCRIPTOR	ppfdBest;
    int						i;
	int						bestIndex = -1;
    int						numPixelFormats;
	DWORD					dwFlags;

	char					buf[128];
	char					cat[8];

	DWORD dwAllFlags = 
					PFD_DRAW_TO_WINDOW |
					PFD_DRAW_TO_BITMAP |
					PFD_SUPPORT_GDI |
					PFD_SUPPORT_OPENGL |
					PFD_GENERIC_FORMAT |
					PFD_NEED_PALETTE |
					PFD_NEED_SYSTEM_PALETTE |
					PFD_DOUBLEBUFFER |
					/*PFD_STEREO |*/
					/*PFD_SWAP_LAYER_BUFFERS |*/
					PFD_DOUBLEBUFFER_DONTCARE |
					/*PFD_STEREO_DONTCARE |*/
					PFD_SWAP_COPY |
					PFD_SWAP_EXCHANGE |
					PFD_GENERIC_ACCELERATED |
					0;

	// Validate license
	if (!gldValidate())
		return 0;

	// List may not be built until gldValidate() is called! KeithH
	lpPF = glb.lpPF;

	//
	// Lets print the input pixel format to the log
	// ** Based on "wglinfo" by Nate Robins **
	//
	gldLogMessage(GLDLOG_SYSTEM, "ChoosePixelFormat:\n");
	gldLogMessage(GLDLOG_INFO, "Input pixel format for ChoosePixelFormat:\n");
	gldLogMessage(GLDLOG_INFO,
		"   visual  x  bf lv rg d st  r  g  b a  ax dp st accum buffs  ms\n");
	gldLogMessage(GLDLOG_INFO,
		" id dep cl sp sz l  ci b ro sz sz sz sz bf th cl  r  g  b  a ns b\n");
	gldLogMessage(GLDLOG_INFO,
		"-----------------------------------------------------------------\n");
	sprintf(buf, "  .  ");

	sprintf(cat, "%2d ", ppfd->cColorBits);
	strcat(buf, cat);
	if(ppfd->dwFlags & PFD_DRAW_TO_WINDOW)      sprintf(cat, "wn ");
	else if(ppfd->dwFlags & PFD_DRAW_TO_BITMAP) sprintf(cat, "bm ");
	else sprintf(cat, ".  ");
	strcat(buf, cat);

	/* should find transparent pixel from LAYERPLANEDESCRIPTOR */
	sprintf(cat, " . "); 
	strcat(buf, cat);

	sprintf(cat, "%2d ", ppfd->cColorBits);
	strcat(buf, cat);

	/* bReserved field indicates number of over/underlays */
	if(ppfd->bReserved) sprintf(cat, " %d ", ppfd->bReserved);
	else sprintf(cat, " . "); 
	strcat(buf, cat);

	sprintf(cat, " %c ", ppfd->iPixelType == PFD_TYPE_RGBA ? 'r' : 'c');
	strcat(buf, cat);

	sprintf(cat, "%c ", ppfd->dwFlags & PFD_DOUBLEBUFFER ? 'y' : '.');
	strcat(buf, cat);

	sprintf(cat, " %c ", ppfd->dwFlags & PFD_STEREO ? 'y' : '.');
	strcat(buf, cat);

	if(ppfd->cRedBits && ppfd->iPixelType == PFD_TYPE_RGBA) 
	    sprintf(cat, "%2d ", ppfd->cRedBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);

	if(ppfd->cGreenBits && ppfd->iPixelType == PFD_TYPE_RGBA) 
	    sprintf(cat, "%2d ", ppfd->cGreenBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);

	if(ppfd->cBlueBits && ppfd->iPixelType == PFD_TYPE_RGBA) 
	    sprintf(cat, "%2d ", ppfd->cBlueBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);
	
	if(ppfd->cAlphaBits && ppfd->iPixelType == PFD_TYPE_RGBA) 
		sprintf(cat, "%2d ", ppfd->cAlphaBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);
	
	if(ppfd->cAuxBuffers)     sprintf(cat, "%2d ", ppfd->cAuxBuffers);
	else sprintf(cat, " . ");
	strcat(buf, cat);
	
	if(ppfd->cDepthBits)      sprintf(cat, "%2d ", ppfd->cDepthBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);
	
	if(ppfd->cStencilBits)    sprintf(cat, "%2d ", ppfd->cStencilBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);
	
	if(ppfd->cAccumRedBits)   sprintf(cat, "%2d ", ppfd->cAccumRedBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);

	if(ppfd->cAccumGreenBits) sprintf(cat, "%2d ", ppfd->cAccumGreenBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);
	
	if(ppfd->cAccumBlueBits)  sprintf(cat, "%2d ", ppfd->cAccumBlueBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);
	
	if(ppfd->cAccumAlphaBits) sprintf(cat, "%2d ", ppfd->cAccumAlphaBits);
	else sprintf(cat, " . ");
	strcat(buf, cat);
	
	/* no multisample in Win32 */
	sprintf(cat, " . .\n");
	strcat(buf, cat);

	gldLogMessage(GLDLOG_INFO, buf);
	gldLogMessage(GLDLOG_INFO,
		"-----------------------------------------------------------------\n");
	gldLogMessage(GLDLOG_INFO, "\n");

	//
	// Examine the flags for correctness
	//
	dwFlags = ppfd->dwFlags;
    if (dwFlags != (dwFlags & dwAllFlags))
    {
		/* error: bad dwFlags */
		gldLogPrintf(GLDLOG_WARN,
					"ChoosePixelFormat: bad flags (0x%x)",
					dwFlags & (~dwAllFlags));
		// Mask illegal flags and continue
		dwFlags = dwFlags & dwAllFlags;
    }
	
    switch (ppfd->iPixelType) {
    case PFD_TYPE_RGBA:
    case PFD_TYPE_COLORINDEX:
		break;
    default:
		/* error: bad iPixelType */
		gldLogMessage(GLDLOG_WARN, "ChoosePixelFormat: bad pixel type\n");
		return 0;
    }
	
    switch (ppfd->iLayerType) {
    case PFD_MAIN_PLANE:
    case PFD_OVERLAY_PLANE:
    case PFD_UNDERLAY_PLANE:
		break;
    default:
		/* error: bad iLayerType */
		gldLogMessage(GLDLOG_WARN, "ChoosePixelFormat: bad layer type\n");
		return 0;
    }
	
    numPixelFormats = glb.nPixelFormatCount;
	
    /* loop through candidate pixel format descriptors */
    for (i=0; i<numPixelFormats; ++i) {
		PIXELFORMATDESCRIPTOR ppfdCandidate;
		
		memcpy(&ppfdCandidate, &lpPF[i].pfd, sizeof(PIXELFORMATDESCRIPTOR));
		
		/*
		** Check attributes which must match
		*/
		if (ppfd->iPixelType != ppfdCandidate.iPixelType) {
			continue;
		}

		if (ppfd->iLayerType != ppfdCandidate.iLayerType) {
			continue;
		}
		
		if (((dwFlags ^ ppfdCandidate.dwFlags) & dwFlags) &
			(PFD_DRAW_TO_WINDOW | PFD_DRAW_TO_BITMAP |
			PFD_SUPPORT_GDI | PFD_SUPPORT_OPENGL))
		{
			continue;
		}
		
		if (!(dwFlags & PFD_DOUBLEBUFFER_DONTCARE)) {
			if ((dwFlags & PFD_DOUBLEBUFFER) !=
				(ppfdCandidate.dwFlags & PFD_DOUBLEBUFFER))
			{
				continue;
			}
		}
		
        if (ppfd->iPixelType==PFD_TYPE_RGBA
            && ppfd->cAlphaBits && !ppfdCandidate.cAlphaBits) {
            continue;
		}
		
        if (ppfd->iPixelType==PFD_TYPE_RGBA
			&& ppfd->cAccumBits && !ppfdCandidate.cAccumBits) {
			continue;
        }
		
        if (ppfd->cDepthBits && !ppfdCandidate.cDepthBits) {
			continue;
        }
		
        if (ppfd->cStencilBits && !ppfdCandidate.cStencilBits) {
            continue;
        }

		if (ppfd->cAuxBuffers && !ppfdCandidate.cAuxBuffers) {
			continue;
		}
		
		/*
		** See if candidate is better than the previous best choice
		*/
		if (bestIndex == -1) {
			ppfdBest = ppfdCandidate;
			bestIndex = i;
			continue;
		}
		
		if ((ppfd->cColorBits > ppfdBest.cColorBits &&
			ppfdCandidate.cColorBits > ppfdBest.cColorBits) ||
			(ppfd->cColorBits <= ppfdCandidate.cColorBits &&
			ppfdCandidate.cColorBits < ppfdBest.cColorBits))
		{
			ppfdBest = ppfdCandidate;
			bestIndex = i;
			continue;
		}
		
		if (ppfd->iPixelType==PFD_TYPE_RGBA
            && ppfd->cAlphaBits
            && ppfdCandidate.cAlphaBits > ppfdBest.cAlphaBits)
		{
			ppfdBest = ppfdCandidate;
			bestIndex = i;
			continue;
		}
		
		if (ppfd->iPixelType==PFD_TYPE_RGBA
			&& ppfd->cAccumBits
            && ppfdCandidate.cAccumBits > ppfdBest.cAccumBits)
		{
			ppfdBest = ppfdCandidate;
			bestIndex = i;
			continue;
		}
		
		if ((ppfd->cDepthBits > ppfdBest.cDepthBits &&
			ppfdCandidate.cDepthBits > ppfdBest.cDepthBits) ||
			(ppfd->cDepthBits <= ppfdCandidate.cDepthBits &&
			ppfdCandidate.cDepthBits < ppfdBest.cDepthBits))
		{
			ppfdBest = ppfdCandidate;
			bestIndex = i;
			continue;
		}
		
		if (ppfd->cStencilBits &&
			ppfdCandidate.cStencilBits > ppfdBest.cStencilBits)
		{
			ppfdBest = ppfdCandidate;
			bestIndex = i;
			continue;
		}
		
		if (ppfd->cAuxBuffers &&
			ppfdCandidate.cAuxBuffers > ppfdBest.cAuxBuffers)
		{
			ppfdBest = ppfdCandidate;
			bestIndex = i;
			continue;
		}
    }

	if (bestIndex != -1) {
		gldLogPrintf(GLDLOG_SYSTEM, "Pixel Format %d chosen as best match", bestIndex+1);
	    return bestIndex + 1;
	}

	// Return the pixelformat that has the most capabilities.

	// ** NOTE: This is only possible due to the way the list
	// of pixelformats is built. **

	// Now picks best pixelformat. KeithH
	bestIndex = numPixelFormats;	// most capable double buffer format
	gldLogPrintf(GLDLOG_SYSTEM, "Pixel Format %d chosen by default", bestIndex);
	return (bestIndex);
}

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(CopyContext)(
	HGLRC a,
	HGLRC b,
	UINT c)
{
	// Validate license
	if (!gldValidate())
		return FALSE;
    UNSUPPORTED("wglCopyContext")
    return FALSE; // Failed
}

// ***********************************************************************

HGLRC APIENTRY _GLD_WGL_EXPORT(CreateContext)(
	HDC a)
{
	int ipf;

	// Validate license
	if (!gldValidate())
		return 0;

	// Check that the current PFD is valid
	ipf = gldGetPixelFormat();
	if (!IsValidPFD(ipf))
		return (HGLRC)0;

	return gldCreateContext(a, &glb.lpPF[ipf-1]);
}

// ***********************************************************************

HGLRC APIENTRY _GLD_WGL_EXPORT(CreateLayerContext)(
	HDC a,
	int b)
{
	// Validate license
	if (!gldValidate())
		return 0;

    UNSUPPORTED("wglCreateLayerContext")
    return NULL; // Failed
}

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(DeleteContext)(
	HGLRC a)
{
	// Validate license
	if (!gldValidate())
		return FALSE;

    return gldDeleteContext(a);
}

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(DescribeLayerPlane)(
	HDC hDC,
	int iPixelFormat,
	int iLayerPlane,
	UINT nBytes,
	LPLAYERPLANEDESCRIPTOR plpd)
{
	// Validate license
	if (!gldValidate())
		return FALSE;

	UNSUPPORTED("GLD_DescribeLayerPlane")

//	gldLogPrintf(GLDLOG_INFO, "DescribeLayerPlane: %d, %d", iPixelFormat, iLayerPlane);

	return FALSE;
}

// ***********************************************************************

int APIENTRY _GLD_WGL_EXPORT(DescribePixelFormat)(
	HDC a,
	int b,
	UINT c,
	LPPIXELFORMATDESCRIPTOR d)
{
	UINT nSize;

	// Validate license
	if (!gldValidate())
		return 0;

	if (d == NULL) // Calling app requires max number of PF's
		return glb.nPixelFormatCount;

	// The supplied buffer may be larger than the info that we
	// will be copying.
	if (c > sizeof(PIXELFORMATDESCRIPTOR))
		nSize = sizeof(PIXELFORMATDESCRIPTOR);
	else
		nSize = c;

    // Setup an empty PFD before doing validation check
    memset(d, 0, nSize);
    d->nSize = nSize;
    d->nVersion = 1;

	if (!IsValidPFD(b))
		return 0; // Bail if PFD index is invalid

	memcpy(d, &glb.lpPF[b-1].pfd, nSize);

	return glb.nPixelFormatCount;
}

// ***********************************************************************

HGLRC APIENTRY _GLD_WGL_EXPORT(GetCurrentContext)(void)
{
	// Validate license
	if (!gldValidate())
		return 0;

	return gldGetCurrentContext();
}

// ***********************************************************************

HDC APIENTRY _GLD_WGL_EXPORT(GetCurrentDC)(void)
{
	// Validate license
	if (!gldValidate())
		return 0;

	return gldGetCurrentDC();
}

// ***********************************************************************

PROC APIENTRY _GLD_WGL_EXPORT(GetDefaultProcAddress)(
	LPCSTR a)
{
	// Validate license
	if (!gldValidate())
		return NULL;

    UNSUPPORTED("GLD_GetDefaultProcAddress")
    return NULL;
}

// ***********************************************************************

int APIENTRY _GLD_WGL_EXPORT(GetLayerPaletteEntries)(
	HDC a,
	int b,
	int c,
	int d,
	COLORREF *e)
{
	// Validate license
	if (!gldValidate())
		return 0;

    UNSUPPORTED("GLD_GetLayerPaletteEntries")
    return 0;
}

// ***********************************************************************

int APIENTRY _GLD_WGL_EXPORT(GetPixelFormat)(
	HDC a)
{
	// Validate license
	if (!gldValidate())
		return 0;

	return gldGetPixelFormat();
}

// ***********************************************************************

PROC APIENTRY _GLD_WGL_EXPORT(GetProcAddress)(
	LPCSTR a)
{
	PROC gldGetProcAddressD3D(LPCSTR a);

	// Validate license
	if (!gldValidate())
		return NULL;

	return _gldDriver.wglGetProcAddress(a);
}

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(MakeCurrent)(
	HDC a,
	HGLRC b)
{
	// Validate license
	if (!gldValidate())
		return FALSE;

	return gldMakeCurrent(a, b);
}

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(RealizeLayerPalette)(
	HDC a,
	int b,
	BOOL c)
{
	// Validate license
	if (!gldValidate())
		return FALSE;

    UNSUPPORTED("GLD_RealizeLayerPalette")
	return FALSE;
}

// ***********************************************************************

int APIENTRY _GLD_WGL_EXPORT(SetLayerPaletteEntries)(
	HDC a,
	int b,
	int c,
	int d,
	CONST COLORREF *e)
{
	// Validate license
	if (!gldValidate())
		return 0;

    UNSUPPORTED("GLD_SetLayerPaletteEntries")
	return 0;
}

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(SetPixelFormat)(
	HDC a,
	int b,
	CONST PIXELFORMATDESCRIPTOR *c)
{
	// Validate license
	if (!gldValidate())
		return FALSE;

	if (IsValidPFD(b)) {
		gldLogPrintf(GLDLOG_SYSTEM, "SetPixelFormat: PixelFormat %d has been set", b);
		gldSetPixelFormat(b);
		return TRUE;
	} else {
		gldLogPrintf(GLDLOG_ERROR,
					"SetPixelFormat: PixelFormat %d is invalid and cannot be set", b);
		return FALSE;
	}
}

//
// ** DO NOT SHARE DISPLAY LISTS WHEN USING CURRENT GLDIRECT5 DISPLAY LIST CODE **
//

#if 0
// ***********************************************************************
/*
 * Share lists between two gl_context structures.
 * This was added for WIN32 WGL function support, since wglShareLists()
 * must be called *after* wglCreateContext() with valid GLRCs. (DaveM)
 */
//
// Copied from GLD2.x. KeithH
//
static GLboolean _gldShareLists(
	GLcontext *ctx1,
	GLcontext *ctx2)
{
	/* Sanity check context pointers */
	if (ctx1 == NULL || ctx2 == NULL)
		return GL_FALSE;
	/* Sanity check shared list pointers */
	if (ctx1->Shared == NULL || ctx2->Shared == NULL)
		return GL_FALSE;
	/* Decrement reference count on sharee to release previous list */
	ctx2->Shared->RefCount--;
#if 0	/* 3DStudio exits on this memory release */
	if (ctx2->Shared->RefCount == 0)
		free_shared_state(ctx2, ctx2->Shared);
#endif
	/* Re-assign list from sharer to sharee and increment reference count */
	ctx2->Shared = ctx1->Shared;
	ctx1->Shared->RefCount++;
	return GL_TRUE;
}

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(ShareLists)(
	HGLRC a,
	HGLRC b)
{
	GLD_ctx *gld1, *gld2;

	// Validate license
	if (!gldValidate())
		return FALSE;

	// Mesa supports shared lists, but you need to supply the shared
	// GL context info when calling gl_create_context(). An auxiliary
	// function gl_share_lists() has been added to update the shared
	// list info after the GL contexts have been created. (DaveM)
	gld1 = gldGetContextAddress(a);
	gld2 = gldGetContextAddress(b);
	if (gld1->bAllocated && gld2->bAllocated) {
		return _gldShareLists(gld1->glCtx, gld2->glCtx);
	}
	return FALSE;
}
#else

// ***********************************************************************

//
// ** DO NOT SHARE DISPLAY LISTS WHEN USING CURRENT GLDIRECT5 DISPLAY LIST CODE **
//
// GLdirect5 includes optimised display list support, but the use of device-specific
// data embedded into the display list opcodes precludes the sharing of display lists.
// KeithH.
//

BOOL APIENTRY _GLD_WGL_EXPORT(ShareLists)(
	HGLRC a,
	HGLRC b)
{
	return FALSE;
}
#endif

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(SwapBuffers)(
	HDC a)
{
	// Validate license
	if (!gldValidate())
		return FALSE;

	return gldSwapBuffers(a);
}

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(SwapLayerBuffers)(
	HDC a,
	UINT b)
{
	// Validate license
	if (!gldValidate())
		return FALSE;

	return gldSwapBuffers(a);
}

// ***********************************************************************

// ***********************************************************************
// Note: This ResizeBuffers() function may be called from
// either MESA glViewport() or GLD wglMakeCurrent().

BOOL gldWglResizeBuffers(
	GLcontext *ctx,
	BOOL bDefaultDriver)
{
	GLD_ctx						*gld = NULL;
	RECT						rcScreenRect;
	DWORD						dwWidth;
	DWORD						dwHeight;
	IDirectDrawClipper			*lpddClipper = NULL;

	BOOL						bWasFullscreen;
	BOOL						bSaveDesktop;
	BOOL						bFullScrnWin = FALSE;

	GLD_displayMode				glddm;

#define GLDLOG_CRITICAL_OR_WARN	(bDefaultDriver ? GLDLOG_WARN : DDLOG_CRITICAL)

	// Validate license
	if (!gldValidate())
		return FALSE;

	// Sanity checks
	if (ctx == NULL)
		return FALSE;
	gld = (GLD_ctx*)ctx->DriverCtx;
	if (gld == NULL)
		return FALSE;

	// Get the window size and calculate its dimensions
	if (gld->hWnd == NULL) {
		// Check for non-window DC = memory DC ?
		if (GetClipBox(gld->hDC, &rcScreenRect) == ERROR)
			SetRect(&rcScreenRect, 0, 0, 0, 0);
	}
	else if (!GetClientRect(gld->hWnd, &rcScreenRect))
		SetRect(&rcScreenRect, 0, 0, 0, 0);
	dwWidth = rcScreenRect.right - rcScreenRect.left;
	dwHeight = rcScreenRect.bottom - rcScreenRect.top;
    CopyRect(&gld->rcScreenRect, &rcScreenRect);

	// This will occur on Alt-Tab
	if ((dwWidth == 0) && (dwHeight == 0)) {
		//gld->bCanRender = FALSE;
		return TRUE; // No resize possible!
	}

	// Some apps zero only 1 dimension for non-visible window... (DaveM)
	if ((dwWidth == 0) || (dwHeight == 0)) {
		dwWidth = 8;
		dwHeight = 8;
	}

	// Test to see if a resize is required.
	// Note that the dimensions will be the same if a prior resize attempt failed.
	if ((dwWidth == gld->dwWidth) && (dwHeight == gld->dwHeight) && bDefaultDriver) {
		return TRUE; // No resize required
	}

	gldLogPrintf(GLDLOG_SYSTEM, "gldResize: %dx%d", dwWidth, dwHeight);

	// Note previous fullscreen vs window display status
	bWasFullscreen = gld->bFullscreen;

	if (_gldDriver.GetDisplayMode(gld, &glddm)) {
		if ( (dwWidth == glddm.Width) &&
				 (dwHeight == glddm.Height) ) {
			bFullScrnWin = TRUE;
		}
		// Exclude MDI windows from fullscreen page-flipping. (DaveM)
		if (bFullScrnWin && glb.bPrimary && !glb.bFullscreenBlit && !glb.bDirectDrawPersistant
				&& gld->hWnd == glb.hWndActive) {
			gld->bFullscreen = TRUE;
			gldLogMessage(GLDLOG_INFO, "Fullscreen window after resize.\n");
		}
		else {
			gld->bFullscreen = FALSE;
			gldLogMessage(GLDLOG_INFO, "Non-Fullscreen window after resize.\n");
		}
		// Cache the display mode dimensions
		gld->dwModeWidth = glddm.Width;
		gld->dwModeHeight = glddm.Height;
	}

	// Clamp the effective window dimensions to primary surface.
	// We need to do this for D3D viewport dimensions even if wide
	// surfaces are supported. This also is a good idea for handling
	// whacked-out window dimensions passed for non-drawable windows
	// like Solid Edge. (DaveM)
	if (gld->dwWidth > glddm.Width)
		gld->dwWidth = glddm.Width;
	if (gld->dwHeight > glddm.Height)
		gld->dwHeight = glddm.Height;

	// Note if fullscreen vs window display has changed?
	bSaveDesktop = (!bWasFullscreen && !gld->bFullscreen) ? TRUE : FALSE;
	// Save the desktop primary surface from being destroyed
	// whenever remaining in windowed mode, since the stereo mode
	// switches are expensive...

	gld->bCanRender = FALSE;

#ifdef GLD_THREADS
	// Serialize access to DirectDraw and DDS operations
	if (glb.bMultiThreaded)
		EnterCriticalSection(&CriticalSection);
#endif

	gld->dwWidth = dwWidth;
	gld->dwHeight = dwHeight;

	// Set defaults
	gld->dwModeWidth = gld->dwWidth;
	gld->dwModeHeight = gld->dwHeight;

	// Only safe to resize when remaining in windowed mode. (DaveM)
	if (bSaveDesktop) { 
		if (!_gldDriver.ResizeDrawable(gld, bDefaultDriver, glb.bDirectDrawPersistant, glb.bPersistantBuffers))
		goto cleanup_and_return_with_error;
	}
	// Otherwise re-create D3D rendering context.
	else {
		BOOL bFullscreen = gld->bFullscreen;
		gld->bFullscreen = bWasFullscreen;
		_gldDriver.DestroyDrawable(gld);
		gld->bFullscreen = bFullscreen;
		if (!_gldDriver.CreateDrawable(gld, glb.bDirectDrawPersistant, glb.bPersistantBuffers))
		goto cleanup_and_return_with_error;
	}

	gld->bCanRender = TRUE;

#ifdef GLD_THREADS
	// Release serialized access
	if (glb.bMultiThreaded)
		LeaveCriticalSection(&CriticalSection);
#endif

	// SUCCESS.
	return TRUE;

cleanup_and_return_with_error:
	// Relase all interfaces before returning.
	_gldDriver.DestroyDrawable(gld);	// severe... (DaveM)

	// Mark context as not being able to render
	gld->bCanRender = FALSE;

#ifdef GLD_THREADS
	// Release serialized access
	if (glb.bMultiThreaded)
		LeaveCriticalSection(&CriticalSection);
#endif

	return FALSE;
}

// ***********************************************************************
// ***********************************************************************
// Support for bitmap fonts.
// ***********************************************************************
// ***********************************************************************

/*****************************************************************************
**
** InvertGlyphBitmap.
**
** Invert the bitmap so that it suits OpenGL's representation.
** Each row starts on a double word boundary.
**
*****************************************************************************/

static void InvertGlyphBitmap(
	int w,
	int h,
	DWORD *fptr,
	DWORD *tptr)
{
	int dWordsInRow = (w+31)/32;
	int i, j;
	DWORD *tmp = tptr;

	if (w <= 0 || h <= 0) {
	return;
	}

	tptr += ((h-1)*dWordsInRow);
	for (i = 0; i < h; i++) {
	for (j = 0; j < dWordsInRow; j++) {
		*(tptr + j) = *(fptr + j);
	}
	tptr -= dWordsInRow;
	fptr += dWordsInRow;
	}
}

// ***********************************************************************

/*****************************************************************************
 * wglUseFontBitmaps
 *
 * Converts a subrange of the glyphs in a GDI font to OpenGL display
 * lists.
 *
 * Extended to support any GDI font, not just TrueType fonts. (DaveM)
 *
 *****************************************************************************/

BOOL APIENTRY _GLD_WGL_EXPORT(UseFontBitmapsA)(
	HDC hDC,
	DWORD first,
	DWORD count,
	DWORD listBase)
{
	int					i, ox, oy, ix, iy;
	int					w, h;
	int					iBufSize, iCurBufSize = 0;
	DWORD				*bitmapBuffer = NULL;
	DWORD				*invertedBitmapBuffer = NULL;
	BOOL				bSuccessOrFail = TRUE;
	BOOL				bTrueType = FALSE;
	TEXTMETRIC			tm;
	GLYPHMETRICS		gm;
	RASTERIZER_STATUS	rs;
	MAT2				mat;
	SIZE				size;
	RECT				rect;
	HDC					hDCMem;
	HBITMAP				hBitmap;
	BITMAPINFO			bmi;
	HFONT				hFont;

	// Validate SciTech GLDirect license
	if (!gldValidate())
		return FALSE;

	// Set up a unity matrix.
	ZeroMemory(&mat, sizeof(mat));
	mat.eM11.value = 1;
	mat.eM22.value = 1;

	// Test to see if selected font is TrueType or not
	ZeroMemory(&tm, sizeof(tm));
	if (!GetTextMetrics(hDC, &tm)) {
		gldLogMessage(GLDLOG_ERROR, "GLD_UseFontBitmaps: Font metrics error\n");
		return (FALSE);
	}
	bTrueType = (tm.tmPitchAndFamily & TMPF_TRUETYPE) ? TRUE : FALSE;

	// Test to see if TRUE-TYPE capabilities are installed
	// (only necessary if TrueType font selected)
	ZeroMemory(&rs, sizeof(rs));
	if (bTrueType) {
		if (!GetRasterizerCaps (&rs, sizeof (RASTERIZER_STATUS))) {
			gldLogMessage(GLDLOG_ERROR, "GLD_UseFontBitmaps: Raster caps error\n");
			return (FALSE);
		}
		if (!(rs.wFlags & TT_ENABLED)) {
			gldLogMessage(GLDLOG_ERROR, "GLD_UseFontBitmaps: No TrueType caps\n");
			return (FALSE);
		}
	}

	// Trick to get the current font handle
	hFont = (HFONT)SelectObject(hDC, GetStockObject(SYSTEM_FONT));
	SelectObject(hDC, hFont);

	// Have memory device context available for holding bitmaps of font glyphs
	hDCMem = CreateCompatibleDC(hDC);
	SelectObject(hDCMem, hFont);
	SetTextColor(hDCMem, RGB(0xFF, 0xFF, 0xFF));
	SetBkColor(hDCMem, 0);

	for (i = first; (DWORD) i < (first + count); i++) {
		// Find out how much space is needed for the bitmap so we can
		// Set the buffer size correctly.
		if (bTrueType) {
			// Use TrueType support to get bitmap size of glyph
			iBufSize = GetGlyphOutline(hDC, i, GGO_BITMAP, &gm,
				0, NULL, &mat);
			if (iBufSize == GDI_ERROR) {
				bSuccessOrFail = FALSE;
				break;
			}
		}
		else {
			// Use generic GDI support to compute bitmap size of glyph
			w = tm.tmMaxCharWidth;
			h = tm.tmHeight;
			if (GetTextExtentPoint32(hDC, (LPCTSTR)&i, 1, &size)) {
				w = size.cx;
				h = size.cy;
			}
			iBufSize = w * h;
			// Use DWORD multiple for compatibility
			iBufSize += 3;
			iBufSize /= 4;
			iBufSize *= 4;
		}

		// If we need to allocate Larger Buffers, then do so - but allocate
		// An extra 50 % so that we don't do too many mallocs !
		if (iBufSize > iCurBufSize) {
			if (bitmapBuffer) {
				__wglFree(bitmapBuffer);
			}
			if (invertedBitmapBuffer) {
				__wglFree(invertedBitmapBuffer);
			}

			iCurBufSize = iBufSize * 2;
			bitmapBuffer = (DWORD *) __wglMalloc(iCurBufSize);
			invertedBitmapBuffer = (DWORD *) __wglMalloc(iCurBufSize);

			if (bitmapBuffer == NULL || invertedBitmapBuffer == NULL) {
				bSuccessOrFail = FALSE;
				break;
			}
		}

		// If we fail to get the Glyph data, delete the display lists
		// Created so far and return FALSE.
		if (bTrueType) {
			// Use TrueType support to get bitmap of glyph
			if (GetGlyphOutline(hDC, i, GGO_BITMAP, &gm,
					iBufSize, bitmapBuffer, &mat) == GDI_ERROR) {
				bSuccessOrFail = FALSE;
				break;
			}

			// Setup glBitmap parameters for current font glyph
			w  = gm.gmBlackBoxX;
			h  = gm.gmBlackBoxY;
			ox = gm.gmptGlyphOrigin.x;
			oy = gm.gmptGlyphOrigin.y;
			ix = gm.gmCellIncX;
			iy = gm.gmCellIncY;
		}
		else {
			// Use generic GDI support to create bitmap of glyph
			ZeroMemory(bitmapBuffer, iBufSize);

			if (i >= tm.tmFirstChar && i <= tm.tmLastChar) {
				// Only create bitmaps for actual font glyphs
				hBitmap = CreateBitmap(w, h, 1, 1, NULL);
				SelectObject(hDCMem, hBitmap);
				// Make bitmap of current font glyph
				SetRect(&rect, 0, 0, w, h);
				DrawText(hDCMem, (LPCTSTR)&i, 1, &rect,
					DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_NOCLIP);
				// Make copy of bitmap in our local buffer
				ZeroMemory(&bmi, sizeof(bmi));
				bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				bmi.bmiHeader.biWidth = w;
				bmi.bmiHeader.biHeight = -h;
				bmi.bmiHeader.biPlanes = 1;
				bmi.bmiHeader.biBitCount = 1;
				bmi.bmiHeader.biCompression = BI_RGB;
				GetDIBits(hDCMem, hBitmap, 0, h, bitmapBuffer, &bmi, 0);
				DeleteObject(hBitmap);
			}
			else {
				// Otherwise use empty display list for non-existing glyph
				iBufSize = 0;
			}

			// Setup glBitmap parameters for current font glyph
			ox = 0;
			oy = tm.tmDescent;
			ix = w;
			iy = 0;
		}

		// Create an OpenGL display list.
		_GLD_glNewList((listBase + i), GL_COMPILE);

		// Some fonts have no data for the space character, yet advertise
		// a non-zero size.
		if (0 == iBufSize) {
			_GLD_glBitmap(0, 0, 0.0f, 0.0f, (GLfloat) ix, (GLfloat) iy, NULL);
		} else {
			// Invert the Glyph data.
			InvertGlyphBitmap(w, h, bitmapBuffer, invertedBitmapBuffer);

			// Render an OpenGL bitmap and invert the origin.
			_GLD_glBitmap(w, h,
				(GLfloat) ox, (GLfloat) (h-oy),
				(GLfloat) ix, (GLfloat) iy,
				(GLubyte *) invertedBitmapBuffer);
		}

		// Close this display list.
		_GLD_glEndList();
	}

	if (bSuccessOrFail == FALSE) {
		gldLogMessage(GLDLOG_ERROR, "GLD_UseFontBitmaps: Get glyph failed\n");
		_GLD_glDeleteLists((i+listBase), (i-first));
	}

	// Release resources used
	DeleteObject(hFont);
	DeleteDC(hDCMem);

	if (bitmapBuffer)
		__wglFree(bitmapBuffer);
	if (invertedBitmapBuffer)
		__wglFree(invertedBitmapBuffer);

	return(bSuccessOrFail);
}

// ***********************************************************************

BOOL APIENTRY _GLD_WGL_EXPORT(UseFontBitmapsW)(
	HDC a,
	DWORD b,
	DWORD c,
	DWORD d)
{
	// Validate license
	if (!gldValidate())
		return FALSE;

	return _GLD_WGL_EXPORT(UseFontBitmapsA)(a, b, c, d);
}

// ***********************************************************************
// ***********************************************************************
// Support for outline TrueType fonts.
// ***********************************************************************
// ***********************************************************************

void * __wglRealloc(
	void *oldPtr,
	size_t newSize)
{
    void *newPtr = NULL;
	
    if (newSize != 0) {
		newPtr = (void *) GlobalAlloc(GPTR, newSize);
		if (oldPtr && newPtr) {
			DWORD oldSize = GlobalSize(oldPtr);
			
			memcpy(newPtr, oldPtr, (oldSize <= newSize ? oldSize : newSize));
			GlobalFree(oldPtr);
		}
    } else if (oldPtr) {
		GlobalFree(oldPtr);
    }
    if (newPtr == NULL) {
		return NULL;	/* XXX out of memory error */
    }
    return newPtr;
}

// ***********************************************************************


/*****************************************************************************
 * wglUseFontOutlinesW
 *
 * Converts a subrange of the glyphs in a TrueType font to OpenGL display
 * lists.
 *****************************************************************************/

BOOL APIENTRY _GLD_WGL_EXPORT(UseFontOutlinesW)(
	IN	HDC			hDC,
	IN	DWORD			first,
	IN	DWORD			count,
	IN	DWORD			listBase,
	IN	FLOAT			chordalDeviation,
	IN	FLOAT			extrusion,
	IN	INT			format,
	OUT	LPGLYPHMETRICSFLOAT	lpgmf)
{
	return _GLD_WGL_EXPORT(UseFontOutlinesA)(hDC, first, count, listBase,
		chordalDeviation, extrusion, format, lpgmf);
}

/*****************************************************************************
 * wglUseFontOutlinesA
 *
 * Converts a subrange of the glyphs in a TrueType font to OpenGL display
 * lists.
 *****************************************************************************/

BOOL APIENTRY _GLD_WGL_EXPORT(UseFontOutlinesA)(
	IN	HDC			hDC,
			IN	DWORD			first,
			IN	DWORD			count,
			IN	DWORD			listBase,
			IN	FLOAT			chordalDeviation,
			IN	FLOAT			extrusion,
			IN	INT			format,
			OUT	LPGLYPHMETRICSFLOAT	glyphMetricsFloatArray)
	{
	DWORD	glyphIndex;
	UCHAR*	glyphBuf;
	DWORD	glyphBufSize;


	/*
	 * Flush any previous OpenGL errors.  This allows us to check for
	 * new errors so they can be reported via the function return value.
	 */
	while (_GLD_glGetError() != GL_NO_ERROR)
		;

	/*
	 * Make sure that the current font can be sampled accurately.
	 */
	hNewFont = CreateHighResolutionFont(hDC);
	if (!hNewFont)
		return FALSE;

	hOldFont = (HFONT)SelectObject(hDC, hNewFont);
	if (!hOldFont)
		return FALSE;

	/*
	 * Preallocate a buffer for the outline data, and track its size:
	 */
	glyphBuf = (UCHAR*) __wglMalloc(glyphBufSize = 10240);
	if (!glyphBuf)
		return FALSE; /*WGL_STATUS_NOT_ENOUGH_MEMORY*/

	/*
	 * Process each glyph in the given range:
	 */
	for (glyphIndex = first; glyphIndex - first < count; ++glyphIndex)
		{
		GLYPHMETRICS	glyphMetrics;
		DWORD		glyphSize;
		static MAT2	matrix =
			{
			{0, 1},		{0, 0},
			{0, 0},		{0, 1}
			};
		LPGLYPHMETRICSFLOAT glyphMetricsFloat =
			&glyphMetricsFloatArray[glyphIndex - first];


		/*
		 * Determine how much space is needed to store the glyph's
		 * outlines.  If our glyph buffer isn't large enough,
		 * resize it.
		 */
		glyphSize = GetGlyphOutline(	hDC,
						glyphIndex,
						GGO_NATIVE,
						&glyphMetrics,
						0,
						NULL,
						&matrix
						);
		if (glyphSize < 0)
			return FALSE; /*WGL_STATUS_FAILURE*/
		if (glyphSize > glyphBufSize)
			{
			__wglFree(glyphBuf);
			glyphBuf = (UCHAR*) __wglMalloc(glyphBufSize = glyphSize);
			if (!glyphBuf)
				return FALSE; /*WGL_STATUS_NOT_ENOUGH_MEMORY*/
			}


		/*
		 * Get the glyph's outlines.
		 */
		if (GetGlyphOutline(	hDC,
					glyphIndex,
					GGO_NATIVE,
					&glyphMetrics,
					glyphBufSize,
					glyphBuf,
					&matrix
					) < 0)
			{
			__wglFree(glyphBuf);
			return FALSE; /*WGL_STATUS_FAILURE*/
			}
		
		glyphMetricsFloat->gmfBlackBoxX =
			(FLOAT) glyphMetrics.gmBlackBoxX * ScaleFactor;
		glyphMetricsFloat->gmfBlackBoxY =
			(FLOAT) glyphMetrics.gmBlackBoxY * ScaleFactor;
		glyphMetricsFloat->gmfptGlyphOrigin.x =
			(FLOAT) glyphMetrics.gmptGlyphOrigin.x * ScaleFactor;
		glyphMetricsFloat->gmfptGlyphOrigin.y =
			(FLOAT) glyphMetrics.gmptGlyphOrigin.y * ScaleFactor;
		glyphMetricsFloat->gmfCellIncX =
			(FLOAT) glyphMetrics.gmCellIncX * ScaleFactor;
		glyphMetricsFloat->gmfCellIncY =
			(FLOAT) glyphMetrics.gmCellIncY * ScaleFactor;
		
		/*
		 * Turn the glyph into a display list:
		 */
		if (!MakeDisplayListFromGlyph(	(glyphIndex - first) + listBase,
						glyphBuf,
						glyphSize,
						glyphMetricsFloat,
						chordalDeviation + ScaleFactor,
						extrusion,
						format))
			{
			__wglFree(glyphBuf);
			return FALSE; /*WGL_STATUS_FAILURE*/
			}
		}


	/*
	 * Clean up temporary storage and return.  If an error occurred,
	 * clear all OpenGL error flags and return FAILURE status;
	 * otherwise just return SUCCESS.
	 */
	__wglFree(glyphBuf);

	SelectObject(hDC, hOldFont);

	if (_GLD_glGetError() == GL_NO_ERROR)
		return TRUE; /*WGL_STATUS_SUCCESS*/
	else
		{
		while (_GLD_glGetError() != GL_NO_ERROR)
			;
		return FALSE; /*WGL_STATUS_FAILURE*/
		}
	}



/*****************************************************************************
 * CreateHighResolutionFont
 *
 * Gets metrics for the current font and creates an equivalent font
 * scaled to the design units of the font.
 * 
 *****************************************************************************/

static HFONT
CreateHighResolutionFont(HDC hDC)
	{
	UINT otmSize;
	OUTLINETEXTMETRIC *otm;
	LONG fontHeight, fontWidth, fontUnits;
	LOGFONT logFont;

	otmSize = GetOutlineTextMetrics(hDC, 0, NULL);
	if (otmSize == 0) 
		return NULL;

	otm = (OUTLINETEXTMETRIC *) __wglMalloc(otmSize);
	if (otm == NULL)
		return NULL;

	otm->otmSize = otmSize;
	if (GetOutlineTextMetrics(hDC, otmSize, otm) == 0) 
		return NULL;
	
	fontHeight = otm->otmTextMetrics.tmHeight -
			otm->otmTextMetrics.tmInternalLeading;
	fontWidth = otm->otmTextMetrics.tmAveCharWidth;
	fontUnits = (LONG) otm->otmEMSquare;
	
	ScaleFactor = 1.0F / (FLOAT) fontUnits;

	logFont.lfHeight = - ((LONG) fontUnits);
	logFont.lfWidth = (LONG)
		((FLOAT) (fontWidth * fontUnits) / (FLOAT) fontHeight);
	logFont.lfEscapement = 0;
	logFont.lfOrientation = 0;
	logFont.lfWeight = otm->otmTextMetrics.tmWeight;
	logFont.lfItalic = otm->otmTextMetrics.tmItalic;
	logFont.lfUnderline = otm->otmTextMetrics.tmUnderlined;
	logFont.lfStrikeOut = otm->otmTextMetrics.tmStruckOut;
	logFont.lfCharSet = otm->otmTextMetrics.tmCharSet;
	logFont.lfOutPrecision = OUT_OUTLINE_PRECIS;
	logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	logFont.lfQuality = DEFAULT_QUALITY;
	logFont.lfPitchAndFamily =
		otm->otmTextMetrics.tmPitchAndFamily & 0xf0;
	strcpy(logFont.lfFaceName,
	       (char *)otm + (int)otm->otmpFaceName);

	hNewFont = CreateFontIndirect(&logFont);
	if (hNewFont == NULL)
		return NULL;

	__wglFree(otm);

	return hNewFont;
	}



/*****************************************************************************
 * MakeDisplayListFromGlyph
 * 
 * Converts the outline of a glyph to an OpenGL display list.
 *
 * Return value is nonzero for success, zero for failure.
 *
 * Does not check for OpenGL errors, so if the caller needs to know about them,
 * it should call glGetError().
 *****************************************************************************/

static int
MakeDisplayListFromGlyph(	IN  DWORD		listName,
				IN  UCHAR*		glyphBuf,
				IN  DWORD		glyphSize,
				IN  LPGLYPHMETRICSFLOAT	glyphMetricsFloat,
				IN  FLOAT		chordalDeviation,
				IN  FLOAT		extrusion,
				IN  INT			format)
	{
	int status;

	_GLD_glNewList(listName, GL_COMPILE);
		status = DrawGlyph(	glyphBuf,
					glyphSize,
					chordalDeviation,
					extrusion,
					format);
		
	_GLD_glTranslatef(glyphMetricsFloat->gmfCellIncX,
		     glyphMetricsFloat->gmfCellIncY,
		     0.0F);
	_GLD_glEndList();

	return status;
	}



/*****************************************************************************
 * DrawGlyph
 * 
 * Converts the outline of a glyph to OpenGL drawing primitives, tessellating
 * as needed, and then draws the glyph.  Tessellation of the quadratic splines
 * in the outline is controlled by "chordalDeviation", and the drawing
 * primitives (lines or polygons) are selected by "format".
 *
 * Return value is nonzero for success, zero for failure.
 *
 * Does not check for OpenGL errors, so if the caller needs to know about them,
 * it should call glGetError().
 *****************************************************************************/

static int
DrawGlyph(	IN  UCHAR*	glyphBuf,
		IN  DWORD	glyphSize,
		IN  FLOAT	chordalDeviation,
		IN  FLOAT	extrusion,
		IN  INT		format)
	{
	INT			status = 0;
	FLOAT*			p;
	DWORD			loop;
	DWORD			point;
	GLUtesselator*		tess = NULL;


	/*
	 * Initialize the global buffer into which we place the outlines:
	 */
	if (!InitLineBuf())
		goto exit;


	/*
	 * Convert the glyph outlines to a set of polyline loops.
	 * (See MakeLinesFromGlyph() for the format of the loop data
	 * structure.)
	 */
	if (!MakeLinesFromGlyph(glyphBuf, glyphSize, chordalDeviation))
		goto exit;
	p = LineBuf;


	/*
	 * Now draw the loops in the appropriate format:
	 */
	if (format == WGL_FONT_LINES)
		{
		/*
		 * This is the easy case.  Just draw the outlines.
		 */
		for (loop = (DWORD) *p++; loop; --loop)
			{
			_GLD_glBegin(GL_LINE_LOOP);
				for (point = (DWORD) *p++; point; --point)
					{
					_GLD_glVertex2fv(p);
					p += 2;
					}
			_GLD_glEnd();
			}
		status = 1;
		}

	else if (format == WGL_FONT_POLYGONS)
		{
		double v[3];
		FLOAT *save_p = p;
		GLfloat z_value;
		
		/*
		 * This is the hard case.  We have to set up a tessellator
		 * to convert the outlines into a set of polygonal
		 * primitives, which the tessellator passes to some
		 * auxiliary routines for drawing.
		 */
		if (!LoadGLUTesselator())
			goto exit;
		if (!InitVertBuf())
			goto exit;
		if (!(tess = gluNewTessProc()))
			goto exit;
		gluTessCallbackProc(tess,	GLU_BEGIN,	(void(CALLBACK *)()) _GLD_glBegin);
		gluTessCallbackProc(tess,	GLU_TESS_VERTEX_DATA,
				    (void(CALLBACK *)()) TessVertexOutData);
		gluTessCallbackProc(tess,	GLU_END,	(void(CALLBACK *)()) _GLD_glEnd);
		gluTessCallbackProc(tess,	GLU_ERROR,	(void(CALLBACK *)()) TessError);
		gluTessCallbackProc(tess,	GLU_TESS_COMBINE, (void(CALLBACK *)()) TessCombine);
		gluTessNormalProc(tess,	0.0F, 0.0F, 1.0F);

		TessErrorOccurred = 0;
		_GLD_glNormal3f(0.0f, 0.0f, 1.0f);
		v[2] = 0.0;
		z_value = 0.0f;

		gluTessBeginPolygonProc(tess, (void *)*(int *)&z_value);
			for (loop = (DWORD) *p++; loop; --loop)
				{
				gluTessBeginContourProc(tess);
				
				for (point = (DWORD) *p++; point; --point)
					{
					v[0] = p[0];
					v[1] = p[1];
					gluTessVertexProc(tess, v, p);
					p += 2;
					}

				gluTessEndContourProc(tess);
				}
		gluTessEndPolygonProc(tess);

		status = !TessErrorOccurred;

		/* Extrusion code */
		if (extrusion) {
			DWORD loops;
			GLfloat thickness = (GLfloat) -extrusion;
			FLOAT *vert, *vert2;
			DWORD count;

			p = save_p;
			loops = (DWORD) *p++;

			for (loop = 0; loop < loops; loop++) {
				GLfloat dx, dy, len;
				DWORD last;

				count = (DWORD) *p++;
				_GLD_glBegin(GL_QUAD_STRIP);

				/* Check if the first and last vertex are identical
				 * so we don't draw the same quad twice.
				 */
				vert = p + (count-1)*2;
				last = (p[0] == vert[0] && p[1] == vert[1]) ? count-1 : count;

				for (point = 0; point <= last; point++) {
					vert  = p + 2 * (point % last);
					vert2 = p + 2 * ((point+1) % last);

					dx = vert[0] - vert2[0];
					dy = vert[1] - vert2[1];
					len = (GLfloat)sqrt(dx * dx + dy * dy);

					_GLD_glNormal3f(dy / len, -dx / len, 0.0f);
					_GLD_glVertex3f((GLfloat) vert[0],
							   (GLfloat) vert[1], thickness);
					_GLD_glVertex3f((GLfloat) vert[0],
							   (GLfloat) vert[1], 0.0f);
				}

				_GLD_glEnd();
				p += count*2;
			}

			/* Draw the back face */
			p = save_p;
			v[2] = thickness;
			_GLD_glNormal3f(0.0f, 0.0f, -1.0f);
			gluTessNormalProc(tess,	0.0F, 0.0F, -1.0F);

			gluTessBeginPolygonProc(tess, (void *)*(int *)&thickness);

			for (loop = (DWORD) *p++; loop; --loop)
			{
				count = (DWORD) *p++;

				gluTessBeginContourProc(tess);
				
				for (point = 0; point < count; point++)
				{
					vert = p + ((count-point-1)<<1);
					v[0] = vert[0];
					v[1] = vert[1];
					gluTessVertexProc(tess, v, vert);
				}
				p += count*2;

				gluTessEndContourProc(tess);
			}
			gluTessEndPolygonProc(tess);
		}

#if DEBUG
	if (TessErrorOccurred)
		printf("Tessellation error %s\n",
			gluErrorString(TessErrorOccurred));
#endif
		}


exit:
	FreeLineBuf();
	if (tess)
		gluDeleteTessProc(tess);
	// UnloadGLUTesselator();
	FreeVertBuf();
	return status;
	}



/*****************************************************************************
 * LoadGLUTesselator
 *
 * Maps the glu32.dll module and gets function pointers for the 
 * tesselator functions.
 *****************************************************************************/

static BOOL
LoadGLUTesselator(void)
	{
	if (gluModuleHandle != NULL)
		return TRUE;

	{
		extern HINSTANCE hInstanceOpenGL;
		char *gluName = "GLU32.DLL";
//		char name[256];
//		char *ptr;
//		int len;

/*
		len = GetModuleFileName(hInstanceOpenGL, name, 255);
		if (len != 0)
			{
			ptr = name+len-1;
			while (ptr > name && *ptr != '\\')
				ptr--;
			if (*ptr == '\\')
				ptr++;
			if (!stricmp(ptr, "cosmogl.dll"))
				{
				gluName = "COSMOGLU.DLL";
				}
			else if (!stricmp(ptr, "opengl32.dll"))
				{
				gluName = "GLU32.DLL";
				}
			}
*/
		if ((gluModuleHandle = LoadLibrary(gluName)) == NULL)
			return FALSE;
	}

	if ((gluNewTessProc = (gluNewTessProto)
		GetProcAddress(gluModuleHandle, "gluNewTess")) == NULL)
		return FALSE;
	
	if ((gluDeleteTessProc = (gluDeleteTessProto)
		GetProcAddress(gluModuleHandle, "gluDeleteTess")) == NULL)
		return FALSE;
	
	if ((gluTessBeginPolygonProc = (gluTessBeginPolygonProto)
		GetProcAddress(gluModuleHandle, "gluTessBeginPolygon")) == NULL)
		return FALSE;
	
	if ((gluTessBeginContourProc = (gluTessBeginContourProto)
		GetProcAddress(gluModuleHandle, "gluTessBeginContour")) == NULL)
		return FALSE;
	
	if ((gluTessVertexProc = (gluTessVertexProto)
		GetProcAddress(gluModuleHandle, "gluTessVertex")) == NULL)
		return FALSE;
	
	if ((gluTessEndContourProc = (gluTessEndContourProto)
		GetProcAddress(gluModuleHandle, "gluTessEndContour")) == NULL)
		return FALSE;
	
	if ((gluTessEndPolygonProc = (gluTessEndPolygonProto)
		GetProcAddress(gluModuleHandle, "gluTessEndPolygon")) == NULL)
		return FALSE;
	
	if ((gluTessPropertyProc = (gluTessPropertyProto)
		GetProcAddress(gluModuleHandle, "gluTessProperty")) == NULL)
		return FALSE;

	if ((gluTessNormalProc = (gluTessNormalProto)
		GetProcAddress(gluModuleHandle, "gluTessNormal")) == NULL)
		return FALSE;
	
	if ((gluTessCallbackProc = (gluTessCallbackProto)
		GetProcAddress(gluModuleHandle, "gluTessCallback")) == NULL)
		return FALSE;

	return TRUE;
	}



/*****************************************************************************
 * UnloadGLUTesselator
 *
 * Unmaps the glu32.dll module.
 *****************************************************************************/

static BOOL
UnloadGLUTesselator(void)
	{
	if (gluModuleHandle != NULL)
	    if (FreeLibrary(gluModuleHandle) == FALSE)
		return FALSE;
	gluModuleHandle = NULL;
	}



/*****************************************************************************
 * TessVertexOut
 *
 * Used by tessellator to handle output vertexes.
 *****************************************************************************/
 
static void CALLBACK
TessVertexOut(FLOAT	p[3])
	{
	    GLfloat v[2];

	    v[0] = p[0] * ScaleFactor;
	    v[1] = p[1] * ScaleFactor;
	    _GLD_glVertex2fv(v);
	}

static void CALLBACK
TessVertexOutData(FLOAT	p[3], GLfloat z)
{
    GLfloat v[3];

    v[0] = (GLfloat) p[0];
    v[1] = (GLfloat) p[1];
    v[2] = z;
    _GLD_glVertex3fv(v);
}


/*****************************************************************************
 * TessCombine
 *
 * Used by tessellator to handle self-intersecting contours and degenerate
 * geometry.
 *****************************************************************************/
 
static void CALLBACK
TessCombine(double	coords[3],
	    void*	vertex_data[4],
	    FLOAT	weight[4],
	    void**	outData)
	{
	if (!AppendToVertBuf((FLOAT) coords[0])
	 || !AppendToVertBuf((FLOAT) coords[1])
	 || !AppendToVertBuf((FLOAT) coords[2]))
		TessErrorOccurred = GL_OUT_OF_MEMORY;
	*outData = VertBuf + (VertBufIndex - 3);
	}



/*****************************************************************************
 * TessError
 *
 * Saves the last tessellator error code in the global TessErrorOccurred.
 *****************************************************************************/
 
static void CALLBACK
TessError(GLenum error)
	{
	TessErrorOccurred = error;
	}



/*****************************************************************************
 * MakeLinesFromGlyph
 * 
 * Converts the outline of a glyph from the TTPOLYGON format to a simple
 * array of floating-point values containing one or more loops.
 *
 * The first element of the output array is a count of the number of loops.
 * The loop data follows this count.  Each loop consists of a count of the
 * number of vertices it contains, followed by the vertices.  Each vertex
 * is an X and Y coordinate.  For example, a single triangle might be
 * described by this array:
 *
 *	1.,	3.,	0., 0.,		1., 0.,		0., 1.
 *       ^	 ^	 ^    ^		 ^    ^		 ^    ^
 *     #loops	#verts	 x1   y1	 x2   y2	 x3   y3
 *
 * A two-loop glyph would look like this:
 *
 *	2.,	3.,  0.,0.,  1.,0.,  0.,1.,	3.,  .2,.2,  .4,.2,  .2,.4
 *
 * Line segments from the TTPOLYGON are transferred to the output array in
 * the obvious way.  Quadratic splines in the TTPOLYGON are converted to
 * collections of line segments
 *****************************************************************************/

static int
MakeLinesFromGlyph(IN  UCHAR*	glyphBuf,
		   IN  DWORD	glyphSize,
		   IN  FLOAT	chordalDeviation)
	{
	UCHAR*	p;
	int	status = 0;


	/*
	 * Pick up all the polygons (aka loops) that make up the glyph:
	 */
	if (!AppendToLineBuf(0.0F))	/* loop count at LineBuf[0] */
		goto exit;

	p = glyphBuf;
	while (p < glyphBuf + glyphSize)
		{
		if (!MakeLinesFromTTPolygon(&p, chordalDeviation))
			goto exit;
		LineBuf[0] += 1.0F;	/* increment loop count */
		}

	status = 1;

exit:
	return status;
	}



/*****************************************************************************
 * MakeLinesFromTTPolygon
 *
 * Converts a TTPOLYGONHEADER and its associated curve structures into a
 * single polyline loop in the global LineBuf.
 *****************************************************************************/

static int
MakeLinesFromTTPolygon(	IN OUT	UCHAR**	pp,
			IN	FLOAT	chordalDeviation)
	{
	DWORD	polySize;
	UCHAR*	polyStart;
	DWORD	vertexCountIndex;

	/*
	 * Record where the polygon data begins, and where the loop's
	 * vertex count resides:
	 */
	polyStart = *pp;
	vertexCountIndex = LineBufIndex;
	if (!AppendToLineBuf(0.0F))
		return 0;

	/*
	 * Extract relevant data from the TTPOLYGONHEADER:
	 */
	polySize = GetDWord(pp);
	if (GetDWord(pp) != TT_POLYGON_TYPE)	/* polygon type */
		return 0;
	if (!AppendToLineBuf((FLOAT) GetFixed(pp)))	/* first X coord */
		return 0;
	if (!AppendToLineBuf((FLOAT) GetFixed(pp)))	/* first Y coord */
		return 0;
	LineBuf[vertexCountIndex] += 1.0F;

	/*
	 * Process each of the TTPOLYCURVE structures in the polygon:
	 */
	while (*pp < polyStart + polySize)
		if (!MakeLinesFromTTPolycurve(	pp,
						vertexCountIndex,
						chordalDeviation))
		return 0;

	return 1;
	}



/*****************************************************************************
 * MakeLinesFromTTPolyCurve
 *
 * Converts the lines and splines in a single TTPOLYCURVE structure to points
 * in the global LineBuf.
 *****************************************************************************/

static int
MakeLinesFromTTPolycurve(	IN OUT	UCHAR**	pp,
				IN	DWORD	vertexCountIndex,
				IN	FLOAT	chordalDeviation)
	{
	WORD type;
	WORD pointCount;


	/*
	 * Pick up the relevant fields of the TTPOLYCURVE structure:
	 */
	type = (WORD) GetWord(pp);
	pointCount = (WORD) GetWord(pp);

	/*
	 * Convert the "curve" to line segments:
	 */
	if (type == TT_PRIM_LINE)
		return MakeLinesFromTTLine(	pp,
						vertexCountIndex,
						pointCount);
	else if (type == TT_PRIM_QSPLINE)
		return MakeLinesFromTTQSpline(	pp,
						vertexCountIndex,
						pointCount,
						chordalDeviation);
	else
		return 0;
	}



/*****************************************************************************
 * MakeLinesFromTTLine
 *
 * Converts points from the polyline in a TT_PRIM_LINE structure to
 * equivalent points in the global LineBuf.
 *****************************************************************************/
static int
MakeLinesFromTTLine(	IN OUT	UCHAR**	pp,
			IN	DWORD	vertexCountIndex,
			IN	WORD	pointCount)
	{
	/*
	 * Just copy the line segments into the line buffer (converting
	 * type as we go):
	 */
	LineBuf[vertexCountIndex] += pointCount;
	while (pointCount--)
		{
		if (!AppendToLineBuf((FLOAT) GetFixed(pp))	/* X coord */
		 || !AppendToLineBuf((FLOAT) GetFixed(pp)))	/* Y coord */
			return 0;
		}

	return 1;
	}



/*****************************************************************************
 * MakeLinesFromTTQSpline
 *
 * Converts points from the poly quadratic spline in a TT_PRIM_QSPLINE
 * structure to polyline points in the global LineBuf.
 *****************************************************************************/

static int
MakeLinesFromTTQSpline(	IN OUT	UCHAR**	pp,
			IN	DWORD	vertexCountIndex,
			IN	WORD	pointCount,
			IN	FLOAT	chordalDeviation)
	{
	FLOAT x0, y0, x1, y1, x2, y2;
	WORD point;

	/*
	 * Process each of the non-interpolated points in the outline.
	 * To do this, we need to generate two interpolated points (the
	 * start and end of the arc) for each non-interpolated point.
	 * The first interpolated point is always the one most recently
	 * stored in LineBuf, so we just extract it from there.  The
	 * second interpolated point is either the average of the next
	 * two points in the QSpline, or the last point in the QSpline
	 * if only one remains.
	 */
	for (point = 0; point < pointCount - 1; ++point)
		{
		x0 = LineBuf[LineBufIndex - 2];
		y0 = LineBuf[LineBufIndex - 1];

		x1 = (FLOAT) GetFixed(pp);
		y1 = (FLOAT) GetFixed(pp);

		if (point == pointCount - 2)
			{
			/*
			 * This is the last arc in the QSpline.  The final
			 * point is the end of the arc.
			 */
			x2 = (FLOAT) GetFixed(pp);
			y2 = (FLOAT) GetFixed(pp);
			}
		else
			{
			/*
			 * Peek at the next point in the input to compute
			 * the end of the arc:
			 */
			x2 = 0.5F * (x1 + (FLOAT) GetFixed(pp));
			y2 = 0.5F * (y1 + (FLOAT) GetFixed(pp));
			/*
			 * Push the point back onto the input so it will
			 * be reused as the next off-curve point:
			 */
			*pp -= 8;
			}

		if (!MakeLinesFromArc(	x0, y0,
					x1, y1,
					x2, y2,
					vertexCountIndex,
					chordalDeviation * chordalDeviation))
			return 0;
		}

	return 1;
	}



/*****************************************************************************
 * MakeLinesFromArc
 *
 * Subdivides one arc of a quadratic spline until the chordal deviation
 * tolerance requirement is met, then places the resulting set of line
 * segments in the global LineBuf.
 *****************************************************************************/

static int
MakeLinesFromArc(	IN	FLOAT	x0,
			IN	FLOAT	y0,
			IN	FLOAT	x1,
			IN	FLOAT	y1,
			IN	FLOAT	x2,
			IN	FLOAT	y2,
			IN	DWORD	vertexCountIndex,
			IN	FLOAT	chordalDeviationSquared)
	{
	FLOAT	x01;
	FLOAT	y01;
	FLOAT	x12;
	FLOAT	y12;
	FLOAT	midPointX;
	FLOAT	midPointY;
	FLOAT	deltaX;
	FLOAT	deltaY;

	/*
	 * Calculate midpoint of the curve by de Casteljau:
	 */
	x01 = 0.5F * (x0 + x1);
	y01 = 0.5F * (y0 + y1);
	x12 = 0.5F * (x1 + x2);
	y12 = 0.5F * (y1 + y2);
	midPointX = 0.5F * (x01 + x12);
	midPointY = 0.5F * (y01 + y12);


	/*
	 * Estimate chordal deviation by the distance from the midpoint
	 * of the curve to its non-interpolated control point.  If this
	 * distance is greater than the specified chordal deviation
	 * constraint, then subdivide.  Otherwise, generate polylines
	 * from the three control points.
	 */
	deltaX = midPointX - x1;
	deltaY = midPointY - y1;
	if (deltaX * deltaX + deltaY * deltaY > chordalDeviationSquared)
		{
		MakeLinesFromArc(	x0, y0,
					x01, y01,
					midPointX, midPointY,
					vertexCountIndex,
					chordalDeviationSquared);
		
		MakeLinesFromArc(	midPointX, midPointY,
					x12, y12,
					x2, y2,
					vertexCountIndex,
					chordalDeviationSquared);
		}
	else
		{
		/*
		 * The "pen" is already at (x0, y0), so we don't need to
		 * add that point to the LineBuf.
		 */
		if (!AppendToLineBuf(x1)
		 || !AppendToLineBuf(y1)
		 || !AppendToLineBuf(x2)
		 || !AppendToLineBuf(y2))
			return 0;
		LineBuf[vertexCountIndex] += 2.0F;
		}

	return 1;
	}



/*****************************************************************************
 * InitLineBuf
 *
 * Initializes the global LineBuf and its associated size and current-element
 * counters.
 *****************************************************************************/

static int
InitLineBuf(void)
	{
	if (!(LineBuf = (FLOAT*)
		__wglMalloc((LineBufSize = LINE_BUF_QUANT) * sizeof(FLOAT))))
			return 0;
	LineBufIndex = 0;
	return 1;
	}



/*****************************************************************************
 * InitVertBuf
 *
 * Initializes the global VertBuf and its associated size and current-element
 * counters.
 *****************************************************************************/

static int
InitVertBuf(void)
	{
	if (!(VertBuf = (FLOAT*)
		__wglMalloc((VertBufSize = VERT_BUF_QUANT) * sizeof(FLOAT))))
			return 0;
	VertBufIndex = 0;
	return 1;
	}



/*****************************************************************************
 * AppendToLineBuf
 *
 * Appends one floating-point value to the global LineBuf array.  Return value
 * is non-zero for success, zero for failure.
 *****************************************************************************/

static int
AppendToLineBuf(FLOAT value)
	{
	if (LineBufIndex >= LineBufSize)
		{
		FLOAT* f;
		
		f = (FLOAT*) __wglRealloc(LineBuf,
			(LineBufSize += LINE_BUF_QUANT) * sizeof(FLOAT));
		if (!f)
			return 0;
		LineBuf = f;
		}
	LineBuf[LineBufIndex++] = value;
	return 1;
	}



/*****************************************************************************
 * AppendToVertBuf
 *
 * Appends one floating-point value to the global VertBuf array.  Return value
 * is non-zero for success, zero for failure.
 *
 * Note that we can't realloc this one, because the tessellator is using
 * pointers into it.
 *****************************************************************************/

static int
AppendToVertBuf(FLOAT value)
	{
	if (VertBufIndex >= VertBufSize)
		return 0;
	VertBuf[VertBufIndex++] = value;
	return 1;
	}



/*****************************************************************************
 * FreeLineBuf
 *
 * Cleans up vertex buffer structure.
 *****************************************************************************/

static void
FreeLineBuf(void)
	{
	if (LineBuf)
		{
		__wglFree(LineBuf);
		LineBuf = NULL;
		}
	}



/*****************************************************************************
 * FreeVertBuf
 *
 * Cleans up vertex buffer structure.
 *****************************************************************************/

static void
FreeVertBuf(void)
	{
	if (VertBuf)
		{
		__wglFree(VertBuf);
		VertBuf = NULL;
		}
	}



/*****************************************************************************
 * GetWord
 *
 * Fetch the next 16-bit word from a little-endian byte stream, and increment
 * the stream pointer to the next unscanned byte.
 *****************************************************************************/

static long GetWord(UCHAR** p)
	{
	long value;

	value = ((*p)[1] << 8) + (*p)[0];
	*p += 2;
	return value;
	}



/*****************************************************************************
 * GetDWord
 *
 * Fetch the next 32-bit word from a little-endian byte stream, and increment
 * the stream pointer to the next unscanned byte.
 *****************************************************************************/

static long GetDWord(UCHAR** p)
	{
	long value;

	value = ((*p)[3] << 24) + ((*p)[2] << 16) + ((*p)[1] << 8) + (*p)[0];
	*p += 4;
	return value;
	}




/*****************************************************************************
 * GetFixed
 *
 * Fetch the next 32-bit fixed-point value from a little-endian byte stream,
 * convert it to floating-point, and increment the stream pointer to the next
 * unscanned byte.
 *****************************************************************************/

static double GetFixed(
	UCHAR** p)
{
	long hiBits, loBits;
	double value;

	loBits = GetWord(p);
	hiBits = GetWord(p);
	value = (double) ((hiBits << 16) | loBits) / 65536.0;

	return value * ScaleFactor;
}

// ***********************************************************************
