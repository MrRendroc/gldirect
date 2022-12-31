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
* Description:  Run-time HLSL (High Level Shading Language) Shader Generation
*
*********************************************************************************/

#pragma warning( disable:4996) // secure versions of strcat(), etc

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

const char *_mesa_lookup_enum_by_nr( int nr );

//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------

#define TEXGEN_NEED_TEXPLANE (TEXGEN_OBJ_LINEAR | TEXGEN_EYE_LINEAR)

//---------------------------------------------------------------------------
// GLDirect HLSL File format
//---------------------------------------------------------------------------

// 1. Structs (inputs/outputs/etc.)
// 2. Variables
// 3. Samplers
// 4. Helper functions
// 5. Vertex shader
// 6. Pixel Shader
// 7. Technique (currently must be named "tecGLDirect")

//---------------------------------------------------------------------------
// ** Test Shaders **
//---------------------------------------------------------------------------

static const char *g_pszDefaultEffect =
"struct VS_INPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"};\n"
"\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"};\n"
"shared float4x4 g_matWorldViewProject : WORLDVIEWPROJECTION;\n"
"\n"
"VS_OUTPUT VS(VS_INPUT In)\n"
"{\n"
"    VS_OUTPUT Out = (VS_OUTPUT)0;\n"
"    float4 Diffuse = In.Diff;\n"
"    Out.Pos  = mul(In.Pos, g_matWorldViewProject);\n"
"    Out.Diff = saturate(Diffuse);\n"
"    return Out;\n"
"}\n"
"\n"
"float4 PS(VS_OUTPUT In) : COLOR\n"
"{\n"
"    float4 Color = In.Diff;\n"
"    return Color;\n"
"}\n"
"\n"
"technique tecGLDirect\n"
"{\n"
"    pass Pass0\n"
"    {\n"
"        VertexShader = compile vs_1_1 VS();\n"
"        PixelShader  = compile ps_1_1 PS();\n"
"    }\n"
"}\n";

//---------------------------------------------------------------------------

// Absolute bare minimum Effect for performce testing
static const char *g_pszMinimalEffect =
"struct VS_INPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"};\n"
"\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"};\n"
"shared float4x4 g_matWorldViewProject : WORLDVIEWPROJECTION;\n"
"\n"
"VS_OUTPUT VS(VS_INPUT In)\n"
"{\n"
"    VS_OUTPUT Out = (VS_OUTPUT)0;\n"
"    float4 Diffuse = In.Diff;\n"
"    Out.Pos  = mul(In.Pos, g_matWorldViewProject);\n"
"    Out.Diff = saturate(Diffuse);\n"
"    return Out;\n"
"}\n"
"\n"
"float4 PS(VS_OUTPUT In) : COLOR\n"
"{\n"
"    float4 Color = In.Diff;\n"
"    return Color;\n"
"}\n"
"\n"
"technique tecGLDirect\n"
"{\n"
"    pass Pass0\n"
"    {\n"
"        VertexShader = compile vs_2_0 VS();\n"
"        PixelShader  = compile ps_2_0 PS();\n"
"    }\n"
"}\n";

//---------------------------------------------------------------------------

static const char *g_pszTestEffect =
"float4x4 g_matWorldViewProject : WORLDVIEWPROJECTION;\n"
"\n"
"struct VS_INPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"};\n"
"\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"};\n"
"\n"
"VS_OUTPUT VS(VS_INPUT In)\n"
"{\n"
"    VS_OUTPUT Out = (VS_OUTPUT)0;\n"
"\n"
"    Out.Pos  = mul(In.Pos, matWorldViewProject);             // position (projected)\n"
"    Out.Diff = In.Diff;\n"
"    return Out;\n"
"}\n"
"\n"
"float4 PS(VS_OUTPUT In) : COLOR\n"
"{\n"
"    float4 Color = (float4)0;\n"
"    Color = In.Diff;\n"
"    return Color;\n"
"}\n"
"\n"
"technique tecGLDirect\n"
"{\n"
"    pass P0\n"
"    {\n"
"        VertexShader = compile vs_2_0 VS();\n"
"        PixelShader  = compile ps_2_0 PS();\n"
"    }\n"
"}\n"
"\n"
"//---------------------------------------------------------------------------\n\0"
;

//---------------------------------------------------------------------------

static const char *g_pszTestEffectTextured =
"float4x4 g_matWorldViewProject : WORLDVIEWPROJECTION;\n"
"texture g_texDiffuse0;\n"
"sampler Sampler = sampler_state\n"
"{\n"
"    Texture   = (g_texDiffuse0);\n"
"\n"
"    MipFilter = POINT;\n"
"    MinFilter = LINEAR;\n"
"    MagFilter = LINEAR;\n"
"};\n"
"\n"
"\n"
"struct VS_INPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"    float4 Tex  : TEXCOORD0;\n"
"};\n"
"\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"    float4 Tex  : TEXCOORD0;\n"
"};\n"
"\n"
"VS_OUTPUT VS(VS_INPUT In)\n"
"{\n"
"    VS_OUTPUT Out = (VS_OUTPUT)0;\n"
"\n"
"    Out.Pos  = mul(In.Pos, matWorldViewProject);             // position (projected)\n"
"    Out.Diff = In.Diff;\n"
"    Out.Tex  = In.Tex;\n"
"    return Out;\n"
"}\n"
"\n"
"float4 PS(VS_OUTPUT In) : COLOR\n"
"{\n"
"    float4 Color = (float4)0;\n"
"    Color = tex2D(Sampler, In.Tex.xy);\n"
"    return Color;\n"
"}\n"
"\n"
"technique tecGLDirect\n"
"{\n"
"    pass P0\n"
"    {\n"
"        Sampler[0] = (Sampler);\n"
"        VertexShader = compile vs_2_0 VS();\n"
"        PixelShader  = compile ps_2_0 PS();\n"
"    }\n"
"}\n"
"\n"
"//---------------------------------------------------------------------------\n\0"
;

//---------------------------------------------------------------------------
// Shader Text
//---------------------------------------------------------------------------

static const char *g_pszVertexShaderInput =
"\n"
"struct VS_INPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"    float3 Norm : NORMAL;\n"
"    float4 Tex0 : TEXCOORD0;\n"
"    float4 Tex1 : TEXCOORD1;\n"
"};\n";

//---------------------------------------------------------------------------

static const char *g_pszVertexShaderOutput =
"\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"    float4 Tex0 : TEXCOORD0;\n"
"    float4 Tex1 : TEXCOORD1;\n"
"};\n";

//---------------------------------------------------------------------------

static const char *g_pszVertexShaderOutputFog =
"\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 Pos  : POSITION;\n"
"    float4 Diff : COLOR0;\n"
"    float4 Tex0 : TEXCOORD0;\n"
"    float4 Tex1 : TEXCOORD1;\n"
"    float  Fog  : FOG;\n"
"};\n";

//---------------------------------------------------------------------------

static const char *g_pszGLD_HLSL_light =
"\n"
"struct GLD_HLSL_lightmat\n"
"{\n"
"    float4    Ambient;\n"
"    float4    Diffuse;\n"
"    float4    Specular;\n"
"    float4    _Ambient;\n"
"    float4    _Diffuse;\n"
"    float4    _Specular;\n"
"};\n\n"
"struct GLD_HLSL_light\n"
"{\n"
"    GLD_HLSL_lightmat  Front;\n"
"    GLD_HLSL_lightmat  Back;\n"
"    float4             Position  : LIGHTPOSITION;\n"
"    float4             Direction : LIGHTDIRECTION;\n"
"    float4             Attenuation;\n"
"    float4             SpotLight;\n"
"};\n";
//---------------------------------------------------------------------------

static const char *g_pszSamplerTemplate =
"\n"
"sampler Sampler%d = sampler_state\n"
"{\n"
"    Texture   = (g_texDiffuse%d);\n"
"    MipFilter = %s;\n"
"    MinFilter = %s;\n"
"    MagFilter = %s;\n"
"    AddressU  = %s;\n"
"    AddressV  = %s;\n"
"};\n";

//---------------------------------------------------------------------------

// Full World/View/Projection transformation.
// Needed to calculate output homogenous vertex coordinate.
static const char *g_pszWVPTransform =
"shared float4x4 g_matWorldViewProject : WORLDVIEWPROJECTION;\n";

//---------------------------------------------------------------------------

// Needed when eye-coordinates are required in the vertex shader.
static const char *g_pszTransformEyeCoords =
"shared float4x4 g_matWorldView        : WORLDVIEW;\n";

//---------------------------------------------------------------------------

// Transforms vertex to eye-space for lighting, etc..
static const char *g_pszInverseTransformEyeCoords =
"shared float4x4 g_matInvWorldView     : WORLDVIEW;\n";

//---------------------------------------------------------------------------
// Texgen functions
//---------------------------------------------------------------------------

static const char *g_pszTexGenObject =
"\n"
"float gld_texgen_object(float4 vpos, float4 TexPlane)\n"
"{\n"
"    return dot(vpos, TexPlane);\n"
"}\n";

//---------------------------------------------------------------------------

static const char *g_pszTexGenEye =
"\n"
"float gld_texgen_eye(float4 epos, float4 TexPlane)\n"
"{\n"
"    return dot(epos, TexPlane);\n"
"}\n";

