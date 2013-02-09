/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement CPLSystem().
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2013,Even Rouault
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef CPL_SPAWN_H_INCLUDED
#define CPL_SPAWN_H_INCLUDED

#include "cpl_vsi.h"

CPL_C_START

/* -------------------------------------------------------------------- */
/*      Spawn a process.                                                */
/* -------------------------------------------------------------------- */

int CPL_DLL CPLSystem( const char* pszApplicationName, const char* pszCommandLine );
int CPL_DLL CPLSpawn( const char * const papszArgv[], VSILFILE* fin, VSILFILE* fout,
                      int bDisplayErr );

#ifdef WIN32
#include <windows.h>
typedef HANDLE CPL_FILE_HANDLE;
#define CPL_FILE_INVALID_HANDLE NULL
typedef DWORD  CPL_PID;
#else
#include <sys/types.h>
typedef int    CPL_FILE_HANDLE;
#define CPL_FILE_INVALID_HANDLE -1
typedef pid_t  CPL_PID;
#endif

typedef struct _CPLSpawnedProcess CPLSpawnedProcess;

CPLSpawnedProcess CPL_DLL* CPLSpawnAsync( int (*pfnMain)(CPL_FILE_HANDLE, CPL_FILE_HANDLE),
                                          const char * const papszArgv[],
                                          int bCreateInputPipe,
                                          int bCreateOutputPipe,
                                          int bCreateErrorPipe,
                                          char** papszOptions );
CPL_PID CPL_DLL CPLSpawnAsyncGetChildProcessId(CPLSpawnedProcess* p);
int CPL_DLL CPLSpawnAsyncFinish(CPLSpawnedProcess* p, int bWait, int bKill);
CPL_FILE_HANDLE CPL_DLL CPLSpawnAsyncGetInputFileHandle(CPLSpawnedProcess* p);
CPL_FILE_HANDLE CPL_DLL CPLSpawnAsyncGetOutputFileHandle(CPLSpawnedProcess* p);
CPL_FILE_HANDLE CPL_DLL CPLSpawnAsyncGetErrorFileHandle(CPLSpawnedProcess* p);
void CPL_DLL CPLSpawnAsyncCloseInputFileHandle(CPLSpawnedProcess* p);
void CPL_DLL CPLSpawnAsyncCloseOutputFileHandle(CPLSpawnedProcess* p);
void CPL_DLL CPLSpawnAsyncCloseErrorFileHandle(CPLSpawnedProcess* p);

int CPL_DLL CPLPipeRead(CPL_FILE_HANDLE fin, void* data, int length);
int CPL_DLL CPLPipeWrite(CPL_FILE_HANDLE fout, const void* data, int length);

CPL_C_END

#endif // CPL_SPAWN_H_INCLUDED

