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
* Description:  Display List support
*
*********************************************************************************/

#include <malloc.h> // For _msize()

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
#include "m_eval.h" // Evaluator functions

void _mesa_update_state( GLcontext *ctx );
void mesa_print_display_list( GLuint list );
void _mesa_noop_vtxfmt_init( GLvertexformat *vfmt );

void gldSaveFlushVertices(GLcontext *ctx);

//---------------------------------------------------------------------------
// Display List defines
//---------------------------------------------------------------------------

/**
 * Flush vertices.
 *
 * \param ctx GL context.
 *
 * Checks if dd_function_table::SaveNeedFlush is marked to flush
 * stored (save) vertices, and calls
 * dd_function_table::SaveFlushVertices if so.
 */
#define SAVE_FLUSH_VERTICES(ctx)		\
do {						\
   if (ctx->Driver.SaveNeedFlush)		\
      ctx->Driver.SaveFlushVertices(ctx);	\
} while (0)

//---------------------------------------------------------------------------
// Display List Opcode: EvalCoord (1f/2f)
//---------------------------------------------------------------------------

typedef struct {
	int		dims;	// 1 or 2: Maps to EvalCoord1f/EvalCoord2f
	float	u;		// U for EvalCoord1f/EvalCoord2f
	float	v;		// V for EvalCoord2f
} GLD_data_EvalCoord;

//---------------------------------------------------------------------------

void gldEvalCoord_Execute(
	GLcontext *ctx,
	void *data)
{
	GLD_data_EvalCoord *pEval = (GLD_data_EvalCoord *)data;

	// Sanity tests in Debug build
	ASSERT(pEval);
	ASSERT((pEval->dims==1) || (pEval->dims==2));

	if (pEval->dims == 2)
		ctx->Exec->EvalCoord2f(pEval->u, pEval->v);

	if (pEval->dims == 1)
		ctx->Exec->EvalCoord1f(pEval->u);
}

//---------------------------------------------------------------------------

void gldEvalCoord_Destroy(
	GLcontext *ctx,
	void *data)
{
	// Nothing to do...
}

//---------------------------------------------------------------------------

void gldEvalCoord_Print(
	GLcontext *ctx,
	void *data)
{
	GLD_data_EvalCoord *pEval = (GLD_data_EvalCoord *)data;
	_mesa_printf("EvalCoord%df u=%f v=%f\n", pEval->dims, pEval->u, pEval->v);
}

//---------------------------------------------------------------------------
// Display List Opcode: SetStreamSource
//---------------------------------------------------------------------------

void gldSetStreamSource_Execute(
	GLcontext *ctx,
	void *data)
{
	GLD_context					*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9				*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list			*dl		= &gld->DList;
	GLD_data_SetStreamSource	*pStream = (GLD_data_SetStreamSource *)data;

	// Execute the function
//	if (pStream->pDevice && pStream->pVB)
//		IDirect3DDevice9_SetStreamSource(pStream->pDevice, pStream->StreamNumber, pStream->pVB, pStream->OffsetInBytes, pStream->Stride);
	dl->CurrentStream = *pStream;
}

//---------------------------------------------------------------------------

void gldSetStreamSource_Destroy(
	GLcontext *ctx,
	void *data)
{
	GLD_data_SetStreamSource *pStream = (GLD_data_SetStreamSource *)data;

	// Release the D3D Vertex Buffer
	SAFE_RELEASE(pStream->pVB);
}

//---------------------------------------------------------------------------

void gldSetStreamSource_Print(
	GLcontext *ctx,
	void *data)
{
	GLD_data_SetStreamSource *pStream = (GLD_data_SetStreamSource *)data;

	_mesa_printf("SetStreamSource dev=%x stream=%d VB=%x offset=%d stride=%d\n", pStream->pDevice, pStream->StreamNumber, pStream->pVB, pStream->OffsetInBytes, pStream->Stride);
}

//---------------------------------------------------------------------------
// Display List Opcode: DrawPrimitive
//---------------------------------------------------------------------------

// IDirect3DDevice9::DrawPrimitive:
typedef struct {
	IDirect3DDevice9		*pDevice;
	D3DPRIMITIVETYPE		PrimitiveType;
	UINT					StartVertex;
	UINT					PrimitiveCount;
	UINT					PointSize;
} GLD_data_DrawPrimitive;

