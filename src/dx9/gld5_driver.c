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
* Description:  Driver interface code to Mesa
*
*********************************************************************************/

#include "gld_context.h"

#include "gld_log.h"
#include "gldirect5.h"

#include "glheader.h"
#include "context.h"
#include "colormac.h"
#include "depth.h"
#include "extensions.h"
#include "macros.h"
#include "matrix.h"
#include "mtypes.h"
#include "texformat.h"
#include "texstore.h"
#include "teximage.h"
#include "api_arrayelt.h"

extern BOOL gldSwapBuffers(HDC hDC);

//---------------------------------------------------------------------------
// Internal functions
//---------------------------------------------------------------------------

static void _gldD3DMatrixProjection(
	D3DXMATRIX *pMatIn,
	GLD_context *gldCtx)
{
	//
	// Convert a GL-format perspective projection matrix into a
	// Direct3D-compatible perspective projection matrix.
	//
	// OpenGL perspective projection matrices project Z into
	// a canonical clip volume of -1...1.
	// Direct3D expects a canonical clip volume of 0...1
	//

	// The matrix that will be used to convert input matrix
	// into Direct3D's canonical clip volume.
    D3DXMATRIX matD3DClipVolume;

	// Start off with an identity matrix
	D3DXMatrixIdentity(&matD3DClipVolume);

	// Set matrix to scale Z by half and offset Z by half
	matD3DClipVolume._33 = matD3DClipVolume._43 = 0.5f;

	// Ensure 2D/HUD/Orthographic vertices project to non-integer coordinates
	matD3DClipVolume._41 = -1.0 / gldCtx->dwWidth;
	matD3DClipVolume._42 = 1.0 / gldCtx->dwHeight;

	// Convert input matrix to D3D's canonical clip volume
	D3DXMatrixMultiply(pMatIn, pMatIn, &matD3DClipVolume);
}

//---------------------------------------------------------------------------

void _gld_mesa_warning(
    __GLcontext *gc,
    char *str)
{
    // Intercept Mesa's internal warning mechanism
    gldLogPrintf(GLDLOG_WARN, "Mesa warning: %s", str);
}

//---------------------------------------------------------------------------

void _gld_mesa_fatal(
    __GLcontext *gc,
    char *str)
{
    // Intercept Mesa's internal fatal-message mechanism
    gldLogPrintf(GLDLOG_CRITICAL, "Mesa FATAL: %s", str);

    // Mesa calls abort(0) here.
    gldLogClose();
    exit(0);
}

//---------------------------------------------------------------------------

D3DSTENCILOP _gldConvertStencilOp(
    GLenum StencilOp)
{
    // Used by Stencil: pass, fail and zfail

    switch (StencilOp) {
    case GL_KEEP:
        return D3DSTENCILOP_KEEP;
    case GL_ZERO:
        return D3DSTENCILOP_ZERO;
    case GL_REPLACE:
        return D3DSTENCILOP_REPLACE;
    case GL_INCR:
        return D3DSTENCILOP_INCRSAT;
    case GL_DECR:
        return D3DSTENCILOP_DECRSAT;
    case GL_INVERT:
        return D3DSTENCILOP_INVERT;
    case GL_INCR_WRAP_EXT:  // GL_EXT_stencil_wrap
        return D3DSTENCILOP_INCR;
    case GL_DECR_WRAP_EXT:  // GL_EXT_stencil_wrap
        return D3DSTENCILOP_DECR;
    }

#ifdef _DEBUG
    gldLogMessage(GLDLOG_ERROR, "_gldConvertStencilOp: Unknown StencilOp\n");
#endif

    return D3DSTENCILOP_KEEP;
}

//---------------------------------------------------------------------------

D3DCMPFUNC _gldConvertCompareFunc(
    GLenum CmpFunc)
{
    // Used for Alpha func, depth func and stencil func.

    switch (CmpFunc) {
    case GL_NEVER:
        return D3DCMP_NEVER;
    case GL_LESS:
        return D3DCMP_LESS;
    case GL_EQUAL:
        return D3DCMP_EQUAL;
    case GL_LEQUAL:
        return D3DCMP_LESSEQUAL;
    case GL_GREATER:
        return D3DCMP_GREATER;
    case GL_NOTEQUAL:
        return D3DCMP_NOTEQUAL;
    case GL_GEQUAL:
        return D3DCMP_GREATEREQUAL;
    case GL_ALWAYS:
        return D3DCMP_ALWAYS;
    };

#ifdef _DEBUG
    gldLogMessage(GLDLOG_ERROR, "_gldConvertCompareFunc: Unknown CompareFunc\n");
#endif

    return D3DCMP_ALWAYS;
}

//---------------------------------------------------------------------------

