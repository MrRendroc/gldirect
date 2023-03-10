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

/**
 * \file nvvertparse.c
 * NVIDIA vertex program parser.
 * \author Brian Paul
 */

/*
 * Regarding GL_NV_vertex_program, GL_NV_vertex_program1_1:
 *
 * Portions of this software may use or implement intellectual
 * property owned and licensed by NVIDIA Corporation. NVIDIA disclaims
 * any and all warranties with respect to such intellectual property,
 * including any use thereof or modifications thereto.
 */

#include "glheader.h"
#include "context.h"
#include "hash.h"
#include "imports.h"
#include "macros.h"
#include "mtypes.h"
#include "nvprogram.h"
#include "nvvertparse.h"
#include "nvvertprog.h"
#include "program.h"


/**
 * Current parsing state.  This structure is passed among the parsing
 * functions and keeps track of the current parser position and various
 * program attributes.
 */
struct parse_state {
   GLcontext *ctx;
   const GLubyte *start;
   const GLubyte *pos;
   const GLubyte *curLine;
   GLboolean isStateProgram;
   GLboolean isPositionInvariant;
   GLboolean isVersion1_1;
   GLuint inputsRead;
   GLuint outputsWritten;
   GLboolean anyProgRegsWritten;
   GLuint numInst;                 /* number of instructions parsed */
};


/*
 * Called whenever we find an error during parsing.
 */
static void
record_error(struct parse_state *parseState, const char *msg, int lineNo)
{
#ifdef DEBUG
   GLint line, column;
   const GLubyte *lineStr;
   lineStr = _mesa_find_line_column(parseState->start,
                                    parseState->pos, &line, &column);
   _mesa_debug(parseState->ctx,
               "nvfragparse.c(%d): line %d, column %d:%s (%s)\n",
               lineNo, line, column, (char *) lineStr, msg);
   _mesa_free((void *) lineStr);
#else
   (void) lineNo;
#endif

   /* Check that no error was already recorded.  Only record the first one. */
   if (parseState->ctx->Program.ErrorString[0] == 0) {
      _mesa_set_program_error(parseState->ctx,
                              parseState->pos - parseState->start,
                              msg);
   }
}


#define RETURN_ERROR							\
do {									\
   record_error(parseState, "Unexpected end of input.", __LINE__);	\
   return GL_FALSE;							\
} while(0)

#define RETURN_ERROR1(msg)						\
do {									\
   record_error(parseState, msg, __LINE__);				\
   return GL_FALSE;							\
} while(0)

#define RETURN_ERROR2(msg1, msg2)					\
do {									\
   char err[1000];							\
   _mesa_sprintf(err, "%s %s", msg1, msg2);				\
   record_error(parseState, err, __LINE__);				\
   return GL_FALSE;							\
} while(0)





static GLboolean IsLetter(GLubyte b)
{
   return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z');
}


static GLboolean IsDigit(GLubyte b)
{
   return b >= '0' && b <= '9';
}


static GLboolean IsWhitespace(GLubyte b)
{
   return b == ' ' || b == '\t' || b == '\n' || b == '\r';
}


/**
 * Starting at 'str' find the next token.  A token can be an integer,
 * an identifier or punctuation symbol.
 * \return <= 0 we found an error, else, return number of characters parsed.
 */
static GLint
GetToken(struct parse_state *parseState, GLubyte *token)
{
   const GLubyte *str = parseState->pos;
   GLint i = 0, j = 0;

   token[0] = 0;

   /* skip whitespace and comments */
   while (str[i] && (IsWhitespace(str[i]) || str[i] == '#')) {
      if (str[i] == '#') {
         /* skip comment */
         while (str[i] && (str[i] != '\n' && str[i] != '\r')) {
            i++;
         }
         if (str[i] == '\n' || str[i] == '\r')
            parseState->curLine = str + i + 1;
      }
      else {
         /* skip whitespace */
         if (str[i] == '\n' || str[i] == '\r')
            parseState->curLine = str + i + 1;
         i++;
      }
   }

   if (str[i] == 0)
      return -i;

   /* try matching an integer */
   while (str[i] && IsDigit(str[i])) {
      token[j++] = str[i++];
   }
   if (j > 0 || !str[i]) {
      token[j] = 0;
      return i;
   }

   /* try matching an identifier */
   if (IsLetter(str[i])) {
      while (str[i] && (IsLetter(str[i]) || IsDigit(str[i]))) {
         token[j++] = str[i++];
      }
      token[j] = 0;
      return i;
   }

   /* punctuation character */
   if (str[i]) {
      token[0] = str[i++];
      token[1] = 0;
      return i;
   }

   /* end of input */
   token[0] = 0;
   return i;
}


/**
 * Get next token from input stream and increment stream pointer past token.
 */
static GLboolean
Parse_Token(struct parse_state *parseState, GLubyte *token)
{
   GLint i;
   i = GetToken(parseState, token);
   if (i <= 0) {
      parseState->pos += (-i);
      return GL_FALSE;
   }
   parseState->pos += i;
   return GL_TRUE;
}


/**
 * Get next token from input stream but don't increment stream pointer.
 */
static GLboolean
Peek_Token(struct parse_state *parseState, GLubyte *token)
{
   GLint i, len;
   i = GetToken(parseState, token);
   if (i <= 0) {
      parseState->pos += (-i);
      return GL_FALSE;
   }
   len = _mesa_strlen((const char *) token);
   parseState->pos += (i - len);
   return GL_TRUE;
}


/**
 * Try to match 'pattern' as the next token after any whitespace/comments.
 * Advance the current parsing position only if we match the pattern.
 * \return GL_TRUE if pattern is matched, GL_FALSE otherwise.
 */
