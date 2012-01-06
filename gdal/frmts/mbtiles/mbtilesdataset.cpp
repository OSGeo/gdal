/******************************************************************************
 * $Id$
 *
 * Project:  GDAL MBTiles driver
 * Purpose:  Implement GDAL MBTiles support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2012, Even Rouault, <even dot rouault at mines dash paris dot org>
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

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_api.h"
#include <math.h>

extern "C" void GDALRegister_MBTiles();

CPL_CVSID("$Id$");

static const char * const apszAllowedDrivers[] = {"JPEG", "PNG", NULL};

class MBTilesBand;

/************************************************************************/
/* ==================================================================== */
/*                              MBTilesDataset                          */
/* ==================================================================== */
/************************************************************************/

class MBTilesDataset : public GDALPamDataset
{
    friend class MBTilesBand;

  public:
                 MBTilesDataset();
                 MBTilesDataset(MBTilesDataset* poMainDS, int nLevel);

    virtual     ~MBTilesDataset();

    virtual CPLErr GetGeoTransform(double* padfGeoTransform);
    virtual const char* GetProjectionRef();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );

  protected:
    virtual int         CloseDependentDatasets();

  private:

    int bMustFree;
    MBTilesDataset* poMainDS;
    int nLevel;
    int nMinTileCol, nMinTileRow;
    int nMinLevel;

    char** papszMetadata;
    char** papszImageStructure;

    int nResolutions;
    MBTilesDataset** papoOverviews;

    OGRDataSourceH hDS;
};

/************************************************************************/
/* ==================================================================== */
/*                              MBTilesBand                             */
/* ==================================================================== */
/************************************************************************/

class MBTilesBand: public GDALPamRasterBand
{
    friend class MBTilesDataset;

  public:
                            MBTilesBand( MBTilesDataset* poDS, int nBand,
                                            GDALDataType eDataType,
                                            int nBlockXSize, int nBlockYSize);

    virtual GDALColorInterp GetColorInterpretation();

    virtual int             GetOverviewCount();
    virtual GDALRasterBand* GetOverview(int nLevel);

    virtual CPLErr          IReadBlock( int, int, void * );
};

/************************************************************************/
/*                            MBTilesBand()                          */
/************************************************************************/

