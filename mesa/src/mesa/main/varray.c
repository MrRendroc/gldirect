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
#include "imports.h"
#include "context.h"
#include "enable.h"
#include "enums.h"
#include "dlist.h"
#include "texstate.h"
#include "mtypes.h"
#include "varray.h"


#ifndef GL_BOOLEAN
#define GL_BOOLEAN 0x9999
#endif


/**
 * Update the fields of a vertex array structure.
 * We need to do a few special things for arrays that live in
 * vertex buffer objects.
 */
static void
update_array(GLcontext *ctx, struct gl_client_array *array,
             GLuint dirtyFlag, GLsizei elementSize,
             GLint size, GLenum type,
             GLsizei stride, GLboolean normalized, const GLvoid *ptr)
{
   array->Size = size;
   array->Type = type;
   array->Stride = stride;
   array->StrideB = stride ? stride : elementSize;
   array->Normalized = normalized;
   array->Ptr = (const GLubyte *) ptr;
#if FEATURE_ARB_vertex_buffer_object
   array->BufferObj->RefCount--;
   /* XXX free buffer object if RefCount == 0 ? */
   array->BufferObj = ctx->Array.ArrayBufferObj;
   array->BufferObj->RefCount++;
   /* Compute the index of the last array element that's inside the buffer.
    * Later in glDrawArrays we'll check if start + count > _MaxElement to
    * be sure we won't go out of bounds.
    */
   if (ctx->Array.ArrayBufferObj->Name)
      array->_MaxElement = ((GLsizeiptrARB) ctx->Array.ArrayBufferObj->Size
                            - (GLsizeiptrARB) array->Ptr) / array->StrideB;
   else
#endif
      array->_MaxElement = 2 * 1000 * 1000 * 1000; /* just a big number */

   ctx->NewState |= _NEW_ARRAY;
   ctx->Array.NewState |= dirtyFlag;
}


void GLAPIENTRY
_mesa_VertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
   GLsizei elementSize;
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (size < 2 || size > 4) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glVertexPointer(size)" );
      return;
   }
   if (stride < 0) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glVertexPointer(stride)" );
      return;
   }

   if (MESA_VERBOSE&(VERBOSE_VARRAY|VERBOSE_API))
      _mesa_debug(ctx, "glVertexPointer( sz %d type %s stride %d )\n", size,
                  _mesa_lookup_enum_by_nr( type ), stride);

   /* always need to check that <type> is legal */
   switch (type) {
      case GL_SHORT:
         elementSize = size * sizeof(GLshort);
         break;
      case GL_INT:
         elementSize = size * sizeof(GLint);
         break;
      case GL_FLOAT:
         elementSize = size * sizeof(GLfloat);
         break;
      case GL_DOUBLE:
         elementSize = size * sizeof(GLdouble);
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glVertexPointer(type)" );
         return;
   }

   update_array(ctx, &ctx->Array.Vertex, _NEW_ARRAY_VERTEX,
                elementSize, size, type, stride, GL_FALSE, ptr);

   if (ctx->Driver.VertexPointer)
      ctx->Driver.VertexPointer( ctx, size, type, stride, ptr );
}


void GLAPIENTRY
_mesa_NormalPointer(GLenum type, GLsizei stride, const GLvoid *ptr )
{
   GLsizei elementSize;
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (stride < 0) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glNormalPointer(stride)" );
      return;
   }

   if (MESA_VERBOSE&(VERBOSE_VARRAY|VERBOSE_API))
      _mesa_debug(ctx, "glNormalPointer( type %s stride %d )\n",
                  _mesa_lookup_enum_by_nr( type ), stride);

   switch (type) {
      case GL_BYTE:
         elementSize = 3 * sizeof(GLbyte);
         break;
      case GL_SHORT:
         elementSize = 3 * sizeof(GLshort);
         break;
      case GL_INT:
         elementSize = 3 * sizeof(GLint);
         break;
      case GL_FLOAT:
         elementSize = 3 * sizeof(GLfloat);
         break;
      case GL_DOUBLE:
         elementSize = 3 * sizeof(GLdouble);
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glNormalPointer(type)" );
         return;
   }

   update_array(ctx, &ctx->Array.Normal, _NEW_ARRAY_NORMAL,
                elementSize, 3, type, stride, GL_FALSE, ptr);

   if (ctx->Driver.NormalPointer)
      ctx->Driver.NormalPointer( ctx, type, stride, ptr );
}


