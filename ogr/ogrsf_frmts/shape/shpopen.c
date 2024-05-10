/******************************************************************************
 *
 * Project:  Shapelib
 * Purpose:  Implementation of core Shapefile read/write functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001, Frank Warmerdam
 * Copyright (c) 2011-2019, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
 ******************************************************************************/

#include "shapefil_private.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#define ByteCopy(a, b, c) memcpy(b, a, c)
#ifndef MAX
#define MIN(a, b) ((a < b) ? a : b)
#define MAX(a, b) ((a > b) ? a : b)
#endif

#ifndef USE_CPL
#if defined(_MSC_VER)
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#elif defined(_WIN32)
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif
#endif

/************************************************************************/
/*                          SHPWriteHeader()                            */
/*                                                                      */
/*      Write out a header for the .shp and .shx files as well as the   */
/*      contents of the index (.shx) file.                              */
/************************************************************************/

void SHPAPI_CALL SHPWriteHeader(SHPHandle psSHP)
{
    if (psSHP->fpSHX == SHPLIB_NULLPTR)
    {
        psSHP->sHooks.Error("SHPWriteHeader failed : SHX file is closed");
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Prepare header block for .shp file.                             */
    /* -------------------------------------------------------------------- */

    unsigned char abyHeader[100] = {0};
    abyHeader[2] = 0x27; /* magic cookie */
    abyHeader[3] = 0x0a;

    uint32_t i32 = psSHP->nFileSize / 2; /* file size */
    ByteCopy(&i32, abyHeader + 24, 4);
#if !defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(abyHeader + 24);
#endif

    i32 = 1000; /* version */
    ByteCopy(&i32, abyHeader + 28, 4);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(abyHeader + 28);
#endif

    i32 = psSHP->nShapeType; /* shape type */
    ByteCopy(&i32, abyHeader + 32, 4);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(abyHeader + 32);
#endif

    double dValue = psSHP->adBoundsMin[0]; /* set bounds */
    ByteCopy(&dValue, abyHeader + 36, 8);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(abyHeader + 36);
#endif
    dValue = psSHP->adBoundsMin[1];
    ByteCopy(&dValue, abyHeader + 44, 8);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(abyHeader + 44);
#endif
    dValue = psSHP->adBoundsMax[0];
    ByteCopy(&dValue, abyHeader + 52, 8);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(abyHeader + 52);
#endif

    dValue = psSHP->adBoundsMax[1];
    ByteCopy(&dValue, abyHeader + 60, 8);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(abyHeader + 60);
#endif

    dValue = psSHP->adBoundsMin[2]; /* z */
    ByteCopy(&dValue, abyHeader + 68, 8);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(abyHeader + 68);
#endif

    dValue = psSHP->adBoundsMax[2];
    ByteCopy(&dValue, abyHeader + 76, 8);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(abyHeader + 76);
#endif

    dValue = psSHP->adBoundsMin[3]; /* m */
    ByteCopy(&dValue, abyHeader + 84, 8);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(abyHeader + 84);
#endif

    dValue = psSHP->adBoundsMax[3];
    ByteCopy(&dValue, abyHeader + 92, 8);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(abyHeader + 92);
#endif

    /* -------------------------------------------------------------------- */
    /*      Write .shp file header.                                         */
    /* -------------------------------------------------------------------- */
    if (psSHP->sHooks.FSeek(psSHP->fpSHP, 0, 0) != 0 ||
        psSHP->sHooks.FWrite(abyHeader, 100, 1, psSHP->fpSHP) != 1)
    {
        char szErrorMsg[200];

        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Failure writing .shp header: %s", strerror(errno));
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psSHP->sHooks.Error(szErrorMsg);
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Prepare, and write .shx file header.                            */
    /* -------------------------------------------------------------------- */
    i32 = (psSHP->nRecords * 2 * sizeof(uint32_t) + 100) / 2; /* file size */
    ByteCopy(&i32, abyHeader + 24, 4);
#if !defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(abyHeader + 24);
#endif

    if (psSHP->sHooks.FSeek(psSHP->fpSHX, 0, 0) != 0 ||
        psSHP->sHooks.FWrite(abyHeader, 100, 1, psSHP->fpSHX) != 1)
    {
        char szErrorMsg[200];

        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Failure writing .shx header: %s", strerror(errno));
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psSHP->sHooks.Error(szErrorMsg);

        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Write out the .shx contents.                                    */
    /* -------------------------------------------------------------------- */
    uint32_t *panSHX =
        STATIC_CAST(uint32_t *, malloc(sizeof(uint32_t) * 2 * psSHP->nRecords));
    if (panSHX == SHPLIB_NULLPTR)
    {
        psSHP->sHooks.Error("Failure allocatin panSHX");
        return;
    }

    for (int i = 0; i < psSHP->nRecords; i++)
    {
        panSHX[i * 2] = psSHP->panRecOffset[i] / 2;
        panSHX[i * 2 + 1] = psSHP->panRecSize[i] / 2;
#if !defined(SHP_BIG_ENDIAN)
        SHP_SWAP32(panSHX + i * 2);
        SHP_SWAP32(panSHX + i * 2 + 1);
#endif
    }

    if (STATIC_CAST(int, psSHP->sHooks.FWrite(panSHX, sizeof(uint32_t) * 2,
                                              psSHP->nRecords, psSHP->fpSHX)) !=
        psSHP->nRecords)
    {
        char szErrorMsg[200];

        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Failure writing .shx contents: %s", strerror(errno));
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psSHP->sHooks.Error(szErrorMsg);
    }

    free(panSHX);

    /* -------------------------------------------------------------------- */
    /*      Flush to disk.                                                  */
    /* -------------------------------------------------------------------- */
    psSHP->sHooks.FFlush(psSHP->fpSHP);
    psSHP->sHooks.FFlush(psSHP->fpSHX);
}

/************************************************************************/
/*                              SHPOpen()                               */
/************************************************************************/

SHPHandle SHPAPI_CALL SHPOpen(const char *pszLayer, const char *pszAccess)
{
    SAHooks sHooks;

    SASetupDefaultHooks(&sHooks);

    return SHPOpenLL(pszLayer, pszAccess, &sHooks);
}

/************************************************************************/
/*                      SHPGetLenWithoutExtension()                     */
/************************************************************************/

static int SHPGetLenWithoutExtension(const char *pszBasename)
{
    const int nLen = STATIC_CAST(int, strlen(pszBasename));
    for (int i = nLen - 1;
         i > 0 && pszBasename[i] != '/' && pszBasename[i] != '\\'; i--)
    {
        if (pszBasename[i] == '.')
        {
            return i;
        }
    }
    return nLen;
}

/************************************************************************/
/*                              SHPOpen()                               */
/*                                                                      */
/*      Open the .shp and .shx files based on the basename of the       */
/*      files or either file name.                                      */
/************************************************************************/

SHPHandle SHPAPI_CALL SHPOpenLL(const char *pszLayer, const char *pszAccess,
                                const SAHooks *psHooks)
{
    /* -------------------------------------------------------------------- */
    /*      Ensure the access string is one of the legal ones.  We          */
    /*      ensure the result string indicates binary to avoid common       */
    /*      problems on Windows.                                            */
    /* -------------------------------------------------------------------- */
    bool bLazySHXLoading = false;
    if (strcmp(pszAccess, "rb+") == 0 || strcmp(pszAccess, "r+b") == 0 ||
        strcmp(pszAccess, "r+") == 0)
    {
        pszAccess = "r+b";
    }
    else
    {
        bLazySHXLoading = strchr(pszAccess, 'l') != SHPLIB_NULLPTR;
        pszAccess = "rb";
    }

    /* -------------------------------------------------------------------- */
    /*  Initialize the info structure.                  */
    /* -------------------------------------------------------------------- */
    SHPHandle psSHP = STATIC_CAST(SHPHandle, calloc(1, sizeof(SHPInfo)));

    psSHP->bUpdated = FALSE;
    memcpy(&(psSHP->sHooks), psHooks, sizeof(SAHooks));

    /* -------------------------------------------------------------------- */
    /*  Open the .shp and .shx files.  Note that files pulled from  */
    /*  a PC to Unix with upper case filenames won't work!      */
    /* -------------------------------------------------------------------- */
    const int nLenWithoutExtension = SHPGetLenWithoutExtension(pszLayer);
    char *pszFullname = STATIC_CAST(char *, malloc(nLenWithoutExtension + 5));
    memcpy(pszFullname, pszLayer, nLenWithoutExtension);
    memcpy(pszFullname + nLenWithoutExtension, ".shp", 5);
    psSHP->fpSHP =
        psSHP->sHooks.FOpen(pszFullname, pszAccess, psSHP->sHooks.pvUserData);
    if (psSHP->fpSHP == SHPLIB_NULLPTR)
    {
        memcpy(pszFullname + nLenWithoutExtension, ".SHP", 5);
        psSHP->fpSHP = psSHP->sHooks.FOpen(pszFullname, pszAccess,
                                           psSHP->sHooks.pvUserData);
    }

    if (psSHP->fpSHP == SHPLIB_NULLPTR)
    {
        const size_t nMessageLen = strlen(pszFullname) * 2 + 256;
        char *pszMessage = STATIC_CAST(char *, malloc(nMessageLen));
        pszFullname[nLenWithoutExtension] = 0;
        snprintf(pszMessage, nMessageLen,
                 "Unable to open %s.shp or %s.SHP in %s mode.", pszFullname,
                 pszFullname, pszAccess);
        psHooks->Error(pszMessage);
        free(pszMessage);

        free(psSHP);
        free(pszFullname);

        return SHPLIB_NULLPTR;
    }

    memcpy(pszFullname + nLenWithoutExtension, ".shx", 5);
    psSHP->fpSHX =
        psSHP->sHooks.FOpen(pszFullname, pszAccess, psSHP->sHooks.pvUserData);
    if (psSHP->fpSHX == SHPLIB_NULLPTR)
    {
        memcpy(pszFullname + nLenWithoutExtension, ".SHX", 5);
        psSHP->fpSHX = psSHP->sHooks.FOpen(pszFullname, pszAccess,
                                           psSHP->sHooks.pvUserData);
    }

    if (psSHP->fpSHX == SHPLIB_NULLPTR)
    {
        const size_t nMessageLen = strlen(pszFullname) * 2 + 256;
        char *pszMessage = STATIC_CAST(char *, malloc(nMessageLen));
        pszFullname[nLenWithoutExtension] = 0;
        snprintf(pszMessage, nMessageLen,
                 "Unable to open %s.shx or %s.SHX. "
                 "Set SHAPE_RESTORE_SHX config option to YES to restore or "
                 "create it.",
                 pszFullname, pszFullname);
        psHooks->Error(pszMessage);
        free(pszMessage);

        psSHP->sHooks.FClose(psSHP->fpSHP);
        free(psSHP);
        free(pszFullname);
        return SHPLIB_NULLPTR;
    }

    free(pszFullname);

    /* -------------------------------------------------------------------- */
    /*  Read the file size from the SHP file.               */
    /* -------------------------------------------------------------------- */
    unsigned char *pabyBuf = STATIC_CAST(unsigned char *, malloc(100));
    if (psSHP->sHooks.FRead(pabyBuf, 100, 1, psSHP->fpSHP) != 1)
    {
        psSHP->sHooks.Error(".shp file is unreadable, or corrupt.");
        psSHP->sHooks.FClose(psSHP->fpSHP);
        psSHP->sHooks.FClose(psSHP->fpSHX);
        free(pabyBuf);
        free(psSHP);

        return SHPLIB_NULLPTR;
    }

    psSHP->nFileSize = (STATIC_CAST(unsigned int, pabyBuf[24]) << 24) |
                       (pabyBuf[25] << 16) | (pabyBuf[26] << 8) | pabyBuf[27];
    if (psSHP->nFileSize < UINT_MAX / 2)
        psSHP->nFileSize *= 2;
    else
        psSHP->nFileSize = (UINT_MAX / 2) * 2;

    /* -------------------------------------------------------------------- */
    /*  Read SHX file Header info                                           */
    /* -------------------------------------------------------------------- */
    if (psSHP->sHooks.FRead(pabyBuf, 100, 1, psSHP->fpSHX) != 1 ||
        pabyBuf[0] != 0 || pabyBuf[1] != 0 || pabyBuf[2] != 0x27 ||
        (pabyBuf[3] != 0x0a && pabyBuf[3] != 0x0d))
    {
        psSHP->sHooks.Error(".shx file is unreadable, or corrupt.");
        psSHP->sHooks.FClose(psSHP->fpSHP);
        psSHP->sHooks.FClose(psSHP->fpSHX);
        free(pabyBuf);
        free(psSHP);

        return SHPLIB_NULLPTR;
    }

    psSHP->nRecords = pabyBuf[27] | (pabyBuf[26] << 8) | (pabyBuf[25] << 16) |
                      ((pabyBuf[24] & 0x7F) << 24);
    psSHP->nRecords = (psSHP->nRecords - 50) / 4;

    psSHP->nShapeType = pabyBuf[32];

    if (psSHP->nRecords < 0 || psSHP->nRecords > 256000000)
    {
        char szErrorMsg[200];

        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Record count in .shx header is %d, which seems\n"
                 "unreasonable.  Assuming header is corrupt.",
                 psSHP->nRecords);
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psSHP->sHooks.Error(szErrorMsg);
        psSHP->sHooks.FClose(psSHP->fpSHP);
        psSHP->sHooks.FClose(psSHP->fpSHX);
        free(psSHP);
        free(pabyBuf);

        return SHPLIB_NULLPTR;
    }

    /* If a lot of records are advertized, check that the file is big enough */
    /* to hold them */
    if (psSHP->nRecords >= 1024 * 1024)
    {
        psSHP->sHooks.FSeek(psSHP->fpSHX, 0, 2);
        const SAOffset nFileSize = psSHP->sHooks.FTell(psSHP->fpSHX);
        if (nFileSize > 100 &&
            nFileSize / 2 < STATIC_CAST(SAOffset, psSHP->nRecords * 4 + 50))
        {
            psSHP->nRecords = STATIC_CAST(int, (nFileSize - 100) / 8);
        }
        psSHP->sHooks.FSeek(psSHP->fpSHX, 100, 0);
    }

    /* -------------------------------------------------------------------- */
    /*      Read the bounds.                                                */
    /* -------------------------------------------------------------------- */
    double dValue;

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(pabyBuf + 36);
#endif
    memcpy(&dValue, pabyBuf + 36, 8);
    psSHP->adBoundsMin[0] = dValue;

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(pabyBuf + 44);
#endif
    memcpy(&dValue, pabyBuf + 44, 8);
    psSHP->adBoundsMin[1] = dValue;

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(pabyBuf + 52);
#endif
    memcpy(&dValue, pabyBuf + 52, 8);
    psSHP->adBoundsMax[0] = dValue;

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(pabyBuf + 60);
#endif
    memcpy(&dValue, pabyBuf + 60, 8);
    psSHP->adBoundsMax[1] = dValue;

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(pabyBuf + 68); /* z */
#endif
    memcpy(&dValue, pabyBuf + 68, 8);
    psSHP->adBoundsMin[2] = dValue;

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(pabyBuf + 76);
#endif
    memcpy(&dValue, pabyBuf + 76, 8);
    psSHP->adBoundsMax[2] = dValue;

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(pabyBuf + 84); /* z */
#endif
    memcpy(&dValue, pabyBuf + 84, 8);
    psSHP->adBoundsMin[3] = dValue;

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(pabyBuf + 92);
#endif
    memcpy(&dValue, pabyBuf + 92, 8);
    psSHP->adBoundsMax[3] = dValue;

    free(pabyBuf);

    /* -------------------------------------------------------------------- */
    /*  Read the .shx file to get the offsets to each record in     */
    /*  the .shp file.                          */
    /* -------------------------------------------------------------------- */
    psSHP->nMaxRecords = psSHP->nRecords;

    psSHP->panRecOffset =
        STATIC_CAST(unsigned int *,
                    malloc(sizeof(unsigned int) * MAX(1, psSHP->nMaxRecords)));
    psSHP->panRecSize =
        STATIC_CAST(unsigned int *,
                    malloc(sizeof(unsigned int) * MAX(1, psSHP->nMaxRecords)));
    if (bLazySHXLoading)
        pabyBuf = SHPLIB_NULLPTR;
    else
        pabyBuf =
            STATIC_CAST(unsigned char *, malloc(8 * MAX(1, psSHP->nRecords)));

    if (psSHP->panRecOffset == SHPLIB_NULLPTR ||
        psSHP->panRecSize == SHPLIB_NULLPTR ||
        (!bLazySHXLoading && pabyBuf == SHPLIB_NULLPTR))
    {
        char szErrorMsg[200];

        snprintf(
            szErrorMsg, sizeof(szErrorMsg),
            "Not enough memory to allocate requested memory (nRecords=%d).\n"
            "Probably broken SHP file",
            psSHP->nRecords);
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psSHP->sHooks.Error(szErrorMsg);
        psSHP->sHooks.FClose(psSHP->fpSHP);
        psSHP->sHooks.FClose(psSHP->fpSHX);
        if (psSHP->panRecOffset)
            free(psSHP->panRecOffset);
        if (psSHP->panRecSize)
            free(psSHP->panRecSize);
        if (pabyBuf)
            free(pabyBuf);
        free(psSHP);
        return SHPLIB_NULLPTR;
    }

    if (bLazySHXLoading)
    {
        memset(psSHP->panRecOffset, 0,
               sizeof(unsigned int) * MAX(1, psSHP->nMaxRecords));
        memset(psSHP->panRecSize, 0,
               sizeof(unsigned int) * MAX(1, psSHP->nMaxRecords));
        free(pabyBuf);  // sometimes make cppcheck happy, but
        return (psSHP);
    }

    if (STATIC_CAST(int, psSHP->sHooks.FRead(pabyBuf, 8, psSHP->nRecords,
                                             psSHP->fpSHX)) != psSHP->nRecords)
    {
        char szErrorMsg[200];

        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Failed to read all values for %d records in .shx file: %s.",
                 psSHP->nRecords, strerror(errno));
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psSHP->sHooks.Error(szErrorMsg);

        /* SHX is short or unreadable for some reason. */
        psSHP->sHooks.FClose(psSHP->fpSHP);
        psSHP->sHooks.FClose(psSHP->fpSHX);
        free(psSHP->panRecOffset);
        free(psSHP->panRecSize);
        free(pabyBuf);
        free(psSHP);

        return SHPLIB_NULLPTR;
    }

    /* In read-only mode, we can close the SHX now */
    if (strcmp(pszAccess, "rb") == 0)
    {
        psSHP->sHooks.FClose(psSHP->fpSHX);
        psSHP->fpSHX = SHPLIB_NULLPTR;
    }

    for (int i = 0; i < psSHP->nRecords; i++)
    {
        unsigned int nOffset;
        memcpy(&nOffset, pabyBuf + i * 8, 4);
#if !defined(SHP_BIG_ENDIAN)
        SHP_SWAP32(&nOffset);
#endif

        unsigned int nLength;
        memcpy(&nLength, pabyBuf + i * 8 + 4, 4);
#if !defined(SHP_BIG_ENDIAN)
        SHP_SWAP32(&nLength);
#endif

        if (nOffset > STATIC_CAST(unsigned int, INT_MAX))
        {
            char str[128];
            snprintf(str, sizeof(str), "Invalid offset for entity %d", i);
            str[sizeof(str) - 1] = '\0';

            psSHP->sHooks.Error(str);
            SHPClose(psSHP);
            free(pabyBuf);
            return SHPLIB_NULLPTR;
        }
        if (nLength > STATIC_CAST(unsigned int, INT_MAX / 2 - 4))
        {
            char str[128];
            snprintf(str, sizeof(str), "Invalid length for entity %d", i);
            str[sizeof(str) - 1] = '\0';

            psSHP->sHooks.Error(str);
            SHPClose(psSHP);
            free(pabyBuf);
            return SHPLIB_NULLPTR;
        }
        psSHP->panRecOffset[i] = nOffset * 2;
        psSHP->panRecSize[i] = nLength * 2;
    }
    free(pabyBuf);

    return (psSHP);
}

/************************************************************************/
/*                              SHPOpenLLEx()                           */
/*                                                                      */
/*      Open the .shp and .shx files based on the basename of the       */
/*      files or either file name. It generally invokes SHPRestoreSHX() */
/*      in case when bRestoreSHX equals true.                           */
/************************************************************************/

SHPHandle SHPAPI_CALL SHPOpenLLEx(const char *pszLayer, const char *pszAccess,
                                  const SAHooks *psHooks, int bRestoreSHX)
{
    if (!bRestoreSHX)
        return SHPOpenLL(pszLayer, pszAccess, psHooks);
    else
    {
        if (SHPRestoreSHX(pszLayer, pszAccess, psHooks))
        {
            return SHPOpenLL(pszLayer, pszAccess, psHooks);
        }
    }

    return SHPLIB_NULLPTR;
}

/************************************************************************/
/*                              SHPRestoreSHX()                         */
/*                                                                      */
/*      Restore .SHX file using associated .SHP file.                   */
/*                                                                      */
/************************************************************************/

int SHPAPI_CALL SHPRestoreSHX(const char *pszLayer, const char *pszAccess,
                              const SAHooks *psHooks)
{
    /* -------------------------------------------------------------------- */
    /*      Ensure the access string is one of the legal ones.  We          */
    /*      ensure the result string indicates binary to avoid common       */
    /*      problems on Windows.                                            */
    /* -------------------------------------------------------------------- */
    if (strcmp(pszAccess, "rb+") == 0 || strcmp(pszAccess, "r+b") == 0 ||
        strcmp(pszAccess, "r+") == 0)
    {
        pszAccess = "r+b";
    }
    else
    {
        pszAccess = "rb";
    }

    /* -------------------------------------------------------------------- */
    /*  Open the .shp file.  Note that files pulled from                    */
    /*  a PC to Unix with upper case filenames won't work!                  */
    /* -------------------------------------------------------------------- */
    const int nLenWithoutExtension = SHPGetLenWithoutExtension(pszLayer);
    char *pszFullname = STATIC_CAST(char *, malloc(nLenWithoutExtension + 5));
    memcpy(pszFullname, pszLayer, nLenWithoutExtension);
    memcpy(pszFullname + nLenWithoutExtension, ".shp", 5);
    SAFile fpSHP = psHooks->FOpen(pszFullname, pszAccess, psHooks->pvUserData);
    if (fpSHP == SHPLIB_NULLPTR)
    {
        memcpy(pszFullname + nLenWithoutExtension, ".SHP", 5);
        fpSHP = psHooks->FOpen(pszFullname, pszAccess, psHooks->pvUserData);
    }

    if (fpSHP == SHPLIB_NULLPTR)
    {
        const size_t nMessageLen = strlen(pszFullname) * 2 + 256;
        char *pszMessage = STATIC_CAST(char *, malloc(nMessageLen));

        pszFullname[nLenWithoutExtension] = 0;
        snprintf(pszMessage, nMessageLen, "Unable to open %s.shp or %s.SHP.",
                 pszFullname, pszFullname);
        psHooks->Error(pszMessage);
        free(pszMessage);

        free(pszFullname);

        return (0);
    }

    /* -------------------------------------------------------------------- */
    /*  Read the file size from the SHP file.                               */
    /* -------------------------------------------------------------------- */
    unsigned char *pabyBuf = STATIC_CAST(unsigned char *, malloc(100));
    if (psHooks->FRead(pabyBuf, 100, 1, fpSHP) != 1)
    {
        psHooks->Error(".shp file is unreadable, or corrupt.");
        psHooks->FClose(fpSHP);

        free(pabyBuf);
        free(pszFullname);

        return (0);
    }

    unsigned int nSHPFilesize = (STATIC_CAST(unsigned int, pabyBuf[24]) << 24) |
                                (pabyBuf[25] << 16) | (pabyBuf[26] << 8) |
                                pabyBuf[27];
    if (nSHPFilesize < UINT_MAX / 2)
        nSHPFilesize *= 2;
    else
        nSHPFilesize = (UINT_MAX / 2) * 2;

    memcpy(pszFullname + nLenWithoutExtension, ".shx", 5);
    const char pszSHXAccess[] = "w+b";
    SAFile fpSHX =
        psHooks->FOpen(pszFullname, pszSHXAccess, psHooks->pvUserData);
    if (fpSHX == SHPLIB_NULLPTR)
    {
        size_t nMessageLen = strlen(pszFullname) * 2 + 256;
        char *pszMessage = STATIC_CAST(char *, malloc(nMessageLen));
        pszFullname[nLenWithoutExtension] = 0;
        snprintf(pszMessage, nMessageLen,
                 "Error opening file %s.shx for writing", pszFullname);
        psHooks->Error(pszMessage);
        free(pszMessage);

        psHooks->FClose(fpSHP);

        free(pabyBuf);
        free(pszFullname);

        return (0);
    }

    /* -------------------------------------------------------------------- */
    /*  Open SHX and create it using SHP file content.                      */
    /* -------------------------------------------------------------------- */
    psHooks->FSeek(fpSHP, 100, 0);
    char *pabySHXHeader = STATIC_CAST(char *, malloc(100));
    memcpy(pabySHXHeader, pabyBuf, 100);
    psHooks->FWrite(pabySHXHeader, 100, 1, fpSHX);
    free(pabyBuf);

    // unsigned int nCurrentRecordOffset = 0;
    unsigned int nCurrentSHPOffset = 100;
    unsigned int nRealSHXContentSize = 100;
    int nRetCode = TRUE;
    unsigned int nRecordOffset = 50;

    while (nCurrentSHPOffset < nSHPFilesize)
    {
        unsigned int niRecord = 0;
        unsigned int nRecordLength = 0;
        int nSHPType;

        if (psHooks->FRead(&niRecord, 4, 1, fpSHP) == 1 &&
            psHooks->FRead(&nRecordLength, 4, 1, fpSHP) == 1 &&
            psHooks->FRead(&nSHPType, 4, 1, fpSHP) == 1)
        {
            char abyReadRecord[8];
            unsigned int nRecordOffsetBE = nRecordOffset;

#if !defined(SHP_BIG_ENDIAN)
            SHP_SWAP32(&nRecordOffsetBE);
#endif
            memcpy(abyReadRecord, &nRecordOffsetBE, 4);
            memcpy(abyReadRecord + 4, &nRecordLength, 4);

#if !defined(SHP_BIG_ENDIAN)
            SHP_SWAP32(&nRecordLength);
#endif
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP32(&nSHPType);
#endif

            // Sanity check on record length
            if (nRecordLength < 1 ||
                nRecordLength > (nSHPFilesize - (nCurrentSHPOffset + 8)) / 2)
            {
                char szErrorMsg[200];
                snprintf(szErrorMsg, sizeof(szErrorMsg),
                         "Error parsing .shp to restore .shx. "
                         "Invalid record length = %u at record starting at "
                         "offset %u",
                         nRecordLength, nCurrentSHPOffset);
                psHooks->Error(szErrorMsg);

                nRetCode = FALSE;
                break;
            }

            // Sanity check on record type
            if (nSHPType != SHPT_NULL && nSHPType != SHPT_POINT &&
                nSHPType != SHPT_ARC && nSHPType != SHPT_POLYGON &&
                nSHPType != SHPT_MULTIPOINT && nSHPType != SHPT_POINTZ &&
                nSHPType != SHPT_ARCZ && nSHPType != SHPT_POLYGONZ &&
                nSHPType != SHPT_MULTIPOINTZ && nSHPType != SHPT_POINTM &&
                nSHPType != SHPT_ARCM && nSHPType != SHPT_POLYGONM &&
                nSHPType != SHPT_MULTIPOINTM && nSHPType != SHPT_MULTIPATCH)
            {
                char szErrorMsg[200];
                snprintf(szErrorMsg, sizeof(szErrorMsg),
                         "Error parsing .shp to restore .shx. "
                         "Invalid shape type = %d at record starting at "
                         "offset %u",
                         nSHPType, nCurrentSHPOffset);
                psHooks->Error(szErrorMsg);

                nRetCode = FALSE;
                break;
            }

            psHooks->FWrite(abyReadRecord, 8, 1, fpSHX);

            nRecordOffset += nRecordLength + 4;
            // nCurrentRecordOffset += 8;
            nCurrentSHPOffset += 8 + nRecordLength * 2;

            psHooks->FSeek(fpSHP, nCurrentSHPOffset, 0);
            nRealSHXContentSize += 8;
        }
        else
        {
            char szErrorMsg[200];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Error parsing .shp to restore .shx. "
                     "Cannot read first bytes of record starting at "
                     "offset %u",
                     nCurrentSHPOffset);
            psHooks->Error(szErrorMsg);

            nRetCode = FALSE;
            break;
        }
    }
    if (nRetCode && nCurrentSHPOffset != nSHPFilesize)
    {
        psHooks->Error("Error parsing .shp to restore .shx. "
                       "Not expected number of bytes");

        nRetCode = FALSE;
    }

    nRealSHXContentSize /= 2;  // Bytes counted -> WORDs
#if !defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(&nRealSHXContentSize);
#endif

    psHooks->FSeek(fpSHX, 24, 0);
    psHooks->FWrite(&nRealSHXContentSize, 4, 1, fpSHX);

    psHooks->FClose(fpSHP);
    psHooks->FClose(fpSHX);

    free(pszFullname);
    free(pabySHXHeader);

    return nRetCode;
}

