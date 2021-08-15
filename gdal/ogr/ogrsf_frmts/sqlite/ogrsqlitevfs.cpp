/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements SQLite VFS
 * Author:   Even Rouault, <even dot rouault at spatialys.com>

 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_sqlite.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_atomic_ops.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogrsqlitevfs.h"

CPL_CVSID("$Id$")

#ifdef DEBUG_IO
# define DEBUG_ONLY
#else
# define DEBUG_ONLY CPL_UNUSED
#endif

//#define DEBUG_IO 1

typedef struct
{
    char                       szVFSName[64];
    sqlite3_vfs               *pDefaultVFS;
    pfnNotifyFileOpenedType    pfn;
    void                      *pfnUserData;
    int                        nCounter;
} OGRSQLiteVFSAppDataStruct;

#define GET_UNDERLYING_VFS(pVFS)  ((OGRSQLiteVFSAppDataStruct* )pVFS->pAppData)->pDefaultVFS

typedef struct
{
    const struct sqlite3_io_methods *pMethods;
    VSILFILE                        *fp;
    int                              bDeleteOnClose;
    char                            *pszFilename;
} OGRSQLiteFileStruct;

static int OGRSQLiteIOClose(sqlite3_file* pFile)
{
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteIOClose(%p (%s))", pMyFile->fp, pMyFile->pszFilename);
#endif
    VSIFCloseL(pMyFile->fp);
    if (pMyFile->bDeleteOnClose)
        VSIUnlink(pMyFile->pszFilename);
    CPLFree(pMyFile->pszFilename);
    return SQLITE_OK;
}

static int OGRSQLiteIORead(sqlite3_file* pFile, void* pBuffer, int iAmt, sqlite3_int64 iOfst)
{
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    VSIFSeekL(pMyFile->fp, (vsi_l_offset) iOfst, SEEK_SET);
    int nRead = (int)VSIFReadL(pBuffer, 1, iAmt, pMyFile->fp);
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteIORead(%p, %d, %d) = %d", pMyFile->fp, iAmt, (int)iOfst, nRead);
#endif
    if (nRead < iAmt)
    {
        memset(((char*)pBuffer) + nRead, 0, iAmt - nRead);
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

static int OGRSQLiteIOWrite(sqlite3_file* pFile, const void* pBuffer, int iAmt, sqlite3_int64 iOfst)
{
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    VSIFSeekL(pMyFile->fp, (vsi_l_offset) iOfst, SEEK_SET);
    int nWritten = (int)VSIFWriteL(pBuffer, 1, iAmt, pMyFile->fp);
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteIOWrite(%p, %d, %d) = %d", pMyFile->fp, iAmt, (int)iOfst, nWritten);
#endif
    if (nWritten < iAmt)
    {
        return SQLITE_IOERR_WRITE;
    }
    return SQLITE_OK;
}

static int OGRSQLiteIOTruncate(sqlite3_file* pFile, sqlite3_int64 size)
{
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteIOTruncate(%p, " CPL_FRMT_GIB ")", pMyFile->fp, size);
#endif
    int nRet = VSIFTruncateL(pMyFile->fp, size);
    return (nRet == 0) ? SQLITE_OK : SQLITE_IOERR_TRUNCATE;
}

static int OGRSQLiteIOSync(DEBUG_ONLY sqlite3_file* pFile, DEBUG_ONLY int flags)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOSync(%p, %d)", pMyFile->fp, flags);
#endif
    return SQLITE_OK;
}

static int OGRSQLiteIOFileSize(sqlite3_file* pFile, sqlite3_int64 *pSize)
{
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    vsi_l_offset nCurOffset = VSIFTellL(pMyFile->fp);
    VSIFSeekL(pMyFile->fp, 0, SEEK_END);
    *pSize = VSIFTellL(pMyFile->fp);
    VSIFSeekL(pMyFile->fp, nCurOffset, SEEK_SET);
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteIOFileSize(%p) = " CPL_FRMT_GIB, pMyFile->fp, *pSize);
#endif
    return SQLITE_OK;
}

static int OGRSQLiteIOLock(DEBUG_ONLY sqlite3_file* pFile, DEBUG_ONLY int flags)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOLock(%p)", pMyFile->fp);
#endif
    return SQLITE_OK;
}

