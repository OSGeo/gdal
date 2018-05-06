/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Raster Polygon Enumerator
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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
#include "gdal_alg_priv.h"

#include <cstddef>

#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                    GDALRasterPolygonEnumeratorT()                    */
/************************************************************************/

template<class DataType, class EqualityTest>
GDALRasterPolygonEnumeratorT<DataType,
                             EqualityTest>::GDALRasterPolygonEnumeratorT(
    int nConnectednessIn ) :
    nConnectedness( nConnectednessIn )

{
    CPLAssert( nConnectedness == 4 || nConnectedness == 8 );
}

/************************************************************************/
/*                    ~GDALRasterPolygonEnumeratorT()                    */
/************************************************************************/

template<class DataType, class EqualityTest>
GDALRasterPolygonEnumeratorT<DataType,
                             EqualityTest>::~GDALRasterPolygonEnumeratorT()

{
    Clear();
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

template<class DataType, class EqualityTest>
void GDALRasterPolygonEnumeratorT<DataType, EqualityTest>::Clear()

{
    CPLFree( panPolyIdMap );
    CPLFree( panPolyValue );

    panPolyIdMap = nullptr;
    panPolyValue = nullptr;

    nNextPolygonId = 0;
    nPolyAlloc = 0;
}

/************************************************************************/
/*                            MergePolygon()                            */
/*                                                                      */
/*      Update the polygon map to indicate the merger of two polygons.  */
/************************************************************************/

template<class DataType, class EqualityTest>
void GDALRasterPolygonEnumeratorT<DataType,
                                  EqualityTest>::MergePolygon( int nSrcId,
                                                               int nDstIdInit )

{
    // Figure out the final dest id.
    int nDstIdFinal = nDstIdInit;
    while( panPolyIdMap[nDstIdFinal] != nDstIdFinal )
        nDstIdFinal = panPolyIdMap[nDstIdFinal];

    // Map the whole intermediate chain to it.
    int nDstIdCur = nDstIdInit;
    while( panPolyIdMap[nDstIdCur] != nDstIdCur )
    {
        int nNextDstId = panPolyIdMap[nDstIdCur];
        panPolyIdMap[nDstIdCur] = nDstIdFinal;
        nDstIdCur = nNextDstId;
    }

    // And map the whole source chain to it too (can be done in one pass).
    while( panPolyIdMap[nSrcId] != nSrcId )
    {
        int nNextSrcId = panPolyIdMap[nSrcId];
        panPolyIdMap[nSrcId] = nDstIdFinal;
        nSrcId = nNextSrcId;
    }
    panPolyIdMap[nSrcId] = nDstIdFinal;
}

/************************************************************************/
/*                             NewPolygon()                             */
/*                                                                      */
/*      Allocate a new polygon id, and reallocate the polygon maps      */
/*      if needed.                                                      */
/************************************************************************/

template<class DataType, class EqualityTest>
int GDALRasterPolygonEnumeratorT<DataType, EqualityTest>::NewPolygon(
    DataType nValue )

{
    const int nPolyId = nNextPolygonId;

    if( nNextPolygonId >= nPolyAlloc )
    {
        nPolyAlloc = nPolyAlloc * 2 + 20;
        panPolyIdMap = static_cast<GInt32 *>(
            CPLRealloc(panPolyIdMap, nPolyAlloc*sizeof(GInt32)));
        panPolyValue = static_cast<DataType *>(
            CPLRealloc(panPolyValue, nPolyAlloc*sizeof(DataType)));
    }

    nNextPolygonId++;

    panPolyIdMap[nPolyId] = nPolyId;
    panPolyValue[nPolyId] = nValue;

    return nPolyId;
}

/************************************************************************/
/*                           CompleteMerges()                           */
/*                                                                      */
/*      Make a pass through the maps, ensuring every polygon id         */
/*      points to the final id it should use, not an intermediate       */
/*      value.                                                          */
/************************************************************************/

template<class DataType, class EqualityTest>
void GDALRasterPolygonEnumeratorT<DataType, EqualityTest>::CompleteMerges()

{
    int nFinalPolyCount = 0;

    for( int iPoly = 0; iPoly < nNextPolygonId; iPoly++ )
    {
        // Figure out the final id.
        int nId = panPolyIdMap[iPoly];
        while( nId != panPolyIdMap[nId] )
        {
            nId = panPolyIdMap[nId];
        }

        // Then map the whole intermediate chain to it.
        int nIdCur = panPolyIdMap[iPoly];
        panPolyIdMap[iPoly] = nId;
        while( nIdCur != panPolyIdMap[nIdCur] )
        {
            int nNextId = panPolyIdMap[nIdCur];
            panPolyIdMap[nIdCur] = nId;
            nIdCur = nNextId;
        }

        if( panPolyIdMap[iPoly] == iPoly )
            nFinalPolyCount++;
    }

    CPLDebug( "GDALRasterPolygonEnumerator",
              "Counted %d polygon fragments forming %d final polygons.",
              nNextPolygonId, nFinalPolyCount );
}

/************************************************************************/
/*                            ProcessLine()                             */
/*                                                                      */
/*      Assign ids to polygons, one line at a time.                     */
/************************************************************************/

template<class DataType, class EqualityTest>
void GDALRasterPolygonEnumeratorT<DataType, EqualityTest>::ProcessLine(
    DataType *panLastLineVal, DataType *panThisLineVal,
    GInt32 *panLastLineId,  GInt32 *panThisLineId,
    int nXSize )

{
    EqualityTest eq;

/* -------------------------------------------------------------------- */
/*      Special case for the first line.                                */
/* -------------------------------------------------------------------- */
    if( panLastLineVal == nullptr )
    {
        for( int i = 0; i < nXSize; i++ )
        {
            if( panThisLineVal[i] == GP_NODATA_MARKER )
            {
                panThisLineId[i] = -1;
            }
            else if( i == 0 ||
                     !(eq.operator()(panThisLineVal[i], panThisLineVal[i-1])) )
            {
                panThisLineId[i] = NewPolygon( panThisLineVal[i] );
            }
            else
            {
                panThisLineId[i] = panThisLineId[i-1];
            }
        }

        return;
    }

/* -------------------------------------------------------------------- */
/*      Process each pixel comparing to the previous pixel, and to      */
/*      the last line.                                                  */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nXSize; i++ )
    {
        if( panThisLineVal[i] == GP_NODATA_MARKER )
        {
            panThisLineId[i] = -1;
        }
        else if( i > 0 &&
                 eq.operator()(panThisLineVal[i], panThisLineVal[i-1]) )
        {
            panThisLineId[i] = panThisLineId[i-1];

            if( eq.operator()(panLastLineVal[i], panThisLineVal[i])
                && (panPolyIdMap[panLastLineId[i]]
                    != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i], panThisLineId[i] );
            }

            if( nConnectedness == 8
                && eq.operator()(panLastLineVal[i-1], panThisLineVal[i])
                && (panPolyIdMap[panLastLineId[i-1]]
                    != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i-1], panThisLineId[i] );
            }

            if( nConnectedness == 8 && i < nXSize-1
                && eq.operator()(panLastLineVal[i+1], panThisLineVal[i])
                && (panPolyIdMap[panLastLineId[i+1]]
                    != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i+1], panThisLineId[i] );
            }
        }
        else if( eq.operator()(panLastLineVal[i], panThisLineVal[i]) )
        {
            panThisLineId[i] = panLastLineId[i];
        }
        else if( i > 0 && nConnectedness == 8
                 && eq.operator()(panLastLineVal[i-1], panThisLineVal[i]) )
        {
            panThisLineId[i] = panLastLineId[i-1];

            if( i < nXSize-1 &&
                eq.operator()(panLastLineVal[i+1], panThisLineVal[i]) &&
                (panPolyIdMap[panLastLineId[i+1]]
                 != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i+1], panThisLineId[i] );
            }
        }
        else if( i < nXSize-1 && nConnectedness == 8
                 && eq.operator()(panLastLineVal[i+1], panThisLineVal[i]) )
        {
            panThisLineId[i] = panLastLineId[i+1];
        }
        else
        {
            panThisLineId[i] = NewPolygon(panThisLineVal[i]);
        }
    }
}

template class GDALRasterPolygonEnumeratorT<GInt32, IntEqualityTest>;

template class GDALRasterPolygonEnumeratorT<float, FloatEqualityTest>;

/*! @endcond */