//---------------------------------------------------------------------------

static const char *g_pszTexGenSphere =
"\n"
"float4 gld_texgen_sphere(float4 epos, float3 norm)\n"
"{\n"
"    float3 vdir   = normalize(epos.xyz);\n"
"    float3 ref    = reflect(vdir, norm);\n"
"    float3 em     = ref + float3(0,0,1);\n"
"    em.x          = 2 * sqrt(dot(em, em));\n"
"    float4 tcoord = {0,0,0,1};\n"
"    tcoord.xy     = ref.xy/em.x + 0.5;\n"
"    return tcoord;\n"
"}\n";

//---------------------------------------------------------------------------
// Fog
//---------------------------------------------------------------------------

static const char *g_pszFogLinear =
"    Out.Fog = saturate((g_Fog.y - -EPos.z) / (g_Fog.y - g_Fog.x));\n";

static const char *g_pszFogExponential =
"    Out.Fog = saturate(exp(-(g_Fog.z * -EPos.z)));\n";

static const char *g_pszFogExponentialSquared =
"    float f = g_Fog.z * -EPos.z;\n"
"    Out.Fog = saturate(exp(-(f*f)));\n";

//---------------------------------------------------------------------------
// Lighting functions
//---------------------------------------------------------------------------

// Directional light infinite viewer
static const char *g_pszLightDir =
"\n"
"float4 light_dir(const float3 norm, const GLD_HLSL_light Light, float4 _Ambient, float4 _Diffuse, float4 _Specular, float Shininess)\n"
"{\n"
"    float3 lightDir = normalize(-Light.Direction.xyz);\n"
"    float3 vdir     = {0,0,1};\n"
"    float3 vHalf    = normalize(vdir - lightDir);\n"
"    float4 coeffs   = lit(dot(norm, - lightDir), dot(norm,vHalf), Shininess);\n"
"    return _Ambient + (_Diffuse * coeffs.y) + (_Specular * coeffs.z);\n"
"}\n"
"\n";

//---------------------------------------------------------------------------

// Positional light local viewer
static const char *g_pszLightPoint =
"\n"
"float4 light_point(float3 epos, float3 norm, GLD_HLSL_light Light, float4 _Ambient, float4 _Diffuse, float4 _Specular, float Shininess)\n"
"{\n"
"    float3 vert2light = Light.Position.xyz - epos;\n"
"    float  d          = length(vert2light);\n"
"    float3 ldir       = vert2light / d;\n"
"    float3 vdir       = {0,0,1}; //normalize(-epos);\n"
"    float3 vHalf      = normalize(ldir + vdir);\n"
"    float4 coeffs     = lit(dot(norm,ldir), dot(norm,vHalf), Shininess);\n"
"    float  Atten = 1 / (Light.Attenuation.x + (d*Light.Attenuation.y) + (d*d*Light.Attenuation.z));\n"
"    return Atten * (_Ambient + (_Diffuse * coeffs.y) + (_Specular * coeffs.z));\n"
"}\n";

//---------------------------------------------------------------------------

// Spot light local viewer
static const char *g_pszLightSpot =
"\n"
"float4 light_spot(float3 epos, float3 norm, GLD_HLSL_light Light, float4 _Ambient, float4 _Diffuse, float4 _Specular, float Shininess)\n"
"{\n"
"    float3 vert2light = Light.Position.xyz - epos;\n"
"    float  d          = length(vert2light);\n"
"    float3 ldir       = vert2light / d;\n"
"    float3 vdir       = {0,0,1}; //normalize(-epos);\n"
"    float3 vHalf      = normalize(ldir + vdir);\n"
"    float4 coeffs     = lit(dot(norm,ldir), dot(norm,vHalf), Shininess);\n"
"    float3 lightDir   = normalize(Light.Direction.xyz);\n"
"    float  spotDot    = dot(ldir, -lightDir);\n"
"    float4 spot       = lit(spotDot - Light.SpotLight.x, spotDot, Light.SpotLight.y);\n"
"    float  Atten = 1 / (Light.Attenuation.x + (d*Light.Attenuation.y) + (d*d*Light.Attenuation.z));\n"
"    return Atten * (spot.z * (_Ambient + (_Diffuse * coeffs.y) + (_Specular * coeffs.z)));\n"
"}\n";

//---------------------------------------------------------------------------
// Utils
//---------------------------------------------------------------------------

static void gl4fToVec4(D3DXVECTOR4 *vec, const GLfloat *f)
{
	vec->x = f[0];
	vec->y = f[1];
	vec->z = f[2];
	vec->w = f[3];
}

//---------------------------------------------------------------------------

static void gl3fToVec4(D3DXVECTOR4 *vec, const GLfloat *f)
{
	vec->x = f[0];
	vec->y = f[1];
	vec->z = f[2];
	vec->w = 1.0f;
}

//---------------------------------------------------------------------------

static void _gldGetIndexedEffectString(
	char *pszBuffer,
	const char *pszVariable,
	DWORD dwIndex)
{
	// Concatenate the string with the index of the variable
	sprintf(pszBuffer, "%s%d", pszVariable, dwIndex);
}

//---------------------------------------------------------------------------

static D3DXHANDLE _gldGetIndexedEffectVariable(
	ID3DXEffect *pEffect,
	const char *pszName,
	DWORD dwIndex)
{
	char szIndexedName[512];

	// Concatenate the string with the index of the variable
	_gldGetIndexedEffectString(szIndexedName, pszName, dwIndex);
	// Find the handle for the variable (or NULL if not found)
	return ID3DXEffect_GetParameterByName(pEffect, NULL, szIndexedName);
}

//---------------------------------------------------------------------------

static __inline void _gldSetEffectVector(
	ID3DXEffect *pEffect,
	D3DXHANDLE hParam,
	const GLfloat *p4f)
{
	// Update the shader parameter with the supplied vector
	D3DXVECTOR4 d3dxVec;

	if (!hParam)
		return;

	d3dxVec.x = p4f[0];
	d3dxVec.y = p4f[1];
	d3dxVec.z = p4f[2];
	d3dxVec.w = p4f[3];

	ASSERT(pEffect);

	ID3DXEffect_SetVector(pEffect, hParam, &d3dxVec);
}

//---------------------------------------------------------------------------
// Texture filters
//---------------------------------------------------------------------------

const char *pszNone		= "None";
const char *pszPoint	= "Point";
const char *pszLinear	= "Linear";

//---------------------------------------------------------------------------

static const char* _gldMipFilter(GLenum MipFilter)
{
	switch (MipFilter) {
	case GL_NEAREST:
	case GL_LINEAR:
		return pszNone;
	case GL_NEAREST_MIPMAP_NEAREST:
	case GL_LINEAR_MIPMAP_NEAREST:
		return pszPoint;
	case GL_NEAREST_MIPMAP_LINEAR:
	case GL_LINEAR_MIPMAP_LINEAR:
		return pszLinear;
	default:
		return pszNone;
	}
}

//---------------------------------------------------------------------------

static const char* _gldMinFilter(GLenum MinFilter)
{
	switch (MinFilter) {
	case GL_NEAREST:
	case GL_NEAREST_MIPMAP_NEAREST:
	case GL_NEAREST_MIPMAP_LINEAR:
		return pszPoint;
	case GL_LINEAR:
	case GL_LINEAR_MIPMAP_NEAREST:
	case GL_LINEAR_MIPMAP_LINEAR:
		return pszLinear;
	default:
		return pszNone;
	}
}

//---------------------------------------------------------------------------

static const char* _gldMagFilter(GLenum MagFilter)
{
	switch (MagFilter) {
	case GL_NEAREST:
		return "Point";
	case GL_LINEAR:
		return "Linear";
	default:
		return pszNone;
	}
}

//---------------------------------------------------------------------------

static const char *_gldWrap(GLenum Wrap)
{
#if 0
	// For debugging
	gldLogPrintf(GLDLOG_SYSTEM, "Wrap=%s", _mesa_lookup_enum_by_nr(Wrap));
#endif

	if (glb.bBugdom) {
		return "Wrap";
	}

	switch (Wrap) {
	case GL_CLAMP:
	case GL_CLAMP_TO_EDGE:
		return "Clamp";
	case GL_REPEAT:
		return "Wrap";
	case GL_MIRRORED_REPEAT:
		return "Mirror";
	}

	ASSERT(0); // Bail in Debug builds
	return "Wrap";
}

//---------------------------------------------------------------------------
// Texture Environment
//---------------------------------------------------------------------------

// Broken out for legibility
const char *pszBlendPrintf =
"    Color.rgb = Color.rgb * (1 - tex%d.rgb) + (tex%d.rgb * g_EnvColor%d.rgb); // GL_BLEND\n"
"    Color.a   = Color.a * tex%d.a; // GL_BLEND\n";

//---------------------------------------------------------------------------

