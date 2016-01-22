/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Server application that is forked by libgdal
 * Author:   Even Rouault, <even dot rouault at mines-paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

// So that __USE_XOPEN2K is defined to have getaddrinfo
#define _XOPEN_SOURCE 600

#include "cpl_port.h"

#ifdef WIN32
  #ifdef _WIN32_WINNT
    #undef _WIN32_WINNT
  #endif
  #define _WIN32_WINNT 0x0501
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET CPL_SOCKET;
  #ifndef HAVE_GETADDRINFO
    #define HAVE_GETADDRINFO 1
  #endif
#else
  #include <sys/time.h>
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <sys/socket.h>
  #include <sys/un.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #ifdef HAVE_GETADDRINFO
    #include <netdb.h>
  #endif
  typedef int CPL_SOCKET;
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
  #define SOCKADDR struct sockaddr
  #define WSAGetLastError() errno
  #define WSACleanup()
  #define closesocket(s) close(s)
  #ifndef SOMAXCONN
  #define SOMAXCONN 128
  #endif 
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
#ifdef WIN32
    printf( "Usage: gdalserver [--help-general] [--help] [-tcpserver port | -stdinout]\n");
#else
    printf( "Usage: gdalserver [--help-general] [--help] [-tcpserver port | -unixserver filename | -stdinout | [-pipe_in fdin,fdtoclose -pipe_out fdout,fdtoclose]]\n");
#endif
    printf( "\n" );
    printf( "-tcpserver : Launch a TCP server on the specified port that can accept.\n");
    printf( "             connections from GDAL clients.\n");
    printf( "-stdinout  : This mode is not meant at being directly used by a user.\n");
    printf( "             It is a helper utility for the client/server working of GDAL.\n");
#ifndef WIN32
    printf( "-pipe_in/out:This mode is not meant at being directly used by a user.\n");
    printf( "             It is a helper utility for the client/server working of GDAL.\n");
#endif

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}


/************************************************************************/
/*                CreateSocketAndBindAndListen()                        */
/************************************************************************/

int CreateSocketAndBindAndListen(const char* pszService,
                                 int *pnFamily,
                                 int *pnSockType,
                                 int *pnProtocol)
{
    CPL_SOCKET nListenSocket = INVALID_SOCKET;

#ifdef HAVE_GETADDRINFO
    int nRet;
    struct addrinfo sHints;
    struct addrinfo* psResults = NULL, *psResultsIter;
    memset(&sHints, 0, sizeof(struct addrinfo));
    sHints.ai_family = AF_UNSPEC;
    sHints.ai_socktype = SOCK_STREAM;
    sHints.ai_flags = AI_PASSIVE;
    sHints.ai_protocol = IPPROTO_TCP;

    nRet = getaddrinfo(NULL, pszService, &sHints, &psResults);
    if (nRet)
    {
        fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(nRet));
        return INVALID_SOCKET;
    }

    for( psResultsIter = psResults;
         psResultsIter != NULL;
         psResultsIter = psResultsIter->ai_next)
    {
        nListenSocket = socket(psResultsIter->ai_family,
                               psResultsIter->ai_socktype,
                               psResultsIter->ai_protocol);
        if (nListenSocket == INVALID_SOCKET)
            continue;

        if (bind(nListenSocket, psResultsIter->ai_addr,
                 psResultsIter->ai_addrlen) != SOCKET_ERROR)
        {
            if( pnFamily )   *pnFamily =   psResultsIter->ai_family;
            if( pnSockType ) *pnSockType = psResultsIter->ai_socktype;
            if( pnProtocol ) *pnProtocol = psResultsIter->ai_protocol;

            break;
        }

        closesocket(nListenSocket);
    }

    freeaddrinfo(psResults);

    if (psResultsIter == NULL)
    {
        fprintf(stderr, "Could not bind()\n");
        return INVALID_SOCKET;
    }

