/******************************************************************************
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"

#include "rasterlitedataset.h"

#include <algorithm>

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) || defined(ALLOW_FORMAT_DUMPS)
// Enable accepting a SQL dump (starting with a "-- SQL SQLITE" or
// "-- SQL RASTERLITE" line) as a valid
// file. This makes fuzzer life easier
#define ENABLE_SQL_SQLITE_FORMAT
#endif

CPL_CVSID("$Id$")

/************************************************************************/
/*                        RasterliteOpenSQLiteDB()                      */
/************************************************************************/

OGRDataSourceH RasterliteOpenSQLiteDB(const char* pszFilename,
                                      GDALAccess eAccess)
{
    const char* const apszAllowedDrivers[] = { "SQLITE", nullptr };
    return reinterpret_cast<OGRDataSourceH>(
        GDALOpenEx( pszFilename,
                    GDAL_OF_VECTOR |
                    ((eAccess == GA_Update) ? GDAL_OF_UPDATE : 0),
                    apszAllowedDrivers, nullptr, nullptr ));
}

/************************************************************************/
/*                       RasterliteGetPixelSizeCond()                   */
/************************************************************************/

CPLString RasterliteGetPixelSizeCond(double dfPixelXSize,
                                     double dfPixelYSize,
                                     const char* pszTablePrefixWithDot)
{
    CPLString osCond;
    osCond.Printf("((%spixel_x_size >= %s AND %spixel_x_size <= %s) AND "
                   "(%spixel_y_size >= %s AND %spixel_y_size <= %s))",
                  pszTablePrefixWithDot,
                  CPLString().FormatC(dfPixelXSize - 1e-15,"%.15f").c_str(),
                  pszTablePrefixWithDot,
                  CPLString().FormatC(dfPixelXSize + 1e-15,"%.15f").c_str(),
                  pszTablePrefixWithDot,
                  CPLString().FormatC(dfPixelYSize - 1e-15,"%.15f").c_str(),
                  pszTablePrefixWithDot,
                  CPLString().FormatC(dfPixelYSize + 1e-15,"%.15f").c_str());
    return osCond;
}
/************************************************************************/
/*                     RasterliteGetSpatialFilterCond()                 */
/************************************************************************/

CPLString RasterliteGetSpatialFilterCond(double minx, double miny,
                                         double maxx, double maxy)
{
    CPLString osCond;
    osCond.Printf("(xmin < %s AND xmax > %s AND ymin < %s AND ymax > %s)",
                  CPLString().FormatC(maxx,"%.15f").c_str(),
                  CPLString().FormatC(minx,"%.15f").c_str(),
                  CPLString().FormatC(maxy,"%.15f").c_str(),
                  CPLString().FormatC(miny,"%.15f").c_str());
    return osCond;
}

/************************************************************************/
/*                            RasterliteBand()                          */
/************************************************************************/

RasterliteBand::RasterliteBand( RasterliteDataset* poDSIn, int nBandIn,
                                GDALDataType eDataTypeIn,
                                int nBlockXSizeIn, int nBlockYSizeIn )
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDataTypeIn;
    nBlockXSize = nBlockXSizeIn;
    nBlockYSize = nBlockYSizeIn;
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

//#define RASTERLITE_DEBUG

CPLErr RasterliteBand::IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage)
{
    RasterliteDataset* poGDS = reinterpret_cast<RasterliteDataset *>( poDS );

    double minx = poGDS->adfGeoTransform[0] +
                  nBlockXOff * nBlockXSize * poGDS->adfGeoTransform[1];
    double maxx = poGDS->adfGeoTransform[0] +
                  (nBlockXOff + 1) * nBlockXSize * poGDS->adfGeoTransform[1];
    double maxy = poGDS->adfGeoTransform[3] +
                  nBlockYOff * nBlockYSize * poGDS->adfGeoTransform[5];
    double miny = poGDS->adfGeoTransform[3] +
                  (nBlockYOff + 1) * nBlockYSize * poGDS->adfGeoTransform[5];
    int nDataTypeSize = GDALGetDataTypeSize(eDataType) / 8;

#ifdef RASTERLITE_DEBUG
    if (nBand == 1)
    {
        printf("nBlockXOff = %d, nBlockYOff = %d, nBlockXSize = %d, nBlockYSize = %d\n" /*ok*/
               "minx=%.15f miny=%.15f maxx=%.15f maxy=%.15f\n",
                nBlockXOff, nBlockYOff, nBlockXSize, nBlockYSize, minx, miny, maxx, maxy);
    }
#endif

    CPLString osSQL;
    osSQL.Printf("SELECT m.geometry, r.raster, m.id, m.width, m.height FROM \"%s_metadata\" AS m, "
                 "\"%s_rasters\" AS r WHERE m.rowid IN "
                 "(SELECT pkid FROM \"idx_%s_metadata_geometry\" "
                  "WHERE %s) AND %s AND r.id = m.id",
                 poGDS->osTableName.c_str(),
                 poGDS->osTableName.c_str(),
                 poGDS->osTableName.c_str(),
                 RasterliteGetSpatialFilterCond(minx, miny, maxx, maxy).c_str(),
                 RasterliteGetPixelSizeCond(poGDS->adfGeoTransform[1], -poGDS->adfGeoTransform[5], "m.").c_str());

    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(poGDS->hDS, osSQL.c_str(), nullptr, nullptr);
    if (hSQLLyr == nullptr)
    {
        memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize);
        return CE_None;
    }

    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/%p", this);

#ifdef RASTERLITE_DEBUG
    if (nBand == 1)
    {
        printf("nTiles = %d\n", static_cast<int>(/*ok*/
            OGR_L_GetFeatureCount(hSQLLyr, TRUE) ));
    }
