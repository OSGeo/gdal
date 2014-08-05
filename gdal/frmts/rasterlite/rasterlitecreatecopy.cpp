/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
/*                  RasterliteGetTileDriverOptions ()                   */
/************************************************************************/

static char** RasterliteAddTileDriverOptionsForDriver(char** papszOptions,
                                                    char** papszTileDriverOptions,
                                                    const char* pszOptionName,
                                                    const char* pszExpectedDriverName)
{
    const char* pszVal = CSLFetchNameValue(papszOptions, pszOptionName);
    if (pszVal)
    {
        const char* pszDriverName =
            CSLFetchNameValueDef(papszOptions, "DRIVER", "GTiff");
        if (EQUAL(pszDriverName, pszExpectedDriverName))
        {
            papszTileDriverOptions =
                CSLSetNameValue(papszTileDriverOptions, pszOptionName, pszVal);
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unexpected option '%s' for driver '%s'",
                     pszOptionName, pszDriverName);
        }
    }
    return papszTileDriverOptions;
}

char** RasterliteGetTileDriverOptions(char** papszOptions)
{
    char** papszTileDriverOptions = NULL;

    const char* pszDriverName =
        CSLFetchNameValueDef(papszOptions, "DRIVER", "GTiff");
        
    if (EQUAL(pszDriverName, "EPSILON"))
    {
        papszTileDriverOptions = CSLSetNameValue(papszTileDriverOptions,
                                                "RASTERLITE_OUTPUT", "YES");
    }
    
    const char* pszQuality = CSLFetchNameValue(papszOptions, "QUALITY");
    if (pszQuality)
    {
        if (EQUAL(pszDriverName, "GTiff"))
        {
            papszTileDriverOptions =
                CSLSetNameValue(papszTileDriverOptions, "JPEG_QUALITY", pszQuality);
        }
        else if (EQUAL(pszDriverName, "JPEG") || EQUAL(pszDriverName, "WEBP"))
        {
            papszTileDriverOptions =
                CSLSetNameValue(papszTileDriverOptions, "QUALITY", pszQuality);
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unexpected option '%s' for driver '%s'",
                     "QUALITY", pszDriverName);
        }
    }
    
    papszTileDriverOptions = RasterliteAddTileDriverOptionsForDriver(
                papszOptions, papszTileDriverOptions, "COMPRESS", "GTiff");
    papszTileDriverOptions = RasterliteAddTileDriverOptionsForDriver(
                papszOptions, papszTileDriverOptions, "PHOTOMETRIC", "GTiff");
    papszTileDriverOptions = RasterliteAddTileDriverOptionsForDriver(
                papszOptions, papszTileDriverOptions, "TARGET", "EPSILON");
    papszTileDriverOptions = RasterliteAddTileDriverOptionsForDriver(
                papszOptions, papszTileDriverOptions, "FILTER", "EPSILON");
    
    return papszTileDriverOptions;
}

/************************************************************************/
/*                      RasterliteInsertSRID ()                         */
/************************************************************************/