D3DBLEND _gldConvertBlendFunc(
    GLenum blend,
    D3DBLEND DefaultBlend)
{
    switch (blend) {
    case GL_ZERO:
        return D3DBLEND_ZERO;
    case GL_ONE:
        return D3DBLEND_ONE;
    case GL_DST_COLOR:
        return D3DBLEND_DESTCOLOR;
    case GL_SRC_COLOR:
        return D3DBLEND_SRCCOLOR;
    case GL_ONE_MINUS_DST_COLOR:
        return D3DBLEND_INVDESTCOLOR;
    case GL_ONE_MINUS_SRC_COLOR:
        return D3DBLEND_INVSRCCOLOR;
    case GL_SRC_ALPHA:
        return D3DBLEND_SRCALPHA;
    case GL_ONE_MINUS_SRC_ALPHA:
        return D3DBLEND_INVSRCALPHA;
    case GL_DST_ALPHA:
        return D3DBLEND_DESTALPHA;
    case GL_ONE_MINUS_DST_ALPHA:
        return D3DBLEND_INVDESTALPHA;
    case GL_SRC_ALPHA_SATURATE:
        return D3DBLEND_SRCALPHASAT;
    }

#ifdef _DEBUG
    gldLogMessage(GLDLOG_ERROR, "_gldConvertBlendFunc: Unknown BlendFunc\n");
#endif

    return DefaultBlend;
}

//---------------------------------------------------------------------------
// Misc. functions
//---------------------------------------------------------------------------

void gld_Noop_DX9(
    GLcontext *ctx)
{
#ifdef _DEBUG
    gldLogMessage(GLDLOG_ERROR, "gld_Noop called!\n");
#endif
}

//---------------------------------------------------------------------------

void gld_Error_DX9(
    GLcontext *ctx)
{
#ifdef _DEBUG
    // Quite useless, as the Mesa error message isn't passed.
//  gldLogMessage(GLDLOG_ERROR, "ctx->Driver.Error called!\n");
#endif
}

//---------------------------------------------------------------------------
// Required Mesa functions
//---------------------------------------------------------------------------

static void gld_set_draw_buffer_DX9(
    GLcontext *ctx,
    GLenum mode)
{
/*
   (void) ctx;
   if ((mode==GL_FRONT_LEFT) || (mode == GL_BACK_LEFT)) {
      return GL_TRUE;
   }
   else {
      return GL_FALSE;
   }
*/
}

//---------------------------------------------------------------------------

static void gld_set_read_buffer_DX9(
    GLcontext *ctx,
    GLframebuffer *buffer,
    GLenum mode)
{
   /* separate read buffer not supported */
/*
   ASSERT(buffer == ctx->DrawBuffer);
   ASSERT(mode == GL_FRONT_LEFT);
*/
}

//---------------------------------------------------------------------------

void gld_Clear_DX9(
    GLcontext *ctx,
    GLbitfield mask,
    GLboolean all,
    GLint x,
    GLint y,
    GLint width,
    GLint height)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    DWORD       dwFlags = 0;
    D3DCOLOR    Color = 0;
    float       Z = 0.0f;
    DWORD       Stencil = 0;
    D3DRECT     d3dClearRect;

    // TODO: Colourmask
    const GLuint *colorMask = (GLuint *) &ctx->Color.ColorMask;

    if (!gld || !gld->pDev)
        return;

    if (mask & (DD_FRONT_LEFT_BIT | DD_BACK_LEFT_BIT)) {
        dwFlags |= D3DCLEAR_TARGET;
		Color = gldClampedColour(ctx->Color.ClearColor);
    }

    if (mask & DD_DEPTH_BIT) {
        // D3D8/9 will fail the Clear call if we try and clear a
        // depth buffer and we haven't created one.
        // Also, some apps try and clear a depth buffer,
        // when a depth buffer hasn't been requested by the app.
        if (ctx->Visual.depthBits == 0) {
            mask &= ~DD_DEPTH_BIT; // Remove depth bit from mask
        } else {
            dwFlags |= D3DCLEAR_ZBUFFER;
            Z = ctx->Depth.Clear;
        }
    }

    if (mask & DD_STENCIL_BIT) {
        if (ctx->Visual.stencilBits == 0) {
            // No stencil bits in depth buffer
            mask &= ~DD_STENCIL_BIT; // Remove stencil bit from mask
        } else {
            dwFlags |= D3DCLEAR_STENCIL;
            Stencil = ctx->Stencil.Clear;
        }
    }

    // Some apps do really weird things with the rect, such as Quake3.
    if ((x < 0) || (y < 0) || (width <= 0) || (height <= 0)) {
        all = GL_TRUE;
    }

    if (!all) {
        // Calculate clear subrect
        d3dClearRect.x1 = x;
        d3dClearRect.y1 = gldCtx->dwHeight - (y + height);
        d3dClearRect.x2 = x + width;
        d3dClearRect.y2 = d3dClearRect.y1 + height;
//      gldLogPrintf(GLDLOG_INFO, "Rect %d,%d %d,%d", x,y,width,height);
    }

    // dwFlags will be zero if there's nothing to clear
    if (dwFlags) {
        _GLD_DX9_DEV(Clear(
            gld->pDev,
            all ? 0 : 1,
            all ? NULL : &d3dClearRect,
            dwFlags,
            Color, Z, Stencil));
    }

    if (mask & DD_ACCUM_BIT) {
        // Clear accumulation buffer
    }
}

//---------------------------------------------------------------------------

// Mesa 5: Parameter change
static void gld_buffer_size_DX9(
    GLframebuffer *fb,
    GLuint *width,
    GLuint *height)
{
    *width = fb->Width;
    *height = fb->Height;
}