#endif

    bool bHasFoundTile = false;
    bool bHasMemsetTile = false;

    OGRFeatureH hFeat;
    CPLErr eErr = CE_None;
    while( (hFeat = OGR_L_GetNextFeature(hSQLLyr)) != nullptr && eErr == CE_None )
    {
        OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);
        if (hGeom == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "null geometry found");
            OGR_F_Destroy(hFeat);
            OGR_DS_ReleaseResultSet(poGDS->hDS, hSQLLyr);
            return CE_Failure;
        }

        OGREnvelope oEnvelope;
        OGR_G_GetEnvelope(hGeom, &oEnvelope);

        const int nTileId = OGR_F_GetFieldAsInteger(hFeat, 1);
        if( poGDS->m_nLastBadTileId == nTileId )
        {
            OGR_F_Destroy(hFeat);
            continue;
        }
        const int nTileXSize = OGR_F_GetFieldAsInteger(hFeat, 2);
        const int nTileYSize = OGR_F_GetFieldAsInteger(hFeat, 3);

        int nDstXOff = static_cast<int>(
            ( oEnvelope.MinX - minx ) / poGDS->adfGeoTransform[1] + 0.5 );
        int nDstYOff = static_cast<int>(
            ( maxy - oEnvelope.MaxY ) / ( -poGDS->adfGeoTransform[5] ) + 0.5 );

        int nReqXSize = nTileXSize;
        int nReqYSize = nTileYSize;

        int nSrcXOff, nSrcYOff;

        if (nDstXOff >= 0)
        {
            nSrcXOff = 0;
        }
        else
        {
            nSrcXOff = -nDstXOff;
            nReqXSize += nDstXOff;
            nDstXOff = 0;
        }

        if (nDstYOff >= 0)
        {
            nSrcYOff = 0;
        }
        else
        {
            nSrcYOff = -nDstYOff;
            nReqYSize += nDstYOff;
            nDstYOff = 0;
        }

        if (nDstXOff + nReqXSize > nBlockXSize)
            nReqXSize = nBlockXSize - nDstXOff;

        if (nDstYOff + nReqYSize > nBlockYSize)
            nReqYSize = nBlockYSize - nDstYOff;

#ifdef RASTERLITE_DEBUG
        if (nBand == 1)
        {
            printf("id = %d, minx=%.15f miny=%.15f maxx=%.15f maxy=%.15f\n"/*ok*/
                   "nDstXOff = %d, nDstYOff = %d, nSrcXOff = %d, nSrcYOff = %d, "
                   "nReqXSize=%d, nReqYSize=%d\n",
                   nTileId,
                   oEnvelope.MinX, oEnvelope.MinY, oEnvelope.MaxX, oEnvelope.MaxY,
                   nDstXOff, nDstYOff,
                   nSrcXOff, nSrcYOff, nReqXSize, nReqYSize);
        }
#endif

        if (nReqXSize > 0 && nReqYSize > 0 &&
            nSrcXOff < nTileXSize && nSrcYOff < nTileYSize)
        {

#ifdef RASTERLITE_DEBUG
            if (nBand == 1)
            {
                printf("id = %d, selected !\n",  nTileId);/*ok*/
            }
#endif
            int nDataSize = 0;
            GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);

            VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyData,
                                              nDataSize, FALSE);
            VSIFCloseL(fp);

            GDALDatasetH hDSTile = GDALOpenEx(osMemFileName.c_str(),
                                              GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                              nullptr, nullptr, nullptr);
            int nTileBands = 0;
            if (hDSTile && (nTileBands = GDALGetRasterCount(hDSTile)) == 0)
            {
                GDALClose(hDSTile);
                hDSTile = nullptr;
            }
            if (hDSTile == nullptr)
            {
                poGDS->m_nLastBadTileId = nTileId;
                CPLError(CE_Failure, CPLE_AppDefined, "Can't open tile %d",
                         nTileId);
            }

            int nReqBand = 1;
            if (nTileBands == poGDS->nBands)
                nReqBand = nBand;
            else if (eDataType == GDT_Byte && nTileBands == 1 && poGDS->nBands == 3)
                nReqBand = 1;
            else
            {
                poGDS->m_nLastBadTileId = nTileId;
                GDALClose(hDSTile);
                hDSTile = nullptr;
            }

            if( hDSTile )
            {
                if( GDALGetRasterXSize(hDSTile) != nTileXSize ||
                    GDALGetRasterYSize(hDSTile) != nTileYSize )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Invalid dimensions for tile %d",
                             nTileId);
                    poGDS->m_nLastBadTileId = nTileId;
                    GDALClose(hDSTile);
                    hDSTile = nullptr;
                }
            }

            if (hDSTile)
            {
                bHasFoundTile = true;

                bool bHasJustMemsetTileBand1 = false;

                /* If the source tile doesn't fit the entire block size, then */
                /* we memset 0 before */
                if (!(nDstXOff == 0 && nDstYOff == 0 &&
                      nReqXSize == nBlockXSize && nReqYSize == nBlockYSize) &&
                    !bHasMemsetTile)
                {
                    memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize);
                    bHasMemsetTile = true;
                    bHasJustMemsetTileBand1 = true;
                }

                GDALColorTable* poTileCT =
                    reinterpret_cast<GDALColorTable *>(
                        GDALGetRasterColorTable(
                            GDALGetRasterBand( hDSTile, 1 ) ) );
                unsigned char* pabyTranslationTable = nullptr;
                if (poGDS->nBands == 1 && poGDS->poCT != nullptr && poTileCT != nullptr)
                {
                    pabyTranslationTable =
                        reinterpret_cast<GDALRasterBand *>(
                            GDALGetRasterBand( hDSTile, 1 ) )->
                                GetIndexColorTranslationTo(this, nullptr, nullptr);
                }

