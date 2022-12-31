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
* Description:  Pixel Formats.
*
*********************************************************************************/

#include "gld_pf.h"

#include "gld_driver.h"

BOOL gldValidate();

// ***********************************************************************

char	szColorDepthWarning[] =
"GLDirect does not support the current desktop\n\
color depth.\n\n\
You may need to change the display resolution to\n\
16 bits per pixel or higher color depth using\n\
the Windows Display Settings control panel\n\
before running this OpenGL application.\n";

// ***********************************************************************
// This pixel format will be used as a template when compiling the list
// of pixel formats supported by the hardware. Many fields will be
// filled in at runtime.
// PFD flag defaults are upgraded to match ChoosePixelFormat() -- DaveM
GLD_pixelFormat pfTemplateHW =
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
		5, 5,						// Green bits, shift
		5, 10,						// Blue bits, shift
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
	-1,	// No depth/stencil buffer
};

// ***********************************************************************
// Return the count of the number of bits in a Bit Mask.
int BitCount(
	DWORD dw)
{
	int i;

	if (dw == 0)
		return 0;	// account for non-RGB mode

	for (i=0; dw; dw=dw>>1)
        i += (dw & 1);
    return i;
}

// ***********************************************************************

DWORD BitShift(
	DWORD dwMaskIn)
{
	DWORD dwShift, dwMask;

	if (dwMaskIn == 0)
		return 0;	// account for non-RGB mode

	for (dwShift=0, dwMask=dwMaskIn; !(dwMask&1); dwShift++, dwMask>>=1);

    return dwShift;
}

// ***********************************************************************

BOOL IsValidPFD(int iPFD)
{
	GLD_pixelFormat *lpPF;

	// Validate license
	if (!gldValidate())
		return FALSE;

	if ((glb.lpPF == NULL) ||
		(glb.nPixelFormatCount == 0))
		return FALSE;

	// Check PFD range
	if ( (iPFD < 1) || (iPFD > glb.nPixelFormatCount) ) {
		gldLogMessage(GLDLOG_ERROR, "PFD out of range\n");
		return FALSE; // PFD is invalid
	}

	// Make a pointer to the pixel format
	lpPF = &glb.lpPF[iPFD-1];

	// Check size
	if (lpPF->pfd.nSize != sizeof(PIXELFORMATDESCRIPTOR)) {
		gldLogMessage(GLDLOG_ERROR, "Bad PFD size\n");
		return FALSE; // PFD is invalid
	}

	// Check version
	if (lpPF->pfd.nVersion != 1) {
		gldLogMessage(GLDLOG_ERROR, "PFD is not Version 1\n");
		return FALSE; // PFD is invalid
	}

	return TRUE; // PFD is valid
}

// ***********************************************************************

BOOL IsStencilSupportBroken(LPDIRECTDRAW4 lpDD4)
{
	DDDEVICEIDENTIFIER	dddi; // DX6 device identifier
	BOOL				bBroken = FALSE;

	// Microsoft really messed up with the GetDeviceIdentifier function
	// on Windows 2000, since it locks up on stock drivers on the CD. Updated
	// drivers from vendors appear to work, but we can't identify the drivers
	// without this function!!! For now we skip these tests on Windows 2000.
	if ((GetVersion() & 0x80000000UL) == 0)
		return FALSE;

	// Obtain device info
	if (FAILED(IDirectDraw4_GetDeviceIdentifier(lpDD4, &dddi, 0)))
		return FALSE;

	// Matrox G400 stencil buffer support does not draw anything in AutoCAD,
	// but ordinary Z buffers draw shaded models fine. (DaveM)
	if (dddi.dwVendorId == 0x102B) {		// Matrox
		if (dddi.dwDeviceId == 0x0525) {	// G400
			bBroken = TRUE;
		}
	}

	return bBroken;
}

// ***********************************************************************

