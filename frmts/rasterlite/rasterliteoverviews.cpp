/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2009, Even Rouault, <even dot rouault at mines dash paris dot org>
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

CPL_CVSID("$Id$");

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

    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    if (hSQLLyr == NULL)
    {
        if (hRasterPyramidsLyr == NULL)
            return CE_Failure;
            
        osSQL.Printf("SELECT DISTINCT(pixel_x_size), pixel_y_size "
                     "FROM \"%s_metadata\" WHERE pixel_x_size != 0  "
                     "ORDER BY pixel_x_size ASC",
                     osTableName.c_str());
                     
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
        if (hSQLLyr == NULL)
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */

    int i;
    for(i=1;i<nResolutions;i++)
        delete papoOverviews[i-1];
    CPLFree(papoOverviews);
    papoOverviews = NULL;
    CPLFree(padfXResolutions);
    padfXResolutions = NULL;
    CPLFree(padfYResolutions);
    padfYResolutions = NULL;
    
/* -------------------------------------------------------------------- */
/*      Rebuild arrays                                                  */
/* -------------------------------------------------------------------- */

    nResolutions = OGR_L_GetFeatureCount(hSQLLyr, TRUE);
    
    padfXResolutions =
        (double*)CPLMalloc(sizeof(double) * nResolutions);
    padfYResolutions =
        (double*)CPLMalloc(sizeof(double) * nResolutions);

    i = 0;
    OGRFeatureH hFeat;
    while((hFeat = OGR_L_GetNextFeature(hSQLLyr)) != NULL)
    {
        padfXResolutions[i] = OGR_F_GetFieldAsDouble(hFeat, 0);
        padfYResolutions[i] = OGR_F_GetFieldAsDouble(hFeat, 1);

        OGR_F_Destroy(hFeat);

        i ++;
    }

    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    hSQLLyr = NULL;

/* -------------------------------------------------------------------- */
/*      Add overview levels as internal datasets                        */
/* -------------------------------------------------------------------- */
    if (nResolutions > 1)
    {
        CPLString osRasterTableName = osTableName;
        osRasterTableName += "_rasters";
        
        OGRLayerH hRasterLyr = OGR_DS_GetLayerByName(hDS, osRasterTableName.c_str());
        
        papoOverviews = (RasterliteDataset**)
            CPLCalloc(nResolutions - 1, sizeof(RasterliteDataset*));
        int nLev;
        for(nLev=1;nLev<nResolutions;nLev++)
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
                  
                int iBand;
                for(iBand=0;iBand<nBands;iBand++)
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
                papoOverviews[nLev-1] = NULL;
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
    CPLString osSQL;
    
    if (nLevel != 0)
        return CE_Failure;
        
    osSQL.Printf("BEGIN");
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    
    CPLString osResolutionCond;
    osResolutionCond.Printf(
                 "(pixel_x_size < %.15f OR pixel_x_size > %.15f) AND "
                 "(pixel_y_size < %.15f OR pixel_y_size > %.15f)",
                 padfXResolutions[0] - 1e-15, padfXResolutions[0] + 1e-15,
                 padfYResolutions[0] - 1e-15, padfYResolutions[0] + 1e-15);
    
    osSQL.Printf("DELETE FROM \"%s_rasters\" WHERE id "
                 "IN(SELECT id FROM \"%s_metadata\" WHERE %s)",
                  osTableName.c_str(), osTableName.c_str(),
                  osResolutionCond.c_str());
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);

    osSQL.Printf("DELETE FROM \"%s_metadata\" WHERE %s",
                  osTableName.c_str(), osResolutionCond.c_str());
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    
    OGRLayerH hRasterPyramidsLyr = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
    if (hRasterPyramidsLyr)
    {
        osSQL.Printf("DELETE FROM raster_pyramids WHERE table_prefix = '%s' AND %s",
                      osTableName.c_str(), osResolutionCond.c_str());
        OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    }
    
    osSQL.Printf("COMMIT");
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    
    int i;
    for(i=1;i<nResolutions;i++)
        delete papoOverviews[i-1];
    CPLFree(papoOverviews);
    papoOverviews = NULL;
    nResolutions = 1;
    
    return CE_None;
}

/************************************************************************/
/*                       CleanOverviewLevel()                           */
/************************************************************************/