/************************************************************************/
/*                              SHPClose()                              */
/*                                                                      */
/*      Close the .shp and .shx files.                                  */
/************************************************************************/

void SHPAPI_CALL SHPClose(SHPHandle psSHP)
{
    if (psSHP == SHPLIB_NULLPTR)
        return;

    /* -------------------------------------------------------------------- */
    /*      Update the header if we have modified anything.                 */
    /* -------------------------------------------------------------------- */
    if (psSHP->bUpdated)
        SHPWriteHeader(psSHP);

    /* -------------------------------------------------------------------- */
    /*      Free all resources, and close files.                            */
    /* -------------------------------------------------------------------- */
    free(psSHP->panRecOffset);
    free(psSHP->panRecSize);

    if (psSHP->fpSHX != SHPLIB_NULLPTR)
        psSHP->sHooks.FClose(psSHP->fpSHX);
    psSHP->sHooks.FClose(psSHP->fpSHP);

    if (psSHP->pabyRec != SHPLIB_NULLPTR)
    {
        free(psSHP->pabyRec);
    }

    if (psSHP->pabyObjectBuf != SHPLIB_NULLPTR)
    {
        free(psSHP->pabyObjectBuf);
    }
    if (psSHP->psCachedObject != SHPLIB_NULLPTR)
    {
        free(psSHP->psCachedObject);
    }

    free(psSHP);
}