/* -------------------------------------------------------------------- */
/*      Read tile data                                                  */
/* -------------------------------------------------------------------- */
                eErr = GDALRasterIO(
                    GDALGetRasterBand(hDSTile, nReqBand), GF_Read,
                    nSrcXOff, nSrcYOff, nReqXSize, nReqYSize,
                    reinterpret_cast<char*>( pImage )
                    + (nDstXOff + nDstYOff * nBlockXSize) * nDataTypeSize,
                    nReqXSize, nReqYSize,
                    eDataType, nDataTypeSize, nBlockXSize * nDataTypeSize);

                if (eDataType == GDT_Byte && pabyTranslationTable)
                {
/* -------------------------------------------------------------------- */
/*      Convert from tile CT to band CT                                 */
/* -------------------------------------------------------------------- */
                    for( int j = nDstYOff; j < nDstYOff + nReqYSize; j++ )
                    {
                        for( int i = nDstXOff; i < nDstXOff + nReqXSize; i++ )
                        {
                            GByte* pPixel = reinterpret_cast<GByte *>( pImage )
                                + i + j * nBlockXSize;
                            *pPixel = pabyTranslationTable[*pPixel];
                        }
                    }
                    CPLFree(pabyTranslationTable);
                    pabyTranslationTable = nullptr;
                }
                else if (eDataType == GDT_Byte && nTileBands == 1 &&
                         poGDS->nBands == 3 && poTileCT != nullptr)
                {
/* -------------------------------------------------------------------- */
/*      Expand from PCT to RGB                                          */
/* -------------------------------------------------------------------- */
                    GByte abyCT[256];
                    const int nEntries = std::min(256, poTileCT->GetColorEntryCount());
                    for( int i = 0; i < nEntries; i++ )
                    {
                        const GDALColorEntry* psEntry = poTileCT->GetColorEntry(i);
                        if (nBand == 1)
                            abyCT[i] = static_cast<GByte>( psEntry->c1 );
                        else if (nBand == 2)
                            abyCT[i] = static_cast<GByte>( psEntry->c2 );
                        else
                            abyCT[i] = static_cast<GByte>( psEntry->c3 );
                    }
                    for( int i = nEntries; i < 256; i++ )
                        abyCT[i] = 0;

                    for( int j = nDstYOff; j < nDstYOff + nReqYSize; j++ )
                    {
                        for( int i = nDstXOff; i < nDstXOff + nReqXSize; i++ )
                        {
                            GByte* pPixel = reinterpret_cast<GByte *>( pImage )
                                + i + j * nBlockXSize;
                            *pPixel = abyCT[*pPixel];
                        }
                    }
                }

/* -------------------------------------------------------------------- */
/*      Put in the block cache the data for this block into other bands */
/*      while the underlying dataset is opened                          */
/* -------------------------------------------------------------------- */
                if (nBand == 1 && poGDS->nBands > 1)
                {
                    for( int iOtherBand = 2;
                         iOtherBand<=poGDS->nBands && eErr == CE_None;
                         iOtherBand++ )
                    {
                        GDALRasterBlock *poBlock
                            = poGDS->GetRasterBand(iOtherBand)->
                            GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
                        if (poBlock == nullptr)
                            break;

                        GByte* pabySrcBlock = reinterpret_cast<GByte *>(
                            poBlock->GetDataRef() );
                        if( pabySrcBlock == nullptr )
                        {
                            poBlock->DropLock();
                            break;
                        }

                        if (nTileBands == 1)
                            nReqBand = 1;
                        else
                            nReqBand = iOtherBand;

                        if (bHasJustMemsetTileBand1)
                            memset(pabySrcBlock, 0,
                                   nBlockXSize * nBlockYSize * nDataTypeSize);

/* -------------------------------------------------------------------- */
/*      Read tile data                                                  */
/* -------------------------------------------------------------------- */
                        eErr = GDALRasterIO(
                            GDALGetRasterBand(hDSTile, nReqBand), GF_Read,
                            nSrcXOff, nSrcYOff, nReqXSize, nReqYSize,
                            reinterpret_cast<char *>( pabySrcBlock ) +
                            (nDstXOff + nDstYOff * nBlockXSize) * nDataTypeSize,
                            nReqXSize, nReqYSize,
                            eDataType, nDataTypeSize,
                            nBlockXSize * nDataTypeSize);

                        if (eDataType == GDT_Byte && nTileBands == 1 &&
                            poGDS->nBands == 3 && poTileCT != nullptr)
                        {
/* -------------------------------------------------------------------- */
/*      Expand from PCT to RGB                                          */
/* -------------------------------------------------------------------- */

                            GByte abyCT[256];
                            const int nEntries
                                = std::min(256, poTileCT->GetColorEntryCount());
                            for( int i=0; i < nEntries; i++ )
                            {
                                const GDALColorEntry* psEntry = poTileCT->GetColorEntry(i);
                                if (iOtherBand == 1)
                                    abyCT[i] = static_cast<GByte>( psEntry->c1 );
                                else if (iOtherBand == 2)
                                    abyCT[i] = static_cast<GByte>( psEntry->c2 );
                                else
                                    abyCT[i] = static_cast<GByte>( psEntry->c3 );
                            }
                            for( int i = nEntries; i < 256; i++ )
                                abyCT[i] = 0;

                            for( int j = nDstYOff; j < nDstYOff + nReqYSize; j++ )
                            {
                                for( int i = nDstXOff; i < nDstXOff + nReqXSize; i++ )
                                {
                                  GByte* pPixel
                                      = reinterpret_cast<GByte*>( pabySrcBlock )
                                      + i + j * nBlockXSize;
                                    *pPixel = abyCT[*pPixel];
                                }
                            }
                        }

                        poBlock->DropLock();
                    }
                }
                GDALClose(hDSTile);
            }

            VSIUnlink(osMemFileName.c_str());
        }
        else
        {
#ifdef RASTERLITE_DEBUG
            if (nBand == 1)
            {
                printf("id = %d, NOT selected !\n",  nTileId);/*ok*/
            }
#endif
        }
        OGR_F_Destroy(hFeat);
    }

    VSIUnlink(osMemFileName.c_str());
    VSIUnlink((osMemFileName + ".aux.xml").c_str());

    if (!bHasFoundTile)
    {
        memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize);
    }

    OGR_DS_ReleaseResultSet(poGDS->hDS, hSQLLyr);

#ifdef RASTERLITE_DEBUG
    if (nBand == 1)
        printf("\n");/*ok*/