MBTilesBand::MBTilesBand(MBTilesDataset* poDS, int nBand,
                                GDALDataType eDataType,
                                int nBlockXSize, int nBlockYSize)
{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = eDataType;
    this->nBlockXSize = nBlockXSize;
    this->nBlockYSize = nBlockYSize;
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr MBTilesBand::IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage)
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;

    int bGotTile = FALSE;
    int nDataTypeSize = GDALGetDataTypeSize(eDataType) / 8;

    int nMinLevel = (poGDS->poMainDS) ? poGDS->poMainDS->nMinLevel : poGDS->nMinLevel;
    int nMinTileCol = (poGDS->poMainDS) ? poGDS->poMainDS->nMinTileCol : poGDS->nMinTileCol;
    int nMinTileRow = (poGDS->poMainDS) ? poGDS->poMainDS->nMinTileRow : poGDS->nMinTileRow;
    nMinTileCol >>= poGDS->nLevel;

    int nYOff = (((nRasterYSize / nBlockYSize - 1 - nBlockYOff) << poGDS->nLevel) + nMinTileRow) >> poGDS->nLevel;

    int nZoomLevel = ((poGDS->poMainDS) ? poGDS->poMainDS->nResolutions : poGDS->nResolutions) - poGDS->nLevel;

    const char* pszSQL = CPLSPrintf("SELECT tile_data FROM tiles WHERE "
                                    "tile_column = %d AND tile_row = %d AND zoom_level=%d",
                                    nBlockXOff + nMinTileCol, nYOff, nMinLevel + nZoomLevel);
    CPLDebug("MBTILES", "nBand=%d, nBlockXOff=%d, nBlockYOff=%d, %s",
             nBand, nBlockXOff, nBlockYOff, pszSQL);
    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(poGDS->hDS, pszSQL, NULL, NULL);

    OGRFeatureH hFeat = hSQLLyr ? OGR_L_GetNextFeature(hSQLLyr) : NULL;
    if (hFeat != NULL)
    {
        CPLString osMemFileName;
        osMemFileName.Printf("/vsimem/%p", this);

        int nDataSize = 0;
        GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);

        VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyData,
                                            nDataSize, FALSE);
        VSIFCloseL(fp);

        GDALDatasetH hDSTile = GDALOpenInternal(osMemFileName.c_str(), GA_ReadOnly, apszAllowedDrivers);
        if (hDSTile != NULL)
        {
            int nTileBands = GDALGetRasterCount(hDSTile);
            if (nTileBands == 4 && poGDS->nBands == 3)
                nTileBands = 3;

            if (GDALGetRasterXSize(hDSTile) == nBlockXSize &&
                GDALGetRasterYSize(hDSTile) == nBlockYSize &&
                (nTileBands == poGDS->nBands ||
                 (nTileBands == 1 && poGDS->nBands == 3)))
            {
                int iBand;
                void* pSrcImage = NULL;
                GByte abyTranslation[256][3];

                bGotTile = TRUE;

                GDALColorTableH hCT = GDALGetRasterColorTable(GDALGetRasterBand(hDSTile, 1));
                if (nTileBands == 1 && poGDS->nBands == 3)
                {
                    if (hCT != NULL)
                        pSrcImage = CPLMalloc(nBlockXSize * nBlockYSize * nDataTypeSize);
                    iBand = 1;
                }
                else
                    iBand = nBand;

                GDALRasterIO(GDALGetRasterBand(hDSTile, iBand), GF_Read,
                             0, 0, nBlockXSize, nBlockYSize,
                             pImage, nBlockXSize, nBlockYSize, eDataType, 0, 0);

                if (pSrcImage != NULL && hCT != NULL)
                {
                    int i;
                    memcpy(pSrcImage, pImage,
                           nBlockXSize * nBlockYSize * nDataTypeSize);

                    int nEntryCount = GDALGetColorEntryCount( hCT );
                    if (nEntryCount > 256)
                        nEntryCount = 256;
                    for(i = 0; i < nEntryCount; i++)
                    {
                        const GDALColorEntry* psEntry = GDALGetColorEntry( hCT, i );
                        abyTranslation[i][0] = (GByte) psEntry->c1;
                        abyTranslation[i][1] = (GByte) psEntry->c2;
                        abyTranslation[i][2] = (GByte) psEntry->c3;
                    }
                    for(; i < 256; i++)
                    {
                        abyTranslation[i][0] = 0;
                        abyTranslation[i][1] = 0;
                        abyTranslation[i][2] = 0;
                    }

                    for(i = 0; i < nBlockXSize * nBlockYSize; i++)
                    {
                        ((GByte*)pImage)[i] = abyTranslation[((GByte*)pSrcImage)[i]][nBand-1];
                    }
                }

                for(int iOtherBand=1;iOtherBand<=poGDS->nBands;iOtherBand++)
                {
                    GDALRasterBlock *poBlock;

                    if (iOtherBand == nBand)
                        continue;

                    poBlock = ((MBTilesBand*)poGDS->GetRasterBand(iOtherBand))->
                        TryGetLockedBlockRef(nBlockXOff,nBlockYOff);

                    if (poBlock != NULL)
                    {
                        poBlock->DropLock();
                        continue;
                    }

                    poBlock = poGDS->GetRasterBand(iOtherBand)->
                        GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
                    if (poBlock == NULL)
                        break;

                    GByte* pabySrcBlock = (GByte *) poBlock->GetDataRef();
                    if( pabySrcBlock == NULL )
                    {
                        poBlock->DropLock();
                        break;
                    }

                    if (nTileBands == 1 && poGDS->nBands == 3)
                    {
                        int i;
                        if (pSrcImage)
                        {
                            for(i = 0; i < nBlockXSize * nBlockYSize; i++)
                            {
                                ((GByte*)pabySrcBlock)[i] =
                                    abyTranslation[((GByte*)pSrcImage)[i]][iOtherBand-1];
                            }
                        }
                        else
                            memcpy(pabySrcBlock, pImage,
                                    nBlockXSize * nBlockYSize * nDataTypeSize);
                    }
                    else
                    {
                        GDALRasterIO(GDALGetRasterBand(hDSTile, iOtherBand), GF_Read,
                            0, 0, nBlockXSize, nBlockYSize,
                            pabySrcBlock, nBlockXSize, nBlockYSize, eDataType, 0, 0);
                    }

                    poBlock->DropLock();
                }

                CPLFree(pSrcImage);
            }
            else
            {
                CPLDebug("MBTILES", "tile size = %d, tile height = %d, tile bands = %d",
                         GDALGetRasterXSize(hDSTile), GDALGetRasterYSize(hDSTile),
                         GDALGetRasterCount(hDSTile));
            }
            GDALClose(hDSTile);
        }

        VSIUnlink( osMemFileName.c_str() );

        OGR_F_Destroy(hFeat);
    }

    OGR_DS_ReleaseResultSet(poGDS->hDS, hSQLLyr);

    if (!bGotTile)
    {
        memset(pImage, (nBand == 4) ? 0 : 255,
               nBlockXSize * nBlockYSize * nDataTypeSize);

        for(int iOtherBand=1;iOtherBand<=poGDS->nBands;iOtherBand++)
        {
            GDALRasterBlock *poBlock;

            if (iOtherBand == nBand)
                continue;

            poBlock = poGDS->GetRasterBand(iOtherBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
            if (poBlock == NULL)
                break;

            GByte* pabySrcBlock = (GByte *) poBlock->GetDataRef();
            if( pabySrcBlock == NULL )
            {
                poBlock->DropLock();
                break;
            }

            memset(pabySrcBlock, (iOtherBand == 4) ? 0 : 255,
                   nBlockXSize * nBlockYSize * nDataTypeSize);

            poBlock->DropLock();
        }
    }

    return CE_None;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int MBTilesBand::GetOverviewCount()
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;

    if (poGDS->nResolutions >= 1)
        return poGDS->nResolutions;
    else
        return GDALPamRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand* MBTilesBand::GetOverview(int nLevel)
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;

    if (poGDS->nResolutions == 0)
        return GDALPamRasterBand::GetOverview(nLevel);

    if (nLevel < 0 || nLevel >= poGDS->nResolutions)
        return NULL;

    GDALDataset* poOvrDS = poGDS->papoOverviews[nLevel];
    if (poOvrDS)
        return poOvrDS->GetRasterBand(nBand);
    else
        return NULL;
}