/************************************************************************/
/*                    SHPSetFastModeReadObject()                        */
/************************************************************************/

/* If setting bFastMode = TRUE, the content of SHPReadObject() is owned by the SHPHandle. */
/* So you cannot have 2 valid instances of SHPReadObject() simultaneously. */
/* The SHPObject padfZ and padfM members may be NULL depending on the geometry */
/* type. It is illegal to free at hand any of the pointer members of the SHPObject structure */
void SHPAPI_CALL SHPSetFastModeReadObject(SHPHandle hSHP, int bFastMode)
{
    if (bFastMode)
    {
        if (hSHP->psCachedObject == SHPLIB_NULLPTR)
        {
            hSHP->psCachedObject =
                STATIC_CAST(SHPObject *, calloc(1, sizeof(SHPObject)));
            assert(hSHP->psCachedObject != SHPLIB_NULLPTR);
        }
    }

    hSHP->bFastModeReadObject = bFastMode;
}

/************************************************************************/
/*                             SHPGetInfo()                             */
/*                                                                      */
/*      Fetch general information about the shape file.                 */
/************************************************************************/

void SHPAPI_CALL SHPGetInfo(const SHPHandle psSHP, int *pnEntities,
                            int *pnShapeType, double *padfMinBound,
                            double *padfMaxBound)
{
    if (psSHP == SHPLIB_NULLPTR)
        return;

    if (pnEntities != SHPLIB_NULLPTR)
        *pnEntities = psSHP->nRecords;

    if (pnShapeType != SHPLIB_NULLPTR)
        *pnShapeType = psSHP->nShapeType;

    for (int i = 0; i < 4; i++)
    {
        if (padfMinBound != SHPLIB_NULLPTR)
            padfMinBound[i] = psSHP->adBoundsMin[i];
        if (padfMaxBound != SHPLIB_NULLPTR)
            padfMaxBound[i] = psSHP->adBoundsMax[i];
    }
}

/************************************************************************/
/*                             SHPCreate()                              */
/*                                                                      */
/*      Create a new shape file and return a handle to the open         */
/*      shape file with read/write access.                              */
/************************************************************************/

SHPHandle SHPAPI_CALL SHPCreate(const char *pszLayer, int nShapeType)
{
    SAHooks sHooks;

    SASetupDefaultHooks(&sHooks);

    return SHPCreateLL(pszLayer, nShapeType, &sHooks);
}

/************************************************************************/
/*                             SHPCreate()                              */
/*                                                                      */
/*      Create a new shape file and return a handle to the open         */
/*      shape file with read/write access.                              */
/************************************************************************/

SHPHandle SHPAPI_CALL SHPCreateLL(const char *pszLayer, int nShapeType,
                                  const SAHooks *psHooks)
{
    /* -------------------------------------------------------------------- */
    /*      Open the two files so we can write their headers.               */
    /* -------------------------------------------------------------------- */
    const int nLenWithoutExtension = SHPGetLenWithoutExtension(pszLayer);
    char *pszFullname = STATIC_CAST(char *, malloc(nLenWithoutExtension + 5));
    memcpy(pszFullname, pszLayer, nLenWithoutExtension);
    memcpy(pszFullname + nLenWithoutExtension, ".shp", 5);
    SAFile fpSHP = psHooks->FOpen(pszFullname, "w+b", psHooks->pvUserData);
    if (fpSHP == SHPLIB_NULLPTR)
    {
        char szErrorMsg[200];
        snprintf(szErrorMsg, sizeof(szErrorMsg), "Failed to create file %s: %s",
                 pszFullname, strerror(errno));
        psHooks->Error(szErrorMsg);

        free(pszFullname);
        return SHPLIB_NULLPTR;
    }

    memcpy(pszFullname + nLenWithoutExtension, ".shx", 5);
    SAFile fpSHX = psHooks->FOpen(pszFullname, "w+b", psHooks->pvUserData);
    if (fpSHX == SHPLIB_NULLPTR)
    {
        char szErrorMsg[200];
        snprintf(szErrorMsg, sizeof(szErrorMsg), "Failed to create file %s: %s",
                 pszFullname, strerror(errno));
        psHooks->Error(szErrorMsg);

        free(pszFullname);
        psHooks->FClose(fpSHP);
        return SHPLIB_NULLPTR;
    }

    free(pszFullname);
    pszFullname = SHPLIB_NULLPTR;

    /* -------------------------------------------------------------------- */
    /*      Prepare header block for .shp file.                             */
    /* -------------------------------------------------------------------- */
    unsigned char abyHeader[100];
    memset(abyHeader, 0, sizeof(abyHeader));

    abyHeader[2] = 0x27; /* magic cookie */
    abyHeader[3] = 0x0a;

    uint32_t i32 = 50; /* file size */
    ByteCopy(&i32, abyHeader + 24, 4);
#if !defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(abyHeader + 24);
#endif

    i32 = 1000; /* version */
    ByteCopy(&i32, abyHeader + 28, 4);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(abyHeader + 28);
#endif

    i32 = nShapeType; /* shape type */
    ByteCopy(&i32, abyHeader + 32, 4);
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(abyHeader + 32);
#endif

    double dValue = 0.0; /* set bounds */
    ByteCopy(&dValue, abyHeader + 36, 8);
    ByteCopy(&dValue, abyHeader + 44, 8);
    ByteCopy(&dValue, abyHeader + 52, 8);
    ByteCopy(&dValue, abyHeader + 60, 8);

    /* -------------------------------------------------------------------- */
    /*      Write .shp file header.                                         */
    /* -------------------------------------------------------------------- */
    if (psHooks->FWrite(abyHeader, 100, 1, fpSHP) != 1)
    {
        char szErrorMsg[200];

        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Failed to write .shp header: %s", strerror(errno));
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psHooks->Error(szErrorMsg);

        free(pszFullname);
        psHooks->FClose(fpSHP);
        psHooks->FClose(fpSHX);
        return SHPLIB_NULLPTR;
    }

    /* -------------------------------------------------------------------- */
    /*      Prepare, and write .shx file header.                            */
    /* -------------------------------------------------------------------- */
    i32 = 50; /* file size */
    ByteCopy(&i32, abyHeader + 24, 4);
#if !defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(abyHeader + 24);
#endif

    if (psHooks->FWrite(abyHeader, 100, 1, fpSHX) != 1)
    {
        char szErrorMsg[200];

        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Failure writing .shx header: %s", strerror(errno));
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psHooks->Error(szErrorMsg);

        free(pszFullname);
        psHooks->FClose(fpSHP);
        psHooks->FClose(fpSHX);
        return SHPLIB_NULLPTR;
    }

    SHPHandle psSHP = STATIC_CAST(SHPHandle, calloc(1, sizeof(SHPInfo)));

    psSHP->bUpdated = FALSE;
    memcpy(&(psSHP->sHooks), psHooks, sizeof(SAHooks));

    psSHP->fpSHP = fpSHP;
    psSHP->fpSHX = fpSHX;
    psSHP->nShapeType = nShapeType;
    psSHP->nFileSize = 100;
    psSHP->panRecOffset =
        STATIC_CAST(unsigned int *, malloc(sizeof(unsigned int)));
    psSHP->panRecSize =
        STATIC_CAST(unsigned int *, malloc(sizeof(unsigned int)));

    if (psSHP->panRecOffset == SHPLIB_NULLPTR ||
        psSHP->panRecSize == SHPLIB_NULLPTR)
    {
        psSHP->sHooks.Error("Not enough memory to allocate requested memory");
        psSHP->sHooks.FClose(psSHP->fpSHP);
        psSHP->sHooks.FClose(psSHP->fpSHX);
        if (psSHP->panRecOffset)
            free(psSHP->panRecOffset);
        if (psSHP->panRecSize)
            free(psSHP->panRecSize);
        free(psSHP);
        return SHPLIB_NULLPTR;
    }

    return psSHP;
}

/************************************************************************/
/*                           _SHPSetBounds()                            */
/*                                                                      */
/*      Compute a bounds rectangle for a shape, and set it into the     */
/*      indicated location in the record.                               */
/************************************************************************/

static void _SHPSetBounds(unsigned char *pabyRec, const SHPObject *psShape)
{
    ByteCopy(&(psShape->dfXMin), pabyRec + 0, 8);
    ByteCopy(&(psShape->dfYMin), pabyRec + 8, 8);
    ByteCopy(&(psShape->dfXMax), pabyRec + 16, 8);
    ByteCopy(&(psShape->dfYMax), pabyRec + 24, 8);

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP64(pabyRec + 0);
    SHP_SWAP64(pabyRec + 8);
    SHP_SWAP64(pabyRec + 16);
    SHP_SWAP64(pabyRec + 24);
#endif
}

/************************************************************************/
/*                         SHPComputeExtents()                          */
/*                                                                      */
/*      Recompute the extents of a shape.  Automatically done by        */
/*      SHPCreateObject().                                              */
/************************************************************************/

void SHPAPI_CALL SHPComputeExtents(SHPObject *psObject)
{
    /* -------------------------------------------------------------------- */
    /*      Build extents for this object.                                  */
    /* -------------------------------------------------------------------- */
    if (psObject->nVertices > 0)
    {
        psObject->dfXMin = psObject->dfXMax = psObject->padfX[0];
        psObject->dfYMin = psObject->dfYMax = psObject->padfY[0];
        psObject->dfZMin = psObject->dfZMax = psObject->padfZ[0];
        psObject->dfMMin = psObject->dfMMax = psObject->padfM[0];
    }

    for (int i = 0; i < psObject->nVertices; i++)
    {
        psObject->dfXMin = MIN(psObject->dfXMin, psObject->padfX[i]);
        psObject->dfYMin = MIN(psObject->dfYMin, psObject->padfY[i]);
        psObject->dfZMin = MIN(psObject->dfZMin, psObject->padfZ[i]);
        psObject->dfMMin = MIN(psObject->dfMMin, psObject->padfM[i]);

        psObject->dfXMax = MAX(psObject->dfXMax, psObject->padfX[i]);
        psObject->dfYMax = MAX(psObject->dfYMax, psObject->padfY[i]);
        psObject->dfZMax = MAX(psObject->dfZMax, psObject->padfZ[i]);
        psObject->dfMMax = MAX(psObject->dfMMax, psObject->padfM[i]);
    }
}