#endif

    return eErr;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int RasterliteBand::GetOverviewCount()
{
    RasterliteDataset* poGDS = reinterpret_cast<RasterliteDataset *>( poDS );

    if (poGDS->nLimitOvrCount >= 0)
        return poGDS->nLimitOvrCount;
    else if (poGDS->nResolutions > 1)
        return poGDS->nResolutions - 1;
    else
        return GDALPamRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand* RasterliteBand::GetOverview(int nLevel)
{
    RasterliteDataset* poGDS = reinterpret_cast<RasterliteDataset *>( poDS );

    if (poGDS->nLimitOvrCount >= 0)
    {
        if (nLevel < 0 || nLevel >= poGDS->nLimitOvrCount)
            return nullptr;
    }

    if (poGDS->nResolutions == 1)
        return GDALPamRasterBand::GetOverview(nLevel);

    if (nLevel < 0 || nLevel >= poGDS->nResolutions - 1)
        return nullptr;

    GDALDataset* poOvrDS = poGDS->papoOverviews[nLevel];
    if (poOvrDS)
        return poOvrDS->GetRasterBand(nBand);

    return nullptr;
}

/************************************************************************/
/*                   GetColorInterpretation()                           */
/************************************************************************/

GDALColorInterp RasterliteBand::GetColorInterpretation()
{
    RasterliteDataset* poGDS = reinterpret_cast<RasterliteDataset *>( poDS );
    if (poGDS->nBands == 1)
    {
        if (poGDS->poCT != nullptr)
            return GCI_PaletteIndex;

        return GCI_GrayIndex;
    }
    else if (poGDS->nBands == 3)
    {
        if (nBand == 1)
            return GCI_RedBand;
        else if (nBand == 2)
            return GCI_GreenBand;
        else if (nBand == 3)
            return GCI_BlueBand;
    }

    return GCI_Undefined;
}

/************************************************************************/
/*                        GetColorTable()                               */
/************************************************************************/

GDALColorTable* RasterliteBand::GetColorTable()
{
    RasterliteDataset* poGDS = reinterpret_cast<RasterliteDataset *>( poDS );
    if (poGDS->nBands == 1)
        return poGDS->poCT;

    return nullptr;
}

/************************************************************************/
/*                         RasterliteDataset()                          */
/************************************************************************/

RasterliteDataset::RasterliteDataset() :
    bMustFree(FALSE),
    poMainDS(nullptr),
    nLevel(0),
    papszMetadata(nullptr),
    papszImageStructure(CSLAddString(nullptr, "INTERLEAVE=PIXEL")),
    papszSubDatasets(nullptr),
    nResolutions(0),
    padfXResolutions(nullptr),
    padfYResolutions(nullptr),
    papoOverviews(nullptr),
    nLimitOvrCount(-1),
    bValidGeoTransform(FALSE),
    pszSRS(nullptr),
    poCT(nullptr),
    bCheckForExistingOverview(TRUE),
    hDS(nullptr)
{
    memset( adfGeoTransform, 0, sizeof(adfGeoTransform) );
}

/************************************************************************/
/*                         RasterliteDataset()                          */
/************************************************************************/

RasterliteDataset::RasterliteDataset( RasterliteDataset* poMainDSIn,
                                      int nLevelIn ) :
    bMustFree(FALSE),
    poMainDS(poMainDSIn),
    nLevel(nLevelIn),
    papszMetadata(poMainDSIn->papszMetadata),
    papszImageStructure(poMainDSIn->papszImageStructure),
    papszSubDatasets(poMainDS->papszSubDatasets),
    nResolutions(poMainDSIn->nResolutions - nLevelIn),
    padfXResolutions(poMainDSIn->padfXResolutions + nLevelIn),
    padfYResolutions(poMainDSIn->padfYResolutions + nLevelIn),
    papoOverviews(poMainDSIn->papoOverviews + nLevelIn),
    nLimitOvrCount(-1),
    bValidGeoTransform(TRUE),
    pszSRS(poMainDSIn->pszSRS),
    poCT(poMainDSIn->poCT),
    osTableName(poMainDSIn->osTableName),
    osFileName(poMainDSIn->osFileName),
    bCheckForExistingOverview(TRUE),
    // TODO: osOvrFileName?
    hDS(poMainDSIn->hDS)
{
    nRasterXSize = static_cast<int>(poMainDS->nRasterXSize *
        (poMainDS->padfXResolutions[0] / padfXResolutions[0]) + 0.5);
    nRasterYSize = static_cast<int>(poMainDS->nRasterYSize *
        (poMainDS->padfYResolutions[0] / padfYResolutions[0]) + 0.5);

    memcpy(adfGeoTransform, poMainDS->adfGeoTransform, 6 * sizeof(double));
    adfGeoTransform[1] = padfXResolutions[0];
    adfGeoTransform[5] = - padfYResolutions[0];
}

/************************************************************************/
/*                        ~RasterliteDataset()                          */
/************************************************************************/

RasterliteDataset::~RasterliteDataset()
{
    RasterliteDataset::CloseDependentDatasets();
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int RasterliteDataset::CloseDependentDatasets()
{
    int bRet = GDALPamDataset::CloseDependentDatasets();

    if (poMainDS == nullptr && !bMustFree)
    {
        CSLDestroy(papszMetadata);
        papszMetadata = nullptr;
        CSLDestroy(papszSubDatasets);
        papszSubDatasets = nullptr;
        CSLDestroy(papszImageStructure);
        papszImageStructure = nullptr;
        CPLFree(pszSRS);
        pszSRS = nullptr;

        if (papoOverviews)
        {
            for( int i = 1; i < nResolutions; i++ )
            {
                if (papoOverviews[i-1] != nullptr &&
                    papoOverviews[i-1]->bMustFree)
                {
                    papoOverviews[i-1]->poMainDS = nullptr;
                }
                delete papoOverviews[i-1];
            }
            CPLFree(papoOverviews);
            papoOverviews = nullptr;
            nResolutions = 0;
            bRet = TRUE;
        }

        if (hDS != nullptr)
            OGRReleaseDataSource(hDS);
        hDS = nullptr;

        CPLFree(padfXResolutions);
        CPLFree(padfYResolutions);
        padfXResolutions = nullptr;
        padfYResolutions = nullptr;

        delete poCT;
        poCT = nullptr;
    }
    else if (poMainDS != nullptr && bMustFree)
    {
        poMainDS->papoOverviews[nLevel-1] = nullptr;
        delete poMainDS;
        poMainDS = nullptr;
        bRet = TRUE;
    }

    return bRet;
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void RasterliteDataset::AddSubDataset( const char* pszDSName)
{
    char szName[80];
    const int nCount = CSLCount(papszSubDatasets ) / 2;

    snprintf( szName, sizeof(szName), "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName, pszDSName);

    snprintf( szName, sizeof(szName), "SUBDATASET_%d_DESC", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName, pszDSName);
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **RasterliteDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", "IMAGE_STRUCTURE", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **RasterliteDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != nullptr && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;

    if( CSLCount(papszSubDatasets) < 2 &&
        pszDomain != nullptr && EQUAL(pszDomain,"IMAGE_STRUCTURE") )
        return papszImageStructure;

    if ( pszDomain == nullptr || EQUAL(pszDomain, "") )
        return papszMetadata;

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *RasterliteDataset::GetMetadataItem( const char *pszName,
                                                const char *pszDomain )
{
    if (pszDomain != nullptr &&EQUAL(pszDomain,"OVERVIEWS") )
    {
        if (nResolutions > 1 || CSLCount(papszSubDatasets) > 2)
            return nullptr;

        osOvrFileName.Printf("%s_%s", osFileName.c_str(), osTableName.c_str());
        if (bCheckForExistingOverview == FALSE ||
            CPLCheckForFile(const_cast<char *>( osOvrFileName.c_str() ), nullptr))
            return osOvrFileName.c_str();

        return nullptr;
    }

    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr RasterliteDataset::GetGeoTransform( double* padfGeoTransform )
{
    if (bValidGeoTransform)
    {
        memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));
        return CE_None;
    }

    return CE_Failure;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* RasterliteDataset::_GetProjectionRef()
{
    if (pszSRS)
        return pszSRS;

    return "";
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** RasterliteDataset::GetFileList()
{
    char** papszFileList
        = CSLAddString(nullptr, osFileName);
    return papszFileList;
}

/************************************************************************/
/*                         GetBlockParams()                             */
/************************************************************************/

int RasterliteDataset::GetBlockParams(OGRLayerH hRasterLyr, int nLevelIn,
                                      int* pnBands, GDALDataType* peDataType,
                                      int* pnBlockXSize, int* pnBlockYSize)
{
    CPLString osSQL;
    osSQL.Printf("SELECT m.geometry, r.raster, m.id "
                 "FROM \"%s_metadata\" AS m, \"%s_rasters\" AS r "
                 "WHERE %s AND r.id = m.id",
                 osTableName.c_str(), osTableName.c_str(),
                 RasterliteGetPixelSizeCond(padfXResolutions[nLevelIn],padfYResolutions[nLevelIn], "m.").c_str());

    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
    if (hSQLLyr == nullptr)
    {
        return FALSE;
    }

    OGRFeatureH hFeat = OGR_L_GetNextFeature(hRasterLyr);
    if (hFeat == nullptr)
    {
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return FALSE;
    }

    int nDataSize;
    GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);

    if (nDataSize > 32 &&
        STARTS_WITH_CI(reinterpret_cast<const char *>(pabyData),
                       "StartWaveletsImage$$"))
    {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Rasterlite driver no longer support WAVELET compressed "
                     "images");
            OGR_F_Destroy(hFeat);
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            return FALSE;
    }

    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/%p", this);
    VSILFILE *fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyData,
                                         nDataSize, FALSE);
    VSIFCloseL(fp);

    GDALDatasetH hDSTile = GDALOpen(osMemFileName.c_str(), GA_ReadOnly);
    if (hDSTile)
    {
        *pnBands = GDALGetRasterCount(hDSTile);
        if (*pnBands == 0)
        {
            GDALClose(hDSTile);
            hDSTile = nullptr;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Can't open tile %d",
                 OGR_F_GetFieldAsInteger(hFeat, 1));
    }

    if (hDSTile)
    {
        *peDataType = GDALGetRasterDataType(GDALGetRasterBand(hDSTile, 1));

        for( int iBand = 2; iBand <= *pnBands; iBand++ )
        {
            if (*peDataType != GDALGetRasterDataType(GDALGetRasterBand(hDSTile, 1)))
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Band types must be identical");
                GDALClose(hDSTile);
                hDSTile = nullptr;
                goto end;
            }
        }

        *pnBlockXSize = GDALGetRasterXSize(hDSTile);
        *pnBlockYSize = GDALGetRasterYSize(hDSTile);
        if (CSLFindName(papszImageStructure, "COMPRESSION") == -1)
        {
            const char* pszCompression =
                GDALGetMetadataItem(hDSTile, "COMPRESSION", "IMAGE_STRUCTURE");
            if (pszCompression != nullptr && EQUAL(pszCompression, "JPEG"))
                papszImageStructure =
                    CSLAddString(papszImageStructure, "COMPRESSION=JPEG");
        }

        if (CSLFindName(papszMetadata, "TILE_FORMAT") == -1)
        {
            papszMetadata =
                CSLSetNameValue(papszMetadata, "TILE_FORMAT",
                           GDALGetDriverShortName(GDALGetDatasetDriver(hDSTile)));
        }

        if (*pnBands == 1 && this->poCT == nullptr)
        {
            GDALColorTable* l_poCT =
                reinterpret_cast<GDALColorTable *>(
                    GDALGetRasterColorTable(GDALGetRasterBand(hDSTile, 1) ) );
            if (l_poCT)
                this->poCT = l_poCT->Clone();
        }

        GDALClose(hDSTile);
    }
end:
    VSIUnlink(osMemFileName.c_str());
    VSIUnlink((osMemFileName + ".aux.xml").c_str());

    OGR_F_Destroy(hFeat);

    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);

    return hDSTile != nullptr;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int RasterliteDataset::Identify(GDALOpenInfo* poOpenInfo)
{

#ifdef ENABLE_SQL_SQLITE_FORMAT
    if( poOpenInfo->pabyHeader &&
        STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL RASTERLITE") )
    {
        return TRUE;
    }
#endif

    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MBTILES") &&
        !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "GPKG") &&
        poOpenInfo->nHeaderBytes >= 1024 &&
        poOpenInfo->pabyHeader &&
        STARTS_WITH_CI((const char*)poOpenInfo->pabyHeader, "SQLite Format 3") &&
        // Do not match direct Amazon S3 signed URLs that contains .mbtiles in the middle of the URL
        strstr(poOpenInfo->pszFilename, ".mbtiles") == nullptr)
    {
        // Could be a SQLite/Spatialite file as well
        return -1;
    }
    else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "RASTERLITE:"))
    {
        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* RasterliteDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( Identify(poOpenInfo) == FALSE )
        return nullptr;

    CPLString osFileName;
    CPLString osTableName;
    int nLevel = 0;
    double minx = 0.0;
    double miny = 0.0;
    double maxx = 0.0;
    double maxy = 0.0;
    int bMinXSet = FALSE;
    int bMinYSet = FALSE;
    int bMaxXSet = FALSE;
    int bMaxYSet = FALSE;
    int nReqBands = 0;

/* -------------------------------------------------------------------- */
/*      Parse "file name"                                               */
/* -------------------------------------------------------------------- */
#ifdef ENABLE_SQL_SQLITE_FORMAT
    if( poOpenInfo->pabyHeader &&
        STARTS_WITH((const char*)poOpenInfo->pabyHeader, "-- SQL RASTERLITE") )
    {
        osFileName = poOpenInfo->pszFilename;
    }
    else
#endif
    if (poOpenInfo->nHeaderBytes >= 1024 &&
        STARTS_WITH_CI((const char*)poOpenInfo->pabyHeader, "SQLite Format 3"))
    {
        osFileName = poOpenInfo->pszFilename;
    }
    else
    {
        char** papszTokens = CSLTokenizeStringComplex(
                poOpenInfo->pszFilename + 11, ",", FALSE, FALSE );
        int nTokens = CSLCount(papszTokens);
        if (nTokens == 0)
        {
            CSLDestroy(papszTokens);
            return nullptr;
        }

        osFileName = papszTokens[0];

        for( int i=1; i < nTokens; i++)
        {
            if (STARTS_WITH_CI(papszTokens[i], "table="))
                osTableName = papszTokens[i] + 6;
            else if (STARTS_WITH_CI(papszTokens[i], "level="))
                nLevel = atoi(papszTokens[i] + 6);
            else if (STARTS_WITH_CI(papszTokens[i], "minx="))
            {
                bMinXSet = TRUE;
                minx = CPLAtof(papszTokens[i] + 5);
            }
            else if (STARTS_WITH_CI(papszTokens[i], "miny="))
            {
                bMinYSet = TRUE;
                miny = CPLAtof(papszTokens[i] + 5);
            }
            else if (STARTS_WITH_CI(papszTokens[i], "maxx="))
            {
                bMaxXSet = TRUE;
                maxx = CPLAtof(papszTokens[i] + 5);
            }
            else if (STARTS_WITH_CI(papszTokens[i], "maxy="))
            {
                bMaxYSet = TRUE;
                maxy = CPLAtof(papszTokens[i] + 5);
            }
            else if (STARTS_WITH_CI(papszTokens[i], "bands="))
            {
                nReqBands = atoi(papszTokens[i] + 6);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid option : %s", papszTokens[i]);
            }
        }
        CSLDestroy(papszTokens);
    }

    if (OGRGetDriverCount() == 0)
        OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Open underlying OGR DB                                          */
/* -------------------------------------------------------------------- */

    OGRDataSourceH hDS = RasterliteOpenSQLiteDB(osFileName.c_str(), poOpenInfo->eAccess);
    CPLDebug("RASTERLITE", "SQLite DB Open");

    RasterliteDataset* poDS = nullptr;

    if (hDS == nullptr)
        goto end;

    if (osTableName.empty())
    {
        int nCountSubdataset = 0;
        int nLayers = OGR_DS_GetLayerCount(hDS);
/* -------------------------------------------------------------------- */
/*      Add raster layers as subdatasets                                */
/* -------------------------------------------------------------------- */
        for( int i=0; i < nLayers; i++ )
        {
            OGRLayerH hLyr = OGR_DS_GetLayer(hDS, i);
            const char* pszLayerName = OGR_L_GetName(hLyr);
            if (strstr(pszLayerName, "_metadata"))
            {
                char* pszShortName = CPLStrdup(pszLayerName);
                *strstr(pszShortName, "_metadata") = '\0';

                CPLString osRasterTableName = pszShortName;
                osRasterTableName += "_rasters";

                if (OGR_DS_GetLayerByName(hDS, osRasterTableName.c_str()) != nullptr)
                {
                    if (poDS == nullptr)
                    {
                        poDS = new RasterliteDataset();
                        osTableName = pszShortName;
                    }

                    CPLString osSubdatasetName;
                    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "RASTERLITE:"))
                        osSubdatasetName += "RASTERLITE:";
                    osSubdatasetName += poOpenInfo->pszFilename;
                    osSubdatasetName += ",table=";
                    osSubdatasetName += pszShortName;
                    poDS->AddSubDataset(osSubdatasetName.c_str());

                    nCountSubdataset++;
                }

                CPLFree(pszShortName);
            }
        }

        if (nCountSubdataset == 0)
        {
            goto end;
        }
        else if (nCountSubdataset != 1)
        {
            poDS->SetDescription( poOpenInfo->pszFilename );
            goto end;
        }

