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
#include "extensions.h"
#include "simple_list.h"
#include "mtypes.h"


#define F(x) (int)(unsigned long)&(((struct gl_extensions *)0)->x)
#define ON GL_TRUE
#define OFF GL_FALSE


static const struct {
   GLboolean enabled;
   const char *name;
   int flag_offset;
} default_extensions[] = {
   { OFF, "GL_ARB_depth_texture",              F(ARB_depth_texture) },
   { OFF, "GL_ARB_fragment_program",           F(ARB_fragment_program) },
   { OFF, "GL_ARB_imaging",                    F(ARB_imaging) },
   { OFF, "GL_ARB_multisample",                F(ARB_multisample) },
   { OFF, "GL_ARB_multitexture",               F(ARB_multitexture) },
   { OFF, "GL_ARB_occlusion_query",            F(ARB_occlusion_query) },
   { OFF, "GL_ARB_point_parameters",           F(EXT_point_parameters) },
   { OFF, "GL_ARB_point_sprite",               F(ARB_point_sprite) },
   { OFF, "GL_ARB_shadow",                     F(ARB_shadow) },
   { OFF, "GL_ARB_shadow_ambient",             F(SGIX_shadow_ambient) },
   { OFF, "GL_ARB_texture_border_clamp",       F(ARB_texture_border_clamp) },
   { OFF, "GL_ARB_texture_compression",        F(ARB_texture_compression) },
   { OFF, "GL_ARB_texture_cube_map",           F(ARB_texture_cube_map) },
   { OFF, "GL_ARB_texture_env_add",            F(EXT_texture_env_add) },
   { OFF, "GL_ARB_texture_env_combine",        F(ARB_texture_env_combine) },
   { OFF, "GL_ARB_texture_env_crossbar",       F(ARB_texture_env_crossbar) },
   { OFF, "GL_ARB_texture_env_dot3",           F(ARB_texture_env_dot3) },
   { OFF, "GL_ARB_texture_mirrored_repeat",    F(ARB_texture_mirrored_repeat)},
   { OFF, "GL_ARB_texture_non_power_of_two",   F(ARB_texture_non_power_of_two)},
   { ON,  "GL_ARB_transpose_matrix",           0 },
   { OFF, "GL_ARB_vertex_buffer_object",       F(ARB_vertex_buffer_object) },
   { OFF, "GL_ARB_vertex_program",             F(ARB_vertex_program) },
   { ON,  "GL_ARB_window_pos",                 F(ARB_window_pos) },
   { ON,  "GL_EXT_abgr",                       0 },
   { ON,  "GL_EXT_bgra",                       0 },
   { OFF, "GL_EXT_blend_color",                F(EXT_blend_color) },
   { OFF, "GL_EXT_blend_func_separate",        F(EXT_blend_func_separate) },
   { OFF, "GL_EXT_blend_logic_op",             F(EXT_blend_logic_op) },
   { OFF, "GL_EXT_blend_minmax",               F(EXT_blend_minmax) },
   { OFF, "GL_EXT_blend_subtract",             F(EXT_blend_subtract) },
   { ON,  "GL_EXT_clip_volume_hint",           0 },
   { ON,  "GL_EXT_compiled_vertex_array",      F(EXT_compiled_vertex_array) },
   { OFF, "GL_EXT_convolution",                F(EXT_convolution) },
   { ON,  "GL_EXT_copy_texture",               0 },
   { OFF, "GL_EXT_depth_bounds_test",          F(EXT_depth_bounds_test) },
   { ON,  "GL_EXT_draw_range_elements",        0 },
   { OFF, "GL_EXT_fog_coord",                  F(EXT_fog_coord) },
   { OFF, "GL_EXT_histogram",                  F(EXT_histogram) },
   { OFF, "GL_EXT_multi_draw_arrays",          F(EXT_multi_draw_arrays) },
   { ON,  "GL_EXT_packed_pixels",              0 },
   { OFF, "GL_EXT_paletted_texture",           F(EXT_paletted_texture) },
   { OFF, "GL_EXT_point_parameters",           F(EXT_point_parameters) },
   { ON,  "GL_EXT_polygon_offset",             0 },
   { ON,  "GL_EXT_rescale_normal",             0 },
   { OFF, "GL_EXT_secondary_color",            F(EXT_secondary_color) },
   { ON,  "GL_EXT_separate_specular_color",    0 },
   { OFF, "GL_EXT_shadow_funcs",               F(EXT_shadow_funcs) },
   { OFF, "GL_EXT_shared_texture_palette",     F(EXT_shared_texture_palette) },
   { OFF, "GL_EXT_stencil_two_side",           F(EXT_stencil_two_side) },
   { OFF, "GL_EXT_stencil_wrap",               F(EXT_stencil_wrap) },
   { ON,  "GL_EXT_subtexture",                 0 },
   { ON,  "GL_EXT_texture",                    0 },
   { ON,  "GL_EXT_texture3D",                  F(EXT_texture3D) },
   { OFF, "GL_EXT_texture_compression_s3tc",   F(EXT_texture_compression_s3tc) },
   { ON,  "GL_EXT_texture_edge_clamp",         F(SGIS_texture_edge_clamp) },
   { OFF, "GL_EXT_texture_env_add",            F(EXT_texture_env_add) },
   { OFF, "GL_EXT_texture_env_combine",        F(EXT_texture_env_combine) },
   { OFF, "GL_EXT_texture_env_dot3",           F(EXT_texture_env_dot3) },
   { OFF, "GL_EXT_texture_filter_anisotropic", F(EXT_texture_filter_anisotropic) },
   { OFF, "GL_EXT_texture_lod_bias",           F(EXT_texture_lod_bias) },
   { OFF, "GL_EXT_texture_mirror_clamp",       F(EXT_texture_mirror_clamp) },
   { ON,  "GL_EXT_texture_object",             0 },
   { OFF, "GL_EXT_texture_rectangle",          F(NV_texture_rectangle) },
   { ON,  "GL_EXT_vertex_array",               0 },
   { OFF, "GL_EXT_vertex_array_set",           F(EXT_vertex_array_set) },
   { OFF, "GL_3DFX_texture_compression_FXT1",  F(TDFX_texture_compression_FXT1) },
   { OFF, "GL_APPLE_client_storage",           F(APPLE_client_storage) },
   { ON,  "GL_APPLE_packed_pixels",            0 },
   { OFF, "GL_ATI_texture_env_combine3",       F(ATI_texture_env_combine3)},
   { OFF, "GL_ATI_texture_mirror_once",        F(ATI_texture_mirror_once)},
   { OFF, "GL_HP_occlusion_test",              F(HP_occlusion_test) },
   { OFF, "GL_IBM_multimode_draw_arrays",      F(IBM_multimode_draw_arrays) },
   { ON,  "GL_IBM_rasterpos_clip",             F(IBM_rasterpos_clip) },
   { OFF, "GL_IBM_texture_mirrored_repeat",    F(ARB_texture_mirrored_repeat)},
   { OFF, "GL_INGR_blend_func_separate",       F(EXT_blend_func_separate) },
   { OFF, "GL_MESA_pack_invert",               F(MESA_pack_invert) },
   { OFF, "GL_MESA_packed_depth_stencil",      F(MESA_packed_depth_stencil) },
   { OFF, "GL_MESA_program_debug",             F(MESA_program_debug) },
   { OFF, "GL_MESA_resize_buffers",            F(MESA_resize_buffers) },
   { OFF, "GL_MESA_ycbcr_texture",             F(MESA_ycbcr_texture) },
   { ON,  "GL_MESA_window_pos",                F(ARB_window_pos) },
   { OFF, "GL_NV_blend_square",                F(NV_blend_square) },
   { OFF, "GL_NV_fragment_program",            F(NV_fragment_program) },
   { ON,  "GL_NV_light_max_exponent",          0 },
   { OFF, "GL_NV_point_sprite",                F(NV_point_sprite) },
   { OFF, "GL_NV_texture_rectangle",           F(NV_texture_rectangle) },
   { ON,  "GL_NV_texgen_reflection",           0 },
   { OFF, "GL_NV_vertex_program",              F(NV_vertex_program) },
   { OFF, "GL_NV_vertex_program1_1",           F(NV_vertex_program1_1) },
   { OFF, "GL_SGI_color_matrix",               F(SGI_color_matrix) },
   { OFF, "GL_SGI_color_table",                F(SGI_color_table) },
   { OFF, "GL_SGI_texture_color_table",        F(SGI_texture_color_table) },
   { OFF, "GL_SGIS_generate_mipmap",           F(SGIS_generate_mipmap) },
   { OFF, "GL_SGIS_pixel_texture",             F(SGIS_pixel_texture) },
   { OFF, "GL_SGIS_texture_border_clamp",      F(ARB_texture_border_clamp) },
   { ON,  "GL_SGIS_texture_edge_clamp",        F(SGIS_texture_edge_clamp) },
   { ON,  "GL_SGIS_texture_lod",               0 },
   { OFF, "GL_SGIX_depth_texture",             F(SGIX_depth_texture) },
   { OFF, "GL_SGIX_pixel_texture",             F(SGIX_pixel_texture) },
   { OFF, "GL_SGIX_shadow",                    F(SGIX_shadow) },
   { OFF, "GL_SGIX_shadow_ambient",            F(SGIX_shadow_ambient) },
   { OFF, "GL_SUN_multi_draw_arrays",          F(EXT_multi_draw_arrays) },
   { OFF, "GL_S3_s3tc",                        F(S3_s3tc) },
};