BOOL gldBuildPixelFormatList()
{
	int				i;
	char			buf[128];
	char			cat[8];
	GLD_pixelFormat	*lpPF;

	 if (!_gldDriver.BuildPixelformatList())
		 return FALSE; // BAIL

	// Lets dump the list to the log
	// ** Based on "wglinfo" by Nate Robins **
	gldLogMessage(GLDLOG_INFO, "\n");
	gldLogMessage(GLDLOG_INFO, "Pixel Formats:\n");
	gldLogMessage(GLDLOG_INFO,
		"   visual  x  bf lv rg d st  r  g  b a  ax dp st accum buffs  ms\n");
	gldLogMessage(GLDLOG_INFO,
		" id dep cl sp sz l  ci b ro sz sz sz sz bf th cl  r  g  b  a ns b\n");
	gldLogMessage(GLDLOG_INFO,
		"-----------------------------------------------------------------\n");
	for (i=0, lpPF = glb.lpPF; i<glb.nPixelFormatCount; i++, lpPF++) {
		sprintf(buf, "0x%02x ", i+1);

		sprintf(cat, "%2d ", lpPF->pfd.cColorBits);
		strcat(buf, cat);
		if(lpPF->pfd.dwFlags & PFD_DRAW_TO_WINDOW)      sprintf(cat, "wn ");
		else if(lpPF->pfd.dwFlags & PFD_DRAW_TO_BITMAP) sprintf(cat, "bm ");
		else sprintf(cat, ".  ");
		strcat(buf, cat);

		/* should find transparent pixel from LAYERPLANEDESCRIPTOR */
		sprintf(cat, " . "); 
		strcat(buf, cat);

		sprintf(cat, "%2d ", lpPF->pfd.cColorBits);
		strcat(buf, cat);

		/* bReserved field indicates number of over/underlays */
		if(lpPF->pfd.bReserved) sprintf(cat, " %d ", lpPF->pfd.bReserved);
		else sprintf(cat, " . "); 
		strcat(buf, cat);

		sprintf(cat, " %c ", lpPF->pfd.iPixelType == PFD_TYPE_RGBA ? 'r' : 'c');
		strcat(buf, cat);

		sprintf(cat, "%c ", lpPF->pfd.dwFlags & PFD_DOUBLEBUFFER ? 'y' : '.');
		strcat(buf, cat);

		sprintf(cat, " %c ", lpPF->pfd.dwFlags & PFD_STEREO ? 'y' : '.');
		strcat(buf, cat);

		if(lpPF->pfd.cRedBits && lpPF->pfd.iPixelType == PFD_TYPE_RGBA) 
		    sprintf(cat, "%2d ", lpPF->pfd.cRedBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);

		if(lpPF->pfd.cGreenBits && lpPF->pfd.iPixelType == PFD_TYPE_RGBA) 
		    sprintf(cat, "%2d ", lpPF->pfd.cGreenBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);

		if(lpPF->pfd.cBlueBits && lpPF->pfd.iPixelType == PFD_TYPE_RGBA) 
		    sprintf(cat, "%2d ", lpPF->pfd.cBlueBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);
	
		if(lpPF->pfd.cAlphaBits && lpPF->pfd.iPixelType == PFD_TYPE_RGBA) 
			sprintf(cat, "%2d ", lpPF->pfd.cAlphaBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);
	
		if(lpPF->pfd.cAuxBuffers)     sprintf(cat, "%2d ", lpPF->pfd.cAuxBuffers);
		else sprintf(cat, " . ");
		strcat(buf, cat);
	
		if(lpPF->pfd.cDepthBits)      sprintf(cat, "%2d ", lpPF->pfd.cDepthBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);
	
		if(lpPF->pfd.cStencilBits)    sprintf(cat, "%2d ", lpPF->pfd.cStencilBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);
	
		if(lpPF->pfd.cAccumRedBits)   sprintf(cat, "%2d ", lpPF->pfd.cAccumRedBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);

		if(lpPF->pfd.cAccumGreenBits) sprintf(cat, "%2d ", lpPF->pfd.cAccumGreenBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);
	
		if(lpPF->pfd.cAccumBlueBits)  sprintf(cat, "%2d ", lpPF->pfd.cAccumBlueBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);
	
		if(lpPF->pfd.cAccumAlphaBits) sprintf(cat, "%2d ", lpPF->pfd.cAccumAlphaBits);
		else sprintf(cat, " . ");
		strcat(buf, cat);
	
		/* no multisample in Win32 */
		sprintf(cat, " . .\n");
		strcat(buf, cat);

		gldLogMessage(GLDLOG_INFO, buf);
	}
	gldLogMessage(GLDLOG_INFO,
		"-----------------------------------------------------------------\n");
	gldLogMessage(GLDLOG_INFO, "\n");

	return TRUE;
}

// ***********************************************************************

void gldReleasePixelFormatList()
{
	glb.nPixelFormatCount = 0;
	if (glb.lpPF) {
		free(glb.lpPF);
		glb.lpPF = NULL;
	}
}

// ***********************************************************************