/************************************************************************/
/*                          SHPCreateObject()                           */
/*                                                                      */
/*      Create a shape object.  It should be freed with                 */
/*      SHPDestroyObject().                                             */
/************************************************************************/

SHPObject SHPAPI_CALL1(*)
    SHPCreateObject(int nSHPType, int nShapeId, int nParts,
                    const int *panPartStart, const int *panPartType,
                    int nVertices, const double *padfX, const double *padfY,
                    const double *padfZ, const double *padfM)
{
    SHPObject *psObject =
        STATIC_CAST(SHPObject *, calloc(1, sizeof(SHPObject)));
    psObject->nSHPType = nSHPType;
    psObject->nShapeId = nShapeId;
    psObject->bMeasureIsUsed = FALSE;

    /* -------------------------------------------------------------------- */
    /*      Establish whether this shape type has M, and Z values.          */
    /* -------------------------------------------------------------------- */
    bool bHasM;
    bool bHasZ;

    if (nSHPType == SHPT_ARCM || nSHPType == SHPT_POINTM ||
        nSHPType == SHPT_POLYGONM || nSHPType == SHPT_MULTIPOINTM)
    {
        bHasM = true;
        bHasZ = false;
    }
    else if (nSHPType == SHPT_ARCZ || nSHPType == SHPT_POINTZ ||
             nSHPType == SHPT_POLYGONZ || nSHPType == SHPT_MULTIPOINTZ ||
             nSHPType == SHPT_MULTIPATCH)
    {
        bHasM = true;
        bHasZ = true;
    }
    else
    {
        bHasM = false;
        bHasZ = false;
    }

    /* -------------------------------------------------------------------- */
    /*      Capture parts.  Note that part type is optional, and            */
    /*      defaults to ring.                                               */
    /* -------------------------------------------------------------------- */
    if (nSHPType == SHPT_ARC || nSHPType == SHPT_POLYGON ||
        nSHPType == SHPT_ARCM || nSHPType == SHPT_POLYGONM ||
        nSHPType == SHPT_ARCZ || nSHPType == SHPT_POLYGONZ ||
        nSHPType == SHPT_MULTIPATCH)
    {
        psObject->nParts = MAX(1, nParts);

        psObject->panPartStart =
            STATIC_CAST(int *, calloc(psObject->nParts, sizeof(int)));
        psObject->panPartType =
            STATIC_CAST(int *, malloc(sizeof(int) * psObject->nParts));

        psObject->panPartStart[0] = 0;
        psObject->panPartType[0] = SHPP_RING;

        for (int i = 0; i < nParts; i++)
        {
            if (panPartStart != SHPLIB_NULLPTR)
                psObject->panPartStart[i] = panPartStart[i];

            if (panPartType != SHPLIB_NULLPTR)
                psObject->panPartType[i] = panPartType[i];
            else
                psObject->panPartType[i] = SHPP_RING;
        }

        psObject->panPartStart[0] = 0;
    }

    /* -------------------------------------------------------------------- */
    /*      Capture vertices.  Note that X, Y, Z and M are optional.        */
    /* -------------------------------------------------------------------- */
    if (nVertices > 0)
    {
        const size_t nSize = sizeof(double) * nVertices;
        psObject->padfX =
            STATIC_CAST(double *, padfX ? malloc(nSize)
                                        : calloc(nVertices, sizeof(double)));
        psObject->padfY =
            STATIC_CAST(double *, padfY ? malloc(nSize)
                                        : calloc(nVertices, sizeof(double)));
        psObject->padfZ = STATIC_CAST(
            double *,
            padfZ &&bHasZ ? malloc(nSize) : calloc(nVertices, sizeof(double)));
        psObject->padfM = STATIC_CAST(
            double *,
            padfM &&bHasM ? malloc(nSize) : calloc(nVertices, sizeof(double)));
        if (padfX != SHPLIB_NULLPTR)
            memcpy(psObject->padfX, padfX, nSize);
        if (padfY != SHPLIB_NULLPTR)
            memcpy(psObject->padfY, padfY, nSize);
        if (padfZ != SHPLIB_NULLPTR && bHasZ)
            memcpy(psObject->padfZ, padfZ, nSize);
        if (padfM != SHPLIB_NULLPTR && bHasM)
        {
            memcpy(psObject->padfM, padfM, nSize);
            psObject->bMeasureIsUsed = TRUE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Compute the extents.                                            */
    /* -------------------------------------------------------------------- */
    psObject->nVertices = nVertices;
    SHPComputeExtents(psObject);

    return (psObject);
}

/************************************************************************/
/*                       SHPCreateSimpleObject()                        */
/*                                                                      */
/*      Create a simple (common) shape object.  Destroy with            */
/*      SHPDestroyObject().                                             */
/************************************************************************/

SHPObject SHPAPI_CALL1(*)
    SHPCreateSimpleObject(int nSHPType, int nVertices, const double *padfX,
                          const double *padfY, const double *padfZ)
{
    return (SHPCreateObject(nSHPType, -1, 0, SHPLIB_NULLPTR, SHPLIB_NULLPTR,
                            nVertices, padfX, padfY, padfZ, SHPLIB_NULLPTR));
}

/************************************************************************/
/*                           SHPWriteObject()                           */
/*                                                                      */
/*      Write out the vertices of a new structure.  Note that it is     */
/*      only possible to write vertices at the end of the file.         */
/************************************************************************/

int SHPAPI_CALL SHPWriteObject(SHPHandle psSHP, int nShapeId,
                               const SHPObject *psObject)
{
    psSHP->bUpdated = TRUE;

    /* -------------------------------------------------------------------- */
    /*      Ensure that shape object matches the type of the file it is     */
    /*      being written to.                                               */
    /* -------------------------------------------------------------------- */
    assert(psObject->nSHPType == psSHP->nShapeType ||
           psObject->nSHPType == SHPT_NULL);

    /* -------------------------------------------------------------------- */
    /*      Ensure that -1 is used for appends.  Either blow an             */
    /*      assertion, or if they are disabled, set the shapeid to -1       */
    /*      for appends.                                                    */
    /* -------------------------------------------------------------------- */
    assert(nShapeId == -1 || (nShapeId >= 0 && nShapeId < psSHP->nRecords));

    if (nShapeId != -1 && nShapeId >= psSHP->nRecords)
        nShapeId = -1;

    /* -------------------------------------------------------------------- */
    /*      Add the new entity to the in memory index.                      */
    /* -------------------------------------------------------------------- */
    if (nShapeId == -1 && psSHP->nRecords + 1 > psSHP->nMaxRecords)
    {
        /* This cannot overflow given that we check that the file size does
         * not grow over 4 GB, and the minimum size of a record is 12 bytes,
         * hence the maximm value for nMaxRecords is 357,913,941
         */
        int nNewMaxRecords = psSHP->nMaxRecords + psSHP->nMaxRecords / 3 + 100;
        unsigned int *panRecOffsetNew;
        unsigned int *panRecSizeNew;

        panRecOffsetNew = STATIC_CAST(
            unsigned int *, realloc(psSHP->panRecOffset,
                                    sizeof(unsigned int) * nNewMaxRecords));
        if (panRecOffsetNew == SHPLIB_NULLPTR)
        {
            psSHP->sHooks.Error("Failed to write shape object. "
                                "Memory allocation error.");
            return -1;
        }
        psSHP->panRecOffset = panRecOffsetNew;

        panRecSizeNew = STATIC_CAST(
            unsigned int *,
            realloc(psSHP->panRecSize, sizeof(unsigned int) * nNewMaxRecords));
        if (panRecSizeNew == SHPLIB_NULLPTR)
        {
            psSHP->sHooks.Error("Failed to write shape object. "
                                "Memory allocation error.");
            return -1;
        }
        psSHP->panRecSize = panRecSizeNew;

        psSHP->nMaxRecords = nNewMaxRecords;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize record.                                              */
    /* -------------------------------------------------------------------- */

    /* The following computation cannot overflow on 32-bit platforms given that
     * the user had to allocate arrays of at least that size. */
    size_t nRecMaxSize =
        psObject->nVertices * 4 * sizeof(double) + psObject->nParts * 8;
    /* But the following test could trigger on 64-bit platforms on huge
     * geometries. */
    const unsigned nExtraSpaceForGeomHeader = 128;
    if (nRecMaxSize > UINT_MAX - nExtraSpaceForGeomHeader)
    {
        psSHP->sHooks.Error("Failed to write shape object. Too big geometry.");
        return -1;
    }
    nRecMaxSize += nExtraSpaceForGeomHeader;
    unsigned char *pabyRec = STATIC_CAST(unsigned char *, malloc(nRecMaxSize));
    if (pabyRec == SHPLIB_NULLPTR)
    {
        psSHP->sHooks.Error("Failed to write shape object. "
                            "Memory allocation error.");
        return -1;
    }

    /* -------------------------------------------------------------------- */
    /*      Extract vertices for a Polygon or Arc.                          */
    /* -------------------------------------------------------------------- */
    unsigned int nRecordSize = 0;
    const bool bFirstFeature = psSHP->nRecords == 0;

    if (psObject->nSHPType == SHPT_POLYGON ||
        psObject->nSHPType == SHPT_POLYGONZ ||
        psObject->nSHPType == SHPT_POLYGONM || psObject->nSHPType == SHPT_ARC ||
        psObject->nSHPType == SHPT_ARCZ || psObject->nSHPType == SHPT_ARCM ||
        psObject->nSHPType == SHPT_MULTIPATCH)
    {
        uint32_t nPoints = psObject->nVertices;
        uint32_t nParts = psObject->nParts;

        _SHPSetBounds(pabyRec + 12, psObject);

#if defined(SHP_BIG_ENDIAN)
        SHP_SWAP32(&nPoints);
        SHP_SWAP32(&nParts);
#endif

        ByteCopy(&nPoints, pabyRec + 40 + 8, 4);
        ByteCopy(&nParts, pabyRec + 36 + 8, 4);

        nRecordSize = 52;

        /*
         * Write part start positions.
         */
        ByteCopy(psObject->panPartStart, pabyRec + 44 + 8,
                 4 * psObject->nParts);
        for (int i = 0; i < psObject->nParts; i++)
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP32(pabyRec + 44 + 8 + 4 * i);
#endif
            nRecordSize += 4;
        }

        /*
         * Write multipatch part types if needed.
         */
        if (psObject->nSHPType == SHPT_MULTIPATCH)
        {
            memcpy(pabyRec + nRecordSize, psObject->panPartType,
                   4 * psObject->nParts);
            for (int i = 0; i < psObject->nParts; i++)
            {
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAP32(pabyRec + nRecordSize);
#endif
                nRecordSize += 4;
            }
        }

        /*
         * Write the (x,y) vertex values.
         */
        for (int i = 0; i < psObject->nVertices; i++)
        {
            ByteCopy(psObject->padfX + i, pabyRec + nRecordSize, 8);
            ByteCopy(psObject->padfY + i, pabyRec + nRecordSize + 8, 8);

#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
            SHP_SWAP64(pabyRec + nRecordSize + 8);
#endif

            nRecordSize += 2 * 8;
        }

        /*
         * Write the Z coordinates (if any).
         */
        if (psObject->nSHPType == SHPT_POLYGONZ ||
            psObject->nSHPType == SHPT_ARCZ ||
            psObject->nSHPType == SHPT_MULTIPATCH)
        {
            ByteCopy(&(psObject->dfZMin), pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;

            ByteCopy(&(psObject->dfZMax), pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;

            for (int i = 0; i < psObject->nVertices; i++)
            {
                ByteCopy(psObject->padfZ + i, pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAP64(pabyRec + nRecordSize);
#endif
                nRecordSize += 8;
            }
        }

        /*
         * Write the M values, if any.
         */
        if (psObject->bMeasureIsUsed &&
            (psObject->nSHPType == SHPT_POLYGONM ||
             psObject->nSHPType == SHPT_ARCM
#ifndef DISABLE_MULTIPATCH_MEASURE
             || psObject->nSHPType == SHPT_MULTIPATCH
#endif
             || psObject->nSHPType == SHPT_POLYGONZ ||
             psObject->nSHPType == SHPT_ARCZ))
        {
            ByteCopy(&(psObject->dfMMin), pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;

            ByteCopy(&(psObject->dfMMax), pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;

            for (int i = 0; i < psObject->nVertices; i++)
            {
                ByteCopy(psObject->padfM + i, pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAP64(pabyRec + nRecordSize);
#endif
                nRecordSize += 8;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Extract vertices for a MultiPoint.                              */
    /* -------------------------------------------------------------------- */
    else if (psObject->nSHPType == SHPT_MULTIPOINT ||
             psObject->nSHPType == SHPT_MULTIPOINTZ ||
             psObject->nSHPType == SHPT_MULTIPOINTM)
    {
        uint32_t nPoints = psObject->nVertices;

        _SHPSetBounds(pabyRec + 12, psObject);

#if defined(SHP_BIG_ENDIAN)
        SHP_SWAP32(&nPoints);
#endif
        ByteCopy(&nPoints, pabyRec + 44, 4);

        for (int i = 0; i < psObject->nVertices; i++)
        {
            ByteCopy(psObject->padfX + i, pabyRec + 48 + i * 16, 8);
            ByteCopy(psObject->padfY + i, pabyRec + 48 + i * 16 + 8, 8);

#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + 48 + i * 16);
            SHP_SWAP64(pabyRec + 48 + i * 16 + 8);
#endif
        }

        nRecordSize = 48 + 16 * psObject->nVertices;

        if (psObject->nSHPType == SHPT_MULTIPOINTZ)
        {
            ByteCopy(&(psObject->dfZMin), pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;

            ByteCopy(&(psObject->dfZMax), pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;

            for (int i = 0; i < psObject->nVertices; i++)
            {
                ByteCopy(psObject->padfZ + i, pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAP64(pabyRec + nRecordSize);
#endif
                nRecordSize += 8;
            }
        }

        if (psObject->bMeasureIsUsed &&
            (psObject->nSHPType == SHPT_MULTIPOINTZ ||
             psObject->nSHPType == SHPT_MULTIPOINTM))
        {
            ByteCopy(&(psObject->dfMMin), pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;

            ByteCopy(&(psObject->dfMMax), pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;

            for (int i = 0; i < psObject->nVertices; i++)
            {
                ByteCopy(psObject->padfM + i, pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAP64(pabyRec + nRecordSize);
#endif
                nRecordSize += 8;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Write point.                                                    */
    /* -------------------------------------------------------------------- */
    else if (psObject->nSHPType == SHPT_POINT ||
             psObject->nSHPType == SHPT_POINTZ ||
             psObject->nSHPType == SHPT_POINTM)
    {
        ByteCopy(psObject->padfX, pabyRec + 12, 8);
        ByteCopy(psObject->padfY, pabyRec + 20, 8);

#if defined(SHP_BIG_ENDIAN)
        SHP_SWAP64(pabyRec + 12);
        SHP_SWAP64(pabyRec + 20);
#endif

        nRecordSize = 28;

        if (psObject->nSHPType == SHPT_POINTZ)
        {
            ByteCopy(psObject->padfZ, pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;
        }

        if (psObject->bMeasureIsUsed && (psObject->nSHPType == SHPT_POINTZ ||
                                         psObject->nSHPType == SHPT_POINTM))
        {
            ByteCopy(psObject->padfM, pabyRec + nRecordSize, 8);
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP64(pabyRec + nRecordSize);
#endif
            nRecordSize += 8;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Not much to do for null geometries.                             */
    /* -------------------------------------------------------------------- */
    else if (psObject->nSHPType == SHPT_NULL)
    {
        nRecordSize = 12;
    }
    else
    {
        /* unknown type */
        assert(false);
    }

    /* -------------------------------------------------------------------- */
    /*      Establish where we are going to put this record. If we are      */
    /*      rewriting the last record of the file, then we can update it in */
    /*      place. Otherwise if rewriting an existing record, and it will   */
    /*      fit, then put it  back where the original came from.  Otherwise */
    /*      write at the end.                                               */
    /* -------------------------------------------------------------------- */
    SAOffset nRecordOffset;
    bool bAppendToLastRecord = false;
    bool bAppendToFile = false;
    if (nShapeId != -1 &&
        psSHP->panRecOffset[nShapeId] + psSHP->panRecSize[nShapeId] + 8 ==
            psSHP->nFileSize)
    {
        nRecordOffset = psSHP->panRecOffset[nShapeId];
        bAppendToLastRecord = true;
    }
    else if (nShapeId == -1 || psSHP->panRecSize[nShapeId] < nRecordSize - 8)
    {
        if (psSHP->nFileSize > UINT_MAX - nRecordSize)
        {
            char str[255];
            snprintf(str, sizeof(str),
                     "Failed to write shape object. "
                     "The maximum file size of %u has been reached. "
                     "The current record of size %u cannot be added.",
                     psSHP->nFileSize, nRecordSize);
            str[sizeof(str) - 1] = '\0';
            psSHP->sHooks.Error(str);
            free(pabyRec);
            return -1;
        }

        bAppendToFile = true;
        nRecordOffset = psSHP->nFileSize;
    }
    else
    {
        nRecordOffset = psSHP->panRecOffset[nShapeId];
    }

    /* -------------------------------------------------------------------- */
    /*      Set the shape type, record number, and record size.             */
    /* -------------------------------------------------------------------- */
    uint32_t i32 =
        (nShapeId < 0) ? psSHP->nRecords + 1 : nShapeId + 1; /* record # */
#if !defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(&i32);
#endif
    ByteCopy(&i32, pabyRec, 4);

    i32 = (nRecordSize - 8) / 2; /* record size */
#if !defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(&i32);
#endif
    ByteCopy(&i32, pabyRec + 4, 4);

    i32 = psObject->nSHPType; /* shape type */
#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(&i32);
#endif
    ByteCopy(&i32, pabyRec + 8, 4);

    /* -------------------------------------------------------------------- */
    /*      Write out record.                                               */
    /* -------------------------------------------------------------------- */

    /* -------------------------------------------------------------------- */
    /*      Guard FSeek with check for whether we're already at position;   */
    /*      no-op FSeeks defeat network filesystems' write buffering.       */
    /* -------------------------------------------------------------------- */
    if (psSHP->sHooks.FTell(psSHP->fpSHP) != nRecordOffset)
    {
        if (psSHP->sHooks.FSeek(psSHP->fpSHP, nRecordOffset, 0) != 0)
        {
            char szErrorMsg[200];

            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Error in psSHP->sHooks.FSeek() while writing object to "
                     ".shp file: %s",
                     strerror(errno));
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);

            free(pabyRec);
            return -1;
        }
    }
    if (psSHP->sHooks.FWrite(pabyRec, nRecordSize, 1, psSHP->fpSHP) < 1)
    {
        char szErrorMsg[200];

        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Error in psSHP->sHooks.FWrite() while writing object of %u "
                 "bytes to .shp file: %s",
                 nRecordSize, strerror(errno));
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psSHP->sHooks.Error(szErrorMsg);

        free(pabyRec);
        return -1;
    }

    free(pabyRec);

    if (bAppendToLastRecord)
    {
        psSHP->nFileSize = psSHP->panRecOffset[nShapeId] + nRecordSize;
    }
    else if (bAppendToFile)
    {
        if (nShapeId == -1)
            nShapeId = psSHP->nRecords++;

        psSHP->panRecOffset[nShapeId] = psSHP->nFileSize;
        psSHP->nFileSize += nRecordSize;
    }
    psSHP->panRecSize[nShapeId] = nRecordSize - 8;

    /* -------------------------------------------------------------------- */
    /*      Expand file wide bounds based on this shape.                    */
    /* -------------------------------------------------------------------- */
    if (bFirstFeature)
    {
        if (psObject->nSHPType == SHPT_NULL || psObject->nVertices == 0)
        {
            psSHP->adBoundsMin[0] = psSHP->adBoundsMax[0] = 0.0;
            psSHP->adBoundsMin[1] = psSHP->adBoundsMax[1] = 0.0;
            psSHP->adBoundsMin[2] = psSHP->adBoundsMax[2] = 0.0;
            psSHP->adBoundsMin[3] = psSHP->adBoundsMax[3] = 0.0;
        }
        else
        {
            psSHP->adBoundsMin[0] = psSHP->adBoundsMax[0] = psObject->padfX[0];
            psSHP->adBoundsMin[1] = psSHP->adBoundsMax[1] = psObject->padfY[0];
            psSHP->adBoundsMin[2] = psSHP->adBoundsMax[2] =
                psObject->padfZ ? psObject->padfZ[0] : 0.0;
            psSHP->adBoundsMin[3] = psSHP->adBoundsMax[3] =
                psObject->padfM ? psObject->padfM[0] : 0.0;
        }
    }

    for (int i = 0; i < psObject->nVertices; i++)
    {
        psSHP->adBoundsMin[0] = MIN(psSHP->adBoundsMin[0], psObject->padfX[i]);
        psSHP->adBoundsMin[1] = MIN(psSHP->adBoundsMin[1], psObject->padfY[i]);
        psSHP->adBoundsMax[0] = MAX(psSHP->adBoundsMax[0], psObject->padfX[i]);
        psSHP->adBoundsMax[1] = MAX(psSHP->adBoundsMax[1], psObject->padfY[i]);
        if (psObject->padfZ)
        {
            psSHP->adBoundsMin[2] =
                MIN(psSHP->adBoundsMin[2], psObject->padfZ[i]);
            psSHP->adBoundsMax[2] =
                MAX(psSHP->adBoundsMax[2], psObject->padfZ[i]);
        }
        if (psObject->padfM)
        {
            psSHP->adBoundsMin[3] =
                MIN(psSHP->adBoundsMin[3], psObject->padfM[i]);
            psSHP->adBoundsMax[3] =
                MAX(psSHP->adBoundsMax[3], psObject->padfM[i]);
        }
    }

    return (nShapeId);
}

/************************************************************************/
/*                         SHPAllocBuffer()                             */
/************************************************************************/

static void *SHPAllocBuffer(unsigned char **pBuffer, int nSize)
{
    if (pBuffer == SHPLIB_NULLPTR)
        return calloc(1, nSize);

    unsigned char *pRet = *pBuffer;
    if (pRet == SHPLIB_NULLPTR)
        return SHPLIB_NULLPTR;

    (*pBuffer) += nSize;
    return pRet;
}

/************************************************************************/
/*                    SHPReallocObjectBufIfNecessary()                  */
/************************************************************************/

static unsigned char *SHPReallocObjectBufIfNecessary(SHPHandle psSHP,
                                                     int nObjectBufSize)
{
    if (nObjectBufSize == 0)
    {
        nObjectBufSize = 4 * sizeof(double);
    }

    unsigned char *pBuffer;
    if (nObjectBufSize > psSHP->nObjectBufSize)
    {
        pBuffer = STATIC_CAST(unsigned char *,
                              realloc(psSHP->pabyObjectBuf, nObjectBufSize));
        if (pBuffer != SHPLIB_NULLPTR)
        {
            psSHP->pabyObjectBuf = pBuffer;
            psSHP->nObjectBufSize = nObjectBufSize;
        }
    }
    else
    {
        pBuffer = psSHP->pabyObjectBuf;
    }

    return pBuffer;
}

/************************************************************************/
/*                          SHPReadObject()                             */
/*                                                                      */
/*      Read the vertices, parts, and other non-attribute information   */
/*      for one shape.                                                  */
/************************************************************************/

SHPObject SHPAPI_CALL1(*) SHPReadObject(const SHPHandle psSHP, int hEntity)
{
    /* -------------------------------------------------------------------- */
    /*      Validate the record/entity number.                              */
    /* -------------------------------------------------------------------- */
    if (hEntity < 0 || hEntity >= psSHP->nRecords)
        return SHPLIB_NULLPTR;

    /* -------------------------------------------------------------------- */
    /*      Read offset/length from SHX loading if necessary.               */
    /* -------------------------------------------------------------------- */
    if (psSHP->panRecOffset[hEntity] == 0 && psSHP->fpSHX != SHPLIB_NULLPTR)
    {
        unsigned int nOffset;
        unsigned int nLength;

        if (psSHP->sHooks.FSeek(psSHP->fpSHX, 100 + 8 * hEntity, 0) != 0 ||
            psSHP->sHooks.FRead(&nOffset, 1, 4, psSHP->fpSHX) != 4 ||
            psSHP->sHooks.FRead(&nLength, 1, 4, psSHP->fpSHX) != 4)
        {
            char str[128];
            snprintf(str, sizeof(str),
                     "Error in fseek()/fread() reading object from .shx file "
                     "at offset %d",
                     100 + 8 * hEntity);
            str[sizeof(str) - 1] = '\0';

            psSHP->sHooks.Error(str);
            return SHPLIB_NULLPTR;
        }
#if !defined(SHP_BIG_ENDIAN)
        SHP_SWAP32(&nOffset);
        SHP_SWAP32(&nLength);
#endif

        if (nOffset > STATIC_CAST(unsigned int, INT_MAX))
        {
            char str[128];
            snprintf(str, sizeof(str), "Invalid offset for entity %d", hEntity);
            str[sizeof(str) - 1] = '\0';

            psSHP->sHooks.Error(str);
            return SHPLIB_NULLPTR;
        }
        if (nLength > STATIC_CAST(unsigned int, INT_MAX / 2 - 4))
        {
            char str[128];
            snprintf(str, sizeof(str), "Invalid length for entity %d", hEntity);
            str[sizeof(str) - 1] = '\0';

            psSHP->sHooks.Error(str);
            return SHPLIB_NULLPTR;
        }

        psSHP->panRecOffset[hEntity] = nOffset * 2;
        psSHP->panRecSize[hEntity] = nLength * 2;
    }

    /* -------------------------------------------------------------------- */
    /*      Ensure our record buffer is large enough.                       */
    /* -------------------------------------------------------------------- */
    const int nEntitySize = psSHP->panRecSize[hEntity] + 8;
    if (nEntitySize > psSHP->nBufSize)
    {
        int nNewBufSize = nEntitySize;
        if (nNewBufSize < INT_MAX - nNewBufSize / 3)
            nNewBufSize += nNewBufSize / 3;
        else
            nNewBufSize = INT_MAX;

        /* Before allocating too much memory, check that the file is big enough */
        /* and do not trust the file size in the header the first time we */
        /* need to allocate more than 10 MB */
        if (nNewBufSize >= 10 * 1024 * 1024)
        {
            if (psSHP->nBufSize < 10 * 1024 * 1024)
            {
                SAOffset nFileSize;
                psSHP->sHooks.FSeek(psSHP->fpSHP, 0, 2);
                nFileSize = psSHP->sHooks.FTell(psSHP->fpSHP);
                if (nFileSize >= UINT_MAX)
                    psSHP->nFileSize = UINT_MAX;
                else
                    psSHP->nFileSize = STATIC_CAST(unsigned int, nFileSize);
            }

            if (psSHP->panRecOffset[hEntity] >= psSHP->nFileSize ||
                /* We should normally use nEntitySize instead of*/
                /* psSHP->panRecSize[hEntity] in the below test, but because of */
                /* the case of non conformant .shx files detailed a bit below, */
                /* let be more tolerant */
                psSHP->panRecSize[hEntity] >
                    psSHP->nFileSize - psSHP->panRecOffset[hEntity])
            {
                char str[128];
                snprintf(str, sizeof(str),
                         "Error in fread() reading object of size %d at offset "
                         "%u from .shp file",
                         nEntitySize, psSHP->panRecOffset[hEntity]);
                str[sizeof(str) - 1] = '\0';

                psSHP->sHooks.Error(str);
                return SHPLIB_NULLPTR;
            }
        }

        unsigned char *pabyRecNew =
            STATIC_CAST(unsigned char *, realloc(psSHP->pabyRec, nNewBufSize));
        if (pabyRecNew == SHPLIB_NULLPTR)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Not enough memory to allocate requested memory "
                     "(nNewBufSize=%d). "
                     "Probably broken SHP file",
                     nNewBufSize);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            return SHPLIB_NULLPTR;
        }

        /* Only set new buffer size after successful alloc */
        psSHP->pabyRec = pabyRecNew;
        psSHP->nBufSize = nNewBufSize;
    }

    /* In case we were not able to reallocate the buffer on a previous step */
    if (psSHP->pabyRec == SHPLIB_NULLPTR)
    {
        return SHPLIB_NULLPTR;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the record.                                                */
    /* -------------------------------------------------------------------- */
    if (psSHP->sHooks.FSeek(psSHP->fpSHP, psSHP->panRecOffset[hEntity], 0) != 0)
    {
        /*
         * TODO - mloskot: Consider detailed diagnostics of shape file,
         * for example to detect if file is truncated.
         */
        char str[128];
        snprintf(str, sizeof(str),
                 "Error in fseek() reading object from .shp file at offset %u",
                 psSHP->panRecOffset[hEntity]);
        str[sizeof(str) - 1] = '\0';

        psSHP->sHooks.Error(str);
        return SHPLIB_NULLPTR;
    }

    const int nBytesRead = STATIC_CAST(
        int, psSHP->sHooks.FRead(psSHP->pabyRec, 1, nEntitySize, psSHP->fpSHP));

    /* Special case for a shapefile whose .shx content length field is not equal */
    /* to the content length field of the .shp, which is a violation of "The */
    /* content length stored in the index record is the same as the value stored in the main */
    /* file record header." (http://www.esri.com/library/whitepapers/pdfs/shapefile.pdf, page 24) */
    /* Actually in that case the .shx content length is equal to the .shp content length + */
    /* 4 (16 bit words), representing the 8 bytes of the record header... */
    if (nBytesRead >= 8 && nBytesRead == nEntitySize - 8)
    {
        /* Do a sanity check */
        int nSHPContentLength;
        memcpy(&nSHPContentLength, psSHP->pabyRec + 4, 4);
#if !defined(SHP_BIG_ENDIAN)
        SHP_SWAP32(&(nSHPContentLength));
#endif
        if (nSHPContentLength < 0 || nSHPContentLength > INT_MAX / 2 - 4 ||
            2 * nSHPContentLength + 8 != nBytesRead)
        {
            char str[128];
            snprintf(str, sizeof(str),
                     "Sanity check failed when trying to recover from "
                     "inconsistent .shx/.shp with shape %d",
                     hEntity);
            str[sizeof(str) - 1] = '\0';

            psSHP->sHooks.Error(str);
            return SHPLIB_NULLPTR;
        }
    }
    else if (nBytesRead != nEntitySize)
    {
        /*
         * TODO - mloskot: Consider detailed diagnostics of shape file,
         * for example to detect if file is truncated.
         */
        char str[128];
        snprintf(str, sizeof(str),
                 "Error in fread() reading object of size %d at offset %u from "
                 ".shp file",
                 nEntitySize, psSHP->panRecOffset[hEntity]);
        str[sizeof(str) - 1] = '\0';

        psSHP->sHooks.Error(str);
        return SHPLIB_NULLPTR;
    }

    if (8 + 4 > nEntitySize)
    {
        char szErrorMsg[160];
        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Corrupted .shp file : shape %d : nEntitySize = %d", hEntity,
                 nEntitySize);
        szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
        psSHP->sHooks.Error(szErrorMsg);
        return SHPLIB_NULLPTR;
    }
    int nSHPType;
    memcpy(&nSHPType, psSHP->pabyRec + 8, 4);

#if defined(SHP_BIG_ENDIAN)
    SHP_SWAP32(&(nSHPType));
#endif

    /* -------------------------------------------------------------------- */
    /*      Allocate and minimally initialize the object.                   */
    /* -------------------------------------------------------------------- */
    SHPObject *psShape;
    if (psSHP->bFastModeReadObject)
    {
        if (psSHP->psCachedObject->bFastModeReadObject)
        {
            psSHP->sHooks.Error("Invalid read pattern in fast read mode. "
                                "SHPDestroyObject() should be called.");
            return SHPLIB_NULLPTR;
        }

        psShape = psSHP->psCachedObject;
        memset(psShape, 0, sizeof(SHPObject));
    }
    else
    {
        psShape = STATIC_CAST(SHPObject *, calloc(1, sizeof(SHPObject)));
    }
    psShape->nShapeId = hEntity;
    psShape->nSHPType = nSHPType;
    psShape->bMeasureIsUsed = FALSE;
    psShape->bFastModeReadObject = psSHP->bFastModeReadObject;

    /* ==================================================================== */
    /*  Extract vertices for a Polygon or Arc.                              */
    /* ==================================================================== */
    if (psShape->nSHPType == SHPT_POLYGON || psShape->nSHPType == SHPT_ARC ||
        psShape->nSHPType == SHPT_POLYGONZ ||
        psShape->nSHPType == SHPT_POLYGONM || psShape->nSHPType == SHPT_ARCZ ||
        psShape->nSHPType == SHPT_ARCM || psShape->nSHPType == SHPT_MULTIPATCH)
    {
        if (40 + 8 + 4 > nEntitySize)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Corrupted .shp file : shape %d : nEntitySize = %d",
                     hEntity, nEntitySize);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            SHPDestroyObject(psShape);
            return SHPLIB_NULLPTR;
        }
        /* -------------------------------------------------------------------- */
        /*      Get the X/Y bounds.                                             */
        /* -------------------------------------------------------------------- */
#if defined(SHP_BIG_ENDIAN)
        SHP_SWAPDOUBLE_CPY(&psShape->dfXMin, psSHP->pabyRec + 8 + 4);
        SHP_SWAPDOUBLE_CPY(&psShape->dfYMin, psSHP->pabyRec + 8 + 12);
        SHP_SWAPDOUBLE_CPY(&psShape->dfXMax, psSHP->pabyRec + 8 + 20);
        SHP_SWAPDOUBLE_CPY(&psShape->dfYMax, psSHP->pabyRec + 8 + 28);
#else
        memcpy(&psShape->dfXMin, psSHP->pabyRec + 8 + 4, 8);
        memcpy(&psShape->dfYMin, psSHP->pabyRec + 8 + 12, 8);
        memcpy(&psShape->dfXMax, psSHP->pabyRec + 8 + 20, 8);
        memcpy(&psShape->dfYMax, psSHP->pabyRec + 8 + 28, 8);
#endif

        /* -------------------------------------------------------------------- */
        /*      Extract part/point count, and build vertex and part arrays      */
        /*      to proper size.                                                 */
        /* -------------------------------------------------------------------- */
        uint32_t nPoints;
        memcpy(&nPoints, psSHP->pabyRec + 40 + 8, 4);
        uint32_t nParts;
        memcpy(&nParts, psSHP->pabyRec + 36 + 8, 4);

#if defined(SHP_BIG_ENDIAN)
        SHP_SWAP32(&nPoints);
        SHP_SWAP32(&nParts);
#endif

        /* nPoints and nParts are unsigned */
        if (/* nPoints < 0 || nParts < 0 || */
            nPoints > 50 * 1000 * 1000 || nParts > 10 * 1000 * 1000)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Corrupted .shp file : shape %d, nPoints=%u, nParts=%u.",
                     hEntity, nPoints, nParts);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            SHPDestroyObject(psShape);
            return SHPLIB_NULLPTR;
        }

        /* With the previous checks on nPoints and nParts, */
        /* we should not overflow here and after */
        /* since 50 M * (16 + 8 + 8) = 1 600 MB */
        int nRequiredSize = 44 + 8 + 4 * nParts + 16 * nPoints;
        if (psShape->nSHPType == SHPT_POLYGONZ ||
            psShape->nSHPType == SHPT_ARCZ ||
            psShape->nSHPType == SHPT_MULTIPATCH)
        {
            nRequiredSize += 16 + 8 * nPoints;
        }
        if (psShape->nSHPType == SHPT_MULTIPATCH)
        {
            nRequiredSize += 4 * nParts;
        }
        if (nRequiredSize > nEntitySize)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Corrupted .shp file : shape %d, nPoints=%u, nParts=%u, "
                     "nEntitySize=%d.",
                     hEntity, nPoints, nParts, nEntitySize);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            SHPDestroyObject(psShape);
            return SHPLIB_NULLPTR;
        }

        unsigned char *pBuffer = SHPLIB_NULLPTR;
        unsigned char **ppBuffer = SHPLIB_NULLPTR;

        if (psShape->bFastModeReadObject)
        {
            const int nObjectBufSize =
                4 * sizeof(double) * nPoints + 2 * sizeof(int) * nParts;
            pBuffer = SHPReallocObjectBufIfNecessary(psSHP, nObjectBufSize);
            ppBuffer = &pBuffer;
        }

        psShape->nVertices = nPoints;
        psShape->padfX = STATIC_CAST(
            double *, SHPAllocBuffer(ppBuffer, sizeof(double) * nPoints));
        psShape->padfY = STATIC_CAST(
            double *, SHPAllocBuffer(ppBuffer, sizeof(double) * nPoints));
        psShape->padfZ = STATIC_CAST(
            double *, SHPAllocBuffer(ppBuffer, sizeof(double) * nPoints));
        psShape->padfM = STATIC_CAST(
            double *, SHPAllocBuffer(ppBuffer, sizeof(double) * nPoints));

        psShape->nParts = nParts;
        psShape->panPartStart =
            STATIC_CAST(int *, SHPAllocBuffer(ppBuffer, nParts * sizeof(int)));
        psShape->panPartType =
            STATIC_CAST(int *, SHPAllocBuffer(ppBuffer, nParts * sizeof(int)));

        if (psShape->padfX == SHPLIB_NULLPTR ||
            psShape->padfY == SHPLIB_NULLPTR ||
            psShape->padfZ == SHPLIB_NULLPTR ||
            psShape->padfM == SHPLIB_NULLPTR ||
            psShape->panPartStart == SHPLIB_NULLPTR ||
            psShape->panPartType == SHPLIB_NULLPTR)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Not enough memory to allocate requested memory "
                     "(nPoints=%u, nParts=%u) for shape %d. "
                     "Probably broken SHP file",
                     nPoints, nParts, hEntity);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            SHPDestroyObject(psShape);
            return SHPLIB_NULLPTR;
        }

        for (int i = 0; STATIC_CAST(uint32_t, i) < nParts; i++)
            psShape->panPartType[i] = SHPP_RING;

        /* -------------------------------------------------------------------- */
        /*      Copy out the part array from the record.                        */
        /* -------------------------------------------------------------------- */
        memcpy(psShape->panPartStart, psSHP->pabyRec + 44 + 8, 4 * nParts);
        for (int i = 0; STATIC_CAST(uint32_t, i) < nParts; i++)
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAP32(psShape->panPartStart + i);
#endif

            /* We check that the offset is inside the vertex array */
            if (psShape->panPartStart[i] < 0 ||
                (psShape->panPartStart[i] >= psShape->nVertices &&
                 psShape->nVertices > 0) ||
                (psShape->panPartStart[i] > 0 && psShape->nVertices == 0))
            {
                char szErrorMsg[160];
                snprintf(szErrorMsg, sizeof(szErrorMsg),
                         "Corrupted .shp file : shape %d : panPartStart[%d] = "
                         "%d, nVertices = %d",
                         hEntity, i, psShape->panPartStart[i],
                         psShape->nVertices);
                szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
                psSHP->sHooks.Error(szErrorMsg);
                SHPDestroyObject(psShape);
                return SHPLIB_NULLPTR;
            }
            if (i > 0 &&
                psShape->panPartStart[i] <= psShape->panPartStart[i - 1])
            {
                char szErrorMsg[160];
                snprintf(szErrorMsg, sizeof(szErrorMsg),
                         "Corrupted .shp file : shape %d : panPartStart[%d] = "
                         "%d, panPartStart[%d] = %d",
                         hEntity, i, psShape->panPartStart[i], i - 1,
                         psShape->panPartStart[i - 1]);
                szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
                psSHP->sHooks.Error(szErrorMsg);
                SHPDestroyObject(psShape);
                return SHPLIB_NULLPTR;
            }
        }

        int nOffset = 44 + 8 + 4 * nParts;

        /* -------------------------------------------------------------------- */
        /*      If this is a multipatch, we will also have parts types.         */
        /* -------------------------------------------------------------------- */
        if (psShape->nSHPType == SHPT_MULTIPATCH)
        {
            memcpy(psShape->panPartType, psSHP->pabyRec + nOffset, 4 * nParts);
            for (int i = 0; STATIC_CAST(uint32_t, i) < nParts; i++)
            {
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAP32(psShape->panPartType + i);
#endif
            }

            nOffset += 4 * nParts;
        }

        /* -------------------------------------------------------------------- */
        /*      Copy out the vertices from the record.                          */
        /* -------------------------------------------------------------------- */
        for (int i = 0; STATIC_CAST(uint32_t, i) < nPoints; i++)
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAPDOUBLE_CPY(psShape->padfX + i,
                               psSHP->pabyRec + nOffset + i * 16);
            SHP_SWAPDOUBLE_CPY(psShape->padfY + i,
                               psSHP->pabyRec + nOffset + i * 16 + 8);
#else
            memcpy(psShape->padfX + i, psSHP->pabyRec + nOffset + i * 16, 8);
            memcpy(psShape->padfY + i, psSHP->pabyRec + nOffset + i * 16 + 8,
                   8);
#endif
        }

        nOffset += 16 * nPoints;

        /* -------------------------------------------------------------------- */
        /*      If we have a Z coordinate, collect that now.                    */
        /* -------------------------------------------------------------------- */
        if (psShape->nSHPType == SHPT_POLYGONZ ||
            psShape->nSHPType == SHPT_ARCZ ||
            psShape->nSHPType == SHPT_MULTIPATCH)
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAPDOUBLE_CPY(&psShape->dfZMin, psSHP->pabyRec + nOffset);
            SHP_SWAPDOUBLE_CPY(&psShape->dfZMax, psSHP->pabyRec + nOffset + 8);
#else
            memcpy(&psShape->dfZMin, psSHP->pabyRec + nOffset, 8);
            memcpy(&psShape->dfZMax, psSHP->pabyRec + nOffset + 8, 8);

#endif

            for (int i = 0; STATIC_CAST(uint32_t, i) < nPoints; i++)
            {
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAPDOUBLE_CPY(psShape->padfZ + i,
                                   psSHP->pabyRec + nOffset + 16 + i * 8);
#else
                memcpy(psShape->padfZ + i,
                       psSHP->pabyRec + nOffset + 16 + i * 8, 8);
#endif
            }

            nOffset += 16 + 8 * nPoints;
        }
        else if (psShape->bFastModeReadObject)
        {
            psShape->padfZ = SHPLIB_NULLPTR;
        }

        /* -------------------------------------------------------------------- */
        /*      If we have a M measure value, then read it now.  We assume      */
        /*      that the measure can be present for any shape if the size is    */
        /*      big enough, but really it will only occur for the Z shapes      */
        /*      (options), and the M shapes.                                    */
        /* -------------------------------------------------------------------- */
        if (nEntitySize >= STATIC_CAST(int, nOffset + 16 + 8 * nPoints))
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAPDOUBLE_CPY(&psShape->dfMMin, psSHP->pabyRec + nOffset);
            SHP_SWAPDOUBLE_CPY(&psShape->dfMMax, psSHP->pabyRec + nOffset + 8);
#else
            memcpy(&psShape->dfMMin, psSHP->pabyRec + nOffset, 8);
            memcpy(&psShape->dfMMax, psSHP->pabyRec + nOffset + 8, 8);
#endif

            for (int i = 0; STATIC_CAST(uint32_t, i) < nPoints; i++)
            {
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAPDOUBLE_CPY(psShape->padfM + i,
                                   psSHP->pabyRec + nOffset + 16 + i * 8);
#else
                memcpy(psShape->padfM + i,
                       psSHP->pabyRec + nOffset + 16 + i * 8, 8);
#endif
            }
            psShape->bMeasureIsUsed = TRUE;
        }
        else if (psShape->bFastModeReadObject)
        {
            psShape->padfM = SHPLIB_NULLPTR;
        }
    }

    /* ==================================================================== */
    /*  Extract vertices for a MultiPoint.                                  */
    /* ==================================================================== */
    else if (psShape->nSHPType == SHPT_MULTIPOINT ||
             psShape->nSHPType == SHPT_MULTIPOINTM ||
             psShape->nSHPType == SHPT_MULTIPOINTZ)
    {
        if (44 + 4 > nEntitySize)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Corrupted .shp file : shape %d : nEntitySize = %d",
                     hEntity, nEntitySize);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            SHPDestroyObject(psShape);
            return SHPLIB_NULLPTR;
        }
        uint32_t nPoints;
        memcpy(&nPoints, psSHP->pabyRec + 44, 4);

#if defined(SHP_BIG_ENDIAN)
        SHP_SWAP32(&nPoints);
#endif

        /* nPoints is unsigned */
        if (/* nPoints < 0 || */ nPoints > 50 * 1000 * 1000)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Corrupted .shp file : shape %d : nPoints = %u", hEntity,
                     nPoints);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            SHPDestroyObject(psShape);
            return SHPLIB_NULLPTR;
        }

        int nRequiredSize = 48 + nPoints * 16;
        if (psShape->nSHPType == SHPT_MULTIPOINTZ)
        {
            nRequiredSize += 16 + nPoints * 8;
        }
        if (nRequiredSize > nEntitySize)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Corrupted .shp file : shape %d : nPoints = %u, "
                     "nEntitySize = %d",
                     hEntity, nPoints, nEntitySize);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            SHPDestroyObject(psShape);
            return SHPLIB_NULLPTR;
        }

        unsigned char *pBuffer = SHPLIB_NULLPTR;
        unsigned char **ppBuffer = SHPLIB_NULLPTR;

        if (psShape->bFastModeReadObject)
        {
            const int nObjectBufSize = 4 * sizeof(double) * nPoints;
            pBuffer = SHPReallocObjectBufIfNecessary(psSHP, nObjectBufSize);
            ppBuffer = &pBuffer;
        }

        psShape->nVertices = nPoints;

        psShape->padfX = STATIC_CAST(
            double *, SHPAllocBuffer(ppBuffer, sizeof(double) * nPoints));
        psShape->padfY = STATIC_CAST(
            double *, SHPAllocBuffer(ppBuffer, sizeof(double) * nPoints));
        psShape->padfZ = STATIC_CAST(
            double *, SHPAllocBuffer(ppBuffer, sizeof(double) * nPoints));
        psShape->padfM = STATIC_CAST(
            double *, SHPAllocBuffer(ppBuffer, sizeof(double) * nPoints));

        if (psShape->padfX == SHPLIB_NULLPTR ||
            psShape->padfY == SHPLIB_NULLPTR ||
            psShape->padfZ == SHPLIB_NULLPTR ||
            psShape->padfM == SHPLIB_NULLPTR)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Not enough memory to allocate requested memory "
                     "(nPoints=%u) for shape %d. "
                     "Probably broken SHP file",
                     nPoints, hEntity);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            SHPDestroyObject(psShape);
            return SHPLIB_NULLPTR;
        }

        for (int i = 0; STATIC_CAST(uint32_t, i) < nPoints; i++)
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAPDOUBLE_CPY(psShape->padfX + i,
                               psSHP->pabyRec + 48 + 16 * i);
            SHP_SWAPDOUBLE_CPY(psShape->padfY + i,
                               psSHP->pabyRec + 48 + 16 * i + 8);