static int RasterliteInsertSRID(OGRDataSourceH hDS, const char* pszWKT)
{
    CPLString osSQL;
    
    int nAuthorityCode = 0;
    CPLString osAuthorityName, osProjCS, osProj4;
    if (pszWKT != NULL && strlen(pszWKT) != 0)
    {
        OGRSpatialReferenceH hSRS = OSRNewSpatialReference(pszWKT);
        if (hSRS)
        {
            const char* pszAuthorityName = OSRGetAuthorityName(hSRS, NULL);
            if (pszAuthorityName) osAuthorityName = pszAuthorityName;
            
            const char* pszProjCS = OSRGetAttrValue(hSRS, "PROJCS", 0);
            if (pszProjCS) osProjCS = pszProjCS;
            
            const char* pszAuthorityCode = OSRGetAuthorityCode(hSRS, NULL);
            if (pszAuthorityCode) nAuthorityCode = atoi(pszAuthorityCode);
            
            char    *pszProj4 = NULL;
            if( OSRExportToProj4( hSRS, &pszProj4 ) != OGRERR_NONE )
                pszProj4 = CPLStrdup("");
            osProj4 = pszProj4;
            CPLFree(pszProj4);
        }
        OSRDestroySpatialReference(hSRS);
    }
        
    int nSRSId = -1;
    if (nAuthorityCode != 0 && osAuthorityName.size() != 0)
    {
        osSQL.Printf   ("SELECT srid FROM spatial_ref_sys WHERE auth_srid = %d", nAuthorityCode);
        OGRLayerH hLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
        if (hLyr == NULL)
        {
            nSRSId = nAuthorityCode;
            
            if ( osProjCS.size() != 0 )
                osSQL.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, ref_sys_name, proj4text) "
                    "VALUES (%d, '%s', '%d', '%s', '%s')",
                    nSRSId, osAuthorityName.c_str(),
                    nAuthorityCode, osProjCS.c_str(), osProj4.c_str() );
            else
                osSQL.Printf(
                    "INSERT INTO spatial_ref_sys "
                    "(srid, auth_name, auth_srid, proj4text) "
                    "VALUES (%d, '%s', '%d', '%s')",
                    nSRSId, osAuthorityName.c_str(),
                    nAuthorityCode, osProj4.c_str() );

            
            OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
        }
        else
        {
            OGRFeatureH hFeat = OGR_L_GetNextFeature(hLyr);
            if (hFeat)
            {
                nSRSId = OGR_F_GetFieldAsInteger(hFeat, 0);
                OGR_F_Destroy(hFeat);
            }
            OGR_DS_ReleaseResultSet(hDS, hLyr);
        }
    }
    
    return nSRSId;
}

/************************************************************************/
/*                     RasterliteCreateTables ()                        */
/************************************************************************/

OGRDataSourceH RasterliteCreateTables(OGRDataSourceH hDS, const char* pszTableName,
                                      int nSRSId, int bWipeExistingData)
{
    CPLString osSQL;
    
    CPLString osDBName = OGR_DS_GetName(hDS);
    
    CPLString osRasterLayer;
    osRasterLayer.Printf("%s_rasters", pszTableName);
    
    CPLString osMetatadataLayer;
    osMetatadataLayer.Printf("%s_metadata", pszTableName);

    OGRLayerH hLyr;

    if (OGR_DS_GetLayerByName(hDS, osRasterLayer.c_str()) == NULL)
    {
/* -------------------------------------------------------------------- */
/*      The table don't exist. Create them                              */
/* -------------------------------------------------------------------- */

        /* Create _rasters table */
        osSQL.Printf   ("CREATE TABLE \"%s\" ("
                        "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
                        "raster BLOB NOT NULL)", osRasterLayer.c_str());
        OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
            
        /* Create _metadata table */
        osSQL.Printf   ("CREATE TABLE \"%s\" ("
                        "id INTEGER NOT NULL PRIMARY KEY,"
                        "source_name TEXT NOT NULL,"
                        "tile_id INTEGER NOT NULL,"
                        "width INTEGER NOT NULL,"
                        "height INTEGER NOT NULL,"
                        "pixel_x_size DOUBLE NOT NULL,"
                        "pixel_y_size DOUBLE NOT NULL)",
                        osMetatadataLayer.c_str());
        OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);

        /* Add geometry column to _metadata table */
        osSQL.Printf("SELECT AddGeometryColumn('%s', 'geometry', %d, 'POLYGON', 2)",
                      osMetatadataLayer.c_str(), nSRSId);
        if ((hLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL)) == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Check that the OGR SQLite driver has Spatialite support");
            OGRReleaseDataSource(hDS);
            return NULL;
        }
        OGR_DS_ReleaseResultSet(hDS, hLyr);
                
        /* Create spatial index on _metadata table */
        osSQL.Printf("SELECT CreateSpatialIndex('%s', 'geometry')",
                      osMetatadataLayer.c_str());
        if ((hLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL)) == NULL)
        {
            OGRReleaseDataSource(hDS);
            return NULL;
        }
        OGR_DS_ReleaseResultSet(hDS, hLyr);

        /* Create statistics tables */
        osSQL.Printf("SELECT UpdateLayerStatistics()");
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
        CPLPopErrorHandler();
        OGR_DS_ReleaseResultSet(hDS, hLyr);

        /* Re-open the DB to take into account the new tables*/
        OGRReleaseDataSource(hDS);
        
        hDS = RasterliteOpenSQLiteDB(osDBName.c_str(), GA_Update);
    }
    else
    {
        /* Check that the existing SRS is consistent with the one of the new */
        /* data to be inserted */
        osSQL.Printf("SELECT srid FROM geometry_columns WHERE f_table_name = '%s'",
                     osMetatadataLayer.c_str());
        hLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
        if (hLyr)
        {
            int nExistingSRID = -1;
            OGRFeatureH hFeat = OGR_L_GetNextFeature(hLyr);
            if (hFeat)
            {
                nExistingSRID = OGR_F_GetFieldAsInteger(hFeat, 0);
                OGR_F_Destroy(hFeat);
            }
            OGR_DS_ReleaseResultSet(hDS, hLyr);

            if (nExistingSRID != nSRSId)
            {
                if (bWipeExistingData)
                {
                    osSQL.Printf("UPDATE geometry_columns SET srid = %d "
                                 "WHERE f_table_name = \"%s\"",
                                 nSRSId, osMetatadataLayer.c_str());
                    OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
        
                    /* Re-open the DB to take into account the change of SRS */
                    OGRReleaseDataSource(hDS);
                    
                    hDS = RasterliteOpenSQLiteDB(osDBName.c_str(), GA_Update);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "New data has not the same SRS as existing data");
                    OGRReleaseDataSource(hDS);
                    return NULL;
                }
            }            
        }
        
        if (bWipeExistingData)
        {
            osSQL.Printf("DELETE FROM \"%s\"", osRasterLayer.c_str());
            OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
            
            osSQL.Printf("DELETE FROM \"%s\"", osMetatadataLayer.c_str());
            OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
        }
    }

    return hDS;
}