void GLAPIENTRY
_mesa_ColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
   GLsizei elementSize;
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (size < 3 || size > 4) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glColorPointer(size)" );
      return;
   }
   if (stride < 0) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glColorPointer(stride)" );
      return;
   }

   if (MESA_VERBOSE&(VERBOSE_VARRAY|VERBOSE_API))
      _mesa_debug(ctx, "glColorPointer( sz %d type %s stride %d )\n", size,
                  _mesa_lookup_enum_by_nr( type ), stride);

   switch (type) {
      case GL_BYTE:
         elementSize = size * sizeof(GLbyte);
         break;
      case GL_UNSIGNED_BYTE:
         elementSize = size * sizeof(GLubyte);
         break;
      case GL_SHORT:
         elementSize = size * sizeof(GLshort);
         break;
      case GL_UNSIGNED_SHORT:
         elementSize = size * sizeof(GLushort);
         break;
      case GL_INT:
         elementSize = size * sizeof(GLint);
         break;
      case GL_UNSIGNED_INT:
         elementSize = size * sizeof(GLuint);
         break;
      case GL_FLOAT:
         elementSize = size * sizeof(GLfloat);
         break;
      case GL_DOUBLE:
         elementSize = size * sizeof(GLdouble);
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glColorPointer(type)" );
         return;
   }

   update_array(ctx, &ctx->Array.Color, _NEW_ARRAY_COLOR0,
                elementSize, size, type, stride, GL_FALSE, ptr);

   if (ctx->Driver.ColorPointer)
      ctx->Driver.ColorPointer( ctx, size, type, stride, ptr );
}


void GLAPIENTRY
_mesa_FogCoordPointerEXT(GLenum type, GLsizei stride, const GLvoid *ptr)
{
   GLint elementSize;
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (stride < 0) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glFogCoordPointer(stride)" );
      return;
   }

   switch (type) {
      case GL_FLOAT:
         elementSize = sizeof(GLfloat);
         break;
      case GL_DOUBLE:
         elementSize = sizeof(GLdouble);
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glFogCoordPointer(type)" );
         return;
   }

   update_array(ctx, &ctx->Array.FogCoord, _NEW_ARRAY_FOGCOORD,
                elementSize, 1, type, stride, GL_FALSE, ptr);

   if (ctx->Driver.FogCoordPointer)
      ctx->Driver.FogCoordPointer( ctx, type, stride, ptr );
}


void GLAPIENTRY
_mesa_IndexPointer(GLenum type, GLsizei stride, const GLvoid *ptr)
{
   GLsizei elementSize;
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (stride < 0) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glIndexPointer(stride)" );
      return;
   }

   switch (type) {
      case GL_UNSIGNED_BYTE:
         elementSize = sizeof(GLubyte);
         break;
      case GL_SHORT:
         elementSize = sizeof(GLshort);
         break;
      case GL_INT:
         elementSize = sizeof(GLint);
         break;
      case GL_FLOAT:
         elementSize = sizeof(GLfloat);
         break;
      case GL_DOUBLE:
         elementSize = sizeof(GLdouble);
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glIndexPointer(type)" );
         return;
   }

   update_array(ctx, &ctx->Array.Index, _NEW_ARRAY_INDEX,
                elementSize, 1, type, stride, GL_FALSE, ptr);

   if (ctx->Driver.IndexPointer)
      ctx->Driver.IndexPointer( ctx, type, stride, ptr );
}


