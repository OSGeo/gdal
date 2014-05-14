/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement CPLSystem().
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_spawn.h"

#include "cpl_error.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"

#define PIPE_BUFFER_SIZE    4096

#define IN_FOR_PARENT   0
#define OUT_FOR_PARENT  1

CPL_CVSID("$Id$");

static void FillFileFromPipe(CPL_FILE_HANDLE pipe_fd, VSILFILE* fout);

//int CPL_DLL CPLSystem( const char* pszApplicationName, const char* pszCommandLine );

/************************************************************************/
/*                        FillPipeFromFile()                            */
/************************************************************************/

static void FillPipeFromFile(VSILFILE* fin, CPL_FILE_HANDLE pipe_fd)
{
    char buf[PIPE_BUFFER_SIZE];
    while(TRUE)
    {
        int nRead = (int)VSIFReadL(buf, 1, PIPE_BUFFER_SIZE, fin);
        if( nRead <= 0 )
            break;
        if (!CPLPipeWrite(pipe_fd, buf, nRead))
            break;
    }
}

/************************************************************************/
/*                            CPLSpawn()                                */
/************************************************************************/

/**
 * Runs an executable in another process.
 *
 * This function runs an executable, wait for it to finish and returns
 * its exit code.
 *
 * It is implemented as CreateProcess() on Windows platforms, and fork()/exec()
 * on other platforms.
 *
 * @param papszArgv argument list of the executable to run. papszArgv[0] is the
 *                  name of the executable
 * @param fin File handle for input data to feed to the standard input of the
 *            sub-process. May be NULL.
 * @param fout File handle for output data to extract from the standard output of the
 *            sub-process. May be NULL.
 * @param bDisplayErr Set to TRUE to emit the content of the standard error stream of
 *                    the sub-process with CPLError().
 *
 * @return the exit code of the spawned process, or -1 in case of error.
 *
 * @since GDAL 1.10.0
 */

int CPLSpawn(const char * const papszArgv[], VSILFILE* fin, VSILFILE* fout,
             int bDisplayErr)
{
    CPLSpawnedProcess* sp = CPLSpawnAsync(NULL, papszArgv, TRUE, TRUE, TRUE, NULL);
    if( sp == NULL )
        return -1;

    CPL_FILE_HANDLE in_child = CPLSpawnAsyncGetOutputFileHandle(sp);
    if (fin != NULL)
        FillPipeFromFile(fin, in_child);
    CPLSpawnAsyncCloseOutputFileHandle(sp);

    CPL_FILE_HANDLE out_child = CPLSpawnAsyncGetInputFileHandle(sp);
    if (fout != NULL)
        FillFileFromPipe(out_child, fout);
    CPLSpawnAsyncCloseInputFileHandle(sp);

    CPL_FILE_HANDLE err_child = CPLSpawnAsyncGetErrorFileHandle(sp);
    CPLString osName;
    osName.Printf("/vsimem/child_stderr_" CPL_FRMT_GIB, CPLGetPID());
    VSILFILE* ferr = VSIFOpenL(osName.c_str(), "w");

    FillFileFromPipe(err_child, ferr);
    CPLSpawnAsyncCloseErrorFileHandle(sp);

    VSIFCloseL(ferr);
    vsi_l_offset nDataLength = 0;
    GByte* pData = VSIGetMemFileBuffer(osName.c_str(), &nDataLength, TRUE);
    if( nDataLength > 0 )
        pData[nDataLength-1] = '\0';
    if( pData && strstr((const char*)pData, "An error occured while forking process") != NULL )
        bDisplayErr = TRUE;
    if( pData && bDisplayErr )
        CPLError(CE_Failure, CPLE_AppDefined, "[%s error] %s", papszArgv[0], pData);
    CPLFree(pData);

    return CPLSpawnAsyncFinish(sp, TRUE, FALSE);
}

#if defined(WIN32)

#include <windows.h>

#if 0
/************************************************************************/
/*                            CPLSystem()                               */
/************************************************************************/