/* -------------------------------------------------------------------- */
/*      If just one subdataset, then open it                            */
/* -------------------------------------------------------------------- */
        delete poDS;
        poDS = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Build dataset                                                   */
/* -------------------------------------------------------------------- */
    {
        GDALDataType eDataType;

        const CPLString osMetadataTableName = osTableName + "_metadata";

        OGRLayerH hMetadataLyr
            = OGR_DS_GetLayerByName(hDS, osMetadataTableName.c_str());
        if (hMetadataLyr == nullptr)
            goto end;

        const CPLString osRasterTableName = osTableName + "_rasters";

        OGRLayerH hRasterLyr
            = OGR_DS_GetLayerByName(hDS, osRasterTableName.c_str());
        if (hRasterLyr == nullptr)
            goto end;

/* -------------------------------------------------------------------- */
/*      Fetch resolutions                                               */
/* -------------------------------------------------------------------- */
        CPLString osSQL;
        OGRLayerH hSQLLyr;
        int nResolutions = 0;

        OGRLayerH hRasterPyramidsLyr
            = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
        if (hRasterPyramidsLyr)
        {
            osSQL.Printf("SELECT pixel_x_size, pixel_y_size "
                         "FROM raster_pyramids WHERE table_prefix = '%s' "
                         "ORDER BY pixel_x_size ASC",
                         osTableName.c_str());

            hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
            if (hSQLLyr != nullptr)
            {
                nResolutions = static_cast<int>(OGR_L_GetFeatureCount(hSQLLyr, TRUE));
                if( nResolutions == 0 )
                {
                    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
                    hSQLLyr = nullptr;
                }
            }
        }
        else
            hSQLLyr = nullptr;

        if( hSQLLyr == nullptr )
        {
            osSQL.Printf("SELECT DISTINCT(pixel_x_size), pixel_y_size "
                         "FROM \"%s_metadata\" WHERE pixel_x_size != 0  "
                         "ORDER BY pixel_x_size ASC",
                         osTableName.c_str());

            hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
            if (hSQLLyr == nullptr)
                goto end;

            nResolutions = static_cast<int>(OGR_L_GetFeatureCount(hSQLLyr, TRUE));

            if (nResolutions == 0)
            {
                OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
                goto end;
            }
        }

/* -------------------------------------------------------------------- */
/*      Set dataset attributes                                          */
/* -------------------------------------------------------------------- */

        poDS = new RasterliteDataset();
        poDS->SetDescription( poOpenInfo->pszFilename );
        poDS->eAccess = poOpenInfo->eAccess;
        poDS->osTableName = osTableName;
        poDS->osFileName = osFileName;
        poDS->hDS = hDS;

        /* poDS will release it from now */
        hDS = nullptr;

/* -------------------------------------------------------------------- */
/*      Fetch spatial extent or use the one provided by the user        */
/* -------------------------------------------------------------------- */
        OGREnvelope oEnvelope;
        if (bMinXSet && bMinYSet && bMaxXSet && bMaxYSet)
        {
            oEnvelope.MinX = minx;
            oEnvelope.MinY = miny;
            oEnvelope.MaxX = maxx;
            oEnvelope.MaxY = maxy;
        }
        else
        {
            CPLString osOldVal = CPLGetConfigOption("OGR_SQLITE_EXACT_EXTENT", "NO");
            CPLSetThreadLocalConfigOption("OGR_SQLITE_EXACT_EXTENT", "YES");
            OGR_L_GetExtent(hMetadataLyr, &oEnvelope, TRUE);
            CPLSetThreadLocalConfigOption("OGR_SQLITE_EXACT_EXTENT", osOldVal.c_str());
            //printf("minx=%.15f miny=%.15f maxx=%.15f maxy=%.15f\n",
            //       oEnvelope.MinX, oEnvelope.MinY, oEnvelope.MaxX, oEnvelope.MaxY);
        }

/* -------------------------------------------------------------------- */
/*      Store resolutions                                               */
/* -------------------------------------------------------------------- */
        poDS->nResolutions = nResolutions;
        poDS->padfXResolutions = reinterpret_cast<double *>(
            CPLMalloc( sizeof(double) * poDS->nResolutions ) );
        poDS->padfYResolutions = reinterpret_cast<double *>(
            CPLMalloc( sizeof(double) * poDS->nResolutions ) );

        {
          // Add a scope for i.
          OGRFeatureH hFeat;
          int i = 0;
          while((hFeat = OGR_L_GetNextFeature(hSQLLyr)) != nullptr)
          {
            poDS->padfXResolutions[i] = OGR_F_GetFieldAsDouble(hFeat, 0);
            poDS->padfYResolutions[i] = OGR_F_GetFieldAsDouble(hFeat, 1);

            OGR_F_Destroy(hFeat);

#ifdef RASTERLITE_DEBUG
            printf("[%d] xres=%.15f yres=%.15f\n", i,/*ok*/
                   poDS->padfXResolutions[i], poDS->padfYResolutions[i]);
#endif

            if (poDS->padfXResolutions[i] <= 0 || poDS->padfYResolutions[i] <= 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "res=%d, xres=%.15f, yres=%.15f",
                         i, poDS->padfXResolutions[i], poDS->padfYResolutions[i]);
                OGR_DS_ReleaseResultSet(poDS->hDS, hSQLLyr);
                delete poDS;
                poDS = nullptr;
                goto end;
            }
            i++;
          }
        }

        OGR_DS_ReleaseResultSet(poDS->hDS, hSQLLyr);
        hSQLLyr = nullptr;