/**
 * Enable all extensions suitable for a software-only renderer.
 * This is a convenience function used by the XMesa, OSMesa, GGI drivers, etc.
 */
void
_mesa_enable_sw_extensions(GLcontext *ctx)
{
   ctx->Extensions.ARB_depth_texture = GL_TRUE;
#if FEATURE_ARB_fragment_program
   /*ctx->Extensions.ARB_fragment_program = GL_TRUE;*/
#endif
   ctx->Extensions.ARB_imaging = GL_TRUE;
   ctx->Extensions.ARB_multitexture = GL_TRUE;
#if FEATURE_ARB_occlusion_query
   ctx->Extensions.ARB_occlusion_query = GL_TRUE;
#endif
   ctx->Extensions.ARB_point_sprite = GL_TRUE;
   ctx->Extensions.ARB_shadow = GL_TRUE;
   ctx->Extensions.ARB_texture_border_clamp = GL_TRUE;
   ctx->Extensions.ARB_texture_cube_map = GL_TRUE;
   ctx->Extensions.ARB_texture_env_combine = GL_TRUE;
   ctx->Extensions.ARB_texture_env_crossbar = GL_TRUE;
   ctx->Extensions.ARB_texture_env_dot3 = GL_TRUE;
   ctx->Extensions.ARB_texture_mirrored_repeat = GL_TRUE;
   ctx->Extensions.ARB_texture_non_power_of_two = GL_TRUE;
#if FEATURE_ARB_vertex_program
   /*ctx->Extensions.ARB_vertex_program = GL_TRUE;*/
#endif
#if FEATURE_ARB_vertex_buffer_object
   ctx->Extensions.ARB_vertex_buffer_object = GL_TRUE;
#endif
   ctx->Extensions.ATI_texture_env_combine3 = GL_TRUE;
   ctx->Extensions.ATI_texture_mirror_once = GL_TRUE;
   ctx->Extensions.EXT_blend_color = GL_TRUE;
   ctx->Extensions.EXT_blend_func_separate = GL_TRUE;
   ctx->Extensions.EXT_blend_logic_op = GL_TRUE;
   ctx->Extensions.EXT_blend_minmax = GL_TRUE;
   ctx->Extensions.EXT_blend_subtract = GL_TRUE;
   ctx->Extensions.EXT_convolution = GL_TRUE;
   ctx->Extensions.EXT_depth_bounds_test = GL_TRUE;
   ctx->Extensions.EXT_fog_coord = GL_TRUE;
   ctx->Extensions.EXT_histogram = GL_TRUE;
   ctx->Extensions.EXT_multi_draw_arrays = GL_TRUE;
   ctx->Extensions.EXT_paletted_texture = GL_TRUE;
   ctx->Extensions.EXT_point_parameters = GL_TRUE;
   ctx->Extensions.EXT_shadow_funcs = GL_TRUE;
   ctx->Extensions.EXT_secondary_color = GL_TRUE;
   ctx->Extensions.EXT_shared_texture_palette = GL_TRUE;
   ctx->Extensions.EXT_stencil_wrap = GL_TRUE;
   ctx->Extensions.EXT_stencil_two_side = GL_TRUE;
   ctx->Extensions.EXT_texture_env_add = GL_TRUE;
   ctx->Extensions.EXT_texture_env_combine = GL_TRUE;
   ctx->Extensions.EXT_texture_env_dot3 = GL_TRUE;
   ctx->Extensions.EXT_texture_mirror_clamp = GL_TRUE;
   ctx->Extensions.EXT_texture_lod_bias = GL_TRUE;
   ctx->Extensions.HP_occlusion_test = GL_TRUE;
   ctx->Extensions.IBM_multimode_draw_arrays = GL_TRUE;
   ctx->Extensions.MESA_pack_invert = GL_TRUE;
#if FEATURE_MESA_program_debug
   ctx->Extensions.MESA_program_debug = GL_TRUE;
#endif
   ctx->Extensions.MESA_resize_buffers = GL_TRUE;
   ctx->Extensions.MESA_ycbcr_texture = GL_TRUE;
   ctx->Extensions.NV_blend_square = GL_TRUE;
   /*ctx->Extensions.NV_light_max_exponent = GL_TRUE;*/
   ctx->Extensions.NV_point_sprite = GL_TRUE;
   ctx->Extensions.NV_texture_rectangle = GL_TRUE;
   /*ctx->Extensions.NV_texgen_reflection = GL_TRUE;*/
#if FEATURE_NV_vertex_program
   ctx->Extensions.NV_vertex_program = GL_TRUE;
   ctx->Extensions.NV_vertex_program1_1 = GL_TRUE;
#endif
#if FEATURE_NV_fragment_program
   ctx->Extensions.NV_fragment_program = GL_TRUE;
#endif
   ctx->Extensions.SGI_color_matrix = GL_TRUE;
   ctx->Extensions.SGI_color_table = GL_TRUE;
   ctx->Extensions.SGI_texture_color_table = GL_TRUE;
   ctx->Extensions.SGIS_generate_mipmap = GL_TRUE;
   ctx->Extensions.SGIS_pixel_texture = GL_TRUE;
   ctx->Extensions.SGIS_texture_edge_clamp = GL_TRUE;
   ctx->Extensions.SGIX_depth_texture = GL_TRUE;
   ctx->Extensions.SGIX_pixel_texture = GL_TRUE;
   ctx->Extensions.SGIX_shadow = GL_TRUE;
   ctx->Extensions.SGIX_shadow_ambient = GL_TRUE;
}