static GLboolean
Parse_String(struct parse_state *parseState, const char *pattern)
{
   const GLubyte *m;
   GLint i;

   /* skip whitespace and comments */
   while (IsWhitespace(*parseState->pos) || *parseState->pos == '#') {
      if (*parseState->pos == '#') {
         while (*parseState->pos && (*parseState->pos != '\n' && *parseState->pos != '\r')) {
            parseState->pos += 1;
         }
         if (*parseState->pos == '\n' || *parseState->pos == '\r')
            parseState->curLine = parseState->pos + 1;
      }
      else {
         /* skip whitespace */
         if (*parseState->pos == '\n' || *parseState->pos == '\r')
            parseState->curLine = parseState->pos + 1;
         parseState->pos += 1;
      }
   }

   /* Try to match the pattern */
   m = parseState->pos;
   for (i = 0; pattern[i]; i++) {
      if (*m != (GLubyte) pattern[i])
         return GL_FALSE;
      m += 1;
   }
   parseState->pos = m;

   return GL_TRUE; /* success */
}


/**********************************************************************/

static const char *InputRegisters[MAX_NV_VERTEX_PROGRAM_INPUTS + 1] = {
   "OPOS", "WGHT", "NRML", "COL0", "COL1", "FOGC", "6", "7",
   "TEX0", "TEX1", "TEX2", "TEX3", "TEX4", "TEX5", "TEX6", "TEX7", NULL
};

static const char *OutputRegisters[MAX_NV_VERTEX_PROGRAM_OUTPUTS + 1] = {
   "HPOS", "COL0", "COL1", "BFC0", "BFC1", "FOGC", "PSIZ",
   "TEX0", "TEX1", "TEX2", "TEX3", "TEX4", "TEX5", "TEX6", "TEX7", NULL
};

/* NOTE: the order here must match opcodes in nvvertprog.h */
static const char *Opcodes[] = {
   "MOV", "LIT", "RCP", "RSQ", "EXP", "LOG", "MUL", "ADD", "DP3", "DP4",
   "DST", "MIN", "MAX", "SLT", "SGE", "MAD", "ARL", "DPH", "RCC", "SUB",
   "ABS", "END",
   /* GL_ARB_vertex_program */
   "FLR", "FRC", "EX2", "LG2", "POW", "XPD", "SWZ",
   NULL
};



/**
 * Parse a temporary register: Rnn
 */
static GLboolean
Parse_TempReg(struct parse_state *parseState, GLint *tempRegNum)
{
   GLubyte token[100];

   /* Should be 'R##' */
   if (!Parse_Token(parseState, token))
      RETURN_ERROR;
   if (token[0] != 'R')
      RETURN_ERROR1("Expected R##");

   if (IsDigit(token[1])) {
      GLint reg = _mesa_atoi((char *) (token + 1));
      if (reg >= MAX_NV_VERTEX_PROGRAM_TEMPS)
         RETURN_ERROR1("Bad temporary register name");
      *tempRegNum = reg;
   }
   else {
      RETURN_ERROR1("Bad temporary register name");
   }

   return GL_TRUE;
}


/**
 * Parse address register "A0.x"
 */
static GLboolean
Parse_AddrReg(struct parse_state *parseState)
{
   /* match 'A0' */
   if (!Parse_String(parseState, "A0"))
      RETURN_ERROR;

   /* match '.' */
   if (!Parse_String(parseState, "."))
      RETURN_ERROR;

   /* match 'x' */
   if (!Parse_String(parseState, "x"))
      RETURN_ERROR;

   return GL_TRUE;
}


/**
 * Parse absolute program parameter register "c[##]"
 */
static GLboolean
Parse_AbsParamReg(struct parse_state *parseState, GLint *regNum)
{
   GLubyte token[100];

   if (!Parse_String(parseState, "c"))
      RETURN_ERROR;

   if (!Parse_String(parseState, "["))
      RETURN_ERROR;

   if (!Parse_Token(parseState, token))
      RETURN_ERROR;

   if (IsDigit(token[0])) {
      /* a numbered program parameter register */
      GLint reg = _mesa_atoi((char *) token);
      if (reg >= MAX_NV_VERTEX_PROGRAM_PARAMS)
         RETURN_ERROR1("Bad program parameter number");
      *regNum = reg;
   }
   else {
      RETURN_ERROR;
   }

   if (!Parse_String(parseState, "]"))
      RETURN_ERROR;

   return GL_TRUE;
}


static GLboolean
Parse_ParamReg(struct parse_state *parseState, struct vp_src_register *srcReg)
{
   GLubyte token[100];

   if (!Parse_String(parseState, "c"))
      RETURN_ERROR;

   if (!Parse_String(parseState, "["))
      RETURN_ERROR;

   if (!Peek_Token(parseState, token))
      RETURN_ERROR;

   if (IsDigit(token[0])) {
      /* a numbered program parameter register */
      GLint reg;
      (void) Parse_Token(parseState, token);
      reg = _mesa_atoi((char *) token);
      if (reg >= MAX_NV_VERTEX_PROGRAM_PARAMS)
         RETURN_ERROR1("Bad program parameter number");
      srcReg->File = PROGRAM_ENV_PARAM;
      srcReg->Index = reg;
   }
   else if (_mesa_strcmp((const char *) token, "A0") == 0) {
      /* address register "A0.x" */
      if (!Parse_AddrReg(parseState))
         RETURN_ERROR;

      srcReg->RelAddr = GL_TRUE;
      srcReg->File = PROGRAM_ENV_PARAM;
      /* Look for +/-N offset */
      if (!Peek_Token(parseState, token))
         RETURN_ERROR;

      if (token[0] == '-' || token[0] == '+') {
         const GLubyte sign = token[0];
         (void) Parse_Token(parseState, token); /* consume +/- */

         /* an integer should be next */
         if (!Parse_Token(parseState, token))
            RETURN_ERROR;

         if (IsDigit(token[0])) {
            const GLint k = _mesa_atoi((char *) token);
            if (sign == '-') {
               if (k > 64)
                  RETURN_ERROR1("Bad address offset");
               srcReg->Index = -k;
            }
            else {
               if (k > 63)
                  RETURN_ERROR1("Bad address offset");
               srcReg->Index = k;
            }
         }
         else {
            RETURN_ERROR;
         }
      }
      else {
         /* probably got a ']', catch it below */
      }
   }
   else {
      RETURN_ERROR;
   }

   /* Match closing ']' */
   if (!Parse_String(parseState, "]"))
      RETURN_ERROR;

   return GL_TRUE;
}