void GLAPIENTRY
_mesa_SecondaryColorPointerEXT(GLint size, GLenum type,
			       GLsizei stride, const GLvoid *ptr)
{
   GLsizei elementSize;
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (size != 3 && size != 4) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glSecondaryColorPointer(size)" );
      return;
   }
   if (stride < 0) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glSecondaryColorPointer(stride)" );
      return;
   }

   if (MESA_VERBOSE&(VERBOSE_VARRAY|VERBOSE_API))
      _mesa_debug(ctx, "glSecondaryColorPointer( sz %d type %s stride %d )\n",
                  size, _mesa_lookup_enum_by_nr( type ), stride);

   switch (type) {
      case GL_BYTE:
         elementSize = size * sizeof(GLbyte);
         break;
      case GL_UNSIGNED_BYTE:
         elementSize = size * sizeof(GLubyte);
         break;
      case GL_SHORT:
         elementSize = size * sizeof(GLshort);
         break;
      case GL_UNSIGNED_SHORT:
         elementSize = size * sizeof(GLushort);
         break;
      case GL_INT:
         elementSize = size * sizeof(GLint);
         break;
      case GL_UNSIGNED_INT:
         elementSize = size * sizeof(GLuint);
         break;
      case GL_FLOAT:
         elementSize = size * sizeof(GLfloat);
         break;
      case GL_DOUBLE:
         elementSize = size * sizeof(GLdouble);
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glSecondaryColorPointer(type)" );
         return;
   }

   update_array(ctx, &ctx->Array.SecondaryColor, _NEW_ARRAY_COLOR1,
                elementSize, size, type, stride, GL_FALSE, ptr);

   if (ctx->Driver.SecondaryColorPointer)
      ctx->Driver.SecondaryColorPointer( ctx, size, type, stride, ptr );
}


void GLAPIENTRY
_mesa_TexCoordPointer(GLint size, GLenum type, GLsizei stride,
                      const GLvoid *ptr)
{
   GLint elementSize;
   GET_CURRENT_CONTEXT(ctx);
   const GLuint unit = ctx->Array.ActiveTexture;
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (size < 1 || size > 4) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glTexCoordPointer(size)" );
      return;
   }
   if (stride < 0) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glTexCoordPointer(stride)" );
      return;
   }

   if (MESA_VERBOSE&(VERBOSE_VARRAY|VERBOSE_API))
      _mesa_debug(ctx, "glTexCoordPointer(unit %u sz %d type %s stride %d)\n",
                  unit, size, _mesa_lookup_enum_by_nr( type ), stride);

   /* always need to check that <type> is legal */
   switch (type) {
      case GL_SHORT:
         elementSize = size * sizeof(GLshort);
         break;
      case GL_INT:
         elementSize = size * sizeof(GLint);
         break;
      case GL_FLOAT:
         elementSize = size * sizeof(GLfloat);
         break;
      case GL_DOUBLE:
         elementSize = size * sizeof(GLdouble);
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glTexCoordPointer(type)" );
         return;
   }

   update_array(ctx, &ctx->Array.TexCoord[unit], _NEW_ARRAY_TEXCOORD(unit),
                elementSize, size, type, stride, GL_FALSE, ptr);

   if (ctx->Driver.TexCoordPointer)
      ctx->Driver.TexCoordPointer( ctx, size, type, stride, ptr );
}


void GLAPIENTRY
_mesa_EdgeFlagPointer(GLsizei stride, const GLvoid *ptr)
{
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (stride < 0) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glEdgeFlagPointer(stride)" );
      return;
   }

   update_array(ctx, &ctx->Array.EdgeFlag, _NEW_ARRAY_EDGEFLAG,
                sizeof(GLboolean), 1, GL_BOOLEAN, stride, GL_FALSE, ptr);

   if (ctx->Driver.EdgeFlagPointer)
      ctx->Driver.EdgeFlagPointer( ctx, stride, ptr );
}