//---------------------------------------------------------------------------

void gldDrawPrimitive_Execute(
	GLcontext *ctx,
	void *data)
{
	GLD_context					*gldCtx		= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9				*gld		= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list			*dl			= &gld->DList;
	GLD_data_DrawPrimitive		*pDrawPrim	= (GLD_data_DrawPrimitive *)data;
	GLD_data_SetStreamSource	*pStream	= (GLD_data_SetStreamSource *)&dl->CurrentStream;

	_mesa_update_state(ctx);

	// Ensure the stream is set
	IDirect3DDevice9_SetStreamSource(pStream->pDevice, pStream->StreamNumber, pStream->pVB, pStream->OffsetInBytes, pStream->Stride);

	if (pDrawPrim->PrimitiveType == D3DPT_POINTLIST) {
		IDirect3DDevice9_SetRenderState(pDrawPrim->pDevice, D3DRS_POINTSIZE, pDrawPrim->PointSize);
	}

	// Execute the function
	if (pDrawPrim->pDevice && pDrawPrim->PrimitiveCount) {
		IDirect3DDevice9_DrawPrimitive(pDrawPrim->pDevice, pDrawPrim->PrimitiveType, pDrawPrim->StartVertex, pDrawPrim->PrimitiveCount);
	} else {
		//gldLogMessage(GLDLOG_ERROR, "DrawPrimitive_Execute: Bad device or PrimCount\n");
	}
}

//---------------------------------------------------------------------------

void gldDrawPrimitive_Destroy(
	GLcontext *ctx,
	void *data)
{
	// Nothing to do. Nothing to destroy.
}

//---------------------------------------------------------------------------

void gldDrawPrimitive_Print(
	GLcontext *ctx,
	void *data)
{
	GLD_data_DrawPrimitive *pDrawPrim = (GLD_data_DrawPrimitive *)data;
	char szLine[1024];
	sprintf(szLine, "DrawPrimitive dev=%x type=%d start=%d primcount=%d ptsize=%d\n", pDrawPrim->pDevice, pDrawPrim->PrimitiveType, pDrawPrim->StartVertex, pDrawPrim->PrimitiveCount, pDrawPrim->PointSize);
	_mesa_printf(szLine);
	gldLogMessage(GLDLOG_INFO, szLine);
}

//---------------------------------------------------------------------------
// Utils
//---------------------------------------------------------------------------

static void _gldEnlargeSavePrimitiveBuffer(
	GLcontext *ctx,
	GLD_display_list *dl)
{
	// Ran out of space in the primitive buffer. Enlarge it.
	// Enlarge in chunks of vertices; adding a single vertex at a time is Not Good
	dl->dwMaxPrimVerts += GLD_PRIM_BLOCK_SIZE;
	dl->pPrim = realloc(dl->pPrim, GLD_4D_VERTEX_SIZE * dl->dwMaxPrimVerts);
	ASSERT(dl->pPrim);
#ifdef DEBUG
	// Useful info to know; dump it in Debug builds
	gldLogPrintf(GLDLOG_SYSTEM, "** Primitive Save buffer (list %d) enlarged to %d verts **", ctx->ListState.CurrentListNum, dl->dwMaxPrimVerts);
#endif
}

//---------------------------------------------------------------------------
// Stream Source Utils
//---------------------------------------------------------------------------

void _gldCreateStreamSourceNode(
	GLcontext *ctx,
	GLD_display_list *dl)
{
	GLD_data_SetStreamSource	*node;

	ASSERT(dl->opSetStreamSource);

	node = (GLD_data_SetStreamSource*)_mesa_alloc_instruction(ctx, dl->opSetStreamSource, sizeof(*node));

	ZeroMemory(node, sizeof(*node));

	// Cache the pointer for future use
	dl->pSetStreamSource = node;
}

//---------------------------------------------------------------------------