/**
 * Parse v[#] or v[<name>]
 */
static GLboolean
Parse_AttribReg(struct parse_state *parseState, GLint *tempRegNum)
{
   GLubyte token[100];
   GLint j;

   /* Match 'v' */
   if (!Parse_String(parseState, "v"))
      RETURN_ERROR;

   /* Match '[' */
   if (!Parse_String(parseState, "["))
      RETURN_ERROR;

   /* match number or named register */
   if (!Parse_Token(parseState, token))
      RETURN_ERROR;

   if (parseState->isStateProgram && token[0] != '0')
      RETURN_ERROR1("Only v[0] accessible in vertex state programs");

   if (IsDigit(token[0])) {
      GLint reg = _mesa_atoi((char *) token);
      if (reg >= MAX_NV_VERTEX_PROGRAM_INPUTS)
         RETURN_ERROR1("Bad vertex attribute register name");
      *tempRegNum = reg;
   }
   else {
      for (j = 0; InputRegisters[j]; j++) {
         if (_mesa_strcmp((const char *) token, InputRegisters[j]) == 0) {
            *tempRegNum = j;
            break;
         }
      }
      if (!InputRegisters[j]) {
         /* unknown input register label */
         RETURN_ERROR2("Bad register name", token);
      }
   }

   /* Match '[' */
   if (!Parse_String(parseState, "]"))
      RETURN_ERROR;

   return GL_TRUE;
}


static GLboolean
Parse_OutputReg(struct parse_state *parseState, GLint *outputRegNum)
{
   GLubyte token[100];
   GLint start, j;

   /* Match 'o' */
   if (!Parse_String(parseState, "o"))
      RETURN_ERROR;

   /* Match '[' */
   if (!Parse_String(parseState, "["))
      RETURN_ERROR;

   /* Get output reg name */
   if (!Parse_Token(parseState, token))
      RETURN_ERROR;

   if (parseState->isPositionInvariant)
      start = 1; /* skip HPOS register name */
   else
      start = 0;

   /* try to match an output register name */
   for (j = start; OutputRegisters[j]; j++) {
      if (_mesa_strcmp((const char *) token, OutputRegisters[j]) == 0) {
         *outputRegNum = j;
         break;
      }
   }
   if (!OutputRegisters[j])
      RETURN_ERROR1("Unrecognized output register name");

   /* Match ']' */
   if (!Parse_String(parseState, "]"))
      RETURN_ERROR1("Expected ]");

   return GL_TRUE;
}


static GLboolean
Parse_MaskedDstReg(struct parse_state *parseState, struct vp_dst_register *dstReg)
{
   GLubyte token[100];

   /* Dst reg can be R<n> or o[n] */
   if (!Peek_Token(parseState, token))
      RETURN_ERROR;

   if (token[0] == 'R') {
      /* a temporary register */
      dstReg->File = PROGRAM_TEMPORARY;
      if (!Parse_TempReg(parseState, &dstReg->Index))
         RETURN_ERROR;
   }
   else if (!parseState->isStateProgram && token[0] == 'o') {
      /* an output register */
      dstReg->File = PROGRAM_OUTPUT;
      if (!Parse_OutputReg(parseState, &dstReg->Index))
         RETURN_ERROR;
   }
   else if (parseState->isStateProgram && token[0] == 'c') {
      /* absolute program parameter register */
      dstReg->File = PROGRAM_ENV_PARAM;
      if (!Parse_AbsParamReg(parseState, &dstReg->Index))
         RETURN_ERROR;
   }
   else {
      RETURN_ERROR1("Bad destination register name");
   }

   /* Parse optional write mask */
   if (!Peek_Token(parseState, token))
      RETURN_ERROR;

   if (token[0] == '.') {
      /* got a mask */
      GLint k = 0;

      if (!Parse_String(parseState, "."))
         RETURN_ERROR;

      if (!Parse_Token(parseState, token))
         RETURN_ERROR;

      dstReg->WriteMask[0] = GL_FALSE;
      dstReg->WriteMask[1] = GL_FALSE;
      dstReg->WriteMask[2] = GL_FALSE;
      dstReg->WriteMask[3] = GL_FALSE;

      if (token[k] == 'x') {
         dstReg->WriteMask[0] = GL_TRUE;
         k++;
      }
      if (token[k] == 'y') {
         dstReg->WriteMask[1] = GL_TRUE;
         k++;
      }
      if (token[k] == 'z') {
         dstReg->WriteMask[2] = GL_TRUE;
         k++;
      }
      if (token[k] == 'w') {
         dstReg->WriteMask[3] = GL_TRUE;
         k++;
      }
      if (k == 0) {
         RETURN_ERROR1("Bad writemask character");
      }
      return GL_TRUE;
   }
   else {
      dstReg->WriteMask[0] = GL_TRUE;
      dstReg->WriteMask[1] = GL_TRUE;
      dstReg->WriteMask[2] = GL_TRUE;
      dstReg->WriteMask[3] = GL_TRUE;
      return GL_TRUE;
   }
}