/* -------------------------------------------------------------------- */
/*      Compute raster size, geotransform and projection                */
/* -------------------------------------------------------------------- */
        const double dfRasterXSize =
            (oEnvelope.MaxX - oEnvelope.MinX) / poDS->padfXResolutions[0] + 0.5;
        const double dfRasterYSize =
            (oEnvelope.MaxY - oEnvelope.MinY) / poDS->padfYResolutions[0] + 0.5;
        if( !(dfRasterXSize >= 1 && dfRasterXSize <= INT_MAX) ||
            !(dfRasterYSize >= 1 && dfRasterYSize <= INT_MAX) )
        {
            delete poDS;
            poDS = nullptr;
            goto end;
        }
        poDS->nRasterXSize = static_cast<int>(dfRasterXSize);
        poDS->nRasterYSize = static_cast<int>(dfRasterYSize);

        poDS->bValidGeoTransform = TRUE;
        poDS->adfGeoTransform[0] = oEnvelope.MinX;
        poDS->adfGeoTransform[1] = poDS->padfXResolutions[0];
        poDS->adfGeoTransform[2] = 0;
        poDS->adfGeoTransform[3] = oEnvelope.MaxY;
        poDS->adfGeoTransform[4] = 0;
        poDS->adfGeoTransform[5] = - poDS->padfYResolutions[0];

        OGRSpatialReferenceH hSRS = OGR_L_GetSpatialRef(hMetadataLyr);
        if (hSRS)
        {
            OSRExportToWkt(hSRS, &poDS->pszSRS);
        }