//---------------------------------------------------------------------------

static void gld_Finish_DX9(
    GLcontext *ctx)
{
//    if (ctx) ctx->Driver.Flush(ctx);        // DaveM
}

//---------------------------------------------------------------------------

static void gld_Flush_DX9(
    GLcontext *ctx)
{
    GLD_context     *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);
/*
    // GL App Flush should cause D3D Flush. (DaveM)
    if (gldCtx->bSceneStarted) {
        _GLD_DX9_DEV(EndScene(gld->pDev));  // DaveM
        gldCtx->bSceneStarted = FALSE;
    }
*/
    // TODO: Detect apps that glFlush() then SwapBuffers() ?

    if (gldCtx->EmulateSingle) {
        // Emulating a single-buffered context.
        // [Direct3D doesn't allow rendering to front buffer]
        gldSwapBuffers(gldCtx->hDC);
    }
/*
    if (!gldCtx->bSceneStarted) {
        _GLD_DX9_DEV(BeginScene(gld->pDev));    // DaveM
        gldCtx->bSceneStarted = TRUE;
    }
*/
}

//---------------------------------------------------------------------------

void gld_NEW_STENCIL(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    // Two-sided stencil. New for Mesa 5
    const GLuint        uiFace  = 0UL;

    struct gl_stencil_attrib *pStencil = &ctx->Stencil;

    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_STENCILENABLE, pStencil->Enabled ? TRUE : FALSE));
    if (pStencil->Enabled) {
        _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_STENCILFUNC, _gldConvertCompareFunc(pStencil->Function[uiFace])));
        _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_STENCILREF, pStencil->Ref[uiFace]));
        _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_STENCILMASK, pStencil->ValueMask[uiFace]));
        _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_STENCILWRITEMASK, pStencil->WriteMask[uiFace]));
        _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_STENCILFAIL, _gldConvertStencilOp(pStencil->FailFunc[uiFace])));
        _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_STENCILZFAIL, _gldConvertStencilOp(pStencil->ZFailFunc[uiFace])));
        _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_STENCILPASS, _gldConvertStencilOp(pStencil->ZPassFunc[uiFace])));
    }
}

//---------------------------------------------------------------------------

void gld_NEW_COLOR(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    DWORD       dwFlags = 0;
    D3DBLEND    src;
    D3DBLEND    dest;

	// Mesa stores alpha-ref as 0.0 to 1.0.
	// Direct3D9 expects alpha-ref as a DWORD in the range 0x0 to 0xFF
	DWORD		dwAlphaRef = (DWORD)(ctx->Color.AlphaRef * 255.0f);

    // Alpha func
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_ALPHAFUNC, _gldConvertCompareFunc(ctx->Color.AlphaFunc)));
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_ALPHAREF, dwAlphaRef));
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_ALPHATESTENABLE, ctx->Color.AlphaEnabled));

    // Blend func
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_ALPHABLENDENABLE, ctx->Color.BlendEnabled));
    src     = _gldConvertBlendFunc(ctx->Color.BlendSrcRGB, D3DBLEND_ONE);
    dest    = _gldConvertBlendFunc(ctx->Color.BlendDstRGB, D3DBLEND_ZERO);
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_SRCBLEND, src));
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_DESTBLEND, dest));

    // Color mask
    if (ctx->Color.ColorMask[0]) dwFlags |= D3DCOLORWRITEENABLE_RED;
    if (ctx->Color.ColorMask[1]) dwFlags |= D3DCOLORWRITEENABLE_GREEN;
    if (ctx->Color.ColorMask[2]) dwFlags |= D3DCOLORWRITEENABLE_BLUE;
    if (ctx->Color.ColorMask[3]) dwFlags |= D3DCOLORWRITEENABLE_ALPHA;
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_COLORWRITEENABLE, dwFlags));
}

//---------------------------------------------------------------------------

void gld_NEW_DEPTH(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

	// DEBUG INFO
#ifdef _DEBUG
#if 0
	gldLogPrintf(GLDLOG_SYSTEM, "ZEnable=%d, ZFunc=%x, ZWrite=%d", ctx->Depth.Test, _gldConvertCompareFunc(ctx->Depth.Func), ctx->Depth.Mask);
#endif
#endif

	_GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_ZENABLE, ctx->Depth.Test ? D3DZB_TRUE : D3DZB_FALSE));
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_ZWRITEENABLE, ctx->Depth.Mask ? TRUE : FALSE));

	if (glb.bDisableZTrick) {
//		if ((_gldConvertCompareFunc(ctx->Depth.Func) == 7) && ctx->Depth.Mask) {
//			_GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_ZFUNC, D3DCMP_GREATEREQUAL));
//		} else {
			_GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_ZFUNC, D3DCMP_LESSEQUAL));
//		}
	} else {
		_GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_ZFUNC, _gldConvertCompareFunc(ctx->Depth.Func)));
	}
}

//---------------------------------------------------------------------------