#else
            memcpy(psShape->padfX + i, psSHP->pabyRec + 48 + 16 * i, 8);
            memcpy(psShape->padfY + i, psSHP->pabyRec + 48 + 16 * i + 8, 8);
#endif
        }

        int nOffset = 48 + 16 * nPoints;

        /* -------------------------------------------------------------------- */
        /*      Get the X/Y bounds.                                             */
        /* -------------------------------------------------------------------- */
#if defined(SHP_BIG_ENDIAN)
        SHP_SWAPDOUBLE_CPY(&psShape->dfXMin, psSHP->pabyRec + 8 + 4);
        SHP_SWAPDOUBLE_CPY(&psShape->dfYMin, psSHP->pabyRec + 8 + 12);
        SHP_SWAPDOUBLE_CPY(&psShape->dfXMax, psSHP->pabyRec + 8 + 20);
        SHP_SWAPDOUBLE_CPY(&psShape->dfYMax, psSHP->pabyRec + 8 + 28);
#else
        memcpy(&psShape->dfXMin, psSHP->pabyRec + 8 + 4, 8);
        memcpy(&psShape->dfYMin, psSHP->pabyRec + 8 + 12, 8);
        memcpy(&psShape->dfXMax, psSHP->pabyRec + 8 + 20, 8);
        memcpy(&psShape->dfYMax, psSHP->pabyRec + 8 + 28, 8);