/* -------------------------------------------------------------------- */
/*      Get number of bands and block size                              */
/* -------------------------------------------------------------------- */

        int nBands;
        int nBlockXSize, nBlockYSize;
        if (poDS->GetBlockParams(hRasterLyr, 0, &nBands, &eDataType,
                                 &nBlockXSize, &nBlockYSize) == FALSE)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find block characteristics");
            delete poDS;
            poDS = nullptr;
            goto end;
        }

        if (eDataType == GDT_Byte && nBands == 1 && nReqBands == 3)
            nBands = 3;
        else if (nReqBands != 0)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Parameters bands=%d ignored", nReqBands);
        }

/* -------------------------------------------------------------------- */
/*      Add bands                                                       */
/* -------------------------------------------------------------------- */

        for( int iBand=0; iBand < nBands; iBand++ )
            poDS->SetBand(iBand+1, new RasterliteBand(poDS, iBand+1, eDataType,
                                                      nBlockXSize, nBlockYSize));

/* -------------------------------------------------------------------- */
/*      Add overview levels as internal datasets                        */
/* -------------------------------------------------------------------- */
        if (nResolutions > 1)
        {
            poDS->papoOverviews = reinterpret_cast<RasterliteDataset **>(
                CPLCalloc(nResolutions - 1, sizeof(RasterliteDataset*)) );
            for( int nLev = 1; nLev < nResolutions; nLev++ )
            {
                int nOvrBands;
                GDALDataType eOvrDataType;
                if (poDS->GetBlockParams(hRasterLyr, nLev, &nOvrBands, &eOvrDataType,
                                         &nBlockXSize, &nBlockYSize) == FALSE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot find block characteristics for overview %d", nLev);
                    delete poDS;
                    poDS = nullptr;
                    goto end;
                }

                if (eDataType == GDT_Byte && nOvrBands == 1 && nReqBands == 3)
                    nOvrBands = 3;

                if (nBands != nOvrBands || eDataType != eOvrDataType)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Overview %d has not the same number characteristics as main band", nLev);
                    delete poDS;
                    poDS = nullptr;
                    goto end;
                }

                poDS->papoOverviews[nLev-1] = new RasterliteDataset(poDS, nLev);

                for( int iBand = 0; iBand < nBands; iBand++ )
                {
                    poDS->papoOverviews[nLev-1]->SetBand(iBand+1,
                        new RasterliteBand(poDS->papoOverviews[nLev-1], iBand+1, eDataType,
                                           nBlockXSize, nBlockYSize));
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Select an overview if the user has requested so                 */
/* -------------------------------------------------------------------- */
        if (nLevel == 0)
        {
        }
        else if (nLevel >= 1 && nLevel <= nResolutions - 1)
        {
            poDS->papoOverviews[nLevel-1]->bMustFree = TRUE;
            poDS = poDS->papoOverviews[nLevel-1];
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                      "Invalid requested level : %d. Must be >= 0 and <= %d",
                      nLevel, nResolutions - 1);
            delete poDS;
            poDS = nullptr;
        }
    }

    if (poDS)
    {
/* -------------------------------------------------------------------- */
/*      Setup PAM info for this subdatasets                             */
/* -------------------------------------------------------------------- */
        poDS->SetPhysicalFilename( osFileName.c_str() );

        CPLString osSubdatasetName;
        osSubdatasetName.Printf("RASTERLITE:%s:table=%s",
                                osFileName.c_str(), osTableName.c_str());
        poDS->SetSubdatasetName( osSubdatasetName.c_str() );
        poDS->TryLoadXML();
        poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );
    }