#if FEATURE_NV_vertex_program
void GLAPIENTRY
_mesa_VertexAttribPointerNV(GLuint index, GLint size, GLenum type,
                            GLsizei stride, const GLvoid *ptr)
{
   GLsizei elementSize;
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END(ctx);

   if (index >= VERT_ATTRIB_MAX) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glVertexAttribPointerNV(index)");
      return;
   }

   if (size < 1 || size > 4) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glVertexAttribPointerNV(size)");
      return;
   }

   if (stride < 0) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glVertexAttribPointerNV(stride)");
      return;
   }

   if (type == GL_UNSIGNED_BYTE && size != 4) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glVertexAttribPointerNV(size!=4)");
      return;
   }

   /* check for valid 'type' and compute StrideB right away */
   switch (type) {
      case GL_UNSIGNED_BYTE:
         elementSize = size * sizeof(GLubyte);
         break;
      case GL_SHORT:
         elementSize = size * sizeof(GLshort);
         break;
      case GL_FLOAT:
         elementSize = size * sizeof(GLfloat);
         break;
      case GL_DOUBLE:
         elementSize = size * sizeof(GLdouble);
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttribPointerNV(type)" );
         return;
   }

   update_array(ctx, &ctx->Array.VertexAttrib[index], _NEW_ARRAY_ATTRIB(index),
                elementSize, size, type, stride, GL_FALSE, ptr);

   if (ctx->Driver.VertexAttribPointer)
      ctx->Driver.VertexAttribPointer( ctx, index, size, type, stride, ptr );
}
#endif


#if FEATURE_ARB_vertex_program
void GLAPIENTRY
_mesa_VertexAttribPointerARB(GLuint index, GLint size, GLenum type,
                             GLboolean normalized,
                             GLsizei stride, const GLvoid *ptr)
{
   GLsizei elementSize;
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END(ctx);

   if (index >= ctx->Const.MaxVertexProgramAttribs) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glVertexAttribPointerARB(index)");
      return;
   }

   if (size < 1 || size > 4) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glVertexAttribPointerARB(size)");
      return;
   }

   if (stride < 0) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glVertexAttribPointerARB(stride)");
      return;
   }

   if (type == GL_UNSIGNED_BYTE && size != 4) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glVertexAttribPointerARB(size!=4)");
      return;
   }

   /* check for valid 'type' and compute StrideB right away */
   /* NOTE: more types are supported here than in the NV extension */
   switch (type) {
      case GL_BYTE:
         elementSize = size * sizeof(GLbyte);
         break;
      case GL_UNSIGNED_BYTE:
         elementSize = size * sizeof(GLubyte);
         break;
      case GL_SHORT:
         elementSize = size * sizeof(GLshort);
         break;
      case GL_UNSIGNED_SHORT:
         elementSize = size * sizeof(GLushort);
         break;
      case GL_INT:
         elementSize = size * sizeof(GLint);
         break;
      case GL_UNSIGNED_INT:
         elementSize = size * sizeof(GLuint);
         break;
      case GL_FLOAT:
         elementSize = size * sizeof(GLfloat);
         break;
      case GL_DOUBLE:
         elementSize = size * sizeof(GLdouble);
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glVertexAttribPointerARB(type)" );
         return;
   }

   update_array(ctx, &ctx->Array.VertexAttrib[index], _NEW_ARRAY_ATTRIB(index),
                elementSize, size, type, stride, normalized, ptr);

   /* XXX fix
   if (ctx->Driver.VertexAttribPointer)
      ctx->Driver.VertexAttribPointer( ctx, index, size, type, stride, ptr );
   */
}
#endif


void GLAPIENTRY
_mesa_VertexPointerEXT(GLint size, GLenum type, GLsizei stride,
                       GLsizei count, const GLvoid *ptr)
{
   (void) count;
   _mesa_VertexPointer(size, type, stride, ptr);
}


void GLAPIENTRY
_mesa_NormalPointerEXT(GLenum type, GLsizei stride, GLsizei count,
                       const GLvoid *ptr)
{
   (void) count;
   _mesa_NormalPointer(type, stride, ptr);
}


void GLAPIENTRY
_mesa_ColorPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count,
                      const GLvoid *ptr)
{
   (void) count;
   _mesa_ColorPointer(size, type, stride, ptr);
}


void GLAPIENTRY
_mesa_IndexPointerEXT(GLenum type, GLsizei stride, GLsizei count,
                      const GLvoid *ptr)
{
   (void) count;
   _mesa_IndexPointer(type, stride, ptr);
}