#endif

        /* -------------------------------------------------------------------- */
        /*      If we have a Z coordinate, collect that now.                    */
        /* -------------------------------------------------------------------- */
        if (psShape->nSHPType == SHPT_MULTIPOINTZ)
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAPDOUBLE_CPY(&psShape->dfZMin, psSHP->pabyRec + nOffset);
            SHP_SWAPDOUBLE_CPY(&psShape->dfZMax, psSHP->pabyRec + nOffset + 8);
#else
            memcpy(&psShape->dfZMin, psSHP->pabyRec + nOffset, 8);
            memcpy(&psShape->dfZMax, psSHP->pabyRec + nOffset + 8, 8);
#endif

            for (int i = 0; STATIC_CAST(uint32_t, i) < nPoints; i++)
            {
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAPDOUBLE_CPY(psShape->padfZ + i,
                                   psSHP->pabyRec + nOffset + 16 + i * 8);
#else
                memcpy(psShape->padfZ + i,
                       psSHP->pabyRec + nOffset + 16 + i * 8, 8);
#endif
            }

            nOffset += 16 + 8 * nPoints;
        }
        else if (psShape->bFastModeReadObject)
            psShape->padfZ = SHPLIB_NULLPTR;

        /* -------------------------------------------------------------------- */
        /*      If we have a M measure value, then read it now.  We assume      */
        /*      that the measure can be present for any shape if the size is    */
        /*      big enough, but really it will only occur for the Z shapes      */
        /*      (options), and the M shapes.                                    */
        /* -------------------------------------------------------------------- */
        if (nEntitySize >= STATIC_CAST(int, nOffset + 16 + 8 * nPoints))
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAPDOUBLE_CPY(&psShape->dfMMin, psSHP->pabyRec + nOffset);
            SHP_SWAPDOUBLE_CPY(&psShape->dfMMax, psSHP->pabyRec + nOffset + 8);