int CPLSystem( const char* pszApplicationName, const char* pszCommandLine )
{
    int nRet = -1;
    PROCESS_INFORMATION processInfo;
    STARTUPINFO startupInfo;
    ZeroMemory( &processInfo, sizeof(PROCESS_INFORMATION) );
    ZeroMemory( &startupInfo, sizeof(STARTUPINFO) );
    startupInfo.cb = sizeof(STARTUPINFO);

    char* pszDupedCommandLine = (pszCommandLine) ? CPLStrdup(pszCommandLine) : NULL;

    if( !CreateProcess( pszApplicationName, 
                        pszDupedCommandLine, 
                        NULL,
                        NULL,
                        FALSE,
                        CREATE_NO_WINDOW|NORMAL_PRIORITY_CLASS,
                        NULL,
                        NULL,
                        &startupInfo,
                        &processInfo) )
    {
        DWORD err = GetLastError();
        CPLDebug("CPL", "'%s' failed : err = %d", pszCommandLine, (int)err);
        nRet = -1;
    }
    else
    {
        WaitForSingleObject( processInfo.hProcess, INFINITE );

        DWORD exitCode;

        // Get the exit code.
        int err = GetExitCodeProcess(processInfo.hProcess, &exitCode);

        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);

        if( !err )
        {
            CPLDebug("CPL", "GetExitCodeProcess() failed : err = %d", err);
        }
        else
            nRet = exitCode;
    }

    CPLFree(pszDupedCommandLine);

    return nRet;
}
#endif

/************************************************************************/
/*                          CPLPipeRead()                               */
/************************************************************************/

int CPLPipeRead(CPL_FILE_HANDLE fin, void* data, int length)
{
    GByte* pabyData = (GByte*)data;
    int nRemain = length;
    while( nRemain > 0 )
    {
        DWORD nRead = 0;
        if (!ReadFile( fin, pabyData, nRemain, &nRead, NULL))
            return FALSE;
        pabyData += nRead;
        nRemain -= nRead;
    }
    return TRUE;
}

/************************************************************************/
/*                         CPLPipeWrite()                               */
/************************************************************************/

int CPLPipeWrite(CPL_FILE_HANDLE fout, const void* data, int length)
{
    const GByte* pabyData = (const GByte*)data;
    int nRemain = length;
    while( nRemain > 0 )
    {
        DWORD nWritten = 0;
        if (!WriteFile(fout, pabyData, nRemain, &nWritten, NULL))
            return FALSE;
        pabyData += nWritten;
        nRemain -= nWritten;
    }
    return TRUE;
}

/************************************************************************/
/*                        FillFileFromPipe()                            */
/************************************************************************/

static void FillFileFromPipe(CPL_FILE_HANDLE pipe_fd, VSILFILE* fout)
{
    char buf[PIPE_BUFFER_SIZE];
    while(TRUE)
    {
        DWORD nRead;
        if (!ReadFile( pipe_fd, buf, PIPE_BUFFER_SIZE, &nRead, NULL))
            break;
        if (nRead <= 0)
            break;
        int nWritten = (int)VSIFWriteL(buf, 1, nRead, fout);
        if (nWritten < (int)nRead)
            break;
    }
}

struct _CPLSpawnedProcess
{
    HANDLE hProcess;
    DWORD  nProcessId;
    HANDLE hThread;
    CPL_FILE_HANDLE fin;
    CPL_FILE_HANDLE fout;
    CPL_FILE_HANDLE ferr;
};

/************************************************************************/
/*                            CPLSpawnAsync()                           */
/************************************************************************/