static void _gldTexEnvString(
	char *szTexEnv,
	int Unit,
	GLenum EnvMode)
{
	//
	// Convert the GL texture environment enum into HLSL
	//
	switch (EnvMode) {
	case GL_REPLACE:
		sprintf(szTexEnv, "    Color = tex%d; // GL_REPLACE\n", Unit);
		return;
	case GL_ADD:
		sprintf(szTexEnv, "    Color = tex%d + Color; // GL_ADD\n", Unit);
		return;
	case GL_MODULATE:
		sprintf(szTexEnv, "    Color = tex%d * Color; // GL_MODULATE\n", Unit);
		return;
	case GL_DECAL:
		sprintf(szTexEnv, "    Color.rgb = Color * (1-tex%d.a) + (tex%d.rgb * tex%d.a); // GL_DECAL\n", Unit, Unit, Unit);
		return;
	case GL_BLEND:
		sprintf(szTexEnv, pszBlendPrintf, Unit, Unit, Unit, Unit);
		return;
	default:
		ASSERT(0); // Should NOT get here!
	}
}

//---------------------------------------------------------------------------
// TexGen
//---------------------------------------------------------------------------

void _gldTexGenFunctionString(
	char *pszBuf,
	int unit,
	GLuint uiGenMode,
	GLuint uiGenBit)
{
	if (uiGenMode & TEXGEN_SPHERE_MAP) {
		if (uiGenBit & S_BIT) {
			sprintf(pszBuf, "    tex%d.x = spheremap.x;\n", unit);
			return;
		}
		if (uiGenBit & T_BIT) {
			sprintf(pszBuf, "    tex%d.y = spheremap.y;\n", unit);
			return;
		}
		if (uiGenBit & R_BIT) {
			// Spheremap texgen only makes sense for S and T
			sprintf(pszBuf, "    tex%d.z = 0;\n", unit);
			return;
		}
		if (uiGenBit & Q_BIT) {
			// Spheremap texgen only makes sense for S and T
			sprintf(pszBuf, "    tex%d.w = 1;\n", unit);
			return;
		}
	}

	if (uiGenMode & TEXGEN_OBJ_LINEAR) {
		if (uiGenBit & S_BIT) {
			sprintf(pszBuf, "    tex%d.x = gld_texgen_object(In.Pos, g_TexPlaneS%d);\n", unit, unit);
			return;
		}
		if (uiGenBit & T_BIT) {
			sprintf(pszBuf, "    tex%d.y = gld_texgen_object(In.Pos, g_TexPlaneT%d);\n", unit, unit);
			return;
		}
		if (uiGenBit & R_BIT) {
			sprintf(pszBuf, "    tex%d.z = gld_texgen_object(In.Pos, g_TexPlaneR%d);\n", unit, unit);
			return;
		}
		if (uiGenBit & Q_BIT) {
			sprintf(pszBuf, "    tex%d.w = gld_texgen_object(In.Pos, g_TexPlaneQ%d);\n", unit, unit);
			return;
		}
	}

	if (uiGenMode & TEXGEN_EYE_LINEAR) {
		if (uiGenBit & S_BIT) {
			sprintf(pszBuf, "    tex%d.x = gld_texgen_eye(EPos, g_TexPlaneS%d);\n", unit, unit);
			return;
		}
		if (uiGenBit & T_BIT) {
			sprintf(pszBuf, "    tex%d.y = gld_texgen_eye(EPos, g_TexPlaneT%d);\n", unit, unit);
			return;
		}
		if (uiGenBit & R_BIT) {
			sprintf(pszBuf, "    tex%d.z = gld_texgen_eye(EPos, g_TexPlaneR%d);\n", unit, unit);
			return;
		}
		if (uiGenBit & Q_BIT) {
			sprintf(pszBuf, "    tex%d.w = gld_texgen_eye(EPos, g_TexPlaneQ%d);\n", unit, unit);
			return;
		}
	}

	// No texgen - get texcoord from input texcoord
	if (uiGenBit & S_BIT) {
		sprintf(pszBuf, "    tex%d.x = In.Tex%d.x;\n", unit, unit);
		return;
	}
	if (uiGenBit & T_BIT) {
		sprintf(pszBuf, "    tex%d.y = In.Tex%d.y;\n", unit, unit);
		return;
	}
	if (uiGenBit & R_BIT) {
		sprintf(pszBuf, "    tex%d.z = In.Tex%d.z;\n", unit, unit);
		return;
	}
	if (uiGenBit & Q_BIT) {
		sprintf(pszBuf, "    tex%d.w = In.Tex%d.w;\n", unit, unit);
		return;
	}

	ASSERT(0); // Should not get here.
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

static const char *g_pszEnabled		= "Enabled";
static const char *g_pszDisabled	= "Disabled";

#ifdef DEBUG
void _gldOutputDebugShaderText(
	char *pOut,
	const GLD_effect_state *es)
{
	//
	// Output details of the input GLD_effect into the shader text for debugging purposes.
	//

	int		i;
	UINT	uiUnitMask;
	char	szLine[1024];

	// Header
	strcat(pOut, "\n//---------------------------------------------------------------------------\n");

	// Texture
	sprintf(szLine, "// Texture    : %s\n", (es->Texture._EnabledUnits) ? g_pszEnabled : g_pszDisabled);
	strcat(pOut, szLine);
	if (es->Texture._EnabledUnits) {
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if (es->Texture._EnabledUnits & uiUnitMask) {
				const GLD_effect_texunit *pUnit = &es->Texture.Unit[i];
				sprintf(szLine, "//              TexUnit%d\n", i);
				strcat(pOut, szLine);
				sprintf(szLine, "//                           : MinFilter = %s\n", _mesa_lookup_enum_by_nr(pUnit->MinFilter));
				strcat(pOut, szLine);
				sprintf(szLine, "//                           : MagFilter = %s\n", _mesa_lookup_enum_by_nr(pUnit->MagFilter));
				strcat(pOut, szLine);
				sprintf(szLine, "//                           : WrapS     = %s\n", _mesa_lookup_enum_by_nr(pUnit->WrapS));
				strcat(pOut, szLine);
				sprintf(szLine, "//                           : WrapT     = %s\n", _mesa_lookup_enum_by_nr(pUnit->WrapT));
				strcat(pOut, szLine);
				sprintf(szLine, "//                           : EnvMode   = %s\n", _mesa_lookup_enum_by_nr(pUnit->EnvMode));
				strcat(pOut, szLine);
				if (es->Texture._GenFlags) {
					sprintf(szLine, "//                           : _GenBitS  = %x\n", pUnit->_GenBitS);
					strcat(pOut, szLine);
					sprintf(szLine, "//                           : _GenBitT  = %x\n", pUnit->_GenBitT);
					strcat(pOut, szLine);
					sprintf(szLine, "//                           : _GenBitR  = %x\n", pUnit->_GenBitR);
					strcat(pOut, szLine);
					sprintf(szLine, "//                           : _GenBitQ  = %x\n", pUnit->_GenBitQ);
					strcat(pOut, szLine);
				}
			}
		}
	}

	// Light
	sprintf(szLine, "// Light      : %s\n", (es->Light.Enabled) ? g_pszEnabled : g_pszDisabled);
	strcat(pOut, szLine);
	if (es->Light.Enabled) {
		sprintf(szLine, "//              TwoSided     : %s\n", (es->Light.TwoSide) ? g_pszEnabled : g_pszDisabled);
		strcat(pOut, szLine);
		sprintf(szLine, "//              ColorMaterial: %s\n", (es->Light.ColorMaterial.Enabled) ? g_pszEnabled : g_pszDisabled);
		strcat(pOut, szLine);
		if (es->Light.ColorMaterial.Enabled) {
			sprintf(szLine, "//              ColorMaterial: Face = %s\n", _mesa_lookup_enum_by_nr(es->Light.ColorMaterial.Face));
			strcat(pOut, szLine);
			sprintf(szLine, "//              ColorMaterial : Mode = %s\n", _mesa_lookup_enum_by_nr(es->Light.ColorMaterial.Mode));
			strcat(pOut, szLine);
		}
		for (i=0; i<GLD_MAX_LIGHTS_DX9; i++) {
			const GLD_effect_light *pLight = &es->Light.Light[i];
			if (pLight->Enabled) {
				sprintf(szLine, "//              Light%d       : ", i);
				strcat(pOut, szLine);
				if (pLight->_Flags & LIGHT_SPOT) {
					strcat(pOut, "Spotlight");
				} else if (pLight->_Flags & LIGHT_POSITIONAL) {
					strcat(pOut, "Positional");
				} else {
					strcat(pOut, "Directional");
				}
				strcat(pOut, "\n");
			}
		}
	}

	// Fog
	sprintf(szLine, "// Fog        : %s\n", (es->Fog.Enabled) ? g_pszEnabled : g_pszDisabled);
	strcat(pOut, szLine);
	if (es->Fog.Enabled) {
		sprintf(szLine, "//            Fog Mode : %s\n", _mesa_lookup_enum_by_nr(es->Fog.Mode));
		strcat(pOut, szLine);
	}

	// Footer
	strcat(pOut, "//---------------------------------------------------------------------------\n");
}
#endif

//---------------------------------------------------------------------------

