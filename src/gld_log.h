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
* Environment:  Windows 9x (Win32)
*
* Description:  Logging functions.
*
*********************************************************************************/

#ifndef __GLD_LOG_H
#define __GLD_LOG_H

#include <stdio.h>

/*---------------------- Macros and type definitions ----------------------*/

#define GLDLOG_FILENAME	"gldirect.log"

typedef enum {
	GLDLOG_NONE					= 0,			// No log output
	GLDLOG_NORMAL				= 1,			// Log is kept open
	GLDLOG_CRASHPROOF			= 2,			// Log is closed and flushed
	GLDLOG_METHOD_FORCE_DWORD	= 0x7fffffff,
} GLDLOG_loggingMethodType;

// Denotes type of message sent to the logging functions
typedef enum {
	GLDLOG_INFO					= 0,			// Information only
	GLDLOG_WARN					= 1,			// Warning only
	GLDLOG_ERROR				= 2,			// Notify user of an error
	GLDLOG_CRITICAL				= 3,			// Exceptionally severe error
	GLDLOG_SYSTEM				= 4,			// System message. Not an error
												// but must always be printed.
	GLDLOG_SEVERITY_FORCE_DWORD	= 0x7fffffff,	// Make enum dword
} GLDLOG_severityType;

// The message that will be output to the log
static const char *gldLogSeverityMessages[] = {
	"INF",	// Info
	"WRN",	// Warn
	"ERR",	// Error
	"CRI",	// Critical
	"SYS",	// System
};

/*------------------------- Function Prototypes ---------------------------*/

#ifdef  __cplusplus
extern "C" {
#endif

void gldLogOpen(GLDLOG_loggingMethodType LoggingMethod, GLDLOG_severityType Severity);
void gldLogClose();
void gldLogMessage(GLDLOG_severityType severity, LPSTR message);
void gldLogError(GLDLOG_severityType severity, LPSTR message, HRESULT hResult);
void gldLogPrintf(GLDLOG_severityType severity, LPSTR message, ...);
void gldLogWarnOption(BOOL bWarnOption);
void gldLogPathOption(LPSTR szPath);

#ifdef  __cplusplus
}
#endif

#endif