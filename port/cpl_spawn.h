/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement CPLSystem().
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_SPAWN_H_INCLUDED
#define CPL_SPAWN_H_INCLUDED

#include "cpl_vsi.h"

CPL_C_START

/* -------------------------------------------------------------------- */
/*      Spawn a process.                                                */
/* -------------------------------------------------------------------- */

int CPL_DLL CPLSpawn(const char *const papszArgv[], VSILFILE *fin,
                     VSILFILE *fout, int bDisplayErr);

#ifdef _WIN32
#include <windows.h>
typedef HANDLE CPL_FILE_HANDLE;
#define CPL_FILE_INVALID_HANDLE CPL_NULLPTR
typedef DWORD CPL_PID;
#else
#include <sys/types.h>
typedef int CPL_FILE_HANDLE;
#define CPL_FILE_INVALID_HANDLE -1
typedef pid_t CPL_PID;
#endif

typedef struct _CPLSpawnedProcess CPLSpawnedProcess;

CPLSpawnedProcess CPL_DLL *
CPLSpawnAsync(int (*pfnMain)(CPL_FILE_HANDLE, CPL_FILE_HANDLE),
              const char *const papszArgv[], int bCreateInputPipe,
              int bCreateOutputPipe, int bCreateErrorPipe, char **papszOptions);
CPL_PID CPL_DLL CPLSpawnAsyncGetChildProcessId(CPLSpawnedProcess *p);
int CPL_DLL CPLSpawnAsyncFinish(CPLSpawnedProcess *p, int bWait, int bKill);
CPL_FILE_HANDLE CPL_DLL CPLSpawnAsyncGetInputFileHandle(CPLSpawnedProcess *p);
CPL_FILE_HANDLE CPL_DLL CPLSpawnAsyncGetOutputFileHandle(CPLSpawnedProcess *p);
CPL_FILE_HANDLE CPL_DLL CPLSpawnAsyncGetErrorFileHandle(CPLSpawnedProcess *p);
void CPL_DLL CPLSpawnAsyncCloseInputFileHandle(CPLSpawnedProcess *p);
void CPL_DLL CPLSpawnAsyncCloseOutputFileHandle(CPLSpawnedProcess *p);
void CPL_DLL CPLSpawnAsyncCloseErrorFileHandle(CPLSpawnedProcess *p);

int CPL_DLL CPLPipeRead(CPL_FILE_HANDLE fin, void *data, int length);
int CPL_DLL CPLPipeWrite(CPL_FILE_HANDLE fout, const void *data, int length);

CPL_C_END

#endif  // CPL_SPAWN_H_INCLUDED
