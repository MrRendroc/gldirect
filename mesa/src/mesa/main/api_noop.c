
/*
 * Mesa 3-D graphics library
 * Version:  5.1
 *
 * Copyright (C) 1999-2003  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include "glheader.h"
#include "api_noop.h"
#include "api_validate.h"
#include "api_arrayelt.h"
#include "context.h"
#include "colormac.h"
#include "light.h"
#include "macros.h"
#include "mtypes.h"
#include "dlist.h"


/* In states where certain vertex components are required for t&l or
 * rasterization, we still need to keep track of the current values.
 * These functions provide this service by keeping uptodate the
 * 'ctx->Current' struct for all data elements not included in the
 * currently enabled hardware vertex.
 *
 */
void GLAPIENTRY _mesa_noop_EdgeFlag( GLboolean b )
{
   GET_CURRENT_CONTEXT(ctx);
   ctx->Current.EdgeFlag = b;
}

void GLAPIENTRY _mesa_noop_EdgeFlagv( const GLboolean *b )
{
   GET_CURRENT_CONTEXT(ctx);
   ctx->Current.EdgeFlag = *b;
}

void GLAPIENTRY _mesa_noop_Indexf( GLfloat f )
{
   GET_CURRENT_CONTEXT(ctx);
   ctx->Current.Index = f;
}

void GLAPIENTRY _mesa_noop_Indexfv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   ctx->Current.Index = *v;
}

void GLAPIENTRY _mesa_noop_FogCoordfEXT( GLfloat a )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_FOG];
   dest[0] = a;
   dest[1] = 0.0;
   dest[2] = 0.0;
   dest[3] = 1.0;
}

void GLAPIENTRY _mesa_noop_FogCoordfvEXT( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_NORMAL];
   dest[0] = v[0];
   dest[1] = 0.0;
   dest[2] = 0.0;
   dest[3] = 1.0;
}

void GLAPIENTRY _mesa_noop_Normal3f( GLfloat a, GLfloat b, GLfloat c )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_NORMAL];
   dest[0] = a;
   dest[1] = b;
   dest[2] = c;
   dest[3] = 1.0;
}

void GLAPIENTRY _mesa_noop_Normal3fv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_NORMAL];
   dest[0] = v[0];
   dest[1] = v[1];
   dest[2] = v[2];
   dest[3] = 1.0;
}

void GLAPIENTRY _mesa_noop_Color4f( GLfloat a, GLfloat b, GLfloat c, GLfloat d )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *color = ctx->Current.Attrib[VERT_ATTRIB_COLOR0];
   color[0] = a;
   color[1] = b;
   color[2] = c;
   color[3] = d;
}

void GLAPIENTRY _mesa_noop_Color4fv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *color = ctx->Current.Attrib[VERT_ATTRIB_COLOR0];
   color[0] = v[0];
   color[1] = v[1];
   color[2] = v[2];
   color[3] = v[3];
}

void GLAPIENTRY _mesa_noop_Color3f( GLfloat a, GLfloat b, GLfloat c )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *color = ctx->Current.Attrib[VERT_ATTRIB_COLOR0];
   color[0] = a;
   color[1] = b;
   color[2] = c;
   color[3] = 1.0;
}

void GLAPIENTRY _mesa_noop_Color3fv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *color = ctx->Current.Attrib[VERT_ATTRIB_COLOR0];
   color[0] = v[0];
   color[1] = v[1];
   color[2] = v[2];
   color[3] = 1.0;
}

void GLAPIENTRY _mesa_noop_MultiTexCoord1fARB( GLenum target, GLfloat a )
{
   GET_CURRENT_CONTEXT(ctx);
   GLuint unit = target - GL_TEXTURE0_ARB;

   /* unit is unsigned -- cannot be less than zero.
    */
   if (unit < MAX_TEXTURE_COORD_UNITS)
   {
      GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0 + unit];
      dest[0] = a;
      dest[1] = 0;
      dest[2] = 0;
      dest[3] = 1;
   }
}