void gld_NEW_POLYGON(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    D3DFILLMODE d3dFillMode = D3DFILL_SOLID;
    D3DCULL     d3dCullMode = D3DCULL_NONE;
    float       fOffset		= 0; // Changed from int to float for DX9
    float       fSlopeBias	= 0;

    // Fillmode
    switch (ctx->Polygon.FrontMode) {
    case GL_POINT:
        d3dFillMode = D3DFILL_POINT;
        break;
    case GL_LINE:
        d3dFillMode = D3DFILL_WIREFRAME;
        break;
    case GL_FILL:
        d3dFillMode = D3DFILL_SOLID;
        break;
    }
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_FILLMODE, d3dFillMode));

    if (ctx->Polygon.CullFlag) {
        switch (ctx->Polygon.CullFaceMode) {
        case GL_BACK:
            if (ctx->Polygon.FrontFace == GL_CCW)
                d3dCullMode = D3DCULL_CW;
            else
                d3dCullMode = D3DCULL_CCW;
            break;
        case GL_FRONT:
            if (ctx->Polygon.FrontFace == GL_CCW)
                d3dCullMode = D3DCULL_CCW;
            else
                d3dCullMode = D3DCULL_CW;
            break;
        case GL_FRONT_AND_BACK:
            d3dCullMode = D3DCULL_NONE;
            break;
        default:
            break;
        }
    } else {
        d3dCullMode = D3DCULL_NONE;
    }
#if 0
	d3dCullMode = D3DCULL_NONE; // FOR DEBUGGING
#endif
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_CULLMODE, d3dCullMode));

    // Polygon offset
    if (ctx->Polygon.OffsetFill) {
        fOffset		= ctx->Polygon.OffsetUnits / 32768.0f;
		fSlopeBias	= ctx->Polygon.OffsetFactor;
    }
    // NOTE: SetRenderState() only accepts DWORDs, hence the evil float->DWORD cast below.
	_GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_DEPTHBIAS, *((DWORD*)&fOffset)));
	_GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_SLOPESCALEDEPTHBIAS, *((DWORD*)&fSlopeBias)));
}

//---------------------------------------------------------------------------

void gld_NEW_FOG(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);
	D3DCOLOR			d3dFogColour;
	BOOL				bFog = ctx->Fog.Enabled;

	// Only need to enable/disable fog and set fog colour (if enabled).
	// Actual Fog values for each vertex is calculated in the vertex shader.

    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_FOGENABLE, bFog));

	if (bFog) {
		d3dFogColour = gldClampedColour(ctx->Fog.Color);
		_GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_FOGCOLOR, d3dFogColour));
	}
}

//---------------------------------------------------------------------------

void gld_NEW_LIGHT(
    GLcontext *ctx)
{
    GLD_context     *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9  *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    // Shademode
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_SHADEMODE, (ctx->Light.ShadeModel == GL_SMOOTH) ? D3DSHADE_GOURAUD : D3DSHADE_FLAT));
}

//---------------------------------------------------------------------------

static void _gldComputeWorldViewProject(
	GLD_driver_dx9 *gld)
{
	// Take WorldView and Projection matrices and compute the WorldViewProject matrix.
	// Needs to be called if either WorldView or Projection matrices change.
	//
	// In situations where we don't need eye coordinates we can use
	// matModelViewProject in vertex shaders to improve performance.
	D3DXMatrixMultiply(&gld->matModelViewProject, &gld->matModelView, &gld->matProjection);
}

//---------------------------------------------------------------------------

static void _GLMatrixToD3DXMatrix(
	const GLmatrix *pGL,
	D3DXMATRIX *pD3D,
	BOOL bInverse)
{
	//
	// Take the 4x4 homogenous GL matrix and make a D3DX matrix.
	//
	if (0 && pGL->type == MATRIX_IDENTITY) {
		// Shortcut
		D3DXMatrixIdentity(pD3D);
	} else {
		GLfloat *pM = (bInverse) ? pGL->inv : pGL->m;
		// TODO: Straight memcopy? Are both aligned/padded equally? If true, maybe we can use the GL matrix directly?
		pD3D->_11 = pM[0];
		pD3D->_12 = pM[1];
		pD3D->_13 = pM[2];
		pD3D->_14 = pM[3];
		pD3D->_21 = pM[4];
		pD3D->_22 = pM[5];
		pD3D->_23 = pM[6];
		pD3D->_24 = pM[7];
		pD3D->_31 = pM[8];
		pD3D->_32 = pM[9];
		pD3D->_33 = pM[10];
		pD3D->_34 = pM[11];
		pD3D->_41 = pM[12];
		pD3D->_42 = pM[13];
		pD3D->_43 = pM[14];
		pD3D->_44 = pM[15];
	}
}

//---------------------------------------------------------------------------

void gld_NEW_MODELVIEW(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

	_GLMatrixToD3DXMatrix(ctx->ModelviewMatrixStack.Top, &gld->matModelView, FALSE);
	_GLMatrixToD3DXMatrix(ctx->ModelviewMatrixStack.Top, &gld->matInvModelView, TRUE);
	_gldComputeWorldViewProject(gld);
}

//---------------------------------------------------------------------------

void gld_NEW_PROJECTION(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

	_GLMatrixToD3DXMatrix(ctx->ProjectionMatrixStack.Top, &gld->matProjection, FALSE);
	_gldD3DMatrixProjection(&gld->matProjection, gldCtx);
	_gldComputeWorldViewProject(gld);

	// Fog requires a compliant projection matrix
	if (ctx->Fog.Enabled) {
		_GLD_DX9_DEV(SetTransform(gld->pDev, D3DTS_PROJECTION, &gld->matProjection));
	}
}