static GLboolean
Parse_SwizzleSrcReg(struct parse_state *parseState, struct vp_src_register *srcReg)
{
   GLubyte token[100];

   srcReg->RelAddr = GL_FALSE;

   /* check for '-' */
   if (!Peek_Token(parseState, token))
      RETURN_ERROR;
   if (token[0] == '-') {
      (void) Parse_String(parseState, "-");
      srcReg->Negate = GL_TRUE;
      if (!Peek_Token(parseState, token))
         RETURN_ERROR;
   }
   else {
      srcReg->Negate = GL_FALSE;
   }

   /* Src reg can be R<n>, c[n], c[n +/- offset], or a named vertex attrib */
   if (token[0] == 'R') {
      srcReg->File = PROGRAM_TEMPORARY;
      if (!Parse_TempReg(parseState, &srcReg->Index))
         RETURN_ERROR;
   }
   else if (token[0] == 'c') {
      if (!Parse_ParamReg(parseState, srcReg))
         RETURN_ERROR;
   }
   else if (token[0] == 'v') {
      srcReg->File = PROGRAM_INPUT;
      if (!Parse_AttribReg(parseState, &srcReg->Index))
         RETURN_ERROR;
   }
   else {
      RETURN_ERROR2("Bad source register name", token);
   }

   /* init swizzle fields */
   srcReg->Swizzle[0] = 0;
   srcReg->Swizzle[1] = 1;
   srcReg->Swizzle[2] = 2;
   srcReg->Swizzle[3] = 3;

   /* Look for optional swizzle suffix */
   if (!Peek_Token(parseState, token))
      RETURN_ERROR;
   if (token[0] == '.') {
      (void) Parse_String(parseState, ".");  /* consume . */

      if (!Parse_Token(parseState, token))
         RETURN_ERROR;

      if (token[1] == 0) {
         /* single letter swizzle */
         if (token[0] == 'x')
            ASSIGN_4V(srcReg->Swizzle, 0, 0, 0, 0);
         else if (token[0] == 'y')
            ASSIGN_4V(srcReg->Swizzle, 1, 1, 1, 1);
         else if (token[0] == 'z')
            ASSIGN_4V(srcReg->Swizzle, 2, 2, 2, 2);
         else if (token[0] == 'w')
            ASSIGN_4V(srcReg->Swizzle, 3, 3, 3, 3);
         else
            RETURN_ERROR1("Expected x, y, z, or w");
      }
      else {
         /* 2, 3 or 4-component swizzle */
         GLint k;
         for (k = 0; token[k] && k < 5; k++) {
            if (token[k] == 'x')
               srcReg->Swizzle[k] = 0;
            else if (token[k] == 'y')
               srcReg->Swizzle[k] = 1;
            else if (token[k] == 'z')
               srcReg->Swizzle[k] = 2;
            else if (token[k] == 'w')
               srcReg->Swizzle[k] = 3;
            else
               RETURN_ERROR;
         }
         if (k >= 5)
            RETURN_ERROR;
      }
   }

   return GL_TRUE;
}


static GLboolean
Parse_ScalarSrcReg(struct parse_state *parseState, struct vp_src_register *srcReg)
{
   GLubyte token[100];

   srcReg->RelAddr = GL_FALSE;

   /* check for '-' */
   if (!Peek_Token(parseState, token))
      RETURN_ERROR;
   if (token[0] == '-') {
      srcReg->Negate = GL_TRUE;
      (void) Parse_String(parseState, "-"); /* consume '-' */
      if (!Peek_Token(parseState, token))
         RETURN_ERROR;
   }
   else {
      srcReg->Negate = GL_FALSE;
   }

   /* Src reg can be R<n>, c[n], c[n +/- offset], or a named vertex attrib */
   if (token[0] == 'R') {
      srcReg->File = PROGRAM_TEMPORARY;
      if (!Parse_TempReg(parseState, &srcReg->Index))
         RETURN_ERROR;
   }
   else if (token[0] == 'c') {
      if (!Parse_ParamReg(parseState, srcReg))
         RETURN_ERROR;
   }
   else if (token[0] == 'v') {
      srcReg->File = PROGRAM_INPUT;
      if (!Parse_AttribReg(parseState, &srcReg->Index))
         RETURN_ERROR;
   }
   else {
      RETURN_ERROR2("Bad source register name", token);
   }

   /* Look for .[xyzw] suffix */
   if (!Parse_String(parseState, "."))
      RETURN_ERROR;

   if (!Parse_Token(parseState, token))
      RETURN_ERROR;

   if (token[0] == 'x' && token[1] == 0) {
      srcReg->Swizzle[0] = 0;
   }
   else if (token[0] == 'y' && token[1] == 0) {
      srcReg->Swizzle[0] = 1;
   }
   else if (token[0] == 'z' && token[1] == 0) {
      srcReg->Swizzle[0] = 2;
   }
   else if (token[0] == 'w' && token[1] == 0) {
      srcReg->Swizzle[0] = 3;
   }
   else {
      RETURN_ERROR1("Bad scalar source suffix");
   }
   srcReg->Swizzle[1] = srcReg->Swizzle[2] = srcReg->Swizzle[3] = 0;

   return GL_TRUE;
}


static GLint
Parse_UnaryOpInstruction(struct parse_state *parseState,
                         struct vp_instruction *inst, enum vp_opcode opcode)
{
   if (opcode == VP_OPCODE_ABS && !parseState->isVersion1_1)
      RETURN_ERROR1("ABS illegal for vertex program 1.0");

