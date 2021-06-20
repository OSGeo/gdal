/******************************************************************************
 * Project:  GDAL
 * Purpose:  Raster to Polygon Converter
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright (c) 2009-2020, Even Rouault <even dot rouault at spatialys.com>
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
#include <map>
#include <memory>
#include <utility>
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
    double           dfPolyValue = 0.0;
    int              nLastLineUpdated = -1;

    struct XY
    {
        int x;
        int y;

        bool operator< (const XY& other) const
        {
            if( x < other.x )
                return true;
            if( x > other.x )
                return false;
            return y < other.y;
        }

        bool operator== (const XY& other) const { return x == other.x && y == other.y; }
    };

    typedef int StringId;
    typedef std::map< XY, std::pair<StringId, StringId>> MapExtremity;

    std::map< StringId, std::vector<XY> > oMapStrings{};
    MapExtremity oMapStartStrings{};
    MapExtremity oMapEndStrings{};
    StringId iNextStringId = 0;

    static
    StringId findExtremityNot(const MapExtremity& oMap,
                              const XY& xy,
                              StringId excludedId);

    static void removeExtremity(MapExtremity& oMap,
                                const XY& xy,
                                StringId id);

    static void insertExtremity(MapExtremity& oMap,
                                const XY& xy,
                                StringId id);

    explicit RPolygon( double dfValue ): dfPolyValue(dfValue) {}

    void             AddSegment( int x1, int y1, int x2, int y2 );
    void             Dump() const;
    void             Coalesce();
    void             Merge( StringId iBaseString, StringId iSrcString, int iDirection );
};

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/
void RPolygon::Dump() const
{
    /*ok*/printf( "RPolygon: Value=%g, LastLineUpdated=%d\n",
            dfPolyValue, nLastLineUpdated );

    for( const auto& oStringIter: oMapStrings )
    {
        /*ok*/printf( "  String " CPL_FRMT_GIB ":\n", static_cast<GIntBig>(oStringIter.first) );
        for( const auto& xy: oStringIter.second )
        {
            /*ok*/printf( "    (%d,%d)\n", xy.x, xy.y );
        }
    }
}

/************************************************************************/
/*                           findExtremityNot()                         */
/************************************************************************/

RPolygon::StringId RPolygon::findExtremityNot(const MapExtremity& oMap,
                                              const XY& xy,
                                              StringId excludedId)
{
    auto oIter = oMap.find(xy);
    if( oIter == oMap.end() )
        return -1;
    if( oIter->second.first != excludedId )
        return oIter->second.first;
    if( oIter->second.second != excludedId )
        return oIter->second.second;
    return -1;
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
    for( auto& oStringIter: oMapStrings )
    {
        const auto thisId = oStringIter.first;
        auto& oString = oStringIter.second;

/* -------------------------------------------------------------------- */
/*      Keep trying to merge others strings into our target "base"      */
/*      string while there are matches.                                 */
/* -------------------------------------------------------------------- */
        while(true)
        {
            auto nOtherId = findExtremityNot(oMapStartStrings, oString.back(), thisId);
            if( nOtherId != -1 )
            {
                Merge( thisId, nOtherId, 1 );
                continue;
            }
            else
            {
                nOtherId = findExtremityNot(oMapEndStrings, oString.back(), thisId);
                if( nOtherId != -1 )
                {
                    Merge( thisId, nOtherId, -1 );
                    continue;
                }
            }
            break;
        }

        // At this point our loop *should* be closed!
        CPLAssert( oString.front() == oString.back() );
    }
}

/************************************************************************/
/*                           removeExtremity()                          */
/************************************************************************/