/************************************************************************/
/*                   GetColorInterpretation()                           */
/************************************************************************/

GDALColorInterp MBTilesBand::GetColorInterpretation()
{
    MBTilesDataset* poGDS = (MBTilesDataset*) poDS;
    if (poGDS->nBands == 1)
    {
        return GCI_GrayIndex;
    }
    else if (poGDS->nBands == 3 || poGDS->nBands == 4)
    {
        if (nBand == 1)
            return GCI_RedBand;
        else if (nBand == 2)
            return GCI_GreenBand;
        else if (nBand == 3)
            return GCI_BlueBand;
        else if (nBand == 4)
            return GCI_AlphaBand;
    }

    return GCI_Undefined;
}

/************************************************************************/
/*                         MBTilesDataset()                          */
/************************************************************************/

MBTilesDataset::MBTilesDataset()
{
    bMustFree = FALSE;
    nLevel = 0;
    poMainDS = NULL;
    nResolutions = 0;
    hDS = NULL;
    papoOverviews = NULL;
    papszMetadata = NULL;
    papszImageStructure =
        CSLAddString(NULL, "INTERLEAVE=PIXEL");
    nMinTileCol = nMinTileRow = 0;
    nMinLevel = 0;
}

/************************************************************************/
/*                          MBTilesDataset()                            */
/************************************************************************/

MBTilesDataset::MBTilesDataset(MBTilesDataset* poMainDS, int nLevel)
{
    bMustFree = FALSE;
    this->nLevel = nLevel;
    this->poMainDS = poMainDS;
    nResolutions = poMainDS->nResolutions - nLevel;
    hDS = poMainDS->hDS;
    papoOverviews = poMainDS->papoOverviews + nLevel;
    papszMetadata = poMainDS->papszMetadata;
    papszImageStructure =  poMainDS->papszImageStructure;

    nRasterXSize = poMainDS->nRasterXSize / (1 << nLevel);
    nRasterYSize = poMainDS->nRasterYSize / (1 << nLevel);
    nMinTileCol = nMinTileRow = 0;
    nMinLevel = 0;
}