void GLAPIENTRY _mesa_noop_MultiTexCoord1fvARB( GLenum target, const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLuint unit = target - GL_TEXTURE0_ARB;

   /* unit is unsigned -- cannot be less than zero.
    */
   if (unit < MAX_TEXTURE_COORD_UNITS)
   {
      GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0 + unit];
      dest[0] = v[0];
      dest[1] = 0;
      dest[2] = 0;
      dest[3] = 1;
   }
}

void GLAPIENTRY _mesa_noop_MultiTexCoord2fARB( GLenum target, GLfloat a, GLfloat b )
{
   GET_CURRENT_CONTEXT(ctx);
   GLuint unit = target - GL_TEXTURE0_ARB;

   /* unit is unsigned -- cannot be less than zero.
    */
   if (unit < MAX_TEXTURE_COORD_UNITS)
   {
      GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0 + unit];
      dest[0] = a;
      dest[1] = b;
      dest[2] = 0;
      dest[3] = 1;
   }
}

void GLAPIENTRY _mesa_noop_MultiTexCoord2fvARB( GLenum target, const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLuint unit = target - GL_TEXTURE0_ARB;

   /* unit is unsigned -- cannot be less than zero.
    */
   if (unit < MAX_TEXTURE_COORD_UNITS)
   {
      GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0 + unit];
      dest[0] = v[0];
      dest[1] = v[1];
      dest[2] = 0;
      dest[3] = 1;
   }
}

void GLAPIENTRY _mesa_noop_MultiTexCoord3fARB( GLenum target, GLfloat a, GLfloat b, GLfloat c)
{
   GET_CURRENT_CONTEXT(ctx);
   GLuint unit = target - GL_TEXTURE0_ARB;

   /* unit is unsigned -- cannot be less than zero.
    */
   if (unit < MAX_TEXTURE_COORD_UNITS)
   {
      GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0 + unit];
      dest[0] = a;
      dest[1] = b;
      dest[2] = c;
      dest[3] = 1;
   }
}

void GLAPIENTRY _mesa_noop_MultiTexCoord3fvARB( GLenum target, const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLuint unit = target - GL_TEXTURE0_ARB;

   /* unit is unsigned -- cannot be less than zero.
    */
   if (unit < MAX_TEXTURE_COORD_UNITS)
   {
      GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0 + unit];
      dest[0] = v[0];
      dest[1] = v[1];
      dest[2] = v[2];
      dest[3] = 1;
   }
}

void GLAPIENTRY _mesa_noop_MultiTexCoord4fARB( GLenum target, GLfloat a, GLfloat b,
			      GLfloat c, GLfloat d )
{
   GET_CURRENT_CONTEXT(ctx);
   GLuint unit = target - GL_TEXTURE0_ARB;

   /* unit is unsigned -- cannot be less than zero.
    */
   if (unit < MAX_TEXTURE_COORD_UNITS)
   {
      GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0 + unit];
      dest[0] = a;
      dest[1] = b;
      dest[2] = c;
      dest[3] = d;
   }
}

void GLAPIENTRY _mesa_noop_MultiTexCoord4fvARB( GLenum target, const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLuint unit = target - GL_TEXTURE0_ARB;

   /* unit is unsigned -- cannot be less than zero.
    */
   if (unit < MAX_TEXTURE_COORD_UNITS)
   {
      GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0 + unit];
      dest[0] = v[0];
      dest[1] = v[1];
      dest[2] = v[2];
      dest[3] = v[3];
   }
}

void GLAPIENTRY _mesa_noop_SecondaryColor3fEXT( GLfloat a, GLfloat b, GLfloat c )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *color = ctx->Current.Attrib[VERT_ATTRIB_COLOR1];
   color[0] = a;
   color[1] = b;
   color[2] = c;
   color[3] = 1.0;
}

