/******************************************************************************
 *
 * Project:  Shapelib
 * Purpose:  Implementation of search in ESRI SBN spatial index.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
 ******************************************************************************/

#include "shapefil_private.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define CACHED_DEPTH_LIMIT 8

#define READ_MSB_INT(ptr)                                                      \
    STATIC_CAST(int, (((STATIC_CAST(unsigned, (ptr)[0])) << 24) |              \
                      ((ptr)[1] << 16) | ((ptr)[2] << 8) | (ptr)[3]))

typedef int coord;

/*typedef unsigned char coord;*/

typedef struct
{
    unsigned char
        *pabyShapeDesc; /* Cache of (nShapeCount * 8) bytes of the bins. May
                             be NULL. */
    int nBinStart;      /* Index of first bin for this node. */
    int nShapeCount;    /* Number of shapes attached to this node. */
    int nBinCount;  /* Number of bins for this node. May be 0 if node is empty.
                     */
    int nBinOffset; /* Offset in file of the start of the first bin. May be 0 if
                       node is empty. */

    bool bBBoxInit; /* true if the following bounding box has been computed. */
    coord
        bMinX; /* Bounding box of the shapes directly attached to this node. */
    coord bMinY; /* This is *not* the theoretical footprint of the node. */
    coord bMaxX;
    coord bMaxY;
} SBNNodeDescriptor;

struct SBNSearchInfo
{
    SAHooks sHooks;
    SAFile fpSBN;
    SBNNodeDescriptor *pasNodeDescriptor;
    int nShapeCount; /* Total number of shapes */
    int nMaxDepth;   /* Tree depth */
    double dfMinX;   /* Bounding box of all shapes */
    double dfMaxX;
    double dfMinY;
    double dfMaxY;

#ifdef DEBUG_IO
    int nTotalBytesRead;
#endif
};

typedef struct
{
    SBNSearchHandle hSBN;

    coord bMinX; /* Search bounding box */
    coord bMinY;
    coord bMaxX;
    coord bMaxY;

    int nShapeCount;
    int nShapeAlloc;
    int *panShapeId; /* 0 based */

    unsigned char abyBinShape[8 * 100];

#ifdef DEBUG_IO
    int nBytesRead;
#endif
} SearchStruct;

/* Associates a node id with the index of its first bin */
typedef struct
{
    int nNodeId;
    int nBinStart;
} SBNNodeIdBinStartPair;

/************************************************************************/
/*                     SBNCompareNodeIdBinStartPairs()                  */
/************************************************************************/

/* helper for qsort, to sort SBNNodeIdBinStartPair by increasing nBinStart */
static int SBNCompareNodeIdBinStartPairs(const void *a, const void *b)
{
    return STATIC_CAST(const SBNNodeIdBinStartPair *, a)->nBinStart -
           STATIC_CAST(const SBNNodeIdBinStartPair *, b)->nBinStart;
}

/************************************************************************/
/*                         SBNOpenDiskTree()                            */
/************************************************************************/