void GLAPIENTRY
_mesa_TexCoordPointerEXT(GLint size, GLenum type, GLsizei stride,
                         GLsizei count, const GLvoid *ptr)
{
   (void) count;
   _mesa_TexCoordPointer(size, type, stride, ptr);
}


void GLAPIENTRY
_mesa_EdgeFlagPointerEXT(GLsizei stride, GLsizei count, const GLboolean *ptr)
{
   (void) count;
   _mesa_EdgeFlagPointer(stride, ptr);
}


void GLAPIENTRY
_mesa_InterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
   GET_CURRENT_CONTEXT(ctx);
   GLboolean tflag, cflag, nflag;  /* enable/disable flags */
   GLint tcomps, ccomps, vcomps;   /* components per texcoord, color, vertex */
   GLenum ctype = 0;               /* color type */
   GLint coffset = 0, noffset = 0, voffset;/* color, normal, vertex offsets */
   const GLint toffset = 0;        /* always zero */
   GLint defstride;                /* default stride */
   GLint c, f;
   GLint coordUnitSave;

   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   f = sizeof(GLfloat);
   c = f * ((4 * sizeof(GLubyte) + (f - 1)) / f);

   if (stride < 0) {
      _mesa_error( ctx, GL_INVALID_VALUE, "glInterleavedArrays(stride)" );
      return;
   }

   switch (format) {
      case GL_V2F:
         tflag = GL_FALSE;  cflag = GL_FALSE;  nflag = GL_FALSE;
         tcomps = 0;  ccomps = 0;  vcomps = 2;
         voffset = 0;
         defstride = 2*f;
         break;
      case GL_V3F:
         tflag = GL_FALSE;  cflag = GL_FALSE;  nflag = GL_FALSE;
         tcomps = 0;  ccomps = 0;  vcomps = 3;
         voffset = 0;
         defstride = 3*f;
         break;
      case GL_C4UB_V2F:
         tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_FALSE;
         tcomps = 0;  ccomps = 4;  vcomps = 2;
         ctype = GL_UNSIGNED_BYTE;
         coffset = 0;
         voffset = c;
         defstride = c + 2*f;
         break;
      case GL_C4UB_V3F:
         tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_FALSE;
         tcomps = 0;  ccomps = 4;  vcomps = 3;
         ctype = GL_UNSIGNED_BYTE;
         coffset = 0;
         voffset = c;
         defstride = c + 3*f;
         break;
      case GL_C3F_V3F:
         tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_FALSE;
         tcomps = 0;  ccomps = 3;  vcomps = 3;
         ctype = GL_FLOAT;
         coffset = 0;
         voffset = 3*f;
         defstride = 6*f;
         break;
      case GL_N3F_V3F:
         tflag = GL_FALSE;  cflag = GL_FALSE;  nflag = GL_TRUE;
         tcomps = 0;  ccomps = 0;  vcomps = 3;
         noffset = 0;
         voffset = 3*f;
         defstride = 6*f;
         break;
      case GL_C4F_N3F_V3F:
         tflag = GL_FALSE;  cflag = GL_TRUE;  nflag = GL_TRUE;
         tcomps = 0;  ccomps = 4;  vcomps = 3;
         ctype = GL_FLOAT;
         coffset = 0;
         noffset = 4*f;
         voffset = 7*f;
         defstride = 10*f;
         break;
      case GL_T2F_V3F:
         tflag = GL_TRUE;  cflag = GL_FALSE;  nflag = GL_FALSE;
         tcomps = 2;  ccomps = 0;  vcomps = 3;
         voffset = 2*f;
         defstride = 5*f;
         break;
      case GL_T4F_V4F:
         tflag = GL_TRUE;  cflag = GL_FALSE;  nflag = GL_FALSE;
         tcomps = 4;  ccomps = 0;  vcomps = 4;
         voffset = 4*f;
         defstride = 8*f;
         break;
      case GL_T2F_C4UB_V3F:
         tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_FALSE;
         tcomps = 2;  ccomps = 4;  vcomps = 3;
         ctype = GL_UNSIGNED_BYTE;
         coffset = 2*f;
         voffset = c+2*f;
         defstride = c+5*f;
         break;
      case GL_T2F_C3F_V3F:
         tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_FALSE;
         tcomps = 2;  ccomps = 3;  vcomps = 3;
         ctype = GL_FLOAT;
         coffset = 2*f;
         voffset = 5*f;
         defstride = 8*f;
         break;
      case GL_T2F_N3F_V3F:
         tflag = GL_TRUE;  cflag = GL_FALSE;  nflag = GL_TRUE;
         tcomps = 2;  ccomps = 0;  vcomps = 3;
         noffset = 2*f;
         voffset = 5*f;
         defstride = 8*f;
         break;
      case GL_T2F_C4F_N3F_V3F:
         tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_TRUE;
         tcomps = 2;  ccomps = 4;  vcomps = 3;
         ctype = GL_FLOAT;
         coffset = 2*f;
         noffset = 6*f;
         voffset = 9*f;
         defstride = 12*f;
         break;
      case GL_T4F_C4F_N3F_V4F:
         tflag = GL_TRUE;  cflag = GL_TRUE;  nflag = GL_TRUE;
         tcomps = 4;  ccomps = 4;  vcomps = 4;
         ctype = GL_FLOAT;
         coffset = 4*f;
         noffset = 8*f;
         voffset = 11*f;
         defstride = 15*f;
         break;
      default:
         _mesa_error( ctx, GL_INVALID_ENUM, "glInterleavedArrays(format)" );
         return;
   }

   if (stride==0) {
      stride = defstride;
   }

   _mesa_DisableClientState( GL_EDGE_FLAG_ARRAY );
   _mesa_DisableClientState( GL_INDEX_ARRAY );

   /* Texcoords */
   coordUnitSave = ctx->Array.ActiveTexture;
   if (tflag) {
      GLuint i=0;
      /* enable unit 0 texcoord array */
      _mesa_ClientActiveTextureARB( GL_TEXTURE0_ARB );
      _mesa_EnableClientState( GL_TEXTURE_COORD_ARRAY );
      _mesa_TexCoordPointer( tcomps, GL_FLOAT, stride,
                             (GLubyte *) pointer + i * toffset );
      /* disable all other texcoord arrays */
      for (i = 1; i < ctx->Const.MaxTextureCoordUnits; i++) {
         _mesa_ClientActiveTextureARB( (GLenum) (GL_TEXTURE0_ARB + i) );
         _mesa_DisableClientState( GL_TEXTURE_COORD_ARRAY );
      }
   }
   else {
      /* disable all texcoord arrays */
      GLuint i;
      for (i = 0; i < ctx->Const.MaxTextureCoordUnits; i++) {
         _mesa_ClientActiveTextureARB( (GLenum) (GL_TEXTURE0_ARB + i) );
         _mesa_DisableClientState( GL_TEXTURE_COORD_ARRAY );
      }
   }
   /* Restore texture coordinate unit index */
   _mesa_ClientActiveTextureARB( (GLenum) (GL_TEXTURE0_ARB + coordUnitSave) );


   /* Color */
   if (cflag) {
      _mesa_EnableClientState( GL_COLOR_ARRAY );
      _mesa_ColorPointer( ccomps, ctype, stride,
			  (GLubyte *) pointer + coffset );
   }
   else {
      _mesa_DisableClientState( GL_COLOR_ARRAY );
   }


   /* Normals */
   if (nflag) {
      _mesa_EnableClientState( GL_NORMAL_ARRAY );
      _mesa_NormalPointer( GL_FLOAT, stride, (GLubyte *) pointer + noffset );
   }
   else {
      _mesa_DisableClientState( GL_NORMAL_ARRAY );
   }

   /* Vertices */
   _mesa_EnableClientState( GL_VERTEX_ARRAY );
   _mesa_VertexPointer( vcomps, GL_FLOAT, stride,
			(GLubyte *) pointer + voffset );
}