/************************************************************************/
/*                       RasterliteCreateCopy ()                        */
/************************************************************************/

GDALDataset *
RasterliteCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                       int bStrict, char ** papszOptions, 
                       GDALProgressFunc pfnProgress, void * pProgressData )
{
    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "nBands == 0");
        return NULL;
    }
    
    const char* pszDriverName = CSLFetchNameValueDef(papszOptions, "DRIVER", "GTiff");
    if (EQUAL(pszDriverName, "MEM") || EQUAL(pszDriverName, "VRT"))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GDAL %s driver cannot be used as underlying driver",
                 pszDriverName);
        return NULL;
    }

    GDALDriverH hTileDriver = GDALGetDriverByName(pszDriverName);
    if ( hTileDriver == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot load GDAL %s driver", pszDriverName);
        return NULL;
    }
    
    GDALDriverH hMemDriver = GDALGetDriverByName("MEM");
    if (hMemDriver == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot load GDAL MEM driver");
        return NULL;
    }   

    int nXSize = GDALGetRasterXSize(poSrcDS);
    int nYSize = GDALGetRasterYSize(poSrcDS);
    
    double adfGeoTransform[6];
    if (poSrcDS->GetGeoTransform(adfGeoTransform) != CE_None)
    {
        adfGeoTransform[0] = 0;
        adfGeoTransform[1] = 1;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = 0;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = -1;
    }
    else if (adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot use geotransform with rotational terms");
        return NULL;
    }

    int bTiled = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "TILED", "YES"));
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
        nBlockXSize = nXSize;
        nBlockYSize = nYSize;
    }
    
