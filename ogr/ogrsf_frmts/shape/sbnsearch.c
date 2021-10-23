/******************************************************************************
 *
 * Project:  Shapelib
 * Purpose:  Implementation of search in ESRI SBN spatial index.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * This software is available under the following "MIT Style" license,
 * or at the option of the licensee under the LGPL (see COPYING).  This
 * option is discussed in more detail in shapelib.html.
 *
 * --
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
 ******************************************************************************/

#include "shapefil.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SHP_CVSID("$Id$")

#ifndef USE_CPL
#if defined(_MSC_VER)
# if _MSC_VER < 1900
#     define snprintf _snprintf
# endif
#elif defined(WIN32) || defined(_WIN32)
#  ifndef snprintf
#     define snprintf _snprintf
#  endif
#endif
#endif

#define CACHED_DEPTH_LIMIT      8

#ifdef __cplusplus
#define STATIC_CAST(type,x) static_cast<type>(x)
#define REINTERPRET_CAST(type,x) reinterpret_cast<type>(x)
#define CONST_CAST(type,x) const_cast<type>(x)
#define SHPLIB_NULLPTR nullptr
#else
#define STATIC_CAST(type,x) ((type)(x))
#define REINTERPRET_CAST(type,x) ((type)(x))
#define CONST_CAST(type,x) ((type)(x))
#define SHPLIB_NULLPTR NULL
#endif

#define READ_MSB_INT(ptr) \
        STATIC_CAST(int, (((STATIC_CAST(unsigned, (ptr)[0])) << 24) | ((ptr)[1] << 16) | ((ptr)[2] << 8) | (ptr)[3]))

typedef unsigned char uchar;

typedef int coord;
/*typedef uchar coord;*/

typedef struct
{
    uchar  *pabyShapeDesc;  /* Cache of (nShapeCount * 8) bytes of the bins. May be NULL. */
    int     nBinStart;      /* Index of first bin for this node. */
    int     nShapeCount;    /* Number of shapes attached to this node. */
    int     nBinCount;      /* Number of bins for this node. May be 0 if node is empty. */
    int     nBinOffset;     /* Offset in file of the start of the first bin. May be 0 if node is empty. */

    bool    bBBoxInit;      /* true if the following bounding box has been computed. */
    coord   bMinX;          /* Bounding box of the shapes directly attached to this node. */
    coord   bMinY;          /* This is *not* the theoretical footprint of the node. */
    coord   bMaxX;
    coord   bMaxY;
} SBNNodeDescriptor;

struct SBNSearchInfo
{
    SAHooks            sHooks;
    SAFile             fpSBN;
    SBNNodeDescriptor *pasNodeDescriptor;
    int                nShapeCount;         /* Total number of shapes */
    int                nMaxDepth;           /* Tree depth */
    double             dfMinX;              /* Bounding box of all shapes */
    double             dfMaxX;
    double             dfMinY;
    double             dfMaxY;

#ifdef DEBUG_IO
    int                nTotalBytesRead;
#endif
};

typedef struct
{
    SBNSearchHandle hSBN;

    coord               bMinX;    /* Search bounding box */
    coord               bMinY;
    coord               bMaxX;
    coord               bMaxY;

    int                 nShapeCount;
    int                 nShapeAlloc;
    int                *panShapeId; /* 0 based */

    uchar               abyBinShape[8 * 100];

#ifdef DEBUG_IO
    int                 nBytesRead;
#endif
} SearchStruct;

/************************************************************************/
/*                              SwapWord()                              */
/*                                                                      */
/*      Swap a 2, 4 or 8 byte word.                                     */
/************************************************************************/

static void SwapWord( int length, void * wordP ) {
    for( int i=0; i < length/2; i++ )
    {
        const uchar temp = STATIC_CAST(uchar*, wordP)[i];
        STATIC_CAST(uchar*, wordP)[i] = STATIC_CAST(uchar*, wordP)[length-i-1];
        STATIC_CAST(uchar*, wordP)[length-i-1] = temp;
    }
}

