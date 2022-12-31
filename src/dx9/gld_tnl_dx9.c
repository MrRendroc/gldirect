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
* Environment:  Windows 9x/2000/XP
*
* Description:  Direct3D Transformation and Lighting support for Mesa 5.x.
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
#include "vtxfmt.h"
#include "dlist.h"
#include "state.h" // mesa_update_state()
#include "api_noop.h"
#include "api_arrayelt.h"
#include "m_eval.h" // Evaluator functions

GLboolean _mesa_validate_DrawElements(GLcontext *ctx,GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);

extern HGLRC	iCurrentContext;
extern HWND		g_hwndList; // Handle to error-output listbox in editor window

HRESULT _ParseEffectParameters(ID3DXEffect *pEffect, IDirect3DDevice9 *pDevice, const char *pszEffectFile);

void gld_NEW_PROJECTION(GLcontext *ctx);
void gld_NEW_MODELVIEW(GLcontext *ctx);

//---------------------------------------------------------------------------

// NOTE: GLD_driver_dx9 taken out of macro to
// aid VC6 AutoComplete and IntelliSense. :)
#define _D3DTNL_GET_CONTEXT									\
	GET_CURRENT_CONTEXT(ctx);								\
	GLD_context		*gldCtx	= GLD_GET_CONTEXT(ctx);

//---------------------------------------------------------------------------
// Utils
//---------------------------------------------------------------------------

static GLenum _ReducedPrimitive[GL_POLYGON+1] = {
	GL_POINTS,		// GL_POINTS			0x0000
	GL_LINES,		// GL_LINES				0x0001
	GL_LINES,		// GL_LINE_LOOP			0x0002
	GL_LINES,		// GL_LINE_STRIP		0x0003
	GL_TRIANGLES,	// GL_TRIANGLES			0x0004
	GL_TRIANGLES,	// GL_TRIANGLE_STRIP	0x0005
	GL_TRIANGLES,	// GL_TRIANGLE_FAN		0x0006
	GL_TRIANGLES,	// GL_QUADS				0x0007
	GL_TRIANGLES,	// GL_QUAD_STRIP		0x0008
	GL_TRIANGLES,	// GL_POLYGON			0x0009
};

//---------------------------------------------------------------------------