static int OGRSQLiteIOUnlock(DEBUG_ONLY sqlite3_file* pFile, DEBUG_ONLY int flags)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOUnlock(%p)", pMyFile->fp);
#endif
    return SQLITE_OK;
}

static int OGRSQLiteIOCheckReservedLock(DEBUG_ONLY sqlite3_file* pFile,
                                        DEBUG_ONLY int *pResOut)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOCheckReservedLock(%p)", pMyFile->fp);
#endif
    *pResOut = 0;
    return SQLITE_OK;
}

static int OGRSQLiteIOFileControl(DEBUG_ONLY sqlite3_file* pFile,
                           DEBUG_ONLY int op,
                           DEBUG_ONLY void *pArg)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOFileControl(%p, %d)", pMyFile->fp, op);
#endif
    return SQLITE_NOTFOUND;
}

static int OGRSQLiteIOSectorSize(DEBUG_ONLY sqlite3_file* pFile)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOSectorSize(%p)", pMyFile->fp);
#endif
    return 0;
}

static int OGRSQLiteIODeviceCharacteristics(DEBUG_ONLY sqlite3_file* pFile)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIODeviceCharacteristics(%p)", pMyFile->fp);
#endif
    return 0;
}

static const sqlite3_io_methods OGRSQLiteIOMethods =
{
    1,
    OGRSQLiteIOClose,
    OGRSQLiteIORead,
    OGRSQLiteIOWrite,
    OGRSQLiteIOTruncate,
    OGRSQLiteIOSync,
    OGRSQLiteIOFileSize,
    OGRSQLiteIOLock,
    OGRSQLiteIOUnlock,
    OGRSQLiteIOCheckReservedLock,
    OGRSQLiteIOFileControl,
    OGRSQLiteIOSectorSize,
    OGRSQLiteIODeviceCharacteristics,
#if SQLITE_VERSION_NUMBER >= 3007004L /* perhaps older too ? */
    nullptr,  // xShmMap
    nullptr,  // xShmLock
    nullptr,  // xShmBarrier
    nullptr,  // xShmUnmap
#if SQLITE_VERSION_NUMBER >= 3007017L /* perhaps older too ? */
    nullptr,  // xFetch
    nullptr,  // xUnfetch
#endif
#endif
};

static int OGRSQLiteVFSOpen(sqlite3_vfs* pVFS,
                            const char *zName,
                            sqlite3_file* pFile,
                            int flags,
                            int *pOutFlags)
{
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteVFSOpen(%s, %d)", zName ? zName : "(null)", flags);
#endif

    OGRSQLiteVFSAppDataStruct* pAppData = (OGRSQLiteVFSAppDataStruct* )pVFS->pAppData;

    if (zName == nullptr)
    {
        zName = CPLSPrintf("/vsimem/sqlite/%p_%d",
                           pVFS, CPLAtomicInc(&(pAppData->nCounter)));
    }

    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    pMyFile->pMethods = nullptr;
    pMyFile->bDeleteOnClose = FALSE;
    pMyFile->pszFilename = nullptr;
    if ( flags & SQLITE_OPEN_READONLY )
        pMyFile->fp = VSIFOpenL(zName, "rb");
    else if ( flags & SQLITE_OPEN_CREATE )
        pMyFile->fp = VSIFOpenL(zName, "wb+");
    else if ( flags & SQLITE_OPEN_READWRITE )
        pMyFile->fp = VSIFOpenL(zName, "rb+");
    else
        pMyFile->fp = nullptr;

    if (pMyFile->fp == nullptr)
        return SQLITE_CANTOPEN;

#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteVFSOpen() = %p", pMyFile->fp);
#endif

    pfnNotifyFileOpenedType pfn = pAppData->pfn;
    if (pfn)
    {
        pfn(pAppData->pfnUserData, zName, pMyFile->fp);
    }

    pMyFile->pMethods = &OGRSQLiteIOMethods;
    pMyFile->bDeleteOnClose = ( flags & SQLITE_OPEN_DELETEONCLOSE );
    pMyFile->pszFilename = CPLStrdup(zName);

    if (pOutFlags != nullptr)
        *pOutFlags = flags;

    return SQLITE_OK;
}