/************************************************************************/
/*                        ~MBTilesDataset()                             */
/************************************************************************/

MBTilesDataset::~MBTilesDataset()
{
    CloseDependentDatasets();
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int MBTilesDataset::CloseDependentDatasets()
{
    int bRet = GDALPamDataset::CloseDependentDatasets();

    if (poMainDS == NULL && !bMustFree)
    {
        CSLDestroy(papszMetadata);
        papszMetadata = NULL;
        CSLDestroy(papszImageStructure);
        papszImageStructure = NULL;

        if (papoOverviews)
        {
            int i;
            for(i=0;i<nResolutions;i++)
            {
                if (papoOverviews[i] != NULL &&
                    papoOverviews[i]->bMustFree)
                {
                    papoOverviews[i]->poMainDS = NULL;
                }
                delete papoOverviews[i];
            }
            CPLFree(papoOverviews);
            papoOverviews = NULL;
            nResolutions = 0;
            bRet = TRUE;
        }

        if (hDS != NULL)
            OGRReleaseDataSource(hDS);
        hDS = NULL;
    }
    else if (poMainDS != NULL && bMustFree)
    {
        poMainDS->papoOverviews[nLevel-1] = NULL;
        delete poMainDS;
        poMainDS = NULL;
        bRet = TRUE;
    }

    return bRet;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

//#define MAX_GM 20037508.3427892
#define MAX_GM 20037500.

CPLErr MBTilesDataset::GetGeoTransform(double* padfGeoTransform)
{
    int nMaxLevel = nMinLevel + nResolutions;
    if (nMaxLevel == 0)
    {
        padfGeoTransform[0] = -MAX_GM;
        padfGeoTransform[1] = 2 * MAX_GM / nRasterXSize;
        padfGeoTransform[2] = 0;
        padfGeoTransform[3] = MAX_GM;
        padfGeoTransform[4] = 0;
        padfGeoTransform[5] = -2 * MAX_GM / nRasterYSize;
    }
    else
    {
        int nMaxTileCol = nMinTileCol + nRasterXSize / 256;
        int nMaxTileRow = nMinTileRow + nRasterYSize / 256;
        int nMiddleTile = (1 << nMaxLevel) / 2;
        padfGeoTransform[0] = 2 * MAX_GM * (nMinTileCol - nMiddleTile) / (1 << nMaxLevel);
        padfGeoTransform[1] = 2 * MAX_GM * (nMaxTileCol - nMinTileCol) / (1 << nMaxLevel) / nRasterXSize;
        padfGeoTransform[2] = 0;
        padfGeoTransform[3] = 2 * MAX_GM * (nMaxTileRow - nMiddleTile) / (1 << nMaxLevel);
        padfGeoTransform[4] = 0;
        padfGeoTransform[5] = -2 * MAX_GM * (nMaxTileRow - nMinTileRow) / (1 << nMaxLevel) / nRasterYSize;
    }
    return CE_None;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* MBTilesDataset::GetProjectionRef()
{
    return "PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"],AUTHORITY[\"EPSG\",\"3857\"]]";
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int MBTilesDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MBTILES") &&
        poOpenInfo->nHeaderBytes >= 1024 &&
        EQUALN((const char*)poOpenInfo->pabyHeader, "SQLite Format 3", 15))
    {
        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* MBTilesDataset::Open(GDALOpenInfo* poOpenInfo)
{
    CPLString osFileName;
    CPLString osTableName;

    if (!Identify(poOpenInfo))
        return NULL;

    if (OGRGetDriverCount() == 0)
        OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Open underlying OGR DB                                          */
/* -------------------------------------------------------------------- */

    OGRDataSourceH hDS = OGROpen(poOpenInfo->pszFilename, FALSE, NULL);

    MBTilesDataset* poDS = NULL;

    if (hDS == NULL)
        goto end;

/* -------------------------------------------------------------------- */
/*      Build dataset                                                   */
/* -------------------------------------------------------------------- */
    {
        CPLString osMetadataTableName, osRasterTableName;
        CPLString osSQL;
        OGRLayerH hMetadataLyr, hRasterLyr;
        OGRFeatureH hFeat;
        int nResolutions;
        int iBand, nBands, nBlockXSize, nBlockYSize;
        GDALDataType eDataType;
        OGRLayerH hSQLLyr = NULL;
        int nMinLevel, nMaxLevel, nMinTileRow = 0, nMaxTileRow = 0, nMinTileCol = 0, nMaxTileCol = 0;
        GDALDatasetH hDSTile;
        int nDataSize = 0;
        GByte* pabyData = NULL;
        int bHasBounds = FALSE;

        osMetadataTableName = "metadata";

        hMetadataLyr = OGR_DS_GetLayerByName(hDS, osMetadataTableName.c_str());
        if (hMetadataLyr == NULL)
            goto end;

        osRasterTableName += "tiles";

        hRasterLyr = OGR_DS_GetLayerByName(hDS, osRasterTableName.c_str());
        if (hRasterLyr == NULL)
            goto end;

        /*
        int bHasMap = OGR_DS_GetLayerByName(hDS, "map") != NULL &&
                      OGR_DS_GetLayerByName(hDS, "images") != NULL;*/

/* -------------------------------------------------------------------- */
/*      Get minimum and maximum zoom levels                             */
/* -------------------------------------------------------------------- */

#define OPTIMIZED_FOR_VSICURL
#ifdef  OPTIMIZED_FOR_VSICURL

        nMinLevel = -1;
        int iLevel;
        for(iLevel = 0; nMinLevel < 0 && iLevel < 16; iLevel ++)
        {
            osSQL = CPLSPrintf("SELECT zoom_level FROM tiles WHERE zoom_level = %d LIMIT 1",
                               iLevel);
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
            if (hSQLLyr)
            {
                hFeat = OGR_L_GetNextFeature(hSQLLyr);
                if (hFeat)
                {
                    nMinLevel = iLevel;
                    OGR_F_Destroy(hFeat);
                }
                OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            }
        }

        if (nMinLevel < 0)
            goto end;

        nMaxLevel = -1;
        for(iLevel = 32; nMaxLevel < 0 && iLevel >= nMinLevel; iLevel --)
        {
            osSQL = CPLSPrintf("SELECT zoom_level FROM tiles WHERE zoom_level = %d LIMIT 1",
                               iLevel);
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
            if (hSQLLyr)
            {
                hFeat = OGR_L_GetNextFeature(hSQLLyr);
                if (hFeat)
                {
                    nMaxLevel = iLevel;
                    OGR_F_Destroy(hFeat);
                }
                OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            }
        }
#else
        hSQLLyr = OGR_DS_ExecuteSQL(hDS,
            "SELECT min(zoom_level), max(zoom_level) FROM tiles", NULL, NULL);
        if (hSQLLyr == NULL)
        {
            goto end;
        }

        hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat == NULL)
        {
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            goto end;
        }

        nMinLevel = OGR_F_GetFieldAsInteger(hFeat, 0);
        nMaxLevel = OGR_F_GetFieldAsInteger(hFeat, 1);

        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
#endif

/* -------------------------------------------------------------------- */
/*      Get bounds                                                      */
/* -------------------------------------------------------------------- */

        hSQLLyr = OGR_DS_ExecuteSQL(hDS,
            "SELECT value FROM metadata WHERE name = 'bounds'", NULL, NULL);
        if (hSQLLyr)
        {
            hFeat = OGR_L_GetNextFeature(hSQLLyr);
            if (hFeat != NULL)
            {
                const char* pszBounds = OGR_F_GetFieldAsString(hFeat, 0);
                char** papszTokens = CSLTokenizeString2(pszBounds, ",", 0);
                if (CSLCount(papszTokens) != 4 ||
                    fabs(atof(papszTokens[0])) > 180 ||
                    fabs(atof(papszTokens[1])) > 86 ||
                    fabs(atof(papszTokens[2])) > 180 ||
                    fabs(atof(papszTokens[3])) > 86 ||
                    atof(papszTokens[0]) > atof(papszTokens[2]) ||
                    atof(papszTokens[1]) > atof(papszTokens[3]))
                {
                    CSLDestroy(papszTokens);
                    OGR_F_Destroy(hFeat);
                    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
                    goto end;
                }

                #define FORTPI      0.78539816339744833
                /* Latitude to Google-mercator northing */
                #define LAT_TO_NORTHING(lat) \
                    6378137 * log(tan(FORTPI + .5 * (lat) / 180 * (4 * FORTPI)))

                nMinTileCol = (int)(((atof(papszTokens[0]) + 180) / 360) * (1 << nMaxLevel));
                nMaxTileCol = (int)(((atof(papszTokens[2]) + 180) / 360) * (1 << nMaxLevel));
                nMinTileRow = (int)(0.5 + ((LAT_TO_NORTHING(atof(papszTokens[1])) + MAX_GM) / (2* MAX_GM)) * (1 << nMaxLevel));
                nMaxTileRow = (int)(0.5 + ((LAT_TO_NORTHING(atof(papszTokens[3])) + MAX_GM) / (2* MAX_GM)) * (1 << nMaxLevel));

                bHasBounds = TRUE;

                CSLDestroy(papszTokens);

                OGR_F_Destroy(hFeat);
            }
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        }

        if (!bHasBounds)
        {
            osSQL = CPLSPrintf("SELECT min(tile_column), max(tile_column), "
                               "min(tile_row), max(tile_row) FROM tiles "
                               "WHERE zoom_level = %d",
                               nMaxLevel);
            hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
            if (hSQLLyr == NULL)
            {
                goto end;
            }

            hFeat = OGR_L_GetNextFeature(hSQLLyr);
            if (hFeat == NULL)
            {
                OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
                goto end;
            }

            nMinTileCol = OGR_F_GetFieldAsInteger(hFeat, 0);
            nMaxTileCol = OGR_F_GetFieldAsInteger(hFeat, 1);
            nMinTileRow = OGR_F_GetFieldAsInteger(hFeat, 2);
            nMaxTileRow = OGR_F_GetFieldAsInteger(hFeat, 3);

            OGR_F_Destroy(hFeat);
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        }

/* -------------------------------------------------------------------- */
/*      Get number of bands                                             */
/* -------------------------------------------------------------------- */
        osSQL = CPLSPrintf("SELECT tile_data FROM tiles WHERE "
                           "tile_column = %d AND tile_row = %d AND zoom_level = %d",
                           (nMinTileCol  + nMaxTileCol) / 2,
                           (nMinTileRow  + nMaxTileRow) / 2,
                           nMaxLevel);
        hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
        if (hSQLLyr == NULL)
        {
            goto end;
        }

        hFeat = OGR_L_GetNextFeature(hSQLLyr);
        if (hFeat == NULL)
        {
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            goto end;
        }

        CPLString osMemFileName;
        osMemFileName.Printf("/vsimem/%p", hSQLLyr);

        nDataSize = 0;
        pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);

        VSIFCloseL(VSIFileFromMemBuffer( osMemFileName.c_str(), pabyData,
                                         nDataSize, FALSE));

        hDSTile = GDALOpenInternal(osMemFileName.c_str(), GA_ReadOnly, apszAllowedDrivers);
        if (hDSTile == NULL)
        {
            VSIUnlink(osMemFileName.c_str());
            OGR_F_Destroy(hFeat);
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            goto end;
        }

        if (GDALGetRasterXSize(hDSTile) != 256 ||
            GDALGetRasterYSize(hDSTile) != 256)
        {
            GDALClose(hDSTile);
            VSIUnlink(osMemFileName.c_str());
            OGR_F_Destroy(hFeat);
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            goto end;
        }

        nBands = GDALGetRasterCount(hDSTile);
        if (nBands == 1 &&
            GDALGetRasterColorTable(GDALGetRasterBand(hDSTile, 1)) != NULL &&
            GDALGetRasterDataType(GDALGetRasterBand(hDSTile, 1)) == GDT_Byte)
        {
            nBands = 3;
        }

        GDALClose(hDSTile);
        VSIUnlink(osMemFileName.c_str());
        OGR_F_Destroy(hFeat);
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);

/* -------------------------------------------------------------------- */
/*      Set dataset attributes                                          */
/* -------------------------------------------------------------------- */

        poDS = new MBTilesDataset();
        poDS->eAccess = poOpenInfo->eAccess;
        poDS->hDS = hDS;

        /* poDS will release it from now */
        hDS = NULL;

/* -------------------------------------------------------------------- */
/*      Store resolutions                                               */
/* -------------------------------------------------------------------- */
        poDS->nMinLevel = nMinLevel;
        poDS->nResolutions = nResolutions = nMaxLevel - nMinLevel;

/* -------------------------------------------------------------------- */
/*      Round bounds to the lowest zoom level                           */
/* -------------------------------------------------------------------- */

        //CPLDebug("MBTiles", "%d %d %d %d", nMinTileCol, nMinTileRow, nMaxTileCol, nMaxTileRow);
        nMinTileCol = (int)(1.0 * nMinTileCol / (1 << nResolutions)) * (1 << nResolutions);
        nMinTileRow = (int)(1.0 * nMinTileRow / (1 << nResolutions)) * (1 << nResolutions);
        nMaxTileCol = (int)ceil(1.0 * nMaxTileCol / (1 << nResolutions)) * (1 << nResolutions);
        nMaxTileRow = (int)ceil(1.0 * nMaxTileRow / (1 << nResolutions)) * (1 << nResolutions);

/* -------------------------------------------------------------------- */
/*      Compute raster size, geotransform and projection                */
/* -------------------------------------------------------------------- */
        poDS->nMinTileCol = nMinTileCol;
        poDS->nMinTileRow = nMinTileRow;
        poDS->nRasterXSize = (nMaxTileCol-nMinTileCol) * 256;
        poDS->nRasterYSize = (nMaxTileRow-nMinTileRow) * 256;

        nBlockXSize = nBlockYSize = 256;
        eDataType = GDT_Byte;

/* -------------------------------------------------------------------- */
/*      Add bands                                                       */
/* -------------------------------------------------------------------- */

        for(iBand=0;iBand<nBands;iBand++)
            poDS->SetBand(iBand+1, new MBTilesBand(poDS, iBand+1, eDataType,
                                                  nBlockXSize, nBlockYSize));

/* -------------------------------------------------------------------- */
/*      Add overview levels as internal datasets                        */
/* -------------------------------------------------------------------- */
        if (nResolutions >= 1)
        {
            poDS->papoOverviews = (MBTilesDataset**)
                CPLCalloc(nResolutions, sizeof(MBTilesDataset*));
            int nLev;
            for(nLev=1;nLev<=nResolutions;nLev++)
            {
                poDS->papoOverviews[nLev-1] = new MBTilesDataset(poDS, nLev);

                for(iBand=0;iBand<nBands;iBand++)
                {
                    poDS->papoOverviews[nLev-1]->SetBand(iBand+1,
                        new MBTilesBand(poDS->papoOverviews[nLev-1], iBand+1, eDataType,
                                           nBlockXSize, nBlockYSize));
                }
            }
        }

        poDS->SetMetadata(poDS->papszImageStructure, "IMAGE_STRUCTURE");

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
        poDS->SetDescription( poOpenInfo->pszFilename );

        if ( !EQUALN(poOpenInfo->pszFilename, "/vsicurl/", 9) )
            poDS->TryLoadXML();
        else
            poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);
    }

end:
    if (hDS)
        OGRReleaseDataSource(hDS);

    return poDS;
}

/************************************************************************/
/*                       GDALRegister_MBTiles()                         */
/************************************************************************/

void GDALRegister_MBTiles()

{
    GDALDriver  *poDriver;

    if (! GDAL_CHECK_VERSION("MBTiles driver"))
        return;

    if( GDALGetDriverByName( "MBTiles" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "MBTiles" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "MBTiles" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_mbtiles.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mbtiles" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = MBTilesDataset::Open;
        poDriver->pfnIdentify = MBTilesDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