/* -------------------------------------------------------------------- */
/*      Analyze arguments                                               */
/* -------------------------------------------------------------------- */
    
    CPLString osDBName;
    CPLString osTableName;
    VSIStatBuf sBuf;
    int bExists;

    /* Skip optionnal RASTERLITE: prefix */
    const char* pszFilenameWithoutPrefix = pszFilename;
    if (EQUALN(pszFilename, "RASTERLITE:", 11))
        pszFilenameWithoutPrefix += 11;
    
    char** papszTokens = CSLTokenizeStringComplex( 
                pszFilenameWithoutPrefix, ", ", FALSE, FALSE );
    int nTokens = CSLCount(papszTokens);
    if (nTokens == 0)
    {
        osDBName = pszFilenameWithoutPrefix;
        osTableName = CPLGetBasename(pszFilenameWithoutPrefix);
    }
    else
    {
        osDBName = papszTokens[0];
        
        int i;
        for(i=1;i<nTokens;i++)
        {
            if (EQUALN(papszTokens[i], "table=", 6))
                osTableName = papszTokens[i] + 6;
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid option : %s", papszTokens[i]);
            }
        }
    }
    
    CSLDestroy(papszTokens);
    papszTokens = NULL;
    
    bExists = (VSIStat(osDBName.c_str(), &sBuf) == 0);

    if (osTableName.size() == 0)
    {
        if (bExists)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Database already exists. Explicit table name must be specified");
            return NULL;
        }
        osTableName = CPLGetBasename(osDBName.c_str());
    }    
    
    CPLString osRasterLayer;
    osRasterLayer.Printf("%s_rasters", osTableName.c_str());
    
    CPLString osMetatadataLayer;
    osMetatadataLayer.Printf("%s_metadata", osTableName.c_str());

/* -------------------------------------------------------------------- */
/*      Create or open the SQLite DB                                    */
/* -------------------------------------------------------------------- */
    
    if (OGRGetDriverCount() == 0)
        OGRRegisterAll();
        
    OGRSFDriverH hSQLiteDriver = OGRGetDriverByName("SQLite");
    if (hSQLiteDriver == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot load OGR SQLite driver");
        return NULL;
    }   
    
    OGRDataSourceH hDS;
    
    if (!bExists)
    {
        char** papszOGROptions = CSLAddString(NULL, "SPATIALITE=YES");
        hDS = OGR_Dr_CreateDataSource(hSQLiteDriver,
                                      osDBName.c_str(), papszOGROptions);
        CSLDestroy(papszOGROptions);
    }
    else
    {
        hDS = RasterliteOpenSQLiteDB(osDBName.c_str(), GA_Update);
    }
    
    if (hDS == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot load or create SQLite database");
        return NULL;
    }

    CPLString osSQL;
    
/* -------------------------------------------------------------------- */
/*      Get the SRID for the SRS                                        */
/* -------------------------------------------------------------------- */
    int nSRSId = RasterliteInsertSRID(hDS, poSrcDS->GetProjectionRef());

/* -------------------------------------------------------------------- */
/*      Create or wipe existing tables                                  */
/* -------------------------------------------------------------------- */
    int bWipeExistingData =
        CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "WIPE", "NO"));
        
    hDS = RasterliteCreateTables(hDS, osTableName.c_str(),
                                 nSRSId, bWipeExistingData);
    if (hDS == NULL)
        return NULL;

    OGRLayerH hRasterLayer = OGR_DS_GetLayerByName(hDS, osRasterLayer.c_str());
    OGRLayerH hMetadataLayer = OGR_DS_GetLayerByName(hDS, osMetatadataLayer.c_str());
    if (hRasterLayer == NULL || hMetadataLayer == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find metadata and/or raster tables");
        OGRReleaseDataSource(hDS);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check if there is overlapping data and warn the user            */
/* -------------------------------------------------------------------- */
    double minx = adfGeoTransform[0];
    double maxx = adfGeoTransform[0] + nXSize * adfGeoTransform[1];
    double maxy = adfGeoTransform[3];
    double miny = adfGeoTransform[3] + nYSize * adfGeoTransform[5];
    
    osSQL.Printf("SELECT COUNT(geometry) FROM \"%s\" "
                 "WHERE rowid IN "
                 "(SELECT pkid FROM \"idx_%s_metadata_geometry\" "
                  "WHERE %s) AND %s",
                  osMetatadataLayer.c_str(),
                  osTableName.c_str(),
                  RasterliteGetSpatialFilterCond(minx, miny, maxx, maxy).c_str(),
                  RasterliteGetPixelSizeCond(adfGeoTransform[1], -adfGeoTransform[5]).c_str());
    
    int nOverlappingGeoms = 0;
    OGRLayerH hCountLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    if (hCountLyr)
    {
        OGRFeatureH hFeat = OGR_L_GetNextFeature(hCountLyr);
        if (hFeat)
        {
            nOverlappingGeoms = OGR_F_GetFieldAsInteger(hFeat, 0);
            OGR_F_Destroy(hFeat);
        }
        OGR_DS_ReleaseResultSet(hDS, hCountLyr);
    }
    
    if (nOverlappingGeoms != 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Raster tiles already exist in the %s table within "
                 "the extent of the data to be inserted in",
                 osTableName.c_str());
    }
   