   inst->Opcode = opcode;
   inst->StringPos = parseState->curLine - parseState->start;

   /* dest reg */
   if (!Parse_MaskedDstReg(parseState, &inst->DstReg))
      RETURN_ERROR;

   /* comma */
   if (!Parse_String(parseState, ","))
      RETURN_ERROR;

   /* src arg */
   if (!Parse_SwizzleSrcReg(parseState, &inst->SrcReg[0]))
      RETURN_ERROR;

   /* semicolon */
   if (!Parse_String(parseState, ";"))
      RETURN_ERROR;

   return GL_TRUE;
}


static GLboolean
Parse_BiOpInstruction(struct parse_state *parseState,
                      struct vp_instruction *inst, enum vp_opcode opcode)
{
   if (opcode == VP_OPCODE_DPH && !parseState->isVersion1_1)
      RETURN_ERROR1("DPH illegal for vertex program 1.0");
   if (opcode == VP_OPCODE_SUB && !parseState->isVersion1_1)
      RETURN_ERROR1("SUB illegal for vertex program 1.0");

   inst->Opcode = opcode;
   inst->StringPos = parseState->curLine - parseState->start;

   /* dest reg */
   if (!Parse_MaskedDstReg(parseState, &inst->DstReg))
      RETURN_ERROR;

   /* comma */
   if (!Parse_String(parseState, ","))
      RETURN_ERROR;

   /* first src arg */
   if (!Parse_SwizzleSrcReg(parseState, &inst->SrcReg[0]))
      RETURN_ERROR;

   /* comma */
   if (!Parse_String(parseState, ","))
      RETURN_ERROR;

   /* second src arg */
   if (!Parse_SwizzleSrcReg(parseState, &inst->SrcReg[1]))
      RETURN_ERROR;

   /* semicolon */
   if (!Parse_String(parseState, ";"))
      RETURN_ERROR;

   /* make sure we don't reference more than one program parameter register */
   if (inst->SrcReg[0].File == PROGRAM_ENV_PARAM &&
       inst->SrcReg[1].File == PROGRAM_ENV_PARAM &&
       inst->SrcReg[0].Index != inst->SrcReg[1].Index)
      RETURN_ERROR1("Can't reference two program parameter registers");

   /* make sure we don't reference more than one vertex attribute register */
   if (inst->SrcReg[0].File == PROGRAM_INPUT &&
       inst->SrcReg[1].File == PROGRAM_INPUT &&
       inst->SrcReg[0].Index != inst->SrcReg[1].Index)
      RETURN_ERROR1("Can't reference two vertex attribute registers");

   return GL_TRUE;
}


static GLboolean
Parse_TriOpInstruction(struct parse_state *parseState,
                       struct vp_instruction *inst, enum vp_opcode opcode)
{
   inst->Opcode = opcode;
   inst->StringPos = parseState->curLine - parseState->start;

   /* dest reg */
   if (!Parse_MaskedDstReg(parseState, &inst->DstReg))
      RETURN_ERROR;

   /* comma */
   if (!Parse_String(parseState, ","))
      RETURN_ERROR;

   /* first src arg */
   if (!Parse_SwizzleSrcReg(parseState, &inst->SrcReg[0]))
      RETURN_ERROR;

   /* comma */
   if (!Parse_String(parseState, ","))
      RETURN_ERROR;

   /* second src arg */
   if (!Parse_SwizzleSrcReg(parseState, &inst->SrcReg[1]))
      RETURN_ERROR;

   /* comma */
   if (!Parse_String(parseState, ","))
      RETURN_ERROR;

   /* third src arg */
   if (!Parse_SwizzleSrcReg(parseState, &inst->SrcReg[2]))
      RETURN_ERROR;

   /* semicolon */
   if (!Parse_String(parseState, ";"))
      RETURN_ERROR;

   /* make sure we don't reference more than one program parameter register */
   if ((inst->SrcReg[0].File == PROGRAM_ENV_PARAM &&
        inst->SrcReg[1].File == PROGRAM_ENV_PARAM &&
        inst->SrcReg[0].Index != inst->SrcReg[1].Index) ||
       (inst->SrcReg[0].File == PROGRAM_ENV_PARAM &&
        inst->SrcReg[2].File == PROGRAM_ENV_PARAM &&
        inst->SrcReg[0].Index != inst->SrcReg[2].Index) ||
       (inst->SrcReg[1].File == PROGRAM_ENV_PARAM &&
        inst->SrcReg[2].File == PROGRAM_ENV_PARAM &&
        inst->SrcReg[1].Index != inst->SrcReg[2].Index))
      RETURN_ERROR1("Can only reference one program register");

   /* make sure we don't reference more than one vertex attribute register */
   if ((inst->SrcReg[0].File == PROGRAM_INPUT &&
        inst->SrcReg[1].File == PROGRAM_INPUT &&
        inst->SrcReg[0].Index != inst->SrcReg[1].Index) ||
       (inst->SrcReg[0].File == PROGRAM_INPUT &&
        inst->SrcReg[2].File == PROGRAM_INPUT &&
        inst->SrcReg[0].Index != inst->SrcReg[2].Index) ||
       (inst->SrcReg[1].File == PROGRAM_INPUT &&
        inst->SrcReg[2].File == PROGRAM_INPUT &&
        inst->SrcReg[1].Index != inst->SrcReg[2].Index))
      RETURN_ERROR1("Can only reference one input register");

   return GL_TRUE;
}