/**
 * Enable GL_ARB_imaging and all the EXT extensions that are subsets of it.
 */
void
_mesa_enable_imaging_extensions(GLcontext *ctx)
{
   ctx->Extensions.ARB_imaging = GL_TRUE;
   ctx->Extensions.EXT_blend_color = GL_TRUE;
   ctx->Extensions.EXT_blend_minmax = GL_TRUE;
   ctx->Extensions.EXT_blend_subtract = GL_TRUE;
   ctx->Extensions.EXT_convolution = GL_TRUE;
   ctx->Extensions.EXT_histogram = GL_TRUE;
   ctx->Extensions.SGI_color_matrix = GL_TRUE;
   ctx->Extensions.SGI_color_table = GL_TRUE;
}



/**
 * Enable all OpenGL 1.3 features and extensions.
 * A convenience function to be called by drivers.
 */
void
_mesa_enable_1_3_extensions(GLcontext *ctx)
{
   ctx->Extensions.ARB_multisample = GL_TRUE;
   ctx->Extensions.ARB_multitexture = GL_TRUE;
   ctx->Extensions.ARB_texture_border_clamp = GL_TRUE;
   ctx->Extensions.ARB_texture_compression = GL_TRUE;
   ctx->Extensions.ARB_texture_cube_map = GL_TRUE;
   ctx->Extensions.ARB_texture_env_combine = GL_TRUE;
   ctx->Extensions.ARB_texture_env_dot3 = GL_TRUE;
   ctx->Extensions.EXT_texture_env_add = GL_TRUE;
   /*ctx->Extensions.ARB_transpose_matrix = GL_TRUE;*/
}