CPLErr RasterliteDataset::CleanOverviewLevel(int nOvrFactor)
{
    CPLString osSQL;
    
    if (nLevel != 0)
        return CE_Failure;
    
/* -------------------------------------------------------------------- */
/*      Find the index of the overview matching the overview factor     */
/* -------------------------------------------------------------------- */
    int iLev;
    for(iLev=1;iLev<nResolutions;iLev++)
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

    osSQL.Printf("BEGIN");
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    
    CPLString osResolutionCond;
    osResolutionCond.Printf(
                 "pixel_x_size >= %.15f AND pixel_x_size <= %.15f AND "
                 "pixel_y_size >= %.15f AND pixel_y_size <= %.15f",
                 padfXResolutions[iLev] - 1e-15, padfXResolutions[iLev] + 1e-15,
                 padfYResolutions[iLev] - 1e-15, padfYResolutions[iLev] + 1e-15);
    
    osSQL.Printf("DELETE FROM \"%s_rasters\" WHERE id "
                 "IN(SELECT id FROM \"%s_metadata\" WHERE %s)",
                  osTableName.c_str(), osTableName.c_str(),
                  osResolutionCond.c_str());
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);

    osSQL.Printf("DELETE FROM \"%s_metadata\" WHERE %s",
                  osTableName.c_str(), osResolutionCond.c_str());
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    
    OGRLayerH hRasterPyramidsLyr = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
    if (hRasterPyramidsLyr)
    {
        osSQL.Printf("DELETE FROM raster_pyramids WHERE table_prefix = '%s' AND %s",
                      osTableName.c_str(), osResolutionCond.c_str());
        OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    }
    
    osSQL.Printf("COMMIT");
    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    
    return CE_None;
}

/************************************************************************/
/*                       CleanOverviewLevel()                           */
/************************************************************************/