static void gldFlushStreamSource(
	GLcontext *ctx)
{
	GLD_context					*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9				*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list			*dl		= &gld->DList;

	HRESULT						hr;
	IDirect3DVertexBuffer9		*pVB = NULL;
	DWORD						dwUsage;
	DWORD						dwPool;
	GLD_4D_VERTEX				*LockPointer;	// Pointer to VB memory
	DWORD						dwVBSize;
	GLD_data_SetStreamSource	*n;

	// Test to see if our Vertex Buffer has anything in it
	if (dl->dwNextVBVert == 0)
		return; // Nothing to do

	// 1. Create a D3D Vertex Buffer
	// 2. copy vertices from our VB into it
	// 3. fill in the current SetStreamSource opcode
	// 4. create and insert a new SetStreamSource opcode

	// This is a static buffer.
	// We want the buffer in the best memory (vidmem->AGP->sysmem) and will lock only once.
	dwUsage = D3DUSAGE_WRITEONLY;

	// Add flag if no HW TnL
	if (!gld->bHasHWTnL)
		dwUsage	|= D3DUSAGE_SOFTWAREPROCESSING;

	// Size of buffer, in bytes
	dwVBSize = GLD_4D_VERTEX_SIZE * dl->dwNextVBVert;

	// Hint to D3D as to where to put the buffer
	dwPool = D3DPOOL_MANAGED;

	// Create a D3D buffer
	hr = IDirect3DDevice9_CreateVertexBuffer(
		gld->pDev,
		dwVBSize,
		dwUsage,
		0, // Non-FVF buffer
		dwPool,
		&pVB,
		NULL);
	if (FAILED(hr)) {
		dl->dwNextVBVert = 0;
		dl->dwFirstVBVert = 0;
		return;
	}

	// Lock the entire buffer ready for filling
	_GLD_DX9_VB(Lock(pVB, 0, 0, &LockPointer, 0));

	// Copy our vertices into the D3D buffer
	memcpy(LockPointer, dl->pVerts, dwVBSize);

	// Unlock
	_GLD_DX9_VB(Unlock(pVB));

	// OK, better emit a DrawPrimitive opcode in order to flush current vertices
	gldSaveFlushVertices(ctx);

	// Fill in existing SetStreamSource opcode
	n = dl->pSetStreamSource;
	n->pDevice			= gld->pDev;			// Pointer to D3D device.
	n->StreamNumber		= 0;					// Stream number. Currently always zero.
	n->pVB				= pVB;					// D3D Vertex Buffer pointer
	n->OffsetInBytes	= 0;					// Offset. Currently always zero.
	n->Stride			= GLD_4D_VERTEX_SIZE;	// Stride between each vertex in buffer

	// Create and insert a new SetStreamSource opcode
	_gldCreateStreamSourceNode(ctx, dl);

	// Start afresh with our vertex buffer
	dl->dwFirstVBVert	= 0;
	dl->dwNextVBVert	= 0;
}

//---------------------------------------------------------------------------
// Save Begin/End
//---------------------------------------------------------------------------