static void _gldBuildLightingFace(
	char *pszHLSL,
	const GLD_effect_lightstate *pLS,
	GLenum face)
{
	//
	// Build up the lighting for a given face (Front or Back)
	//

	int							i;
	const GLD_effect_colormat	*cm = &pLS->ColorMaterial;
	const char					*pszMtlFace = (face == GL_FRONT) ? "g_mtlFront" : "g_mtlBack";
	const char					*pszLightFace = (face == GL_FRONT) ? "Front" : "Back";
	BOOL						bColorMaterial;
	BOOL						bColorMaterialAmbient;
	BOOL						bColorMaterialDiffuse;

	char						szLine[1024];
	char						szAmbient[64];
	char						szDiffuse[64];
	char						szSpecular[64];
	char						szNormal[64];

	// Determine whether ColorMaterial is enabled for this face
	if (cm->Enabled && ((cm->Face == face) || (cm->Face == GL_FRONT_AND_BACK)))
		bColorMaterial = TRUE;
	else
		bColorMaterial = FALSE;

	// Ambient source
	if (bColorMaterial && ((cm->Mode == GL_AMBIENT) || (cm->Mode == GL_AMBIENT_AND_DIFFUSE))) {
		strcpy(szAmbient, "In.Diff");
		bColorMaterialAmbient = TRUE;
	} else {
		bColorMaterialAmbient = FALSE;
		sprintf(szAmbient, "%sAmbient", pszMtlFace);
	}

	// Diffuse source
	if (bColorMaterial && ((cm->Mode == GL_DIFFUSE) || (cm->Mode == GL_AMBIENT_AND_DIFFUSE))) {
		strcpy(szDiffuse, "In.Diff");
		bColorMaterialDiffuse = TRUE;
	} else {
		sprintf(szDiffuse, "%sDiffuse", pszMtlFace);
		bColorMaterialDiffuse = FALSE;
	}

	// Specular source
	if (bColorMaterial && (cm->Mode == GL_SPECULAR)) {
		strcpy(szSpecular, "In.Diff");
	} else {
		sprintf(szSpecular, "%sSpecular", pszMtlFace);
	}

	// Normal
	strcpy(szNormal, (face == GL_FRONT) ? "normal" : "-normal");

	//
	// Emissive
	//
	if (bColorMaterial && (cm->Mode == GL_EMISSION)) {
		// Emissive comes from vertex colour
		strcat(pszHLSL, "        Diffuse = In.Diff;\n");
	} else {
		// Emissive comes from material emissive
		sprintf(szLine, "        Diffuse = %sEmissive;\n", pszMtlFace);
		strcat(pszHLSL, szLine);
	}

	//
	// Global ambient
	//
	sprintf(szLine, "        Diffuse = Diffuse + (g_Ambient * %s);\n", szAmbient);
	strcat(pszHLSL, szLine);

	//
	// Contributions from all enabled light source
	//
	for (i=0; i<GLD_MAX_LIGHTS_DX9; i++) {
		const GLD_effect_light *pLight = &pLS->Light[i];
		if (pLight->Enabled) {
			// Ambient term
			if (bColorMaterial && bColorMaterialAmbient) {
				sprintf(szLine, "        _Ambient  = In.Diff * g_Light%d.%s.Ambient;\n", i, pszLightFace);
			} else {
				sprintf(szLine, "        _Ambient  = g_Light%d.%s._Ambient;\n", i, pszLightFace);
			}
			strcat(pszHLSL, szLine);
			// Diffuse term
			if (bColorMaterial && bColorMaterialDiffuse) {
				sprintf(szLine, "        _Diffuse  = In.Diff * g_Light%d.%s.Diffuse;\n", i, pszLightFace);
			} else {
				sprintf(szLine, "        _Diffuse  = g_Light%d.%s._Diffuse;\n", i, pszLightFace);
			}
			strcat(pszHLSL, szLine);
			if (bColorMaterial && (cm->Mode == GL_SPECULAR)) {
				sprintf(szLine, "        _Specular = In.Diff * g_Light%d.%s.Specular;\n", i, pszLightFace);
			} else {
				sprintf(szLine, "        _Specular = g_Light%d.%s._Specular;\n", i, pszLightFace);
			}
			strcat(pszHLSL, szLine);
			strcat(pszHLSL, "        Diffuse = Diffuse + light_");
			if (pLight->_Flags & LIGHT_SPOT) {
				// Spot light
				sprintf(szLine, "spot(EPos, %s, g_Light%d, _Ambient, _Diffuse, _Specular, %sShininess);\n", szNormal, i, pszMtlFace);
			} else if (pLight->_Flags & LIGHT_POSITIONAL) {
				// Positional light
				sprintf(szLine, "point(EPos, %s, g_Light%d, _Ambient, _Diffuse, _Specular, %sShininess);\n", szNormal, i, pszMtlFace);
			} else {
				// Directional light.
				sprintf(szLine, "dir(%s, g_Light%d, _Ambient, _Diffuse, _Specular, %sShininess);\n", szNormal, i, pszMtlFace);
			}
			strcat(pszHLSL, szLine);
		}
	}
}

//---------------------------------------------------------------------------

