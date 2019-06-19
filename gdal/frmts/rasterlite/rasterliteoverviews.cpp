/******************************************************************************
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_api.h"
#include "ogr_srs_api.h"

#include "rasterlitedataset.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         ReloadOverviews()                            */
/************************************************************************/

CPLErr RasterliteDataset::ReloadOverviews()
{
    if (nLevel != 0)
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Fetch resolutions                                               */
/* -------------------------------------------------------------------- */

    CPLString osSQL;
    OGRLayerH hRasterPyramidsLyr = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
    if (hRasterPyramidsLyr)
    {
        osSQL.Printf("SELECT pixel_x_size, pixel_y_size "
                     "FROM raster_pyramids WHERE table_prefix = '%s' "
                     "ORDER BY pixel_x_size ASC",
                     osTableName.c_str());
     }
     else
     {
        osSQL.Printf("SELECT DISTINCT(pixel_x_size), pixel_y_size "
                     "FROM \"%s_metadata\" WHERE pixel_x_size != 0  "
                     "ORDER BY pixel_x_size ASC",
                     osTableName.c_str());
     }

    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
    if (hSQLLyr == nullptr)
    {
        if (hRasterPyramidsLyr == nullptr)
            return CE_Failure;

        osSQL.Printf("SELECT DISTINCT(pixel_x_size), pixel_y_size "
                     "FROM \"%s_metadata\" WHERE pixel_x_size != 0  "
                     "ORDER BY pixel_x_size ASC",
                     osTableName.c_str());

        hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
        if (hSQLLyr == nullptr)
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    for( int i = 1; i < nResolutions; i++ )
        delete papoOverviews[i-1];
    CPLFree(papoOverviews);
    papoOverviews = nullptr;
    CPLFree(padfXResolutions);
    padfXResolutions = nullptr;
    CPLFree(padfYResolutions);
    padfYResolutions = nullptr;

/* -------------------------------------------------------------------- */
/*      Rebuild arrays                                                  */
/* -------------------------------------------------------------------- */

    nResolutions = static_cast<int>(OGR_L_GetFeatureCount(hSQLLyr, TRUE));

    padfXResolutions =
        reinterpret_cast<double *>( CPLMalloc(sizeof(double) * nResolutions) );
    padfYResolutions =
        reinterpret_cast<double *>( CPLMalloc(sizeof(double) * nResolutions) );

    {
        // Exstra scope for i.
        int i = 0;
        OGRFeatureH hFeat;
        while((hFeat = OGR_L_GetNextFeature(hSQLLyr)) != nullptr)
        {
            padfXResolutions[i] = OGR_F_GetFieldAsDouble(hFeat, 0);
            padfYResolutions[i] = OGR_F_GetFieldAsDouble(hFeat, 1);

            OGR_F_Destroy(hFeat);

            i++;
        }
    }

    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    hSQLLyr = nullptr;

/* -------------------------------------------------------------------- */
/*      Add overview levels as internal datasets                        */
/* -------------------------------------------------------------------- */
    if (nResolutions > 1)
    {
        CPLString osRasterTableName = osTableName;
        osRasterTableName += "_rasters";

        OGRLayerH hRasterLyr = OGR_DS_GetLayerByName(hDS, osRasterTableName.c_str());

        papoOverviews = reinterpret_cast<RasterliteDataset **>(
            CPLCalloc( nResolutions - 1, sizeof(RasterliteDataset*) ) );
        for( int nLev = 1; nLev < nResolutions; nLev++ )
        {
            int nOvrBands;
            GDALDataType eOvrDataType;
            int nBlockXSize, nBlockYSize;
            if (GetBlockParams(hRasterLyr, nLev, &nOvrBands, &eOvrDataType,
                               &nBlockXSize, &nBlockYSize))
            {
                if (eOvrDataType == GDT_Byte && nOvrBands == 1 && nBands == 3)
                    nOvrBands = 3;

                papoOverviews[nLev-1] = new RasterliteDataset(this, nLev);

                for( int iBand = 0; iBand < nBands; iBand++ )
                {
                    papoOverviews[nLev-1]->SetBand(iBand+1,
                        new RasterliteBand(papoOverviews[nLev-1], iBand+1, eOvrDataType,
                                           nBlockXSize, nBlockYSize));
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find block characteristics for overview %d", nLev);
                papoOverviews[nLev-1] = nullptr;
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                          CleanOverviews()                            */
/************************************************************************/

CPLErr RasterliteDataset::CleanOverviews()
{
    if (nLevel != 0)
        return CE_Failure;

    CPLString osSQL("BEGIN");
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);

    const CPLString osResolutionCond =
        "NOT " + RasterliteGetPixelSizeCond(padfXResolutions[0], padfYResolutions[0]);

    osSQL.Printf("DELETE FROM \"%s_rasters\" WHERE id "
                 "IN(SELECT id FROM \"%s_metadata\" WHERE %s)",
                  osTableName.c_str(), osTableName.c_str(),
                  osResolutionCond.c_str());
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);

    osSQL.Printf("DELETE FROM \"%s_metadata\" WHERE %s",
                  osTableName.c_str(), osResolutionCond.c_str());
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);

    OGRLayerH hRasterPyramidsLyr = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
    if (hRasterPyramidsLyr)
    {
        osSQL.Printf("DELETE FROM raster_pyramids WHERE table_prefix = '%s' AND %s",
                      osTableName.c_str(), osResolutionCond.c_str());
        OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
    }

    osSQL = "COMMIT";
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);

    for( int i = 1; i < nResolutions; i++ )
        delete papoOverviews[i-1];
    CPLFree(papoOverviews);
    papoOverviews = nullptr;
    nResolutions = 1;

    return CE_None;
}

/************************************************************************/
/*                       CleanOverviewLevel()                           */
/************************************************************************/

CPLErr RasterliteDataset::CleanOverviewLevel(int nOvrFactor)
{
    if (nLevel != 0)
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Find the index of the overview matching the overview factor     */
/* -------------------------------------------------------------------- */
    int iLev = 1;
    for( ; iLev < nResolutions; iLev++ )
    {
        if (fabs(padfXResolutions[0] * nOvrFactor - padfXResolutions[iLev]) < 1e-15 &&
            fabs(padfYResolutions[0] * nOvrFactor - padfYResolutions[iLev]) < 1e-15)
            break;
    }

    if (iLev == nResolutions)
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Now clean existing overviews at that resolution                 */
/* -------------------------------------------------------------------- */

    CPLString osSQL("BEGIN");
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);

    CPLString osResolutionCond =
        RasterliteGetPixelSizeCond(padfXResolutions[iLev], padfYResolutions[iLev]);

    osSQL.Printf("DELETE FROM \"%s_rasters\" WHERE id "
                 "IN(SELECT id FROM \"%s_metadata\" WHERE %s)",
                  osTableName.c_str(), osTableName.c_str(),
                  osResolutionCond.c_str());
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);

    osSQL.Printf("DELETE FROM \"%s_metadata\" WHERE %s",
                  osTableName.c_str(), osResolutionCond.c_str());
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);

    OGRLayerH hRasterPyramidsLyr = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
    if (hRasterPyramidsLyr)
    {
        osSQL.Printf("DELETE FROM raster_pyramids WHERE table_prefix = '%s' AND %s",
                      osTableName.c_str(), osResolutionCond.c_str());
        OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
    }

    osSQL = "COMMIT";
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);

    return CE_None;
}