/************************************************************************/
/*                         SBNOpenDiskTree()                            */
/************************************************************************/

SBNSearchHandle SBNOpenDiskTree( const char* pszSBNFilename,
                                 SAHooks *psHooks ) {
/* -------------------------------------------------------------------- */
/*  Establish the byte order on this machine.                           */
/* -------------------------------------------------------------------- */
    bool bBigEndian;
    {
    int i = 1;
    if( *REINTERPRET_CAST(unsigned char *, &i) == 1 )
        bBigEndian = false;
    else
        bBigEndian = true;
    }

/* -------------------------------------------------------------------- */
/*      Initialize the handle structure.                                */
/* -------------------------------------------------------------------- */
    SBNSearchHandle hSBN =
        STATIC_CAST(SBNSearchHandle, calloc(sizeof(struct SBNSearchInfo), 1));

    if (psHooks == SHPLIB_NULLPTR)
        SASetupDefaultHooks( &(hSBN->sHooks) );
    else
        memcpy( &(hSBN->sHooks), psHooks, sizeof(SAHooks) );

    hSBN->fpSBN = hSBN->sHooks.FOpen(pszSBNFilename, "rb");
    if (hSBN->fpSBN == SHPLIB_NULLPTR)
    {
        free(hSBN);
        return SHPLIB_NULLPTR;
    }

/* -------------------------------------------------------------------- */
/*      Check file header signature.                                    */
/* -------------------------------------------------------------------- */
    uchar abyHeader[108];
    if (hSBN->sHooks.FRead(abyHeader, 108, 1, hSBN->fpSBN) != 1 ||
        abyHeader[0] != 0 ||
        abyHeader[1] != 0 ||
        abyHeader[2] != 0x27 ||
        (abyHeader[3] != 0x0A && abyHeader[3] != 0x0D) ||
        abyHeader[4] != 0xFF ||
        abyHeader[5] != 0xFF ||
        abyHeader[6] != 0xFE ||
        abyHeader[7] != 0x70)
    {
        hSBN->sHooks.Error( ".sbn file is unreadable, or corrupt." );
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

/* -------------------------------------------------------------------- */
/*      Read shapes bounding box.                                       */
/* -------------------------------------------------------------------- */

    memcpy(&hSBN->dfMinX, abyHeader + 32, 8);
    memcpy(&hSBN->dfMinY, abyHeader + 40, 8);
    memcpy(&hSBN->dfMaxX, abyHeader + 48, 8);
    memcpy(&hSBN->dfMaxY, abyHeader + 56, 8);

    if( !bBigEndian )
    {
        SwapWord(8, &hSBN->dfMinX);
        SwapWord(8, &hSBN->dfMinY);
        SwapWord(8, &hSBN->dfMaxX);
        SwapWord(8, &hSBN->dfMaxY);
    }

    if( hSBN->dfMinX > hSBN->dfMaxX ||
        hSBN->dfMinY > hSBN->dfMaxY )
    {
        hSBN->sHooks.Error( "Invalid extent in .sbn file." );
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

/* -------------------------------------------------------------------- */
/*      Read and check number of shapes.                                */
/* -------------------------------------------------------------------- */
    const int nShapeCount = READ_MSB_INT(abyHeader + 28);
    hSBN->nShapeCount = nShapeCount;
    if (nShapeCount < 0 || nShapeCount > 256000000 )
    {
        char szErrorMsg[64];
        snprintf(szErrorMsg, sizeof(szErrorMsg),
                "Invalid shape count in .sbn : %d", nShapeCount );
        hSBN->sHooks.Error( szErrorMsg );
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    /* Empty spatial index */
    if( nShapeCount == 0 )
    {
        return hSBN;
    }

/* -------------------------------------------------------------------- */
/*      Compute tree depth.                                             */
/*      It is computed such as in average there are not more than 8     */
/*      shapes per node. With a minimum depth of 2, and a maximum of 24 */
/* -------------------------------------------------------------------- */
    int nMaxDepth = 2;
    while( nMaxDepth < 24 && nShapeCount > ((1 << nMaxDepth) - 1) * 8 )
        nMaxDepth ++;
    hSBN->nMaxDepth = nMaxDepth;
    const int nMaxNodes = (1 << nMaxDepth) - 1;

/* -------------------------------------------------------------------- */
/*      Check that the first bin id is 1.                               */
/* -------------------------------------------------------------------- */

    if( READ_MSB_INT(abyHeader + 100) != 1 )
    {
        hSBN->sHooks.Error( "Unexpected bin id" );
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

/* -------------------------------------------------------------------- */
/*      Read and check number of node descriptors to be read.           */
/*      There are at most (2^nMaxDepth) - 1, but all are not necessary  */
/*      described. Non described nodes are empty.                       */
/* -------------------------------------------------------------------- */
    int nNodeDescSize = READ_MSB_INT(abyHeader + 104);
    nNodeDescSize *= 2; /* 16-bit words */

    /* each bin descriptor is made of 2 ints */
    const int nNodeDescCount = nNodeDescSize / 8;

    if ((nNodeDescSize % 8) != 0 ||
        nNodeDescCount < 0 || nNodeDescCount > nMaxNodes )
    {
        char szErrorMsg[64];
        snprintf(szErrorMsg, sizeof(szErrorMsg),
                "Invalid node descriptor size in .sbn : %d", nNodeDescSize );
        hSBN->sHooks.Error( szErrorMsg );
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    /* coverity[tainted_data] */
    uchar *pabyData = STATIC_CAST(uchar*, malloc( nNodeDescSize ));
    SBNNodeDescriptor *pasNodeDescriptor = STATIC_CAST(SBNNodeDescriptor*,
                calloc ( nMaxNodes, sizeof(SBNNodeDescriptor) ));
    if (pabyData == SHPLIB_NULLPTR || pasNodeDescriptor == SHPLIB_NULLPTR)
    {
        free(pabyData);
        free(pasNodeDescriptor);
        hSBN->sHooks.Error( "Out of memory error" );
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

/* -------------------------------------------------------------------- */
/*      Read node descriptors.                                          */
/* -------------------------------------------------------------------- */
    if (hSBN->sHooks.FRead(pabyData, nNodeDescSize, 1,
                                  hSBN->fpSBN) != 1)
    {
        free(pabyData);
        free(pasNodeDescriptor);
        hSBN->sHooks.Error( "Cannot read node descriptors" );
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    hSBN->pasNodeDescriptor = pasNodeDescriptor;

    for(int i = 0; i < nNodeDescCount; i++)
    {
/* -------------------------------------------------------------------- */
/*      Each node descriptor contains the index of the first bin that   */
/*      described it, and the number of shapes in this first bin and    */
/*      the following bins (in the relevant case).                      */
/* -------------------------------------------------------------------- */
        int nBinStart = READ_MSB_INT(pabyData + 8 * i);
        int nNodeShapeCount = READ_MSB_INT(pabyData + 8 * i + 4);
        pasNodeDescriptor[i].nBinStart = nBinStart > 0 ? nBinStart : 0;
        pasNodeDescriptor[i].nShapeCount = nNodeShapeCount;

        if ((nBinStart > 0 && nNodeShapeCount == 0) ||
            nNodeShapeCount < 0 || nNodeShapeCount > nShapeCount)
        {
            hSBN->sHooks.Error( "Inconsistent shape count in bin" );
            SBNCloseDiskTree(hSBN);
            return SHPLIB_NULLPTR;
        }
    }

    free(pabyData);
    /* pabyData = SHPLIB_NULLPTR; */

    /* Locate first non-empty node */
    int nCurNode = 0;
    while(nCurNode < nMaxNodes && pasNodeDescriptor[nCurNode].nBinStart <= 0)
        nCurNode ++;

    if( nCurNode >= nMaxNodes)
    {
        hSBN->sHooks.Error( "All nodes are empty" );
        SBNCloseDiskTree(hSBN);
        return SHPLIB_NULLPTR;
    }

    pasNodeDescriptor[nCurNode].nBinOffset =
        STATIC_CAST(int, hSBN->sHooks.FTell(hSBN->fpSBN));

    /* Compute the index of the next non empty node. */
    int nNextNonEmptyNode = nCurNode + 1;
    while(nNextNonEmptyNode < nMaxNodes &&
        pasNodeDescriptor[nNextNonEmptyNode].nBinStart <= 0)
        nNextNonEmptyNode ++;

    int nExpectedBinId = 1;

/* -------------------------------------------------------------------- */
/*      Traverse bins to compute the offset of the first bin of each    */
/*      node.                                                           */
/*      Note: we could use the .sbx file to compute the offsets instead.*/
/* -------------------------------------------------------------------- */
    uchar abyBinHeader[8];

    while( hSBN->sHooks.FRead(abyBinHeader, 8, 1,
                                     hSBN->fpSBN) == 1 )
    {
        nExpectedBinId++;

        const int nBinId = READ_MSB_INT(abyBinHeader);
        int nBinSize = READ_MSB_INT(abyBinHeader + 4);
        nBinSize *= 2; /* 16-bit words */

        if( nBinId != nExpectedBinId )
        {
            hSBN->sHooks.Error( "Unexpected bin id" );
            SBNCloseDiskTree(hSBN);
            return SHPLIB_NULLPTR;
        }

        /* Bins are always limited to 100 features */
        /* If there are more, then they are located in continuous bins */
        if( (nBinSize % 8) != 0 || nBinSize <= 0 || nBinSize > 100 * 8)
        {
            hSBN->sHooks.Error( "Unexpected bin size" );
            SBNCloseDiskTree(hSBN);
            return SHPLIB_NULLPTR;
        }

        if( nNextNonEmptyNode < nMaxNodes &&
            nBinId == pasNodeDescriptor[nNextNonEmptyNode].nBinStart )
        {
            nCurNode = nNextNonEmptyNode;
            pasNodeDescriptor[nCurNode].nBinOffset =
                STATIC_CAST(int, hSBN->sHooks.FTell(hSBN->fpSBN)) - 8;

            /* Compute the index of the next non empty node. */
            nNextNonEmptyNode = nCurNode + 1;
            while(nNextNonEmptyNode < nMaxNodes &&
                  pasNodeDescriptor[nNextNonEmptyNode].nBinStart <= 0)
                nNextNonEmptyNode ++;
        }

        pasNodeDescriptor[nCurNode].nBinCount ++;

        /* Skip shape description */
        hSBN->sHooks.FSeek(hSBN->fpSBN, nBinSize, SEEK_CUR);
    }

    return hSBN;
}

/***********************************************************************/
/*                          SBNCloseDiskTree()                         */
/************************************************************************/

void SBNCloseDiskTree( SBNSearchHandle hSBN ) {
    if (hSBN == SHPLIB_NULLPTR)
        return;

    if( hSBN->pasNodeDescriptor != SHPLIB_NULLPTR )
    {
        const int nMaxNodes = (1 << hSBN->nMaxDepth) - 1;
        for(int i = 0; i < nMaxNodes; i++)
        {
            if( hSBN->pasNodeDescriptor[i].pabyShapeDesc != SHPLIB_NULLPTR )
                free(hSBN->pasNodeDescriptor[i].pabyShapeDesc);
        }
    }

    /* printf("hSBN->nTotalBytesRead = %d\n", hSBN->nTotalBytesRead); */

    hSBN->sHooks.FClose(hSBN->fpSBN);
    free(hSBN->pasNodeDescriptor);
    free(hSBN);
}


/************************************************************************/
/*                             SfRealloc()                              */
/*                                                                      */
/*      A realloc cover function that will access a NULL pointer as     */
/*      a valid input.                                                  */
/************************************************************************/

static void * SfRealloc( void * pMem, int nNewSize ) {
    if( pMem == SHPLIB_NULLPTR )
        return malloc(nNewSize);
    else
        return realloc(pMem,nNewSize);
}

/************************************************************************/
/*                         SBNAddShapeId()                              */
/************************************************************************/

static bool SBNAddShapeId( SearchStruct* psSearch,
                          int nShapeId ) {
    if (psSearch->nShapeCount == psSearch->nShapeAlloc)
    {
        psSearch->nShapeAlloc =
            STATIC_CAST(int, ((psSearch->nShapeCount + 100) * 5) / 4);
        int *pNewPtr =
            STATIC_CAST(int *, SfRealloc( psSearch->panShapeId,
                               psSearch->nShapeAlloc * sizeof(int) ));
        if( pNewPtr == SHPLIB_NULLPTR )
        {
            psSearch->hSBN->sHooks.Error( "Out of memory error" );
            return false;
        }
        psSearch->panShapeId = pNewPtr;
    }

    psSearch->panShapeId[psSearch->nShapeCount] = nShapeId;
    psSearch->nShapeCount ++;
    return true;
}

/************************************************************************/
/*                     SBNSearchDiskInternal()                          */
/************************************************************************/

/*      Due to the way integer coordinates are rounded,                 */
/*      we can use a strict intersection test, except when the node     */
/*      bounding box or the search bounding box is degenerated.         */
#define SEARCH_BB_INTERSECTS(_bMinX, _bMinY, _bMaxX, _bMaxY) \
   (((bSearchMinX < _bMaxX && bSearchMaxX > _bMinX) || \
    ((_bMinX == _bMaxX || bSearchMinX == bSearchMaxX) && \
     bSearchMinX <= _bMaxX && bSearchMaxX >= _bMinX)) && \
    ((bSearchMinY < _bMaxY && bSearchMaxY > _bMinY) || \
    ((_bMinY == _bMaxY || bSearchMinY == bSearchMaxY ) && \
     bSearchMinY <= _bMaxY && bSearchMaxY >= _bMinY)))


static bool SBNSearchDiskInternal( SearchStruct* psSearch,
                                  int nDepth,
                                  int nNodeId,
                                  coord bNodeMinX,
                                  coord bNodeMinY,
                                  coord bNodeMaxX,
                                  coord bNodeMaxY )
{
    const coord bSearchMinX = psSearch->bMinX;
    const coord bSearchMinY = psSearch->bMinY;
    const coord bSearchMaxX = psSearch->bMaxX;
    const coord bSearchMaxY = psSearch->bMaxY;

    SBNSearchHandle hSBN = psSearch->hSBN;

    SBNNodeDescriptor* psNode = &(hSBN->pasNodeDescriptor[nNodeId]);

/* -------------------------------------------------------------------- */
/*      Check if this node contains shapes that intersect the search    */
/*      bounding box.                                                   */
/* -------------------------------------------------------------------- */
    if ( psNode->bBBoxInit &&
         !(SEARCH_BB_INTERSECTS(psNode->bMinX, psNode->bMinY,
                                psNode->bMaxX, psNode->bMaxY)) )

    {
        /* No intersection, then don't try to read the shapes attached */
        /* to this node */
    }

/* -------------------------------------------------------------------- */
/*      If this node contains shapes that are cached, then read them.   */
/* -------------------------------------------------------------------- */
    else if (psNode->pabyShapeDesc != SHPLIB_NULLPTR)
    {
        uchar* pabyShapeDesc = psNode->pabyShapeDesc;

        /* printf("nNodeId = %d, nDepth = %d\n", nNodeId, nDepth); */

        for(int j = 0; j < psNode->nShapeCount; j++)
        {
            const coord bMinX = pabyShapeDesc[0];
            const coord bMinY = pabyShapeDesc[1];
            const coord bMaxX = pabyShapeDesc[2];
            const coord bMaxY = pabyShapeDesc[3];

            if( SEARCH_BB_INTERSECTS(bMinX, bMinY, bMaxX, bMaxY) )
            {
                int nShapeId = READ_MSB_INT(pabyShapeDesc + 4);

                /* Caution : we count shape id starting from 0, and not 1 */
                nShapeId --;

                /*printf("shape=%d, minx=%d, miny=%d, maxx=%d, maxy=%d\n",
                       nShapeId, bMinX, bMinY, bMaxX, bMaxY);*/

                if( !SBNAddShapeId( psSearch, nShapeId ) )
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
            psNode->pabyShapeDesc = STATIC_CAST(uchar*, malloc(psNode->nShapeCount * 8));

        uchar abyBinHeader[8];
        int nShapeCountAcc = 0;

        for(int i = 0; i < psNode->nBinCount; i++)
        {
#ifdef DEBUG_IO
            psSearch->nBytesRead += 8;
#endif
            if( hSBN->sHooks.FRead(abyBinHeader, 8, 1,
                                          hSBN->fpSBN) != 1)
            {
                hSBN->sHooks.Error( "I/O error" );
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                return false;
            }

            if ( READ_MSB_INT(abyBinHeader + 0) != psNode->nBinStart + i )
            {
                hSBN->sHooks.Error( "Unexpected bin id" );
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                return false;
            }

            int nBinSize = READ_MSB_INT(abyBinHeader + 4);
            nBinSize *= 2; /* 16-bit words */

            int nShapes = nBinSize / 8;

            /* Bins are always limited to 100 features */
            if( (nBinSize % 8) != 0 || nShapes <= 0 || nShapes > 100)
            {
                hSBN->sHooks.Error( "Unexpected bin size" );
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                return false;
            }

            if( nShapeCountAcc + nShapes > psNode->nShapeCount)
            {
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                hSBN->sHooks.Error( "Inconsistent shape count for bin" );
                return false;
            }

            uchar* pabyBinShape;
            if (nDepth < CACHED_DEPTH_LIMIT && psNode->pabyShapeDesc != SHPLIB_NULLPTR)
            {
                pabyBinShape = psNode->pabyShapeDesc + nShapeCountAcc * 8;
            }
            else
            {
                pabyBinShape = psSearch->abyBinShape;
            }

#ifdef DEBUG_IO
            psSearch->nBytesRead += nBinSize;
#endif
            if (hSBN->sHooks.FRead(pabyBinShape, nBinSize, 1,
                                          hSBN->fpSBN) != 1)
            {
                hSBN->sHooks.Error( "I/O error" );
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

            for(int j = 0; j < nShapes; j++)
            {
                const coord bMinX = pabyBinShape[0];
                const coord bMinY = pabyBinShape[1];
                const coord bMaxX = pabyBinShape[2];
                const coord bMaxY = pabyBinShape[3];

                if( !psNode->bBBoxInit )
                {
#ifdef sanity_checks
/* -------------------------------------------------------------------- */
/*      Those tests only check that the shape bounding box in the bin   */
/*      are consistent (self-consistent and consistent with the node    */
/*      they are attached to). They are optional however (as far as     */
/*      the safety of runtime is concerned at least).                   */
/* -------------------------------------------------------------------- */

                    if( !(((bMinX < bMaxX ||
                           (bMinX == 0 && bMaxX == 0) ||
                           (bMinX == 255 && bMaxX == 255))) &&
                          ((bMinY < bMaxY ||
                           (bMinY == 0 && bMaxY == 0) ||
                           (bMinY == 255 && bMaxY == 255)))) ||
                        bMaxX < bNodeMinX || bMaxY < bNodeMinY ||
                        bMinX > bNodeMaxX || bMinY > bNodeMaxY )
                    {
                        /* printf("shape %d %d %d %d\n", bMinX, bMinY, bMaxX, bMaxY);*/
                        /* printf("node  %d %d %d %d\n", bNodeMinX, bNodeMinY, bNodeMaxX, bNodeMaxY);*/
                        hSBN->sHooks.Error(
                            "Invalid shape bounding box in bin" );
                        free(psNode->pabyShapeDesc);
                        psNode->pabyShapeDesc = SHPLIB_NULLPTR;
                        return false;
                    }
#endif
                    if (bMinX < psNode->bMinX) psNode->bMinX = bMinX;
                    if (bMinY < psNode->bMinY) psNode->bMinY = bMinY;
                    if (bMaxX > psNode->bMaxX) psNode->bMaxX = bMaxX;
                    if (bMaxY > psNode->bMaxY) psNode->bMaxY = bMaxY;
                }

                if( SEARCH_BB_INTERSECTS(bMinX, bMinY, bMaxX, bMaxY) )
                {
                    int nShapeId = READ_MSB_INT(pabyBinShape + 4);

                    /* Caution : we count shape id starting from 0, and not 1 */
                    nShapeId --;

                    /*printf("shape=%d, minx=%d, miny=%d, maxx=%d, maxy=%d\n",
                        nShapeId, bMinX, bMinY, bMaxX, bMaxY);*/

                    if( !SBNAddShapeId( psSearch, nShapeId ) )
                        return false;
                }

                pabyBinShape += 8;
            }
        }

        if( nShapeCountAcc != psNode->nShapeCount)
        {
            free(psNode->pabyShapeDesc);
            psNode->pabyShapeDesc = SHPLIB_NULLPTR;
            hSBN->sHooks.Error( "Inconsistent shape count for bin" );
            return false;
        }

        psNode->bBBoxInit = true;
    }

/* -------------------------------------------------------------------- */
/*      Look up in child nodes.                                         */
/* -------------------------------------------------------------------- */
    if( nDepth + 1 < hSBN->nMaxDepth )
    {
        nNodeId = nNodeId * 2 + 1;

        if( (nDepth % 2) == 0 ) /* x split */
        {
            const coord bMid = STATIC_CAST(coord, 1 + (STATIC_CAST(int, bNodeMinX) + bNodeMaxX) / 2);
            if( bSearchMinX <= bMid - 1 &&
                !SBNSearchDiskInternal( psSearch, nDepth + 1, nNodeId + 1,
                                        bNodeMinX, bNodeMinY,
                                        bMid - 1, bNodeMaxY ) )
            {
                return false;
            }
            if( bSearchMaxX >= bMid &&
                !SBNSearchDiskInternal( psSearch, nDepth + 1, nNodeId,
                                        bMid, bNodeMinY,
                                        bNodeMaxX, bNodeMaxY ) )
            {
                return false;
            }
        }
        else /* y split */
        {
            const coord bMid = STATIC_CAST(coord, 1 + (STATIC_CAST(int, bNodeMinY) + bNodeMaxY) / 2);
            if( bSearchMinY <= bMid - 1 &&
                !SBNSearchDiskInternal( psSearch, nDepth + 1, nNodeId + 1,
                                        bNodeMinX, bNodeMinY,
                                        bNodeMaxX, bMid - 1 ) )
            {
                return false;
            }
            if( bSearchMaxY >= bMid &&
                !SBNSearchDiskInternal( psSearch, nDepth + 1, nNodeId,
                                        bNodeMinX, bMid,
                                        bNodeMaxX, bNodeMaxY ) )
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
static int
compare_ints( const void * a, const void * b)
{
    return *REINTERPRET_CAST(const int*, a) - *REINTERPRET_CAST(const int*, b);
}

/************************************************************************/
/*                        SBNSearchDiskTree()                           */
/************************************************************************/

int* SBNSearchDiskTree( SBNSearchHandle hSBN,
                        double *padfBoundsMin, double *padfBoundsMax,
                        int *pnShapeCount ) {
    *pnShapeCount = 0;

    const double dfMinX = padfBoundsMin[0];
    const double dfMinY = padfBoundsMin[1];
    const double dfMaxX = padfBoundsMax[0];
    const double dfMaxY = padfBoundsMax[1];

    if( dfMinX > dfMaxX || dfMinY > dfMaxY )
        return SHPLIB_NULLPTR;

    if( dfMaxX < hSBN->dfMinX || dfMaxY < hSBN->dfMinY ||
        dfMinX > hSBN->dfMaxX || dfMinY > hSBN->dfMaxY )
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
    if ( dfDiskXExtent == 0.0 )
    {
        bMinX = 0;
        bMaxX = 255;
    }
    else
    {
        if( dfMinX < hSBN->dfMinX )
            bMinX = 0;
        else
        {
            const double dfMinX_255 = (dfMinX - hSBN->dfMinX)
                                                    / dfDiskXExtent * 255.0;
            bMinX = STATIC_CAST(int, floor(dfMinX_255 - 0.005));
            if( bMinX < 0 ) bMinX = 0;
        }

        if( dfMaxX > hSBN->dfMaxX )
            bMaxX = 255;
        else
        {
            const double dfMaxX_255 = (dfMaxX - hSBN->dfMinX)
                                                    / dfDiskXExtent * 255.0;
            bMaxX = STATIC_CAST(int, ceil(dfMaxX_255 + 0.005));
            if( bMaxX > 255 ) bMaxX = 255;
        }
    }

    if ( dfDiskYExtent == 0.0 )
    {
        bMinY = 0;
        bMaxY = 255;
    }
    else
    {
        if( dfMinY < hSBN->dfMinY )
            bMinY = 0;
        else
        {
            const double dfMinY_255 = (dfMinY - hSBN->dfMinY)
                                                    / dfDiskYExtent * 255.0;
            bMinY = STATIC_CAST(int, floor(dfMinY_255 - 0.005));
            if( bMinY < 0 ) bMinY = 0;
        }

        if( dfMaxY > hSBN->dfMaxY )
            bMaxY = 255;
        else
        {
            const double dfMaxY_255 = (dfMaxY - hSBN->dfMinY)
                                                    / dfDiskYExtent * 255.0;
            bMaxY = STATIC_CAST(int, ceil(dfMaxY_255 + 0.005));
            if( bMaxY > 255 ) bMaxY = 255;
        }
    }

/* -------------------------------------------------------------------- */
/*      Run the search.                                                 */
/* -------------------------------------------------------------------- */

    return SBNSearchDiskTreeInteger(hSBN,
                                    bMinX, bMinY, bMaxX, bMaxY,
                                    pnShapeCount);
}

/************************************************************************/
/*                     SBNSearchDiskTreeInteger()                       */
/************************************************************************/

int* SBNSearchDiskTreeInteger( SBNSearchHandle hSBN,
                               int bMinX, int bMinY, int bMaxX, int bMaxY,
                               int *pnShapeCount ) {
    *pnShapeCount = 0;

    if( bMinX > bMaxX || bMinY > bMaxY )
        return SHPLIB_NULLPTR;

    if( bMaxX < 0 || bMaxY < 0 || bMinX > 255 || bMinY > 255 )
        return SHPLIB_NULLPTR;

    if( hSBN->nShapeCount == 0 )
        return SHPLIB_NULLPTR;
/* -------------------------------------------------------------------- */
/*      Run the search.                                                 */
/* -------------------------------------------------------------------- */
    SearchStruct sSearch;
    memset( &sSearch, 0, sizeof(sSearch) );
    sSearch.hSBN = hSBN;
    sSearch.bMinX = STATIC_CAST(coord, bMinX >= 0 ? bMinX : 0);
    sSearch.bMinY = STATIC_CAST(coord, bMinY >= 0 ? bMinY : 0);
    sSearch.bMaxX = STATIC_CAST(coord, bMaxX <= 255 ? bMaxX : 255);
    sSearch.bMaxY = STATIC_CAST(coord, bMaxY <= 255 ? bMaxY : 255);
    sSearch.nShapeCount = 0;
    sSearch.nShapeAlloc = 0;
    sSearch.panShapeId = STATIC_CAST(int*, calloc(1, sizeof(int)));
#ifdef DEBUG_IO
    sSearch.nBytesRead = 0;
#endif

    const bool bRet = SBNSearchDiskInternal(&sSearch, 0, 0, 0, 0, 255, 255);

#ifdef DEBUG_IO
    hSBN->nTotalBytesRead += sSearch.nBytesRead;
    /* printf("nBytesRead = %d\n", sSearch.nBytesRead); */
#endif

    if( !bRet )
    {
        free( sSearch.panShapeId );
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

void SBNSearchFreeIds( int* panShapeId ) {
    free( panShapeId );
}