void RPolygon::removeExtremity(MapExtremity& oMap,
                               const XY& xy,
                               StringId id)
{
    auto oIter = oMap.find(xy);
    CPLAssert( oIter != oMap.end() );
    if( oIter->second.first == id )
    {
        oIter->second.first = oIter->second.second;
        oIter->second.second = -1;
        if( oIter->second.first < 0 )
            oMap.erase(oIter);
    }
    else if( oIter->second.second == id )
    {
        oIter->second.second = -1;
        CPLAssert( oIter->second.first >= 0 );
    }
    else
    {
        CPLAssert(false);
    }
}

/************************************************************************/
/*                           insertExtremity()                          */
/************************************************************************/

void RPolygon::insertExtremity(MapExtremity& oMap,
                               const XY& xy,
                               StringId id)
{
    auto oIter = oMap.find(xy);
    if( oIter != oMap.end() )
    {
        CPLAssert( oIter->second.second == -1 );
        oIter->second.second = id;
    }
    else
    {
        oMap[xy] = std::pair<StringId, StringId>(id, -1);
    }
}

/************************************************************************/
/*                               Merge()                                */
/************************************************************************/

void RPolygon::Merge( StringId iBaseString, StringId iSrcString, int iDirection )

{
    auto &anBase = oMapStrings.find(iBaseString)->second;
    auto anStringIter = oMapStrings.find(iSrcString);
    auto& anString = anStringIter->second;
    int iStart = 1;
    int iEnd = -1;

    if( iDirection == 1 )
    {
        iEnd = static_cast<int>(anString.size());
    }
    else
    {
        iStart = static_cast<int>(anString.size()) - 2;
    }

    removeExtremity(oMapEndStrings, anBase.back(), iBaseString);

    anBase.reserve(anBase.size() + anString.size() - 1);
    for( int i = iStart; i != iEnd; i += iDirection )
    {
        anBase.push_back( anString[i] );
    }

    removeExtremity(oMapStartStrings, anString.front(), iSrcString);
    removeExtremity(oMapEndStrings, anString.back(), iSrcString);
    oMapStrings.erase(anStringIter);
    insertExtremity(oMapEndStrings, anBase.back(), iBaseString);
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

    XY xy1 = { x1, y1 };
    XY xy2 = { x2, y2 };

    StringId iExistingString = findExtremityNot(oMapEndStrings, xy1, -1);
    if( iExistingString >= 0 )
    {
        std::swap( xy1, xy2 );
    }
    else
    {
        iExistingString = findExtremityNot(oMapEndStrings, xy2, -1);
    }
    if( iExistingString >= 0 )
    {
        auto& anString = oMapStrings[iExistingString];
        const size_t nSSize = anString.size();

        // We are going to add a segment, but should we just extend
        // an existing segment already going in the right direction?

        const int nLastLen =
            std::max(std::abs(anString[nSSize - 2].x - anString[nSSize - 1].x),
                     std::abs(anString[nSSize - 2].y - anString[nSSize - 1].y));

        removeExtremity(oMapEndStrings, anString.back(), iExistingString);

        if( (anString[nSSize - 2].x - anString[nSSize - 1].x
                == (anString[nSSize - 1].x - xy1.x) * nLastLen)
            && (anString[nSSize - 2].y - anString[nSSize - 1].y
                == (anString[nSSize - 1].y - xy1.y) * nLastLen) )
        {
            anString[nSSize - 1] = xy1;
        }
        else
        {
            anString.push_back( xy1 );
        }
        insertExtremity(oMapEndStrings, anString.back(), iExistingString);
        return;
    }

/* -------------------------------------------------------------------- */
/*      Create a new string.                                            */
/* -------------------------------------------------------------------- */
    oMapStrings[iNextStringId] = std::vector<XY>{ xy1, xy2 };
    insertExtremity(oMapStartStrings, xy1, iNextStringId);
    insertExtremity(oMapEndStrings, xy2, iNextStringId);
    iNextStringId ++;
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
            if( papoPoly[nThisId] == nullptr )
                papoPoly[nThisId] = new RPolygon( panPolyValue[nThisId] );

            papoPoly[nThisId]->AddSegment( iXReal, iY, iXReal+1, iY );
        }
        if( nPreviousId != -1 )
        {
            if( papoPoly[nPreviousId] == nullptr )
                papoPoly[nPreviousId] = new RPolygon(panPolyValue[nPreviousId]);

            papoPoly[nPreviousId]->AddSegment( iXReal, iY, iXReal+1, iY );
        }
    }

    if( nThisId != nRightId )
    {
        if( nThisId != -1 )
        {
            if( papoPoly[nThisId] == nullptr )
                papoPoly[nThisId] = new RPolygon(panPolyValue[nThisId]);

            papoPoly[nThisId]->AddSegment( iXReal+1, iY, iXReal+1, iY+1 );
        }

        if( nRightId != -1 )
        {
            if( papoPoly[nRightId] == nullptr )
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

    for( const auto& oIter: poRPoly->oMapStrings )
    {
        const auto &anString = oIter.second;
        OGRGeometryH hRing = OGR_G_CreateGeometry( wkbLinearRing );

        // We go last to first to ensure the linestring is allocated to
        // the proper size on the first try.
        for( int iVert = static_cast<int>(anString.size()) - 1;
             iVert >= 0;
             iVert-- )
        {
            const int nPixelX = anString[iVert].x;
            const int nPixelY = anString[iVert].y;

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

    if( pfnProgress == nullptr )
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
        hMaskBand != nullptr
        ? static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize))
        : nullptr;

    if( panLastLineVal == nullptr || panThisLineVal == nullptr ||
        panLastLineId == nullptr || panThisLineId == nullptr ||
        (hMaskBand != nullptr && pabyMaskLine == nullptr) )
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
    bool bGotGeoTransform = false;
    const char* pszDatasetForGeoRef = CSLFetchNameValue(papszOptions,
                                                        "DATASET_FOR_GEOREF");
    if( pszDatasetForGeoRef )
    {
        GDALDatasetH hSrcDS = GDALOpen(pszDatasetForGeoRef, GA_ReadOnly);
        if( hSrcDS )
        {
            bGotGeoTransform = GDALGetGeoTransform( hSrcDS, adfGeoTransform ) == CE_None;
            GDALClose(hSrcDS);
        }
    }
    else
    {
        GDALDatasetH hSrcDS = GDALGetBandDataset( hSrcBand );
        if( hSrcDS )
            bGotGeoTransform = GDALGetGeoTransform( hSrcDS, adfGeoTransform ) == CE_None;
    }
    if( !bGotGeoTransform )
    {
        adfGeoTransform[0] = 0;
        adfGeoTransform[1] = 1;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = 0;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = 1;
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

        if( eErr == CE_None && hMaskBand != nullptr )
            eErr = GPMaskImageData(hMaskBand, pabyMaskLine, iY, nXSize,
                                   panThisLineVal);

        if( iY == 0 )
            oFirstEnum.ProcessLine(
                nullptr, panThisLineVal, nullptr, panThisLineId, nXSize );
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

            if( eErr == CE_None && hMaskBand != nullptr )
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
                nullptr, panThisLineVal, nullptr, panThisLineId+1, nXSize );
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
                    papoPoly[iX] = nullptr;
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
            papoPoly[iX] = nullptr;
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
 * attribute into which the pixel value of the polygon should be written. Or
 * -1 to indicate that the pixel value must not be written.
 * @param papszOptions a name/value list of additional options
 * <ul>
 * <li>8CONNECTED=8: May be set to "8" to use 8 connectedness.
 * Otherwise 4 connectedness will be applied to the algorithm</li>
 * </ul>
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
 * attribute into which the pixel value of the polygon should be written. Or
 * -1 to indicate that the pixel value must not be written.
 * @param papszOptions a name/value list of additional options
 * <ul>
 * <li>8CONNECTED=8: May be set to "8" to use 8 connectedness.
 * Otherwise 4 connectedness will be applied to the algorithm</li>
 * </ul>
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