static GLboolean
Parse_ScalarInstruction(struct parse_state *parseState,
                        struct vp_instruction *inst, enum vp_opcode opcode)
{
   if (opcode == VP_OPCODE_RCC && !parseState->isVersion1_1)
      RETURN_ERROR1("RCC illegal for vertex program 1.0");

   inst->Opcode = opcode;
   inst->StringPos = parseState->curLine - parseState->start;

   /* dest reg */
   if (!Parse_MaskedDstReg(parseState, &inst->DstReg))
      RETURN_ERROR;

   /* comma */
   if (!Parse_String(parseState, ","))
      RETURN_ERROR;

   /* first src arg */
   if (!Parse_ScalarSrcReg(parseState, &inst->SrcReg[0]))
      RETURN_ERROR;

   /* semicolon */
   if (!Parse_String(parseState, ";"))
      RETURN_ERROR;

   return GL_TRUE;
}


static GLboolean
Parse_AddressInstruction(struct parse_state *parseState, struct vp_instruction *inst)
{
   inst->Opcode = VP_OPCODE_ARL;
   inst->StringPos = parseState->curLine - parseState->start;

   /* dest A0 reg */
   if (!Parse_AddrReg(parseState))
      RETURN_ERROR;

   /* comma */
   if (!Parse_String(parseState, ","))
      RETURN_ERROR;

   /* parse src reg */
   if (!Parse_ScalarSrcReg(parseState, &inst->SrcReg[0]))
      RETURN_ERROR;

   /* semicolon */
   if (!Parse_String(parseState, ";"))
      RETURN_ERROR;

   return GL_TRUE;
}


static GLboolean
Parse_EndInstruction(struct parse_state *parseState, struct vp_instruction *inst)
{
   GLubyte token[100];

   inst->Opcode = VP_OPCODE_END;
   inst->StringPos = parseState->curLine - parseState->start;

   /* this should fail! */
   if (Parse_Token(parseState, token))
      RETURN_ERROR2("Unexpected token after END:", token);
   else
      return GL_TRUE;
}


static GLboolean
Parse_OptionSequence(struct parse_state *parseState,
                     struct vp_instruction program[])
{
   while (1) {
      if (!Parse_String(parseState, "OPTION"))
         return GL_TRUE;  /* ok, not an OPTION statement */
      if (Parse_String(parseState, "NV_position_invariant")) {
         parseState->isPositionInvariant = GL_TRUE;
      }
      else {
         RETURN_ERROR1("unexpected OPTION statement");
      }
      if (!Parse_String(parseState, ";"))
         return GL_FALSE;
   }
}


static GLboolean
Parse_InstructionSequence(struct parse_state *parseState,
                          struct vp_instruction program[])
{
   while (1) {
      struct vp_instruction *inst = program + parseState->numInst;

      /* Initialize the instruction */
      inst->SrcReg[0].File = (enum register_file) -1;
      inst->SrcReg[1].File = (enum register_file) -1;
      inst->SrcReg[2].File = (enum register_file) -1;
      inst->DstReg.File = (enum register_file) -1;

      if (Parse_String(parseState, "MOV")) {
         if (!Parse_UnaryOpInstruction(parseState, inst, VP_OPCODE_MOV))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "LIT")) {
         if (!Parse_UnaryOpInstruction(parseState, inst, VP_OPCODE_LIT))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "ABS")) {
         if (!Parse_UnaryOpInstruction(parseState, inst, VP_OPCODE_ABS))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "MUL")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_MUL))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "ADD")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_ADD))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "DP3")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_DP3))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "DP4")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_DP4))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "DST")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_DST))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "MIN")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_MIN))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "MAX")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_MAX))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "SLT")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_SLT))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "SGE")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_SGE))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "DPH")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_DPH))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "SUB")) {
         if (!Parse_BiOpInstruction(parseState, inst, VP_OPCODE_SUB))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "MAD")) {
         if (!Parse_TriOpInstruction(parseState, inst, VP_OPCODE_MAD))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "RCP")) {
         if (!Parse_ScalarInstruction(parseState, inst, VP_OPCODE_RCP))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "RSQ")) {
         if (!Parse_ScalarInstruction(parseState, inst, VP_OPCODE_RSQ))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "EXP")) {
         if (!Parse_ScalarInstruction(parseState, inst, VP_OPCODE_EXP))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "LOG")) {
         if (!Parse_ScalarInstruction(parseState, inst, VP_OPCODE_LOG))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "RCC")) {
         if (!Parse_ScalarInstruction(parseState, inst, VP_OPCODE_RCC))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "ARL")) {
         if (!Parse_AddressInstruction(parseState, inst))
            RETURN_ERROR;
      }
      else if (Parse_String(parseState, "END")) {
         if (!Parse_EndInstruction(parseState, inst))
            RETURN_ERROR;
         else {
            parseState->numInst++;
            return GL_TRUE;  /* all done */
         }
      }
      else {
         /* bad instruction name */
         RETURN_ERROR1("Unexpected token");
      }

      /* examine input/output registers */
      if (inst->DstReg.File == PROGRAM_OUTPUT)
         parseState->outputsWritten |= (1 << inst->DstReg.Index);
      else if (inst->DstReg.File == PROGRAM_ENV_PARAM)
         parseState->anyProgRegsWritten = GL_TRUE;

      if (inst->SrcReg[0].File == PROGRAM_INPUT)
         parseState->inputsRead |= (1 << inst->SrcReg[0].Index);
      if (inst->SrcReg[1].File == PROGRAM_INPUT)
         parseState->inputsRead |= (1 << inst->SrcReg[1].Index);
      if (inst->SrcReg[2].File == PROGRAM_INPUT)
         parseState->inputsRead |= (1 << inst->SrcReg[2].Index);

      parseState->numInst++;

      if (parseState->numInst >= MAX_NV_VERTEX_PROGRAM_INSTRUCTIONS)
         RETURN_ERROR1("Program too long");
   }

   RETURN_ERROR;
}