//---------------------------------------------------------------------------

void gld_NEW_TEXTURE_MATRIX(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);
	int					i;

	for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
		_GLMatrixToD3DXMatrix(ctx->TextureMatrixStack[i].Top, &gld->matTexture[i], FALSE);
		_GLMatrixToD3DXMatrix(ctx->TextureMatrixStack[i].Top, &gld->matInvTexture[i], TRUE);
	}
}

//---------------------------------------------------------------------------

void gld_NEW_VIEWPORT(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    D3DVIEWPORT9    d3dvp;
//  GLint           x, y;
//  GLsizei         w, h;

    // Set depth range
    _GLD_DX9_DEV(GetViewport(gld->pDev, &d3dvp));
    // D3D can't do Quake1/Quake2 z-trick
    if (ctx->Viewport.Near <= ctx->Viewport.Far) {
        d3dvp.MinZ      = ctx->Viewport.Near;
        d3dvp.MaxZ      = ctx->Viewport.Far;
    } else {
        d3dvp.MinZ      = ctx->Viewport.Far;
        d3dvp.MaxZ      = ctx->Viewport.Near;
    }

	if (glb.bDisableZTrick) {
		// HACK. Weapons are clipped by very close geometry.
		if (ctx->Viewport.Near > ctx->Viewport.Far) {
			d3dvp.MinZ      = 0.0f;
			d3dvp.MaxZ      = 1.0f;
		}
		//gldLogPrintf(GLDLOG_SYSTEM, "N=%f, F=%f", ctx->Viewport.Near, ctx->Viewport.Far);
	}

/*  x = ctx->Viewport.X;
    y = ctx->Viewport.Y;
    w = ctx->Viewport.Width;
    h = ctx->Viewport.Height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (w > gldCtx->dwWidth)        w = gldCtx->dwWidth;
    if (h > gldCtx->dwHeight)       h = gldCtx->dwHeight;
    // Ditto for D3D viewport dimensions
    if (w+x > gldCtx->dwWidth)      w = gldCtx->dwWidth-x;
    if (h+y > gldCtx->dwHeight)     h = gldCtx->dwHeight-y;
    d3dvp.X         = x;
    d3dvp.Y         = gldCtx->dwHeight - (y + h);
    d3dvp.Width     = w;
    d3dvp.Height    = h;*/
    _GLD_DX9_DEV(SetViewport(gld->pDev, &d3dvp));

//  gld->fFlipWindowY = (float)gldCtx->dwHeight;
}

//---------------------------------------------------------------------------

void gld_NEW_SCISSOR(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    // Bail if IHV driver cannot scissor
    if (!gld->bCanScissor)
        return;

    // Set scissor rect
    if (ctx->Scissor.Enabled) {
        RECT rcRect;
        // Keep in mind that RECT's need an extra row and column
        rcRect.left     = ctx->Scissor.X;
        rcRect.right    = ctx->Scissor.X + ctx->Scissor.Width; // + 1;
        rcRect.top      = gldCtx->dwHeight - (ctx->Scissor.Y + ctx->Scissor.Height);
        rcRect.bottom   = rcRect.top + ctx->Scissor.Height;
        IDirect3DDevice9_SetScissorRect(gld->pDev, &rcRect);
    }

    // Enable/disable scissor as required
    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_SCISSORTESTENABLE, ctx->Scissor.Enabled));
}

//---------------------------------------------------------------------------

void gld_NEW_TRANSFORM(
    GLcontext *ctx)
{
    GLD_context         *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9      *gld    = GLD_GET_DX9_DRIVER(gldCtx);

	_GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_CLIPPLANEENABLE, ctx->Transform.ClipPlanesEnabled));
}

//---------------------------------------------------------------------------

