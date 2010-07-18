/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Utility to fork a process and pipe data into/from it
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_vsi.h"
#include "cpl_error.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"

#include "ogr_gpsbabel.h"

#define PIPE_BUFFER_SIZE    4096

#define IN_FOR_PARENT   0
#define OUT_FOR_PARENT  1


#ifndef WIN32

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

static void WriteToPipe(FILE* fin, int pipe_fd)
{
    char buf[PIPE_BUFFER_SIZE];
    while(TRUE)
    {
        int nRead = (int)VSIFReadL(buf, 1, PIPE_BUFFER_SIZE, fin);
        int nWritten = write(pipe_fd, buf, nRead);
        if (nWritten < nRead || nRead < PIPE_BUFFER_SIZE)
            break;
    }
}

static void ReadFromPipe(int pipe_fd, FILE* fout)
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

int ForkAndPipe(const char * const argv[], FILE* fin, FILE* fout)
{
    pid_t pid;
    int pipe_in[2] = { -1, -1 };
    int pipe_out[2] = { -1, -1 };
    int pipe_err[2] = { -1, -1 };
    int i;

    if (pipe(pipe_in) ||
        pipe(pipe_out) ||
        pipe(pipe_err))
        goto err_pipe;

    pid = fork();
    if (pid == 0)
    {
        /* Close unused end of pipe */
        close(pipe_in[OUT_FOR_PARENT]);
        close(pipe_out[IN_FOR_PARENT]);
        close(pipe_err[IN_FOR_PARENT]);

        dup2(pipe_in[IN_FOR_PARENT], fileno(stdin));
        dup2(pipe_out[OUT_FOR_PARENT], fileno(stdout));
        dup2(pipe_err[OUT_FOR_PARENT], fileno(stderr));

        execvp(argv[0], (char* const*) argv);

        char* pszErr = strerror(errno);

        fprintf(stderr, "An error occured while forking process %s : %s", argv[0], pszErr);

        exit(1);
    }
    else if (pid < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "fork() failed");
        goto err;
    }
    else
    {
        /* Close unused end of pipe */
        close(pipe_in[IN_FOR_PARENT]);
        close(pipe_out[OUT_FOR_PARENT]);
        close(pipe_err[OUT_FOR_PARENT]);

        /* Ignore SIGPIPE */
#ifdef SIGPIPE
        signal (SIGPIPE, SIG_IGN);
#endif

        if (fin != NULL)
            WriteToPipe(fin, pipe_in[OUT_FOR_PARENT]);
        close(pipe_in[OUT_FOR_PARENT]);

        if (fout != NULL)
            ReadFromPipe(pipe_out[IN_FOR_PARENT], fout);
        close(pipe_out[IN_FOR_PARENT]);

        CPLString osName;
        osName.Printf("/vsimem/child_stderr_" CPL_FRMT_GIB, CPLGetPID());
        FILE* ferr = VSIFOpenL(osName.c_str(), "w");
        ReadFromPipe(pipe_err[IN_FOR_PARENT], ferr);
        close(pipe_err[IN_FOR_PARENT]);
        VSIFCloseL(ferr);
        vsi_l_offset nDataLength = 0;
        GByte* pData = VSIGetMemFileBuffer(osName.c_str(), &nDataLength, TRUE);
        if (pData)
            CPLError(CE_Failure, CPLE_AppDefined, "[%s error] %s", argv[0], pData);
        CPLFree(pData);

        while(1)
        {
            int status;
            int ret = waitpid (pid, &status, 0);
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
        return pData == NULL;
    }
err_pipe:
    CPLError(CE_Failure, CPLE_AppDefined, "Could not create pipe");
err:
    for(i=0;i<2;i++)
    {
        if (pipe_in[i] >= 0)
            close(pipe_in[i]);
        if (pipe_out[i] >= 0)
            close(pipe_out[i]);
        if (pipe_err[i] >= 0)
            close(pipe_err[i]);
    }

    return FALSE;
}

#else

#include <windows.h>