GLenum gldReducedPrim(
	GLenum mode)
{
	// Convert input primitive type to a reduced primitive type, since
	// we can only send points, lines and triangles to the graphics card.

	ASSERT((mode >= GL_POINTS) && (mode <= GL_POLYGON));

	return _ReducedPrimitive[mode];
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

D3DCOLOR gldClampedColour(
	GLfloat *c)
{
	//
	// Make a D3DCOLOR (DWORD) from input vector (GLfloat *color[4])
	// Each channel of the input colour must be clamped between 0.0 and 1.0
	//

	const float fScale	= 255.0f;
	int			r, g, b, a; // Ints!

	r = (int)(c[0] * fScale);
	if (r < 0)		r = 0;
	if (r > 255)	r = 255;

	g = (int)(c[1] * fScale);
	if (g < 0)		g = 0;
	if (g > 255)	g = 255;

	b = (int)(c[2] * fScale);
	if (b < 0)		b = 0;
	if (b > 255)	b = 255;

	a = (int)(c[3] * fScale);
	if (a < 0)		a = 0;
	if (a > 255)	a = 255;

	return D3DCOLOR_RGBA((DWORD)r, (DWORD)g, (DWORD)b, (DWORD)a);
}

//---------------------------------------------------------------------------

static void _SetColorvalue(D3DCOLORVALUE *cv, float r, float g, float b, float a)
{
	cv->r = r;
	cv->g = g;
	cv->b = b;
	cv->a = a;
}

//---------------------------------------------------------------------------

static void _SetColorvaluev(D3DCOLORVALUE *cv, const GLfloat *f)
{
	// Vector version of _SetColorvalue
	cv->r = f[0];
	cv->g = f[1];
	cv->b = f[2];
	cv->a = f[3];
}

//---------------------------------------------------------------------------

static void _GLFloatToD3DXVEC4(D3DXVECTOR4 *vec, const GLfloat *f)
{
	vec->x = f[0];
	vec->y = f[1];
	vec->z = f[2];
	vec->w = f[3];
}

//---------------------------------------------------------------------------

static void _gldEnlargePrimitiveBuffer(
	GLD_driver_dx9 *gld)
{
	// Ran out of space in the primitive buffer. Enlarge it.
	// Enlarge in chunks of vertices; adding a single vertex at a time is Not Good
	gld->dwMaxPrimVerts += GLD_PRIM_BLOCK_SIZE;
	gld->pPrim = realloc(gld->pPrim, GLD_4D_VERTEX_SIZE * gld->dwMaxPrimVerts);
	ASSERT(gld->pPrim);
#ifdef DEBUG
	// Useful info to know; dump it in Debug builds
	gldLogPrintf(GLDLOG_SYSTEM, "** Primitive buffer enlarged to %d verts **", gld->dwMaxPrimVerts);
#endif
}

//---------------------------------------------------------------------------

static void _gldEmitVertex(
	GLcontext *ctx,
	GLD_4D_VERTEX *pVin)
{
	//
	// Helper function for evaluators.
	//

	GLD_context		*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9	*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_4D_VERTEX	*pV;	// Pointer to vertex in primitive buffer

	// Ensure there's room in the primitive buffer
	if (gld->dwPrimVert >= gld->dwMaxPrimVerts)
		_gldEnlargePrimitiveBuffer(gld);

	pV = &gld->pPrim[gld->dwPrimVert];

	// Copy vertex
	*pV = *pVin;

	// Advance to next vertex
	gld->dwPrimVert++;
}

//---------------------------------------------------------------------------
// Evaluators
//---------------------------------------------------------------------------

static void GLAPIENTRY d3dEvalCoord1f(
	GLfloat u)
{
	GET_CURRENT_CONTEXT(ctx);

	// Evaluated components
	GLfloat					Color[4]	= {0,0,0,1};
	GLfloat					Normal[4]	= {0,0,0,1};
	GLfloat					Texture[4]	= {0,0,0,1};
	GLfloat					Position[4]	= {0,0,0,1};

	// Eval vars
	const struct gl_1d_map	*map;
	GLfloat					uu;
	GLuint					sz;

	// A vertex to be filled with eval data and stored
	GLD_4D_VERTEX			gldV;

	// No effect if vertex maps disabled.
	// NOTE: VertexProgramNV not supported!
	if (!ctx->Eval.Map1Vertex4 && !ctx->Eval.Map1Vertex3)
		return;

	//
	// Colour evaluator
	//
	if (ctx->Eval.Map1Color4) {
		map = &ctx->EvalMap.Map1Color4;
		uu = (u - map->u1) * map->du;
		sz = 4;
		_math_horner_bezier_curve(map->Points, Color, uu, sz, map->Order);
	} else {
		GLfloat *color	= ctx->Current.Attrib[VERT_ATTRIB_COLOR0];
		Color[0] = color[0];
		Color[1] = color[1];
		Color[2] = color[2];
		Color[3] = color[3];
	}

	//
	// Normal evaluator
	//
	if (ctx->Eval.Map1Normal) {
		map = &ctx->EvalMap.Map1Normal;
		uu = (u - map->u1) * map->du;
		sz = 3;
		_math_horner_bezier_curve(map->Points, Normal, uu, sz, map->Order);
	}

	// Copy default texture coordinate values before they're possibly altered
	gldV.Tex1.x		= Texture[0];
	gldV.Tex1.y		= Texture[1];
	gldV.Tex1.z		= Texture[2];
	gldV.Tex1.w		= Texture[3];

	//
	// Choose texture-coordinate evaluator. Higher evals takes precedence
	//
	map = NULL;
	if (ctx->Eval.Map1TextureCoord4) {
		map = &ctx->EvalMap.Map1Texture4;
		sz = 4;
	} else if (ctx->Eval.Map1TextureCoord3) {
		map = &ctx->EvalMap.Map1Texture3;
		sz = 3;
	} else if (ctx->Eval.Map1TextureCoord2) {
		map = &ctx->EvalMap.Map1Texture2;
		sz = 2;
	} else if (ctx->Eval.Map1TextureCoord1) {
		map = &ctx->EvalMap.Map1Texture1;
		sz = 1;
	}
	//
	// Execute highest enabled texture evaluator.
	//
	if (map) {
		uu = (u - map->u1) * map->du;
		_math_horner_bezier_curve(map->Points, Texture, uu, sz, map->Order);
	}

	//
	// Choose vertex evaluator. Higher evals takes precedence
	//
	map = NULL;
	if (ctx->Eval.Map1Vertex4) {
		map = &ctx->EvalMap.Map1Vertex4;
		sz = 4;
	} else if (ctx->Eval.Map1Vertex3) {
		map = &ctx->EvalMap.Map1Vertex3;
		sz = 3;
	}

	// Function is a nop if vertex eval is not enabled.
	// [Tested above, but repeated here as a sanity check]
	if (!map) {
		return;
	}

	// Evaluate the vertex
	uu = (u - map->u1) * map->du;
	_math_horner_bezier_curve(map->Points, Position, uu, sz, map->Order);

	// Fill in vertex elements
	gldV.Position.x	= Position[0];
	gldV.Position.y	= Position[1];
	gldV.Position.z	= Position[2];
	gldV.Position.w	= Position[3];
	gldV.Normal.x	= Normal[0];
	gldV.Normal.y	= Normal[1];
	gldV.Normal.z	= Normal[2];
	gldV.Diffuse	= gldClampedColour(Color);
	gldV.Tex0.x		= Texture[0];
	gldV.Tex0.y		= Texture[1];
	gldV.Tex0.z		= Texture[2];
	gldV.Tex0.w		= Texture[3];

	// Emit the vertex
	_gldEmitVertex(ctx, &gldV);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dEvalCoord2f(
	GLfloat u,
	GLfloat v)
{
	GET_CURRENT_CONTEXT(ctx);

	// Evaluated components
	GLfloat					Color[4]	= {0,0,0,1};
	GLfloat					Normal[4]	= {0,0,0,1};
	GLfloat					Texture[4]	= {0,0,0,1};
	GLfloat					Position[4]	= {0,0,0,1};

	// Eval vars
	const struct gl_2d_map	*map;
	GLfloat					uu, vv;
	GLuint					sz;

	// A vertex to be filled with eval data and stored
	GLD_4D_VERTEX			gldV;

	// No effect if vertex maps disabled.
	// NOTE: VertexProgramNV not supported!
	if (!ctx->Eval.Map2Vertex4 && !ctx->Eval.Map2Vertex3)
		return;

	//
	// Colour evaluator
	//
	if (ctx->Eval.Map2Color4) {
		map = &ctx->EvalMap.Map2Color4;
		uu = (u - map->u1) * map->du;
		vv = (v - map->v1) * map->dv;
		sz = 4;
		_math_horner_bezier_surf(map->Points, Color, uu, vv, sz, map->Uorder, map->Vorder);
	}

	//
	// Normal evaluator
	//
	if (ctx->Eval.Map2Normal) {
		map = &ctx->EvalMap.Map2Normal;
		uu = (u - map->u1) * map->du;
		vv = (v - map->v1) * map->dv;
		sz = 3;
		_math_horner_bezier_surf(map->Points, Normal, uu, vv, sz, map->Uorder, map->Vorder);
	}

	// Copy default texture coordinate values before they're possibly altered
	gldV.Tex1.x		= Texture[0];
	gldV.Tex1.y		= Texture[1];
	gldV.Tex1.z		= Texture[2];
	gldV.Tex1.w		= Texture[3];

	//
	// Choose texture-coordinate evaluator. Higher evals takes precedence
	//
	map = NULL;
	if (ctx->Eval.Map2TextureCoord4) {
		map = &ctx->EvalMap.Map2Texture4;
		sz = 4;
	} else if (ctx->Eval.Map2TextureCoord3) {
		map = &ctx->EvalMap.Map2Texture3;
		sz = 3;
	} else if (ctx->Eval.Map2TextureCoord2) {
		map = &ctx->EvalMap.Map2Texture2;
		sz = 2;
	} else if (ctx->Eval.Map2TextureCoord1) {
		map = &ctx->EvalMap.Map2Texture1;
		sz = 1;
	}
	//
	// Execute highest enabled texture evaluator.
	//
	if (map) {
		uu = (u - map->u1) * map->du;
		vv = (v - map->v1) * map->dv;
		_math_horner_bezier_surf(map->Points, Texture, uu, vv, sz, map->Uorder, map->Vorder);
	}

	//
	// Choose vertex evaluator. Higher evals takes precedence
	//
	map = NULL;
	if (ctx->Eval.Map2Vertex4) {
		map = &ctx->EvalMap.Map2Vertex4;
		sz = 4;
	} else if (ctx->Eval.Map2Vertex3) {
		map = &ctx->EvalMap.Map2Vertex3;
		sz = 3;
	}

	// Function is a nop if vertex eval is not enabled.
	// [Tested above, but repeated here as a sanity check]
	if (!map) {
		return;
	}

	// Evaluate the vertex
	uu = (u - map->u1) * map->du;
	vv = (v - map->v1) * map->dv;

	if (ctx->Eval.AutoNormal) {
		GLfloat du[4], dv[4];
		
		_math_de_casteljau_surf(map->Points, Position, du, dv, uu, vv, sz, map->Uorder, map->Vorder);
		
		if (sz == 4) {
			du[0] = du[0]*Position[3] - du[3]*Position[0];
			du[1] = du[1]*Position[3] - du[3]*Position[1];
			du[2] = du[2]*Position[3] - du[3]*Position[2];
			
			dv[0] = dv[0]*Position[3] - dv[3]*Position[0];
			dv[1] = dv[1]*Position[3] - dv[3]*Position[1];
			dv[2] = dv[2]*Position[3] - dv[3]*Position[2];
		}
		
		CROSS3(Normal, du, dv);
		NORMALIZE_3FV(Normal);
		Normal[3] = 1.0;
		
	} else {
		_math_horner_bezier_surf(map->Points, Position, uu, vv, sz, map->Uorder, map->Vorder);
	}

	// Fill in vertex elements
	gldV.Position.x	= Position[0];
	gldV.Position.y	= Position[1];
	gldV.Position.z	= Position[2];
	gldV.Position.w	= Position[3];
	gldV.Normal.x	= Normal[0];
	gldV.Normal.y	= Normal[1];
	gldV.Normal.z	= Normal[2];
	gldV.Diffuse	= gldClampedColour(Color);
	gldV.Tex0.x		= Texture[0];
	gldV.Tex0.y		= Texture[1];
	gldV.Tex0.z		= Texture[2];
	gldV.Tex0.w		= Texture[3];

	// Emit the vertex
	_gldEmitVertex(ctx, &gldV);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dEvalCoord1fv(
	const GLfloat *u)
{
	d3dEvalCoord1f(u[0]);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dEvalCoord2fv(
	const GLfloat *u)
{
	d3dEvalCoord2f(u[0], u[1]);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dEvalPoint1(
	GLint i)
{
	GET_CURRENT_CONTEXT( ctx );

	GLfloat du = ((ctx->Eval.MapGrid1u2 - ctx->Eval.MapGrid1u1) / (GLfloat) ctx->Eval.MapGrid1un);
	GLfloat u = i * du + ctx->Eval.MapGrid1u1;

	d3dEvalCoord1f(u);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dEvalPoint2(
	GLint i,
	GLint j)
{
	GET_CURRENT_CONTEXT( ctx );

	GLfloat du = ((ctx->Eval.MapGrid2u2 - ctx->Eval.MapGrid2u1) / (GLfloat) ctx->Eval.MapGrid2un);
	GLfloat dv = ((ctx->Eval.MapGrid2v2 - ctx->Eval.MapGrid2v1) / (GLfloat) ctx->Eval.MapGrid2vn);
	GLfloat u = i * du + ctx->Eval.MapGrid2u1;
	GLfloat v = j * dv + ctx->Eval.MapGrid2v1;
	
	d3dEvalCoord2f(u, v);
}

//---------------------------------------------------------------------------
// Vertices
//---------------------------------------------------------------------------

static void GLAPIENTRY d3dVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	GET_CURRENT_CONTEXT(ctx);
	GLD_context		*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9	*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_4D_VERTEX	*pV;	// Pointer to vertex in primitive buffer
	GLfloat			*color	= ctx->Current.Attrib[VERT_ATTRIB_COLOR0];
	GLfloat			*normal	= ctx->Current.Attrib[VERT_ATTRIB_NORMAL];
	GLfloat			*tex0	= ctx->Current.Attrib[VERT_ATTRIB_TEX0];
	GLfloat			*tex1	= ctx->Current.Attrib[VERT_ATTRIB_TEX1];

	// Bail if not inside a valid primitive
	if (ctx->Driver.CurrentExecPrimitive == PRIM_OUTSIDE_BEGIN_END)
		return;

	// Ensure there's room in the primitive buffer
	if (gld->dwPrimVert >= gld->dwMaxPrimVerts)
		_gldEnlargePrimitiveBuffer(gld);

	pV = &gld->pPrim[gld->dwPrimVert];

	// Fill current vertex
	pV->Position.x	= x;
	pV->Position.y	= y - gld->fViewportY;
	pV->Position.z	= z;
	pV->Position.w	= w;
	pV->Diffuse		= gldClampedColour(color);
	pV->Tex0.x		= tex0[0];
	pV->Tex0.y		= tex0[1];
	pV->Tex0.z		= tex0[2];
	pV->Tex0.w		= tex0[3];
	pV->Tex1.x		= tex1[0];
	pV->Tex1.y		= tex1[1];
	pV->Tex1.z		= tex1[2];
	pV->Tex1.w		= tex1[3];
	pV->Normal.x	= normal[0];
	pV->Normal.y	= normal[1];
	pV->Normal.z	= normal[2];

	// Advance to next vertex
	gld->dwPrimVert++;
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dVertex2f(GLfloat x, GLfloat y)
{
	d3dVertex4f(x, y, 0, 1);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dVertex2fv(const GLfloat *v)
{
	d3dVertex4f(v[0], v[1], 0, 1);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dVertex3fv(const GLfloat *v)
{
	d3dVertex4f(v[0], v[1], v[2], 1);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	d3dVertex4f(x, y, z, 1);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dVertex4fv(const GLfloat *v)
{
	d3dVertex4f(v[0], v[1], v[2], v[3]);
}

//---------------------------------------------------------------------------
// Support for nVidia's VertexAttribNV extension.
//---------------------------------------------------------------------------

static __inline void _gldEmitVertexNV(GLcontext *ctx)
{
	GLfloat *pV = &ctx->Current.Attrib[VERT_ATTRIB_POS][0];
	d3dVertex4f(pV[0], pV[1], pV[2], pV[3]);
}

//---------------------------------------------------------------------------

void GLAPIENTRY d3dVertexAttrib1fNV(
	GLuint index,
	GLfloat x)
{
	GET_CURRENT_CONTEXT(ctx);
	if (index < VERT_ATTRIB_MAX) {
		ASSIGN_4V(ctx->Current.Attrib[index], x, 0, 0, 1);
		if (index == VERT_ATTRIB_POS) {
			_gldEmitVertexNV(ctx);
		}
	}
	else
		_mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib1f" );
}

//---------------------------------------------------------------------------

void GLAPIENTRY d3dVertexAttrib1fvNV(
	GLuint index,
	const GLfloat *v)
{
	GET_CURRENT_CONTEXT(ctx);
	if (index < VERT_ATTRIB_MAX) {
		ASSIGN_4V(ctx->Current.Attrib[index], v[0], 0, 0, 1);
		if (index == VERT_ATTRIB_POS) {
			_gldEmitVertexNV(ctx);
		}
	}
	else
		_mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib1fv" );
}

//---------------------------------------------------------------------------

void GLAPIENTRY d3dVertexAttrib2fNV(
	GLuint index,
	GLfloat x,
	GLfloat y)
{
	GET_CURRENT_CONTEXT(ctx);
	if (index < VERT_ATTRIB_MAX) {
		ASSIGN_4V(ctx->Current.Attrib[index], x, y, 0, 1);
		if (index == VERT_ATTRIB_POS) {
			_gldEmitVertexNV(ctx);
		}
	}
	else
		_mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib2f" );
}

//---------------------------------------------------------------------------

void GLAPIENTRY d3dVertexAttrib2fvNV(
	GLuint index,
	const GLfloat *v)
{
	GET_CURRENT_CONTEXT(ctx);
	if (index < VERT_ATTRIB_MAX) {
		ASSIGN_4V(ctx->Current.Attrib[index], v[0], v[1], 0, 1);
		if (index == VERT_ATTRIB_POS) {
			_gldEmitVertexNV(ctx);
		}
	}
	else
		_mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib2fv" );
}

//---------------------------------------------------------------------------

void GLAPIENTRY d3dVertexAttrib3fNV(
	GLuint index,
	GLfloat x,
	GLfloat y,
	GLfloat z)
{
	GET_CURRENT_CONTEXT(ctx);
	if (index < VERT_ATTRIB_MAX) {
		ASSIGN_4V(ctx->Current.Attrib[index], x, y, z, 1);
		if (index == VERT_ATTRIB_POS) {
			_gldEmitVertexNV(ctx);
		}
	}
	else
		_mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib3f" );
}

//---------------------------------------------------------------------------

void GLAPIENTRY d3dVertexAttrib3fvNV(
	GLuint index,
	const GLfloat *v)
{
	GET_CURRENT_CONTEXT(ctx);
	if (index < VERT_ATTRIB_MAX) {
		ASSIGN_4V(ctx->Current.Attrib[index], v[0], v[1], v[2], 1);
		if (index == VERT_ATTRIB_POS) {
			_gldEmitVertexNV(ctx);
		}
	}
	else
		_mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib3fv" );
}

//---------------------------------------------------------------------------

void GLAPIENTRY d3dVertexAttrib4fNV(
	GLuint index,
	GLfloat x,
	GLfloat y,
	GLfloat z,
	GLfloat w)
{
	GET_CURRENT_CONTEXT(ctx);
	if (index < VERT_ATTRIB_MAX) {
		ASSIGN_4V(ctx->Current.Attrib[index], x, y, z, w);
		if (index == VERT_ATTRIB_POS) {
			_gldEmitVertexNV(ctx);
		}
	}
	else
		_mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib4f" );
}

//---------------------------------------------------------------------------

void GLAPIENTRY d3dVertexAttrib4fvNV(
	GLuint index,
	const GLfloat *v)
{
	GET_CURRENT_CONTEXT(ctx);
	if (index < VERT_ATTRIB_MAX) {
		ASSIGN_4V(ctx->Current.Attrib[index], v[0], v[1], v[2], v[3]);
		if (index == VERT_ATTRIB_POS) {
			_gldEmitVertexNV(ctx);
		}
	}
	else
		_mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib4fv" );
}

//---------------------------------------------------------------------------
// Begin/End
//---------------------------------------------------------------------------

static void gldSuperPrimitive(
	GLcontext *ctx,
	GLD_driver_dx9 *gld)
{
	// The current primitive is too big to fit into a our vertex buffer in one go.
	// Split the primitive into manageable multiples.

	// TODO: Actually put some real code in this function...
	gldLogMessage(GLDLOG_SYSTEM, "~~ imm: gldSuperPrimitive! ~~\n");

	// Reset state before returning
	gldResetPrimitiveBuffer(gld);
	ctx->Driver.NeedFlush &= ~FLUSH_STORED_VERTICES;
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dBegin(GLenum mode)
{
	GET_CURRENT_CONTEXT(ctx);
	GLD_context		*gldCtx			= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9	*gld			= GLD_GET_DX9_DRIVER(gldCtx);
	GLenum			GLReducedPrim;

	if (ctx->Driver.CurrentExecPrimitive == PRIM_OUTSIDE_BEGIN_END) {
		// Update Mesa
		if (ctx->NewState)
			_mesa_update_state(ctx);

		GLReducedPrim						= gldReducedPrim(mode);
		ctx->Driver.CurrentExecPrimitive	= mode;

		// Test for a change in reduced primitive
		if (GLReducedPrim != gld->GLReducedPrim) {
			// Primitive has changed; flush current primitve
			FLUSH_VERTICES(ctx, 0);
			gld->GLReducedPrim	= GLReducedPrim;
		}

		ctx->Driver.CurrentExecPrimitive = mode;
	} else 
		_mesa_error( ctx, GL_INVALID_OPERATION, "glBegin" );
}

//---------------------------------------------------------------------------

static void GLAPIENTRY d3dEnd(void)
{
	GET_CURRENT_CONTEXT(ctx);
	GLD_context		*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9	*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	int					nGLPrimitives;	// Number of GL primitives
	int					nD3DPrimitives;	// Number of Direct3D primitives
	int					nD3DVertices;	// Number of vertices that make up nPrimtives
	GLD_4D_VERTEX		*pSrc;			// Source Vertex
	GLD_4D_VERTEX		*LockPointer;	// Pointer to VB memory
	GLD_4D_VERTEX		*pDst;			// First vertex to start filling
	DWORD				dwFlags;
	DWORD				dwOffset, dwSize;	// Size and offset of lock
	int					j, count;
	GLuint				parity;

	if (ctx->Driver.CurrentExecPrimitive == PRIM_OUTSIDE_BEGIN_END) {
		_mesa_error( ctx, GL_INVALID_OPERATION, "glEnd" );
		goto d3dEnd_bail;
	}

	if (gld->dwPrimVert == 0) {
		goto d3dEnd_bail; // Nothing to do...
	}

	nGLPrimitives = nD3DPrimitives = nD3DVertices = 0;

	// Calculate primitive count
	switch (ctx->Driver.CurrentExecPrimitive) {
	case GL_POINTS:
		nGLPrimitives	= gld->dwPrimVert;
		nD3DPrimitives	= nGLPrimitives; // One vertex per point
		nD3DVertices	= nD3DPrimitives; // One vertex per point
		count			= nD3DVertices;
		break;
	case GL_LINES:
		nGLPrimitives	= gld->dwPrimVert / 2;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 2; // Two vertices per line
		count			= nD3DVertices;
		break;
	case GL_LINE_LOOP:
		nGLPrimitives	= gld->dwPrimVert;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 2; // Two vertices per line
		count			= nD3DPrimitives;
		break;
	case GL_LINE_STRIP:
		nGLPrimitives	= gld->dwPrimVert - 1;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 2; // Two vertices per line
		count			= nD3DPrimitives;
		break;
	case GL_TRIANGLES:
		nGLPrimitives	= gld->dwPrimVert / 3;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 3; // Three vertices per triangle
		count			= nD3DVertices;
		break;
	case GL_TRIANGLE_STRIP:
		nGLPrimitives	= gld->dwPrimVert - 2;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 3; // Three vertices per triangle
		count			= gld->dwPrimVert;
		break;
	case GL_TRIANGLE_FAN:
		nGLPrimitives	= gld->dwPrimVert - 2;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 3; // Three vertices per triangle
		count			= gld->dwPrimVert;
		break;
	case GL_QUAD_STRIP:
		nGLPrimitives	= (gld->dwPrimVert - 2) / 2;
		nD3DPrimitives	= nGLPrimitives * 2;
		nD3DVertices	= nD3DPrimitives * 3; // Three vertices per triangle, two tris per quad
		count			= gld->dwPrimVert;
		break;
	case GL_QUADS:
		nGLPrimitives	= gld->dwPrimVert / 4;
		nD3DPrimitives	= nGLPrimitives * 2;	// Two tris per quad
		nD3DVertices	= nD3DPrimitives * 3;	// Three vertices per triangle, two tris per quad
		count			= gld->dwPrimVert;
		break;
	case GL_POLYGON:
		nGLPrimitives	= 1;
		nD3DPrimitives	= gld->dwPrimVert - 2;	// Two tris per quad
		nD3DVertices	= nD3DPrimitives * 3;	// Three vertices per triangle
		count			= nD3DPrimitives;
		break;
	default:
		ASSERT(0); // BANG!
	}

#if 0
	//
	// ** DEBUGGING **
	//
	if (gld->GLPrim == GL_LINE_STRIP) {
		nGLPrimitives = nD3DPrimitives = nD3DVertices = 0;
	}
#endif

	// Test for invalid primitives
	if ((nGLPrimitives <= 0) || (nD3DPrimitives <= 0) || (nD3DVertices <= 0)) {
		// Bail if too few vertices
		goto d3dEnd_bail;
	}

	// Detect Super Primitives
	if (nD3DVertices > gld->dwMaxVBVerts) {
		// Huge primitive - pass off work to another function
		gldSuperPrimitive(ctx, gld);
		goto d3dEnd_bail;
	}

	// Determine whether there's enough room in the VB for the new vertices
	if ((gld->dwNextVBVert + nD3DVertices) >= gld->dwMaxVBVerts) {
		// No room - make some!
		FLUSH_VERTICES(ctx, 0);
		// Start at the beginning of the buffer again
		gld->dwFirstVBVert = gld->dwNextVBVert = 0;
	}

	// Pointer to first vertex in primitive
	pSrc = gld->pPrim;

	// Decide on the flags for the Lock
	if (gld->dwNextVBVert == 0) {
		// Lock and discard the whole buffer
		dwOffset	= 0;
		dwSize		= 0;
		dwFlags		= D3DLOCK_DISCARD;
	} else {
		// Lock only the vertices we need
		dwOffset	= GLD_4D_VERTEX_SIZE * gld->dwNextVBVert;
		dwSize		= GLD_4D_VERTEX_SIZE * nD3DVertices;
		dwFlags		= D3DLOCK_NOOVERWRITE;
	}

	// Lock vertex buffer ready for filling and obtain pointer to it's memory
	_GLD_DX9_VB(Lock(gld->pVB, dwOffset, dwSize, &LockPointer, dwFlags));

	// Calculate where to start filling
	pDst = LockPointer;

	// Put vertices into VB
	// NOTE: Keep Provoking Vertex in mind! D3D takes flatshaded colour from 1st vertex in primitive
	switch (ctx->Driver.CurrentExecPrimitive) {
	case GL_POINTS:
		// Straight one-to-one copy
		memcpy(pDst, pSrc, GLD_4D_VERTEX_SIZE * nD3DVertices);
		break;
	case GL_LINES:
		// Flatshaded colour: GL=second vertex, D3D=first vertex
		for (j=0; j<count; j+=2, pDst+=2, pSrc+=2) {
			pDst[0] = pSrc[1];
			pDst[1] = pSrc[0];
		}
		break;
	case GL_LINE_LOOP:
		for (j=1; j<count; j++, pDst+=2) {
			pDst[0] = pSrc[j];
			pDst[1] = pSrc[j-1];
		}
		// Close off the loop with a line from the last to the first vertex
		pDst[0] = pSrc[j-1];
		pDst[1] = pSrc[0];
		break;
	case GL_LINE_STRIP:
		for (j=1; j<count+1; j++, pDst+=2) {
			pDst[0] = pSrc[j];
			pDst[1] = pSrc[j-1];
		}
		break;
	case GL_TRIANGLES:
		// Can't memcpy because of provoking vertex
		//memcpy(pDst, pSrc, GLD_4D_VERTEX_SIZE * nD3DVertices);
		for (j=0; j<count; j+=3, pDst+=3, pSrc+=3) {
			pDst[0] = pSrc[2];
			pDst[1] = pSrc[0];
			pDst[2] = pSrc[1];
		}
		break;
	case GL_TRIANGLE_STRIP:
		parity = 0;
		for (j=2; j<count; j++, parity^=1, pDst+=3) {
			//RENDER_TRI( ELT(j-2+parity), ELT(j-1-parity), ELT(j) );
			pDst[0] = pSrc[j];
			pDst[1] = pSrc[j-2+parity];
			pDst[2] = pSrc[j-1-parity];
		}
		break;
	case GL_TRIANGLE_FAN:
		for (j=2; j<count; j++, pDst+=3) {
			//RENDER_TRI( ELT(start), ELT(j-1), ELT(j) );
			pDst[0] = pSrc[j];
			pDst[1] = pSrc[0];
			pDst[2] = pSrc[j-1];
		}
		break;
	case GL_QUAD_STRIP:
		//for (j=start+3;j<count;j+=2) {
		//	RENDER_QUAD( ELT(j-1), ELT(j-3), ELT(j-2), ELT(j) );
		//}
		for (j=3; j<count; j+=2, pDst+=6) {
			pDst[0] = pSrc[j];
			pDst[1] = pSrc[j-1];
			pDst[2] = pSrc[j-3];
			pDst[3] = pSrc[j];
			pDst[4] = pSrc[j-3];
			pDst[5] = pSrc[j-2];
		}
		break;
	case GL_QUADS:
		// Every four input vertices makes up two triangles
		for (j=0; j<count; j+=4, pDst+=6, pSrc+=4) {
			pDst[0] = pSrc[3];
			pDst[1] = pSrc[0];
			pDst[2] = pSrc[1];
			pDst[3] = pSrc[3];
			pDst[4] = pSrc[1];
			pDst[5] = pSrc[2];
		}
		break;
	case GL_POLYGON:
		// Flatshade colour for each triangle comes from 1st vertex
		for (j=1; j<count+1; j++, pDst+=3) {
			pDst[0] = pSrc[0];
			pDst[1] = pSrc[j];
			pDst[2] = pSrc[j+1];
		}
		break;
	default:
		ASSERT(0); // Sanity test...
	}

	// Unlock vertex buffer
	_GLD_DX9_VB(Unlock(gld->pVB));

	// Update count of vertices in VB
	gld->dwNextVBVert += nD3DVertices;

	// Notify a need to flush
	ctx->Driver.NeedFlush |= FLUSH_STORED_VERTICES;

d3dEnd_bail:
	// Prepare for next primitive
	gld->dwPrimVert	= 0;
	ctx->Driver.CurrentExecPrimitive = PRIM_OUTSIDE_BEGIN_END;
}

//---------------------------------------------------------------------------
// Driver callbacks
//---------------------------------------------------------------------------

static void d3dFlushVertices(
	GLcontext *ctx,
	GLuint flags)
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	D3DPRIMITIVETYPE	d3dpt;
	DWORD				nVertices, nPrimitives;

	if (flags & FLUSH_UPDATE_CURRENT) {
	}

	if (!(flags & FLUSH_STORED_VERTICES))
		return; // Not being asked to flush vertices

	// Determine number of vertices in current batch
	nVertices = gld->dwNextVBVert - gld->dwFirstVBVert;

	// Bail if nothing to do
	if (nVertices <= 0) {
		goto bail;
	}

	// Determine number of primitives to draw
	switch (gld->GLReducedPrim) {
	case GL_POINTS:
		IDirect3DDevice9_SetRenderState(gld->pDev, D3DRS_POINTSIZE, *((DWORD*)&ctx->Point._Size));
		d3dpt		= D3DPT_POINTLIST;
		nPrimitives	= nVertices;
		break;
	case GL_LINES:
		d3dpt		= D3DPT_LINELIST;
		nPrimitives	= nVertices / 2;
		break;
	case GL_TRIANGLES:
		d3dpt		= D3DPT_TRIANGLELIST;
		nPrimitives	= nVertices / 3;
		break;
	case PRIM_UNKNOWN:
		return; // Invalid primitive type
	default:
		ASSERT(0);
		return;
	}

	// Bail if nothing to do
	if (nPrimitives <= 0) {
		goto bail;
	}

#if 1
	//
	// Runtime Shaders. Should use these all the time.
	//

	{
		GLD_effect	*pGLDEffect;
		pGLDEffect = &gld->Effects[gld->iCurEffect];
		ID3DXEffect_CommitChanges(pGLDEffect->pEffect);
	}

	// TODO: Reduce redundant SetStreamSource() calls
	_GLD_DX9_DEV(SetStreamSource(gld->pDev, 0, gld->pVB, 0, GLD_4D_VERTEX_SIZE));
	_GLD_DX9_DEV(DrawPrimitive(gld->pDev, d3dpt, gld->dwFirstVBVert, nPrimitives));
#else
	//
	// Fixed Function. For testing only.
	//
	_GLD_DX9_DEV(SetTransform(gld->pDev, D3DTS_PROJECTION, &gld->matProjection));
	_GLD_DX9_DEV(SetTransform(gld->pDev, D3DTS_WORLD, &gld->matModelView));
	_GLD_DX9_DEV(SetPixelShader(gld->pDev, NULL));
	_GLD_DX9_DEV(SetVertexShader(gld->pDev, NULL));
	_GLD_DX9_DEV(DrawPrimitive(gld->pDev, d3dpt, gld->dwFirstVBVert, nPrimitives));
#endif

bail:
	gld->dwFirstVBVert		= gld->dwNextVBVert;
	ctx->Driver.NeedFlush	= 0;
}

//---------------------------------------------------------------------------

static void d3dMakeCurrent(GLcontext *ctx, GLframebuffer *drawBuffer, GLframebuffer *readBuffer )
{
}

//---------------------------------------------------------------------------

static void GLAPIENTRY _gldDrawElements(GLenum mode, GLsizei count, GLenum type,
			     const GLvoid *indices)
{
	GET_CURRENT_CONTEXT(ctx);
	GLint i;
	
	if (!_mesa_validate_DrawElements( ctx, mode, count, type, indices ))
		return;
	
	glBegin(mode);
	
	switch (type) {
	case GL_UNSIGNED_BYTE:
		for (i = 0 ; i < count ; i++)
			glArrayElement( ((GLubyte *)indices)[i] );
		break;
	case GL_UNSIGNED_SHORT:
		for (i = 0 ; i < count ; i++)
			glArrayElement( ((GLushort *)indices)[i] );
		break;
	case GL_UNSIGNED_INT:
		for (i = 0 ; i < count ; i++)
			glArrayElement( ((GLuint *)indices)[i] );
		break;
	default:
		_mesa_error( ctx, GL_INVALID_ENUM, "glDrawElements(type)" );
		break;
	}
	
	glEnd();
}

//---------------------------------------------------------------------------
// Create/Destroy
//---------------------------------------------------------------------------

void gldDestroyD3DTnl(GLcontext *ctx)
{
	GLD_context		*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9	*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	if (gld->vfExec) {
		free(gld->vfExec);
		gld->vfExec = NULL;
	}

   _ae_destroy_context( ctx );
}

//---------------------------------------------------------------------------

BOOL gldInstallD3DTnl(GLcontext *ctx)
{
	GLD_context				*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9			*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLvertexformat			*vf;
	int						i;

	// Initialize the arrayelt helper
	if (!_ae_create_context( ctx ))
		return FALSE;

	// Create our own vertexformat struct
	// NOTE: CALLOC sets all function pointers to NULL
	vf = gld->vfExec = (GLvertexformat*)CALLOC(sizeof(GLvertexformat));
	if (vf == NULL)
		return FALSE;

	// The following commented functions are filled in by _mesa_noop_vtxfmt_init()
/*
	// Plug in our own Execute function pointers
	vf->ArrayElement			= d3dArrayElement;
	vf->Color3f					= d3dColor3f;
	vf->Color3fv				= d3dColor3fv;
	vf->Color4f					= d3dColor4f;
	vf->Color4fv				= d3dColor4fv;
	vf->DrawArrays				= d3dDrawArrays;
	vf->DrawElements			= d3dDrawElements;
	vf->DrawRangeElements		= d3dDrawRangeElements;
	vf->EdgeFlag				= d3dEdgeFlag;
	vf->EdgeFlagv				= d3dEdgeFlagv;
	vf->EvalMesh1				= d3dEvalMesh1;
	vf->EvalMesh2				= d3dEvalMesh2;
	vf->FogCoordfEXT			= d3dFogCoordfEXT;
	vf->FogCoordfvEXT			= d3dFogCoordfvEXT;
	vf->Materialfv				= d3dMaterialfv;
	vf->MultiTexCoord1fARB		= d3dMultiTexCoord1fARB;
	vf->MultiTexCoord1fvARB		= d3dMultiTexCoord1fvARB;
	vf->MultiTexCoord2fARB		= d3dMultiTexCoord2fARB;
	vf->MultiTexCoord2fvARB		= d3dMultiTexCoord2fvARB;
	vf->MultiTexCoord3fARB		= d3dMultiTexCoord3fARB;
	vf->MultiTexCoord3fvARB		= d3dMultiTexCoord3fvARB;
	vf->MultiTexCoord4fARB		= d3dMultiTexCoord4fARB;
	vf->MultiTexCoord4fvARB		= d3dMultiTexCoord4fvARB;
	vf->Normal3f				= d3dNormal3f;
	vf->Normal3fv				= d3dNormal3fv;
	vf->Rectf					= d3dRectf;
	vf->SecondaryColor3fEXT		= d3dSecondaryColor3fEXT;
	vf->SecondaryColor3fvEXT	= d3dSecondaryColor3fvEXT;
	vf->TexCoord1f				= d3dTexCoord1f;
	vf->TexCoord1fv				= d3dTexCoord1fv;
	vf->TexCoord2f				= d3dTexCoord2f;
	vf->TexCoord2fv				= d3dTexCoord2fv;
	vf->TexCoord3f				= d3dTexCoord3f;
	vf->TexCoord3fv				= d3dTexCoord3fv;
	vf->TexCoord4f				= d3dTexCoord4f;
	vf->TexCoord4fv				= d3dTexCoord4fv;
*/
	// Let Mesa handle fill in default callbacks
	_mesa_noop_vtxfmt_init(vf);

	//
	// Plug in our functions
	//

	// Begin/End (primitive assembly)
	vf->Begin					= d3dBegin;
	vf->End						= d3dEnd;

	// Evaluators (higher order surfaces, such as Bezier, etc.)
	// Mesa handles EvalMesh1 and EvalMesh2...
	vf->EvalCoord1f				= d3dEvalCoord1f;
	vf->EvalCoord1fv			= d3dEvalCoord1fv;
	vf->EvalCoord2f				= d3dEvalCoord2f;
	vf->EvalCoord2fv			= d3dEvalCoord2fv;
	vf->EvalPoint1				= d3dEvalPoint1;
	vf->EvalPoint2				= d3dEvalPoint2;

	// Vertex*: when called, a vertex is emitted.
	vf->Vertex2f				= d3dVertex2f;
	vf->Vertex2fv				= d3dVertex2fv;
	vf->Vertex3f				= d3dVertex3f;
	vf->Vertex3fv				= d3dVertex3fv;
	vf->Vertex4f				= d3dVertex4f;
	vf->Vertex4fv				= d3dVertex4fv;

	// Compiled display lists call these functions
	vf->VertexAttrib1fNV		= d3dVertexAttrib1fNV;
	vf->VertexAttrib1fvNV		= d3dVertexAttrib1fvNV;
	vf->VertexAttrib2fNV		= d3dVertexAttrib2fNV;
	vf->VertexAttrib2fvNV		= d3dVertexAttrib2fvNV;
	vf->VertexAttrib3fNV		= d3dVertexAttrib3fNV;
	vf->VertexAttrib3fvNV		= d3dVertexAttrib3fvNV;
	vf->VertexAttrib4fNV		= d3dVertexAttrib4fNV;
	vf->VertexAttrib4fvNV		= d3dVertexAttrib4fvNV;

#if 0
	// For debugging
	vf->DrawElements			= _gldDrawElements;
#endif

	// Install our own vertexformat
	_mesa_install_exec_vtxfmt(ctx, vf);

	//
	// Install stub functions. Use Mesa display list functionality.
	//
	vf->CallList				= _mesa_CallList;
	vf->CallLists				= _mesa_CallLists;

	// Plug in our optimised display list support
	_gld_install_save_vtxfmt(ctx);

	// Install our tnl function pointers
	ctx->Driver.FlushVertices			= d3dFlushVertices;
	ctx->Driver.MakeCurrent				= d3dMakeCurrent;
	ctx->Driver.NeedFlush				= FLUSH_UPDATE_CURRENT;
	ctx->Driver.CurrentExecPrimitive	= PRIM_OUTSIDE_BEGIN_END;
	ctx->Driver.CurrentSavePrimitive	= PRIM_OUTSIDE_BEGIN_END;

	//gld->GLPrim							= PRIM_UNKNOWN;
	gld->GLReducedPrim					= PRIM_UNKNOWN;

	gld->nEffects		= 0;
	gld->iCurEffect		= -1; // No effect current
	gld->iLastEffect	= -1; // No effect current

	gld->fViewportY		= 0.0f;

	// Default Direction for directional lights
	for (i=0; i<GLD_MAX_LIGHTS_DX9; i++) {
		gld->LightDir[i].x = 0.0f;
		gld->LightDir[i].y = 0.0f;
		gld->LightDir[i].z = 1.0f;
		gld->LightDir[i].x = 0.0f;
	}

	// Create en effect pool to allow vars to be shared between effects
	D3DXCreateEffectPool(&gld->pEffectPool);

	// Update the runtime shader generator
	gldUpdateShaders(ctx);

	// Set some state
	_GLD_DX9_DEV(SetStreamSource(gld->pDev, 0, gld->pVB, 0, GLD_4D_VERTEX_SIZE));
	_GLD_DX9_DEV(SetVertexDeclaration(gld->pDev, gld->pVertDecl));

    _GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_CLIPPING, TRUE));
	_GLD_DX9_DEV(SetSoftwareVertexProcessing(gld->pDev, !gld->bHasHWTnL));
	_GLD_DX9_DEV(SetRenderState(gld->pDev, D3DRS_LIGHTING, FALSE));

	return TRUE;
}

//---------------------------------------------------------------------------