#else
            memcpy(&psShape->dfMMin, psSHP->pabyRec + nOffset, 8);
            memcpy(&psShape->dfMMax, psSHP->pabyRec + nOffset + 8, 8);
#endif

            for (int i = 0; STATIC_CAST(uint32_t, i) < nPoints; i++)
            {
#if defined(SHP_BIG_ENDIAN)
                SHP_SWAPDOUBLE_CPY(psShape->padfM + i,
                                   psSHP->pabyRec + nOffset + 16 + i * 8);
#else
                memcpy(psShape->padfM + i,
                       psSHP->pabyRec + nOffset + 16 + i * 8, 8);
#endif
            }
            psShape->bMeasureIsUsed = TRUE;
        }
        else if (psShape->bFastModeReadObject)
            psShape->padfM = SHPLIB_NULLPTR;
    }

    /* ==================================================================== */
    /*      Extract vertices for a point.                                   */
    /* ==================================================================== */
    else if (psShape->nSHPType == SHPT_POINT ||
             psShape->nSHPType == SHPT_POINTM ||
             psShape->nSHPType == SHPT_POINTZ)
    {
        psShape->nVertices = 1;
        if (psShape->bFastModeReadObject)
        {
            psShape->padfX = &(psShape->dfXMin);
            psShape->padfY = &(psShape->dfYMin);
            psShape->padfZ = &(psShape->dfZMin);
            psShape->padfM = &(psShape->dfMMin);
            psShape->padfZ[0] = 0.0;
            psShape->padfM[0] = 0.0;
        }
        else
        {
            psShape->padfX = STATIC_CAST(double *, calloc(1, sizeof(double)));
            psShape->padfY = STATIC_CAST(double *, calloc(1, sizeof(double)));
            psShape->padfZ = STATIC_CAST(double *, calloc(1, sizeof(double)));
            psShape->padfM = STATIC_CAST(double *, calloc(1, sizeof(double)));
        }

        if (20 + 8 + ((psShape->nSHPType == SHPT_POINTZ) ? 8 : 0) > nEntitySize)
        {
            char szErrorMsg[160];
            snprintf(szErrorMsg, sizeof(szErrorMsg),
                     "Corrupted .shp file : shape %d : nEntitySize = %d",
                     hEntity, nEntitySize);
            szErrorMsg[sizeof(szErrorMsg) - 1] = '\0';
            psSHP->sHooks.Error(szErrorMsg);
            SHPDestroyObject(psShape);
            return SHPLIB_NULLPTR;
        }
#if defined(SHP_BIG_ENDIAN)
        SHP_SWAPDOUBLE_CPY(psShape->padfX, psSHP->pabyRec + 12);
        SHP_SWAPDOUBLE_CPY(psShape->padfY, psSHP->pabyRec + 20);
#else
        memcpy(psShape->padfX, psSHP->pabyRec + 12, 8);
        memcpy(psShape->padfY, psSHP->pabyRec + 20, 8);
#endif

        int nOffset = 20 + 8;

        /* -------------------------------------------------------------------- */
        /*      If we have a Z coordinate, collect that now.                    */
        /* -------------------------------------------------------------------- */
        if (psShape->nSHPType == SHPT_POINTZ)
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAPDOUBLE_CPY(psShape->padfZ, psSHP->pabyRec + nOffset);
#else
            memcpy(psShape->padfZ, psSHP->pabyRec + nOffset, 8);
#endif

            nOffset += 8;
        }

        /* -------------------------------------------------------------------- */
        /*      If we have a M measure value, then read it now.  We assume      */
        /*      that the measure can be present for any shape if the size is    */
        /*      big enough, but really it will only occur for the Z shapes      */
        /*      (options), and the M shapes.                                    */
        /* -------------------------------------------------------------------- */
        if (nEntitySize >= nOffset + 8)
        {
#if defined(SHP_BIG_ENDIAN)
            SHP_SWAPDOUBLE_CPY(psShape->padfM, psSHP->pabyRec + nOffset);
#else
            memcpy(psShape->padfM, psSHP->pabyRec + nOffset, 8);
#endif
            psShape->bMeasureIsUsed = TRUE;
        }

        /* -------------------------------------------------------------------- */
        /*      Since no extents are supplied in the record, we will apply      */
        /*      them from the single vertex.                                    */
        /* -------------------------------------------------------------------- */
        psShape->dfXMin = psShape->dfXMax = psShape->padfX[0];
        psShape->dfYMin = psShape->dfYMax = psShape->padfY[0];
        psShape->dfZMin = psShape->dfZMax = psShape->padfZ[0];
        psShape->dfMMin = psShape->dfMMax = psShape->padfM[0];
    }

    return (psShape);
}