CPLErr RasterliteDataset::CreateOverviewLevel(int nOvrFactor,
                                              GDALProgressFunc pfnProgress,
                                              void * pProgressData)
{

    double dfXResolution = padfXResolutions[0] * nOvrFactor;
    double dfYResolution = padfXResolutions[0] * nOvrFactor;
    
    CPLString osSQL;

    int nBlockXSize = 256;
    int nBlockYSize = 256;
    int nOvrXSize = nRasterXSize / nOvrFactor;
    int nOvrYSize = nRasterYSize / nOvrFactor;
    
    if (nOvrXSize == 0 || nOvrYSize == 0)
        return CE_Failure;
    
    int nXBlocks = (nOvrXSize + nBlockXSize - 1) / nBlockXSize;
    int nYBlocks = (nOvrYSize + nBlockYSize - 1) / nBlockYSize;
    
    const char* pszDriverName = "GTiff";
    GDALDriverH hTileDriver = GDALGetDriverByName(pszDriverName);
    if (hTileDriver == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot load GDAL %s driver", pszDriverName);
        return CE_Failure;
    }
    
    GDALDriverH hMemDriver = GDALGetDriverByName("MEM");
    if (hMemDriver == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot load GDAL MEM driver");
        return CE_Failure;
    }   

    GDALDataType eDataType = GetRasterBand(1)->GetRasterDataType();
    int nDataTypeSize = GDALGetDataTypeSize(eDataType) / 8;
    GByte* pabyMEMDSBuffer =
        (GByte*)VSIMalloc3(nBlockXSize, nBlockYSize, nBands * nDataTypeSize);
    if (pabyMEMDSBuffer == NULL)
    {
        return CE_Failure;
    }
    
    char** papszTileDriverOptions = NULL;
    
    CPLString osTempFileName;
    osTempFileName.Printf("/vsimem/%p", hDS);
    
    int nTileId = 0;
    int nBlocks = 0;
    int nTotalBlocks = nXBlocks * nYBlocks;
    
    CPLString osRasterLayer;
    osRasterLayer.Printf("%s_rasters", osTableName.c_str());
    
    CPLString osMetatadataLayer;
    osMetatadataLayer.Printf("%s_metadata", osTableName.c_str());
    
    OGRLayerH hRasterLayer = OGR_DS_GetLayerByName(hDS, osRasterLayer.c_str());
    OGRLayerH hMetadataLayer = OGR_DS_GetLayerByName(hDS, osMetatadataLayer.c_str());
    
    CPLString osSourceName = "unknown";
    
    osSQL.Printf("SELECT source_name FROM \"%s\" WHERE "
                 "pixel_x_size >= %.15f AND pixel_x_size <= %.15f AND "
                 "pixel_y_size >= %.15f AND pixel_y_size <= %.15f LIMIT 1",
                 osMetatadataLayer.c_str(),
                 padfXResolutions[0] - 1e-15, padfXResolutions[0] + 1e-15,
                 padfYResolutions[0] - 1e-15, padfYResolutions[0] + 1e-15);
    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
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
    int iLev;
    nLimitOvrCount = 0;
    for(iLev=1;iLev<nResolutions;iLev++)
    {
        if (!(padfXResolutions[iLev] < dfXResolution - 1e-10 &&
              padfYResolutions[iLev] < dfYResolution - 1e-10))
        {
            break;
        }
        nLimitOvrCount++;
    }
    
/* -------------------------------------------------------------------- */
/*      Iterate over blocks to add data into raster and metadata tables */
/* -------------------------------------------------------------------- */

    OGR_DS_ExecuteSQL(hDS, "BEGIN", NULL, NULL);
    
    CPLErr eErr = CE_None;
    int nBlockXOff, nBlockYOff;
    for(nBlockYOff=0;eErr == CE_None && nBlockYOff<nYBlocks;nBlockYOff++)
    {
        for(nBlockXOff=0;eErr == CE_None && nBlockXOff<nXBlocks;nBlockXOff++)
        {
/* -------------------------------------------------------------------- */
/*      Create in-memory tile                                           */
/* -------------------------------------------------------------------- */
            int nReqXSize = nBlockXSize, nReqYSize = nBlockYSize;
            if ((nBlockXOff+1) * nBlockXSize > nOvrXSize)
                nReqXSize = nOvrXSize - nBlockXOff * nBlockXSize;
            if ((nBlockYOff+1) * nBlockYSize > nOvrYSize)
                nReqYSize = nOvrYSize - nBlockYOff * nBlockYSize;
            
            eErr = RasterIO(GF_Read,
                            nBlockXOff * nBlockXSize * nOvrFactor,
                            nBlockYOff * nBlockYSize * nOvrFactor,
                            nReqXSize * nOvrFactor, nReqYSize * nOvrFactor,
                            pabyMEMDSBuffer, nReqXSize, nReqYSize,
                            eDataType, nBands, NULL,
                            0, 0, 0);
            if (eErr != CE_None)
            {
                break;
            }
            
            GDALDatasetH hMemDS = GDALCreate(hMemDriver, "MEM:::",
                                              nReqXSize, nReqYSize, 0, 
                                              eDataType, NULL);
            if (hMemDS == NULL)
            {
                eErr = CE_Failure;
                break;
            }
            
            int iBand;
            for(iBand = 0; iBand < nBands; iBand ++)
            {
                char** papszOptions = NULL;
                char szTmp[64];
                memset(szTmp, 0, sizeof(szTmp));
                CPLPrintPointer(szTmp,
                                pabyMEMDSBuffer + iBand * nDataTypeSize *
                                nReqXSize * nReqYSize, sizeof(szTmp));
                papszOptions = CSLSetNameValue(papszOptions, "DATAPOINTER", szTmp);
                GDALAddBand(hMemDS, eDataType, papszOptions);
                CSLDestroy(papszOptions);
            }
            
            GDALDatasetH hOutDS = GDALCreateCopy(hTileDriver,
                                        osTempFileName.c_str(), hMemDS, FALSE,
                                        papszTileDriverOptions, NULL, NULL);

            GDALClose(hMemDS);
            if (hOutDS)
                GDALClose(hOutDS);
            else
            {
                eErr = CE_Failure;
                break;
            }

/* -------------------------------------------------------------------- */
/*      Insert new entry into raster table                              */
/* -------------------------------------------------------------------- */

            vsi_l_offset nDataLength;
            GByte *pabyData = VSIGetMemFileBuffer( osTempFileName.c_str(),
                                                   &nDataLength, FALSE);

            OGRFeatureH hFeat = OGR_F_Create( OGR_L_GetLayerDefn(hRasterLayer) );
            OGR_F_SetFieldBinary(hFeat, 0, (int)nDataLength, pabyData);
            
            OGR_L_CreateFeature(hRasterLayer, hFeat);
            /* Query raster ID to set it as the ID of the associated metadata */
            int nRasterID = (int)OGR_F_GetFID(hFeat);
            
            OGR_F_Destroy(hFeat);
            
            VSIUnlink(osTempFileName.c_str());
            
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
            
            double minx, maxx, maxy, miny;
            minx = adfGeoTransform[0] +
                (nBlockXSize * nBlockXOff) * dfXResolution;
            maxx = adfGeoTransform[0] +
                (nBlockXSize * nBlockXOff + nReqXSize) * dfXResolution;
            maxy = adfGeoTransform[3] +
                (nBlockYSize * nBlockYOff) * (-dfYResolution);
            miny = adfGeoTransform[3] +
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
            
            OGR_L_CreateFeature(hMetadataLayer, hFeat);
            OGR_F_Destroy(hFeat);
            
            nBlocks++;
            if (pfnProgress && !pfnProgress(1.0 * nBlocks / nTotalBlocks,
                                            NULL, pProgressData))
                eErr = CE_Failure;
        }
    }
    
    nLimitOvrCount = -1;
    
    if (eErr == CE_None)
        OGR_DS_ExecuteSQL(hDS, "COMMIT", NULL, NULL);
    else
        OGR_DS_ExecuteSQL(hDS, "ROLLBACK", NULL, NULL);
    
    VSIFree(pabyMEMDSBuffer);
    
/* -------------------------------------------------------------------- */
/*      Update raster_pyramids table                                    */
/* -------------------------------------------------------------------- */
    if (eErr == CE_None)
    {
        OGRLayerH hRasterPyramidsLyr = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
        if (hRasterPyramidsLyr == NULL)
        {
            osSQL.Printf   ("CREATE TABLE raster_pyramids ("
                            "table_prefix TEXT NOT NULL,"
                            "pixel_x_size DOUBLE NOT NULL,"
                            "pixel_y_size DOUBLE NOT NULL,"
                            "tile_count INTEGER NOT NULL)");
            OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
            
            /* Re-open the DB to take into account the new tables*/
            OGRReleaseDataSource(hDS);
            
            CPLString osOldVal = CPLGetConfigOption("SQLITE_LIST_ALL_TABLES", "FALSE");
            CPLSetConfigOption("SQLITE_LIST_ALL_TABLES", "TRUE");
            hDS = OGROpen(osFileName.c_str(), TRUE, NULL);
            CPLSetConfigOption("SQLITE_LIST_ALL_TABLES", osOldVal.c_str());
            
            osSQL.Printf("SELECT COUNT(*) FROM \"%s\" WHERE "
                          "pixel_x_size >= %.15f AND pixel_x_size <= %.15f AND "
                          "pixel_y_size >= %.15f AND pixel_y_size <= %.15f",
                          osMetatadataLayer.c_str(),
                          padfXResolutions[0] - 1e-15, padfXResolutions[0] + 1e-15,
                          padfYResolutions[0] - 1e-15, padfYResolutions[0] + 1e-15);
                          
            int nBlocksMainRes = 0;
            
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
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
            
            osSQL.Printf("INSERT INTO raster_pyramids "
                         "( table_prefix, pixel_x_size, pixel_y_size, tile_count ) "
                         "VALUES ( '%s', %.18f, %.18f, %d )",
                         osTableName.c_str(), padfXResolutions[0], padfYResolutions[0],
                         nBlocksMainRes);
            OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
        }
        
        osSQL.Printf("INSERT INTO raster_pyramids "
                     "( table_prefix, pixel_x_size, pixel_y_size, tile_count ) "
                     "VALUES ( '%s', %.18f, %.18f, %d )",
                     osTableName.c_str(), dfXResolution, dfYResolution,
                     nTotalBlocks);
        OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    }

    return eErr;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr RasterliteDataset::IBuildOverviews( const char * pszResampling, 
                                           int nOverviews, int * panOverviewList,
                                           int nBands, int * panBandList,
                                           GDALProgressFunc pfnProgress,
                                           void * pProgressData )
{
    CPLErr eErr = CE_None;
    
    if (nLevel != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Overviews can only be computed on the base dataset");
        return CE_Failure;
    }
        
    if (osTableName.size() == 0)
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
        eErr = GDALDataset::IBuildOverviews( 
                            pszResampling, nOverviews, panOverviewList, 
                            nBands, panBandList, pfnProgress, pProgressData );
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
    
    if( nBands != GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in RASTERLITE only"
                  " supported when operating on all bands.\n" 
                  "Operation failed.\n" );
        return CE_Failure;
    }
    
    if( !EQUALN(pszResampling, "NEAR", 4))
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Only NEAREST resampling is allowed for now "
                  "for RASTERLITE overviews");
        return CE_Failure;
    }
    
    int i;
    for(i=0;i<nOverviews && eErr == CE_None;i++)
    {
        if (panOverviewList[i] <= 1)
            continue;

        eErr = CleanOverviewLevel(panOverviewList[i]);
        if (eErr == CE_None)
            eErr = CreateOverviewLevel(panOverviewList[i], pfnProgress, pProgressData);
    
        ReloadOverviews();
    }
    
    return eErr;
}