SBNSearchHandle SBNOpenDiskTree(const char *pszSBNFilename,
                                const SAHooks *psHooks)
{
    /* -------------------------------------------------------------------- */
    /*      Initialize the handle structure.                                */
    /* -------------------------------------------------------------------- */
    SBNSearchHandle hSBN =
        STATIC_CAST(SBNSearchHandle, calloc(1, sizeof(struct SBNSearchInfo)));

    if (psHooks == SHPLIB_NULLPTR)
        SASetupDefaultHooks(&(hSBN->sHooks));
    else
        memcpy(&(hSBN->sHooks), psHooks, sizeof(SAHooks));

    hSBN->fpSBN =
        hSBN->sHooks.FOpen(pszSBNFilename, "rb", hSBN->sHooks.pvUserData);
    if (hSBN->fpSBN == SHPLIB_NULLPTR)
    {
        free(hSBN);
        return SHPLIB_NULLPTR;
    }

    /* -------------------------------------------------------------------- */
    /*      Check file header signature.                                    */
    /* -------------------------------------------------------------------- */
    unsigned char abyHeader[108];
    if (hSBN->sHooks.FRead(abyHeader, 108, 1, hSBN->fpSBN) != 1 ||
        abyHeader[0] != 0 || abyHeader[1] != 0 || abyHeader[2] != 0x27 ||
        (abyHeader[3] != 0x0A && abyHeader[3] != 0x0D) ||
        abyHeader[4] != 0xFF || abyHeader[5] != 0xFF || abyHeader[6] != 0xFE ||
        abyHeader[7] != 0x70)
    {
        hSBN->sHooks.Error(".sbn file is unreadable, or corrupt.");
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    /* -------------------------------------------------------------------- */
    /*      Read shapes bounding box.                                       */
    /* -------------------------------------------------------------------- */

#if !defined(SHP_BIG_ENDIAN)
    SHP_SWAPDOUBLE_CPY(&hSBN->dfMinX, abyHeader + 32);
    SHP_SWAPDOUBLE_CPY(&hSBN->dfMinY, abyHeader + 40);
    SHP_SWAPDOUBLE_CPY(&hSBN->dfMaxX, abyHeader + 48);
    SHP_SWAPDOUBLE_CPY(&hSBN->dfMaxY, abyHeader + 56);
#else
    memcpy(&hSBN->dfMinX, abyHeader + 32, 8);
    memcpy(&hSBN->dfMinY, abyHeader + 40, 8);
    memcpy(&hSBN->dfMaxX, abyHeader + 48, 8);
    memcpy(&hSBN->dfMaxY, abyHeader + 56, 8);
#endif

    if (hSBN->dfMinX > hSBN->dfMaxX || hSBN->dfMinY > hSBN->dfMaxY)
    {
        hSBN->sHooks.Error("Invalid extent in .sbn file.");
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    /* -------------------------------------------------------------------- */
    /*      Read and check number of shapes.                                */
    /* -------------------------------------------------------------------- */
    const int nShapeCount = READ_MSB_INT(abyHeader + 28);
    hSBN->nShapeCount = nShapeCount;
    if (nShapeCount < 0 || nShapeCount > 256000000)
    {
        char szErrorMsg[64];
        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Invalid shape count in .sbn : %d", nShapeCount);
        hSBN->sHooks.Error(szErrorMsg);
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    /* Empty spatial index */
    if (nShapeCount == 0)
    {
        return hSBN;
    }

    /* -------------------------------------------------------------------- */
    /*      Compute tree depth.                                             */
    /*      It is computed such as in average there are not more than 8     */
    /*      shapes per node. With a minimum depth of 2, and a maximum of 24 */
    /* -------------------------------------------------------------------- */
    int nMaxDepth = 2;
    while (nMaxDepth < 24 && nShapeCount > ((1 << nMaxDepth) - 1) * 8)
        nMaxDepth++;
    hSBN->nMaxDepth = nMaxDepth;
    const int nMaxNodes = (1 << nMaxDepth) - 1;

    /* -------------------------------------------------------------------- */
    /*      Check that the first bin id is 1.                               */
    /* -------------------------------------------------------------------- */

    if (READ_MSB_INT(abyHeader + 100) != 1)
    {
        hSBN->sHooks.Error("Unexpected bin id");
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    /* -------------------------------------------------------------------- */
    /*      Read and check number of node descriptors to be read.           */
    /*      There are at most (2^nMaxDepth) - 1, but all are not necessary  */
    /*      described. Non described nodes are empty.                       */
    /* -------------------------------------------------------------------- */
    const int nNodeDescSize = READ_MSB_INT(abyHeader + 104); /* 16-bit words */

    /* each bin descriptor is made of 2 ints */
    const int nNodeDescCount = nNodeDescSize / 4;

    if ((nNodeDescSize % 4) != 0 || nNodeDescCount < 0 ||
        nNodeDescCount > nMaxNodes)
    {
        char szErrorMsg[64];
        snprintf(szErrorMsg, sizeof(szErrorMsg),
                 "Invalid node descriptor size in .sbn : %d",
                 nNodeDescSize * STATIC_CAST(int, sizeof(uint16_t)));
        hSBN->sHooks.Error(szErrorMsg);
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    const int nNodeDescSizeBytes = nNodeDescCount * 2 * 4;
    /* coverity[tainted_data] */
    unsigned char *pabyData =
        STATIC_CAST(unsigned char *, malloc(nNodeDescSizeBytes));
    SBNNodeDescriptor *pasNodeDescriptor = STATIC_CAST(
        SBNNodeDescriptor *, calloc(nMaxNodes, sizeof(SBNNodeDescriptor)));
    if (pabyData == SHPLIB_NULLPTR || pasNodeDescriptor == SHPLIB_NULLPTR)
    {
        free(pabyData);
        free(pasNodeDescriptor);
        hSBN->sHooks.Error("Out of memory error");
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    /* -------------------------------------------------------------------- */
    /*      Read node descriptors.                                          */
    /* -------------------------------------------------------------------- */
    if (hSBN->sHooks.FRead(pabyData, nNodeDescSizeBytes, 1, hSBN->fpSBN) != 1)
    {
        free(pabyData);
        free(pasNodeDescriptor);
        hSBN->sHooks.Error("Cannot read node descriptors");
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    hSBN->pasNodeDescriptor = pasNodeDescriptor;

    SBNNodeIdBinStartPair *pasNodeIdBinStartPairs =
        STATIC_CAST(SBNNodeIdBinStartPair *,
                    malloc(nNodeDescCount * sizeof(SBNNodeIdBinStartPair)));
    if (pasNodeIdBinStartPairs == SHPLIB_NULLPTR)
    {
        free(pabyData);
        hSBN->sHooks.Error("Out of memory error");
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

#ifdef ENABLE_SBN_SANITY_CHECKS
    int nShapeCountAcc = 0;
#endif
    int nEntriesInNodeIdBinStartPairs = 0;
    for (int i = 0; i < nNodeDescCount; i++)
    {
        /* -------------------------------------------------------------------- */
        /*      Each node descriptor contains the index of the first bin that   */
        /*      described it, and the number of shapes in this first bin and    */
        /*      the following bins (in the relevant case).                      */
        /* -------------------------------------------------------------------- */
        const int nBinStart = READ_MSB_INT(pabyData + 8 * i);
        const int nNodeShapeCount = READ_MSB_INT(pabyData + 8 * i + 4);
        pasNodeDescriptor[i].nBinStart = nBinStart > 0 ? nBinStart : 0;
        pasNodeDescriptor[i].nShapeCount = nNodeShapeCount;

#ifdef DEBUG_SBN
        fprintf(stderr, "node[%d], nBinStart=%d, nShapeCount=%d\n", i,
                nBinStart, nNodeShapeCount);
#endif

        if ((nBinStart > 0 && nNodeShapeCount == 0) || nNodeShapeCount < 0 ||
            nNodeShapeCount > nShapeCount)
        {
            hSBN->sHooks.Error("Inconsistent shape count in bin");
            free(pabyData);
            free(pasNodeIdBinStartPairs);
            SBNCloseDiskTree(hSBN);
            return SHPLIB_NULLPTR;
        }

#ifdef ENABLE_SBN_SANITY_CHECKS
        if (nShapeCountAcc > nShapeCount - nNodeShapeCount)
        {
            hSBN->sHooks.Error("Inconsistent shape count in bin");
            free(pabyData);
            free(pasNodeIdBinStartPairs);
            SBNCloseDiskTree(hSBN);
            return SHPLIB_NULLPTR;
        }
        nShapeCountAcc += nNodeShapeCount;
#endif

        if (nBinStart > 0)
        {
            pasNodeIdBinStartPairs[nEntriesInNodeIdBinStartPairs].nNodeId = i;
            pasNodeIdBinStartPairs[nEntriesInNodeIdBinStartPairs].nBinStart =
                nBinStart;
            ++nEntriesInNodeIdBinStartPairs;
        }
    }

    free(pabyData);
    /* pabyData = SHPLIB_NULLPTR; */

    if (nEntriesInNodeIdBinStartPairs == 0)
    {
        free(pasNodeIdBinStartPairs);
        hSBN->sHooks.Error("All nodes are empty");
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

#ifdef ENABLE_SBN_SANITY_CHECKS
    if (nShapeCountAcc != nShapeCount)
    {
        /* Not totally sure if the above condition is always true */
        /* Not enabled by default, as non-needed for the good working */
        /* of our code. */
        free(pasNodeIdBinStartPairs);
        char szMessage[128];
        snprintf(szMessage, sizeof(szMessage),
                 "Inconsistent shape count read in .sbn header (%d) vs total "
                 "of shapes over nodes (%d)",
                 nShapeCount, nShapeCountAcc);
        hSBN->sHooks.Error(szMessage);
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }
#endif

    /* Sort node descriptors by increasing nBinStart */
    /* In most cases, the node descriptors have already an increasing nBinStart,
     * but not for https://github.com/OSGeo/gdal/issues/9430 */
    qsort(pasNodeIdBinStartPairs, nEntriesInNodeIdBinStartPairs,
          sizeof(SBNNodeIdBinStartPair), SBNCompareNodeIdBinStartPairs);

    /* Consistency check: the first referenced nBinStart should be 2. */
    if (pasNodeIdBinStartPairs[0].nBinStart != 2)
    {
        char szMessage[128];
        snprintf(szMessage, sizeof(szMessage),
                 "First referenced bin (by node %d) should be 2, but %d found",
                 pasNodeIdBinStartPairs[0].nNodeId,
                 pasNodeIdBinStartPairs[0].nBinStart);
        hSBN->sHooks.Error(szMessage);
        SBNCloseDiskTree(hSBN);
        free(pasNodeIdBinStartPairs);
        return SHPLIB_NULLPTR;
    }

    /* And referenced nBinStart should be all distinct. */
    for (int i = 1; i < nEntriesInNodeIdBinStartPairs; ++i)
    {
        if (pasNodeIdBinStartPairs[i].nBinStart ==
            pasNodeIdBinStartPairs[i - 1].nBinStart)
        {
            char szMessage[128];
            snprintf(szMessage, sizeof(szMessage),
                     "Node %d and %d have the same nBinStart=%d",
                     pasNodeIdBinStartPairs[i - 1].nNodeId,
                     pasNodeIdBinStartPairs[i].nNodeId,
                     pasNodeIdBinStartPairs[i].nBinStart);
            hSBN->sHooks.Error(szMessage);
            SBNCloseDiskTree(hSBN);
            free(pasNodeIdBinStartPairs);
            return SHPLIB_NULLPTR;
        }
    }

    int nExpectedBinId = 1;
    int nIdxInNodeBinPair = 0;
    int nCurNode = pasNodeIdBinStartPairs[nIdxInNodeBinPair].nNodeId;

    pasNodeDescriptor[nCurNode].nBinOffset =
        STATIC_CAST(int, hSBN->sHooks.FTell(hSBN->fpSBN));

    /* -------------------------------------------------------------------- */
    /*      Traverse bins to compute the offset of the first bin of each    */
    /*      node.                                                           */
    /*      Note: we could use the .sbx file to compute the offsets instead.*/
    /* -------------------------------------------------------------------- */
    unsigned char abyBinHeader[8];

    while (hSBN->sHooks.FRead(abyBinHeader, 8, 1, hSBN->fpSBN) == 1)
    {
        nExpectedBinId++;

        const int nBinId = READ_MSB_INT(abyBinHeader);
        const int nBinSize = READ_MSB_INT(abyBinHeader + 4); /* 16-bit words */

#ifdef DEBUG_SBN
        fprintf(stderr, "bin id=%d, bin size (in features) = %d\n", nBinId,
                nBinSize / 4);
#endif

        if (nBinId != nExpectedBinId)
        {
            char szMessage[128];
            snprintf(szMessage, sizeof(szMessage),
                     "Unexpected bin id at bin starting at offset %d. Got %d, "
                     "expected %d",
                     STATIC_CAST(int, hSBN->sHooks.FTell(hSBN->fpSBN)) - 8,
                     nBinId, nExpectedBinId);
            hSBN->sHooks.Error(szMessage);
            SBNCloseDiskTree(hSBN);
            free(pasNodeIdBinStartPairs);
            return SHPLIB_NULLPTR;
        }

        /* Bins are always limited to 100 features */
        /* If there are more, then they are located in continuous bins */
        if ((nBinSize % 4) != 0 || nBinSize <= 0 || nBinSize > 100 * 4)
        {
            char szMessage[128];
            snprintf(szMessage, sizeof(szMessage),
                     "Unexpected bin size at bin starting at offset %d. Got %d",
                     STATIC_CAST(int, hSBN->sHooks.FTell(hSBN->fpSBN)) - 8,
                     nBinSize);
            hSBN->sHooks.Error(szMessage);
            SBNCloseDiskTree(hSBN);
            free(pasNodeIdBinStartPairs);
            return SHPLIB_NULLPTR;
        }

        if (nIdxInNodeBinPair + 1 < nEntriesInNodeIdBinStartPairs &&
            nBinId == pasNodeIdBinStartPairs[nIdxInNodeBinPair + 1].nBinStart)
        {
            ++nIdxInNodeBinPair;
            nCurNode = pasNodeIdBinStartPairs[nIdxInNodeBinPair].nNodeId;
            pasNodeDescriptor[nCurNode].nBinOffset =
                STATIC_CAST(int, hSBN->sHooks.FTell(hSBN->fpSBN)) - 8;
        }

        pasNodeDescriptor[nCurNode].nBinCount++;

        /* Skip shape description */
        hSBN->sHooks.FSeek(hSBN->fpSBN, nBinSize * sizeof(uint16_t), SEEK_CUR);
    }

    if (nIdxInNodeBinPair + 1 != nEntriesInNodeIdBinStartPairs)
    {
        hSBN->sHooks.Error("Could not determine nBinOffset / nBinCount for all "
                           "non-empty nodes.");
        SBNCloseDiskTree(hSBN);
        free(pasNodeIdBinStartPairs);
        return SHPLIB_NULLPTR;
    }

    free(pasNodeIdBinStartPairs);

    return hSBN;
}

/***********************************************************************/
/*                          SBNCloseDiskTree()                         */
/************************************************************************/

void SBNCloseDiskTree(SBNSearchHandle hSBN)
{
    if (hSBN == SHPLIB_NULLPTR)
        return;

    if (hSBN->pasNodeDescriptor != SHPLIB_NULLPTR)
    {
        const int nMaxNodes = (1 << hSBN->nMaxDepth) - 1;
        for (int i = 0; i < nMaxNodes; i++)
        {
            if (hSBN->pasNodeDescriptor[i].pabyShapeDesc != SHPLIB_NULLPTR)
                free(hSBN->pasNodeDescriptor[i].pabyShapeDesc);
        }
    }

    /* printf("hSBN->nTotalBytesRead = %d\n", hSBN->nTotalBytesRead); */

    hSBN->sHooks.FClose(hSBN->fpSBN);
    free(hSBN->pasNodeDescriptor);
    free(hSBN);
}

/************************************************************************/
/*                         SBNAddShapeId()                              */
/************************************************************************/

static bool SBNAddShapeId(SearchStruct *psSearch, int nShapeId)
{
    if (psSearch->nShapeCount == psSearch->nShapeAlloc)
    {
        psSearch->nShapeAlloc =
            STATIC_CAST(int, ((psSearch->nShapeCount + 100) * 5) / 4);
        int *pNewPtr =
            STATIC_CAST(int *, realloc(psSearch->panShapeId,
                                       psSearch->nShapeAlloc * sizeof(int)));
        if (pNewPtr == SHPLIB_NULLPTR)
        {
            psSearch->hSBN->sHooks.Error("Out of memory error");
            return false;
        }
        psSearch->panShapeId = pNewPtr;
    }

    psSearch->panShapeId[psSearch->nShapeCount] = nShapeId;
    psSearch->nShapeCount++;
    return true;
}

/************************************************************************/
/*                     SBNSearchDiskInternal()                          */
/************************************************************************/

/*      Due to the way integer coordinates are rounded,                 */
/*      we can use a strict intersection test, except when the node     */
/*      bounding box or the search bounding box is degenerated.         */
#define SEARCH_BB_INTERSECTS(_bMinX, _bMinY, _bMaxX, _bMaxY)                   \
    (((bSearchMinX < _bMaxX && bSearchMaxX > _bMinX) ||                        \
      ((_bMinX == _bMaxX || bSearchMinX == bSearchMaxX) &&                     \
       bSearchMinX <= _bMaxX && bSearchMaxX >= _bMinX)) &&                     \
     ((bSearchMinY < _bMaxY && bSearchMaxY > _bMinY) ||                        \
      ((_bMinY == _bMaxY || bSearchMinY == bSearchMaxY) &&                     \
       bSearchMinY <= _bMaxY && bSearchMaxY >= _bMinY)))

static bool SBNSearchDiskInternal(SearchStruct *psSearch, int nDepth,
                                  int nNodeId, coord bNodeMinX, coord bNodeMinY,
                                  coord bNodeMaxX, coord bNodeMaxY)
{
    const coord bSearchMinX = psSearch->bMinX;
    const coord bSearchMinY = psSearch->bMinY;
    const coord bSearchMaxX = psSearch->bMaxX;
    const coord bSearchMaxY = psSearch->bMaxY;

    SBNSearchHandle hSBN = psSearch->hSBN;

    SBNNodeDescriptor *psNode = &(hSBN->pasNodeDescriptor[nNodeId]);

    /* -------------------------------------------------------------------- */
    /*      Check if this node contains shapes that intersect the search    */
    /*      bounding box.                                                   */
    /* -------------------------------------------------------------------- */
    if (psNode->bBBoxInit &&
        !(SEARCH_BB_INTERSECTS(psNode->bMinX, psNode->bMinY, psNode->bMaxX,
                               psNode->bMaxY)))

    {
        /* No intersection, then don't try to read the shapes attached */
        /* to this node */
    }

    /* -------------------------------------------------------------------- */
    /*      If this node contains shapes that are cached, then read them.   */
    /* -------------------------------------------------------------------- */
    else if (psNode->pabyShapeDesc != SHPLIB_NULLPTR)
    {
        unsigned char *pabyShapeDesc = psNode->pabyShapeDesc;

        /* printf("nNodeId = %d, nDepth = %d\n", nNodeId, nDepth); */

        for (int j = 0; j < psNode->nShapeCount; j++)
        {
            const coord bMinX = pabyShapeDesc[0];
            const coord bMinY = pabyShapeDesc[1];
            const coord bMaxX = pabyShapeDesc[2];
            const coord bMaxY = pabyShapeDesc[3];

            if (SEARCH_BB_INTERSECTS(bMinX, bMinY, bMaxX, bMaxY))
            {
                int nShapeId = READ_MSB_INT(pabyShapeDesc + 4);

                /* Caution : we count shape id starting from 0, and not 1 */
                nShapeId--;

                /*printf("shape=%d, minx=%d, miny=%d, maxx=%d, maxy=%d\n",
                       nShapeId, bMinX, bMinY, bMaxX, bMaxY);*/

                if (!SBNAddShapeId(psSearch, nShapeId))
                    return false;
            }

            pabyShapeDesc += 8;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      If the node has attached shapes (that are not (yet) cached),    */
    /*      then retrieve them from disk.                                   */
    /* -------------------------------------------------------------------- */

    else if (psNode->nBinCount > 0)
    {
        hSBN->sHooks.FSeek(hSBN->fpSBN, psNode->nBinOffset, SEEK_SET);

        if (nDepth < CACHED_DEPTH_LIMIT)
            psNode->pabyShapeDesc =
                STATIC_CAST(unsigned char *, malloc(psNode->nShapeCount * 8));

        unsigned char abyBinHeader[8];
        int nShapeCountAcc = 0;

        for (int i = 0; i < psNode->nBinCount; i++)
        {
#ifdef DEBUG_IO
            psSearch->nBytesRead += 8;
#endif
            if (hSBN->sHooks.FRead(abyBinHeader, 8, 1, hSBN->fpSBN) != 1)
            {
                hSBN->sHooks.Error("I/O error");
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                return false;
            }

            if (READ_MSB_INT(abyBinHeader + 0) != psNode->nBinStart + i)
            {
                hSBN->sHooks.Error("Unexpected bin id");
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                return false;
            }

            /* 16-bit words */
            const int nBinSize = READ_MSB_INT(abyBinHeader + 4);
            const int nShapes = nBinSize / 4;

            /* Bins are always limited to 100 features */
            if ((nBinSize % 4) != 0 || nShapes <= 0 || nShapes > 100)
            {
                hSBN->sHooks.Error("Unexpected bin size");
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                return false;
            }

            if (nShapeCountAcc + nShapes > psNode->nShapeCount)
            {
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                char szMessage[128];
                snprintf(
                    szMessage, sizeof(szMessage),
                    "Inconsistent shape count for bin idx=%d of node %d. "
                    "nShapeCountAcc=(%d) + nShapes=(%d) > nShapeCount(=%d)",
                    i, nNodeId, nShapeCountAcc, nShapes, psNode->nShapeCount);
                hSBN->sHooks.Error(szMessage);
                return false;
            }

            unsigned char *pabyBinShape;
            if (nDepth < CACHED_DEPTH_LIMIT &&
                psNode->pabyShapeDesc != SHPLIB_NULLPTR)
            {
                pabyBinShape = psNode->pabyShapeDesc + nShapeCountAcc * 8;
            }
            else
            {
                pabyBinShape = psSearch->abyBinShape;
            }

#ifdef DEBUG_IO
            psSearch->nBytesRead += nBinSize * sizeof(uint16_t);
#endif
            if (hSBN->sHooks.FRead(pabyBinShape, nBinSize * sizeof(uint16_t), 1,
                                   hSBN->fpSBN) != 1)
            {
                hSBN->sHooks.Error("I/O error");
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                return false;
            }

            nShapeCountAcc += nShapes;

            if (i == 0 && !psNode->bBBoxInit)
            {
                psNode->bMinX = pabyBinShape[0];
                psNode->bMinY = pabyBinShape[1];
                psNode->bMaxX = pabyBinShape[2];
                psNode->bMaxY = pabyBinShape[3];
            }

            for (int j = 0; j < nShapes; j++)
            {
                const coord bMinX = pabyBinShape[0];
                const coord bMinY = pabyBinShape[1];
                const coord bMaxX = pabyBinShape[2];
                const coord bMaxY = pabyBinShape[3];

                if (!psNode->bBBoxInit)
                {
                    /* clang-format off */
#ifdef ENABLE_SBN_SANITY_CHECKS
                    /* -------------------------------------------------------------------- */
                    /*      Those tests only check that the shape bounding box in the bin   */
                    /*      are consistent (self-consistent and consistent with the node    */
                    /*      they are attached to). They are optional however (as far as     */
                    /*      the safety of runtime is concerned at least).                   */
                    /* -------------------------------------------------------------------- */

                    if (!(((bMinX < bMaxX || (bMinX == 0 && bMaxX == 0) ||
                            (bMinX == 255 && bMaxX == 255))) &&
                          ((bMinY < bMaxY || (bMinY == 0 && bMaxY == 0) ||
                            (bMinY == 255 && bMaxY == 255)))) ||
                        bMaxX < bNodeMinX || bMaxY < bNodeMinY ||
                        bMinX > bNodeMaxX || bMinY > bNodeMaxY)
                    {
                        /* printf("shape %d %d %d %d\n", bMinX, bMinY, bMaxX, bMaxY);*/
                        /* printf("node  %d %d %d %d\n", bNodeMinX, bNodeMinY, bNodeMaxX, bNodeMaxY);*/
                        hSBN->sHooks.Error("Invalid shape bounding box in bin");
                        free(psNode->pabyShapeDesc);
                        psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                        return false;
                    }
#endif
                    /* clang-format on */
                    if (bMinX < psNode->bMinX)
                        psNode->bMinX = bMinX;
                    if (bMinY < psNode->bMinY)
                        psNode->bMinY = bMinY;
                    if (bMaxX > psNode->bMaxX)
                        psNode->bMaxX = bMaxX;
                    if (bMaxY > psNode->bMaxY)
                        psNode->bMaxY = bMaxY;
                }

                if (SEARCH_BB_INTERSECTS(bMinX, bMinY, bMaxX, bMaxY))
                {
                    int nShapeId = READ_MSB_INT(pabyBinShape + 4);

                    /* Caution : we count shape id starting from 0, and not 1 */
                    nShapeId--;

                    /*printf("shape=%d, minx=%d, miny=%d, maxx=%d, maxy=%d\n",
                        nShapeId, bMinX, bMinY, bMaxX, bMaxY);*/

                    if (!SBNAddShapeId(psSearch, nShapeId))
                        return false;
                }

                pabyBinShape += 8;
            }
        }

        if (nShapeCountAcc != psNode->nShapeCount)
        {
            free(psNode->pabyShapeDesc);
            psNode->pabyShapeDesc = SHPLIB_NULLPTR;
            char szMessage[96];
            snprintf(
                szMessage, sizeof(szMessage),
                "Inconsistent shape count for node %d. Got %d, expected %d",
                nNodeId, nShapeCountAcc, psNode->nShapeCount);
            hSBN->sHooks.Error(szMessage);
            return false;
        }

        psNode->bBBoxInit = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Look up in child nodes.                                         */
    /* -------------------------------------------------------------------- */
    if (nDepth + 1 < hSBN->nMaxDepth)
    {
        nNodeId = nNodeId * 2 + 1;

        if ((nDepth % 2) == 0) /* x split */
        {
            const coord bMid = STATIC_CAST(
                coord, 1 + (STATIC_CAST(int, bNodeMinX) + bNodeMaxX) / 2);
            if (bSearchMinX <= bMid - 1 &&
                !SBNSearchDiskInternal(psSearch, nDepth + 1, nNodeId + 1,
                                       bNodeMinX, bNodeMinY, bMid - 1,
                                       bNodeMaxY))
            {
                return false;
            }
            if (bSearchMaxX >= bMid &&
                !SBNSearchDiskInternal(psSearch, nDepth + 1, nNodeId, bMid,
                                       bNodeMinY, bNodeMaxX, bNodeMaxY))
            {
                return false;
            }
        }
        else /* y split */
        {
            const coord bMid = STATIC_CAST(
                coord, 1 + (STATIC_CAST(int, bNodeMinY) + bNodeMaxY) / 2);
            if (bSearchMinY <= bMid - 1 &&
                !SBNSearchDiskInternal(psSearch, nDepth + 1, nNodeId + 1,
                                       bNodeMinX, bNodeMinY, bNodeMaxX,
                                       bMid - 1))
            {
                return false;
            }
            if (bSearchMaxY >= bMid &&
                !SBNSearchDiskInternal(psSearch, nDepth + 1, nNodeId, bNodeMinX,
                                       bMid, bNodeMaxX, bNodeMaxY))
            {
                return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                          compare_ints()                              */
/************************************************************************/

/* helper for qsort */
static int compare_ints(const void *a, const void *b)
{
    return *STATIC_CAST(const int *, a) - *STATIC_CAST(const int *, b);
}

/************************************************************************/
/*                        SBNSearchDiskTree()                           */
/************************************************************************/

int *SBNSearchDiskTree(const SBNSearchHandle hSBN, const double *padfBoundsMin,
                       const double *padfBoundsMax, int *pnShapeCount)
{
    *pnShapeCount = 0;

    const double dfMinX = padfBoundsMin[0];
    const double dfMinY = padfBoundsMin[1];
    const double dfMaxX = padfBoundsMax[0];
    const double dfMaxY = padfBoundsMax[1];

    if (dfMinX > dfMaxX || dfMinY > dfMaxY)
        return SHPLIB_NULLPTR;

    if (dfMaxX < hSBN->dfMinX || dfMaxY < hSBN->dfMinY ||
        dfMinX > hSBN->dfMaxX || dfMinY > hSBN->dfMaxY)
        return SHPLIB_NULLPTR;

    /* -------------------------------------------------------------------- */
    /*      Compute the search coordinates in [0,255]x[0,255] coord. space  */
    /* -------------------------------------------------------------------- */
    const double dfDiskXExtent = hSBN->dfMaxX - hSBN->dfMinX;
    const double dfDiskYExtent = hSBN->dfMaxY - hSBN->dfMinY;

    int bMinX;
    int bMaxX;
    int bMinY;
    int bMaxY;
    if (dfDiskXExtent == 0.0)
    {
        bMinX = 0;
        bMaxX = 255;
    }
    else
    {
        if (dfMinX < hSBN->dfMinX)
            bMinX = 0;
        else
        {
            const double dfMinX_255 =
                (dfMinX - hSBN->dfMinX) / dfDiskXExtent * 255.0;
            bMinX = STATIC_CAST(int, floor(dfMinX_255 - 0.005));
            if (bMinX < 0)
                bMinX = 0;
        }

        if (dfMaxX > hSBN->dfMaxX)
            bMaxX = 255;
        else
        {
            const double dfMaxX_255 =
                (dfMaxX - hSBN->dfMinX) / dfDiskXExtent * 255.0;
            bMaxX = STATIC_CAST(int, ceil(dfMaxX_255 + 0.005));
            if (bMaxX > 255)
                bMaxX = 255;
        }
    }

    if (dfDiskYExtent == 0.0)
    {
        bMinY = 0;
        bMaxY = 255;
    }
    else
    {
        if (dfMinY < hSBN->dfMinY)
            bMinY = 0;
        else
        {
            const double dfMinY_255 =
                (dfMinY - hSBN->dfMinY) / dfDiskYExtent * 255.0;
            bMinY = STATIC_CAST(int, floor(dfMinY_255 - 0.005));
            if (bMinY < 0)
                bMinY = 0;
        }

        if (dfMaxY > hSBN->dfMaxY)
            bMaxY = 255;
        else
        {
            const double dfMaxY_255 =
                (dfMaxY - hSBN->dfMinY) / dfDiskYExtent * 255.0;
            bMaxY = STATIC_CAST(int, ceil(dfMaxY_255 + 0.005));
            if (bMaxY > 255)
                bMaxY = 255;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Run the search.                                                 */
    /* -------------------------------------------------------------------- */

    return SBNSearchDiskTreeInteger(hSBN, bMinX, bMinY, bMaxX, bMaxY,
                                    pnShapeCount);
}

/************************************************************************/
/*                     SBNSearchDiskTreeInteger()                       */
/************************************************************************/

int *SBNSearchDiskTreeInteger(const SBNSearchHandle hSBN, int bMinX, int bMinY,
                              int bMaxX, int bMaxY, int *pnShapeCount)
{
    *pnShapeCount = 0;

    if (bMinX > bMaxX || bMinY > bMaxY)
        return SHPLIB_NULLPTR;

    if (bMaxX < 0 || bMaxY < 0 || bMinX > 255 || bMinY > 255)
        return SHPLIB_NULLPTR;

    if (hSBN->nShapeCount == 0)
        return SHPLIB_NULLPTR;
    /* -------------------------------------------------------------------- */
    /*      Run the search.                                                 */
    /* -------------------------------------------------------------------- */
    SearchStruct sSearch;
    memset(&sSearch, 0, sizeof(sSearch));
    sSearch.hSBN = hSBN;
    sSearch.bMinX = STATIC_CAST(coord, bMinX >= 0 ? bMinX : 0);
    sSearch.bMinY = STATIC_CAST(coord, bMinY >= 0 ? bMinY : 0);
    sSearch.bMaxX = STATIC_CAST(coord, bMaxX <= 255 ? bMaxX : 255);
    sSearch.bMaxY = STATIC_CAST(coord, bMaxY <= 255 ? bMaxY : 255);
    sSearch.nShapeCount = 0;
    sSearch.nShapeAlloc = 0;
    sSearch.panShapeId = STATIC_CAST(int *, calloc(1, sizeof(int)));
#ifdef DEBUG_IO
    sSearch.nBytesRead = 0;
#endif

    const bool bRet = SBNSearchDiskInternal(&sSearch, 0, 0, 0, 0, 255, 255);

#ifdef DEBUG_IO
    hSBN->nTotalBytesRead += sSearch.nBytesRead;
    /* printf("nBytesRead = %d\n", sSearch.nBytesRead); */
#endif

    if (!bRet)
    {
        free(sSearch.panShapeId);
        *pnShapeCount = 0;
        return SHPLIB_NULLPTR;
    }

    *pnShapeCount = sSearch.nShapeCount;

    /* -------------------------------------------------------------------- */
    /*      Sort the id array                                               */
    /* -------------------------------------------------------------------- */
    qsort(sSearch.panShapeId, *pnShapeCount, sizeof(int), compare_ints);

    return sSearch.panShapeId;
}

/************************************************************************/
/*                         SBNSearchFreeIds()                           */
/************************************************************************/

void SBNSearchFreeIds(int *panShapeId)
{
    free(panShapeId);
}