static char *_gldBuildShaderText(
	const GLD_effect *pGLDEffect,
	DWORD dwVSVersion,	// Vertex Shader version
	DWORD dwPSVersion,	// Pixel Shader version
	int index)
{
	//
	// Take the state in pGLDEffect and build an HLSL text file from it.
	//

	int							i;
	GLuint						uiUnitMask; // Bitmask for determining if a particular unit is enabled
	const GLD_effect_state		*pState = &pGLDEffect->State;
	const GLD_effect_texture	*pTexState = &pState->Texture;
	char						*pszHLSL = (char*)malloc(65536);
	char						szLine[1024];
	DWORD						dwVerMajor, dwVerMinor;
	BOOL						bNeedEyeCoords = FALSE; // Lighting or Texgen may need this
	BOOL						bNeedNormals = FALSE;

	BOOL						bColorMaterialFront	= FALSE;
	BOOL						bColorMaterialBack	= FALSE;

	pszHLSL[0] = 0;

#if 0
	// FOR TESTING PURPOSES ONLY!
	//strcpy(pszHLSL, g_pszTestEffect);
	//strcpy(pszHLSL, g_pszTestEffectTextured);
	//strcpy(pszHLSL, g_pszMinimalEffect);
	strcpy(pszHLSL, g_pszDefaultEffect);
	return pszHLSL;
#endif

	// Color Material
	if (pState->Light.ColorMaterial.Enabled) {
		if (pState->Light.ColorMaterial.Face == GL_FRONT_AND_BACK) {
			bColorMaterialFront = bColorMaterialBack = TRUE;
		} else {
			if (pState->Light.ColorMaterial.Face == GL_FRONT)
				bColorMaterialFront = TRUE;
			if (pState->Light.ColorMaterial.Face == GL_BACK)
				bColorMaterialBack = TRUE;
		}
	}

	// Test for REF
	if (glb.dwDriver == GLDS_DRIVER_REF) {
		// Current HLSL compiler does not support VS3.0 or PS3.0
		dwVSVersion = D3DVS_VERSION(2,0);
		dwPSVersion = D3DPS_VERSION(2,0);
	}

	// Test for eye-coords
	if ((pTexState->_GenFlags & TEXGEN_NEED_EYE_COORD) || pState->Fog.Enabled || pState->Light.Enabled)
		bNeedEyeCoords = TRUE;

	// Test for normal
	if ((pTexState->_GenFlags & TEXGEN_NEED_NORMALS) || pState->Light.Enabled)
		bNeedNormals = TRUE;

	// Copyright message
	strcat(pszHLSL, "\n// GLDirect HLSL Effect File.\n");
	strcat(pszHLSL, "// Copyright (C) 2004-2007 SciTech Software, Inc.\n");

#ifdef DEBUG
	_gldOutputDebugShaderText(pszHLSL, &pGLDEffect->State);
#endif

	//
	// ** Structs **
	//

	// Default structs
	strcat(pszHLSL, g_pszVertexShaderInput);
	strcat(pszHLSL, (pState->Fog.Enabled) ? g_pszVertexShaderOutputFog : g_pszVertexShaderOutput);
	if (pState->Light.Enabled) {
		strcat(pszHLSL, g_pszGLD_HLSL_light);
	}

	//
	// ** Variables **
	//

	if (pState->Light.Enabled) {
		// Emit scene global ambient light
		strcat(pszHLSL, "\nshared float4 g_Ambient;\n");
		// Emit Front material
		strcat(pszHLSL, "shared float  g_mtlFrontShininess;\n");
		strcat(pszHLSL, "shared float4 g_mtlFrontAmbient;\n");
		if (bColorMaterialFront) {
			if (pState->Light.ColorMaterial.Mode != GL_EMISSION) {
				strcat(pszHLSL, "shared float4 g_mtlFrontEmissive;\n");
			}
			switch (pState->Light.ColorMaterial.Mode) {
//			case GL_EMISSION:
//				strcat(pszHLSL, "shared float4 g_mtlFrontEmissive;\n");
//				break;
//			case GL_AMBIENT:
//				strcat(pszHLSL, "shared float4 g_mtlFrontAmbient;\n");
//				break;
			case GL_DIFFUSE:
				strcat(pszHLSL, "shared float4 g_mtlFrontDiffuse;\n");
				break;
			case GL_SPECULAR:
				strcat(pszHLSL, "shared float4 g_mtlFrontSpecular;\n");
				break;
			case GL_AMBIENT_AND_DIFFUSE:
				strcat(pszHLSL, "shared float4 g_mtlFrontDiffuse;\n");
				break;
			default:
				ASSERT(0);
			}
		} else {
			strcat(pszHLSL, "shared float4 g_mtlFrontEmissive;\n");
		}
		// Emit Back material
		if (pState->Light.TwoSide) {
			strcat(pszHLSL, "shared float  g_mtlBackShininess;\n");
			strcat(pszHLSL, "shared float4 g_mtlBackAmbient;\n");
			if (bColorMaterialBack) {
				if (pState->Light.ColorMaterial.Mode != GL_EMISSION) {
					strcat(pszHLSL, "shared float4 g_mtlBackEmissive;\n");
				}
				switch (pState->Light.ColorMaterial.Mode) {
//				case GL_EMISSION:
//					strcat(pszHLSL, "shared float4 g_mtlBackEmissive;\n");
//					break;
//				case GL_AMBIENT:
//					strcat(pszHLSL, "shared float4 g_mtlBackAmbient;\n");
//					break;
				case GL_DIFFUSE:
					strcat(pszHLSL, "shared float4 g_mtlBackDiffuse;\n");
					break;
				case GL_SPECULAR:
					strcat(pszHLSL, "shared float4 g_mtlBackSpecular;\n");
					break;
				case GL_AMBIENT_AND_DIFFUSE:
					strcat(pszHLSL, "shared float4 g_mtlBackDiffuse;\n");
					break;
				default:
					ASSERT(0);
				}
			} else {
				strcat(pszHLSL, "shared float4 g_mtlBackEmissive;\n");
			}
		}
		// Emit lights
		for (i=0; i<GLD_MAX_LIGHTS_DX9; i++) {
			if (pState->Light.Light[i].Enabled ) {
				sprintf(szLine, "shared GLD_HLSL_light g_Light%d;\n", i);
				strcat(pszHLSL, szLine);
			}
		}
	}

	// Fog
	if (pState->Fog.Enabled) {
		strcat(pszHLSL, "\nfloat4 g_Fog;\n");
	}

	// Always need full transform to output vertex
	strcat(pszHLSL, g_pszWVPTransform);

	// Transformation
	if (bNeedEyeCoords) {
		strcat(pszHLSL, g_pszTransformEyeCoords);
		strcat(pszHLSL, g_pszInverseTransformEyeCoords);
	}

	// Texture
	if (pTexState->_EnabledUnits) {
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if (pTexState->_EnabledUnits & uiUnitMask) {
				sprintf(szLine, "\nshared texture g_texDiffuse%d;\n", i);
				strcat(pszHLSL, szLine);
				// Blend colour
				if (pTexState->Unit[i].EnvMode == GL_BLEND) {
					sprintf(szLine, "shared float4 g_EnvColor%d;\n", i);
					strcat(pszHLSL, szLine);
				}
				// Texgen S
				if ((pTexState->Unit[i].TexGenEnabled & S_BIT) && (pTexState->Unit[i]._GenBitS & TEXGEN_NEED_TEXPLANE)) {
					sprintf(szLine, "shared float4 g_TexPlaneS%d;\n", i);
					strcat(pszHLSL, szLine);
				}
				// Texgen T
				if ((pTexState->Unit[i].TexGenEnabled & T_BIT) && (pTexState->Unit[i]._GenBitT & TEXGEN_NEED_TEXPLANE)) {
					sprintf(szLine, "shared float4 g_TexPlaneT%d;\n", i);
					strcat(pszHLSL, szLine);
				}
				// Texgen R
				if ((pTexState->Unit[i].TexGenEnabled & R_BIT) && (pTexState->Unit[i]._GenBitR & TEXGEN_NEED_TEXPLANE)) {
					sprintf(szLine, "shared float4 g_TexPlaneR%d;\n", i);
					strcat(pszHLSL, szLine);
				}
				// Texgen Q
				if ((pTexState->Unit[i].TexGenEnabled & Q_BIT) && (pTexState->Unit[i]._GenBitQ & TEXGEN_NEED_TEXPLANE)) {
					sprintf(szLine, "shared float4 g_TexPlaneQ%d;\n", i);
					strcat(pszHLSL, szLine);
				}
			}
			if (pTexState->_TexMatEnabled & uiUnitMask) {
				sprintf(szLine, "shared float4x4 g_matTexture%d;\n", i);
				strcat(pszHLSL, szLine);
			}
		}
	}

	//
	// ** Texture Samplers **
	//

	if (pTexState->_EnabledUnits) {
		const char *pszMipFilter, *pszMinFilter, *pszMagFilter;
		const char *pszWrapS, *pszWrapT;
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if (pTexState->_EnabledUnits & uiUnitMask) {
				pszMipFilter = _gldMipFilter(pTexState->Unit[i].MinFilter);
				pszMinFilter = _gldMinFilter(pTexState->Unit[i].MinFilter);
				pszMagFilter = _gldMagFilter(pTexState->Unit[i].MagFilter);
				pszWrapS = _gldWrap(pTexState->Unit[i].WrapS);
				pszWrapT = _gldWrap(pTexState->Unit[i].WrapT);
				sprintf(szLine, g_pszSamplerTemplate, i, i, pszMipFilter, pszMinFilter, pszMagFilter, pszWrapS, pszWrapT);
				strcat(pszHLSL, szLine);
			}
		}
	}

	//
	//  ** Helper Functions **
	//

	// Texgen functions
	if (pTexState->_EnabledUnits & pTexState->_TexGenEnabled) {
		// Object linear
		if (pTexState->_GenFlags & TEXGEN_OBJ_LINEAR)
			strcat(pszHLSL, g_pszTexGenObject);
		// Eye linear
		if (pTexState->_GenFlags & TEXGEN_EYE_LINEAR)
			strcat(pszHLSL, g_pszTexGenEye);
		// Spheremap
		if (pTexState->_GenFlags & TEXGEN_SPHERE_MAP)
			strcat(pszHLSL, g_pszTexGenSphere);
	}

	if (pState->Light.Enabled) {
		BOOL bDir, bSpot, bPoint;
		// Determine which lighting function(s) are required.
		// We could emit all three functions and let the HLSL compiler optimise out
		// the unneeded functions, but this is more favourable for debugging.
		bDir = bSpot = bPoint = FALSE;
		for (i=0; i<GLD_MAX_LIGHTS_DX9; i++) {
			if (pState->Light.Light[i].Enabled ) {
				if (pState->Light.Light[i]._Flags & LIGHT_SPOT) {
					// Spot light
					bSpot = TRUE;
				} else if (pState->Light.Light[i]._Flags & LIGHT_POSITIONAL) {
					// Positional light
					bPoint = TRUE;
				} else {
					// Directional light.
					bDir = TRUE;
				}
			}
		}
		// Emit functions
		if (bDir)
			strcat(pszHLSL, g_pszLightDir);
		if (bSpot)
			strcat(pszHLSL, g_pszLightSpot);
		if (bPoint)
			strcat(pszHLSL, g_pszLightPoint);
	}

	//
	//  ** Vertex Shader **
	//

	// Header
	strcat(pszHLSL, "\nVS_OUTPUT VS(VS_INPUT In)\n");
	strcat(pszHLSL, "{\n");
	strcat(pszHLSL, "    VS_OUTPUT Out = (VS_OUTPUT)0;\n");

	// Diffuse colour
	strcat(pszHLSL, "    float4 Diffuse = ");
	if (pState->Light.Enabled) {
		strcat(pszHLSL, "float4(0,0,0,0);\n");
	} else
		strcat(pszHLSL, "In.Diff;\n");

	// Define Variables
	if (pTexState->_EnabledUnits) {
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if (pTexState->_EnabledUnits & uiUnitMask) {
				sprintf(szLine, "    float4 tex%d = float4(0,0,0,1);\n", i);
				strcat(pszHLSL, szLine);
			}
		}
	}

	// Transformations. Always need to output an homogenous vertex.
	strcat(pszHLSL, "    Out.Pos  = mul(In.Pos, g_matWorldViewProject);\n");

	// Transformed normal
	if (bNeedNormals) {
		strcat(pszHLSL, "    float3 normal = normalize( mul( (float3x3)g_matInvWorldView, In.Norm));\n");
	}

	// Eye position
	if (bNeedEyeCoords) {
		strcat(pszHLSL, "    float4 EPos = mul(In.Pos, g_matWorldView);\n");
	}

	// Lighting
	if (pState->Light.Enabled) {
		strcat(pszHLSL, "    float4 _Ambient, _Diffuse, _Specular;\n");
		if (pState->Light.TwoSide) {
			strcat(pszHLSL, "    if (normal.z < 0) {\n");
			// Back material
			_gldBuildLightingFace(pszHLSL, &pState->Light, GL_BACK);
			// Else
			strcat(pszHLSL, "    } else {\n");
		}
		// Front material
		_gldBuildLightingFace(pszHLSL, &pState->Light, GL_FRONT);
		if (pState->Light.TwoSide) {
			strcat(pszHLSL, "    };\n");
		}
	}

	// Diffuse colour
	if (pState->Light.Enabled) {
		strcat(pszHLSL, "    Out.Diff.rgb = saturate(Diffuse);\n");
		// Alpha value comes from material diffuse alpha when lighting is enabled
		strcat(pszHLSL, "    Out.Diff.a = saturate(_Diffuse.a);\n");
	} else {
		strcat(pszHLSL, "    Out.Diff = saturate(Diffuse);\n");
	}

	// Spheremap
	if (pTexState->_EnabledUnits && pTexState->_TexGenEnabled && (pTexState->_GenFlags & TEXGEN_SPHERE_MAP)) {
		strcat(pszHLSL, "    float4 spheremap = gld_texgen_sphere(In.Pos, normal);\n");
	}

	// Texture functions
	if (pTexState->_EnabledUnits) {
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if (pTexState->_EnabledUnits & uiUnitMask) {
				if (pTexState->Unit[i].TexGenEnabled) {
					// Texgen S
					if (pTexState->Unit[i].TexGenEnabled & S_BIT) {
						_gldTexGenFunctionString(szLine, i, pTexState->Unit[i]._GenBitS, S_BIT);
						strcat(pszHLSL, szLine);
					}
					// Texgen T
					if (pTexState->Unit[i].TexGenEnabled & T_BIT) {
						_gldTexGenFunctionString(szLine, i, pTexState->Unit[i]._GenBitT, T_BIT);
						strcat(pszHLSL, szLine);
					}
					// Texgen R
					if (pTexState->Unit[i].TexGenEnabled & R_BIT) {
						_gldTexGenFunctionString(szLine, i, pTexState->Unit[i]._GenBitR, R_BIT);
						strcat(pszHLSL, szLine);
					}
					// Texgen Q
					if (pTexState->Unit[i].TexGenEnabled & Q_BIT) {
						_gldTexGenFunctionString(szLine, i, pTexState->Unit[i]._GenBitQ, Q_BIT);
						strcat(pszHLSL, szLine);
					}
				} else {
					sprintf(szLine, "    tex%d = In.Tex%d;\n", i, i);
					strcat(pszHLSL, szLine);
				}
			}
		}
	}

	// Texture matrices
	if (pTexState->_EnabledUnits) {
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if ((pTexState->_EnabledUnits & uiUnitMask) && (pTexState->_TexMatEnabled &uiUnitMask)) {
				sprintf(szLine, "    tex%d = mul(g_matTexture%d, tex%d);\n", i, i, i);
				strcat(pszHLSL, szLine);
			}
		}
	}

	// Output texture coords
	if (pTexState->_EnabledUnits) {
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if (pTexState->_EnabledUnits & uiUnitMask) {
				sprintf(szLine, "    Out.Tex%d = tex%d;\n", i, i);
				strcat(pszHLSL, szLine);
			}
		}
	}

	// Fog
	if (pState->Fog.Enabled) {
		switch (pState->Fog.Mode) {
		case GL_LINEAR:
			strcat(pszHLSL, g_pszFogLinear);
			break;
		case GL_EXP:
			strcat(pszHLSL, g_pszFogExponential);
			break;
		case GL_EXP2:
			strcat(pszHLSL, g_pszFogExponentialSquared);
			break;
		}
	}

	// Footer
	strcat(pszHLSL, "    return Out;\n");
	strcat(pszHLSL, "}\n");

	//
	// ** Pixel Shader **
	//
	
	// Header
	strcat(pszHLSL, "\nfloat4 PS(VS_OUTPUT In) : COLOR\n");
	strcat(pszHLSL, "{\n");
	strcat(pszHLSL, "    float4 Color = In.Diff;\n");

	// Textures
	if (pTexState->_EnabledUnits) {
		// Sample textures
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if (pTexState->_EnabledUnits & uiUnitMask) {
				sprintf(szLine, "    float4 tex%d = tex2D(Sampler%d, In.Tex%d.xy);\n", i, i, i);
				strcat(pszHLSL, szLine);
			}
		}
		// Combine diffuse and texture samples
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if (pTexState->_EnabledUnits & uiUnitMask) {
				_gldTexEnvString(szLine, i, pTexState->Unit[i].EnvMode);
				strcat(pszHLSL, szLine);
			}
		}
	}

	// Footer
	strcat(pszHLSL, "    return Color;\n}\n");

	//
	// ** Technique (currently must be named "tecGLDirect") **
	//

	// Header
	strcat(pszHLSL, "\ntechnique tecGLDirect\n{\n    pass Pass0\n    {\n");

	// Samplers
	if (pTexState->_EnabledUnits) {
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			uiUnitMask = (1 << i);
			if (pTexState->_EnabledUnits & uiUnitMask) {
				sprintf(szLine, "        Sampler[%d] = (Sampler%d);\n", i, i);
				strcat(pszHLSL, szLine);
			}
		}
	}

	// Vertex Shader
	dwVerMajor = D3DSHADER_VERSION_MAJOR(dwVSVersion);
	dwVerMinor = D3DSHADER_VERSION_MINOR(dwVSVersion);
	sprintf(szLine, "        VertexShader = compile vs_%d_%d VS();\n", dwVerMajor, dwVerMinor);
	strcat(pszHLSL, szLine);

	// Pixel Shader
	dwVerMajor = D3DSHADER_VERSION_MAJOR(dwPSVersion);
	dwVerMinor = D3DSHADER_VERSION_MINOR(dwPSVersion);
	sprintf(szLine, "        PixelShader  = compile ps_%d_%d PS();\n", dwVerMajor, dwVerMinor);
	strcat(pszHLSL, szLine);

	// Footer
	strcat(pszHLSL, "    }\n}\n");

	// String terminator - MUST BE LAST ITEM IN TEXT FILE.
	strcat(pszHLSL, "\0");

