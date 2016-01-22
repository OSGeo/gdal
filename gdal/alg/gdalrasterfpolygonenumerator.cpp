/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Version of Raster Polygon Enumerator using float buffers.
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com. Most of the code
 * taken from GDALRasterFPolygonEnumerator, by Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Jorge Arevalo
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

#ifdef OGR_ENABLED
/************************************************************************/
/*                    GDALRasterFPolygonEnumerator()                     */
/************************************************************************/

GDALRasterFPolygonEnumerator::GDALRasterFPolygonEnumerator(
    int nConnectedness )

{
    panPolyIdMap = NULL;
    pafPolyValue = NULL;
    nNextPolygonId = 0;
    nPolyAlloc = 0;
    this->nConnectedness = nConnectedness;
    CPLAssert( nConnectedness == 4 || nConnectedness == 8 );
}

/************************************************************************/
/*                    ~GDALRasterFPolygonEnumerator()                    */
/************************************************************************/

GDALRasterFPolygonEnumerator::~GDALRasterFPolygonEnumerator()

{
    Clear();
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

void GDALRasterFPolygonEnumerator::Clear()

{
    CPLFree( panPolyIdMap );
    CPLFree( pafPolyValue );
    
    panPolyIdMap = NULL;
    pafPolyValue = NULL;
    
    nNextPolygonId = 0;
    nPolyAlloc = 0;
}

/************************************************************************/
/*                            MergePolygon()                            */
/*                                                                      */
/*      Update the polygon map to indicate the merger of two polygons.  */
/************************************************************************/

void GDALRasterFPolygonEnumerator::MergePolygon( int nSrcId, int nDstId )

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

int GDALRasterFPolygonEnumerator::NewPolygon( float fValue )

{
    int nPolyId = nNextPolygonId;

    if( nNextPolygonId >= nPolyAlloc )
    {
        nPolyAlloc = nPolyAlloc * 2 + 20;
        panPolyIdMap = (GInt32 *) CPLRealloc(panPolyIdMap,nPolyAlloc*4);
        pafPolyValue = (float *) CPLRealloc(pafPolyValue,nPolyAlloc*4);
    }

    nNextPolygonId++;

    panPolyIdMap[nPolyId] = nPolyId;
    pafPolyValue[nPolyId] = fValue;

    return nPolyId;
}

/************************************************************************/
/*                           CompleteMerges()                           */
/*                                                                      */
/*      Make a pass through the maps, ensuring every polygon id         */
/*      points to the final id it should use, not an intermediate       */
/*      value.                                                          */
/************************************************************************/

void GDALRasterFPolygonEnumerator::CompleteMerges()

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

    CPLDebug( "GDALRasterFPolygonEnumerator",
              "Counted %d polygon fragments forming %d final polygons.", 
              nNextPolygonId, nFinalPolyCount );
}

/************************************************************************/
/*                            ProcessLine()                             */
/*                                                                      */
/*      Assign ids to polygons, one line at a time.                     */
/************************************************************************/

void GDALRasterFPolygonEnumerator::ProcessLine(
    float *pafLastLineVal, float *pafThisLineVal,
    GInt32 *panLastLineId,  GInt32 *panThisLineId,
    int nXSize )

{
    int i;

/* -------------------------------------------------------------------- */
/*      Special case for the first line.                                */
/* -------------------------------------------------------------------- */
    if( pafLastLineVal == NULL )
    {
        for( i=0; i < nXSize; i++ )
        {
            if( i == 0 || !GDALFloatEquals(pafThisLineVal[i], pafThisLineVal[i-1]) )
            {
                panThisLineId[i] = NewPolygon( pafThisLineVal[i] );
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
        if( i > 0 && GDALFloatEquals(pafThisLineVal[i], pafThisLineVal[i-1]) )
        {
            panThisLineId[i] = panThisLineId[i-1];        

            if( GDALFloatEquals(pafLastLineVal[i], pafThisLineVal[i])
                && (panPolyIdMap[panLastLineId[i]]
                    != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i], panThisLineId[i] );
            }

            if( nConnectedness == 8 
                && pafLastLineVal[i-1] == pafThisLineVal[i] 
                && (panPolyIdMap[panLastLineId[i-1]]
                    != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i-1], panThisLineId[i] );
            }

            if( nConnectedness == 8 && i < nXSize-1 
                && pafLastLineVal[i+1] == pafThisLineVal[i] 
                && (panPolyIdMap[panLastLineId[i+1]]
                    != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i+1], panThisLineId[i] );
            }
        }
        else if( GDALFloatEquals(pafLastLineVal[i], pafThisLineVal[i]) )
        {
            panThisLineId[i] = panLastLineId[i];
        }
        else if( i > 0 && nConnectedness == 8 
                 && GDALFloatEquals(pafLastLineVal[i-1], pafThisLineVal[i]) )
        {
            panThisLineId[i] = panLastLineId[i-1];

            if( i < nXSize-1 && pafLastLineVal[i+1] == pafThisLineVal[i]
                && (panPolyIdMap[panLastLineId[i+1]]
                != panPolyIdMap[panThisLineId[i]]) )
            {
                MergePolygon( panLastLineId[i+1], panThisLineId[i] );
            }
        }
        else if( i < nXSize-1 && nConnectedness == 8 
                 && GDALFloatEquals(pafLastLineVal[i+1], pafThisLineVal[i]) )
        {
            panThisLineId[i] = panLastLineId[i+1];
        }
        else
            panThisLineId[i] = 
                NewPolygon( pafThisLineVal[i] );
    }
}
#endif