static void WriteToPipe(FILE* fin, HANDLE pipe_fd)
{
    char buf[PIPE_BUFFER_SIZE];
    while(TRUE)
    {
        int nRead = (int)VSIFReadL(buf, 1, PIPE_BUFFER_SIZE, fin);
        DWORD nWritten;
        if (!WriteFile(pipe_fd, buf, nRead, &nWritten, NULL))
            break;
        if ((int)nWritten < nRead || nRead < PIPE_BUFFER_SIZE)
            break;
    }
}

static void ReadFromPipe(HANDLE pipe_fd, FILE* fout)
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

int ForkAndPipe(const char * const argv[], FILE* fin, FILE* fout)
{
    HANDLE pipe_in[2] = {NULL, NULL};
    HANDLE pipe_out[2] = {NULL, NULL};
    HANDLE pipe_err[2] = {NULL, NULL};
    SECURITY_ATTRIBUTES saAttr;
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    CPLString osCommandLine;
    int i;
    CPLString osName;
    FILE* ferr;
    vsi_l_offset nDataLength = 0;
    GByte* pData;

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&pipe_in[IN_FOR_PARENT],&pipe_in[OUT_FOR_PARENT],&saAttr, 0))
        goto err_pipe;
    /* The child must not inherit from the write side of the pipe_in */
    if (!SetHandleInformation(pipe_in[OUT_FOR_PARENT],HANDLE_FLAG_INHERIT,0))
        goto err_pipe;

    if (!CreatePipe(&pipe_out[IN_FOR_PARENT],&pipe_out[OUT_FOR_PARENT],&saAttr, 0))
        goto err_pipe;
    /* The child must not inherit from the read side of the pipe_out */
    if (!SetHandleInformation(pipe_out[IN_FOR_PARENT],HANDLE_FLAG_INHERIT,0))
        goto err_pipe;

    if (!CreatePipe(&pipe_err[IN_FOR_PARENT],&pipe_err[OUT_FOR_PARENT],&saAttr, 0))
        goto err_pipe;
    /* The child must not inherit from the read side of the pipe_err */
    if (!SetHandleInformation(pipe_err[IN_FOR_PARENT],HANDLE_FLAG_INHERIT,0))
        goto err_pipe;

    memset(&piProcInfo, 0, sizeof(PROCESS_INFORMATION));
    memset(&siStartInfo, 0, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO); 
    siStartInfo.hStdInput = pipe_in[IN_FOR_PARENT];
    siStartInfo.hStdOutput = pipe_out[OUT_FOR_PARENT];
    siStartInfo.hStdError = pipe_err[OUT_FOR_PARENT];
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    for(i=0;argv[i] != NULL;i++)
    {
        if (i > 0)
            osCommandLine += " ";
        osCommandLine += argv[i];
    }

    if (!CreateProcess(NULL, 
                       (CHAR*)osCommandLine.c_str(),
                       NULL,          // process security attributes 
                       NULL,          // primary thread security attributes 
                       TRUE,          // handles are inherited 
                       0,             // creation flags 
                       NULL,          // use parent's environment 
                       NULL,          // use parent's current directory 
                       &siStartInfo,
                       &piProcInfo))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not create process %s",
                 osCommandLine.c_str());
        goto err;
    }

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(pipe_in[IN_FOR_PARENT]);

    if (fin != NULL)
        WriteToPipe(fin, pipe_in[OUT_FOR_PARENT]);
    CloseHandle(pipe_in[OUT_FOR_PARENT]);

    CloseHandle(pipe_out[OUT_FOR_PARENT]);
    if (fout != NULL)
        ReadFromPipe(pipe_out[IN_FOR_PARENT], fout);

    osName.Printf("/vsimem/child_stderr_" CPL_FRMT_GIB, CPLGetPID());
    ferr = VSIFOpenL(osName.c_str(), "w");
    CloseHandle(pipe_err[OUT_FOR_PARENT]);
    ReadFromPipe(pipe_err[IN_FOR_PARENT], ferr);
    VSIFCloseL(ferr);
    pData = VSIGetMemFileBuffer(osName.c_str(), &nDataLength, TRUE);
    if (pData)
        CPLError(CE_Failure, CPLE_AppDefined, "[%s error] %s", argv[0], pData);
    CPLFree(pData);

    CloseHandle(pipe_out[IN_FOR_PARENT]);
    CloseHandle(pipe_err[IN_FOR_PARENT]);

    return pData == NULL;

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

    return FALSE;
}

#endif