static int OGRSQLiteVFSDelete(DEBUG_ONLY sqlite3_vfs* pVFS,
                           const char *zName, int DEBUG_ONLY syncDir)
{
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteVFSDelete(%s)", zName);
#endif
    VSIUnlink(zName);
    return SQLITE_OK;
}

static int OGRSQLiteVFSAccess (DEBUG_ONLY sqlite3_vfs* pVFS,
                               const char *zName,
                               int flags,
                               int *pResOut)
{
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteVFSAccess(%s, %d)", zName, flags);
#endif
    VSIStatBufL sStatBufL;
    int nRet;  // TODO(schwehr): Cleanup nRet and pResOut.  bools?
    if (flags == SQLITE_ACCESS_EXISTS)
    {
        /* Do not try to check the presence of a journal or a wal on /vsicurl ! */
        if ( (STARTS_WITH(zName, "/vsicurl/") ||
              STARTS_WITH(zName, "/vsitar/") ||
              STARTS_WITH(zName, "/vsizip/")) &&
             ((strlen(zName) > strlen("-journal") &&
               strcmp(zName + strlen(zName) - strlen("-journal"), "-journal") == 0) ||
              (strlen(zName) > strlen("-wal") &&
               strcmp(zName + strlen(zName) - strlen("-wal"), "-wal") == 0)) )
        {
            nRet = -1;
        }
        else
        {
            nRet = VSIStatExL(zName, &sStatBufL, VSI_STAT_EXISTS_FLAG);
        }
    }
    else if (flags == SQLITE_ACCESS_READ)
    {
        VSILFILE* fp = VSIFOpenL(zName, "rb");
        nRet = fp ? 0 : -1;
        if (fp)
            VSIFCloseL(fp);
    }
    else if (flags == SQLITE_ACCESS_READWRITE)
    {
        VSILFILE* fp = VSIFOpenL(zName, "rb+");
        nRet = fp ? 0 : -1;
        if (fp)
            VSIFCloseL(fp);
    }
    else
    {
        nRet = -1;
    }
    *pResOut = (nRet == 0);
    return SQLITE_OK;
}

static int OGRSQLiteVFSFullPathname (sqlite3_vfs* pVFS, const char *zName, int nOut, char *zOut)
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteVFSFullPathname(%s)", zName);
#endif
    if (zName[0] == '/')
    {
        if( static_cast<int>(strlen( zName )) >= nOut )
        {
            // The +8 comes from the fact that sqlite3 does this check as
            // it needs to be able to append .journal to the filename
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Maximum pathname length reserved for SQLite3 VFS "
                     "isn't large enough. Try raising OGR_SQLITE_VFS_MAXPATHNAME to at least %d",
                     static_cast<int>(strlen(zName)) + 8);
            return SQLITE_CANTOPEN;
        }
        strncpy(zOut, zName, nOut);
        zOut[nOut-1] = '\0';
        return SQLITE_OK;
    }
    return pUnderlyingVFS->xFullPathname(pUnderlyingVFS, zName, nOut, zOut);
}