end:
    if (hDS)
        OGRReleaseDataSource(hDS);

    return poDS;
}

/************************************************************************/
/*                     GDALRegister_Rasterlite()                        */
/************************************************************************/

void GDALRegister_Rasterlite()

{
    if( !GDAL_CHECK_VERSION("Rasterlite driver") )
        return;

    if( GDALGetDriverByName( "Rasterlite" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "Rasterlite" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Rasterlite" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/rasterlite.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "sqlite" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 "
                               "Float64 CInt16 CInt32 CFloat32 CFloat64" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='WIPE' type='boolean' default='NO' description='Erase all preexisting data in the specified table'/>"
"   <Option name='TILED' type='boolean' default='YES' description='Use tiling'/>"
"   <Option name='BLOCKXSIZE' type='int' default='256' description='Tile Width'/>"
"   <Option name='BLOCKYSIZE' type='int' default='256' description='Tile Height'/>"
"   <Option name='DRIVER' type='string' description='GDAL driver to use for storing tiles' default='GTiff'/>"
"   <Option name='COMPRESS' type='string' description='(GTiff driver) Compression method' default='NONE'/>"
"   <Option name='QUALITY' type='int' description='(JPEG-compressed GTiff, JPEG and WEBP drivers) JPEG/WEBP Quality 1-100' default='75'/>"
"   <Option name='PHOTOMETRIC' type='string-select' description='(GTiff driver) Photometric interpretation'>"
"       <Value>MINISBLACK</Value>"
"       <Value>MINISWHITE</Value>"
"       <Value>PALETTE</Value>"
"       <Value>RGB</Value>"
"       <Value>CMYK</Value>"
"       <Value>YCBCR</Value>"
"       <Value>CIELAB</Value>"
"       <Value>ICCLAB</Value>"
"       <Value>ITULAB</Value>"
"   </Option>"
"</CreationOptionList>" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

#ifdef ENABLE_SQL_SQLITE_FORMAT
    poDriver->SetMetadataItem("ENABLE_SQL_SQLITE_FORMAT", "YES");
#endif

    poDriver->pfnOpen = RasterliteDataset::Open;
    poDriver->pfnIdentify = RasterliteDataset::Identify;
    poDriver->pfnCreateCopy = RasterliteCreateCopy;
    poDriver->pfnDelete = RasterliteDelete;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