void GLAPIENTRY
_mesa_LockArraysEXT(GLint first, GLsizei count)
{
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (MESA_VERBOSE & VERBOSE_API)
      _mesa_debug(ctx, "glLockArrays %d %d\n", first, count);

   if (first == 0 && count > 0 &&
       count <= (GLint) ctx->Const.MaxArrayLockSize) {
      ctx->Array.LockFirst = first;
      ctx->Array.LockCount = count;
   }
   else {
      ctx->Array.LockFirst = 0;
      ctx->Array.LockCount = 0;
   }

   ctx->NewState |= _NEW_ARRAY;
   ctx->Array.NewState |= _NEW_ARRAY_ALL;

   if (ctx->Driver.LockArraysEXT)
      ctx->Driver.LockArraysEXT( ctx, first, count );
}


void GLAPIENTRY
_mesa_UnlockArraysEXT( void )
{
   GET_CURRENT_CONTEXT(ctx);
   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   if (MESA_VERBOSE & VERBOSE_API)
      _mesa_debug(ctx, "glUnlockArrays\n");

   ctx->Array.LockFirst = 0;
   ctx->Array.LockCount = 0;
   ctx->NewState |= _NEW_ARRAY;
   ctx->Array.NewState |= _NEW_ARRAY_ALL;

   if (ctx->Driver.UnlockArraysEXT)
      ctx->Driver.UnlockArraysEXT( ctx );
}