void GLAPIENTRY _mesa_noop_SecondaryColor3fvEXT( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *color = ctx->Current.Attrib[VERT_ATTRIB_COLOR1];
   color[0] = v[0];
   color[1] = v[1];
   color[2] = v[2];
   color[3] = 1.0;
}

void GLAPIENTRY _mesa_noop_TexCoord1f( GLfloat a )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0];
   dest[0] = a;
   dest[1] = 0;
   dest[2] = 0;
   dest[3] = 1;
}

void GLAPIENTRY _mesa_noop_TexCoord1fv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0];
   dest[0] = v[0];
   dest[1] = 0;
   dest[2] = 0;
   dest[3] = 1;
}

void GLAPIENTRY _mesa_noop_TexCoord2f( GLfloat a, GLfloat b )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0];
   dest[0] = a;
   dest[1] = b;
   dest[2] = 0;
   dest[3] = 1;
}

void GLAPIENTRY _mesa_noop_TexCoord2fv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0];
   dest[0] = v[0];
   dest[1] = v[1];
   dest[2] = 0;
   dest[3] = 1;
}

void GLAPIENTRY _mesa_noop_TexCoord3f( GLfloat a, GLfloat b, GLfloat c )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0];
   dest[0] = a;
   dest[1] = b;
   dest[2] = c;
   dest[3] = 1;
}

void GLAPIENTRY _mesa_noop_TexCoord3fv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0];
   dest[0] = v[0];
   dest[1] = v[1];
   dest[2] = v[2];
   dest[3] = 1;
}

void GLAPIENTRY _mesa_noop_TexCoord4f( GLfloat a, GLfloat b, GLfloat c, GLfloat d )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0];
   dest[0] = a;
   dest[1] = b;
   dest[2] = c;
   dest[3] = d;
}

void GLAPIENTRY _mesa_noop_TexCoord4fv( const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat *dest = ctx->Current.Attrib[VERT_ATTRIB_TEX0];
   dest[0] = v[0];
   dest[1] = v[1];
   dest[2] = v[2];
   dest[3] = v[3];
}



void GLAPIENTRY _mesa_noop_VertexAttrib1fNV( GLuint index, GLfloat x )
{
   GET_CURRENT_CONTEXT(ctx);
   if (index < VERT_ATTRIB_MAX) {
      ASSIGN_4V(ctx->Current.Attrib[index], x, 0, 0, 1);
   }
   else
      _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib1f" );
}

void GLAPIENTRY _mesa_noop_VertexAttrib1fvNV( GLuint index, const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   if (index < VERT_ATTRIB_MAX) {
      ASSIGN_4V(ctx->Current.Attrib[index], v[0], 0, 0, 1);
   }
   else
      _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib1fv" );
}

void GLAPIENTRY _mesa_noop_VertexAttrib2fNV( GLuint index, GLfloat x, GLfloat y )
{
   GET_CURRENT_CONTEXT(ctx);
   if (index < VERT_ATTRIB_MAX) {
      ASSIGN_4V(ctx->Current.Attrib[index], x, y, 0, 1);
   }
   else
      _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib2f" );
}

void GLAPIENTRY _mesa_noop_VertexAttrib2fvNV( GLuint index, const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   if (index < VERT_ATTRIB_MAX) {
      ASSIGN_4V(ctx->Current.Attrib[index], v[0], v[1], 0, 1);
   }
   else
      _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib2fv" );
}

void GLAPIENTRY _mesa_noop_VertexAttrib3fNV( GLuint index, GLfloat x,
                                  GLfloat y, GLfloat z )
{
   GET_CURRENT_CONTEXT(ctx);
   if (index < VERT_ATTRIB_MAX) {
      ASSIGN_4V(ctx->Current.Attrib[index], x, y, z, 1);
   }
   else
      _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib3f" );
}