CPLSpawnedProcess* CPLSpawnAsync(int (*pfnMain)(CPL_FILE_HANDLE, CPL_FILE_HANDLE),
                                 const char * const papszArgv[],
                                 int bCreateInputPipe,
                                 int bCreateOutputPipe,
                                 int bCreateErrorPipe,
                                 char** papszOptions)
{
    HANDLE pipe_in[2] = {NULL, NULL};
    HANDLE pipe_out[2] = {NULL, NULL};
    HANDLE pipe_err[2] = {NULL, NULL};
    SECURITY_ATTRIBUTES saAttr;
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    CPLString osCommandLine;
    int i;
    CPLSpawnedProcess* p = NULL;

    if( papszArgv == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "On Windows, papszArgv argument must not be NULL");
        return NULL;
    }

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if( bCreateInputPipe )
    {
        if (!CreatePipe(&pipe_in[IN_FOR_PARENT],&pipe_in[OUT_FOR_PARENT],&saAttr, 0))
            goto err_pipe;
        /* The child must not inherit from the write side of the pipe_in */
        if (!SetHandleInformation(pipe_in[OUT_FOR_PARENT],HANDLE_FLAG_INHERIT,0))
            goto err_pipe;
    }

    if( bCreateOutputPipe )
    {
        if (!CreatePipe(&pipe_out[IN_FOR_PARENT],&pipe_out[OUT_FOR_PARENT],&saAttr, 0))
            goto err_pipe;
        /* The child must not inherit from the read side of the pipe_out */
        if (!SetHandleInformation(pipe_out[IN_FOR_PARENT],HANDLE_FLAG_INHERIT,0))
            goto err_pipe;
    }

    if( bCreateErrorPipe )
    {
        if (!CreatePipe(&pipe_err[IN_FOR_PARENT],&pipe_err[OUT_FOR_PARENT],&saAttr, 0))
            goto err_pipe;
        /* The child must not inherit from the read side of the pipe_err */
        if (!SetHandleInformation(pipe_err[IN_FOR_PARENT],HANDLE_FLAG_INHERIT,0))
            goto err_pipe;
    }

    memset(&piProcInfo, 0, sizeof(PROCESS_INFORMATION));
    memset(&siStartInfo, 0, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO); 
    siStartInfo.hStdInput = (bCreateInputPipe) ? pipe_in[IN_FOR_PARENT] : GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.hStdOutput = (bCreateOutputPipe) ? pipe_out[OUT_FOR_PARENT] : GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdError = (bCreateErrorPipe) ? pipe_err[OUT_FOR_PARENT] : GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    for(i=0;papszArgv[i] != NULL;i++)
    {
        if (i > 0)
            osCommandLine += " ";
        /* We need to quote arguments with spaces in them (if not already done) */
        if( strchr(papszArgv[i], ' ') != NULL &&
            papszArgv[i][0] != '"' )
        {
            osCommandLine += "\"";
            osCommandLine += papszArgv[i];
            osCommandLine += "\"";
        }
        else
            osCommandLine += papszArgv[i];
    }

    if (!CreateProcess(NULL, 
                       (CHAR*)osCommandLine.c_str(),
                       NULL,          // process security attributes 
                       NULL,          // primary thread security attributes 
                       TRUE,          // handles are inherited 
                       CREATE_NO_WINDOW|NORMAL_PRIORITY_CLASS,             // creation flags 
                       NULL,          // use parent's environment 
                       NULL,          // use parent's current directory 
                       &siStartInfo,
                       &piProcInfo))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not create process %s",
                 osCommandLine.c_str());
        goto err;
    }

    /* Close unused end of pipe */
    if( bCreateInputPipe )
        CloseHandle(pipe_in[IN_FOR_PARENT]);
    if( bCreateOutputPipe )
        CloseHandle(pipe_out[OUT_FOR_PARENT]);
    if( bCreateErrorPipe )
        CloseHandle(pipe_err[OUT_FOR_PARENT]);

    p = (CPLSpawnedProcess*)CPLMalloc(sizeof(CPLSpawnedProcess));
    p->hProcess = piProcInfo.hProcess;
    p->nProcessId = piProcInfo.dwProcessId;
    p->hThread = piProcInfo.hThread;
    p->fin = pipe_out[IN_FOR_PARENT];
    p->fout = pipe_in[OUT_FOR_PARENT];
    p->ferr = pipe_err[IN_FOR_PARENT];
    return p;

