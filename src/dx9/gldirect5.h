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
* Description:  GLDirect Direct3D 9.0 header file
*
*********************************************************************************/

#ifndef _GLD_DX9_H
#define _GLD_DX9_H

//---------------------------------------------------------------------------
// Includes
//---------------------------------------------------------------------------

#ifdef _DEBUG
#include "crtdbg.h"
#define D3D_DEBUG_INFO
#endif

#include <d3d9.h>	// Core Direct3D
#include <d3dx9.h>	// Direct3D Extension library

//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------

// Helper macros for ID3DXEffect
#define ID3DXEffect_Begin(a,b,c)				(a)->lpVtbl->Begin((a), (b), (c))
#define ID3DXEffect_BeginPass(a,b)				(a)->lpVtbl->BeginPass((a), (b))
#define ID3DXEffect_EndPass(a)					(a)->lpVtbl->EndPass((a))
#define ID3DXEffect_CommitChanges(a)			(a)->lpVtbl->CommitChanges((a))
#define ID3DXEffect_End(a)						(a)->lpVtbl->End((a))
#define ID3DXEffect_SetVector(a,b,c)			(a)->lpVtbl->SetVector((a), (b), (c))
#define ID3DXEffect_SetFloat(a,b,c)				(a)->lpVtbl->SetFloat((a), (b), (c))
#define ID3DXEffect_SetMatrix(a,b,c)			(a)->lpVtbl->SetMatrix((a), (b), (c))
#define ID3DXEffect_SetTexture(a,b,c)			(a)->lpVtbl->SetTexture((a), (b), (c))
#define ID3DXEffect_SetTechnique(a,b)			(a)->lpVtbl->SetTechnique((a), (b))
#define ID3DXEffect_SetValue(a,b,c,d)			(a)->lpVtbl->SetValue((a), (b), (c), (d))
#define ID3DXEffect_GetParameterByName(a,b,c)	(a)->lpVtbl->GetParameterByName((a), (b), (c))
#define ID3DXEffect_GetParameterElement(a,b,c)	(a)->lpVtbl->GetParameterElement((a), (b), (c))
#define ID3DXEffect_GetTechniqueByName(a,b)		(a)->lpVtbl->GetTechniqueByName((a), (b))
#define ID3DXEffect_OnLostDevice(a)				(a)->lpVtbl->OnLostDevice((a))
#define ID3DXEffect_OnResetDevice(a)			(a)->lpVtbl->OnResetDevice((a))

//---------------------------------------------------------------------------

// Typedef for obtaining function from d3d9.dll
typedef IDirect3D9* (WINAPI *FNDIRECT3DCREATE9) (UINT);

//---------------------------------------------------------------------------