void GLAPIENTRY _mesa_noop_VertexAttrib3fvNV( GLuint index, const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   if (index < VERT_ATTRIB_MAX) {
      ASSIGN_4V(ctx->Current.Attrib[index], v[0], v[1], v[2], 1);
   }
   else
      _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib3fv" );
}

void GLAPIENTRY _mesa_noop_VertexAttrib4fNV( GLuint index, GLfloat x,
                                  GLfloat y, GLfloat z, GLfloat w )
{
   GET_CURRENT_CONTEXT(ctx);
   if (index < VERT_ATTRIB_MAX) {
      ASSIGN_4V(ctx->Current.Attrib[index], x, y, z, w);
   }
   else
      _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib4f" );
}

void GLAPIENTRY _mesa_noop_VertexAttrib4fvNV( GLuint index, const GLfloat *v )
{
   GET_CURRENT_CONTEXT(ctx);
   if (index < VERT_ATTRIB_MAX) {
      ASSIGN_4V(ctx->Current.Attrib[index], v[0], v[1], v[2], v[3]);
   }
   else
      _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttrib4fv" );
}

/* Material
 */
void GLAPIENTRY _mesa_noop_Materialfv( GLenum face, GLenum pname, const GLfloat *params )
{
   GET_CURRENT_CONTEXT(ctx);
   GLint i, nr;
   struct gl_material *mat = &ctx->Light.Material;
   GLuint bitmask = _mesa_material_bitmask( ctx, face, pname, ~0,
                                            "_mesa_noop_Materialfv" );

   if (ctx->Light.ColorMaterialEnabled)
      bitmask &= ~ctx->Light.ColorMaterialBitmask;

   if (bitmask == 0)
      return;

   switch (face) {
   case GL_SHININESS: nr = 1; break;
   case GL_COLOR_INDEXES: nr = 3; break;
   default: nr = 4 ; break;
   }

   for (i = 0 ; i < MAT_ATTRIB_MAX ; i++) 
      if (bitmask & (1<<i))
	 COPY_SZ_4V( mat->Attrib[i], nr, params ); 

   _mesa_update_material( ctx, bitmask );
}

/* These really are noops outside begin/end:
 */
void GLAPIENTRY _mesa_noop_Vertex2fv( const GLfloat *v )
{
   (void) v;
}

void GLAPIENTRY _mesa_noop_Vertex3fv( const GLfloat *v )
{
   (void) v;
}

void GLAPIENTRY _mesa_noop_Vertex4fv( const GLfloat *v )
{
   (void) v;
}

void GLAPIENTRY _mesa_noop_Vertex2f( GLfloat a, GLfloat b )
{
   (void) a; (void) b;
}

void GLAPIENTRY _mesa_noop_Vertex3f( GLfloat a, GLfloat b, GLfloat c )
{
   (void) a; (void) b; (void) c;
}

void GLAPIENTRY _mesa_noop_Vertex4f( GLfloat a, GLfloat b, GLfloat c, GLfloat d )
{
   (void) a; (void) b; (void) c; (void) d;
}

/* Similarly, these have no effect outside begin/end:
 */
void GLAPIENTRY _mesa_noop_EvalCoord1f( GLfloat a )
{
   (void) a;
}

void GLAPIENTRY _mesa_noop_EvalCoord1fv( const GLfloat *v )
{
   (void) v;
}

void GLAPIENTRY _mesa_noop_EvalCoord2f( GLfloat a, GLfloat b )
{
   (void) a; (void) b;
}

void GLAPIENTRY _mesa_noop_EvalCoord2fv( const GLfloat *v )
{
   (void) v;
}

void GLAPIENTRY _mesa_noop_EvalPoint1( GLint a )
{
   (void) a;
}

void GLAPIENTRY _mesa_noop_EvalPoint2( GLint a, GLint b )
{
   (void) a; (void) b;
}