err_pipe:
    CPLError(CE_Failure, CPLE_AppDefined, "Could not create pipe");
err:
    for(i=0;i<2;i++)
    {
        if (pipe_in[i] != NULL)
            CloseHandle(pipe_in[i]);
        if (pipe_out[i] != NULL)
            CloseHandle(pipe_out[i]);
        if (pipe_err[i] != NULL)
            CloseHandle(pipe_err[i]);
    }

    return NULL;
}

/************************************************************************/
/*                  CPLSpawnAsyncGetChildProcessId()                    */
/************************************************************************/

CPL_PID CPLSpawnAsyncGetChildProcessId(CPLSpawnedProcess* p)
{
    return p->nProcessId;
}

/************************************************************************/
/*                        CPLSpawnAsyncFinish()                         */
/************************************************************************/

int CPLSpawnAsyncFinish(CPLSpawnedProcess* p, int bWait, int bKill)
{
    // Get the exit code.
    DWORD exitCode = -1;

    if( bWait )
    {
        WaitForSingleObject( p->hProcess, INFINITE );
        GetExitCodeProcess(p->hProcess, &exitCode);
    }
    else
        exitCode = 0;

    CloseHandle(p->hProcess);
    CloseHandle(p->hThread);

    CPLSpawnAsyncCloseInputFileHandle(p);
    CPLSpawnAsyncCloseOutputFileHandle(p);
    CPLSpawnAsyncCloseErrorFileHandle(p);
    CPLFree(p);

    return (int)exitCode;
}

/************************************************************************/
/*                 CPLSpawnAsyncCloseInputFileHandle()                  */
/************************************************************************/

void CPLSpawnAsyncCloseInputFileHandle(CPLSpawnedProcess* p)
{
    if( p->fin != NULL )
        CloseHandle(p->fin);
    p->fin = NULL;
}

/************************************************************************/
/*                 CPLSpawnAsyncCloseOutputFileHandle()                 */
/************************************************************************/

void CPLSpawnAsyncCloseOutputFileHandle(CPLSpawnedProcess* p)
{
    if( p->fout != NULL )
        CloseHandle(p->fout);
    p->fout = NULL;
}

/************************************************************************/
/*                 CPLSpawnAsyncCloseErrorFileHandle()                  */
/************************************************************************/

void CPLSpawnAsyncCloseErrorFileHandle(CPLSpawnedProcess* p)
{
    if( p->ferr != NULL )
        CloseHandle(p->ferr);
    p->ferr = NULL;
}

#else

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_POSIX_SPAWNP
    #include <spawn.h>
    #ifdef __APPLE__
        #include <TargetConditionals.h>
    #endif
    #if defined(__APPLE__) && (!defined(TARGET_OS_IPHONE) || TARGET_OS_IPHONE==0)
        #include <crt_externs.h>
        #define environ (*_NSGetEnviron())
    #else
        extern char** environ;
    #endif
#endif

#if 0
/************************************************************************/
/*                            CPLSystem()                               */
/************************************************************************/

/**
 * Runs an executable in another process.
 *
 * This function runs an executable, wait for it to finish and returns
 * its exit code.
 *
 * It is implemented as CreateProcess() on Windows platforms, and system()
 * on other platforms.
 *
 * @param pszApplicationName the lpApplicationName for Windows (might be NULL),
 *                           or ignored on other platforms.
 * @param pszCommandLine the command line, starting with the executable name 
 *
 * @return the exit code of the spawned process, or -1 in case of error.
 *
 * @since GDAL 1.10.0
 */

int CPLSystem( const char* pszApplicationName, const char* pszCommandLine )
{
    return system(pszCommandLine);
}
#endif

/************************************************************************/
/*                          CPLPipeRead()                               */
/************************************************************************/

/**
 * Read data from the standard output of a forked process.
 *
 * @param p handle returned by CPLSpawnAsyncGetInputFileHandle().
 * @param data buffer in which to write.
 * @param length number of bytes to read.
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 1.10.0
 */