/************************************************************************/
/*                       CleanOverviewLevel()                           */
/************************************************************************/

CPLErr RasterliteDataset::CreateOverviewLevel(const char * pszResampling,
                                              int nOvrFactor,
                                              char** papszOptions,
                                              GDALProgressFunc pfnProgress,
                                              void * pProgressData)
{
    const int nOvrXSize = nRasterXSize / nOvrFactor;
    const int nOvrYSize = nRasterYSize / nOvrFactor;

    if (nOvrXSize == 0 || nOvrYSize == 0)
        return CE_Failure;

    const bool bTiled
        = CPLTestBool(CSLFetchNameValueDef(papszOptions, "TILED", "YES"));
    int nBlockXSize, nBlockYSize;
    if (bTiled)
    {
        nBlockXSize = atoi(CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", "256"));
        nBlockYSize = atoi(CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", "256"));
        if (nBlockXSize < 64) nBlockXSize = 64;
        else if (nBlockXSize > 4096)  nBlockXSize = 4096;
        if (nBlockYSize < 64) nBlockYSize = 64;
        else if (nBlockYSize > 4096)  nBlockYSize = 4096;
    }
    else
    {
        nBlockXSize = nOvrXSize;
        nBlockYSize = nOvrYSize;
    }

    const char* pszDriverName = CSLFetchNameValueDef(papszOptions, "DRIVER", "GTiff");
    if (EQUAL(pszDriverName, "MEM") || EQUAL(pszDriverName, "VRT"))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GDAL %s driver cannot be used as underlying driver",
                 pszDriverName);
        return CE_Failure;
    }
    GDALDriverH hTileDriver = GDALGetDriverByName(pszDriverName);
    if (hTileDriver == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot load GDAL %s driver", pszDriverName);
        return CE_Failure;
    }

    GDALDriverH hMemDriver = GDALGetDriverByName("MEM");
    if (hMemDriver == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot load GDAL MEM driver");
        return CE_Failure;
    }

    const GDALDataType eDataType = GetRasterBand(1)->GetRasterDataType();
    int nDataTypeSize = GDALGetDataTypeSize(eDataType) / 8;
    GByte* pabyMEMDSBuffer = reinterpret_cast<GByte *>(
        VSIMalloc3( nBlockXSize, nBlockYSize, nBands * nDataTypeSize ) );
    if (pabyMEMDSBuffer == nullptr)
    {
        return CE_Failure;
    }

    CPLString osTempFileName;
    osTempFileName.Printf("/vsimem/%p", hDS);

    int nTileId = 0;
    int nBlocks = 0;

    const int nXBlocks = (nOvrXSize + nBlockXSize - 1) / nBlockXSize;
    const int nYBlocks = (nOvrYSize + nBlockYSize - 1) / nBlockYSize;
    int nTotalBlocks = nXBlocks * nYBlocks;

    CPLString osRasterLayer;
    osRasterLayer.Printf("%s_rasters", osTableName.c_str());

    CPLString osMetatadataLayer;
    osMetatadataLayer.Printf("%s_metadata", osTableName.c_str());

    OGRLayerH hRasterLayer = OGR_DS_GetLayerByName(hDS, osRasterLayer.c_str());
    OGRLayerH hMetadataLayer = OGR_DS_GetLayerByName(hDS, osMetatadataLayer.c_str());

    CPLString osSourceName = "unknown";

    const double dfXResolution = padfXResolutions[0] * nOvrFactor;
    const double dfYResolution = padfXResolutions[0] * nOvrFactor;

    CPLString osSQL;
    osSQL.Printf("SELECT source_name FROM \"%s\" WHERE "
                 "%s LIMIT 1",
                 osMetatadataLayer.c_str(),
                 RasterliteGetPixelSizeCond(padfXResolutions[0], padfYResolutions[0]).c_str());
    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
    if (hSQLLyr)
    {
        OGRFeatureH hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat)
        {
            const char* pszVal = OGR_F_GetFieldAsString(hFeat, 0);
            if (pszVal)
                osSourceName = pszVal;
            OGR_F_Destroy(hFeat);
        }
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    }