#ifdef _DEBUG
#define _GLD_TEST_HRESULT(h)					\
{												\
	HRESULT _hr = (h);							\
	if (FAILED(_hr)) {							\
		gldLogError(GLDLOG_ERROR, #h, _hr);		\
	}											\
}
#define _GLD_DX9(func)		_GLD_TEST_HRESULT(IDirect3D9_##func##)
#define _GLD_DX9_DEV(func)	_GLD_TEST_HRESULT(IDirect3DDevice9_##func##)
#define _GLD_DX9_VB(func)	_GLD_TEST_HRESULT(IDirect3DVertexBuffer9_##func##)
#define _GLD_DX9_TEX(func)	_GLD_TEST_HRESULT(IDirect3DTexture9_##func##)
#else
#define _GLD_DX9(func)		IDirect3D9_##func
#define _GLD_DX9_DEV(func)	IDirect3DDevice9_##func
#define _GLD_DX9_VB(func)	IDirect3DVertexBuffer9_##func
#define _GLD_DX9_TEX(func)	IDirect3DTexture9_##func
#endif

//---------------------------------------------------------------------------

#define SAFE_RELEASE(p)				\
{									\
	if (p) {						\
		(p)->lpVtbl->Release(p);	\
		(p) = NULL;					\
	}								\
}

#ifndef SAFE_FREE
#define SAFE_FREE(p)				\
{									\
	if (p) {						\
		free(p);					\
		(p) = NULL;					\
	}								\
}
#endif

//---------------------------------------------------------------------------

#define GLD_GET_DX9_DRIVER(c)	(GLD_driver_dx9*)(c)->glPriv
#define GLD_GET_DL_TNL(c)		(&(c)->dltnl)

//---------------------------------------------------------------------------
// Vertex definitions for Fixed-Function pipeline
//---------------------------------------------------------------------------

//
// NOTE: If the number of texture units is increased then most of
//       the texture code will need to be revised.
//

#define GLD_MAX_TEXTURE_UNITS_DX9	2
#define GLD_MAX_LIGHTS_DX9			8	// Same as Mesa; watch for bugs if this changes.

//
// 4D homogenous vertex transformed by Direct3D
//

// DX9 Vertex Declaration
// A more explicit method of describing the contents of GLD_4D_VERTEX than using an FVF.
// {Stream, Offset, Type, Method, Usage, UsageIndex}
static D3DVERTEXELEMENT9 GLD_vertDecl[] = {
	{0, 0,	D3DDECLTYPE_FLOAT4,		D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0},
	{0, 16,	D3DDECLTYPE_FLOAT3,		D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,		0},
	{0, 28,	D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0},
	{0, 32,	D3DDECLTYPE_FLOAT4,		D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	0},
	{0, 48,	D3DDECLTYPE_FLOAT4,		D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	1},
	D3DDECL_END()
};

//#define GLD_FVF (D3DFVF_XYZW | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX2)

typedef struct {
	D3DXVECTOR4		Position;	// XYZW Vector in object space
	D3DXVECTOR3		Normal;		// XYZ Normal in object space
	D3DCOLOR		Diffuse;	// Diffuse colour
	D3DXVECTOR4		Tex0;		// First texture unit
	D3DXVECTOR4		Tex1;		// Second texture unit
} GLD_4D_VERTEX;

#define GLD_4D_VERTEX_SIZE (sizeof(GLD_4D_VERTEX))

//---------------------------------------------------------------------------
// Effects (Vertex Shaders and Pixel Shaders)
//---------------------------------------------------------------------------

// ** HLSL structs must not be padded **
#pragma pack(push, 1)

//---------------------------------------------------------------------------

//
// Colours derived from material and light. Pre-calculated to reduce work in vertex shader.
//
//	NOTE: Derived values are preceeded with an undescore, as per Mesa source.
//
typedef struct {
	D3DXVECTOR4		Ambient;
	D3DXVECTOR4		Diffuse;
	D3DXVECTOR4		Specular;
	// Terms derived from material and light colours
	D3DXVECTOR4		_Ambient;
	D3DXVECTOR4		_Diffuse;
	D3DXVECTOR4		_Specular;
} GLD_HLSL_lightmat;

//---------------------------------------------------------------------------

typedef struct {
	GLD_HLSL_lightmat	Front;
	GLD_HLSL_lightmat	Back;
	D3DXVECTOR4			Position;
	D3DXVECTOR4			Direction;
	D3DXVECTOR4			Attenuation;	// Constant/Linear/Quadratic in .xyz
	D3DXVECTOR4			SpotLight;		// cos(spot cuttoff angle) in .x, spot power in .y, 
} GLD_HLSL_light;

//---------------------------------------------------------------------------

// ** Restore packing **
#pragma pack(pop)

//---------------------------------------------------------------------------

typedef struct {
	// Sampler
	GLenum					MinFilter;		// Minification & Mipmap filters
	GLenum					MagFilter;		// Magnification filter
	GLenum					WrapS;			// GL_CLAMP or GL_REPEAT
	GLenum					WrapT;			// GL_CLAMP or GL_REPEAT

	// TexEnv
	GLenum					EnvMode;		// GL_REPLACE, GL_MODULATE, GL_DECAL, GL_BLEND and GL_ADD

	// TexGen
	GLuint					TexGenEnabled;	// Bitwise-OR of [STRQ]_BIT values

	// Texgen bitflags: GL_OBJECT_LINEAR, GL_EYE_LINEAR or GL_SPHERE_MAP
	GLuint					_GenBitS;
	GLuint					_GenBitT;
	GLuint					_GenBitR;
	GLuint					_GenBitQ;
	GLuint					_GenFlags;		/**< bitwise or of GenBit[STRQ] */
} GLD_effect_texunit;

//---------------------------------------------------------------------------

typedef struct {
	GLuint					_EnabledUnits;		// one bit set for each really-enabled unit
	GLuint					_GenFlags;			/**< for texgen */
	GLuint					_TexGenEnabled;		// Bitwise-OR of texgen in all units
	GLuint					_TexMatEnabled;		// Texture matrix enabled bits
	GLD_effect_texunit		Unit[2];
} GLD_effect_texture;

//---------------------------------------------------------------------------

typedef struct {
	GLboolean				Enabled;		/**< Fog enabled flag */
	GLenum					Mode;			/**< Fog mode */
} GLD_effect_fog;

//---------------------------------------------------------------------------

typedef struct {
	GLboolean				Enabled;		/**< On/off flag */
	GLuint					_Flags;			/**< State */
} GLD_effect_light;

//---------------------------------------------------------------------------

typedef struct {
	GLboolean				Enabled;		/**< On/off flag */
	GLenum					Face;			// GL_FRONT, GL_BACK, GL_FRONT_AND_BACK
	GLenum					Mode;			// GL_AMBIENT, GL_DIFFUSE, GL_SPECULAR, GL_EMISSION, GL_AMBIENT_AND_DIFFUSE
} GLD_effect_colormat;

//---------------------------------------------------------------------------

typedef struct {
	GLboolean				Enabled;
	GLboolean				TwoSide;		/**< Two (or one) sided lighting? */
	GLD_effect_colormat		ColorMaterial;
	GLD_effect_light		Light[GLD_MAX_LIGHTS_DX9];
} GLD_effect_lightstate;

//---------------------------------------------------------------------------

// The unique set of state handled by the effect
typedef struct {
	GLD_effect_texture		Texture;
	GLD_effect_lightstate	Light;
	GLD_effect_fog			Fog;
} GLD_effect_state;

//---------------------------------------------------------------------------

//
// This struct holds handles (DWORDS) corresponding to string names of HLSL variables.
// Handles are queried once only when the text of the HLSL effect is compiled.
// Handles are more efficient than updating variables by specifying their ascii name.
//

typedef struct {
	// Transformation Matrices
	D3DXHANDLE	matWorldViewProject;						// WorldViewProjection matrix
	D3DXHANDLE	matWorldView;								// WorldView matrix
	D3DXHANDLE	matInvWorldView;							// Inverse WorldView matrix

	// Fog
	D3DXHANDLE	Fog;										// Fog: Start, End, Density, 1 in .xyzw

	// Lighting
	D3DXHANDLE	Lights[GLD_MAX_LIGHTS_DX9];					// Light structs
	// Global Ambient term
	D3DXHANDLE	Ambient;
	// Front material
	D3DXHANDLE	mtlFrontEmissive;
	D3DXHANDLE	mtlFrontAmbient;
	D3DXHANDLE	mtlFrontDiffuse;
	D3DXHANDLE	mtlFrontSpecular;
	D3DXHANDLE	mtlFrontShininess;
	// Back material
	D3DXHANDLE	mtlBackEmissive;
	D3DXHANDLE	mtlBackAmbient;
	D3DXHANDLE	mtlBackDiffuse;
	D3DXHANDLE	mtlBackSpecular;
	D3DXHANDLE	mtlBackShininess;

	// Texture
	D3DXHANDLE	texDiffuse[GLD_MAX_TEXTURE_UNITS_DX9];		// Diffuse Texture
	D3DXHANDLE	matTexture[GLD_MAX_TEXTURE_UNITS_DX9];		// Texture matrices
	D3DXHANDLE	matInvTexture[GLD_MAX_TEXTURE_UNITS_DX9];	// Inverse texture matrices
	D3DXHANDLE	EnvColor[GLD_MAX_TEXTURE_UNITS_DX9];		// Colour for GL_BLEND
	D3DXHANDLE	TexPlaneS[GLD_MAX_TEXTURE_UNITS_DX9];		// Shared between EyePlane and ObjectPlane
	D3DXHANDLE	TexPlaneT[GLD_MAX_TEXTURE_UNITS_DX9];
	D3DXHANDLE	TexPlaneR[GLD_MAX_TEXTURE_UNITS_DX9];
	D3DXHANDLE	TexPlaneQ[GLD_MAX_TEXTURE_UNITS_DX9];
} GLD_handles;

//---------------------------------------------------------------------------

typedef struct {
	GLD_effect_state	State;
	ID3DXEffect			*pEffect;				// The compiled Effect, ready for Direct3D to use
	D3DXHANDLE			hTechnique;				// Technique handle
	GLD_handles			Handles;			
} GLD_effect;

//---------------------------------------------------------------------------
// Display lists
//---------------------------------------------------------------------------

// IDirect3DDevice9::SetStreamSource:
typedef struct {
	IDirect3DDevice9			*pDevice;
	UINT						StreamNumber;
	IDirect3DVertexBuffer9		*pVB;
	UINT						OffsetInBytes;
	UINT						Stride;
} GLD_data_SetStreamSource;

//---------------------------------------------------------------------------

typedef struct {
	// Opcodes. These are what we insert into the display list along with Mesa's opcodes.
	int							opSetStreamSource;	// Set a Direct3D VB (Vertex Buffer) as "current"
	int							opDrawPrimitive;	// Draw primitives using current D3D VB.
	int							opEvalCoord;		// Emit EvalCoord1f or EvalCoord2f

	// We'll keep a pointer to the previous SetStreamSource node so we can fill it in
	// when we know how many vertices will be in the D3D Vertex Buffer.
	GLD_data_SetStreamSource	*pSetStreamSource;

	// Data for current stream in Exec mode
	GLD_data_SetStreamSource	CurrentStream;

	// Current save primitive
	//GLenum						GLPrim;				// Current GL primitive type
	GLenum						GLReducedPrim;		// Current reduced GL primitive type (Points, Lines or Triangles)

	// Capacities
	DWORD						dwMaxPrimVerts;		// Capacity of primitive buffer.
	DWORD						dwMaxVBVerts;		// Capacity of Vertex Buffer.

	// Primitive buffer
	GLD_4D_VERTEX				*pPrim;				// primitive buffer
	DWORD						dwPrimVert;			// Index of next free vert in primitive buffer

	// Vertex buffer
	GLD_4D_VERTEX				*pVerts;			// Vertex buffer
	DWORD						dwFirstVBVert;		// First vert of current primitive type
	DWORD						dwNextVBVert;		// Index of next free vert in Vertex Buffer
} GLD_display_list;

//---------------------------------------------------------------------------
// Context struct
//---------------------------------------------------------------------------

// GLDirect DX9 driver data
typedef struct {
	//
	// GLDirect vars
	//
	BOOL						bDoublebuffer;	// Doublebuffer (otherwise single-buffered)
	BOOL						bDepthStencil;	// Depth buffer needed (stencil optional)
	D3DFORMAT					RenderFormat;	// Format of back/front buffer
	D3DFORMAT					DepthFormat;	// Format of depth/stencil
	BOOL						bUseMesaTnL;	// Whether to use Mesa or D3D for TnL
	BOOL						bCanScissor;	// Scissor test - new for DX9

	//
	// Direct3D vars
	//
	D3DCAPS9					d3dCaps9;
	BOOL						bHasHWTnL;				// Device has Hardware Transform/Light?
	IDirect3D9					*pD3D;					// Base Direct3D9 interface
	IDirect3DDevice9			*pDev;					// Direct3D9 Device interface
	D3DXMATRIX					matProjection;			// Projection matrix for D3D TnL
	D3DXMATRIX					matModelView;			// Model/View matrix for D3D TnL
	D3DXMATRIX					matInvModelView;		// Inverse Model/View matrix for D3D TnL
	D3DXMATRIX					matModelViewProject;	// Model/View/Project matrix for D3D TnL
	D3DXMATRIX					matTexture[GLD_MAX_TEXTURE_UNITS_DX9];		// Texture matrix per unit
	D3DXMATRIX					matInvTexture[GLD_MAX_TEXTURE_UNITS_DX9];	// Inverse texture matrix per unit
	IDirect3DVertexDeclaration9	*pVertDecl;				// Vertex declaration for GLD_4D_VERTEX

	// Mesa Vertex Formats for Exec mode and Save mode.
	GLvertexformat				*vfExec;		// exec vertex format (for Mesa)
	GLvertexformat				*vfSave;		// save format (for Mesa display lists)

	//
	// Build up data between glBegin() and glEnd() into the primitive buffer.
	// On glEnd(), copy primitive into Vertex Buffer; if no room in VB then flush VB first.
	//

//	GLenum						GLPrim;			// Current GL primitive type
	GLenum						GLReducedPrim;	// Current reduced GL primitive type (Points, Lines or Triangles)

	DWORD						dwMaxVBVerts;	// Capacity of Vertex Buffer.
	IDirect3DVertexBuffer9		*pVB;			// Holds points, lines, tris and quads for rendering.
	DWORD						dwFirstVBVert;	// Index of first vert in Vertex Buffer
	DWORD						dwNextVBVert;	// Index of next free vert in Vertex Buffer

	DWORD						dwMaxPrimVerts;	// Capacity of primitive buffer.
	GLD_4D_VERTEX				*pPrim;			// primitive buffer
	DWORD						dwPrimVert;		// Index of next free vert in primitive buffer

	//
	// Display list support
	GLD_display_list			DList;			// Data for current Display List 

	//
	// Run-time shader generation
	//
	ID3DXEffectPool				*pEffectPool;	// This allows parameters to be shared between effects
	int							iLastEffect;	// Index of previous effect (or -1)
	int							iCurEffect;		// Index of current effect (or -1)
	int							nEffects;		// Count of current effects
	GLD_effect					Effects[100];	// TODO: Use linked list

	// Keep track of direction of directional lights.
	D3DXVECTOR4					LightDir[GLD_MAX_LIGHTS_DX9];

	// Viewport adjustment hack
	float						fViewportY;
} GLD_driver_dx9;

#define GLD_GET_DX9_DRIVER(c) (GLD_driver_dx9*)(c)->glPriv

// If we run out of space in the primitive buffer (pPrim) then enlarge it by this amount.
#define GLD_PRIM_BLOCK_SIZE		2000 // Number of vertices to add to primitive buffer

//---------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------

#ifdef  __cplusplus
extern "C" {
#endif

PROC							gldGetProcAddress_DX9(LPCSTR a);
void							gldEnableExtensions_DX9(GLcontext *ctx);
void							gldSetupDriverPointers_DX9(GLcontext *ctx);
void							gldResizeBuffers_DX9(GLframebuffer *fb);

// Functions to hook TnL
void							gldDestroyD3DTnl(GLcontext *ctx);
BOOL							gldInstallD3DTnl(GLcontext *ctx);

// Copy Texture functions
void							gldCopyTexImage1D_DX9(GLcontext *ctx, GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
void							gldCopyTexImage2D_DX9(GLcontext *ctx, GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void							gldCopyTexSubImage1D_DX9(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width );
void							gldCopyTexSubImage2D_DX9(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height );
void							gldCopyTexSubImage3D_DX9(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height );

// Texture functions
void							gld_NEW_TEXTURE_DX9(GLcontext *ctx);
void							gld_DrawPixels_DX9(GLcontext *ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const struct gl_pixelstore_attrib *unpack, const GLvoid *pixels);
void							gld_ReadPixels_DX9(GLcontext *ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const struct gl_pixelstore_attrib *unpack, GLvoid *dest);
void							gld_CopyPixels_DX9(GLcontext *ctx, GLint srcx, GLint srcy, GLsizei width, GLsizei height, GLint dstx, GLint dsty, GLenum type);
void							gld_Bitmap_DX9(GLcontext *ctx, GLint x, GLint y, GLsizei width, GLsizei height, const struct gl_pixelstore_attrib *unpack, const GLubyte *bitmap);
const struct gl_texture_format*	gld_ChooseTextureFormat_DX9(GLcontext *ctx, GLint internalFormat, GLenum srcFormat, GLenum srcType);
void							gld_TexImage2D_DX9(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLint width, GLint height, GLint border, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *tObj, struct gl_texture_image *texImage);
void							gld_TexImage1D_DX9(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLint width, GLint border, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage );
void							gld_TexSubImage2D_DX9( GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage );
void							gld_TexSubImage1D_DX9(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels, const struct gl_pixelstore_attrib *packing, struct gl_texture_object *texObj, struct gl_texture_image *texImage);
void							gld_DeleteTexture_DX9(GLcontext *ctx, struct gl_texture_object *tObj);
void							gld_ResetLineStipple_DX9(GLcontext *ctx);

void							gldResetPrimitiveBuffer(GLD_driver_dx9 *gld);
GLenum							gldReducedPrim(GLenum mode);

// Display List support
BOOL							_gld_install_save_vtxfmt(GLcontext *ctx);

// Run-time shader generation
void							gldUpdateShaders(GLcontext *ctx);
void							gldReleaseShaders(GLD_driver_dx9 *gld);
void							gldBeginEffect(GLD_driver_dx9 *gld, int iEffect);
void							gldEndEffect(GLD_driver_dx9 *gld, int iEffect);

D3DCOLOR						gldClampedColour(GLfloat *c);

#ifdef  __cplusplus
}
#endif

#endif