int CPLPipeRead(CPL_FILE_HANDLE fin, void* data, int length)
{
    GByte* pabyData = (GByte*)data;
    int nRemain = length;
    while( nRemain > 0 )
    {
        while(TRUE)
        {
            int n = read(fin, pabyData, nRemain);
            if( n < 0 )
            {
                if( errno == EINTR )
                    continue;
                else
                    return FALSE;
            }
            else if( n == 0 )
                return FALSE;
            pabyData += n;
            nRemain -= n;
            break;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                          CPLPipeWrite()                              */
/************************************************************************/

/**
 * Write data to the standard input of a forked process.
 *
 * @param fout handle returned by CPLSpawnAsyncGetOutputFileHandle().
 * @param data buffer from which to read.
 * @param length number of bytes to write.
 *
 * @return TRUE in case of success.
 *
 * @since GDAL 1.10.0
 */
int CPLPipeWrite(CPL_FILE_HANDLE fout, const void* data, int length)
{
    const GByte* pabyData = (const GByte*)data;
    int nRemain = length;
    while( nRemain > 0 )
    {
        while(TRUE)
        {
            int n = write(fout, pabyData, nRemain);
            if( n < 0 )
            {
                if( errno == EINTR )
                    continue;
                else
                    return FALSE;
            }
            pabyData += n;
            nRemain -= n;
            break;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                          FillFileFromPipe()                              */
/************************************************************************/

static void FillFileFromPipe(CPL_FILE_HANDLE pipe_fd, VSILFILE* fout)
{
    char buf[PIPE_BUFFER_SIZE];
    while(TRUE)
    {
        int nRead = read(pipe_fd, buf, PIPE_BUFFER_SIZE);
        if (nRead <= 0)
            break;
        int nWritten = (int)VSIFWriteL(buf, 1, nRead, fout);
        if (nWritten < nRead)
            break;
    }
}

/************************************************************************/
/*                            CPLSpawnAsync()                           */
/************************************************************************/

struct _CPLSpawnedProcess
{
    pid_t pid;
    CPL_FILE_HANDLE fin;
    CPL_FILE_HANDLE fout;
    CPL_FILE_HANDLE ferr;
#ifdef HAVE_POSIX_SPAWNP
    int bFreeActions;
    posix_spawn_file_actions_t actions;
#endif
};

/**
 * Runs an executable in another process (or fork the current process)
 * and return immediately.
 *
 * This function launches an executable and returns immediately, while letting
 * the sub-process to run asynchronously.
 *
 * It is implemented as CreateProcess() on Windows platforms, and fork()/exec()
 * on other platforms.
 *
 * On Unix, a pointer of function can be provided to run in the child process,
 * without exec()'ing a new executable.
 *
 * @param pfnMain the function to run in the child process (Unix only).
 * @param papszArgv argument list of the executable to run. papszArgv[0] is the
 *                  name of the executable.
 * @param bCreateInputPipe set to TRUE to create a pipe for the child input stream.
 * @param bCreateOutputPipe set to TRUE to create a pipe for the child output stream.
 * @param bCreateErrorPipe set to TRUE to create a pipe for the child error stream.
 *
 * @return a handle, that must be freed with CPLSpawnAsyncFinish()
 *
 * @since GDAL 1.10.0
 */
CPLSpawnedProcess* CPLSpawnAsync(int (*pfnMain)(CPL_FILE_HANDLE, CPL_FILE_HANDLE),
                                 const char * const papszArgv[],
                                 int bCreateInputPipe,
                                 int bCreateOutputPipe,
                                 int bCreateErrorPipe,
                                 char** papszOptions)
{
    pid_t pid;
    int pipe_in[2] = { -1, -1 };
    int pipe_out[2] = { -1, -1 };
    int pipe_err[2] = { -1, -1 };
    int i;
    char** papszArgvDup = CSLDuplicate((char**)papszArgv);
    int bDup2In = bCreateInputPipe,
        bDup2Out = bCreateOutputPipe,
        bDup2Err = bCreateErrorPipe;

    if ((bCreateInputPipe && pipe(pipe_in)) ||
        (bCreateOutputPipe && pipe(pipe_out)) ||
        (bCreateErrorPipe && pipe(pipe_err)))
        goto err_pipe;

    /* If we don't do any file actions, posix_spawnp() might be implemented */
    /* efficiently as a vfork()/exec() pair (or if it is not available, we */
    /* can use vfork()/exec()), so if the child is cooperative */
    /* we pass the pipe handles as commandline arguments */
    if( papszArgv != NULL )
    {
        for(i=0; papszArgvDup[i] != NULL; i++)
        {
            if( bCreateInputPipe && strcmp(papszArgvDup[i], "{pipe_in}") == 0 )
            {
                CPLFree(papszArgvDup[i]);
                papszArgvDup[i] = CPLStrdup(CPLSPrintf("%d,%d",
                    pipe_in[IN_FOR_PARENT], pipe_in[OUT_FOR_PARENT]));
                bDup2In = FALSE;
            }
            else if( bCreateOutputPipe && strcmp(papszArgvDup[i], "{pipe_out}") == 0 )
            {
                CPLFree(papszArgvDup[i]);
                papszArgvDup[i] = CPLStrdup(CPLSPrintf("%d,%d",
                    pipe_out[OUT_FOR_PARENT], pipe_out[IN_FOR_PARENT]));
                bDup2Out = FALSE;
            }
            else if( bCreateErrorPipe && strcmp(papszArgvDup[i], "{pipe_err}") == 0 )
            {
                CPLFree(papszArgvDup[i]);
                papszArgvDup[i] = CPLStrdup(CPLSPrintf("%d,%d",
                    pipe_err[OUT_FOR_PARENT], pipe_err[IN_FOR_PARENT]));
                bDup2Err = FALSE;
            }
        }
    }

#ifdef HAVE_POSIX_SPAWNP
    if( papszArgv != NULL )
    {
        int bHasActions = FALSE;
        posix_spawn_file_actions_t actions;

        if( bDup2In )
        {
            if( !bHasActions ) posix_spawn_file_actions_init(&actions);
            posix_spawn_file_actions_adddup2(&actions, pipe_in[IN_FOR_PARENT], fileno(stdin));
            posix_spawn_file_actions_addclose(&actions, pipe_in[OUT_FOR_PARENT]);
            bHasActions = TRUE;
        }

        if( bDup2Out )
        {
            if( !bHasActions ) posix_spawn_file_actions_init(&actions);
            posix_spawn_file_actions_adddup2(&actions, pipe_out[OUT_FOR_PARENT], fileno(stdout));
            posix_spawn_file_actions_addclose(&actions, pipe_out[IN_FOR_PARENT]);
            bHasActions = TRUE;
        }

        if( bDup2Err )
        {
            if( !bHasActions ) posix_spawn_file_actions_init(&actions);
            posix_spawn_file_actions_adddup2(&actions, pipe_err[OUT_FOR_PARENT], fileno(stderr));
            posix_spawn_file_actions_addclose(&actions, pipe_err[IN_FOR_PARENT]);
            bHasActions = TRUE;
        }

        if( posix_spawnp(&pid, papszArgvDup[0],
                         bHasActions ? &actions : NULL,
                         NULL,
                         (char* const*) papszArgvDup,
                         environ) != 0 )
        {
            if( bHasActions )
                posix_spawn_file_actions_destroy(&actions);
            CPLError(CE_Failure, CPLE_AppDefined, "posix_spawnp() failed");
            goto err;
        }

        CSLDestroy(papszArgvDup);

        /* Close unused end of pipe */
        if( bCreateInputPipe )
            close(pipe_in[IN_FOR_PARENT]);
        if( bCreateOutputPipe )
            close(pipe_out[OUT_FOR_PARENT]);
        if( bCreateErrorPipe )
            close(pipe_err[OUT_FOR_PARENT]);

        /* Ignore SIGPIPE */
    #ifdef SIGPIPE
        signal (SIGPIPE, SIG_IGN);
    #endif
        CPLSpawnedProcess* p = (CPLSpawnedProcess*)CPLMalloc(sizeof(CPLSpawnedProcess));
        if( bHasActions )
            memcpy(&p->actions, &actions, sizeof(actions));
        p->bFreeActions = bHasActions;
        p->pid = pid;
        p->fin = pipe_out[IN_FOR_PARENT];
        p->fout = pipe_in[OUT_FOR_PARENT];
        p->ferr = pipe_err[IN_FOR_PARENT];
        return p;
    }
#endif // #ifdef HAVE_POSIX_SPAWNP

#ifdef HAVE_VFORK
    if( papszArgv != NULL && !bDup2In && !bDup2Out && !bDup2Err )
        pid = vfork();
    else
#endif
        pid = fork();
    if (pid == 0)
    {
        /* Close unused end of pipe */
        if( bDup2In )
            close(pipe_in[OUT_FOR_PARENT]);
        if( bDup2Out )
            close(pipe_out[IN_FOR_PARENT]);
        if( bDup2Err )
            close(pipe_err[IN_FOR_PARENT]);

#ifndef HAVE_POSIX_SPAWNP
        if( papszArgv != NULL )
        {
            if( bDup2In )
                dup2(pipe_in[IN_FOR_PARENT], fileno(stdin));
            if( bDup2Out )
                dup2(pipe_out[OUT_FOR_PARENT], fileno(stdout));
            if( bDup2Err )
                dup2(pipe_err[OUT_FOR_PARENT], fileno(stderr));

            execvp(papszArgvDup[0], (char* const*) papszArgvDup);

            _exit(1);
        }
        else
#endif // HAVE_POSIX_SPAWNP
        {
            if( bCreateErrorPipe )
                close(pipe_err[OUT_FOR_PARENT]);

            int nRet = 0;
            if (pfnMain != NULL)
                nRet = pfnMain((bCreateInputPipe) ? pipe_in[IN_FOR_PARENT] : fileno(stdin),
                               (bCreateOutputPipe) ? pipe_out[OUT_FOR_PARENT] : fileno(stdout));
            _exit(nRet);
        }
    }
    else if( pid > 0 )
    {
        CSLDestroy(papszArgvDup);

        /* Close unused end of pipe */
        if( bCreateInputPipe )
            close(pipe_in[IN_FOR_PARENT]);
        if( bCreateOutputPipe )
            close(pipe_out[OUT_FOR_PARENT]);
        if( bCreateErrorPipe )
            close(pipe_err[OUT_FOR_PARENT]);

        /* Ignore SIGPIPE */
#ifdef SIGPIPE
        signal (SIGPIPE, SIG_IGN);
#endif
        CPLSpawnedProcess* p = (CPLSpawnedProcess*)CPLMalloc(sizeof(CPLSpawnedProcess));
#ifdef HAVE_POSIX_SPAWNP
        p->bFreeActions = FALSE;
#endif
        p->pid = pid;
        p->fin = pipe_out[IN_FOR_PARENT];
        p->fout = pipe_in[OUT_FOR_PARENT];
        p->ferr = pipe_err[IN_FOR_PARENT];
        return p;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Fork failed");
        goto err;
    }

err_pipe:
    CPLError(CE_Failure, CPLE_AppDefined, "Could not create pipe");
err:
    CSLDestroy(papszArgvDup);
    for(i=0;i<2;i++)
    {
        if (pipe_in[i] >= 0)
            close(pipe_in[i]);
        if (pipe_out[i] >= 0)
            close(pipe_out[i]);
        if (pipe_err[i] >= 0)
            close(pipe_err[i]);
    }

    return NULL;
}

/************************************************************************/
/*                  CPLSpawnAsyncGetChildProcessId()                    */
/************************************************************************/

CPL_PID CPLSpawnAsyncGetChildProcessId(CPLSpawnedProcess* p)
{
    return p->pid;
}

/************************************************************************/
/*                        CPLSpawnAsyncFinish()                         */
/************************************************************************/

/**
 * Wait for the forked process to finish.
 *
 * @param p handle returned by CPLSpawnAsync()
 * @param bWait set to TRUE to wait for the child to terminate. Otherwise the associated
 *              handles are just cleaned.
 * @param bKill set to TRUE to force child termination (unimplemented right now).
 *
 * @return the return code of the forked process if bWait == TRUE, 0 otherwise
 *
 * @since GDAL 1.10.0
 */
int CPLSpawnAsyncFinish(CPLSpawnedProcess* p, int bWait, int bKill)
{
    int status = 0;

    if( bWait )
    {
        while(1)
        {
            status = -1;
            int ret = waitpid (p->pid, &status, 0);
            if (ret < 0)
            {
                if (errno != EINTR)
                {
                    break;
                }
            }
            else
                break;
        }
    }
    else
        bWait = FALSE;
    CPLSpawnAsyncCloseInputFileHandle(p);
    CPLSpawnAsyncCloseOutputFileHandle(p);
    CPLSpawnAsyncCloseErrorFileHandle(p);
#ifdef HAVE_POSIX_SPAWNP
    if( p->bFreeActions )
        posix_spawn_file_actions_destroy(&p->actions);
#endif
    CPLFree(p);
    return status;
}

/************************************************************************/
/*                 CPLSpawnAsyncCloseInputFileHandle()                  */
/************************************************************************/

void CPLSpawnAsyncCloseInputFileHandle(CPLSpawnedProcess* p)
{
    if( p->fin >= 0 )
        close(p->fin);
    p->fin = -1;
}

/************************************************************************/
/*                 CPLSpawnAsyncCloseOutputFileHandle()                 */
/************************************************************************/

void CPLSpawnAsyncCloseOutputFileHandle(CPLSpawnedProcess* p)
{
    if( p->fout >= 0 )
        close(p->fout);
    p->fout = -1;
}

/************************************************************************/
/*                 CPLSpawnAsyncCloseErrorFileHandle()                  */
/************************************************************************/

void CPLSpawnAsyncCloseErrorFileHandle(CPLSpawnedProcess* p)
{
    if( p->ferr >= 0 )
        close(p->ferr);
    p->ferr = -1;
}

#endif

/************************************************************************/
/*                    CPLSpawnAsyncGetInputFileHandle()                 */
/************************************************************************/

/**
 * Return the file handle of the standard output of the forked process
 * from which to read.
 *
 * @param p handle returned by CPLSpawnAsync().
 *
 * @return the file handle.
 *
 * @since GDAL 1.10.0
 */
CPL_FILE_HANDLE CPLSpawnAsyncGetInputFileHandle(CPLSpawnedProcess* p)
{
    return p->fin;
}

/************************************************************************/
/*                   CPLSpawnAsyncGetOutputFileHandle()                 */
/************************************************************************/

/**
 * Return the file handle of the standard input of the forked process
 * into which to write
 *
 * @param p handle returned by CPLSpawnAsync().
 *
 * @return the file handle.
 *
 * @since GDAL 1.10.0
 */
CPL_FILE_HANDLE CPLSpawnAsyncGetOutputFileHandle(CPLSpawnedProcess* p)
{
    return p->fout;
}

/************************************************************************/
/*                    CPLSpawnAsyncGetErrorFileHandle()                 */
/************************************************************************/

/**
 * Return the file handle of the standard error of the forked process
 * from which to read.
 *
 * @param p handle returned by CPLSpawnAsync().
 *
 * @return the file handle
 *
 * @since GDAL 1.10.0
 */
CPL_FILE_HANDLE CPLSpawnAsyncGetErrorFileHandle(CPLSpawnedProcess* p)
{
    return p->ferr;
}
