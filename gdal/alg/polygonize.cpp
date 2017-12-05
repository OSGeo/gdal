/******************************************************************************
 * Project:  GDAL
 * Purpose:  Raster to Polygon Converter
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdal_alg.h"

#include <stddef.h>
#include <stdio.h>
#include <cstdlib>
#include <string.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "gdal_alg_priv.h"
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                               RPolygon                               */
/*                                                                      */
/*      This is a helper class to hold polygons while they are being    */
/*      formed in memory, and to provide services to coalesce a much    */
/*      of edge sections into complete rings.                           */
/* ==================================================================== */
/************************************************************************/

class RPolygon {
public:
    explicit RPolygon( double dfValue )
        { dfPolyValue = dfValue; nLastLineUpdated = -1; }

    double           dfPolyValue;
    int              nLastLineUpdated;

    std::vector< std::vector<int> > aanXY;

    void             AddSegment( int x1, int y1, int x2, int y2 );
    void             Dump() const;
    void             Coalesce();
    void             Merge( int iBaseString, int iSrcString, int iDirection );
};

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/
void RPolygon::Dump() const
{
    /*ok*/printf( "RPolygon: Value=%g, LastLineUpdated=%d\n",
            dfPolyValue, nLastLineUpdated );

    for( size_t iString = 0; iString < aanXY.size(); iString++ )
    {
        const std::vector<int> &anString = aanXY[iString];

        /*ok*/printf( "  String %d:\n", (int) iString );
        for( size_t iVert = 0; iVert < anString.size(); iVert += 2 )
        {
            /*ok*/printf( "    (%d,%d)\n", anString[iVert], anString[iVert+1] );
        }
    }
}

/************************************************************************/
/*                              Coalesce()                              */
/************************************************************************/

void RPolygon::Coalesce()

{
/* -------------------------------------------------------------------- */
/*      Iterate over loops starting from the first, trying to merge     */
/*      other segments into them.                                       */
/* -------------------------------------------------------------------- */
    for( size_t iBaseString = 0; iBaseString < aanXY.size(); iBaseString++ )
    {
        std::vector<int> &anBase = aanXY[iBaseString];
        bool bMergeHappened = true;

/* -------------------------------------------------------------------- */
/*      Keep trying to merge the following strings into our target      */
/*      "base" string till we have tried them all once without any      */
/*      mergers.                                                        */
/* -------------------------------------------------------------------- */
        while( bMergeHappened )
        {
            bMergeHappened = false;

/* -------------------------------------------------------------------- */
/*      Loop over the following strings, trying to find one we can      */
/*      merge onto the end of our base string.                          */
/* -------------------------------------------------------------------- */
            for( size_t iString = iBaseString+1;
                 iString < aanXY.size();
                 iString++ )
            {
                std::vector<int> &anString = aanXY[iString];

                if( anBase[anBase.size() - 2] == anString[0]
                    && anBase.back() == anString[1] )
                {
                    Merge( static_cast<int>(iBaseString),
                           static_cast<int>(iString), 1 );
                    bMergeHappened = true;
                }
                else if( anBase[anBase.size() - 2] ==
                             anString[anString.size() - 2] &&
                         anBase.back() ==
                             anString.back() )
                {
                    Merge( static_cast<int>(iBaseString),
                           static_cast<int>(iString), -1 );
                    bMergeHappened = true;
                }
            }
        }

        // At this point our loop *should* be closed!
        CPLAssert( anBase[0] == anBase[anBase.size()-2]
                   && anBase[1] == anBase.back() );
    }
}

/************************************************************************/
/*                               Merge()                                */
/************************************************************************/

void RPolygon::Merge( int iBaseString, int iSrcString, int iDirection )

{
    std::vector<int> &anBase = aanXY[iBaseString];
    std::vector<int> &anString = aanXY[iSrcString];
    int iStart = 1;
    int iEnd = -1;

    if( iDirection == 1 )
    {
        iEnd = static_cast<int>(anString.size()) / 2;
    }
    else
    {
        iStart = static_cast<int>(anString.size()) / 2 - 2;
    }

    for( int i = iStart; i != iEnd; i += iDirection )
    {
        anBase.push_back( anString[i*2+0] );
        anBase.push_back( anString[i*2+1] );
    }

    if( iSrcString < static_cast<int>(aanXY.size()) - 1 )
        aanXY[iSrcString] = aanXY[aanXY.size()-1];

    const size_t nSize = aanXY.size();
    aanXY.resize(nSize - 1);
}