#ifdef DEBUG
	//
	// DEBUG BUILD ONLY!
	// Dump shader text out to a file so we can see what's going on.
	//

	{
		FILE	*fp;
		char	szFile[MAX_PATH];
		char	szModFile[MAX_PATH];
		char	*p;

		GetModuleFileName(NULL, szModFile, MAX_PATH); // NULL for current process
		p = strrchr(szModFile, '.');
		if (p) {
			*p = 0;
			strcpy(szFile, szModFile);
			sprintf(szLine, "_gld_effect_%d.fx", index);
			strcat(szFile, szLine);
			fp = fopen(szFile, "wt");
			if (fp) {
				fprintf(fp, "%s", pszHLSL);
				fclose(fp);
			}
		}
	}

#endif

	// Return the HLSL shader text
	return pszHLSL;
}

//---------------------------------------------------------------------------

static int _gldFindEffect(
	GLD_driver_dx9 *gld,
	const GLD_effect_state *pEffectState)
{
	int				i;
	HRESULT			hr;
	DWORD			dwFlags;
	GLD_effect		*pGLDEffect;
	ID3DXEffect		*pEffect;
	char			*pszEffect;
	GLD_handles		*pHandles;

	// See if effect has already been cached.
	pGLDEffect = gld->Effects;
	for (i=0; i<gld->nEffects; i++, pGLDEffect++) {
		if (memcmp(&pGLDEffect->State, pEffectState, sizeof(GLD_effect_state)) == 0)
			return i; // Found a matching effect. Return its index.
	}

	//
	// Effect not found. Create it.
	//

	// Reset all vars in effect
	ZeroMemory(pGLDEffect, sizeof(GLD_effect));

	// Copy effect details
	pGLDEffect->State = *pEffectState;
#if 0
    // SM 3.x is not working with DX 9.0b SDK; remove this when 9.0c SDK build works
    if (D3DSHADER_VERSION_MAJOR(gld->d3dCaps9.VertexShaderVersion) > 2 ||
        D3DSHADER_VERSION_MAJOR(gld->d3dCaps9.PixelShaderVersion) > 2)
        pszEffect = _gldBuildShaderText(pGLDEffect, D3DVS_VERSION(2,0), D3DPS_VERSION(2,0), gld->nEffects);
    else
    // end of DX 9.0b SDK hack
#endif
#if 1
	pszEffect = _gldBuildShaderText(pGLDEffect, gld->d3dCaps9.VertexShaderVersion, gld->d3dCaps9.PixelShaderVersion, gld->nEffects);
#else
	// FOR TESTING ONLY
	// Force compilation for a particular pixel shader target
	//pszEffect = _gldBuildShaderText(pGLDEffect, D3DPS_VERSION(1,4), gld->nEffects);
	//pszEffect = _gldBuildShaderText(pGLDEffect, D3DVS_VERSION(1,1), D3DPS_VERSION(1,1), gld->nEffects);
	pszEffect = _gldBuildShaderText(pGLDEffect, D3DVS_VERSION(1,1), D3DPS_VERSION(1,3), gld->nEffects);
#endif

#ifdef _DEBUG
	// Always debug shaders in DEBUG builds
	dwFlags = D3DXSHADER_PARTIALPRECISION | D3DXSHADER_DEBUG;
	// Problem with D3DXSHADER_SKIPOPTIMIZATION;
#else
	dwFlags = D3DXSHADER_PARTIALPRECISION;	// For nVidia parts
#endif

	//dwFlags |= D3DXSHADER_USE_LEGACY_D3DX9_31_DLL;

	hr = D3DXCreateEffect(
			gld->pDev,				// device
			pszEffect,				// .fx filename and path
			strlen(pszEffect),
			NULL,					// macro defines
			NULL,					// includes
			dwFlags,				// Flags
			gld->pEffectPool,		// pool
			&pGLDEffect->pEffect,	// pointer to DX9 effect
			NULL					// Ptr to buffer returning compile errors
			);
	free(pszEffect); // Done with effect text. Free memory before we return
	if (FAILED(hr)) {
#if 0
		return -1;
#else
		// Effect compilation failed. Fallback to a default effect.
		hr = D3DXCreateEffect(
				gld->pDev,				// device
				g_pszDefaultEffect,		// .fx filename and path
				strlen(g_pszDefaultEffect),
				NULL,					// macro defines
				NULL,					// includes
				dwFlags,				// Flags
				gld->pEffectPool,		// pool
				&pGLDEffect->pEffect,	// pointer to DX9 effect
				NULL					// Ptr to buffer returning compile errors
				);
		if (FAILED(hr)) {
			return -1;
		}
#endif
	}

	// Obtain pointer for efficiency
	pEffect = pGLDEffect->pEffect;

	//
	// Now obtain handles to string parameters for performance.
	//

	pHandles = &pGLDEffect->Handles;

	// Matrices
	pHandles->matWorldViewProject	= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_matWorldViewProject");
	pHandles->matWorldView			= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_matWorldView");
	pHandles->matInvWorldView		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_matInvWorldView");

	// Material handles
	pHandles->mtlFrontAmbient		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlFrontAmbient");
	pHandles->mtlFrontDiffuse		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlFrontDiffuse");
	pHandles->mtlFrontSpecular		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlFrontSpecular");
	pHandles->mtlFrontEmissive		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlFrontEmissive");
	pHandles->mtlFrontShininess		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlFrontShininess");
	pHandles->mtlBackAmbient		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlBackAmbient");
	pHandles->mtlBackDiffuse		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlBackDiffuse");
	pHandles->mtlBackSpecular		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlBackSpecular");
	pHandles->mtlBackEmissive		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlBackEmissive");
	pHandles->mtlBackShininess		= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_mtlBackShininess");

	// Fog
	pHandles->Fog					= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_Fog");

	// Scene global ambient light
	pHandles->Ambient				= ID3DXEffect_GetParameterByName(pEffect, NULL, "g_Ambient");

	// Lights
	for (i=0; i<GLD_MAX_LIGHTS_DX9; i++) {
		pHandles->Lights[i] = _gldGetIndexedEffectVariable(pEffect, "g_Light", i);
	}

	// Texture unit items
	for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
		// Diffuse textures
		pHandles->texDiffuse[i] = _gldGetIndexedEffectVariable(pEffect, "g_texDiffuse", i);
		// Texture matrices
		pHandles->matTexture[i] = _gldGetIndexedEffectVariable(pEffect, "g_matTexture", i);
		// Inverse Texture matrices
		pHandles->matInvTexture[i] = _gldGetIndexedEffectVariable(pEffect, "g_matInvTexture", i);
		// TexEnv colour
		pHandles->EnvColor[i] = _gldGetIndexedEffectVariable(pEffect, "g_EnvColor", i);
		// TexGen planes
		pHandles->TexPlaneS[i] = _gldGetIndexedEffectVariable(pEffect, "g_TexPlaneS", i);
		pHandles->TexPlaneT[i] = _gldGetIndexedEffectVariable(pEffect, "g_TexPlaneT", i);
		pHandles->TexPlaneR[i] = _gldGetIndexedEffectVariable(pEffect, "g_TexPlaneR", i);
		pHandles->TexPlaneQ[i] = _gldGetIndexedEffectVariable(pEffect, "g_TexPlaneQ", i);
	}

	// Obtain Techniques
	pGLDEffect->hTechnique = ID3DXEffect_GetTechniqueByName(pGLDEffect->pEffect, "tecGLDirect");

	// Return
	i = gld->nEffects;
	gld->nEffects++;
	return i;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