#else

    struct sockaddr_in sockAddrIn;

    if( pnFamily )   *pnFamily = AF_INET;
    if( pnSockType ) *pnSockType = SOCK_STREAM;
    if( pnProtocol ) *pnProtocol = IPPROTO_TCP;

    nListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nListenSocket == INVALID_SOCKET)
    {
        fprintf(stderr, "socket() failed with error: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }

    sockAddrIn.sin_family = AF_INET;
    sockAddrIn.sin_addr.s_addr = INADDR_ANY;
    sockAddrIn.sin_port = htons(atoi(pszService));

    if (bind(nListenSocket, (SOCKADDR *)&sockAddrIn, sizeof (sockAddrIn)) == SOCKET_ERROR)
    {
        fprintf(stderr, "bind() function failed with error: %d\n", WSAGetLastError());
        closesocket(nListenSocket);
        return INVALID_SOCKET;
    }

#endif

    if (listen(nListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        fprintf(stderr, "listen() function failed with error: %d\n", WSAGetLastError());
        closesocket(nListenSocket);
        return INVALID_SOCKET;
    }

    return nListenSocket;
}

#ifdef WIN32

/************************************************************************/
/*                             RunServer()                              */
/************************************************************************/

int RunServer(const char* pszApplication,
              const char* pszService,
              const char* unused_pszUnixSocketFilename)
{
    int nRet;
    WSADATA wsaData;
    SOCKET nListenSocket;
    int nFamily, nSockType, nProtocol;

    nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (nRet != NO_ERROR)
    {
        fprintf(stderr, "WSAStartup() failed with error: %d\n", nRet);
        return 1;
    }

    nListenSocket = CreateSocketAndBindAndListen(pszService, &nFamily, &nSockType, &nProtocol);
    if (nListenSocket == INVALID_SOCKET)
    {
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
        int bOK = TRUE;
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
            bOK = FALSE;
        }

        /* Send socket parameters over the pipe */
        if( bOK &&
            (!CPLPipeWrite(fout, &sSocketInfo, sizeof(sSocketInfo)) ||
             !CPLPipeWrite(fout, &nFamily, sizeof(nFamily)) ||
             !CPLPipeWrite(fout, &nSockType, sizeof(nSockType)) ||
             !CPLPipeWrite(fout, &nProtocol, sizeof(nProtocol))) )
        {
            fprintf(stderr, "CPLWritePipe() failed\n");
            bOK = FALSE;
        }

        /* Wait for child to be ready before closing the socket */
        if( bOK && !CPLPipeRead(fin, szReady, sizeof(szReady)) )
        {
            fprintf(stderr, "CPLReadPipe() failed\n");
            bOK = FALSE;
        }

        if( !bOK )
        {
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
    int nFamily, nSockType, nProtocol;
    SOCKET nConnSocket;
    CPL_FILE_HANDLE fin = GetStdHandle(STD_INPUT_HANDLE);
    CPL_FILE_HANDLE fout = GetStdHandle(STD_OUTPUT_HANDLE);

    /* Get socket parameters from the pipe */
    if (!CPLPipeRead(fin, &sSocketInfo, sizeof(sSocketInfo)) ||
        !CPLPipeRead(fin, &nFamily, sizeof(nFamily)) ||
        !CPLPipeRead(fin, &nSockType, sizeof(nSockType)) ||
        !CPLPipeRead(fin, &nProtocol, sizeof(nProtocol)) )
    {
        fprintf(stderr, "CPLPipeRead() failed\n");
        return 1;
    }

    nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (nRet != NO_ERROR)
    {
        fprintf(stderr, "WSAStartup() failed with error: %d\n", nRet);
        return 1;
    }

    nConnSocket = WSASocket(nFamily, nSockType, nProtocol, &sSocketInfo, 0, WSA_FLAG_OVERLAPPED);
    if (nConnSocket == INVALID_SOCKET)
    {
        fprintf(stderr, "ConnSocket() failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    /* Warn the parent that we are now ready */
    if (!CPLPipeWrite(fout, "ready", 5))
    {
        fprintf(stderr, "CPLPipeWrite() failed\n");
        WSACleanup();
        return 1;
    }
    CloseHandle(fout);

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
/*                             RunServer()                              */
/************************************************************************/

int RunServer(CPL_UNUSED const char* pszApplication,
              const char* pszService,
              const char* pszUnixSocketFilename)
{
    int nListenSocket;

    if( pszUnixSocketFilename != NULL )
    {
        struct sockaddr_un sockAddrUnix;
        int len;

        nListenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (nListenSocket < 0)
        {
            perror("socket");
            return 1;
        }

        sockAddrUnix.sun_family = AF_UNIX;
        CPLStrlcpy(sockAddrUnix.sun_path, pszUnixSocketFilename, sizeof(sockAddrUnix.sun_path));
        unlink(sockAddrUnix.sun_path);
        len = strlen(sockAddrUnix.sun_path) + sizeof(sockAddrUnix.sun_family);
        if (bind(nListenSocket, (struct sockaddr *)&sockAddrUnix, len) == -1)
        {
            perror("bind");
            closesocket(nListenSocket);
            return 1;
        }
        if (listen(nListenSocket, SOMAXCONN) == SOCKET_ERROR)
        {
            fprintf(stderr, "listen() function failed with error: %d\n", WSAGetLastError());
            closesocket(nListenSocket);
            return 1;
        }
    }
    else
    {
        nListenSocket = CreateSocketAndBindAndListen(pszService, NULL, NULL, NULL);
        if (nListenSocket < 0)
        {
            return 1;
        }
    }

    while(TRUE)
    {
        struct sockaddr sockAddr;
        socklen_t nLen = sizeof(sockAddr);
        int nConnSocket;
        pid_t pid;
        int nStatus;
        struct timeval tv;
        fd_set read_fds;

        /* Select on the listen socket, and rip zombie children every second */
        do
        {
            FD_ZERO(&read_fds);
            FD_SET(nListenSocket, &read_fds);
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            waitpid(-1, &nStatus, WNOHANG);
        }
        while( select(nListenSocket + 1, &read_fds, NULL, NULL, &tv) != 1 );

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
    int i, nRet, bStdinout = FALSE, bPipeIn = FALSE, bPipeOut = FALSE, bNewConnection = FALSE;
    const char* pszService = NULL, *pszUnixSocketFilename = NULL;
#ifndef WIN32
    int pipe_in = fileno(stdin);
    int pipe_out = fileno(stdout);
#endif
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
            pszService = argv[i];
        }
#ifndef WIN32
        else if( EQUAL(argv[i],"-unixserver") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            i++;
            pszUnixSocketFilename = argv[i];
        }
#endif
#ifdef WIN32
        else if( EQUAL(argv[i],"-newconnection") )
        {
            bNewConnection = TRUE;
        }
#endif
        else if( EQUAL(argv[i],"-stdinout") )
            bStdinout = TRUE;
#ifndef WIN32
        else if( EQUAL(argv[i],"-pipe_in") )
        {
            const char* pszComma;
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            i++;
            pipe_in = atoi(argv[i]);
            bPipeIn = TRUE;
            pszComma = strchr(argv[i], ',');
            if( pszComma )
                close(atoi(pszComma + 1));
        }
        else if( EQUAL(argv[i],"-pipe_out") )
        {
            const char* pszComma;
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            i++;
            pipe_out = atoi(argv[i]);
            bPipeOut = TRUE;
            pszComma = strchr(argv[i], ',');
            if( pszComma )
                close(atoi(pszComma + 1));
        }
#endif
        else if( EQUAL(argv[i], "-daemonize") )
            ;
        else if( argv[i][0] == '-' )
            Usage(CPLSPrintf("Unknown option name '%s'", argv[i]));
        else
            Usage("Too many command options.");
    }
    if( !bStdinout && !(bPipeIn && bPipeOut) &&
        pszService == NULL && pszUnixSocketFilename == NULL && !bNewConnection )
        Usage(NULL);

    if( pszService != NULL || pszUnixSocketFilename != NULL )
        nRet = RunServer(argv[0], pszService, pszUnixSocketFilename);
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
    nRet = GDALServerLoop(pipe_in, pipe_out);
#endif
    }

    CSLDestroy(argv);

    return nRet;
}