static GLboolean
Parse_Program(struct parse_state *parseState,
              struct vp_instruction instBuffer[])
{
   if (parseState->isVersion1_1) {
      if (!Parse_OptionSequence(parseState, instBuffer)) {
         return GL_FALSE;
      }
   }
   return Parse_InstructionSequence(parseState, instBuffer);
}


/**
 * Parse/compile the 'str' returning the compiled 'program'.
 * ctx->Program.ErrorPos will be -1 if successful.  Otherwise, ErrorPos
 * indicates the position of the error in 'str'.
 */
void
_mesa_parse_nv_vertex_program(GLcontext *ctx, GLenum dstTarget,
                              const GLubyte *str, GLsizei len,
                              struct vertex_program *program)
{
   struct parse_state parseState;
   struct vp_instruction instBuffer[MAX_NV_VERTEX_PROGRAM_INSTRUCTIONS];
   struct vp_instruction *newInst;
   GLenum target;
   GLubyte *programString;

   /* Make a null-terminated copy of the program string */
   programString = (GLubyte *) MALLOC(len + 1);
   if (!programString) {
      _mesa_error(ctx, GL_OUT_OF_MEMORY, "glLoadProgramNV");
      return;
   }
   MEMCPY(programString, str, len);
   programString[len] = 0;

   /* Get ready to parse */
   parseState.ctx = ctx;
   parseState.start = programString;
   parseState.isPositionInvariant = GL_FALSE;
   parseState.isVersion1_1 = GL_FALSE;
   parseState.numInst = 0;
   parseState.inputsRead = 0;
   parseState.outputsWritten = 0;
   parseState.anyProgRegsWritten = GL_FALSE;

   /* Reset error state */
   _mesa_set_program_error(ctx, -1, NULL);

   /* check the program header */
   if (_mesa_strncmp((const char *) programString, "!!VP1.0", 7) == 0) {
      target = GL_VERTEX_PROGRAM_NV;
      parseState.pos = programString + 7;
      parseState.isStateProgram = GL_FALSE;
   }
   else if (_mesa_strncmp((const char *) programString, "!!VP1.1", 7) == 0) {
      target = GL_VERTEX_PROGRAM_NV;
      parseState.pos = programString + 7;
      parseState.isStateProgram = GL_FALSE;
      parseState.isVersion1_1 = GL_TRUE;
   }
   else if (_mesa_strncmp((const char *) programString, "!!VSP1.0", 8) == 0) {
      target = GL_VERTEX_STATE_PROGRAM_NV;
      parseState.pos = programString + 8;
      parseState.isStateProgram = GL_TRUE;
   }
   else {
      /* invalid header */
      ctx->Program.ErrorPos = 0;
      _mesa_error(ctx, GL_INVALID_OPERATION, "glLoadProgramNV(bad header)");
      return;
   }

   /* make sure target and header match */
   if (target != dstTarget) {
      _mesa_error(ctx, GL_INVALID_OPERATION,
                  "glLoadProgramNV(target mismatch)");
      return;
   }


   if (Parse_Program(&parseState, instBuffer)) {
      /* successful parse! */

      if (parseState.isStateProgram) {
         if (!parseState.anyProgRegsWritten) {
            _mesa_error(ctx, GL_INVALID_OPERATION,
                        "glLoadProgramNV(c[#] not written)");
            return;
         }
      }
      else {
         if (!parseState.isPositionInvariant &&
             !(parseState.outputsWritten & 1)) {
            /* bit 1 = HPOS register */
            _mesa_error(ctx, GL_INVALID_OPERATION,
                        "glLoadProgramNV(HPOS not written)");
            return;
         }
      }

      /* copy the compiled instructions */
      assert(parseState.numInst <= MAX_NV_VERTEX_PROGRAM_INSTRUCTIONS);
      newInst = (struct vp_instruction *)
         MALLOC(parseState.numInst * sizeof(struct vp_instruction));
      if (!newInst) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glLoadProgramNV");
         FREE(programString);
         return;  /* out of memory */
      }
      MEMCPY(newInst, instBuffer,
             parseState.numInst * sizeof(struct vp_instruction));

      /* install the program */
      program->Base.Target = target;
      if (program->Base.String) {
         FREE(program->Base.String);
      }
      program->Base.String = programString;
      program->Base.Format = GL_PROGRAM_FORMAT_ASCII_ARB;
      if (program->Instructions) {
         FREE(program->Instructions);
      }
      program->Instructions = newInst;
      program->InputsRead = parseState.inputsRead;
      program->OutputsWritten = parseState.outputsWritten;
      program->IsPositionInvariant = parseState.isPositionInvariant;

#ifdef DEBUG
      _mesa_printf("--- glLoadProgramNV result ---\n");
      _mesa_print_nv_vertex_program(program);
      _mesa_printf("------------------------------\n");
#endif
   }
   else {
      /* Error! */
      _mesa_error(ctx, GL_INVALID_OPERATION, "glLoadProgramNV");
      /* NOTE: _mesa_set_program_error would have been called already */
      /* GL_NV_vertex_program isn't supposed to set the error string
       * so we reset it here.
       */
      _mesa_set_program_error(ctx, ctx->Program.ErrorPos, NULL);
   }
}


