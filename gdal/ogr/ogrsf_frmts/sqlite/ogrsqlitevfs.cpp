/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements SQLite VFS
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>

 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_atomic_ops.h"
#include "ogr_sqlite.h"

CPL_CVSID("$Id$");

//#define DEBUG_IO 1

#ifdef HAVE_SQLITE_VFS

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

static int OGRSQLiteIOSync(
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                           sqlite3_file* pFile,
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                           int flags)
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

static int OGRSQLiteIOLock(
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                           sqlite3_file* pFile,
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                           int flags)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOLock(%p)", pMyFile->fp);
#endif
    return SQLITE_OK;
}

static int OGRSQLiteIOUnlock(CPL_UNUSED sqlite3_file* pFile, CPL_UNUSED int flags)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOUnlock(%p)", pMyFile->fp);
#endif
    return SQLITE_OK;
}

static int OGRSQLiteIOCheckReservedLock(
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                                        sqlite3_file* pFile,
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                                        int *pResOut)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOCheckReservedLock(%p)", pMyFile->fp);
#endif
    *pResOut = 0;
    return SQLITE_OK;
}

static int OGRSQLiteIOFileControl(
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                                  sqlite3_file* pFile,
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                                  int op,
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                                  void *pArg)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOFileControl(%p, %d)", pMyFile->fp, op);
#endif
    return SQLITE_NOTFOUND;
}

static int OGRSQLiteIOSectorSize(
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                                 sqlite3_file* pFile)
{
#ifdef DEBUG_IO
    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    CPLDebug("SQLITE", "OGRSQLiteIOSectorSize(%p)", pMyFile->fp);
#endif
    return 0;
}

static int OGRSQLiteIODeviceCharacteristics(
#ifndef DEBUG_IO
CPL_UNUSED 
#endif
                                            sqlite3_file* pFile)
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
    OGRSQLiteIODeviceCharacteristics
#if 0
    // TODO: These are in sqlite3.
    , 0, // xShmMap
    0, // xShmLock
    0, // xShmBarrier
    0, // xShmUnmap
    0, // xFetch
    0 // xUnfetch
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

    if (zName == NULL)
    {
        zName = CPLSPrintf("/vsimem/sqlite/%p_%d",
                           pVFS, CPLAtomicInc(&(pAppData->nCounter)));
    }

    OGRSQLiteFileStruct* pMyFile = (OGRSQLiteFileStruct*) pFile;
    pMyFile->pMethods = NULL;
    pMyFile->bDeleteOnClose = FALSE;
    pMyFile->pszFilename = NULL;
    if ( flags & SQLITE_OPEN_READONLY )
        pMyFile->fp = VSIFOpenL(zName, "rb");
    else if ( flags & SQLITE_OPEN_CREATE )
        pMyFile->fp = VSIFOpenL(zName, "wb+");
    else if ( flags & SQLITE_OPEN_READWRITE )
        pMyFile->fp = VSIFOpenL(zName, "rb+");
    else
        pMyFile->fp = NULL;

    if (pMyFile->fp == NULL)
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

    if (pOutFlags != NULL)
        *pOutFlags = flags;

    return SQLITE_OK;
}

static int OGRSQLiteVFSDelete(CPL_UNUSED sqlite3_vfs* pVFS,
                              const char *zName,
                              CPL_UNUSED int syncDir)
{
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteVFSDelete(%s)", zName);
#endif
    VSIUnlink(zName);
    return SQLITE_OK;
}

static int OGRSQLiteVFSAccess (CPL_UNUSED sqlite3_vfs* pVFS, const char *zName, int flags, int *pResOut)
{
#ifdef DEBUG_IO
    CPLDebug("SQLITE", "OGRSQLiteVFSAccess(%s, %d)", zName, flags);
#endif
    VSIStatBufL sStatBufL;
    int nRet;
    if (flags == SQLITE_ACCESS_EXISTS)
    {
        /* Do not try to check the presence of a journal on /vsicurl ! */
        if ( strncmp(zName, "/vsicurl/", 9) == 0 &&
             strlen(zName) > strlen("-journal") &&
             strcmp(zName + strlen(zName) - strlen("-journal"), "-journal") == 0 )
            nRet = -1;
        else
            nRet = VSIStatExL(zName, &sStatBufL, VSI_STAT_EXISTS_FLAG);
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
        nRet = -1;
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

static int OGRSQLiteVFSCurrentTime (sqlite3_vfs* pVFS, double* p1)
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
    //CPLDebug("SQLITE", "OGRSQLiteVFSCurrentTime()");
    return pUnderlyingVFS->xCurrentTime(pUnderlyingVFS, p1);
}

static int OGRSQLiteVFSGetLastError (sqlite3_vfs* pVFS, int p1, char *p2)
{
    sqlite3_vfs* pUnderlyingVFS = GET_UNDERLYING_VFS(pVFS);
    //CPLDebug("SQLITE", "OGRSQLiteVFSGetLastError()");
    return pUnderlyingVFS->xGetLastError(pUnderlyingVFS, p1, p2);
}

sqlite3_vfs* OGRSQLiteCreateVFS(pfnNotifyFileOpenedType pfn, void* pfnUserData)
{
    sqlite3_vfs* pDefaultVFS = sqlite3_vfs_find(NULL);
    sqlite3_vfs* pMyVFS = (sqlite3_vfs*) CPLCalloc(1, sizeof(sqlite3_vfs));

    OGRSQLiteVFSAppDataStruct* pVFSAppData =
        (OGRSQLiteVFSAppDataStruct*) CPLCalloc(1, sizeof(OGRSQLiteVFSAppDataStruct));
    sprintf(pVFSAppData->szVFSName, "OGRSQLITEVFS_%p", pVFSAppData);
    pVFSAppData->pDefaultVFS = pDefaultVFS;
    pVFSAppData->pfn = pfn;
    pVFSAppData->pfnUserData = pfnUserData;
    pVFSAppData->nCounter = 0;

    pMyVFS->iVersion = 1;
    pMyVFS->szOsFile = sizeof(OGRSQLiteFileStruct);
    pMyVFS->mxPathname = pDefaultVFS->mxPathname;
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
    return pMyVFS;
}

#endif // HAVE_SQLITE_VFS