/************************************************************************/
/*                            SHPTypeName()                             */
/************************************************************************/

const char SHPAPI_CALL1(*) SHPTypeName(int nSHPType)
{
    switch (nSHPType)
    {
        case SHPT_NULL:
            return "NullShape";

        case SHPT_POINT:
            return "Point";

        case SHPT_ARC:
            return "Arc";

        case SHPT_POLYGON:
            return "Polygon";

        case SHPT_MULTIPOINT:
            return "MultiPoint";

        case SHPT_POINTZ:
            return "PointZ";

        case SHPT_ARCZ:
            return "ArcZ";

        case SHPT_POLYGONZ:
            return "PolygonZ";

        case SHPT_MULTIPOINTZ:
            return "MultiPointZ";

        case SHPT_POINTM:
            return "PointM";

        case SHPT_ARCM:
            return "ArcM";

        case SHPT_POLYGONM:
            return "PolygonM";

        case SHPT_MULTIPOINTM:
            return "MultiPointM";

        case SHPT_MULTIPATCH:
            return "MultiPatch";

        default:
            return "UnknownShapeType";
    }
}

/************************************************************************/
/*                          SHPPartTypeName()                           */
/************************************************************************/

const char SHPAPI_CALL1(*) SHPPartTypeName(int nPartType)
{
    switch (nPartType)
    {
        case SHPP_TRISTRIP:
            return "TriangleStrip";

        case SHPP_TRIFAN:
            return "TriangleFan";

        case SHPP_OUTERRING:
            return "OuterRing";

        case SHPP_INNERRING:
            return "InnerRing";

        case SHPP_FIRSTRING:
            return "FirstRing";

        case SHPP_RING:
            return "Ring";

        default:
            return "UnknownPartType";
    }
}

/************************************************************************/
/*                          SHPDestroyObject()                          */
/************************************************************************/

void SHPAPI_CALL SHPDestroyObject(SHPObject *psShape)
{
    if (psShape == SHPLIB_NULLPTR)
        return;

    if (psShape->bFastModeReadObject)
    {
        psShape->bFastModeReadObject = FALSE;
        return;
    }

    if (psShape->padfX != SHPLIB_NULLPTR)
        free(psShape->padfX);
    if (psShape->padfY != SHPLIB_NULLPTR)
        free(psShape->padfY);
    if (psShape->padfZ != SHPLIB_NULLPTR)
        free(psShape->padfZ);
    if (psShape->padfM != SHPLIB_NULLPTR)
        free(psShape->padfM);

    if (psShape->panPartStart != SHPLIB_NULLPTR)
        free(psShape->panPartStart);
    if (psShape->panPartType != SHPLIB_NULLPTR)
        free(psShape->panPartType);

    free(psShape);
}

/************************************************************************/
/*                       SHPGetPartVertexCount()                        */
/************************************************************************/

static int SHPGetPartVertexCount(const SHPObject *psObject, int iPart)
{
    if (iPart == psObject->nParts - 1)
        return psObject->nVertices - psObject->panPartStart[iPart];
    else
        return psObject->panPartStart[iPart + 1] -
               psObject->panPartStart[iPart];
}

/************************************************************************/
/*                      SHPRewindIsInnerRing()                          */
/************************************************************************/

/* Return -1 in case of ambiguity */
static int SHPRewindIsInnerRing(const SHPObject *psObject, int iOpRing,
                                double dfTestX, double dfTestY,
                                double dfRelativeTolerance, int bSameZ,
                                double dfTestZ)
{
    /* -------------------------------------------------------------------- */
    /*      Determine if this ring is an inner ring or an outer ring        */
    /*      relative to all the other rings.  For now we assume the         */
    /*      first ring is outer and all others are inner, but eventually    */
    /*      we need to fix this to handle multiple island polygons and      */
    /*      unordered sets of rings.                                        */
    /*                                                                      */
    /* -------------------------------------------------------------------- */

    bool bInner = false;
    for (int iCheckRing = 0; iCheckRing < psObject->nParts; iCheckRing++)
    {
        if (iCheckRing == iOpRing)
            continue;

        const int nVertStartCheck = psObject->panPartStart[iCheckRing];
        const int nVertCountCheck = SHPGetPartVertexCount(psObject, iCheckRing);

        /* Ignore rings that don't have the same (constant) Z value as the
         * point. */
        /* As noted in SHPRewindObject(), this is a simplification */
        /* of what we should ideally do. */
        if (!bSameZ)
        {
            int bZTestOK = TRUE;
            for (int iVert = nVertStartCheck + 1;
                 iVert < nVertStartCheck + nVertCountCheck; ++iVert)
            {
                if (psObject->padfZ[iVert] != dfTestZ)
                {
                    bZTestOK = FALSE;
                    break;
                }
            }
            if (!bZTestOK)
                continue;
        }

        for (int iEdge = 0; iEdge < nVertCountCheck; iEdge++)
        {
            int iNext;
            if (iEdge < nVertCountCheck - 1)
                iNext = iEdge + 1;
            else
                iNext = 0;

            const double y0 = psObject->padfY[iEdge + nVertStartCheck];
            const double y1 = psObject->padfY[iNext + nVertStartCheck];
            /* Rule #1:
             * Test whether the edge 'straddles' the horizontal ray from
             * the test point (dfTestY,dfTestY)
             * The rule #1 also excludes edges colinear with the ray.
             */
            if ((y0 < dfTestY && dfTestY <= y1) ||
                (y1 < dfTestY && dfTestY <= y0))
            {
                /* Rule #2:
                 * Test if edge-ray intersection is on the right from the
                 * test point (dfTestY,dfTestY)
                 */
                const double x0 = psObject->padfX[iEdge + nVertStartCheck];
                const double x1 = psObject->padfX[iNext + nVertStartCheck];
                const double intersect_minus_testX =
                    (x0 - dfTestX) + (dfTestY - y0) / (y1 - y0) * (x1 - x0);

                if (fabs(intersect_minus_testX) <=
                    dfRelativeTolerance * fabs(dfTestX))
                {
                    /* Potential shared edge, or slightly overlapping polygons
                     */
                    return -1;
                }
                else if (intersect_minus_testX < 0)
                {
                    bInner = !bInner;
                }
            }
        }
    } /* for iCheckRing */
    return bInner;
}

/************************************************************************/
/*                          SHPRewindObject()                           */
/*                                                                      */
/*      Reset the winding of polygon objects to adhere to the           */
/*      specification.                                                  */
/************************************************************************/

int SHPAPI_CALL SHPRewindObject(const SHPHandle hSHP, SHPObject *psObject)
{
    (void)hSHP;
    /* -------------------------------------------------------------------- */
    /*      Do nothing if this is not a polygon object.                     */
    /* -------------------------------------------------------------------- */
    if (psObject->nSHPType != SHPT_POLYGON &&
        psObject->nSHPType != SHPT_POLYGONZ &&
        psObject->nSHPType != SHPT_POLYGONM)
        return 0;

    if (psObject->nVertices == 0 || psObject->nParts == 0)
        return 0;

    /* -------------------------------------------------------------------- */
    /*      Test if all points have the same Z value.                       */
    /* -------------------------------------------------------------------- */
    int bSameZ = TRUE;
    if (psObject->nSHPType == SHPT_POLYGONZ ||
        psObject->nSHPType == SHPT_POLYGONM)
    {
        for (int iVert = 1; iVert < psObject->nVertices; ++iVert)
        {
            if (psObject->padfZ[iVert] != psObject->padfZ[0])
            {
                bSameZ = FALSE;
                break;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Process each of the rings.                                      */
    /* -------------------------------------------------------------------- */
    int bAltered = 0;
    for (int iOpRing = 0; iOpRing < psObject->nParts; iOpRing++)
    {
        const int nVertStart = psObject->panPartStart[iOpRing];
        const int nVertCount = SHPGetPartVertexCount(psObject, iOpRing);

        if (nVertCount < 2)
            continue;

        /* If a ring has a non-constant Z value, then consider it as an outer */
        /* ring. */
        /* NOTE: this is a rough approximation. If we were smarter, */
        /* we would check that all points of the ring are coplanar, and compare
         */
        /* that to other rings in the same (oblique) plane. */
        int bDoIsInnerRingTest = TRUE;
        if (!bSameZ)
        {
            int bPartSameZ = TRUE;
            for (int iVert = nVertStart + 1; iVert < nVertStart + nVertCount;
                 ++iVert)
            {
                if (psObject->padfZ[iVert] != psObject->padfZ[nVertStart])
                {
                    bPartSameZ = FALSE;
                    break;
                }
            }
            if (!bPartSameZ)
                bDoIsInnerRingTest = FALSE;
        }

        int bInner = FALSE;
        if (bDoIsInnerRingTest)
        {
            for (int iTolerance = 0; iTolerance < 2; iTolerance++)
            {
                /* In a first attempt, use a relaxed criterion to decide if a
                 * point */
                /* is inside another ring. If all points of the current ring are
                 * in the */
                /* "grey" zone w.r.t that criterion, which seems really
                 * unlikely, */
                /* then use the strict criterion for another pass. */
                const double dfRelativeTolerance = (iTolerance == 0) ? 1e-9 : 0;
                for (int iVert = nVertStart;
                     iVert + 1 < nVertStart + nVertCount; ++iVert)
                {
                    /* Use point in the middle of segment to avoid testing
                     * common points of rings.
                     */
                    const double dfTestX =
                        (psObject->padfX[iVert] + psObject->padfX[iVert + 1]) /
                        2;
                    const double dfTestY =
                        (psObject->padfY[iVert] + psObject->padfY[iVert + 1]) /
                        2;
                    const double dfTestZ =
                        !bSameZ ? psObject->padfZ[nVertStart] : 0;

                    bInner = SHPRewindIsInnerRing(psObject, iOpRing, dfTestX,
                                                  dfTestY, dfRelativeTolerance,
                                                  bSameZ, dfTestZ);
                    if (bInner >= 0)
                        break;
                }
                if (bInner >= 0)
                    break;
            }
            if (bInner < 0)
            {
                /* Completely degenerate case. Do not bother touching order. */
                continue;
            }
        }

        /* -------------------------------------------------------------------- */
        /*      Determine the current order of this ring so we will know if     */
        /*      it has to be reversed.                                          */
        /* -------------------------------------------------------------------- */

        double dfSum = psObject->padfX[nVertStart] *
                       (psObject->padfY[nVertStart + 1] -
                        psObject->padfY[nVertStart + nVertCount - 1]);
        int iVert = nVertStart + 1;
        for (; iVert < nVertStart + nVertCount - 1; iVert++)
        {
            dfSum += psObject->padfX[iVert] *
                     (psObject->padfY[iVert + 1] - psObject->padfY[iVert - 1]);
        }

        dfSum += psObject->padfX[iVert] *
                 (psObject->padfY[nVertStart] - psObject->padfY[iVert - 1]);

        /* -------------------------------------------------------------------- */
        /*      Reverse if necessary.                                           */
        /* -------------------------------------------------------------------- */
        if ((dfSum < 0.0 && bInner) || (dfSum > 0.0 && !bInner))
        {
            bAltered++;
            for (int i = 0; i < nVertCount / 2; i++)
            {
                /* Swap X */
                double dfSaved = psObject->padfX[nVertStart + i];
                psObject->padfX[nVertStart + i] =
                    psObject->padfX[nVertStart + nVertCount - i - 1];
                psObject->padfX[nVertStart + nVertCount - i - 1] = dfSaved;

                /* Swap Y */
                dfSaved = psObject->padfY[nVertStart + i];
                psObject->padfY[nVertStart + i] =
                    psObject->padfY[nVertStart + nVertCount - i - 1];
                psObject->padfY[nVertStart + nVertCount - i - 1] = dfSaved;

                /* Swap Z */
                if (psObject->padfZ)
                {
                    dfSaved = psObject->padfZ[nVertStart + i];
                    psObject->padfZ[nVertStart + i] =
                        psObject->padfZ[nVertStart + nVertCount - i - 1];
                    psObject->padfZ[nVertStart + nVertCount - i - 1] = dfSaved;
                }

                /* Swap M */
                if (psObject->padfM)
                {
                    dfSaved = psObject->padfM[nVertStart + i];
                    psObject->padfM[nVertStart + i] =
                        psObject->padfM[nVertStart + nVertCount - i - 1];
                    psObject->padfM[nVertStart + nVertCount - i - 1] = dfSaved;
                }
            }
        }
    }

    return bAltered;
}
