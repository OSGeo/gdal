/******************************************************************************
 * $Id$
 *
 * Project:  Shapelib
 * Purpose:  Implementation of search in ESRI SBN spatial index.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault
 *
 * This software is available under the following "MIT Style" license,
 * or at the option of the licensee under the LGPL (see LICENSE.LGPL).  This
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

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

SHP_CVSID("$Id$")

#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif

#define READ_MSB_INT(ptr) \
        (((ptr)[0] << 24) | ((ptr)[1] << 16) | ((ptr)[2] << 8) | (ptr)[3])

#define CACHED_DEPTH_LIMIT      8

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

    int     bBBoxInit;      /* TRUE if the following bounding box has been computed. */
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

static void SwapWord( int length, void * wordP )

{
    int     i;
    uchar   temp;

    for( i=0; i < length/2; i++ )
    {
        temp = ((uchar *) wordP)[i];
        ((uchar *)wordP)[i] = ((uchar *) wordP)[length-i-1];
        ((uchar *) wordP)[length-i-1] = temp;
    }
}

/************************************************************************/
/*                         SBNOpenDiskTree()                            */
/************************************************************************/

SBNSearchHandle SBNOpenDiskTree( const char* pszSBNFilename,
                                 SAHooks *psHooks )
{
    int i;
    SBNSearchHandle hSBN;
    uchar abyHeader[108];
    int nShapeCount;
    int nMaxDepth;
    int nMaxNodes;
    int nNodeDescSize;
    int nNodeDescCount;
    uchar* pabyData = NULL;
    SBNNodeDescriptor* pasNodeDescriptor = NULL;
    uchar abyBinHeader[8];
    int nCurNode;
    int nNextNonEmptyNode;
    int nExpectedBinId;
    int bBigEndian;

/* -------------------------------------------------------------------- */
/*  Establish the byte order on this machine.                           */
/* -------------------------------------------------------------------- */
    i = 1;
    if( *((unsigned char *) &i) == 1 )
        bBigEndian = FALSE;
    else
        bBigEndian = TRUE;

/* -------------------------------------------------------------------- */
/*      Initialize the handle structure.                                */
/* -------------------------------------------------------------------- */
    hSBN = (SBNSearchHandle)
                        calloc(sizeof(struct SBNSearchInfo),1);

    if (psHooks == NULL)
        SASetupDefaultHooks( &(hSBN->sHooks) );
    else
        memcpy( &(hSBN->sHooks), psHooks, sizeof(SAHooks) );

    hSBN->fpSBN = hSBN->sHooks.FOpen(pszSBNFilename, "rb");
    if (hSBN->fpSBN == NULL)
    {
        free(hSBN);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check file header signature.                                    */
/* -------------------------------------------------------------------- */
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
        return NULL;
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
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read and check number of shapes.                                */
/* -------------------------------------------------------------------- */
    nShapeCount = READ_MSB_INT(abyHeader + 28);
    hSBN->nShapeCount = nShapeCount;
    if (nShapeCount < 0 || nShapeCount > 256000000 )
    {
        char szErrorMsg[64];
        sprintf(szErrorMsg, "Invalid shape count in .sbn : %d", nShapeCount );
        hSBN->sHooks.Error( szErrorMsg );
        SBNCloseDiskTree(hSBN);
        return NULL;
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
    nMaxDepth = 2;
    while( nMaxDepth < 24 && nShapeCount > ((1 << nMaxDepth) - 1) * 8 )
        nMaxDepth ++;
    hSBN->nMaxDepth = nMaxDepth;
    nMaxNodes = (1 << nMaxDepth) - 1;

/* -------------------------------------------------------------------- */
/*      Check that the first bin id is 1.                               */
/* -------------------------------------------------------------------- */

    if( READ_MSB_INT(abyHeader + 100) != 1 )
    {
        hSBN->sHooks.Error( "Unexpected bin id" );
        SBNCloseDiskTree(hSBN);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read and check number of node descriptors to be read.           */
/*      There are at most (2^nMaxDepth) - 1, but all are not necessary  */
/*      described. Non described nodes are empty.                       */
/* -------------------------------------------------------------------- */
    nNodeDescSize = READ_MSB_INT(abyHeader + 104);
    nNodeDescSize *= 2; /* 16-bit words */

    /* each bin descriptor is made of 2 ints */
    nNodeDescCount = nNodeDescSize / 8;

    if ((nNodeDescSize % 8) != 0 ||
        nNodeDescCount < 0 || nNodeDescCount > nMaxNodes )
    {
        char szErrorMsg[64];
        sprintf(szErrorMsg,
                "Invalid node descriptor size in .sbn : %d", nNodeDescSize );
        hSBN->sHooks.Error( szErrorMsg );
        SBNCloseDiskTree(hSBN);
        return NULL;
    }

    pabyData = (uchar*) malloc( nNodeDescSize );
    pasNodeDescriptor = (SBNNodeDescriptor*)
                calloc ( nMaxNodes, sizeof(SBNNodeDescriptor) );
    if (pabyData == NULL || pasNodeDescriptor == NULL)
    {
        free(pabyData);
        free(pasNodeDescriptor);
        hSBN->sHooks.Error( "Out of memory error" );
        SBNCloseDiskTree(hSBN);
        return NULL;
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
        return NULL;
    }

    hSBN->pasNodeDescriptor = pasNodeDescriptor;

    for(i = 0; i < nNodeDescCount; i++)
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
            hSBN->sHooks.Error( "Inconsistant shape count in bin" );
            SBNCloseDiskTree(hSBN);
            return NULL;
        }
    }

    free(pabyData);
    pabyData = NULL;

    /* Locate first non-empty node */
    nCurNode = 0;
    while(nCurNode < nMaxNodes && pasNodeDescriptor[nCurNode].nBinStart <= 0)
        nCurNode ++;

    if( nCurNode >= nMaxNodes)
    {
        hSBN->sHooks.Error( "All nodes are empty" );
        SBNCloseDiskTree(hSBN);
        return NULL;
    }

    pasNodeDescriptor[nCurNode].nBinOffset =
        hSBN->sHooks.FTell(hSBN->fpSBN);

    /* Compute the index of the next non empty node. */
    nNextNonEmptyNode = nCurNode + 1;
    while(nNextNonEmptyNode < nMaxNodes &&
        pasNodeDescriptor[nNextNonEmptyNode].nBinStart <= 0)
        nNextNonEmptyNode ++;

    nExpectedBinId = 1;

/* -------------------------------------------------------------------- */
/*      Traverse bins to compute the offset of the first bin of each    */
/*      node.                                                           */
/*      Note: we could use the .sbx file to compute the offsets instead.*/
/* -------------------------------------------------------------------- */
    while( hSBN->sHooks.FRead(abyBinHeader, 8, 1,
                                     hSBN->fpSBN) == 1 )
    {
        int nBinId;
        int nBinSize;

        nExpectedBinId ++;

        nBinId = READ_MSB_INT(abyBinHeader);
        nBinSize = READ_MSB_INT(abyBinHeader + 4);
        nBinSize *= 2; /* 16-bit words */

        if( nBinId != nExpectedBinId )
        {
            hSBN->sHooks.Error( "Unexpected bin id" );
            SBNCloseDiskTree(hSBN);
            return NULL;
        }

        /* Bins are always limited to 100 features */
        /* If there are more, then they are located in continuous bins */
        if( (nBinSize % 8) != 0 || nBinSize <= 0 || nBinSize > 100 * 8)
        {
            hSBN->sHooks.Error( "Unexpected bin size" );
            SBNCloseDiskTree(hSBN);
            return NULL;
        }

        if( nNextNonEmptyNode < nMaxNodes &&
            nBinId == pasNodeDescriptor[nNextNonEmptyNode].nBinStart )
        {
            nCurNode = nNextNonEmptyNode;
            pasNodeDescriptor[nCurNode].nBinOffset =
                hSBN->sHooks.FTell(hSBN->fpSBN) - 8;

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

void SBNCloseDiskTree( SBNSearchHandle hSBN )
{
    int i;
    int nMaxNodes;

    if (hSBN == NULL)
        return;

    if( hSBN->pasNodeDescriptor != NULL )
    {
        nMaxNodes = (1 << hSBN->nMaxDepth) - 1;
        for(i = 0; i < nMaxNodes; i++)
        {
            if( hSBN->pasNodeDescriptor[i].pabyShapeDesc != NULL )
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

static void * SfRealloc( void * pMem, int nNewSize )

{
    if( pMem == NULL )
        return( (void *) malloc(nNewSize) );
    else
        return( (void *) realloc(pMem,nNewSize) );
}

/************************************************************************/
/*                         SBNAddShapeId()                              */
/************************************************************************/

static int SBNAddShapeId( SearchStruct* psSearch,
                          int nShapeId )
{
    if (psSearch->nShapeCount == psSearch->nShapeAlloc)
    {
        int* pNewPtr;

        psSearch->nShapeAlloc =
            (int) (((psSearch->nShapeCount + 100) * 5) / 4);
        pNewPtr =
            (int *) SfRealloc( psSearch->panShapeId,
                               psSearch->nShapeAlloc * sizeof(int) );
        if( pNewPtr == NULL )
        {
            psSearch->hSBN->sHooks.Error( "Out of memory error" );
            return FALSE;
        }
        psSearch->panShapeId = pNewPtr;
    }

    psSearch->panShapeId[psSearch->nShapeCount] = nShapeId;
    psSearch->nShapeCount ++;
    return TRUE;
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


static int SBNSearchDiskInternal( SearchStruct* psSearch,
                                  int nDepth,
                                  int nNodeId,
                                  coord bNodeMinX,
                                  coord bNodeMinY,
                                  coord bNodeMaxX,
                                  coord bNodeMaxY )
{
    SBNSearchHandle hSBN;
    SBNNodeDescriptor* psNode;
    coord bSearchMinX = psSearch->bMinX;
    coord bSearchMinY = psSearch->bMinY;
    coord bSearchMaxX = psSearch->bMaxX;
    coord bSearchMaxY = psSearch->bMaxY;

    hSBN = psSearch->hSBN;

    psNode = &(hSBN->pasNodeDescriptor[nNodeId]);

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
    else if (psNode->pabyShapeDesc != NULL)
    {
        int j;
        uchar* pabyShapeDesc = psNode->pabyShapeDesc;

        /* printf("nNodeId = %d, nDepth = %d\n", nNodeId, nDepth); */

        for(j = 0; j < psNode->nShapeCount; j++)
        {
            coord bMinX = pabyShapeDesc[0];
            coord bMinY = pabyShapeDesc[1];
            coord bMaxX = pabyShapeDesc[2];
            coord bMaxY = pabyShapeDesc[3];

            if( SEARCH_BB_INTERSECTS(bMinX, bMinY, bMaxX, bMaxY) )
            {
                int nShapeId;

                nShapeId = READ_MSB_INT(pabyShapeDesc + 4);

                /* Caution : we count shape id starting from 0, and not 1 */
                nShapeId --;

                /*printf("shape=%d, minx=%d, miny=%d, maxx=%d, maxy=%d\n",
                       nShapeId, bMinX, bMinY, bMaxX, bMaxY);*/

                if( !SBNAddShapeId( psSearch, nShapeId ) )
                    return FALSE;
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
        uchar abyBinHeader[8];
        int   nBinSize, nShapes;
        int   nShapeCountAcc = 0;
        int   i, j;

        /* printf("nNodeId = %d, nDepth = %d\n", nNodeId, nDepth); */

        hSBN->sHooks.FSeek(hSBN->fpSBN, psNode->nBinOffset, SEEK_SET);

        if (nDepth < CACHED_DEPTH_LIMIT)
            psNode->pabyShapeDesc = (uchar*) malloc(psNode->nShapeCount * 8);

        for(i = 0; i < psNode->nBinCount; i++)
        {
            uchar* pabyBinShape;

#ifdef DEBUG_IO
            psSearch->nBytesRead += 8;
#endif
            if( hSBN->sHooks.FRead(abyBinHeader, 8, 1,
                                          hSBN->fpSBN) != 1)
            {
                hSBN->sHooks.Error( "I/O error" );
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = NULL;
                return FALSE;
            }

            if ( READ_MSB_INT(abyBinHeader + 0) != psNode->nBinStart + i )
            {
                hSBN->sHooks.Error( "Unexpected bin id" );
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = NULL;
                return FALSE;
            }

            nBinSize = READ_MSB_INT(abyBinHeader + 4);
            nBinSize *= 2; /* 16-bit words */

            nShapes = nBinSize / 8;

            /* Bins are always limited to 100 features */
            if( (nBinSize % 8) != 0 || nShapes <= 0 || nShapes > 100)
            {
                hSBN->sHooks.Error( "Unexpected bin size" );
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = NULL;
                return FALSE;
            }

            if( nShapeCountAcc + nShapes > psNode->nShapeCount)
            {
                free(psNode->pabyShapeDesc);
                psNode->pabyShapeDesc = NULL;
                hSBN->sHooks.Error( "Inconsistant shape count for bin" );
                return FALSE;
            }

            if (nDepth < CACHED_DEPTH_LIMIT && psNode->pabyShapeDesc != NULL)
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
                psNode->pabyShapeDesc = NULL;
                return FALSE;
            }

            nShapeCountAcc += nShapes;

            if (i == 0 && !psNode->bBBoxInit)
            {
                psNode->bMinX = pabyBinShape[0];
                psNode->bMinY = pabyBinShape[1];
                psNode->bMaxX = pabyBinShape[2];
                psNode->bMaxY = pabyBinShape[3];
            }

            for(j = 0; j < nShapes; j++)
            {
                coord bMinX = pabyBinShape[0];
                coord bMinY = pabyBinShape[1];
                coord bMaxX = pabyBinShape[2];
                coord bMaxY = pabyBinShape[3];

                if( !psNode->bBBoxInit )
                {
#ifdef sanity_checks
/* -------------------------------------------------------------------- */
/*      Those tests only check that the shape bounding box in the bin   */
/*      are consistant (self-consistant and consistant with the node    */
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
                        /*printf("shape %d %d %d %d\n", bMinX, bMinY, bMaxX, bMaxY);
                        printf("node  %d %d %d %d\n", bNodeMinX, bNodeMinY, bNodeMaxX, bNodeMaxY);*/
                        hSBN->sHooks.Error(
                            "Invalid shape bounding box in bin" );
                        free(psNode->pabyShapeDesc);
                        psNode->pabyShapeDesc = NULL;
                        return FALSE;
                    }
#endif
                    if (bMinX < psNode->bMinX) psNode->bMinX = bMinX;
                    if (bMinY < psNode->bMinY) psNode->bMinY = bMinY;
                    if (bMaxX > psNode->bMaxX) psNode->bMaxX = bMaxX;
                    if (bMaxY > psNode->bMaxY) psNode->bMaxY = bMaxY;
                }

                if( SEARCH_BB_INTERSECTS(bMinX, bMinY, bMaxX, bMaxY) )
                {
                    int nShapeId;

                    nShapeId = READ_MSB_INT(pabyBinShape + 4);

                    /* Caution : we count shape id starting from 0, and not 1 */
                    nShapeId --; 

                    /*printf("shape=%d, minx=%d, miny=%d, maxx=%d, maxy=%d\n",
                        nShapeId, bMinX, bMinY, bMaxX, bMaxY);*/

                    if( !SBNAddShapeId( psSearch, nShapeId ) )
                        return FALSE;
                }

                pabyBinShape += 8;
            }
        }

        if( nShapeCountAcc != psNode->nShapeCount)
        {
            free(psNode->pabyShapeDesc);
            psNode->pabyShapeDesc = NULL;
            hSBN->sHooks.Error( "Inconsistant shape count for bin" );
            return FALSE;
        }

        psNode->bBBoxInit = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Look up in child nodes.                                         */
/* -------------------------------------------------------------------- */
    if( nDepth + 1 < hSBN->nMaxDepth )
    {
        nNodeId = nNodeId * 2 + 1;

        if( (nDepth % 2) == 0 ) /* x split */
        {
            coord bMid = (coord) (1 + ((int)bNodeMinX + bNodeMaxX) / 2);
            if( bSearchMinX <= bMid - 1 &&
                !SBNSearchDiskInternal( psSearch, nDepth + 1, nNodeId + 1,
                                        bNodeMinX, bNodeMinY,
                                        bMid - 1, bNodeMaxY ) )
            {
                return FALSE;
            }
            if( bSearchMaxX >= bMid &&
                !SBNSearchDiskInternal( psSearch, nDepth + 1, nNodeId,
                                        bMid, bNodeMinY,
                                        bNodeMaxX, bNodeMaxY ) )
            {
                return FALSE;
            }
        }
        else /* y split */
        {
            coord bMid = (coord) (1 + ((int)bNodeMinY + bNodeMaxY) / 2);
            if( bSearchMinY <= bMid - 1 &&
                !SBNSearchDiskInternal( psSearch, nDepth + 1, nNodeId + 1,
                                        bNodeMinX, bNodeMinY,
                                        bNodeMaxX, bMid - 1 ) )
            {
                return FALSE;
            }
            if( bSearchMaxY >= bMid &&
                !SBNSearchDiskInternal( psSearch, nDepth + 1, nNodeId,
                                        bNodeMinX, bMid,
                                        bNodeMaxX, bNodeMaxY ) )
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                          compare_ints()                              */
/************************************************************************/

/* helper for qsort */
static int
compare_ints( const void * a, const void * b)
{
    return (*(int*)a) - (*(int*)b);
}

/************************************************************************/
/*                        SBNSearchDiskTree()                           */
/************************************************************************/

int* SBNSearchDiskTree( SBNSearchHandle hSBN,
                        double *padfBoundsMin, double *padfBoundsMax,
                        int *pnShapeCount )
{
    double dfMinX, dfMinY, dfMaxX, dfMaxY;
    double dfDiskXExtent, dfDiskYExtent;
    int bMinX, bMinY, bMaxX, bMaxY;

    *pnShapeCount = 0;

    dfMinX = padfBoundsMin[0];
    dfMinY = padfBoundsMin[1];
    dfMaxX = padfBoundsMax[0];
    dfMaxY = padfBoundsMax[1];

    if( dfMinX > dfMaxX || dfMinY > dfMaxY )
        return NULL;

    if( dfMaxX < hSBN->dfMinX || dfMaxY < hSBN->dfMinY ||
        dfMinX > hSBN->dfMaxX || dfMinY > hSBN->dfMaxY )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Compute the search coordinates in [0,255]x[0,255] coord. space  */
/* -------------------------------------------------------------------- */
    dfDiskXExtent = hSBN->dfMaxX - hSBN->dfMinX;
    dfDiskYExtent = hSBN->dfMaxY - hSBN->dfMinY;

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
            double dfMinX_255 = (dfMinX - hSBN->dfMinX)
                                                    / dfDiskXExtent * 255.0;
            bMinX = (int)floor(dfMinX_255 - 0.005);
            if( bMinX < 0 ) bMinX = 0;
        }

        if( dfMaxX > hSBN->dfMaxX )
            bMaxX = 255;
        else
        {
            double dfMaxX_255 = (dfMaxX - hSBN->dfMinX)
                                                    / dfDiskXExtent * 255.0;
            bMaxX = (int)ceil(dfMaxX_255 + 0.005);
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
            double dfMinY_255 = (dfMinY - hSBN->dfMinY)
                                                    / dfDiskYExtent * 255.0;
            bMinY = (int)floor(dfMinY_255 - 0.005);
            if( bMinY < 0 ) bMinY = 0;
        }

        if( dfMaxY > hSBN->dfMaxY )
            bMaxY = 255;
        else
        {
            double dfMaxY_255 = (dfMaxY - hSBN->dfMinY)
                                                    / dfDiskYExtent * 255.0;
            bMaxY = (int)ceil(dfMaxY_255 + 0.005);
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
                               int *pnShapeCount )
{
    SearchStruct sSearch;
    int bRet;

    *pnShapeCount = 0;

    if( bMinX > bMaxX || bMinY > bMaxY )
        return NULL;

    if( bMaxX < 0 || bMaxY < 0 || bMinX > 255 || bMinX > 255 )
        return NULL;

    if( hSBN->nShapeCount == 0 )
        return NULL;
/* -------------------------------------------------------------------- */
/*      Run the search.                                                 */
/* -------------------------------------------------------------------- */
    sSearch.hSBN = hSBN;
    sSearch.bMinX = (coord) (bMinX >= 0 ? bMinX : 0);
    sSearch.bMinY = (coord) (bMinY >= 0 ? bMinY : 0);
    sSearch.bMaxX = (coord) (bMaxX <= 255 ? bMaxX : 255);
    sSearch.bMaxY = (coord) (bMaxY <= 255 ? bMaxY : 255);
    sSearch.nShapeCount = 0;
    sSearch.nShapeAlloc = 0;
    sSearch.panShapeId = NULL;
#ifdef DEBUG_IO
    sSearch.nBytesRead = 0;
#endif

    bRet = SBNSearchDiskInternal(&sSearch, 0, 0, 0, 0, 255, 255);

#ifdef DEBUG_IO
    hSBN->nTotalBytesRead += sSearch.nBytesRead;
    /* printf("nBytesRead = %d\n", sSearch.nBytesRead); */
#endif

    if( !bRet )
    {
        if( sSearch.panShapeId != NULL )
            free( sSearch.panShapeId );
        *pnShapeCount = 0;
        return NULL;
    }

    *pnShapeCount = sSearch.nShapeCount;

/* -------------------------------------------------------------------- */
/*      Sort the id array                                               */
/* -------------------------------------------------------------------- */
    qsort(sSearch.panShapeId, *pnShapeCount, sizeof(int), compare_ints);

    /* To distinguish between empty intersection from error case */
    if( sSearch.panShapeId == NULL )
        sSearch.panShapeId = (int*) calloc(1, sizeof(int));

    return sSearch.panShapeId;
}

/************************************************************************/
/*                         SBNSearchFreeIds()                           */
/************************************************************************/

void SBNSearchFreeIds( int* panShapeId )
{
    free( panShapeId );
}