static void gldSuperPrimitive(
	GLcontext *ctx,
	GLD_display_list *dl)
{
	// The current primitive is too big to fit into a our vertex buffer in one go.
	// Split the primitive into manageable multiples.

	// TODO: Actually put some real code in this function...
	gldLogMessage(GLDLOG_SYSTEM, "~~ dlist: gldSuperPrimitive! ~~\n");

	// Reset state before returning
	dl->dwFirstVBVert	= dl->dwNextVBVert = 0;
	dl->dwPrimVert	= 0;
	ctx->Driver.SaveNeedFlush = 0;
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_Begin(
	GLenum mode)
{
	GET_CURRENT_CONTEXT(ctx);
	GLD_context			*gldCtx		= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld		= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list	*dl			= &gld->DList;
	GLenum				GLReducedPrim;

	if (ctx->Driver.CurrentSavePrimitive == PRIM_OUTSIDE_BEGIN_END) {
		GLReducedPrim						= gldReducedPrim(mode);
		ctx->Driver.CurrentSavePrimitive	= mode;

		// Test for a change in reduced primitive
		if (GLReducedPrim != dl->GLReducedPrim) {
			// Primitive has changed; flush current primitve
			SAVE_FLUSH_VERTICES(ctx);
			dl->GLReducedPrim	= GLReducedPrim;
		}
	} else 
		_mesa_error( ctx, GL_INVALID_OPERATION, "gld_save_Begin" );
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_End(void)
{
	GET_CURRENT_CONTEXT(ctx);
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list	*dl		= &gld->DList;

	int					nGLPrimitives;	// Number of GL primitives
	int					nD3DPrimitives;	// Number of Direct3D primitives
	int					nD3DVertices;	// Number of vertices that make up nPrimtives
	GLD_4D_VERTEX		*pSrc;			// Source Vertex
	GLD_4D_VERTEX		*pDst;			// First vertex to start filling
	int					j, count;
	GLuint				parity;

	if (ctx->Driver.CurrentSavePrimitive == PRIM_OUTSIDE_BEGIN_END) {
		_mesa_error( ctx, GL_INVALID_OPERATION, "gld_save_End" );
		goto gld_save_End_bail;
	}

	if (dl->dwPrimVert == 0)
		goto gld_save_End_bail; // Nothing to do...

	nGLPrimitives = nD3DPrimitives = nD3DVertices = 0;

	// Calculate primitive count
	switch (ctx->Driver.CurrentSavePrimitive) {
	case GL_POINTS:
		nGLPrimitives	= dl->dwPrimVert;
		nD3DPrimitives	= nGLPrimitives; // One vertex per point
		nD3DVertices	= nD3DPrimitives; // One vertex per point
		count			= nD3DVertices;
		break;
	case GL_LINES:
		nGLPrimitives	= dl->dwPrimVert / 2;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 2; // Two vertices per line
		count			= nD3DVertices;
		break;
	case GL_LINE_LOOP:
		nGLPrimitives	= dl->dwPrimVert;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 2; // Two vertices per line
		count			= nD3DPrimitives;
		break;
	case GL_LINE_STRIP:
		nGLPrimitives	= dl->dwPrimVert - 1;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 2; // Two vertices per line
		count			= nD3DPrimitives;
		break;
	case GL_TRIANGLES:
		nGLPrimitives	= dl->dwPrimVert / 3;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 3; // Three vertices per triangle
		count			= nD3DVertices;
		break;
	case GL_TRIANGLE_STRIP:
		nGLPrimitives	= dl->dwPrimVert - 2;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 3; // Three vertices per triangle
		count			= dl->dwPrimVert;
		break;
	case GL_TRIANGLE_FAN:
		nGLPrimitives	= dl->dwPrimVert - 2;
		nD3DPrimitives	= nGLPrimitives;
		nD3DVertices	= nD3DPrimitives * 3; // Three vertices per triangle
		count			= dl->dwPrimVert;
		break;
	case GL_QUAD_STRIP:
		nGLPrimitives	= (dl->dwPrimVert - 2) / 2;
		nD3DPrimitives	= nGLPrimitives * 2;
		nD3DVertices	= nD3DPrimitives * 3; // Three vertices per triangle, two tris per quad
		count			= dl->dwPrimVert;
		break;
	case GL_QUADS:
		nGLPrimitives	= dl->dwPrimVert / 4;
		nD3DPrimitives	= nGLPrimitives * 2;	// Two tris per quad
		nD3DVertices	= nD3DPrimitives * 3;	// Three vertices per triangle, two tris per quad
		count			= dl->dwPrimVert;
		break;
	case GL_POLYGON:
		nGLPrimitives	= 1;
		nD3DPrimitives	= dl->dwPrimVert - 2;	// Two tris per quad
		nD3DVertices	= nD3DPrimitives * 3;	// Three vertices per triangle
		count			= nD3DPrimitives;
		break;
	default:
		ASSERT(0); // BANG!
	}

	// Test for invalid primitives
	if ((nGLPrimitives <= 0) || (nD3DPrimitives <= 0) || (nD3DVertices <= 0)) {
		// Bail if too few vertices
		goto gld_save_End_bail;
	}

	// Detect Super Primitives
	if (nD3DVertices > dl->dwMaxVBVerts) {
		// Huge primitive - pass off work to another function
		gldSuperPrimitive(ctx, dl);
		goto gld_save_End_bail;
	}

	// Determine whether there's enough room in the VB for the new vertices
	if ((dl->dwNextVBVert + nD3DVertices) >= dl->dwMaxVBVerts) {
		// No room - make some!
		gldFlushStreamSource(ctx);
		// Start at the beginning of the buffer again
		dl->dwFirstVBVert = dl->dwNextVBVert = 0;
	}

	// Pointer to first vertex in primitive
	pSrc = dl->pPrim;

	// Calculate where to start filling
	pDst = &dl->pVerts[dl->dwNextVBVert];

	// Put vertices into VB
	// NOTE: Keep Provoking Vertex in mind! D3D takes flatshaded colour from 1st vertex in primitive
	switch (ctx->Driver.CurrentSavePrimitive) {
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
			pDst[2] = pSrc[j];
			pDst[0] = pSrc[j-2+parity];
			pDst[1] = pSrc[j-1-parity];
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

	// Update count of vertices in VB
	dl->dwNextVBVert += nD3DVertices;

	// Notify a need to flush
	ctx->Driver.SaveNeedFlush |= FLUSH_STORED_VERTICES;

gld_save_End_bail:
	// Prepare for next primitive
	dl->dwPrimVert						= 0;
	ctx->Driver.CurrentSavePrimitive	= PRIM_OUTSIDE_BEGIN_END;
}

//---------------------------------------------------------------------------
// Evaluators
//---------------------------------------------------------------------------
#if 1

//---------------------------------------------------------------------------

static void _gld_save_EvalCoord(
	int dims,
	float u,
	float v)
{
	GET_CURRENT_CONTEXT(ctx);
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list	*dl		= &gld->DList;
	GLD_data_EvalCoord	*n;
	n = (GLD_data_EvalCoord*)_mesa_alloc_instruction(ctx, dl->opEvalCoord, sizeof(*n));
	if (n) {
		n->dims	= dims;
		n->u	= u;
		n->v	= v;
	}
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_EvalCoord2f(
	GLfloat u,
	GLfloat v)
{
	_gld_save_EvalCoord(2, u, v);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_EvalCoord1f(
	GLfloat u)
{
	_gld_save_EvalCoord(1, u, 0.0f);
}

//---------------------------------------------------------------------------
#else
static void _gldEmitEvalVertex(
	GLcontext *ctx,
	GLD_4D_VERTEX *pVin)
{
	//
	// Helper function for evaluators.
	//

	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list	*dl		= &gld->DList;
	GLD_4D_VERTEX		*pV;	// Pointer to vertex in primitive buffer

	// Ensure there's room in the primitive buffer
	if (dl->dwPrimVert >= dl->dwMaxPrimVerts)
		_gldEnlargeSavePrimitiveBuffer(ctx, dl);

	pV = &dl->pPrim[dl->dwPrimVert];

	// Copy vertex
	*pV = *pVin;

	// Advance to next vertex
	dl->dwPrimVert++;
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_EvalCoord1f(
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
	_gldEmitEvalVertex(ctx, &gldV);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_EvalCoord2f(
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
	_gldEmitEvalVertex(ctx, &gldV);
}
#endif
//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_EvalCoord1fv(
	const GLfloat *u)
{
	gld_save_EvalCoord1f(u[0]);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_EvalCoord2fv(
	const GLfloat *u)
{
	gld_save_EvalCoord2f(u[0], u[1]);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_EvalPoint1(
	GLint i)
{
	GET_CURRENT_CONTEXT( ctx );

	GLfloat du = ((ctx->Eval.MapGrid1u2 - ctx->Eval.MapGrid1u1) / (GLfloat) ctx->Eval.MapGrid1un);
	GLfloat u = i * du + ctx->Eval.MapGrid1u1;

	gld_save_EvalCoord1f(u);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_EvalPoint2(
	GLint i,
	GLint j)
{
	GET_CURRENT_CONTEXT( ctx );

	GLfloat du = ((ctx->Eval.MapGrid2u2 - ctx->Eval.MapGrid2u1) / (GLfloat) ctx->Eval.MapGrid2un);
	GLfloat dv = ((ctx->Eval.MapGrid2v2 - ctx->Eval.MapGrid2v1) / (GLfloat) ctx->Eval.MapGrid2vn);
	GLfloat u = i * du + ctx->Eval.MapGrid2u1;
	GLfloat v = j * dv + ctx->Eval.MapGrid2v1;
	
	gld_save_EvalCoord2f(u, v);
}

//---------------------------------------------------------------------------
// Vertices
//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_Vertex4f(
	GLfloat x,
	GLfloat y,
	GLfloat z,
	GLfloat w)
{
	GET_CURRENT_CONTEXT(ctx);
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list	*dl		= &gld->DList;
	GLD_4D_VERTEX		*pV;	// Pointer to vertex in primitive buffer
	GLfloat				*color	= ctx->Current.Attrib[VERT_ATTRIB_COLOR0];
	GLfloat				*normal	= ctx->Current.Attrib[VERT_ATTRIB_NORMAL];
	GLfloat				*tex0	= ctx->Current.Attrib[VERT_ATTRIB_TEX0];
	GLfloat				*tex1	= ctx->Current.Attrib[VERT_ATTRIB_TEX1];

	// Bail if not inside a valid primitive
	if (ctx->Driver.CurrentSavePrimitive == PRIM_OUTSIDE_BEGIN_END)
		return;

	// Ensure there's room in the primitive buffer
	if (dl->dwPrimVert >= dl->dwMaxPrimVerts)
		_gldEnlargeSavePrimitiveBuffer(ctx, dl);

	pV = &dl->pPrim[dl->dwPrimVert];

	// Fill current vertex
	pV->Position.x	= x;
	pV->Position.y	= y;
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
	dl->dwPrimVert++;
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_Vertex2f(
	GLfloat x,
	GLfloat y)
{
	gld_save_Vertex4f(x, y, 0, 1);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_Vertex2fv(
	const GLfloat *v)
{
	gld_save_Vertex4f(v[0], v[1], 0, 1);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_Vertex3fv(
	const GLfloat *v)
{
	gld_save_Vertex4f(v[0], v[1], v[2], 1);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_Vertex3f(
	GLfloat x,
	GLfloat y,
	GLfloat z)
{
	gld_save_Vertex4f(x, y, z, 1);
}

//---------------------------------------------------------------------------

static void GLAPIENTRY gld_save_Vertex4fv(
	const GLfloat *v)
{
	gld_save_Vertex4f(v[0], v[1], v[2], v[3]);
}

//---------------------------------------------------------------------------
// Support for nVidia's VertexAttribNV extension.
//---------------------------------------------------------------------------

static __inline void _gldEmitVertexNV(GLcontext *ctx)
{
	GLfloat *pV = &ctx->Current.Attrib[VERT_ATTRIB_POS][0];
	gld_save_Vertex4f(pV[0], pV[1], pV[2], pV[3]);
}

//---------------------------------------------------------------------------

void GLAPIENTRY gld_save_VertexAttrib1fNV(
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

void GLAPIENTRY gld_save_VertexAttrib1fvNV(
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

void GLAPIENTRY gld_save_VertexAttrib2fNV(
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

void GLAPIENTRY gld_save_VertexAttrib2fvNV(
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

void GLAPIENTRY gld_save_VertexAttrib3fNV(
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

void GLAPIENTRY gld_save_VertexAttrib3fvNV(
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

void GLAPIENTRY gld_save_VertexAttrib4fNV(
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

void GLAPIENTRY gld_save_VertexAttrib4fvNV(
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
//---------------------------------------------------------------------------

static void gldSaveFlushVertices(
	GLcontext *ctx)
{
	GLD_context				*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9			*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list		*dl		= &gld->DList;
	GLD_data_DrawPrimitive	*n;

	D3DPRIMITIVETYPE	d3dpt;
	UINT				nVertices, nPrimitives;
	UINT				PointSize = 0;

	//
	// We don't need to actually send the current vertex buffer to the display list yet.
	// Insert a DrawPrimitive opcode into the display list
	//

	nVertices = dl->dwNextVBVert - dl->dwFirstVBVert;

	// Bail if nothing to do
	if (nVertices <= 0) {
		dl->dwNextVBVert			= dl->dwFirstVBVert;
		ctx->Driver.SaveNeedFlush	= 0;
		return;
	}

	// Determine number of primitives to draw
	switch (dl->GLReducedPrim) {
	case GL_POINTS:
		PointSize	= *((DWORD*)&ctx->Point._Size);
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
		dl->dwNextVBVert			= dl->dwFirstVBVert;
		ctx->Driver.SaveNeedFlush	= 0;
		return; // Invalid primitive type
	default:
		ASSERT(0);
		return;
	}

	// Bail if nothing to do
	if (nPrimitives <= 0) {
		// Not enough vertices to make even a single primitive.
		dl->dwNextVBVert			= dl->dwFirstVBVert;
		ctx->Driver.SaveNeedFlush	= 0;
		return;
	}

	// Create and insert instruction into display list
	n = (GLD_data_DrawPrimitive*)_mesa_alloc_instruction(ctx, dl->opDrawPrimitive, sizeof(*n));
	if (n) {
		n->pDevice			= gld->pDev;
		n->PrimitiveType	= d3dpt;
		n->StartVertex		= dl->dwFirstVBVert;
		n->PrimitiveCount	= nPrimitives;
		n->PointSize		= PointSize;
	}

	dl->dwFirstVBVert			= dl->dwNextVBVert;
	ctx->Driver.SaveNeedFlush	= 0;
}

//---------------------------------------------------------------------------
// GLD5 display list support
//---------------------------------------------------------------------------

static void gldBeginCallList(
	GLcontext *ctx,
	GLuint list)
{
#if 0
#ifdef DEBUG
	//
	// ** This is for debugging purposes! **
	//
	static BOOL bOnceOnly = TRUE;
	if ((list==1) && bOnceOnly) {
		mesa_print_display_list(list);
		bOnceOnly = FALSE;
	}
#endif
#endif
}

//---------------------------------------------------------------------------

static void gldNewList(
	GLcontext *ctx,
	GLuint list,
	GLenum mode)
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list	*dl		= &gld->DList;

	// Create buffer to hold primitives (data between glBegin and glEnd).
	// This buffer will be enlarged as required
	dl->dwMaxPrimVerts		= 0;
	dl->pPrim				= NULL; //malloc(GLD_4D_VERTEX_SIZE * dl->dwMaxPrimVerts);
	dl->dwPrimVert			= 0;

	// Create buffer to hold vertices. Primitives will be expanded (if required) and
	// copied into this buffer. When this buffer is full (or display list is ended)
	// we'll emit an opcode containing a D3D Vertex Buffer.
	dl->dwMaxVBVerts		= 65535;
	dl->pVerts				= malloc(GLD_4D_VERTEX_SIZE * dl->dwMaxVBVerts);
	dl->dwFirstVBVert		= 0;
	dl->dwNextVBVert		= 0;

	// Create and insert a SetStreamSource opcode. We'll keep a pointer to it and fill it in later.
	_gldCreateStreamSourceNode(ctx, dl);

	ctx->Driver.CurrentSavePrimitive	= PRIM_OUTSIDE_BEGIN_END;
	dl->GLReducedPrim					= PRIM_UNKNOWN;
}

//---------------------------------------------------------------------------

static void gldEndList(
	GLcontext *ctx)
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list	*dl		= &gld->DList;

	// Save any outstanding vertices to the display list
	gldFlushStreamSource(ctx);

	// Delete our Vertex Buffer
	SAFE_FREE(dl->pVerts);
	dl->dwMaxVBVerts	= 0;
	dl->dwFirstVBVert	= 0;
	dl->dwNextVBVert	= 0;

	// Delete our Primitive Buffer
	SAFE_FREE(dl->pPrim);
	dl->dwMaxPrimVerts	= 0;
	dl->dwPrimVert		= 0;
}

//---------------------------------------------------------------------------

static void gldEndCallList(
	GLcontext *ctx)
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);

	// Reset the stream source
	_GLD_DX9_DEV(SetStreamSource(gld->pDev, 0, gld->pVB, 0, GLD_4D_VERTEX_SIZE));
}

//---------------------------------------------------------------------------

static GLboolean gldNotifySaveBegin(
	GLcontext *ctx,
	GLenum mode)
{
	return GL_FALSE; // Let Mesa handle data
}

//---------------------------------------------------------------------------

BOOL _gld_install_save_vtxfmt(
	GLcontext *ctx)
{
	GLD_context			*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9		*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_display_list	*dl		= &gld->DList;
	GLvertexformat		*vf;		// Save format
	GLvertexformat		vfMesa;	// Mesa functions

	// Obtain display list opcodes from Mesa
	dl->opSetStreamSource = _mesa_alloc_opcode(ctx, sizeof(GLD_data_SetStreamSource), gldSetStreamSource_Execute, gldSetStreamSource_Destroy, gldSetStreamSource_Print); 
	if (dl->opSetStreamSource == -1)
		return FALSE;
	dl->opDrawPrimitive = _mesa_alloc_opcode(ctx, sizeof(GLD_data_DrawPrimitive), gldDrawPrimitive_Execute, gldDrawPrimitive_Destroy, gldDrawPrimitive_Print); 
	if (dl->opDrawPrimitive == -1)
		return FALSE;
	dl->opEvalCoord = _mesa_alloc_opcode(ctx, sizeof(GLD_data_EvalCoord), gldEvalCoord_Execute, gldEvalCoord_Destroy, gldEvalCoord_Print); 
	if (dl->opEvalCoord == -1)
		return FALSE;

	// Create our own vertexformat struct
	// NOTE: CALLOC sets all function pointers to NULL
	vf = gld->vfSave = (GLvertexformat*)CALLOC(sizeof(GLvertexformat));
	if (vf == NULL)
		return FALSE;

	// Let Mesa handle fill in default callbacks
	// Yes, these callbacks are the default Exec calls!
	_mesa_noop_vtxfmt_init(vf);

	// Get a list of Mesa save functions so we can cherry-pick from them.
	// Most of these functions are declared static, so we can't use them directly...
	_mesa_save_vtxfmt_init(&vfMesa);			// Mesa callback vf entries

	//
	// Plug in our functions
	//

	// Begin/End (primitive assembly)
	vf->Begin					= gld_save_Begin;
	vf->End						= gld_save_End;

	// Evaluators (higher order surfaces, such as Bezier, etc.)
	// Mesa handles EvalMesh1 and EvalMesh2...
	vf->EvalCoord1f				= gld_save_EvalCoord1f;
	vf->EvalCoord1fv			= gld_save_EvalCoord1fv;
	vf->EvalCoord2f				= gld_save_EvalCoord2f;
	vf->EvalCoord2fv			= gld_save_EvalCoord2fv;
	vf->EvalPoint1				= gld_save_EvalPoint1;
	vf->EvalPoint2				= gld_save_EvalPoint2;

	vf->EvalMesh1				= _mesa_save_EvalMesh1;
	vf->EvalMesh2				= _mesa_save_EvalMesh2;

	// Vertex*: when called, a vertex is emitted.
	vf->Vertex2f				= gld_save_Vertex2f;
	vf->Vertex2fv				= gld_save_Vertex2fv;
	vf->Vertex3f				= gld_save_Vertex3f;
	vf->Vertex3fv				= gld_save_Vertex3fv;
	vf->Vertex4f				= gld_save_Vertex4f;
	vf->Vertex4fv				= gld_save_Vertex4fv;

	vf->VertexAttrib1fNV		= gld_save_VertexAttrib1fNV;
	vf->VertexAttrib1fvNV		= gld_save_VertexAttrib1fvNV;
	vf->VertexAttrib2fNV		= gld_save_VertexAttrib2fNV;
	vf->VertexAttrib2fvNV		= gld_save_VertexAttrib2fvNV;
	vf->VertexAttrib3fNV		= gld_save_VertexAttrib3fNV;
	vf->VertexAttrib3fvNV		= gld_save_VertexAttrib3fvNV;
	vf->VertexAttrib4fNV		= gld_save_VertexAttrib4fNV;
	vf->VertexAttrib4fvNV		= gld_save_VertexAttrib4fvNV;

	vf->CallList				= _mesa_save_CallList;
	vf->CallLists				= _mesa_save_CallLists;

	vf->Materialfv				= vfMesa.Materialfv;

	if (glb.bUseMesaDisplayLists) {
		// This will bypass our display list code and use Mesa instead.
		_mesa_save_vtxfmt_init(vf);			// Set Mesa callback vf entries
		// Emit a message for informational purposes
		gldLogMessage(GLDLOG_SYSTEM, "Using default Mesa display list functionality\n");
	}

	_mesa_install_save_vtxfmt(ctx, vf);

	ctx->Driver.NewList				= gldNewList;
	ctx->Driver.EndList				= gldEndList;
	ctx->Driver.BeginCallList		= gldBeginCallList;
	ctx->Driver.EndCallList			= gldEndCallList;
	ctx->Driver.NotifySaveBegin		= gldNotifySaveBegin;
	ctx->Driver.SaveFlushVertices	= gldSaveFlushVertices;

	return TRUE;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
