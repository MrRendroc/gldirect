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

/*==========================================================================;
 *
 *
 *  File:   dxerr9.h
 *  Content:    DirectX Error Library Include File
 *
 ****************************************************************************/

#ifndef _GLD_DXERR9_H_
#define _GLD_DXERR9_H_

#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif
#include <d3d9.h>

//
//  DXGetErrorString9
//  
//  Desc:  Converts an DirectX HRESULT to a string 
//
//  Args:  HRESULT hr   Can be any error code from
//                      DPLAY D3D8 D3DX8 DMUSIC DSOUND
//
//  Return: Converted string 
//
const char*  __stdcall DXGetErrorStringA(HRESULT hr);
const WCHAR* __stdcall DXGetErrorStringW(HRESULT hr);

#ifdef UNICODE
    #define DXGetErrorString DXGetErrorStringW
#else
    #define DXGetErrorString DXGetErrorStringA
#endif 


//
//  DXTrace
//
//  Desc:  Outputs a formatted error message to the debug stream
//
//  Args:  CHAR* strFile   The current file, typically passed in using the 
//                         __FILE__ macro.
//         DWORD dwLine    The current line number, typically passed in using the 
//                         __LINE__ macro.
//         HRESULT hr      An HRESULT that will be traced to the debug stream.
//         CHAR* strMsg    A string that will be traced to the debug stream (may be NULL)
//         BOOL bPopMsgBox If TRUE, then a message box will popup also containing the passed info.
//
//  Return: The hr that was passed in.  
//
//HRESULT __stdcall DXTraceA( char* strFile, DWORD dwLine, HRESULT hr, char* strMsg, BOOL bPopMsgBox = FALSE );
//HRESULT __stdcall DXTraceW( char* strFile, DWORD dwLine, HRESULT hr, WCHAR* strMsg, BOOL bPopMsgBox = FALSE );
HRESULT __stdcall DXTraceA( char* strFile, DWORD dwLine, HRESULT hr, char* strMsg, BOOL bPopMsgBox);
HRESULT __stdcall DXTraceW( char* strFile, DWORD dwLine, HRESULT hr, WCHAR* strMsg, BOOL bPopMsgBox);

#ifdef UNICODE
    #define DXTrace DXTraceW
#else
    #define DXTrace DXTraceA
#endif 


//
// Helper macros
//
#if defined(DEBUG) | defined(_DEBUG)
    #define DXTRACE_MSG(str)              DXTrace( __FILE__, (DWORD)__LINE__, 0, str, FALSE )
    #define DXTRACE_ERR(str,hr)           DXTrace( __FILE__, (DWORD)__LINE__, hr, str, TRUE )
    #define DXTRACE_ERR_NOMSGBOX(str,hr)  DXTrace( __FILE__, (DWORD)__LINE__, hr, str, FALSE )
#else
    #define DXTRACE_MSG(str)              (0L)
    #define DXTRACE_ERR(str,hr)           (hr)
    #define DXTRACE_ERR_NOMSGBOX(str,hr)  (hr)
#endif


#endif