/* GL_EXT_multi_draw_arrays */
/* Somebody forgot to spec the first and count parameters as const! <sigh> */
void GLAPIENTRY
_mesa_MultiDrawArraysEXT( GLenum mode, GLint *first,
                          GLsizei *count, GLsizei primcount )
{
   GET_CURRENT_CONTEXT(ctx);
   GLint i;

   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   for (i = 0; i < primcount; i++) {
      if (count[i] > 0) {
         (ctx->Exec->DrawArrays)(mode, first[i], count[i]);
      }
   }
}


/* GL_EXT_multi_draw_arrays */
void GLAPIENTRY
_mesa_MultiDrawElementsEXT( GLenum mode, const GLsizei *count, GLenum type,
                            const GLvoid **indices, GLsizei primcount )
{
   GET_CURRENT_CONTEXT(ctx);
   GLint i;

   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   for (i = 0; i < primcount; i++) {
      if (count[i] > 0) {
         (ctx->Exec->DrawElements)(mode, count[i], type, indices[i]);
      }
   }
}


/* GL_IBM_multimode_draw_arrays */
void GLAPIENTRY
_mesa_MultiModeDrawArraysIBM( const GLenum * mode, const GLint * first,
			      const GLsizei * count,
			      GLsizei primcount, GLint modestride )
{
   GET_CURRENT_CONTEXT(ctx);
   GLint i;

   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   for ( i = 0 ; i < primcount ; i++ ) {
      if ( count[i] > 0 ) {
         GLenum m = *((GLenum *) ((GLubyte *) mode + i * modestride));
	 (ctx->Exec->DrawArrays)( m, first[i], count[i] );
      }
   }
}


/* GL_IBM_multimode_draw_arrays */
void GLAPIENTRY
_mesa_MultiModeDrawElementsIBM( const GLenum * mode, const GLsizei * count,
				GLenum type, const GLvoid * const * indices,
				GLsizei primcount, GLint modestride )
{
   GET_CURRENT_CONTEXT(ctx);
   GLint i;

   ASSERT_OUTSIDE_BEGIN_END_AND_FLUSH(ctx);

   /* XXX not sure about ARB_vertex_buffer_object handling here */

   for ( i = 0 ; i < primcount ; i++ ) {
      if ( count[i] > 0 ) {
         GLenum m = *((GLenum *) ((GLubyte *) mode + i * modestride));
	 (ctx->Exec->DrawElements)( m, count[i], type, indices[i] );
      }
   }
}


/**********************************************************************/
/*****                      Initialization                        *****/
/**********************************************************************/

