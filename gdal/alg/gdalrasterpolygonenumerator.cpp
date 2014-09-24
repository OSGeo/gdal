/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Raster Polygon Enumerator
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
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

#include "gdal_alg_priv.h"
#include "cpl_conv.h"
#include <vector>

CPL_CVSID("$Id$");

/************************************************************************/
/*                    GDALRasterPolygonEnumerator()                     */
/************************************************************************/

GDALRasterPolygonEnumerator::GDALRasterPolygonEnumerator( 
    int nConnectedness )

{
    panPolyIdMap = NULL;
    panPolyValue = NULL;
    nNextPolygonId = 0;
    nPolyAlloc = 0;
    this->nConnectedness = nConnectedness;
    CPLAssert( nConnectedness == 4 || nConnectedness == 8 );
}

/************************************************************************/
/*                    ~GDALRasterPolygonEnumerator()                    */
/************************************************************************/

GDALRasterPolygonEnumerator::~GDALRasterPolygonEnumerator()

{
    Clear();
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

void GDALRasterPolygonEnumerator::Clear()

{
    CPLFree( panPolyIdMap );
    CPLFree( panPolyValue );
    
    panPolyIdMap = NULL;
    panPolyValue = NULL;
    
    nNextPolygonId = 0;
    nPolyAlloc = 0;
}

/************************************************************************/
/*                            MergePolygon()                            */
/*                                                                      */
/*      Update the polygon map to indicate the merger of two polygons.  */
/************************************************************************/

void GDALRasterPolygonEnumerator::MergePolygon( int nSrcId, int nDstId )

{
    while( panPolyIdMap[nDstId] != nDstId )
        nDstId = panPolyIdMap[nDstId];

    while( panPolyIdMap[nSrcId] != nSrcId )
        nSrcId = panPolyIdMap[nSrcId];

    if( nSrcId == nDstId )
        return;

    panPolyIdMap[nSrcId] = nDstId;
}

/************************************************************************/
/*                             NewPolygon()                             */
/*                                                                      */
/*      Allocate a new polygon id, and reallocate the polygon maps      */
/*      if needed.                                                      */
/************************************************************************/

int GDALRasterPolygonEnumerator::NewPolygon( GInt32 nValue )

{
    int nPolyId = nNextPolygonId;

    if( nNextPolygonId >= nPolyAlloc )
    {
        nPolyAlloc = nPolyAlloc * 2 + 20;
        panPolyIdMap = (GInt32 *) CPLRealloc(panPolyIdMap,nPolyAlloc*4);
        panPolyValue = (GInt32 *) CPLRealloc(panPolyValue,nPolyAlloc*4);
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

void GDALRasterPolygonEnumerator::CompleteMerges()

{
    int iPoly;
    int nFinalPolyCount = 0;

    for( iPoly = 0; iPoly < nNextPolygonId; iPoly++ )
    {
        while( panPolyIdMap[iPoly] 
               != panPolyIdMap[panPolyIdMap[iPoly]] )
            panPolyIdMap[iPoly] = panPolyIdMap[panPolyIdMap[iPoly]];

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

void GDALRasterPolygonEnumerator::ProcessLine( 
    GInt32 *panLastLineVal, GInt32 *panThisLineVal,
    GInt32 *panLastLineId,  GInt32 *panThisLineId,
    int nXSize )

{
    int i;

/* -------------------------------------------------------------------- */
/*      Special case for the first line.                                */
/* -------------------------------------------------------------------- */
    if( panLastLineVal == NULL )
    {
        for( i=0; i < nXSize; i++ )
        {
            if( i == 0 || panThisLineVal[i] != panThisLineVal[i-1] )
            {
                panThisLineId[i] = NewPolygon( panThisLineVal[i] );
            }
            else
                panThisLineId[i] = panThisLineId[i-1];
        }

        return;
    }

/* -------------------------------------------------------------------- */
/*      Process each pixel comparing to the previous pixel, and to      */
/*      the last line.                                                  */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nXSize; i++ )
    {
        if( i > 0 && panThisLineVal[i] == panThisLineVal[i-1] )
        {
            panThisLineId[i] = panThisLineId[i-1];        

            if( panLastLineVal[i] == panThisLineVal[i] 
                && (panPolyIdMap[panLastLineId[i]]
                    != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i], panThisLineId[i] );
            }

            if( nConnectedness == 8 
                && panLastLineVal[i-1] == panThisLineVal[i] 
                && (panPolyIdMap[panLastLineId[i-1]]
                    != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i-1], panThisLineId[i] );
            }

            if( nConnectedness == 8 && i < nXSize-1 
                && panLastLineVal[i+1] == panThisLineVal[i] 
                && (panPolyIdMap[panLastLineId[i+1]]
                    != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i+1], panThisLineId[i] );
            }
        }
        else if( panLastLineVal[i] == panThisLineVal[i] )
        {
            panThisLineId[i] = panLastLineId[i];
        }
        else if( i > 0 && nConnectedness == 8 
                 && panLastLineVal[i-1] == panThisLineVal[i] )
        {
            panThisLineId[i] = panLastLineId[i-1];

            if( i < nXSize-1 && panLastLineVal[i+1] == panThisLineVal[i]
                && (panPolyIdMap[panLastLineId[i+1]]
                != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i+1], panThisLineId[i] );
            }
        }
        else if( i < nXSize-1 && nConnectedness == 8 
                 && panLastLineVal[i+1] == panThisLineVal[i] )
        {
            panThisLineId[i] = panLastLineId[i+1];
        }
        else
            panThisLineId[i] = 
                NewPolygon( panThisLineVal[i] );
    }
}