/************************************************************************/
/*                             AddSegment()                             */
/************************************************************************/

void RPolygon::AddSegment( int x1, int y1, int x2, int y2 )

{
    nLastLineUpdated = std::max(y1, y2);

/* -------------------------------------------------------------------- */
/*      Is there an existing string ending with this?                   */
/* -------------------------------------------------------------------- */
    for( size_t iString = 0; iString < aanXY.size(); iString++ )
    {
        std::vector<int> &anString = aanXY[iString];
        const size_t nSSize = anString.size();

        if( anString[nSSize-2] == x1
            && anString[nSSize-1] == y1 )
        {
            std::swap(x1, x2);
            std::swap(y1, y2);
        }

        if( anString[nSSize - 2] == x2 &&
            anString[nSSize - 1] == y2 )
        {
            // We are going to add a segment, but should we just extend
            // an existing segment already going in the right direction?

            const int nLastLen =
                std::max(std::abs(anString[nSSize - 4] - anString[nSSize - 2]),
                         std::abs(anString[nSSize - 3] - anString[nSSize - 1]));

            if( nSSize >= 4
                && (anString[nSSize - 4] - anString[nSSize - 2]
                    == (anString[nSSize - 2] - x1) * nLastLen)
                && (anString[nSSize - 3] - anString[nSSize - 1]
                    == (anString[nSSize - 1] - y1) * nLastLen) )
            {
                anString.pop_back();
                anString.pop_back();
            }

            anString.push_back( x1 );
            anString.push_back( y1 );
            return;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a new string.                                            */
/* -------------------------------------------------------------------- */
    const size_t nSize = aanXY.size();
    aanXY.resize(nSize + 1);
    std::vector<int> &anString = aanXY[nSize];

    anString.push_back( x1 );
    anString.push_back( y1 );
    anString.push_back( x2 );
    anString.push_back( y2 );

    return;
}

/************************************************************************/
/* ==================================================================== */
/*     End of RPolygon                                                  */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                              AddEdges()                              */
/*                                                                      */
/*      Examine one pixel and compare to its neighbour above            */
/*      (previous) and right.  If they are different polygon ids        */
/*      then add the pixel edge to this polygon and the one on the      */
/*      other side of the edge.                                         */
/************************************************************************/

template<class DataType>
static void AddEdges( GInt32 *panThisLineId, GInt32 *panLastLineId,
                      GInt32 *panPolyIdMap, DataType *panPolyValue,
                      RPolygon **papoPoly, int iX, int iY )

{
    // TODO(schwehr): Simplify these three vars.
    int nThisId = panThisLineId[iX];
    if( nThisId != -1 )
        nThisId = panPolyIdMap[nThisId];
    int nRightId = panThisLineId[iX+1];
    if( nRightId != -1 )
        nRightId = panPolyIdMap[nRightId];
    int nPreviousId = panLastLineId[iX];
    if( nPreviousId != -1 )
        nPreviousId = panPolyIdMap[nPreviousId];

    const int iXReal = iX - 1;

    if( nThisId != nPreviousId )
    {
        if( nThisId != -1 )
        {
            if( papoPoly[nThisId] == NULL )
                papoPoly[nThisId] = new RPolygon( panPolyValue[nThisId] );

            papoPoly[nThisId]->AddSegment( iXReal, iY, iXReal+1, iY );
        }
        if( nPreviousId != -1 )
        {
            if( papoPoly[nPreviousId] == NULL )
                papoPoly[nPreviousId] = new RPolygon(panPolyValue[nPreviousId]);

            papoPoly[nPreviousId]->AddSegment( iXReal, iY, iXReal+1, iY );
        }
    }

    if( nThisId != nRightId )
    {
        if( nThisId != -1 )
        {
            if( papoPoly[nThisId] == NULL )
                papoPoly[nThisId] = new RPolygon(panPolyValue[nThisId]);

            papoPoly[nThisId]->AddSegment( iXReal+1, iY, iXReal+1, iY+1 );
        }

        if( nRightId != -1 )
        {
            if( papoPoly[nRightId] == NULL )
                papoPoly[nRightId] = new RPolygon(panPolyValue[nRightId]);

            papoPoly[nRightId]->AddSegment( iXReal+1, iY, iXReal+1, iY+1 );
        }
    }
}

/************************************************************************/
/*                         EmitPolygonToLayer()                         */
/************************************************************************/

static CPLErr
EmitPolygonToLayer( OGRLayerH hOutLayer, int iPixValField,
                    RPolygon *poRPoly, double *padfGeoTransform )

{
/* -------------------------------------------------------------------- */
/*      Turn bits of lines into coherent rings.                         */
/* -------------------------------------------------------------------- */
    poRPoly->Coalesce();

/* -------------------------------------------------------------------- */
/*      Create the polygon geometry.                                    */
/* -------------------------------------------------------------------- */
    OGRGeometryH hPolygon = OGR_G_CreateGeometry( wkbPolygon );

    for( size_t iString = 0; iString < poRPoly->aanXY.size(); iString++ )
    {
        std::vector<int> &anString = poRPoly->aanXY[iString];
        OGRGeometryH hRing = OGR_G_CreateGeometry( wkbLinearRing );

        // We go last to first to ensure the linestring is allocated to
        // the proper size on the first try.
        for( int iVert = static_cast<int>(anString.size()) / 2 - 1;
             iVert >= 0;
             iVert-- )
        {
            const int nPixelX = anString[iVert*2];
            const int nPixelY = anString[iVert*2+1];

            const double dfX =
                padfGeoTransform[0]
                + nPixelX * padfGeoTransform[1]
                + nPixelY * padfGeoTransform[2];
            const double dfY =
                padfGeoTransform[3]
                + nPixelX * padfGeoTransform[4]
                + nPixelY * padfGeoTransform[5];

            OGR_G_SetPoint_2D( hRing, iVert, dfX, dfY );
        }

        OGR_G_AddGeometryDirectly( hPolygon, hRing );
    }

/* -------------------------------------------------------------------- */
/*      Create the feature object.                                      */
/* -------------------------------------------------------------------- */
    OGRFeatureH hFeat = OGR_F_Create( OGR_L_GetLayerDefn( hOutLayer ) );

    OGR_F_SetGeometryDirectly( hFeat, hPolygon );

    if( iPixValField >= 0 )
        OGR_F_SetFieldDouble( hFeat, iPixValField, poRPoly->dfPolyValue );

/* -------------------------------------------------------------------- */
/*      Write the to the layer.                                         */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( OGR_L_CreateFeature( hOutLayer, hFeat ) != OGRERR_NONE )
        eErr = CE_Failure;

    OGR_F_Destroy( hFeat );

    return eErr;
}

/************************************************************************/
/*                          GPMaskImageData()                           */
/*                                                                      */
/*      Mask out image pixels to a special nodata value if the mask     */
/*      band is zero.                                                   */
/************************************************************************/

template<class DataType>
static CPLErr
GPMaskImageData( GDALRasterBandH hMaskBand, GByte* pabyMaskLine,
                 int iY, int nXSize,
                 DataType *panImageLine )

{
    const CPLErr eErr =
        GDALRasterIO( hMaskBand, GF_Read, 0, iY, nXSize, 1,
                      pabyMaskLine, nXSize, 1, GDT_Byte, 0, 0 );
    if( eErr != CE_None )
        return eErr;

    for( int i = 0; i < nXSize; i++ )
    {
        if( pabyMaskLine[i] == 0 )
            panImageLine[i] = GP_NODATA_MARKER;
    }

    return CE_None;
}

/************************************************************************/
/*                           GDALPolygonizeT()                          */
/************************************************************************/

template<class DataType, class EqualityTest>
static CPLErr
GDALPolygonizeT( GDALRasterBandH hSrcBand,
                 GDALRasterBandH hMaskBand,
                 OGRLayerH hOutLayer, int iPixValField,
                 char **papszOptions,
                 GDALProgressFunc pfnProgress,
                 void * pProgressArg,
                 GDALDataType eDT)

{
    VALIDATE_POINTER1( hSrcBand, "GDALPolygonize", CE_Failure );
    VALIDATE_POINTER1( hOutLayer, "GDALPolygonize", CE_Failure );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    const int nConnectedness =
        CSLFetchNameValue( papszOptions, "8CONNECTED" ) ? 8 : 4;

/* -------------------------------------------------------------------- */
/*      Confirm our output layer will support feature creation.         */
/* -------------------------------------------------------------------- */
    if( !OGR_L_TestCapability( hOutLayer, OLCSequentialWrite ) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Output feature layer does not appear to support creation "
                 "of features in GDALPolygonize().");
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate working buffers.                                       */
/* -------------------------------------------------------------------- */
    const int nXSize = GDALGetRasterBandXSize( hSrcBand );
    const int nYSize = GDALGetRasterBandYSize( hSrcBand );

    DataType *panLastLineVal = static_cast<DataType *>(
        VSI_MALLOC2_VERBOSE(sizeof(DataType), nXSize + 2));
    DataType *panThisLineVal = static_cast<DataType *>(
        VSI_MALLOC2_VERBOSE(sizeof(DataType), nXSize + 2));
    GInt32 *panLastLineId = static_cast<GInt32 *>(
        VSI_MALLOC2_VERBOSE(sizeof(GInt32), nXSize + 2));
    GInt32 *panThisLineId = static_cast<GInt32 *>(
        VSI_MALLOC2_VERBOSE(sizeof(GInt32), nXSize + 2));

    GByte *pabyMaskLine =
        hMaskBand != NULL
        ? static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize))
        : NULL;

    if( panLastLineVal == NULL || panThisLineVal == NULL ||
        panLastLineId == NULL || panThisLineId == NULL ||
        (hMaskBand != NULL && pabyMaskLine == NULL) )
    {
        CPLFree( panThisLineId );
        CPLFree( panLastLineId );
        CPLFree( panThisLineVal );
        CPLFree( panLastLineVal );
        CPLFree( pabyMaskLine );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get the geotransform, if there is one, so we can convert the    */
/*      vectors into georeferenced coordinates.                         */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = { 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

    const char* pszDatasetForGeoRef = CSLFetchNameValue(papszOptions,
                                                        "DATASET_FOR_GEOREF");
    if( pszDatasetForGeoRef )
    {
        GDALDatasetH hSrcDS = GDALOpen(pszDatasetForGeoRef, GA_ReadOnly);
        if( hSrcDS )
        {
            GDALGetGeoTransform( hSrcDS, adfGeoTransform );
            GDALClose(hSrcDS);
        }
    }
    else
    {
        GDALDatasetH hSrcDS = GDALGetBandDataset( hSrcBand );
        if( hSrcDS )
            GDALGetGeoTransform( hSrcDS, adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      The first pass over the raster is only used to build up the     */
/*      polygon id map so we will know in advance what polygons are     */
/*      what on the second pass.                                        */
/* -------------------------------------------------------------------- */
    GDALRasterPolygonEnumeratorT<DataType,
                                 EqualityTest> oFirstEnum(nConnectedness);

    CPLErr eErr = CE_None;

    for( int iY = 0; eErr == CE_None && iY < nYSize; iY++ )
    {
        eErr = GDALRasterIO(
            hSrcBand,
            GF_Read, 0, iY, nXSize, 1,
            panThisLineVal, nXSize, 1, eDT, 0, 0 );

        if( eErr == CE_None && hMaskBand != NULL )
            eErr = GPMaskImageData(hMaskBand, pabyMaskLine, iY, nXSize,
                                   panThisLineVal);

        if( iY == 0 )
            oFirstEnum.ProcessLine(
                NULL, panThisLineVal, NULL, panThisLineId, nXSize );
        else
            oFirstEnum.ProcessLine(
                panLastLineVal, panThisLineVal,
                panLastLineId,  panThisLineId,
                nXSize );

        // Swap lines.
        std::swap(panLastLineVal, panThisLineVal);
        std::swap(panLastLineId, panThisLineId);

/* -------------------------------------------------------------------- */
/*      Report progress, and support interrupts.                        */
/* -------------------------------------------------------------------- */
        if( eErr == CE_None
            && !pfnProgress( 0.10 * ((iY+1) / static_cast<double>(nYSize)),
                             "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Make a pass through the maps, ensuring every polygon id         */
/*      points to the final id it should use, not an intermediate       */
/*      value.                                                          */
/* -------------------------------------------------------------------- */
    oFirstEnum.CompleteMerges();

/* -------------------------------------------------------------------- */
/*      Initialize ids to -1 to serve as a nodata value for the         */
/*      previous line, and past the beginning and end of the            */
/*      scanlines.                                                      */
/* -------------------------------------------------------------------- */
    panThisLineId[0] = -1;
    panThisLineId[nXSize+1] = -1;

    for( int iX = 0; iX < nXSize+2; iX++ )
        panLastLineId[iX] = -1;

/* -------------------------------------------------------------------- */
/*      We will use a new enumerator for the second pass primarily      */
/*      so we can preserve the first pass map.                          */
/* -------------------------------------------------------------------- */
    GDALRasterPolygonEnumeratorT<DataType,
                                 EqualityTest> oSecondEnum(nConnectedness);
    RPolygon **papoPoly = static_cast<RPolygon **>(
        CPLCalloc(sizeof(RPolygon*), oFirstEnum.nNextPolygonId));

/* ==================================================================== */
/*      Second pass during which we will actually collect polygon       */
/*      edges as geometries.                                            */
/* ==================================================================== */
    for( int iY = 0; eErr == CE_None && iY < nYSize+1; iY++ )
    {
/* -------------------------------------------------------------------- */
/*      Read the image data.                                            */
/* -------------------------------------------------------------------- */
        if( iY < nYSize )
        {
            eErr = GDALRasterIO( hSrcBand, GF_Read, 0, iY, nXSize, 1,
                                 panThisLineVal, nXSize, 1, eDT, 0, 0 );

            if( eErr == CE_None && hMaskBand != NULL )
                eErr = GPMaskImageData( hMaskBand, pabyMaskLine, iY, nXSize,
                                        panThisLineVal );
        }

        if( eErr != CE_None )
            continue;

/* -------------------------------------------------------------------- */
/*      Determine what polygon the various pixels belong to (redoing    */
/*      the same thing done in the first pass above).                   */
/* -------------------------------------------------------------------- */
        if( iY == nYSize )
        {
            for( int iX = 0; iX < nXSize+2; iX++ )
                panThisLineId[iX] = -1;
        }
        else if( iY == 0 )
        {
            oSecondEnum.ProcessLine(
                NULL, panThisLineVal, NULL, panThisLineId+1, nXSize );
        }
        else
        {
            oSecondEnum.ProcessLine(
                panLastLineVal, panThisLineVal,
                panLastLineId+1,  panThisLineId+1,
                nXSize );
        }

/* -------------------------------------------------------------------- */
/*      Add polygon edges to our polygon list for the pixel             */
/*      boundaries within and above this line.                          */
/* -------------------------------------------------------------------- */
        for( int iX = 0; iX < nXSize+1; iX++ )
        {
            AddEdges( panThisLineId, panLastLineId,
                      oFirstEnum.panPolyIdMap, oFirstEnum.panPolyValue,
                      papoPoly, iX, iY );
        }

/* -------------------------------------------------------------------- */
/*      Periodically we scan out polygons and write out those that      */
/*      haven't been added to on the last line as we can be sure        */
/*      they are complete.                                              */
/* -------------------------------------------------------------------- */
        if( iY % 8 == 7 )
        {
            for( int iX = 0;
                 eErr == CE_None && iX < oSecondEnum.nNextPolygonId;
                 iX++ )
            {
                if( papoPoly[iX] && papoPoly[iX]->nLastLineUpdated < iY-1 )
                {
                    eErr =
                        EmitPolygonToLayer( hOutLayer, iPixValField,
                                            papoPoly[iX], adfGeoTransform );

                    delete papoPoly[iX];
                    papoPoly[iX] = NULL;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Swap pixel value, and polygon id lines to be ready for the      */
/*      next line.                                                      */
/* -------------------------------------------------------------------- */
        std::swap(panLastLineVal, panThisLineVal);
        std::swap(panLastLineId, panThisLineId);

/* -------------------------------------------------------------------- */
/*      Report progress, and support interrupts.                        */
/* -------------------------------------------------------------------- */
        if( eErr == CE_None
            && !pfnProgress( 0.10 + 0.90 * ((iY + 1) /
                                            static_cast<double>(nYSize)),
                             "", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Make a cleanup pass for all unflushed polygons.                 */
/* -------------------------------------------------------------------- */
    for( int iX = 0; eErr == CE_None && iX < oSecondEnum.nNextPolygonId; iX++ )
    {
        if( papoPoly[iX] )
        {
            eErr = EmitPolygonToLayer( hOutLayer, iPixValField,
                                       papoPoly[iX], adfGeoTransform );

            delete papoPoly[iX];
            papoPoly[iX] = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( panThisLineId );
    CPLFree( panLastLineId );
    CPLFree( panThisLineVal );
    CPLFree( panLastLineVal );
    CPLFree( pabyMaskLine );
    CPLFree( papoPoly );

    return eErr;
}

/******************************************************************************/
/*                          GDALFloatEquals()                                 */
/* Code from:                                                                 */
/* http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm  */
/******************************************************************************/
GBool GDALFloatEquals( float A, float B )
{
    // This function will allow maxUlps-1 floats between A and B.
    const int maxUlps = MAX_ULPS;

    // Make sure maxUlps is non-negative and small enough that the default NAN
    // won't compare as equal to anything.
#if MAX_ULPS <= 0 || MAX_ULPS >= 4 * 1024 * 1024
#error "Invalid MAX_ULPS"
#endif

    // This assignation could violate strict aliasing. It causes a warning with
    // gcc -O2. Use of memcpy preferred. Credits for Even Rouault. Further info
    // at http://trac.osgeo.org/gdal/ticket/4005#comment:6
    int aInt = 0;
    memcpy(&aInt, &A, 4);

    // Make aInt lexicographically ordered as a twos-complement int.
    if( aInt < 0 )
        aInt = INT_MIN - aInt;

    // Make bInt lexicographically ordered as a twos-complement int.
    int bInt = 0;
    memcpy(&bInt, &B, 4);

    if( bInt < 0 )
        bInt = INT_MIN - bInt;
#ifdef COMPAT_WITH_ICC_CONVERSION_CHECK
    const int intDiff =
        abs(static_cast<int>(static_cast<GUIntBig>(
            static_cast<GIntBig>(aInt) - static_cast<GIntBig>(bInt))
            & 0xFFFFFFFFU));
#else
    // To make -ftrapv happy we compute the diff on larger type and
    // cast down later.
    const int intDiff = abs(static_cast<int>(
        static_cast<GIntBig>(aInt) - static_cast<GIntBig>(bInt)));
#endif
    if( intDiff <= maxUlps )
        return true;
    return false;
}

/************************************************************************/
/*                           GDALPolygonize()                           */
/************************************************************************/

/**
 * Create polygon coverage from raster data.
 *
 * This function creates vector polygons for all connected regions of pixels in
 * the raster sharing a common pixel value.  Optionally each polygon may be
 * labeled with the pixel value in an attribute.  Optionally a mask band
 * can be provided to determine which pixels are eligible for processing.
 *
 * Note that currently the source pixel band values are read into a
 * signed 32bit integer buffer (Int32), so floating point or complex
 * bands will be implicitly truncated before processing. If you want to use a
 * version using 32bit float buffers, see GDALFPolygonize().
  *
 * Polygon features will be created on the output layer, with polygon
 * geometries representing the polygons.  The polygon geometries will be
 * in the georeferenced coordinate system of the image (based on the
 * geotransform of the source dataset).  It is acceptable for the output
 * layer to already have features.  Note that GDALPolygonize() does not
 * set the coordinate system on the output layer.  Application code should
 * do this when the layer is created, presumably matching the raster
 * coordinate system.
 *
 * The algorithm used attempts to minimize memory use so that very large
 * rasters can be processed.  However, if the raster has many polygons
 * or very large/complex polygons, the memory use for holding polygon
 * enumerations and active polygon geometries may grow to be quite large.
 *
 * The algorithm will generally produce very dense polygon geometries, with
 * edges that follow exactly on pixel boundaries for all non-interior pixels.
 * For non-thematic raster data (such as satellite images) the result will
 * essentially be one small polygon per pixel, and memory and output layer
 * sizes will be substantial.  The algorithm is primarily intended for
 * relatively simple thematic imagery, masks, and classification results.
 *
 * @param hSrcBand the source raster band to be processed.
 * @param hMaskBand an optional mask band.  All pixels in the mask band with a
 * value other than zero will be considered suitable for collection as
 * polygons.
 * @param hOutLayer the vector feature layer to which the polygons should
 * be written.
 * @param iPixValField the attribute field index indicating the feature
 * attribute into which the pixel value of the polygon should be written.
 * @param papszOptions a name/value list of additional options
 * <dl>
 * <dt>"8CONNECTED":</dt> May be set to "8" to use 8 connectedness.
 * Otherwise 4 connectedness will be applied to the algorithm
 * </dl>
 * @param pfnProgress callback for reporting algorithm progress matching the
 * GDALProgressFunc() semantics.  May be NULL.
 * @param pProgressArg callback argument passed to pfnProgress.
 *
 * @return CE_None on success or CE_Failure on a failure.
 */

CPLErr CPL_STDCALL
GDALPolygonize( GDALRasterBandH hSrcBand,
                GDALRasterBandH hMaskBand,
                OGRLayerH hOutLayer, int iPixValField,
                char **papszOptions,
                GDALProgressFunc pfnProgress,
                void * pProgressArg )

{
    return GDALPolygonizeT<GInt32, IntEqualityTest>(hSrcBand,
                                                    hMaskBand,
                                                    hOutLayer,
                                                    iPixValField,
                                                    papszOptions,
                                                    pfnProgress,
                                                    pProgressArg,
                                                    GDT_Int32);
}

/************************************************************************/
/*                           GDALFPolygonize()                           */
/************************************************************************/

/**
 * Create polygon coverage from raster data.
 *
 * This function creates vector polygons for all connected regions of pixels in
 * the raster sharing a common pixel value.  Optionally each polygon may be
 * labeled with the pixel value in an attribute.  Optionally a mask band
 * can be provided to determine which pixels are eligible for processing.
 *
 * The source pixel band values are read into a 32bit float buffer. If you want
 * to use a (probably faster) version using signed 32bit integer buffer, see
 * GDALPolygonize().
 *
 * Polygon features will be created on the output layer, with polygon
 * geometries representing the polygons.  The polygon geometries will be
 * in the georeferenced coordinate system of the image (based on the
 * geotransform of the source dataset).  It is acceptable for the output
 * layer to already have features.  Note that GDALFPolygonize() does not
 * set the coordinate system on the output layer.  Application code should
 * do this when the layer is created, presumably matching the raster
 * coordinate system.
 *
 * The algorithm used attempts to minimize memory use so that very large
 * rasters can be processed.  However, if the raster has many polygons
 * or very large/complex polygons, the memory use for holding polygon
 * enumerations and active polygon geometries may grow to be quite large.
 *
 * The algorithm will generally produce very dense polygon geometries, with
 * edges that follow exactly on pixel boundaries for all non-interior pixels.
 * For non-thematic raster data (such as satellite images) the result will
 * essentially be one small polygon per pixel, and memory and output layer
 * sizes will be substantial.  The algorithm is primarily intended for
 * relatively simple thematic imagery, masks, and classification results.
 *
 * @param hSrcBand the source raster band to be processed.
 * @param hMaskBand an optional mask band.  All pixels in the mask band with a
 * value other than zero will be considered suitable for collection as
 * polygons.
 * @param hOutLayer the vector feature layer to which the polygons should
 * be written.
 * @param iPixValField the attribute field index indicating the feature
 * attribute into which the pixel value of the polygon should be written.
 * @param papszOptions a name/value list of additional options
 * <dl>
 * <dt>"8CONNECTED":</dt> May be set to "8" to use 8 connectedness.
 * Otherwise 4 connectedness will be applied to the algorithm
 * </dl>
 * @param pfnProgress callback for reporting algorithm progress matching the
 * GDALProgressFunc() semantics.  May be NULL.
 * @param pProgressArg callback argument passed to pfnProgress.
 *
 * @return CE_None on success or CE_Failure on a failure.
 *
 * @since GDAL 1.9.0
 */

CPLErr CPL_STDCALL
GDALFPolygonize( GDALRasterBandH hSrcBand,
                GDALRasterBandH hMaskBand,
                OGRLayerH hOutLayer, int iPixValField,
                char **papszOptions,
                GDALProgressFunc pfnProgress,
                void * pProgressArg )

{
    return GDALPolygonizeT<float, FloatEqualityTest>(hSrcBand,
                                                    hMaskBand,
                                                    hOutLayer,
                                                    iPixValField,
                                                    papszOptions,
                                                    pfnProgress,
                                                    pProgressArg,
                                                    GDT_Float32);
}