void gldUpdateShaders(
	GLcontext *ctx)
{
	int								i;
	GLD_context						*gldCtx	= GLD_GET_CONTEXT(ctx);
	GLD_driver_dx9					*gld	= GLD_GET_DX9_DRIVER(gldCtx);
	GLD_effect_state				gldES;
	GLD_effect						*pGLDEffect;
	GLD_handles						*pHandles;
	ID3DXEffect						*pEffect;
	const struct gl_light			*glLit;
	UINT							uiBytes;
	BOOL							bEffectChanged = FALSE;

	//
	// Fill in  a GLD_effect structure containing the current GL state.
	// This will be used to build the text of the HLSL shader.
	//

	// Important - ensure all member vars are reset.
	ZeroMemory(&gldES, sizeof(gldES));

	//
	// Texture
	//
	if (ctx->Texture._EnabledUnits) {
		// At least one unit is enabled
		gldES.Texture._EnabledUnits	= ctx->Texture._EnabledUnits;
		gldES.Texture._GenFlags		= ctx->Texture._GenFlags;
		gldES.Texture._TexGenEnabled	= ctx->Texture._TexGenEnabled;
		gldES.Texture._TexMatEnabled	= ctx->Texture._TexMatEnabled;
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			GLuint UnitMask = (GLuint)1 << i;
			if (ctx->Texture._EnabledUnits && UnitMask) {
				// Obtain pointers
				const struct gl_texture_unit *glUnit	= &ctx->Texture.Unit[i];
				GLD_effect_texunit *gldUnit				= &gldES.Texture.Unit[i];
				// Fill in the state
				gldUnit->EnvMode			= glUnit->EnvMode;
				if (gldES.Texture._GenFlags) {
					gldUnit->_GenBitS		= glUnit->_GenBitS;
					gldUnit->_GenBitT		= glUnit->_GenBitT;
					gldUnit->_GenBitR		= glUnit->_GenBitR;
					gldUnit->_GenBitQ		= glUnit->_GenBitQ;
				}
				gldUnit->TexGenEnabled	= glUnit->TexGenEnabled;
				if (glUnit->_Current) {
					gldUnit->MagFilter		= glUnit->_Current->MagFilter;
					gldUnit->MinFilter		= glUnit->_Current->MinFilter;
					gldUnit->WrapS			= glUnit->_Current->WrapS;
					gldUnit->WrapT			= glUnit->_Current->WrapT;
				}
			}
		}
	}

	//
	// Fog
	//
	gldES.Fog.Enabled	= ctx->Fog.Enabled;
	gldES.Fog.Mode		= ctx->Fog.Mode;

	//
	// Lighting
	//
	gldES.Light.Enabled = ctx->Light.Enabled;
	if (ctx->Light.Enabled) {
		gldES.Light.ColorMaterial.Enabled	= ctx->Light.ColorMaterialEnabled;
		gldES.Light.ColorMaterial.Face		= ctx->Light.ColorMaterialFace;
		gldES.Light.ColorMaterial.Mode		= ctx->Light.ColorMaterialMode;
		gldES.Light.TwoSide					= ctx->Light.Model.TwoSide;
		for (i=0; i<GLD_MAX_LIGHTS_DX9; i++) {
			gldES.Light.Light[i].Enabled	= ctx->Light.Light[i].Enabled;
			gldES.Light.Light[i]._Flags		= ctx->Light.Light[i]._Flags;
		}
	}

	// Find a matching effect (or create a new one)
	gld->iCurEffect = -1;
	i = _gldFindEffect(gld, &gldES);
	if (i < 0) {
		gldLogMessage(GLDLOG_ERROR, "FindEffect failed\n");
		return;
	}

	// Set current effect
	gld->iCurEffect = i;

	pGLDEffect = &gld->Effects[i];
	pEffect = pGLDEffect->pEffect;

	// Sanity Check
	if (pEffect == NULL) {
		gld->iCurEffect = -1;
		return;
	}

	//
	// Set the Effect
	//
	if (gld->iCurEffect != gld->iLastEffect) {
		// Effect has changed. Set it.
		if (gld->iLastEffect >= 0) {
			gldEndEffect(gld, gld->iLastEffect);
		}
		gld->iLastEffect = gld->iCurEffect;
		if (FAILED(ID3DXEffect_SetTechnique(pEffect, pGLDEffect->hTechnique))) {
			gld->iCurEffect = -1;
			return;
		}
		gldBeginEffect(gld, gld->iCurEffect);
	}

	//
	// Now update Effect paramters
	//

	pHandles = &pGLDEffect->Handles;

	// matWorldViewProject must be set in all vertex shaders, otherwise the input vertex cannot be transformed!
	ASSERT(pHandles->matWorldViewProject); // Sanity test in DEBUG builds
	if (pHandles->matWorldViewProject)
		ID3DXEffect_SetMatrix(pEffect, pHandles->matWorldViewProject, &gld->matModelViewProject);
	if (pHandles->matWorldView)
		ID3DXEffect_SetMatrix(pEffect, pHandles->matWorldView, &gld->matModelView);
	if (pHandles->matInvWorldView)
		ID3DXEffect_SetMatrix(pEffect, pHandles->matInvWorldView, &gld->matInvModelView);

	//
	// Only update light state if lighting is enabled
	//
	if (ctx->Light.Enabled) {
		const struct gl_material	*mat = &ctx->Light.Material;
		GLD_HLSL_light				Light;	// Ensure this is defined without packing!
		// Global ambient light
		_gldSetEffectVector(pEffect, pHandles->Ambient, ctx->Light.Model.Ambient);
		// Front Material
		_gldSetEffectVector(pEffect, pHandles->mtlFrontAmbient, mat->Attrib[MAT_ATTRIB_FRONT_AMBIENT]);
		_gldSetEffectVector(pEffect, pHandles->mtlFrontDiffuse, mat->Attrib[MAT_ATTRIB_FRONT_DIFFUSE]);
		_gldSetEffectVector(pEffect, pHandles->mtlFrontSpecular, mat->Attrib[MAT_ATTRIB_FRONT_SPECULAR]);
		_gldSetEffectVector(pEffect, pHandles->mtlFrontEmissive, mat->Attrib[MAT_ATTRIB_FRONT_EMISSION]);
		if (pHandles->mtlFrontShininess)
			ID3DXEffect_SetFloat(pEffect, pHandles->mtlFrontShininess, mat->Attrib[MAT_ATTRIB_FRONT_SHININESS][0]);
		// Back Material
		if (ctx->Light.Model.TwoSide) {
			_gldSetEffectVector(pEffect, pHandles->mtlBackAmbient, mat->Attrib[MAT_ATTRIB_BACK_AMBIENT]);
			_gldSetEffectVector(pEffect, pHandles->mtlBackDiffuse, mat->Attrib[MAT_ATTRIB_BACK_DIFFUSE]);
			_gldSetEffectVector(pEffect, pHandles->mtlBackSpecular, mat->Attrib[MAT_ATTRIB_BACK_SPECULAR]);
			_gldSetEffectVector(pEffect, pHandles->mtlBackEmissive, mat->Attrib[MAT_ATTRIB_BACK_EMISSION]);
			if (pHandles->mtlBackShininess)
				ID3DXEffect_SetFloat(pEffect, pHandles->mtlBackShininess, mat->Attrib[MAT_ATTRIB_BACK_SHININESS][0]);
		}
		// Lights
		glLit = &ctx->Light.Light[0];
		for (i=0; i<GLD_MAX_LIGHTS_DX9; i++, glLit++) {
			if (pHandles->Lights[i]) {
				gl4fToVec4(&Light.Front.Ambient, glLit->Ambient);
				gl4fToVec4(&Light.Front.Diffuse, glLit->Diffuse);
				gl4fToVec4(&Light.Front.Specular, glLit->Specular);
				gl3fToVec4(&Light.Front._Ambient,	glLit->_MatAmbient[0]);
				gl3fToVec4(&Light.Front._Diffuse,	glLit->_MatDiffuse[0]);
				gl3fToVec4(&Light.Front._Specular,	glLit->_MatSpecular[0]);
				if (ctx->Light.Model.TwoSide) {
					gl4fToVec4(&Light.Back.Ambient, glLit->Ambient);
					gl4fToVec4(&Light.Back.Diffuse, glLit->Diffuse);
					gl4fToVec4(&Light.Back.Specular, glLit->Specular);
					gl3fToVec4(&Light.Back._Ambient,	glLit->_MatAmbient[1]);
					gl3fToVec4(&Light.Back._Diffuse,	glLit->_MatDiffuse[1]);
					gl3fToVec4(&Light.Back._Specular,	glLit->_MatSpecular[1]);
				}
				if (glLit->_Flags & LIGHT_SPOT) {
					gl4fToVec4(&Light.Position, glLit->_Position);
					gl4fToVec4(&Light.Direction, glLit->EyeDirection);
				} else if (glLit->_Flags & LIGHT_POSITIONAL) {
					gl4fToVec4(&Light.Position, glLit->_Position);
				} else {
					Light.Direction = gld->LightDir[i];
				}
				Light.Attenuation.x	= glLit->ConstantAttenuation;
				Light.Attenuation.y	= glLit->LinearAttenuation;
				Light.Attenuation.z	= glLit->QuadraticAttenuation;
				Light.SpotLight.x	= glLit->_CosCutoff;
				Light.SpotLight.y	= glLit->SpotExponent;
#ifdef DEBUG
				uiBytes = sizeof(Light);
#else
				// Pass in D3DX_DEFAULT if you know you buffer is large enough to contain the entire parameter, and want to skip size validation.
				uiBytes = D3DX_DEFAULT;
#endif
				ID3DXEffect_SetValue(pEffect, pHandles->Lights[i], &Light, uiBytes);
			}
		}
	}

	// Only update texture state if texturing is enabled
	if (ctx->Texture._EnabledUnits) {
		// Texture units
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			const struct gl_texture_unit	*pUnit = &ctx->Texture.Unit[i];
			const struct gl_texture_object	*tObj = pUnit->_Current;
			// Diffuse Texture
			if (pHandles->texDiffuse[i])
				ID3DXEffect_SetTexture(pEffect, pHandles->texDiffuse[i], tObj->DriverData);
			// Texture env colour (for GL_BLEND)
			if (pHandles->EnvColor[i])
				_gldSetEffectVector(pEffect, pHandles->EnvColor[i], &pUnit->EnvColor[0]);
			// Eye plane texgen
			if (pHandles->TexPlaneS[i])
				_gldSetEffectVector(pEffect, pHandles->TexPlaneS[i], (pUnit->GenModeS == GL_OBJECT_LINEAR) ? &pUnit->ObjectPlaneS[0] : &pUnit->EyePlaneS[0]);
			if (pHandles->TexPlaneT[i])
				_gldSetEffectVector(pEffect, pHandles->TexPlaneT[i], (pUnit->GenModeT == GL_OBJECT_LINEAR) ? &pUnit->ObjectPlaneT[0] : &pUnit->EyePlaneT[0]);
			if (pHandles->TexPlaneR[i])
				_gldSetEffectVector(pEffect, pHandles->TexPlaneR[i], (pUnit->GenModeR == GL_OBJECT_LINEAR) ? &pUnit->ObjectPlaneR[0] : &pUnit->EyePlaneR[0]);
			if (pHandles->TexPlaneQ[i])
				_gldSetEffectVector(pEffect, pHandles->TexPlaneQ[i], (pUnit->GenModeQ == GL_OBJECT_LINEAR) ? &pUnit->ObjectPlaneQ[0] : &pUnit->EyePlaneQ[0]);
		}
	}

	// Only update texture state if texturing is enabled
	if (ctx->Texture._EnabledUnits) {
		// Texture units
		for (i=0; i<GLD_MAX_TEXTURE_UNITS_DX9; i++) {
			// Texture matrix
			if (pHandles->matTexture[i])
				ID3DXEffect_SetMatrix(pEffect, pHandles->matTexture[i], &gld->matTexture[i]);
			// Inverse Texture matrix
			if (pHandles->matInvTexture[i])
				ID3DXEffect_SetMatrix(pEffect, pHandles->matInvTexture[i], &gld->matInvTexture[i]);
		}
	}

	// Only update fog state if fog is enabled
	if (ctx->Fog.Enabled && pHandles->Fog) {
		D3DXVECTOR4	vFog;
		// Pack Start, End and Density into a single VEC4.
		// This should result in a single constant register begin used in the shader.
		vFog.x = ctx->Fog.Start;
		vFog.y = ctx->Fog.End;
		vFog.z = ctx->Fog.Density;
		vFog.w = 1.0f;
		ID3DXEffect_SetVector(pEffect, pHandles->Fog, &vFog);
	}
}