/**
 * Enable all OpenGL 1.4 features and extensions.
 * A convenience function to be called by drivers.
 */
void
_mesa_enable_1_4_extensions(GLcontext *ctx)
{
   ctx->Extensions.ARB_depth_texture = GL_TRUE;
   ctx->Extensions.ARB_fragment_program = GL_TRUE;
   ctx->Extensions.ARB_shadow = GL_TRUE;
   ctx->Extensions.ARB_texture_env_crossbar = GL_TRUE;
   ctx->Extensions.ARB_texture_mirrored_repeat = GL_TRUE;
   ctx->Extensions.ARB_vertex_program = GL_TRUE;
   ctx->Extensions.ARB_window_pos = GL_TRUE;
   ctx->Extensions.EXT_blend_color = GL_TRUE;
   ctx->Extensions.EXT_blend_func_separate = GL_TRUE;
   ctx->Extensions.EXT_blend_logic_op = GL_TRUE;
   ctx->Extensions.EXT_blend_minmax = GL_TRUE;
   ctx->Extensions.EXT_blend_subtract = GL_TRUE;
   ctx->Extensions.EXT_fog_coord = GL_TRUE;
   ctx->Extensions.EXT_multi_draw_arrays = GL_TRUE;
   ctx->Extensions.EXT_point_parameters = GL_TRUE;
   ctx->Extensions.EXT_secondary_color = GL_TRUE;
   ctx->Extensions.EXT_stencil_wrap = GL_TRUE;
   ctx->Extensions.EXT_texture_lod_bias = GL_TRUE;
   ctx->Extensions.SGIS_generate_mipmap = GL_TRUE;
}


