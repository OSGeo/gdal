/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Server application that is forked by libgdal
 * Author:   Even Rouault, <even dot rouault at mines-paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault, <even dot rouault at mines-paris dot org>
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

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET CPL_SOCKET;
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
typedef int CPL_SOCKET;
#endif


#include <gdal.h>
#include "cpl_spawn.h"
#include "cpl_string.h"

CPL_C_START
int CPL_DLL GDALServerLoop(CPL_FILE_HANDLE fin, CPL_FILE_HANDLE fout);
int CPL_DLL GDALServerLoopSocket(CPL_SOCKET nSocket);
CPL_C_END

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage(const char* pszErrorMsg)

{
    printf( "Usage: gdalserver [--help-general] [--help] [-run | -tcpserver port]\n");
    printf( "\n" );
    printf( "This utility is not meant at being directly used by a user.\n");
    printf( "It is a helper utility for the client/server working of GDAL.\n");

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

#ifdef WIN32

/************************************************************************/
/*                             RunTCPServer()                           */
/************************************************************************/

int RunTCPServer(const char* pszApplication, int nPort)
{
    int nRet;
    WSADATA wsaData;
    SOCKET nListenSocket;
    struct sockaddr_in sockAddrIn;

    nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (nRet != NO_ERROR)
    {
        fprintf(stderr, "WSAStartup() failed with error: %d\n", nRet);
        return 1;
    }

    nListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nListenSocket == INVALID_SOCKET)
    {
        fprintf(stderr, "socket() failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockAddrIn.sin_family = AF_INET;
    sockAddrIn.sin_addr.s_addr = INADDR_ANY;
    sockAddrIn.sin_port = htons(nPort);

    if (bind(nListenSocket, (SOCKADDR *)&sockAddrIn, sizeof (sockAddrIn)) == SOCKET_ERROR)
    {
        fprintf(stderr, "bind() function failed with error: %d\n", WSAGetLastError());
        closesocket(nListenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(nListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        fprintf(stderr, "listen() function failed with error: %d\n", WSAGetLastError());
        closesocket(nListenSocket);
        WSACleanup();
        return 1;
    }

    while(TRUE)
    {
        WSAPROTOCOL_INFO sSocketInfo;
        struct sockaddr sockAddr;
        socklen_t nLen = sizeof(sockAddr);
        CPLSpawnedProcess* psProcess;
        CPL_FILE_HANDLE fin, fout;
        CPL_PID nPid;
        SOCKET nConnSocket;
        char szReady[5];
        const char* apszArgs[] = { NULL, "-newconnection", NULL };

        apszArgs[0] = pszApplication;
        nConnSocket = accept(nListenSocket, &sockAddr, &nLen);
        if( nConnSocket == SOCKET_ERROR )
        {
            fprintf(stderr, "accept() function failed with error: %d\n", WSAGetLastError());
            closesocket(nListenSocket);
            WSACleanup();
            return 1;
        }

        psProcess = CPLSpawnAsync( NULL,
                                   apszArgs,
                                   TRUE,
                                   TRUE,
                                   FALSE,
                                   NULL);
        if( psProcess == NULL )
        {
            fprintf(stderr, "CPLSpawnAsync() function failed.\n");
            closesocket(nConnSocket);
            closesocket(nListenSocket);
            WSACleanup();
            return 1;
        }

        nPid = CPLSpawnAsyncGetChildProcessId(psProcess);
        fin = CPLSpawnAsyncGetInputFileHandle(psProcess);
        fout = CPLSpawnAsyncGetOutputFileHandle(psProcess);

        if( WSADuplicateSocket(nConnSocket, nPid, &sSocketInfo) != 0 )
        {
            fprintf(stderr, "WSADuplicateSocket() failed: %d\n", WSAGetLastError());
            CPLSpawnAsyncFinish(psProcess, FALSE, TRUE);
            closesocket(nConnSocket);
            closesocket(nListenSocket);
            WSACleanup();
            return 1;
        }

        if (!CPLPipeWrite(fout, &sSocketInfo, sizeof(sSocketInfo)))
        {
            fprintf(stderr, "CPLWritePipe() failed\n");
            CPLSpawnAsyncFinish(psProcess, FALSE, TRUE);
            closesocket(nConnSocket);
            closesocket(nListenSocket);
            WSACleanup();
            return 1;
        }

        if (!CPLPipeRead(fin, szReady, sizeof(szReady)))
        {
            fprintf(stderr, "CPLReadPipe() failed\n");
            CPLSpawnAsyncFinish(psProcess, FALSE, TRUE);
            closesocket(nConnSocket);
            closesocket(nListenSocket);
            WSACleanup();
            return 1;
        }

        closesocket(nConnSocket);

        CPLSpawnAsyncFinish(psProcess, FALSE, FALSE);
    }

    // closesocket(nConnSocket);
    // WSACleanup();
}

/************************************************************************/
/*                          RunNewConnection()                          */
/************************************************************************/

int RunNewConnection()
{
    int nRet;
    WSADATA wsaData;
    WSAPROTOCOL_INFO sSocketInfo;
    SOCKET nConnSocket;

    if( fread(&sSocketInfo, sizeof(sSocketInfo), 1, stdin) != 1 )
    {
        fprintf(stderr, "fread failed\n");
        return 1;
    }

    nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (nRet != NO_ERROR)
    {
        fprintf(stderr, "WSAStartup() failed with error: %d\n", nRet);
        return 1;
    }

    nConnSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, &sSocketInfo, 0, WSA_FLAG_OVERLAPPED);
    if (nConnSocket == INVALID_SOCKET)
    {
        fprintf(stderr, "ConnSocket() failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    if( fwrite("ready", 5, 1, stdout) != 1 )
    {
        fprintf(stderr, "fwrite() failed\n");
        closesocket(nConnSocket);
        WSACleanup();
        return 1;
    }
    fflush(stdout);

#ifdef _MSC_VER
    __try {
#endif
    nRet = GDALServerLoopSocket(nConnSocket);
#ifdef _MSC_VER
    } __except(1) 
    {
        fprintf(stderr, "gdalserver exited with a fatal error.\n");
        nRet = 1;
    }
#endif

    closesocket(nConnSocket);
    WSACleanup();

    return nRet;
}

#else

/************************************************************************/
/*                             RunTCPServer()                           */
/************************************************************************/

int RunTCPServer(const char* pszApplication, int nPort)
{
    int nListenSocket;
    struct sockaddr_in sockAddrIn;

    nListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nListenSocket < 0)
    {
        fprintf(stderr, "socket() failed with error: %d\n", errno);
        return 1;
    }

    sockAddrIn.sin_family = AF_INET;
    sockAddrIn.sin_addr.s_addr = INADDR_ANY;
    sockAddrIn.sin_port = htons(nPort);

    if (bind(nListenSocket, (struct sockaddr *)&sockAddrIn, sizeof (sockAddrIn)) < 0)
    {
        fprintf(stderr, "bind() function failed with error: %d\n", errno);
        close(nListenSocket);
        return 1;
    }

    if (listen(nListenSocket, SOMAXCONN) < 0)
    {
        fprintf(stderr, "listen() function failed with error: %d\n", errno);
        close(nListenSocket);
        return 1;
    }

    while(TRUE)
    {
        struct sockaddr sockAddr;
        socklen_t nLen = sizeof(sockAddr);
        int nConnSocket;
        pid_t pid;
        int nStatus;

        waitpid(-1, &nStatus, WNOHANG);
        nConnSocket = accept(nListenSocket, &sockAddr, &nLen);
        if( nConnSocket < 0 )
        {
            fprintf(stderr, "accept() function failed with error: %d\n", errno);
            close(nListenSocket);
            return 1;
        }

        pid = fork();
        if( pid < 0 )
        {
            fprintf(stderr, "fork() failed: %d\n", errno);
            close(nListenSocket);
            close(nConnSocket);
            return 1;
        }
        else if( pid == 0 )
        {
            int nRet;
            close(nListenSocket);
            nRet = GDALServerLoopSocket(nConnSocket);
            close(nConnSocket);
            return nRet;
        }
        else
        {
            close(nConnSocket);
        }
    }
}

#endif

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (i + nExtraArg >= argc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", argv[i], nExtraArg)); } while(0)

int main(int argc, char* argv[])
{
    int i, nRet, bRun = FALSE, nPort = -1, bNewConnection = FALSE;

    /*for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "-daemonize") )
        {
            daemon(0, 0);
            break;
        }
    }*/

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
            Usage(NULL);
        else if( EQUAL(argv[i],"-tcpserver") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            i++;
            nPort = atoi(argv[i]);
        }
#ifdef WIN32
        else if( EQUAL(argv[i],"-newconnection") )
        {
            bNewConnection = TRUE;
        }
#endif
        else if( EQUAL(argv[i],"-run") )
            bRun = TRUE;
        else if( EQUAL(argv[i], "-daemonize") )
            ;
        else if( argv[i][0] == '-' )
            Usage(CPLSPrintf("Unkown option name '%s'", argv[i]));
        else
            Usage("Too many command options.");
    }
    if( !bRun && nPort < 0 && !bNewConnection )
        Usage(NULL);

    if( nPort > 0 )
        nRet = RunTCPServer(argv[0], nPort);
#ifdef WIN32
    else if( bNewConnection )
        nRet = RunNewConnection();
#endif

    else
    {
#ifdef WIN32
#ifdef _MSC_VER
    __try 
#endif
    { 
        nRet = GDALServerLoop(GetStdHandle(STD_INPUT_HANDLE),
                              GetStdHandle(STD_OUTPUT_HANDLE));
    }
#ifdef _MSC_VER
    __except(1) 
    {
        fprintf(stderr, "gdalserver exited with a fatal error.\n");
        nRet = 1;
    }
#endif
#else
    nRet = GDALServerLoop(fileno(stdin),
                          fileno(stdout));
#endif
    }

    CSLDestroy(argv);

    return nRet;
}