/* Begin -- call into driver, should result in the vtxfmt being
 * swapped out:
 */
void GLAPIENTRY _mesa_noop_Begin( GLenum mode )
{
}


/* End -- just raise an error
 */
void GLAPIENTRY _mesa_noop_End( void )
{
   GET_CURRENT_CONTEXT(ctx);
   _mesa_error( ctx, GL_INVALID_OPERATION, "glEnd" );
}


/* Execute a glRectf() function.  This is not suitable for GL_COMPILE
 * modes (as the test for outside begin/end is not compiled),
 * but may be useful for drivers in circumstances which exclude
 * display list interactions.
 *
 * (None of the functions in this file are suitable for GL_COMPILE
 * modes).
 */
void GLAPIENTRY _mesa_noop_Rectf( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 )
{
   {
      GET_CURRENT_CONTEXT(ctx);
      ASSERT_OUTSIDE_BEGIN_END(ctx);
   }

   glBegin( GL_QUADS );
   glVertex2f( x1, y1 );
   glVertex2f( x2, y1 );
   glVertex2f( x2, y2 );
   glVertex2f( x1, y2 );
   glEnd();
}


/* Some very basic support for arrays.  Drivers without explicit array
 * support can hook these in, but still need to supply an array-elt
 * implementation.
 */
void GLAPIENTRY _mesa_noop_DrawArrays(GLenum mode, GLint start, GLsizei count)
{
   GET_CURRENT_CONTEXT(ctx);
   GLint i;

   if (!_mesa_validate_DrawArrays( ctx, mode, start, count ))
      return;

   glBegin(mode);
   for (i = start ; i < count ; i++)
      glArrayElement( i );
   glEnd();
}