/* -------------------------------------------------------------------- */
/*      Iterate over blocks to add data into raster and metadata tables */
/* -------------------------------------------------------------------- */
    int nXBlocks = (nXSize + nBlockXSize - 1) / nBlockXSize;
    int nYBlocks = (nYSize + nBlockYSize - 1) / nBlockYSize;

    GDALDataType eDataType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    int nDataTypeSize = GDALGetDataTypeSize(eDataType) / 8;
    GByte* pabyMEMDSBuffer =
        (GByte*)VSIMalloc3(nBlockXSize, nBlockYSize, nBands * nDataTypeSize);
    if (pabyMEMDSBuffer == NULL)
    {
        OGRReleaseDataSource(hDS);
        return NULL;
    }
    
    CPLString osTempFileName;
    osTempFileName.Printf("/vsimem/%p", hDS);
    
    int nTileId = 0;
    int nBlocks = 0;
    int nTotalBlocks = nXBlocks * nYBlocks;

    char** papszTileDriverOptions = RasterliteGetTileDriverOptions(papszOptions);
    
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
            if ((nBlockXOff+1) * nBlockXSize > nXSize)
                nReqXSize = nXSize - nBlockXOff * nBlockXSize;
            if ((nBlockYOff+1) * nBlockYSize > nYSize)
                nReqYSize = nYSize - nBlockYOff * nBlockYSize;

            eErr = poSrcDS->RasterIO(GF_Read,
                                     nBlockXOff * nBlockXSize,
                                     nBlockYOff * nBlockYSize,
                                     nReqXSize, nReqYSize,
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
                char** papszMEMDSOptions = NULL;
                char szTmp[64];
                memset(szTmp, 0, sizeof(szTmp));
                CPLPrintPointer(szTmp,
                                pabyMEMDSBuffer + iBand * nDataTypeSize *
                                nReqXSize * nReqYSize, sizeof(szTmp));
                papszMEMDSOptions = CSLSetNameValue(papszMEMDSOptions, "DATAPOINTER", szTmp);
                GDALAddBand(hMemDS, eDataType, papszMEMDSOptions);
                CSLDestroy(papszMEMDSOptions);
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

            vsi_l_offset nDataLength = 0;
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
            OGR_F_SetFieldString(hFeat, 0, GDALGetDescription(poSrcDS));
            OGR_F_SetFieldInteger(hFeat, 1, nTileId ++);
            OGR_F_SetFieldInteger(hFeat, 2, nReqXSize);
            OGR_F_SetFieldInteger(hFeat, 3, nReqYSize);
            OGR_F_SetFieldDouble(hFeat, 4, adfGeoTransform[1]);
            OGR_F_SetFieldDouble(hFeat, 5, -adfGeoTransform[5]);
            
            minx = adfGeoTransform[0] +
                (nBlockXSize * nBlockXOff) * adfGeoTransform[1];
            maxx = adfGeoTransform[0] +
                (nBlockXSize * nBlockXOff + nReqXSize) * adfGeoTransform[1];
            maxy = adfGeoTransform[3] +
                (nBlockYSize * nBlockYOff) * adfGeoTransform[5];
            miny = adfGeoTransform[3] +
                (nBlockYSize * nBlockYOff + nReqYSize) * adfGeoTransform[5];
            
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
    
    if (eErr == CE_None)
        OGR_DS_ExecuteSQL(hDS, "COMMIT", NULL, NULL);
    else
        OGR_DS_ExecuteSQL(hDS, "ROLLBACK", NULL, NULL);
    
    CSLDestroy(papszTileDriverOptions);
    
    VSIFree(pabyMEMDSBuffer);
    
    OGRReleaseDataSource(hDS);
        
    return (GDALDataset*) GDALOpen(pszFilename, GA_Update);
}

/************************************************************************/
/*                         RasterliteDelete ()                          */
/************************************************************************/

CPLErr RasterliteDelete(const char* pszFilename)
{
    return CE_None;
}