void gld_update_state_DX9(
    GLcontext *ctx,
    GLuint new_state)
{
    GLD_context     *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9  *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    if (!gld || !gld->pDev)
        return;

	// Array Element helper
	_ae_invalidate_state(ctx, new_state);

#define _GLD_TEST_STATE(a)      \
    if (new_state & (a)) {      \
        gld##a(ctx);            \
        new_state &= ~(a);      \
    }

#define _GLD_TEST_STATE_DX9(a)  \
    if (new_state & (a)) {      \
        gld##a##_DX9(ctx);      \
        new_state &= ~(a);      \
    }

#define _GLD_IGNORE_STATE(a) new_state &= ~(a);

    _GLD_TEST_STATE(_NEW_MODELVIEW);
    _GLD_TEST_STATE(_NEW_PROJECTION);
	_GLD_TEST_STATE(_NEW_TEXTURE_MATRIX);

    _GLD_TEST_STATE_DX9(_NEW_TEXTURE); // extern, so guard with _DX9
    _GLD_TEST_STATE(_NEW_COLOR);
    _GLD_TEST_STATE(_NEW_DEPTH);
    _GLD_TEST_STATE(_NEW_POLYGON);
    _GLD_TEST_STATE(_NEW_STENCIL);
    _GLD_TEST_STATE(_NEW_FOG);
    _GLD_TEST_STATE(_NEW_LIGHT);
    _GLD_TEST_STATE(_NEW_VIEWPORT);

    _GLD_TEST_STATE(_NEW_TRANSFORM);

    // Scissor Test: New for DX9
    _GLD_TEST_STATE(_NEW_SCISSOR);

// Stubs for future use.
/*
	_GLD_TEST_STATE(_NEW_COLOR_MATRIX);
	_GLD_TEST_STATE(_NEW_ACCUM);
	_GLD_TEST_STATE(_NEW_EVAL);
	_GLD_TEST_STATE(_NEW_HINT);
	_GLD_TEST_STATE(_NEW_LINE);
	_GLD_TEST_STATE(_NEW_PIXEL);
	_GLD_TEST_STATE(_NEW_POINT);
	_GLD_TEST_STATE(_NEW_POLYGONSTIPPLE);
	_GLD_TEST_STATE(_NEW_PACKUNPACK);
	_GLD_TEST_STATE(_NEW_ARRAY);
	_GLD_TEST_STATE(_NEW_RENDERMODE);
	_GLD_TEST_STATE(_NEW_BUFFERS);
	_GLD_TEST_STATE(_NEW_MULTISAMPLE);
*/

// For debugging.
#if 0
#define _GLD_TEST_UNHANDLED_STATE(a)                                    \
    if (new_state & (a)) {                                  \
        gldLogMessage(GLDLOG_ERROR, "Unhandled " #a "\n");  \
    }
    _GLD_TEST_UNHANDLED_STATE(_NEW_COLOR_MATRIX);
    _GLD_TEST_UNHANDLED_STATE(_NEW_ACCUM);
    _GLD_TEST_UNHANDLED_STATE(_NEW_EVAL);
    _GLD_TEST_UNHANDLED_STATE(_NEW_HINT);
    _GLD_TEST_UNHANDLED_STATE(_NEW_LINE);
    _GLD_TEST_UNHANDLED_STATE(_NEW_PIXEL);
    _GLD_TEST_UNHANDLED_STATE(_NEW_POINT);
    _GLD_TEST_UNHANDLED_STATE(_NEW_POLYGONSTIPPLE);
    _GLD_TEST_UNHANDLED_STATE(_NEW_PACKUNPACK);
    _GLD_TEST_UNHANDLED_STATE(_NEW_ARRAY);
    _GLD_TEST_UNHANDLED_STATE(_NEW_RENDERMODE);
    _GLD_TEST_UNHANDLED_STATE(_NEW_BUFFERS);
    _GLD_TEST_UNHANDLED_STATE(_NEW_MULTISAMPLE);
#undef _GLD_UNHANDLED_STATE
#endif

#undef _GLD_TEST_STATE
	
	//
	// Examine Mesa state and set appropriate Vertex and Pixel Shaders.
	// Note that this is done after the above has updated state.
	//
	gldUpdateShaders(ctx);
}

//---------------------------------------------------------------------------
// Viewport
//---------------------------------------------------------------------------

void gld_Viewport_DX9(
    GLcontext *ctx,
    GLint x,
    GLint y,
    GLsizei w,
    GLsizei h)
{
    GLD_context     *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9  *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    D3DVIEWPORT9    d3dvp;

    if (!gld || !gld->pDev)
        return;

    // This is a hack. When the app is minimized, Mesa passes
    // w=1 and h=1 for viewport dimensions. Without this test
    // we get a GPF in gld_wgl_resize_buffers().
    if ((w==1) && (h==1))
        return;

    // Call ResizeBuffersMESA. This function will early-out
    // if no resize is needed.
    //ctx->Driver.ResizeBuffersMESA(ctx);
    // Mesa 5: Changed parameters
    ctx->Driver.ResizeBuffers(gldCtx->glBuffer);

    // Check for destroyed buffers in case of resize failure. (DaveM)
    gld = GLD_GET_DX9_DRIVER(gldCtx);
    if (!gld || !gld->pDev)
        return;

#if 0
    gldLogPrintf(GLDLOG_SYSTEM, ">> Viewport x=%d y=%d w=%d h=%d", x,y,w,h);
#endif

	if (glb.bFAKK2) {
		gld->fViewportY = (y < 0) ? y : 0.0f;
	}

    // ** D3D viewport must not be outside the render target surface **
    // Sanity check the GL viewport dimensions
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (w > gldCtx->dwWidth)        w = gldCtx->dwWidth;
    if (h > gldCtx->dwHeight)       h = gldCtx->dwHeight;
    // Ditto for D3D viewport dimensions
    if (w+x > gldCtx->dwWidth)      w = gldCtx->dwWidth-x;
    if (h+y > gldCtx->dwHeight)     h = gldCtx->dwHeight-y;

    d3dvp.X         = x;
    d3dvp.Y         = gldCtx->dwHeight - (y + h);
    d3dvp.Width     = w;
    d3dvp.Height    = h;
    if (ctx->Viewport.Near <= ctx->Viewport.Far) {
        d3dvp.MinZ      = ctx->Viewport.Near;
        d3dvp.MaxZ      = ctx->Viewport.Far;
    } else {
        d3dvp.MinZ      = ctx->Viewport.Far;
        d3dvp.MaxZ      = ctx->Viewport.Near;
    }

    // TODO: DEBUGGING -- No apparent effect (DaveM)
//    d3dvp.MinZ      = 0.0f;
//    d3dvp.MaxZ      = 1.0f;

    _GLD_DX9_DEV(SetViewport(gld->pDev, &d3dvp));
}

//---------------------------------------------------------------------------

void gld_ClipPlane_DX9(
	GLcontext *ctx,
	GLenum plane,
	const GLfloat *equation)
{
    GLD_context     *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9  *gld    = GLD_GET_DX9_DRIVER(gldCtx);
	GLint			p		= plane - (GLint)GL_CLIP_PLANE0;

	_GLD_DX9_DEV(SetClipPlane(gld->pDev, p, ctx->Transform._ClipUserPlane[p]));
}
 
//---------------------------------------------------------------------------

void gld_Lightfv( 
	GLcontext *ctx,
	GLenum light,
	GLenum pname,
	const GLfloat *params)
{
    GLD_context     *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9  *gld    = GLD_GET_DX9_DRIVER(gldCtx);
	GLint			i;

	// Only interested in 
	if (pname != GL_POSITION)
		return;

	// Mesa has already validated the input vars
	i = (GLint) (light - GL_LIGHT0);

	// Mesa stores this data in a transformed form, but we need to keep the pure data.
	// Keep in mind that if w==0 then this is a directional light,
	// and .xyz denotes the light direction.
	gld->LightDir[i].x = params[0];
	gld->LightDir[i].y = params[1];
	gld->LightDir[i].z = params[2];
	gld->LightDir[i].w = params[3];
}

//---------------------------------------------------------------------------
/*
void gld_TexGen(
	GLcontext *ctx,
	GLenum coord,
	GLenum pname,
	const GLfloat *params)
{
    GLD_context     *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9  *gld    = GLD_GET_DX9_DRIVER(gldCtx);
	GLuint			tUnit = ctx->Texture.CurrentUnit;
	D3DXVECTOR4		*pV;

	// Only interested in the EyePlane
	if (pname != GL_EYE_PLANE)
		return;

	switch (coord) {
	case GL_S:
		pV = &gld->EyePlaneS[tUnit];
		break;
	case GL_T:
		pV = &gld->EyePlaneT[tUnit];
		break;
	case GL_R:
		pV = &gld->EyePlaneR[tUnit];
		break;
	case GL_Q:
		pV = &gld->EyePlaneQ[tUnit];
		break;
	default:
		ASSERT(0);
		break;
	}

	// Copy vector
	pV->x = params[0];
	pV->y = params[1];
	pV->z = params[2];
	pV->w = params[3];
}
*/
//---------------------------------------------------------------------------

extern BOOL gldWglResizeBuffers(GLcontext *ctx, BOOL bDefaultDriver);

// Mesa 5: Parameter change
void gldResizeBuffers_DX9(
    GLframebuffer *fb)
{
    GET_CURRENT_CONTEXT(ctx);
    // Attempt to re-create draw buffers in case of resize failure. (DaveM)
    // *Serious Problems* if buffers become deallocated after here...
    if (!gldWglResizeBuffers(ctx, TRUE)) {
        GLD_ctx* gldCtx = gldGetContextAddress(gldGetCurrentContext());
        gldDestroyDrawable_DX(gldCtx);
        gldCreateDrawable_DX(gldCtx, FALSE, FALSE);
    }
}

//---------------------------------------------------------------------------
#ifdef _DEBUG
// This is only for debugging.
// To use, plug into ctx->Driver.Enable pointer below.
const char* _mesa_lookup_enum_by_nr(int);
void gld_Enable(
    GLcontext *ctx,
    GLenum e,
    GLboolean b)
{
    char buf[1024];
    sprintf(buf, "Enable: %s (%s)\n", _mesa_lookup_enum_by_nr(e), b?"TRUE":"FALSE");
    gldLogMessage(GLDLOG_SYSTEM, buf);
}
#endif
//---------------------------------------------------------------------------
// Driver pointer setup
//---------------------------------------------------------------------------

extern const GLubyte* _gldGetStringGeneric(GLcontext*, GLenum);

void gldSetupDriverPointers_DX9(
    GLcontext *ctx)
{
    GLD_context     *gldCtx = GLD_GET_CONTEXT(ctx);
    GLD_driver_dx9  *gld    = GLD_GET_DX9_DRIVER(gldCtx);

    // Mandatory functions
    ctx->Driver.GetString               = _gldGetStringGeneric;
    ctx->Driver.UpdateState             = gld_update_state_DX9;
    ctx->Driver.Clear                   = gld_Clear_DX9;
    ctx->Driver.DrawBuffer              = gld_set_draw_buffer_DX9;
    ctx->Driver.GetBufferSize           = gld_buffer_size_DX9;
    ctx->Driver.Finish                  = gld_Finish_DX9;
    ctx->Driver.Flush                   = gld_Flush_DX9;
    ctx->Driver.Error                   = gld_Error_DX9;

    // Hardware accumulation buffer
    ctx->Driver.Accum                   = NULL; // TODO: gld_Accum;

    // Bitmap functions
    ctx->Driver.CopyPixels              = gld_CopyPixels_DX9;
    ctx->Driver.DrawPixels              = gld_DrawPixels_DX9;
    ctx->Driver.ReadPixels              = gld_ReadPixels_DX9;
    ctx->Driver.Bitmap                  = gld_Bitmap_DX9;

    // Buffer resize
    ctx->Driver.ResizeBuffers           = gldResizeBuffers_DX9;
    
    // Texture image functions
    ctx->Driver.ChooseTextureFormat     = gld_ChooseTextureFormat_DX9;
    ctx->Driver.TexImage1D              = gld_TexImage1D_DX9;
    ctx->Driver.TexImage2D              = gld_TexImage2D_DX9;
    ctx->Driver.TexImage3D              = _mesa_store_teximage3d;
    ctx->Driver.TexSubImage1D           = gld_TexSubImage1D_DX9;
    ctx->Driver.TexSubImage2D           = gld_TexSubImage2D_DX9;
    ctx->Driver.TexSubImage3D           = _mesa_store_texsubimage3d;
    
    ctx->Driver.CopyTexImage1D          = gldCopyTexImage1D_DX9; //NULL;
    ctx->Driver.CopyTexImage2D          = gldCopyTexImage2D_DX9; //NULL;
    ctx->Driver.CopyTexSubImage1D       = gldCopyTexSubImage1D_DX9; //NULL;
    ctx->Driver.CopyTexSubImage2D       = gldCopyTexSubImage2D_DX9; //NULL;
    ctx->Driver.CopyTexSubImage3D       = gldCopyTexSubImage3D_DX9;
    ctx->Driver.TestProxyTexImage       = _mesa_test_proxy_teximage;

    // Texture object functions
    ctx->Driver.BindTexture             = NULL;
    ctx->Driver.CreateTexture           = NULL; // Not yet implemented by Mesa!;
    ctx->Driver.DeleteTexture           = gld_DeleteTexture_DX9;
    ctx->Driver.PrioritizeTexture       = NULL;

    // Imaging functionality
    ctx->Driver.CopyColorTable          = NULL;
    ctx->Driver.CopyColorSubTable       = NULL;
    ctx->Driver.CopyConvolutionFilter1D = NULL;
    ctx->Driver.CopyConvolutionFilter2D = NULL;

    // State changing functions
    ctx->Driver.AlphaFunc               = NULL; //gld_AlphaFunc;
    ctx->Driver.BlendFunc               = NULL; //gld_BlendFunc;
    ctx->Driver.ClearColor              = NULL; //gld_ClearColor;
    ctx->Driver.ClearDepth              = NULL; //gld_ClearDepth;
    ctx->Driver.ClearStencil            = NULL; //gld_ClearStencil;
    ctx->Driver.ColorMask               = NULL; //gld_ColorMask;
    ctx->Driver.CullFace                = NULL; //gld_CullFace;
    ctx->Driver.ClipPlane               = gld_ClipPlane_DX9;
    ctx->Driver.FrontFace               = NULL; //gld_FrontFace;
    ctx->Driver.DepthFunc               = NULL; //gld_DepthFunc;
    ctx->Driver.DepthMask               = NULL; //gld_DepthMask;
    ctx->Driver.DepthRange              = NULL;
    ctx->Driver.Enable                  = NULL; //gld_Enable;
    ctx->Driver.Fogfv                   = NULL; //gld_Fogfv;
    ctx->Driver.Hint                    = NULL; //gld_Hint;
    ctx->Driver.Lightfv                 = gld_Lightfv;
    ctx->Driver.LightModelfv            = NULL; //gld_LightModelfv;
    ctx->Driver.LineStipple             = NULL; //gld_LineStipple;
    ctx->Driver.LineWidth               = NULL; //gld_LineWidth;
    ctx->Driver.LogicOpcode             = NULL; //gld_LogicOpcode;
    ctx->Driver.PointParameterfv        = NULL; //gld_PointParameterfv;
    ctx->Driver.PointSize               = NULL; //gld_PointSize;
    ctx->Driver.PolygonMode             = NULL; //gld_PolygonMode;
    ctx->Driver.PolygonOffset           = NULL; //gld_PolygonOffset;
    ctx->Driver.PolygonStipple          = NULL; //gld_PolygonStipple;
    ctx->Driver.RenderMode              = NULL; //gld_RenderMode;
    ctx->Driver.Scissor                 = NULL; //gld_Scissor;
    ctx->Driver.ShadeModel              = NULL; //gld_ShadeModel;
    ctx->Driver.StencilFunc             = NULL; //gld_StencilFunc;
    ctx->Driver.StencilMask             = NULL; //gld_StencilMask;
    ctx->Driver.StencilOp               = NULL; //gld_StencilOp;
    ctx->Driver.TexGen                  = NULL; //gld_TexGen;
    ctx->Driver.TexEnv                  = NULL;
    ctx->Driver.TexParameter            = NULL;
    ctx->Driver.TextureMatrix           = NULL; //gld_TextureMatrix;
    ctx->Driver.Viewport                = gld_Viewport_DX9;
}

//---------------------------------------------------------------------------