/**
 * Enable all OpenGL 1.5 features and extensions.
 * A convenience function to be called by drivers.
 */
void
_mesa_enable_1_5_extensions(GLcontext *ctx)
{
   ctx->Extensions.ARB_occlusion_query = GL_TRUE;
   ctx->Extensions.ARB_vertex_buffer_object = GL_TRUE;
   ctx->Extensions.ARB_texture_non_power_of_two = GL_TRUE;
   ctx->Extensions.EXT_shadow_funcs = GL_TRUE;
}


/**
 * Either enable or disable the named extension.
 */
static void
set_extension( GLcontext *ctx, const char *name, GLboolean state )
{
   GLboolean *base = (GLboolean *) &ctx->Extensions;
   GLuint i;

   if (ctx->Extensions.String) {
      /* The string was already queried - can't change it now! */
      _mesa_problem(ctx, "Trying to enable/disable extension after glGetString(GL_EXTENSIONS): %s", name);
      return;
   }

   for (i = 0 ; i < Elements(default_extensions) ; i++) {
      if (_mesa_strcmp(default_extensions[i].name, name) == 0) {
         if (default_extensions[i].flag_offset) {
            GLboolean *enabled = base + default_extensions[i].flag_offset;
            *enabled = state;
         }
         return;
      }
   }
   _mesa_problem(ctx, "Trying to enable unknown extension: %s", name);
}


/**
 * Enable the named extension.
 * Typically called by drivers.
 */
void
_mesa_enable_extension( GLcontext *ctx, const char *name )
{
   set_extension(ctx, name, GL_TRUE);
}


/**
 * Disable the named extension.
 * XXX is this really needed???
 */
void
_mesa_disable_extension( GLcontext *ctx, const char *name )
{
   set_extension(ctx, name, GL_FALSE);
}


/**
 * Test if the named extension is enabled in this context.
 */
GLboolean
_mesa_extension_is_enabled( GLcontext *ctx, const char *name )
{
   const GLboolean *base = (const GLboolean *) &ctx->Extensions;
   GLuint i;

   for (i = 0 ; i < Elements(default_extensions) ; i++) {
      if (_mesa_strcmp(default_extensions[i].name, name) == 0) {
         if (!default_extensions[i].flag_offset)
            return GL_TRUE;
         return *(base + default_extensions[i].flag_offset);
      }
   }
   return GL_FALSE;
}


/**
 * Run through the default_extensions array above and set the
 * ctx->Extensions.ARB/EXT_* flags accordingly.
 * To be called during context initialization.
 */
void
_mesa_init_extensions( GLcontext *ctx )
{
   GLboolean *base = (GLboolean *) &ctx->Extensions;
   GLuint i;

   for (i = 0 ; i < Elements(default_extensions) ; i++) {
      if (default_extensions[i].enabled &&
          default_extensions[i].flag_offset) {
         *(base + default_extensions[i].flag_offset) = GL_TRUE;
      }
   }
}


/**
 * Construct the GL_EXTENSIONS string.  Called the first time that
 * glGetString(GL_EXTENSIONS) is called.
 */
GLubyte *
_mesa_make_extension_string( GLcontext *ctx )
{
   const GLboolean *base = (const GLboolean *) &ctx->Extensions;
   GLuint extStrLen = 0;
   GLubyte *s;
   GLuint i;

   /* first, compute length of the extension string */
   for (i = 0 ; i < Elements(default_extensions) ; i++) {
      if (!default_extensions[i].flag_offset ||
          *(base + default_extensions[i].flag_offset)) {
         extStrLen += _mesa_strlen(default_extensions[i].name) + 1;
      }
   }
   s = (GLubyte *) _mesa_malloc(extStrLen);

   /* second, build the extension string */
   extStrLen = 0;
   for (i = 0 ; i < Elements(default_extensions) ; i++) {
      if (!default_extensions[i].flag_offset ||
          *(base + default_extensions[i].flag_offset)) {
         GLuint len = _mesa_strlen(default_extensions[i].name);
         _mesa_memcpy(s + extStrLen, default_extensions[i].name, len);
         extStrLen += len;
         s[extStrLen] = (GLubyte) ' ';
         extStrLen++;
      }
   }
   ASSERT(extStrLen > 0);

   s[extStrLen - 1] = 0;

   return s;
}