static void* OGRSQLiteVFSDlOpen (sqlite3_vfs* pVFS, const char *zFilename)
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
    //CPLDebug("SQLITE", "OGRSQLiteVFSDlOpen(%s)", zFilename);
    return pUnderlyingVFS->xDlOpen(pUnderlyingVFS, zFilename);
}

static void OGRSQLiteVFSDlError (sqlite3_vfs* pVFS, int nByte, char *zErrMsg)
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
    //CPLDebug("SQLITE", "OGRSQLiteVFSDlError()");
    pUnderlyingVFS->xDlError(pUnderlyingVFS, nByte, zErrMsg);
}

/* xDlSym member signature changed in sqlite 3.6.7 (http://www.sqlite.org/changes.html) */
/* This was supposed to be done "in a way that is backwards compatible but which might cause compiler warnings" */
/* Perhaps in C, but definitely not in C++ ( #4515 ) */
#if SQLITE_VERSION_NUMBER >= 3006007
static void (*OGRSQLiteVFSDlSym (sqlite3_vfs* pVFS,void* pHandle, const char *zSymbol))(void)
#else
static void (*OGRSQLiteVFSDlSym (sqlite3_vfs* pVFS,void* pHandle, const char *zSymbol))
#endif
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
    //CPLDebug("SQLITE", "OGRSQLiteVFSDlSym(%s)", zSymbol);
    return pUnderlyingVFS->xDlSym(pUnderlyingVFS, pHandle, zSymbol);
}

static void OGRSQLiteVFSDlClose (sqlite3_vfs* pVFS, void* pHandle)
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
    //CPLDebug("SQLITE", "OGRSQLiteVFSDlClose(%p)", pHandle);
    pUnderlyingVFS->xDlClose(pUnderlyingVFS, pHandle);
}

static int OGRSQLiteVFSRandomness (sqlite3_vfs* pVFS, int nByte, char *zOut)
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
    //CPLDebug("SQLITE", "OGRSQLiteVFSRandomness()");
    return pUnderlyingVFS->xRandomness(pUnderlyingVFS, nByte, zOut);
}

static int OGRSQLiteVFSSleep (sqlite3_vfs* pVFS, int microseconds)
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
    //CPLDebug("SQLITE", "OGRSQLiteVFSSleep()");
    return pUnderlyingVFS->xSleep(pUnderlyingVFS, microseconds);
}

// Derived for sqlite3.c implementation of unixCurrentTime64 and winCurrentTime64
#ifdef WIN32
#include <windows.h>
static int OGRSQLiteVFSCurrentTimeInt64 (sqlite3_vfs* /*pVFS*/, sqlite3_int64 *piNow)
{
    FILETIME ft;
    constexpr sqlite3_int64 winFiletimeEpoch = 23058135*(sqlite3_int64)8640000;
    constexpr sqlite3_int64 max32BitValue =
      (sqlite3_int64)2000000000 + (sqlite3_int64)2000000000 +
      (sqlite3_int64)294967296;

#if defined(_WIN32_WCE)
    SYSTEMTIME time;
    GetSystemTime(&time);
    /* if SystemTimeToFileTime() fails, it returns zero. */
    if (!SystemTimeToFileTime(&time,&ft)){
        return SQLITE_ERROR;
    }
#else
    GetSystemTimeAsFileTime( &ft );
#endif
    *piNow = winFiletimeEpoch +
            ((((sqlite3_int64)ft.dwHighDateTime)*max32BitValue) +
               (sqlite3_int64)ft.dwLowDateTime)/(sqlite3_int64)10000;
    return SQLITE_OK;
}
#else
#include <sys/time.h>
static int OGRSQLiteVFSCurrentTimeInt64 (sqlite3_vfs* /*pVFS*/, sqlite3_int64 *piNow)
{
    struct timeval sNow;
    constexpr sqlite3_int64 unixEpoch = 24405875*(sqlite3_int64)8640000;
    (void)gettimeofday(&sNow, nullptr);  /* Cannot fail given valid arguments */
    *piNow = unixEpoch + 1000*(sqlite3_int64)sNow.tv_sec + sNow.tv_usec/1000;

    return SQLITE_OK;
}
#endif