//---------------------------------------------------------------------------

void gldReleaseShaders(
	GLD_driver_dx9 *gld)
{
	int i;

	SAFE_RELEASE(gld->pEffectPool);

	for (i=0; i<gld->nEffects; i++) {
//		_SetEffectTexture(&gld->Effects[i], GLD_HANDLE_TEXTURE_DIFFUSE0, NULL);
		SAFE_RELEASE(gld->Effects[i].pEffect);
	}

	gld->nEffects = 0;
}

//---------------------------------------------------------------------------

void gldBeginEffect(
	GLD_driver_dx9 *gld,
	int iEffect)
{
	GLD_effect	*pGLDEffect;
	UINT		uPasses;
	DWORD		dwFlags;

	// Sanity test
	if (iEffect < 0)
		return;

#ifdef _DEBUG
	D3DPERF_BeginEvent(0xffffffff, L"gldBeginEffect");
#endif

	pGLDEffect = &gld->Effects[iEffect];

#ifdef _DEBUG
	dwFlags = 0;
#else
	dwFlags = D3DXFX_DONOTSAVESTATE | D3DXFX_DONOTSAVESHADERSTATE;
#endif

	pGLDEffect = &gld->Effects[gld->iCurEffect];
	ID3DXEffect_Begin(pGLDEffect->pEffect, &uPasses, dwFlags);
	ASSERT(uPasses == 1);
	ID3DXEffect_BeginPass(pGLDEffect->pEffect, 0);
}

//---------------------------------------------------------------------------

void gldEndEffect(
	GLD_driver_dx9 *gld,
	int iEffect)
{
	GLD_effect	*pGLDEffect;

	// Sanity test
	if (iEffect < 0)
		return;

	pGLDEffect = &gld->Effects[iEffect];
	ID3DXEffect_EndPass(pGLDEffect->pEffect);
	ID3DXEffect_End(pGLDEffect->pEffect);

#ifdef _DEBUG
	D3DPERF_EndEvent(); // gldBeginEffect
#endif
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