void GLAPIENTRY _mesa_noop_DrawElements(GLenum mode, GLsizei count, GLenum type,
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

void GLAPIENTRY _mesa_noop_DrawRangeElements(GLenum mode,
				  GLuint start, GLuint end,
				  GLsizei count, GLenum type,
				  const GLvoid *indices)
{
   GET_CURRENT_CONTEXT(ctx);

   if (_mesa_validate_DrawRangeElements( ctx, mode,
					 start, end,
					 count, type, indices ))
      glDrawElements( mode, count, type, indices );
}

/*
 * Eval Mesh
 */

/* KW: If are compiling, we don't know whether eval will produce a
 *     vertex when it is run in the future.  If this is pure immediate
 *     mode, eval is a noop if neither vertex map is enabled.
 *
 *     Thus we need to have a check in the display list code or
 *     elsewhere for eval(1,2) vertices in the case where
 *     map(1,2)_vertex is disabled, and to purge those vertices from
 *     the vb.
 */
void GLAPIENTRY _mesa_noop_EvalMesh1( GLenum mode, GLint i1, GLint i2 )
{
   GET_CURRENT_CONTEXT(ctx);
   GLint i;
   GLfloat u, du;
   GLenum prim;

   switch (mode) {
   case GL_POINT:
      prim = GL_POINTS;
      break;
   case GL_LINE:
      prim = GL_LINE_STRIP;
      break;
   default:
      _mesa_error( ctx, GL_INVALID_ENUM, "glEvalMesh1(mode)" );
      return;
   }

   /* No effect if vertex maps disabled.
    */
   if (!ctx->Eval.Map1Vertex4 && 
       !ctx->Eval.Map1Vertex3 &&
       !(ctx->VertexProgram.Enabled && ctx->Eval.Map1Attrib[VERT_ATTRIB_POS]))
      return;

   du = ctx->Eval.MapGrid1du;
   u = ctx->Eval.MapGrid1u1 + i1 * du;

   glBegin( prim );
   for (i=i1;i<=i2;i++,u+=du) {
      glEvalCoord1f( u );
   }
   glEnd();
}



void GLAPIENTRY _mesa_noop_EvalMesh2( GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2 )
{
   GET_CURRENT_CONTEXT(ctx);
   GLfloat u, du, v, dv, v1, u1;
   GLint i, j;

   switch (mode) {
   case GL_POINT:
   case GL_LINE:
   case GL_FILL:
      break;
   default:
      _mesa_error( ctx, GL_INVALID_ENUM, "glEvalMesh2(mode)" );
      return;
   }

   /* No effect if vertex maps disabled.
    */
   if (!ctx->Eval.Map2Vertex4 && 
       !ctx->Eval.Map2Vertex3 &&
       !(ctx->VertexProgram.Enabled && ctx->Eval.Map2Attrib[VERT_ATTRIB_POS]))
      return;

   du = ctx->Eval.MapGrid2du;
   dv = ctx->Eval.MapGrid2dv;
   v1 = ctx->Eval.MapGrid2v1 + j1 * dv;
   u1 = ctx->Eval.MapGrid2u1 + i1 * du;

   switch (mode) {
   case GL_POINT:
      glBegin( GL_POINTS );
      for (v=v1,j=j1;j<=j2;j++,v+=dv) {
	 for (u=u1,i=i1;i<=i2;i++,u+=du) {
	    glEvalCoord2f(u, v );
	 }
      }
      glEnd();
      break;
   case GL_LINE:
      for (v=v1,j=j1;j<=j2;j++,v+=dv) {
	 glBegin( GL_LINE_STRIP );
	 for (u=u1,i=i1;i<=i2;i++,u+=du) {
	    glEvalCoord2f(u, v );
	 }
	 glEnd();
      }
      for (u=u1,i=i1;i<=i2;i++,u+=du) {
	 glBegin( GL_LINE_STRIP );
	 for (v=v1,j=j1;j<=j2;j++,v+=dv) {
	    glEvalCoord2f(u, v );
	 }
	 glEnd();
      }
      break;
   case GL_FILL:
      for (v=v1,j=j1;j<j2;j++,v+=dv) {
	 glBegin( GL_TRIANGLE_STRIP );
	 for (u=u1,i=i1;i<=i2;i++,u+=du) {
	    glEvalCoord2f(u, v );
	    glEvalCoord2f(u, v+dv );
	 }
	 glEnd();
      }
      break;
   default:
      _mesa_error( ctx, GL_INVALID_ENUM, "glEvalMesh2(mode)" );
      return;
   }
}



/* Build a vertexformat full of things to use outside begin/end pairs.
 * 
 * TODO -- build a whole dispatch table for this purpose, and likewise
 * for inside begin/end.
 */
void _mesa_noop_vtxfmt_init( GLvertexformat *vfmt )
{
   vfmt->ArrayElement = _ae_loopback_array_elt;	        /* generic helper */
   vfmt->Begin = _mesa_noop_Begin;
   vfmt->CallList = _mesa_CallList;
   vfmt->CallLists = _mesa_CallLists;
   vfmt->Color3f = _mesa_noop_Color3f;
   vfmt->Color3fv = _mesa_noop_Color3fv;
   vfmt->Color4f = _mesa_noop_Color4f;
   vfmt->Color4fv = _mesa_noop_Color4fv;
   vfmt->EdgeFlag = _mesa_noop_EdgeFlag;
   vfmt->EdgeFlagv = _mesa_noop_EdgeFlagv;
   vfmt->End = _mesa_noop_End;
   vfmt->EvalCoord1f = _mesa_noop_EvalCoord1f;
   vfmt->EvalCoord1fv = _mesa_noop_EvalCoord1fv;
   vfmt->EvalCoord2f = _mesa_noop_EvalCoord2f;
   vfmt->EvalCoord2fv = _mesa_noop_EvalCoord2fv;
   vfmt->EvalPoint1 = _mesa_noop_EvalPoint1;
   vfmt->EvalPoint2 = _mesa_noop_EvalPoint2;
   vfmt->FogCoordfEXT = _mesa_noop_FogCoordfEXT;
   vfmt->FogCoordfvEXT = _mesa_noop_FogCoordfvEXT;
   vfmt->Indexf = _mesa_noop_Indexf;
   vfmt->Indexfv = _mesa_noop_Indexfv;
   vfmt->Materialfv = _mesa_noop_Materialfv;
   vfmt->MultiTexCoord1fARB = _mesa_noop_MultiTexCoord1fARB;
   vfmt->MultiTexCoord1fvARB = _mesa_noop_MultiTexCoord1fvARB;
   vfmt->MultiTexCoord2fARB = _mesa_noop_MultiTexCoord2fARB;
   vfmt->MultiTexCoord2fvARB = _mesa_noop_MultiTexCoord2fvARB;
   vfmt->MultiTexCoord3fARB = _mesa_noop_MultiTexCoord3fARB;
   vfmt->MultiTexCoord3fvARB = _mesa_noop_MultiTexCoord3fvARB;
   vfmt->MultiTexCoord4fARB = _mesa_noop_MultiTexCoord4fARB;
   vfmt->MultiTexCoord4fvARB = _mesa_noop_MultiTexCoord4fvARB;
   vfmt->Normal3f = _mesa_noop_Normal3f;
   vfmt->Normal3fv = _mesa_noop_Normal3fv;
   vfmt->SecondaryColor3fEXT = _mesa_noop_SecondaryColor3fEXT;
   vfmt->SecondaryColor3fvEXT = _mesa_noop_SecondaryColor3fvEXT;
   vfmt->TexCoord1f = _mesa_noop_TexCoord1f;
   vfmt->TexCoord1fv = _mesa_noop_TexCoord1fv;
   vfmt->TexCoord2f = _mesa_noop_TexCoord2f;
   vfmt->TexCoord2fv = _mesa_noop_TexCoord2fv;
   vfmt->TexCoord3f = _mesa_noop_TexCoord3f;
   vfmt->TexCoord3fv = _mesa_noop_TexCoord3fv;
   vfmt->TexCoord4f = _mesa_noop_TexCoord4f;
   vfmt->TexCoord4fv = _mesa_noop_TexCoord4fv;
   vfmt->Vertex2f = _mesa_noop_Vertex2f;
   vfmt->Vertex2fv = _mesa_noop_Vertex2fv;
   vfmt->Vertex3f = _mesa_noop_Vertex3f;
   vfmt->Vertex3fv = _mesa_noop_Vertex3fv;
   vfmt->Vertex4f = _mesa_noop_Vertex4f;
   vfmt->Vertex4fv = _mesa_noop_Vertex4fv;
   vfmt->VertexAttrib1fNV = _mesa_noop_VertexAttrib1fNV;
   vfmt->VertexAttrib1fvNV = _mesa_noop_VertexAttrib1fvNV;
   vfmt->VertexAttrib2fNV = _mesa_noop_VertexAttrib2fNV;
   vfmt->VertexAttrib2fvNV = _mesa_noop_VertexAttrib2fvNV;
   vfmt->VertexAttrib3fNV = _mesa_noop_VertexAttrib3fNV;
   vfmt->VertexAttrib3fvNV = _mesa_noop_VertexAttrib3fvNV;
   vfmt->VertexAttrib4fNV = _mesa_noop_VertexAttrib4fNV;
   vfmt->VertexAttrib4fvNV = _mesa_noop_VertexAttrib4fvNV;

   vfmt->Rectf = _mesa_noop_Rectf;

   vfmt->DrawArrays = _mesa_noop_DrawArrays;
   vfmt->DrawElements = _mesa_noop_DrawElements;
   vfmt->DrawRangeElements = _mesa_noop_DrawRangeElements;
   vfmt->EvalMesh1 = _mesa_noop_EvalMesh1;
   vfmt->EvalMesh2 = _mesa_noop_EvalMesh2;
}