static int OGRSQLiteVFSCurrentTime (sqlite3_vfs* /*pVFS*/, double* p1)
{
    sqlite3_int64 i = 0;
    int rc = OGRSQLiteVFSCurrentTimeInt64(nullptr, &i);
    *p1 = i/86400000.0;
    return rc;
}

static int OGRSQLiteVFSGetLastError (sqlite3_vfs* pVFS, int p1, char *p2)
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
    //CPLDebug("SQLITE", "OGRSQLiteVFSGetLastError()");
    return pUnderlyingVFS->xGetLastError(pUnderlyingVFS, p1, p2);
}

sqlite3_vfs* OGRSQLiteCreateVFS(pfnNotifyFileOpenedType pfn, void* pfnUserData)
{
    sqlite3_vfs* pDefaultVFS = sqlite3_vfs_find(nullptr);
    sqlite3_vfs* pMyVFS = (sqlite3_vfs*) CPLCalloc(1, sizeof(sqlite3_vfs));

    OGRSQLiteVFSAppDataStruct* pVFSAppData =
        (OGRSQLiteVFSAppDataStruct*) CPLCalloc(1, sizeof(OGRSQLiteVFSAppDataStruct));
    char szPtr[32];
    snprintf(szPtr, sizeof(szPtr), "%p", pVFSAppData);
    snprintf(pVFSAppData->szVFSName, sizeof(pVFSAppData->szVFSName), "OGRSQLITEVFS_%s", szPtr);
    pVFSAppData->pDefaultVFS = pDefaultVFS;
    pVFSAppData->pfn = pfn;
    pVFSAppData->pfnUserData = pfnUserData;
    pVFSAppData->nCounter = 0;

#if SQLITE_VERSION_NUMBER >= 3008000L /* perhaps not the minimal version that defines xCurrentTimeInt64, but who cares */
    pMyVFS->iVersion = 2;
#else
    pMyVFS->iVersion = 1;
#endif
    pMyVFS->szOsFile = sizeof(OGRSQLiteFileStruct);
    // must be large enough to hold potentially very long names like /vsicurl/.... with AWS S3 security tokens
    pMyVFS->mxPathname = atoi(CPLGetConfigOption("OGR_SQLITE_VFS_MAXPATHNAME", "2048"));
    pMyVFS->zName = pVFSAppData->szVFSName;
    pMyVFS->pAppData = pVFSAppData;
    pMyVFS->xOpen = OGRSQLiteVFSOpen;
    pMyVFS->xDelete = OGRSQLiteVFSDelete;
    pMyVFS->xAccess = OGRSQLiteVFSAccess;
    pMyVFS->xFullPathname = OGRSQLiteVFSFullPathname;
    pMyVFS->xDlOpen = OGRSQLiteVFSDlOpen;
    pMyVFS->xDlError = OGRSQLiteVFSDlError;
    pMyVFS->xDlSym = OGRSQLiteVFSDlSym;
    pMyVFS->xDlClose = OGRSQLiteVFSDlClose;
    pMyVFS->xRandomness = OGRSQLiteVFSRandomness;
    pMyVFS->xSleep = OGRSQLiteVFSSleep;
    pMyVFS->xCurrentTime = OGRSQLiteVFSCurrentTime;
    pMyVFS->xGetLastError = OGRSQLiteVFSGetLastError;
#if SQLITE_VERSION_NUMBER >= 3008000L /* perhaps not the minimal version that defines xCurrentTimeInt64, but who cares */
    if( pMyVFS->iVersion >= 2 )
        pMyVFS->xCurrentTimeInt64 = OGRSQLiteVFSCurrentTimeInt64;
#endif

    return pMyVFS;
}