void 
_mesa_init_varray( GLcontext * ctx )
{
   GLuint i;

   /* Vertex arrays */
   ctx->Array.Vertex.Size = 4;
   ctx->Array.Vertex.Type = GL_FLOAT;
   ctx->Array.Vertex.Stride = 0;
   ctx->Array.Vertex.StrideB = 0;
   ctx->Array.Vertex.Ptr = NULL;
   ctx->Array.Vertex.Enabled = GL_FALSE;
   ctx->Array.Vertex.Flags = CA_CLIENT_DATA;
   ctx->Array.Normal.Type = GL_FLOAT;
   ctx->Array.Normal.Stride = 0;
   ctx->Array.Normal.StrideB = 0;
   ctx->Array.Normal.Ptr = NULL;
   ctx->Array.Normal.Enabled = GL_FALSE;
   ctx->Array.Normal.Flags = CA_CLIENT_DATA;
   ctx->Array.Color.Size = 4;
   ctx->Array.Color.Type = GL_FLOAT;
   ctx->Array.Color.Stride = 0;
   ctx->Array.Color.StrideB = 0;
   ctx->Array.Color.Ptr = NULL;
   ctx->Array.Color.Enabled = GL_FALSE;
   ctx->Array.Color.Flags = CA_CLIENT_DATA;
   ctx->Array.SecondaryColor.Size = 4;
   ctx->Array.SecondaryColor.Type = GL_FLOAT;
   ctx->Array.SecondaryColor.Stride = 0;
   ctx->Array.SecondaryColor.StrideB = 0;
   ctx->Array.SecondaryColor.Ptr = NULL;
   ctx->Array.SecondaryColor.Enabled = GL_FALSE;
   ctx->Array.SecondaryColor.Flags = CA_CLIENT_DATA;
   ctx->Array.FogCoord.Size = 1;
   ctx->Array.FogCoord.Type = GL_FLOAT;
   ctx->Array.FogCoord.Stride = 0;
   ctx->Array.FogCoord.StrideB = 0;
   ctx->Array.FogCoord.Ptr = NULL;
   ctx->Array.FogCoord.Enabled = GL_FALSE;
   ctx->Array.FogCoord.Flags = CA_CLIENT_DATA;
   ctx->Array.Index.Type = GL_FLOAT;
   ctx->Array.Index.Stride = 0;
   ctx->Array.Index.StrideB = 0;
   ctx->Array.Index.Ptr = NULL;
   ctx->Array.Index.Enabled = GL_FALSE;
   ctx->Array.Index.Flags = CA_CLIENT_DATA;
   for (i = 0; i < MAX_TEXTURE_UNITS; i++) {
      ctx->Array.TexCoord[i].Size = 4;
      ctx->Array.TexCoord[i].Type = GL_FLOAT;
      ctx->Array.TexCoord[i].Stride = 0;
      ctx->Array.TexCoord[i].StrideB = 0;
      ctx->Array.TexCoord[i].Ptr = NULL;
      ctx->Array.TexCoord[i].Enabled = GL_FALSE;
      ctx->Array.TexCoord[i].Flags = CA_CLIENT_DATA;
   }
   ctx->Array.EdgeFlag.Stride = 0;
   ctx->Array.EdgeFlag.StrideB = 0;
   ctx->Array.EdgeFlag.Ptr = NULL;
   ctx->Array.EdgeFlag.Enabled = GL_FALSE;
   ctx->Array.EdgeFlag.Flags = CA_CLIENT_DATA;
   ctx->Array.ActiveTexture = 0;   /* GL_ARB_multitexture */
   for (i = 0; i < VERT_ATTRIB_MAX; i++) {
      ctx->Array.VertexAttrib[i].Size = 4;
      ctx->Array.VertexAttrib[i].Type = GL_FLOAT;
      ctx->Array.VertexAttrib[i].Stride = 0;
      ctx->Array.VertexAttrib[i].StrideB = 0;
      ctx->Array.VertexAttrib[i].Ptr = NULL;
      ctx->Array.VertexAttrib[i].Enabled = GL_FALSE;
      ctx->Array.VertexAttrib[i].Flags = CA_CLIENT_DATA;
   }
}