static void
PrintSrcReg(const struct vp_src_register *src)
{
   static const char comps[5] = "xyzw";
   if (src->Negate)
      _mesa_printf("-");
   if (src->RelAddr) {
      if (src->Index > 0)
         _mesa_printf("c[A0.x + %d]", src->Index);
      else if (src->Index < 0)
         _mesa_printf("c[A0.x - %d]", -src->Index);
      else
         _mesa_printf("c[A0.x]");
   }
   else if (src->File == PROGRAM_OUTPUT) {
      _mesa_printf("o[%s]", OutputRegisters[src->Index]);
   }
   else if (src->File == PROGRAM_INPUT) {
      _mesa_printf("v[%s]", InputRegisters[src->Index]);
   }
   else if (src->File == PROGRAM_ENV_PARAM) {
      _mesa_printf("c[%d]", src->Index);
   }
   else {
      ASSERT(src->File == PROGRAM_TEMPORARY);
      _mesa_printf("R%d", src->Index);
   }

   if (src->Swizzle[0] == src->Swizzle[1] &&
       src->Swizzle[0] == src->Swizzle[2] &&
       src->Swizzle[0] == src->Swizzle[3]) {
      _mesa_printf(".%c", comps[src->Swizzle[0]]);
   }
   else if (src->Swizzle[0] != 0 ||
            src->Swizzle[1] != 1 ||
            src->Swizzle[2] != 2 ||
            src->Swizzle[3] != 3) {
      _mesa_printf(".%c%c%c%c",
             comps[src->Swizzle[0]],
             comps[src->Swizzle[1]],
             comps[src->Swizzle[2]],
             comps[src->Swizzle[3]]);
   }
}


static void
PrintDstReg(const struct vp_dst_register *dst)
{
   GLint w = dst->WriteMask[0] + dst->WriteMask[1]
           + dst->WriteMask[2] + dst->WriteMask[3];

   if (dst->File == PROGRAM_OUTPUT) {
      _mesa_printf("o[%s]", OutputRegisters[dst->Index]);
   }
   else if (dst->File == PROGRAM_INPUT) {
      _mesa_printf("v[%s]", InputRegisters[dst->Index]);
   }
   else if (dst->File == PROGRAM_ENV_PARAM) {
      _mesa_printf("c[%d]", dst->Index);
   }
   else {
      ASSERT(dst->File == PROGRAM_TEMPORARY);
      _mesa_printf("R%d", dst->Index);
   }

   if (w != 0 && w != 4) {
      _mesa_printf(".");
      if (dst->WriteMask[0])
         _mesa_printf("x");
      if (dst->WriteMask[1])
         _mesa_printf("y");
      if (dst->WriteMask[2])
         _mesa_printf("z");
      if (dst->WriteMask[3])
         _mesa_printf("w");
   }
}


/**
 * Print a single NVIDIA vertex program instruction.
 */
void
_mesa_print_nv_vertex_instruction(const struct vp_instruction *inst)
{
   switch (inst->Opcode) {
      case VP_OPCODE_MOV:
      case VP_OPCODE_LIT:
      case VP_OPCODE_RCP:
      case VP_OPCODE_RSQ:
      case VP_OPCODE_EXP:
      case VP_OPCODE_LOG:
      case VP_OPCODE_RCC:
      case VP_OPCODE_ABS:
         _mesa_printf("%s ", Opcodes[(int) inst->Opcode]);
         PrintDstReg(&inst->DstReg);
         _mesa_printf(", ");
         PrintSrcReg(&inst->SrcReg[0]);
         _mesa_printf(";\n");
         break;
      case VP_OPCODE_MUL:
      case VP_OPCODE_ADD:
      case VP_OPCODE_DP3:
      case VP_OPCODE_DP4:
      case VP_OPCODE_DST:
      case VP_OPCODE_MIN:
      case VP_OPCODE_MAX:
      case VP_OPCODE_SLT:
      case VP_OPCODE_SGE:
      case VP_OPCODE_DPH:
      case VP_OPCODE_SUB:
         _mesa_printf("%s ", Opcodes[(int) inst->Opcode]);
         PrintDstReg(&inst->DstReg);
         _mesa_printf(", ");
         PrintSrcReg(&inst->SrcReg[0]);
         _mesa_printf(", ");
         PrintSrcReg(&inst->SrcReg[1]);
         _mesa_printf(";\n");
         break;
      case VP_OPCODE_MAD:
         _mesa_printf("MAD ");
         PrintDstReg(&inst->DstReg);
         _mesa_printf(", ");
         PrintSrcReg(&inst->SrcReg[0]);
         _mesa_printf(", ");
         PrintSrcReg(&inst->SrcReg[1]);
         _mesa_printf(", ");
         PrintSrcReg(&inst->SrcReg[2]);
         _mesa_printf(";\n");
         break;
      case VP_OPCODE_ARL:
         _mesa_printf("ARL A0.x, ");
         PrintSrcReg(&inst->SrcReg[0]);
         _mesa_printf(";\n");
         break;
      case VP_OPCODE_END:
         _mesa_printf("END\n");
         break;
      default:
         _mesa_printf("BAD INSTRUCTION\n");
   }
}


/**
 * Print (unparse) the given vertex program.  Just for debugging.
 */
void
_mesa_print_nv_vertex_program(const struct vertex_program *program)
{
   const struct vp_instruction *inst;

   for (inst = program->Instructions; ; inst++) {
      _mesa_print_nv_vertex_instruction(inst);
      if (inst->Opcode == VP_OPCODE_END)
         return;
   }
}


const char *
_mesa_nv_vertex_input_register_name(GLuint i)
{
   ASSERT(i < MAX_NV_VERTEX_PROGRAM_INPUTS);
   return InputRegisters[i];
}


const char *
_mesa_nv_vertex_output_register_name(GLuint i)
{
   ASSERT(i < MAX_NV_VERTEX_PROGRAM_OUTPUTS);
   return OutputRegisters[i];
}