/* -------------------------------------------------------------------- */
/*      Compute up to which existing overview level we can use for      */
/*      computing the requested overview                                */
/* -------------------------------------------------------------------- */
    nLimitOvrCount = 0;
    int iLev = 1;
    for( ; iLev < nResolutions; iLev++ )
    {
        if (!(padfXResolutions[iLev] < dfXResolution - 1e-10 &&
              padfYResolutions[iLev] < dfYResolution - 1e-10))
        {
            break;
        }
        nLimitOvrCount++;
    }
/* -------------------------------------------------------------------- */
/*      Allocate buffer for tile of previous overview level             */
/* -------------------------------------------------------------------- */

    GDALDataset* poPrevOvrLevel =
        (papoOverviews != nullptr && iLev >= 2 && iLev <= nResolutions && papoOverviews[iLev-2]) ?
            papoOverviews[iLev-2] : this;
    const double dfRatioPrevOvr = static_cast<double>(poPrevOvrLevel->GetRasterBand(1)->GetXSize()) / nOvrXSize;
    const int nPrevOvrBlockXSize = static_cast<int>( nBlockXSize * dfRatioPrevOvr + 0.5 );
    const int nPrevOvrBlockYSize = static_cast<int>(nBlockYSize * dfRatioPrevOvr + 0.5 );
    GByte* pabyPrevOvrMEMDSBuffer = nullptr;

    if( !STARTS_WITH_CI(pszResampling, "NEAR"))
    {
        pabyPrevOvrMEMDSBuffer = reinterpret_cast<GByte*>(
            VSIMalloc3( nPrevOvrBlockXSize, nPrevOvrBlockYSize, nBands * nDataTypeSize ) );
        if (pabyPrevOvrMEMDSBuffer == nullptr)
        {
            VSIFree(pabyMEMDSBuffer);
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Iterate over blocks to add data into raster and metadata tables */
/* -------------------------------------------------------------------- */

    char** papszTileDriverOptions = RasterliteGetTileDriverOptions(papszOptions);

    OGR_DS_ExecuteSQL(hDS, "BEGIN", nullptr, nullptr);

    CPLErr eErr = CE_None;
    for( int nBlockYOff = 0; eErr == CE_None && nBlockYOff < nYBlocks; nBlockYOff++ )
    {
        for( int nBlockXOff = 0; eErr == CE_None && nBlockXOff < nXBlocks; nBlockXOff++ )
        {
            GDALDatasetH hPrevOvrMemDS = nullptr;

/* -------------------------------------------------------------------- */
/*      Create in-memory tile                                           */
/* -------------------------------------------------------------------- */
            int nReqXSize = nBlockXSize;
            int nReqYSize = nBlockYSize;
            if ((nBlockXOff+1) * nBlockXSize > nOvrXSize)
                nReqXSize = nOvrXSize - nBlockXOff * nBlockXSize;
            if ((nBlockYOff+1) * nBlockYSize > nOvrYSize)
                nReqYSize = nOvrYSize - nBlockYOff * nBlockYSize;

            if( pabyPrevOvrMEMDSBuffer != nullptr )
            {
                int nPrevOvrReqXSize =
                    static_cast<int>( nReqXSize * dfRatioPrevOvr + 0.5 );
                int nPrevOvrReqYSize =
                    static_cast<int>(nReqYSize * dfRatioPrevOvr + 0.5 );

                eErr = RasterIO(GF_Read,
                                nBlockXOff * nBlockXSize * nOvrFactor,
                                nBlockYOff * nBlockYSize * nOvrFactor,
                                nReqXSize * nOvrFactor, nReqYSize * nOvrFactor,
                                pabyPrevOvrMEMDSBuffer, nPrevOvrReqXSize, nPrevOvrReqYSize,
                                eDataType, nBands, nullptr,
                                0, 0, 0, nullptr);

                if (eErr != CE_None)
                {
                    break;
                }

                hPrevOvrMemDS = GDALCreate(hMemDriver, "MEM:::",
                                           nPrevOvrReqXSize, nPrevOvrReqYSize, 0,
                                           eDataType, nullptr);

                if (hPrevOvrMemDS == nullptr)
                {
                    eErr = CE_Failure;
                    break;
                }

                for( int iBand = 0; iBand < nBands; iBand++ )
                {
                    char szTmp[64];
                    memset(szTmp, 0, sizeof(szTmp));
                    CPLPrintPointer(szTmp,
                                    pabyPrevOvrMEMDSBuffer + iBand * nDataTypeSize *
                                    nPrevOvrReqXSize * nPrevOvrReqYSize, sizeof(szTmp));
                    char** l_papszOptions
                        = CSLSetNameValue(nullptr, "DATAPOINTER", szTmp);
                    GDALAddBand(hPrevOvrMemDS, eDataType, l_papszOptions);
                    CSLDestroy(l_papszOptions);
                }
            }
            else
            {
                eErr = RasterIO(GF_Read,
                                nBlockXOff * nBlockXSize * nOvrFactor,
                                nBlockYOff * nBlockYSize * nOvrFactor,
                                nReqXSize * nOvrFactor, nReqYSize * nOvrFactor,
                                pabyMEMDSBuffer, nReqXSize, nReqYSize,
                                eDataType, nBands, nullptr,
                                0, 0, 0, nullptr);
                if (eErr != CE_None)
                {
                    break;
                }
            }

            GDALDatasetH hMemDS = GDALCreate(hMemDriver, "MEM:::",
                                              nReqXSize, nReqYSize, 0,
                                              eDataType, nullptr);
            if (hMemDS == nullptr)
            {
                eErr = CE_Failure;
                break;
            }

            for(int iBand = 0; iBand < nBands; iBand ++)
            {
                char szTmp[64];
                memset(szTmp, 0, sizeof(szTmp));
                CPLPrintPointer(szTmp,
                                pabyMEMDSBuffer + iBand * nDataTypeSize *
                                nReqXSize * nReqYSize, sizeof(szTmp));
                char** l_papszOptions
                    = CSLSetNameValue(nullptr, "DATAPOINTER", szTmp);
                GDALAddBand(hMemDS, eDataType, l_papszOptions);
                CSLDestroy(l_papszOptions);
            }

            if( hPrevOvrMemDS != nullptr )
            {
                for(int iBand = 0; iBand < nBands; iBand ++)
                {
                    GDALRasterBandH hDstOvrBand = GDALGetRasterBand(hMemDS, iBand+1);

                    eErr = GDALRegenerateOverviews( GDALGetRasterBand(hPrevOvrMemDS, iBand+1),
                                                    1, &hDstOvrBand,
                                                    pszResampling,
                                                    nullptr, nullptr );
                    if( eErr != CE_None )
                        break;
                }

                GDALClose(hPrevOvrMemDS);
            }

            GDALDatasetH hOutDS = GDALCreateCopy(hTileDriver,
                                        osTempFileName.c_str(), hMemDS, FALSE,
                                        papszTileDriverOptions, nullptr, nullptr);

            GDALClose(hMemDS);
            if (! hOutDS)
            {
                eErr = CE_Failure;
                break;
            }

            GDALClose(hOutDS);

/* -------------------------------------------------------------------- */
/*      Insert new entry into raster table                              */
/* -------------------------------------------------------------------- */

            vsi_l_offset nDataLength;
            GByte *pabyData = VSIGetMemFileBuffer( osTempFileName.c_str(),
                                                   &nDataLength, FALSE);

            OGRFeatureH hFeat = OGR_F_Create( OGR_L_GetLayerDefn(hRasterLayer) );
            OGR_F_SetFieldBinary(hFeat, 0, static_cast<int>( nDataLength ), pabyData);

            if( OGR_L_CreateFeature(hRasterLayer, hFeat) != OGRERR_NONE )
                eErr = CE_Failure;
            /* Query raster ID to set it as the ID of the associated metadata */
            const int nRasterID = static_cast<int>( OGR_F_GetFID(hFeat) );

            OGR_F_Destroy(hFeat);

            VSIUnlink(osTempFileName.c_str());
            if( eErr == CE_Failure )
                break;

/* -------------------------------------------------------------------- */
/*      Insert new entry into metadata table                            */
/* -------------------------------------------------------------------- */

            hFeat = OGR_F_Create( OGR_L_GetLayerDefn(hMetadataLayer) );
            OGR_F_SetFID(hFeat, nRasterID);
            OGR_F_SetFieldString(hFeat, 0, osSourceName);
            OGR_F_SetFieldInteger(hFeat, 1, nTileId ++);
            OGR_F_SetFieldInteger(hFeat, 2, nReqXSize);
            OGR_F_SetFieldInteger(hFeat, 3, nReqYSize);
            OGR_F_SetFieldDouble(hFeat, 4, dfXResolution);
            OGR_F_SetFieldDouble(hFeat, 5, dfYResolution);

            const double minx = adfGeoTransform[0] +
                (nBlockXSize * nBlockXOff) * dfXResolution;
            const double maxx = adfGeoTransform[0] +
                (nBlockXSize * nBlockXOff + nReqXSize) * dfXResolution;
            const double maxy = adfGeoTransform[3] +
                (nBlockYSize * nBlockYOff) * (-dfYResolution);
            const double miny = adfGeoTransform[3] +
                (nBlockYSize * nBlockYOff + nReqYSize) * (-dfYResolution);

            OGRGeometryH hRectangle = OGR_G_CreateGeometry(wkbPolygon);
            OGRGeometryH hLinearRing = OGR_G_CreateGeometry(wkbLinearRing);
            OGR_G_AddPoint_2D(hLinearRing, minx, miny);
            OGR_G_AddPoint_2D(hLinearRing, minx, maxy);
            OGR_G_AddPoint_2D(hLinearRing, maxx, maxy);
            OGR_G_AddPoint_2D(hLinearRing, maxx, miny);
            OGR_G_AddPoint_2D(hLinearRing, minx, miny);
            OGR_G_AddGeometryDirectly(hRectangle, hLinearRing);

            OGR_F_SetGeometryDirectly(hFeat, hRectangle);

            if( OGR_L_CreateFeature(hMetadataLayer, hFeat) != OGRERR_NONE )
                eErr = CE_Failure;
            OGR_F_Destroy(hFeat);

            nBlocks++;
            if (pfnProgress && !pfnProgress(1.0 * nBlocks / nTotalBlocks,
                                            nullptr, pProgressData))
                eErr = CE_Failure;
        }
    }

    nLimitOvrCount = -1;

    VSIUnlink(osTempFileName);
    VSIUnlink((osTempFileName + ".aux.xml").c_str());

    if (eErr == CE_None)
        OGR_DS_ExecuteSQL(hDS, "COMMIT", nullptr, nullptr);
    else
        OGR_DS_ExecuteSQL(hDS, "ROLLBACK", nullptr, nullptr);

    VSIFree(pabyMEMDSBuffer);
    VSIFree(pabyPrevOvrMEMDSBuffer);

    CSLDestroy(papszTileDriverOptions);
    papszTileDriverOptions = nullptr;

/* -------------------------------------------------------------------- */
/*      Update raster_pyramids table                                    */
/* -------------------------------------------------------------------- */
    if (eErr != CE_None)
        return eErr;

    OGRLayerH hRasterPyramidsLyr = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
    if (hRasterPyramidsLyr == nullptr)
    {
        osSQL.Printf   ("CREATE TABLE raster_pyramids ("
                        "table_prefix TEXT NOT NULL,"
                        "pixel_x_size DOUBLE NOT NULL,"
                        "pixel_y_size DOUBLE NOT NULL,"
                        "tile_count INTEGER NOT NULL)");
        OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);

        /* Re-open the DB to take into account the new tables*/
        OGRReleaseDataSource(hDS);

        hDS = RasterliteOpenSQLiteDB(osFileName.c_str(), GA_Update);

        hRasterPyramidsLyr = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
        if (hRasterPyramidsLyr == nullptr)
            return CE_Failure;
    }
    OGRFeatureDefnH hFDefn = OGR_L_GetLayerDefn(hRasterPyramidsLyr);

    /* Insert base resolution into raster_pyramids if not already done */
    bool bHasBaseResolution = false;
    osSQL.Printf("SELECT * FROM raster_pyramids WHERE "
                 "table_prefix = '%s' AND %s",
                 osTableName.c_str(),
                 RasterliteGetPixelSizeCond(padfXResolutions[0], padfYResolutions[0]).c_str());
    hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
    if (hSQLLyr)
    {
        OGRFeatureH hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat)
        {
            bHasBaseResolution = true;
            OGR_F_Destroy(hFeat);
        }
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    }

    if (!bHasBaseResolution)
    {
        osSQL.Printf("SELECT COUNT(*) FROM \"%s\" WHERE %s",
                      osMetatadataLayer.c_str(),
                      RasterliteGetPixelSizeCond(padfXResolutions[0], padfYResolutions[0]).c_str());

        int nBlocksMainRes = 0;

        hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), nullptr, nullptr);
        if (hSQLLyr)
        {
            OGRFeatureH hFeat = OGR_L_GetNextFeature(hSQLLyr);
            if (hFeat)
            {
                nBlocksMainRes = OGR_F_GetFieldAsInteger(hFeat, 0);
                OGR_F_Destroy(hFeat);
            }
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        }

        OGRFeatureH hFeat = OGR_F_Create( hFDefn );
        OGR_F_SetFieldString(hFeat, OGR_FD_GetFieldIndex(hFDefn, "table_prefix"), osTableName.c_str());
        OGR_F_SetFieldDouble(hFeat, OGR_FD_GetFieldIndex(hFDefn, "pixel_x_size"), padfXResolutions[0]);
        OGR_F_SetFieldDouble(hFeat, OGR_FD_GetFieldIndex(hFDefn, "pixel_y_size"), padfYResolutions[0]);
        OGR_F_SetFieldInteger(hFeat, OGR_FD_GetFieldIndex(hFDefn, "tile_count"), nBlocksMainRes);
        if( OGR_L_CreateFeature(hRasterPyramidsLyr, hFeat) != OGRERR_NONE )
            eErr = CE_Failure;
        OGR_F_Destroy(hFeat);
    }

    OGRFeatureH hFeat = OGR_F_Create( hFDefn );
    OGR_F_SetFieldString(hFeat, OGR_FD_GetFieldIndex(hFDefn, "table_prefix"), osTableName.c_str());
    OGR_F_SetFieldDouble(hFeat, OGR_FD_GetFieldIndex(hFDefn, "pixel_x_size"), dfXResolution);
    OGR_F_SetFieldDouble(hFeat, OGR_FD_GetFieldIndex(hFDefn, "pixel_y_size"), dfYResolution);
    OGR_F_SetFieldInteger(hFeat, OGR_FD_GetFieldIndex(hFDefn, "tile_count"), nTotalBlocks);
    if( OGR_L_CreateFeature(hRasterPyramidsLyr, hFeat) != OGRERR_NONE )
        eErr = CE_Failure;
    OGR_F_Destroy(hFeat);

    return eErr;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr RasterliteDataset::IBuildOverviews( const char * pszResampling,
                                           int nOverviews, int * panOverviewList,
                                           int nBandsIn, int * panBandList,
                                           GDALProgressFunc pfnProgress,
                                           void * pProgressData )
{
    if (nLevel != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Overviews can only be computed on the base dataset");
        return CE_Failure;
    }

    if (osTableName.empty())
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      If we don't have read access, then create the overviews         */
/*      externally.                                                     */
/* -------------------------------------------------------------------- */
    if( GetAccess() != GA_Update )
    {
        CPLDebug( "Rasterlite",
                  "File open for read-only accessing, "
                  "creating overviews externally." );

        if (nResolutions != 1)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot add external overviews to a "
                     "dataset with internal overviews");
            return CE_Failure;
        }

        bCheckForExistingOverview = FALSE;
        CPLErr eErr = GDALDataset::IBuildOverviews(
                            pszResampling, nOverviews, panOverviewList,
                            nBandsIn, panBandList, pfnProgress, pProgressData );
        bCheckForExistingOverview = TRUE;
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      If zero overviews were requested, we need to clear all          */
/*      existing overviews.                                             */
/* -------------------------------------------------------------------- */
    if (nOverviews == 0)
    {
        return CleanOverviews();
    }

    if( nBandsIn != GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in RASTERLITE only"
                  " supported when operating on all bands.\n"
                  "Operation failed.\n" );
        return CE_Failure;
    }

    const char* pszOvrOptions = CPLGetConfigOption("RASTERLITE_OVR_OPTIONS", nullptr);
    char** papszOptions = (pszOvrOptions) ? CSLTokenizeString2( pszOvrOptions, ",", 0) : nullptr;
    GDALValidateCreationOptions( GetDriver(), papszOptions);

    CPLErr eErr = CE_None;
    for( int i = 0; i < nOverviews && eErr == CE_None; i++)
    {
        if (panOverviewList[i] <= 1)
            continue;

        eErr = CleanOverviewLevel(panOverviewList[i]);
        if (eErr == CE_None)
            eErr = CreateOverviewLevel(pszResampling, panOverviewList[i], papszOptions, pfnProgress, pProgressData);

        ReloadOverviews();
    }

    CSLDestroy(papszOptions);

    return eErr;
}
