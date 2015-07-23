/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Rasterlite driver
 * Purpose:  Implement GDAL Rasterlite support using OGR SQLite driver
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"

#include "rasterlitedataset.h"

CPL_CVSID("$Id$");


/************************************************************************/
/*                        RasterliteOpenSQLiteDB()                      */
/************************************************************************/

OGRDataSourceH RasterliteOpenSQLiteDB(const char* pszFilename,
                                      GDALAccess eAccess)
{
    const char* const apszAllowedDrivers[] = { "SQLITE", NULL };
    return (OGRDataSourceH)GDALOpenEx(pszFilename,
                                      GDAL_OF_VECTOR |
                                      ((eAccess == GA_Update) ? GDAL_OF_UPDATE : 0),
                                      apszAllowedDrivers, NULL, NULL);
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

RasterliteBand::RasterliteBand(RasterliteDataset* poDS, int nBand,
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

//#define RASTERLITE_DEBUG

CPLErr RasterliteBand::IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage)
{
    RasterliteDataset* poGDS = (RasterliteDataset*) poDS;
    
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
        printf("nBlockXOff = %d, nBlockYOff = %d, nBlockXSize = %d, nBlockYSize = %d\n"
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
    
    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(poGDS->hDS, osSQL.c_str(), NULL, NULL);
    if (hSQLLyr == NULL)
    {
        memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize);
        return CE_None;
    }

    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/%p", this);
    
    int bHasFoundTile = FALSE;
    int bHasMemsetTile = FALSE;

#ifdef RASTERLITE_DEBUG
    if (nBand == 1)
    {
        printf("nTiles = %d\n", OGR_L_GetFeatureCount(hSQLLyr, TRUE));
    }
#endif

    OGRFeatureH hFeat;
    while( (hFeat = OGR_L_GetNextFeature(hSQLLyr)) != NULL )
    {
        OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);
        if (hGeom == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "null geometry found");
            OGR_F_Destroy(hFeat);
            OGR_DS_ReleaseResultSet(poGDS->hDS, hSQLLyr);
            return CE_Failure;
        }
        
        OGREnvelope oEnvelope;
        OGR_G_GetEnvelope(hGeom, &oEnvelope);

        int nTileId = OGR_F_GetFieldAsInteger(hFeat, 1);
        int nTileXSize = OGR_F_GetFieldAsInteger(hFeat, 2);
        int nTileYSize = OGR_F_GetFieldAsInteger(hFeat, 3);
        
        int nDstXOff =
            (int)((oEnvelope.MinX - minx) / poGDS->adfGeoTransform[1] + 0.5);
        int nDstYOff =
            (int)((maxy - oEnvelope.MaxY) / (-poGDS->adfGeoTransform[5]) + 0.5);
        
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
            printf("id = %d, minx=%.15f miny=%.15f maxx=%.15f maxy=%.15f\n"
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
                printf("id = %d, selected !\n",  nTileId);
            }
#endif
            int nDataSize = 0;
            GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);

            VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyData,
                                              nDataSize, FALSE);
            VSIFCloseL(fp);
            
            GDALDatasetH hDSTile = GDALOpenEx(osMemFileName.c_str(),
                                              GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                              NULL, NULL, NULL);
            int nTileBands = 0;
            if (hDSTile && (nTileBands = GDALGetRasterCount(hDSTile)) == 0)
            {
                GDALClose(hDSTile);
                hDSTile = NULL;
            }
            if (hDSTile == NULL)
            {
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
                GDALClose(hDSTile);
                hDSTile = NULL;
            }
                
            if (hDSTile)
            {
                CPLAssert(GDALGetRasterXSize(hDSTile) == nTileXSize);
                CPLAssert(GDALGetRasterYSize(hDSTile) == nTileYSize);

                bHasFoundTile = TRUE;
                
                int bHasJustMemsetTileBand1 = FALSE;
                
                /* If the source tile doesn't fit the entire block size, then */
                /* we memset 0 before */
                if (!(nDstXOff == 0 && nDstYOff == 0 &&
                      nReqXSize == nBlockXSize && nReqYSize == nBlockYSize) &&
                    !bHasMemsetTile)
                {
                    memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize);
                    bHasMemsetTile = TRUE;
                    bHasJustMemsetTileBand1 = TRUE;
                }
                
                GDALColorTable* poTileCT =
                    (GDALColorTable* )GDALGetRasterColorTable(GDALGetRasterBand(hDSTile, 1));
                unsigned char* pabyTranslationTable = NULL;
                if (poGDS->nBands == 1 && poGDS->poCT != NULL && poTileCT != NULL)
                {
                    pabyTranslationTable =
                        ((GDALRasterBand*)GDALGetRasterBand(hDSTile, 1))->
                                GetIndexColorTranslationTo(this, NULL, NULL);
                }
                    
/* -------------------------------------------------------------------- */
/*      Read tile data                                                  */
/* -------------------------------------------------------------------- */
                GDALRasterIO(GDALGetRasterBand(hDSTile, nReqBand), GF_Read,
                             nSrcXOff, nSrcYOff, nReqXSize, nReqYSize,
                             ((char*) pImage) + (nDstXOff + nDstYOff * nBlockXSize) * nDataTypeSize,
                             nReqXSize, nReqYSize,
                             eDataType, nDataTypeSize, nBlockXSize * nDataTypeSize);

                if (eDataType == GDT_Byte && pabyTranslationTable)
                {
/* -------------------------------------------------------------------- */
/*      Convert from tile CT to band CT                                 */
/* -------------------------------------------------------------------- */
                    int i, j;
                    for(j=nDstYOff;j<nDstYOff + nReqYSize;j++)
                    {
                        for(i=nDstXOff;i<nDstXOff + nReqXSize;i++)
                        {
                            GByte* pPixel = ((GByte*) pImage) + i + j * nBlockXSize;
                            *pPixel = pabyTranslationTable[*pPixel];
                        }
                    }
                    CPLFree(pabyTranslationTable);
                    pabyTranslationTable = NULL;
                }
                else if (eDataType == GDT_Byte && nTileBands == 1 &&
                         poGDS->nBands == 3 && poTileCT != NULL)
                {
/* -------------------------------------------------------------------- */
/*      Expand from PCT to RGB                                          */
/* -------------------------------------------------------------------- */
                    int i, j;
                    GByte abyCT[256];
                    int nEntries = MIN(256, poTileCT->GetColorEntryCount());
                    for(i=0;i<nEntries;i++)
                    {
                        const GDALColorEntry* psEntry = poTileCT->GetColorEntry(i);
                        if (nBand == 1)
                            abyCT[i] = (GByte)psEntry->c1;
                        else if (nBand == 2)
                            abyCT[i] = (GByte)psEntry->c2;
                        else
                            abyCT[i] = (GByte)psEntry->c3;
                    }
                    for(;i<256;i++)
                        abyCT[i] = 0;
                    
                    for(j=nDstYOff;j<nDstYOff + nReqYSize;j++)
                    {
                        for(i=nDstXOff;i<nDstXOff + nReqXSize;i++)
                        {
                            GByte* pPixel = ((GByte*) pImage) + i + j * nBlockXSize;
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
                    int iOtherBand;
                    for(iOtherBand=2;iOtherBand<=poGDS->nBands;iOtherBand++)
                    {
                        GDALRasterBlock *poBlock;

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
                        GDALRasterIO(GDALGetRasterBand(hDSTile, nReqBand), GF_Read,
                                     nSrcXOff, nSrcYOff, nReqXSize, nReqYSize,
                                     ((char*) pabySrcBlock) +
                                     (nDstXOff + nDstYOff * nBlockXSize) * nDataTypeSize,
                                     nReqXSize, nReqYSize,
                                     eDataType, nDataTypeSize, nBlockXSize * nDataTypeSize);
                        
                        if (eDataType == GDT_Byte && nTileBands == 1 &&
                            poGDS->nBands == 3 && poTileCT != NULL)
                        {
/* -------------------------------------------------------------------- */
/*      Expand from PCT to RGB                                          */
/* -------------------------------------------------------------------- */

                            int i, j;
                            GByte abyCT[256];
                            int nEntries = MIN(256, poTileCT->GetColorEntryCount());
                            for(i=0;i<nEntries;i++)
                            {
                                const GDALColorEntry* psEntry = poTileCT->GetColorEntry(i);
                                if (iOtherBand == 1)
                                    abyCT[i] = (GByte)psEntry->c1;
                                else if (iOtherBand == 2)
                                    abyCT[i] = (GByte)psEntry->c2;
                                else
                                    abyCT[i] = (GByte)psEntry->c3;
                            }
                            for(;i<256;i++)
                                abyCT[i] = 0;
                            
                            for(j=nDstYOff;j<nDstYOff + nReqYSize;j++)
                            {
                                for(i=nDstXOff;i<nDstXOff + nReqXSize;i++)
                                {
                                    GByte* pPixel = ((GByte*) pabySrcBlock) + i + j * nBlockXSize;
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
                printf("id = %d, NOT selected !\n",  nTileId);
            }
#endif
        }
        
        OGR_F_Destroy(hFeat);
    }
    
    if (!bHasFoundTile)
    {
        memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize);
    }
            
    OGR_DS_ReleaseResultSet(poGDS->hDS, hSQLLyr);
    
#ifdef RASTERLITE_DEBUG
    if (nBand == 1)
        printf("\n");
#endif

    return CE_None;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int RasterliteBand::GetOverviewCount()
{
    RasterliteDataset* poGDS = (RasterliteDataset*) poDS;
    
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
    RasterliteDataset* poGDS = (RasterliteDataset*) poDS;
    
    if (poGDS->nLimitOvrCount >= 0)
    {
        if (nLevel < 0 || nLevel >= poGDS->nLimitOvrCount)
            return NULL;
    }
    
    if (poGDS->nResolutions == 1)
        return GDALPamRasterBand::GetOverview(nLevel);
    
    if (nLevel < 0 || nLevel >= poGDS->nResolutions - 1)
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

GDALColorInterp RasterliteBand::GetColorInterpretation()
{
    RasterliteDataset* poGDS = (RasterliteDataset*) poDS;
    if (poGDS->nBands == 1)
    {
        if (poGDS->poCT != NULL)
            return GCI_PaletteIndex;
        else
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
    RasterliteDataset* poGDS = (RasterliteDataset*) poDS;
    if (poGDS->nBands == 1)
        return poGDS->poCT;
    else
        return NULL;
}

/************************************************************************/
/*                         RasterliteDataset()                          */
/************************************************************************/

RasterliteDataset::RasterliteDataset()
{
    nLimitOvrCount = -1;
    bValidGeoTransform = FALSE;
    bMustFree = FALSE;
    nLevel = 0;
    poMainDS = NULL;
    nResolutions = 0;
    padfXResolutions = NULL;
    padfYResolutions = NULL;
    pszSRS = NULL;
    hDS = NULL;
    papoOverviews = NULL;
    papszMetadata = NULL;
    papszSubDatasets = NULL;
    papszImageStructure =
        CSLAddString(NULL, "INTERLEAVE=PIXEL");
    poCT = NULL;
    bCheckForExistingOverview = TRUE;
}

/************************************************************************/
/*                         RasterliteDataset()                          */
/************************************************************************/

RasterliteDataset::RasterliteDataset(RasterliteDataset* poMainDS, int nLevel)
{
    nLimitOvrCount = -1;
    bMustFree = FALSE;
    this->nLevel = nLevel;
    this->poMainDS = poMainDS;
    nResolutions = poMainDS->nResolutions - nLevel;
    padfXResolutions = poMainDS->padfXResolutions + nLevel;
    padfYResolutions = poMainDS->padfYResolutions + nLevel;
    pszSRS = poMainDS->pszSRS;
    hDS = poMainDS->hDS;
    papoOverviews = poMainDS->papoOverviews + nLevel;
    papszMetadata = poMainDS->papszMetadata;
    papszSubDatasets = poMainDS->papszSubDatasets;
    papszImageStructure =  poMainDS->papszImageStructure;
    poCT =  poMainDS->poCT;
    bCheckForExistingOverview = TRUE;

    osTableName = poMainDS->osTableName;
    osFileName = poMainDS->osFileName;
    
    nRasterXSize = (int)(poMainDS->nRasterXSize *
        (poMainDS->padfXResolutions[0] / padfXResolutions[0]) + 0.5);
    nRasterYSize = (int)(poMainDS->nRasterYSize *
        (poMainDS->padfYResolutions[0] / padfYResolutions[0]) + 0.5);

    bValidGeoTransform = TRUE;
    memcpy(adfGeoTransform, poMainDS->adfGeoTransform, 6 * sizeof(double));
    adfGeoTransform[1] = padfXResolutions[0];
    adfGeoTransform[5] = - padfYResolutions[0];
}

/************************************************************************/
/*                        ~RasterliteDataset()                          */
/************************************************************************/

RasterliteDataset::~RasterliteDataset()
{
    CloseDependentDatasets();
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int RasterliteDataset::CloseDependentDatasets()
{
    int bRet = GDALPamDataset::CloseDependentDatasets();

    if (poMainDS == NULL && !bMustFree)
    {
        CSLDestroy(papszMetadata);
        papszMetadata = NULL;
        CSLDestroy(papszSubDatasets);
        papszSubDatasets = NULL;
        CSLDestroy(papszImageStructure);
        papszImageStructure = NULL;
        CPLFree(pszSRS);
        pszSRS = NULL;

        if (papoOverviews)
        {
            int i;
            for(i=1;i<nResolutions;i++)
            {
                if (papoOverviews[i-1] != NULL &&
                    papoOverviews[i-1]->bMustFree)
                {
                    papoOverviews[i-1]->poMainDS = NULL;
                }
                delete papoOverviews[i-1];
            }
            CPLFree(papoOverviews);
            papoOverviews = NULL;
            nResolutions = 0;
            bRet = TRUE;
        }

        if (hDS != NULL)
            OGRReleaseDataSource(hDS);
        hDS = NULL;

        CPLFree(padfXResolutions);
        CPLFree(padfYResolutions);
        padfXResolutions = padfYResolutions = NULL;

        delete poCT;
        poCT = NULL;
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
/*                           AddSubDataset()                            */
/************************************************************************/

void RasterliteDataset::AddSubDataset( const char* pszDSName)
{
    char	szName[80];
    int		nCount = CSLCount(papszSubDatasets ) / 2;

    sprintf( szName, "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets = 
        CSLSetNameValue( papszSubDatasets, szName, pszDSName);

    sprintf( szName, "SUBDATASET_%d_DESC", nCount+1 );
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
                                   "SUBDATASETS", "IMAGE_STRUCTURE", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **RasterliteDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;
        
    if( CSLCount(papszSubDatasets) < 2 &&
        pszDomain != NULL && EQUAL(pszDomain,"IMAGE_STRUCTURE") )
        return papszImageStructure;
        
    if ( pszDomain == NULL || EQUAL(pszDomain, "") )
        return papszMetadata;
        
    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *RasterliteDataset::GetMetadataItem( const char *pszName, 
                                                const char *pszDomain )
{
    if (pszDomain != NULL &&EQUAL(pszDomain,"OVERVIEWS") )
    {
        if (nResolutions > 1 || CSLCount(papszSubDatasets) > 2)
            return NULL;
        else
        {
            osOvrFileName.Printf("%s_%s", osFileName.c_str(), osTableName.c_str());
            if (bCheckForExistingOverview == FALSE ||
                CPLCheckForFile((char*) osOvrFileName.c_str(), NULL))
                return osOvrFileName.c_str();
            else
                return NULL;
        }
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
    else
        return CE_Failure;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char* RasterliteDataset::GetProjectionRef()
{
    if (pszSRS)
        return pszSRS;
    else
        return "";
}

/************************************************************************/
/*                           GetFileList()                              */
/************************************************************************/

char** RasterliteDataset::GetFileList()
{
    char** papszFileList = NULL;
    papszFileList = CSLAddString(papszFileList, osFileName);
    return papszFileList;
}

/************************************************************************/
/*                         GetBlockParams()                             */
/************************************************************************/

int RasterliteDataset::GetBlockParams(OGRLayerH hRasterLyr, int nLevel,
                                      int* pnBands, GDALDataType* peDataType,
                                      int* pnBlockXSize, int* pnBlockYSize)
{
    CPLString osSQL;
    
    osSQL.Printf("SELECT m.geometry, r.raster, m.id "
                 "FROM \"%s_metadata\" AS m, \"%s_rasters\" AS r "
                 "WHERE %s AND r.id = m.id",
                 osTableName.c_str(), osTableName.c_str(),
                 RasterliteGetPixelSizeCond(padfXResolutions[nLevel],padfYResolutions[nLevel], "m.").c_str());
    
    OGRLayerH hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
    if (hSQLLyr == NULL)
    {
        return FALSE;
    }
    
    OGRFeatureH hFeat = OGR_L_GetNextFeature(hRasterLyr);
    if (hFeat == NULL)
    {
        OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
        return FALSE;
    }
    
    int nDataSize;
    GByte* pabyData = OGR_F_GetFieldAsBinary(hFeat, 0, &nDataSize);
    
    if (nDataSize > 32 &&
        EQUALN((const char*)pabyData, "StartWaveletsImage$$", strlen("StartWaveletsImage$$")))
    {
        if (GDALGetDriverByName("EPSILON") == NULL)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Rasterlite driver doesn't support WAVELET compressed images if EPSILON driver is not compiled");
            OGR_F_Destroy(hFeat);
            OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
            return FALSE;
        }
    }
    
    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/%p", this);
    VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyData,
                                      nDataSize, FALSE);
    VSIFCloseL(fp);
    
    GDALDatasetH hDSTile = GDALOpen(osMemFileName.c_str(), GA_ReadOnly);
    if (hDSTile)
    {
        *pnBands = GDALGetRasterCount(hDSTile);
        if (*pnBands == 0)
        {
            GDALClose(hDSTile);
            hDSTile = NULL;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Can't open tile %d", 
                 OGR_F_GetFieldAsInteger(hFeat, 1));
    }
    
    if (hDSTile)
    {
        int iBand;
        *peDataType = GDALGetRasterDataType(GDALGetRasterBand(hDSTile, 1));
        
        for(iBand=2;iBand<=*pnBands;iBand++)
        {
            if (*peDataType != GDALGetRasterDataType(GDALGetRasterBand(hDSTile, 1)))
            {
                CPLError(CE_Failure, CPLE_NotSupported, "Band types must be identical");
                GDALClose(hDSTile);
                hDSTile = NULL;
                goto end;
            }
        }
        
        *pnBlockXSize = GDALGetRasterXSize(hDSTile);
        *pnBlockYSize = GDALGetRasterYSize(hDSTile);
        if (CSLFindName(papszImageStructure, "COMPRESSION") == -1)
        {
            const char* pszCompression =
                GDALGetMetadataItem(hDSTile, "COMPRESSION", "IMAGE_STRUCTURE");
            if (pszCompression != NULL && EQUAL(pszCompression, "JPEG"))
                papszImageStructure =
                    CSLAddString(papszImageStructure, "COMPRESSION=JPEG");
        }
        
        if (CSLFindName(papszMetadata, "TILE_FORMAT") == -1)
        {
            papszMetadata =
                CSLSetNameValue(papszMetadata, "TILE_FORMAT",
                           GDALGetDriverShortName(GDALGetDatasetDriver(hDSTile)));
        }
        
        
        if (*pnBands == 1 && this->poCT == NULL)
        {
            GDALColorTable* poCT =
                (GDALColorTable*)GDALGetRasterColorTable(GDALGetRasterBand(hDSTile, 1));
            if (poCT)
                this->poCT = poCT->Clone();
        }

        GDALClose(hDSTile);
    }
end:    
    VSIUnlink(osMemFileName.c_str());
    
    OGR_F_Destroy(hFeat);
    
    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
    
    return hDSTile != NULL;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int RasterliteDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MBTILES") &&
        !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "GPKG") &&
        poOpenInfo->nHeaderBytes >= 1024 &&
        EQUALN((const char*)poOpenInfo->pabyHeader, "SQLite Format 3", 15))
    {
        // Could be a SQLite/Spatialite file as well
        return -1;
    }
    else if (EQUALN(poOpenInfo->pszFilename, "RASTERLITE:", 11))
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
    CPLString osFileName;
    CPLString osTableName;
    char **papszTokens = NULL;
    int nLevel = 0;
    double minx = 0, miny = 0, maxx = 0, maxy = 0;
    int bMinXSet = FALSE, bMinYSet = FALSE, bMaxXSet = FALSE, bMaxYSet = FALSE;
    int nReqBands = 0;

    if( Identify(poOpenInfo) == FALSE )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Parse "file name"                                               */
/* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes >= 1024 &&
        EQUALN((const char*)poOpenInfo->pabyHeader, "SQLite Format 3", 15))
    {
        osFileName = poOpenInfo->pszFilename;
    }
    else
    {
        papszTokens = CSLTokenizeStringComplex( 
                poOpenInfo->pszFilename + 11, ",", FALSE, FALSE );
        int nTokens = CSLCount(papszTokens);
        if (nTokens == 0)
        {
            CSLDestroy(papszTokens);
            return NULL;
        }
                
        osFileName = papszTokens[0];
        
        int i;
        for(i=1;i<nTokens;i++)
        {
            if (EQUALN(papszTokens[i], "table=", 6))
                osTableName = papszTokens[i] + 6;
            else if (EQUALN(papszTokens[i], "level=", 6))
                nLevel = atoi(papszTokens[i] + 6);
            else if (EQUALN(papszTokens[i], "minx=", 5))
            {
                bMinXSet = TRUE;
                minx = CPLAtof(papszTokens[i] + 5);
            }
            else if (EQUALN(papszTokens[i], "miny=", 5))
            {
                bMinYSet = TRUE;
                miny = CPLAtof(papszTokens[i] + 5);
            }
            else if (EQUALN(papszTokens[i], "maxx=", 5))
            {
                bMaxXSet = TRUE;
                maxx = CPLAtof(papszTokens[i] + 5);
            }
            else if (EQUALN(papszTokens[i], "maxy=", 5))
            {
                bMaxYSet = TRUE;
                maxy = CPLAtof(papszTokens[i] + 5);
            }
            else if (EQUALN(papszTokens[i], "bands=", 6))
            {
                nReqBands = atoi(papszTokens[i] + 6);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid option : %s", papszTokens[i]);
            }
        }
    }
    
    if (OGRGetDriverCount() == 0)
        OGRRegisterAll();
    
/* -------------------------------------------------------------------- */
/*      Open underlying OGR DB                                          */
/* -------------------------------------------------------------------- */

    OGRDataSourceH hDS = RasterliteOpenSQLiteDB(osFileName.c_str(), poOpenInfo->eAccess);
    CPLDebug("RASTERLITE", "SQLite DB Open");
    
    RasterliteDataset* poDS = NULL;

    if (hDS == NULL)
        goto end;
    
    if (strlen(osTableName) == 0)
    {
        int nCountSubdataset = 0;
        int nLayers = OGR_DS_GetLayerCount(hDS);
        int i;
/* -------------------------------------------------------------------- */
/*      Add raster layers as subdatasets                                */
/* -------------------------------------------------------------------- */
        for(i=0;i<nLayers;i++)
        {   
            OGRLayerH hLyr = OGR_DS_GetLayer(hDS, i);
            const char* pszLayerName = OGR_FD_GetName(OGR_L_GetLayerDefn(hLyr));
            if (strstr(pszLayerName, "_metadata"))
            {
                char* pszShortName = CPLStrdup(pszLayerName);
                *strstr(pszShortName, "_metadata") = '\0';
                
                CPLString osRasterTableName = pszShortName;
                osRasterTableName += "_rasters";

                if (OGR_DS_GetLayerByName(hDS, osRasterTableName.c_str()) != NULL)
                {
                    if (poDS == NULL)
                    {
                        poDS = new RasterliteDataset();
                        osTableName = pszShortName;
                    }
                        
                    CPLString osSubdatasetName;
                    if (!EQUALN(poOpenInfo->pszFilename, "RASTERLITE:", 11))
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
        poDS = NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Build dataset                                                   */
/* -------------------------------------------------------------------- */
    {
        CPLString osMetadataTableName, osRasterTableName;
        CPLString osSQL;
        OGRLayerH hMetadataLyr, hRasterLyr, hRasterPyramidsLyr;
        OGRLayerH hSQLLyr;
        OGRFeatureH hFeat;
        int i, nResolutions = 0;
        int iBand, nBands, nBlockXSize, nBlockYSize;
        GDALDataType eDataType;

        osMetadataTableName = osTableName;
        osMetadataTableName += "_metadata";
        
        hMetadataLyr = OGR_DS_GetLayerByName(hDS, osMetadataTableName.c_str());
        if (hMetadataLyr == NULL)
            goto end;
            
        osRasterTableName = osTableName;
        osRasterTableName += "_rasters";
        
        hRasterLyr = OGR_DS_GetLayerByName(hDS, osRasterTableName.c_str());
        if (hRasterLyr == NULL)
            goto end;
        
/* -------------------------------------------------------------------- */
/*      Fetch resolutions                                               */
/* -------------------------------------------------------------------- */

        hRasterPyramidsLyr = OGR_DS_GetLayerByName(hDS, "raster_pyramids");
        if (hRasterPyramidsLyr)
        {
            osSQL.Printf("SELECT pixel_x_size, pixel_y_size "
                         "FROM raster_pyramids WHERE table_prefix = '%s' "
                         "ORDER BY pixel_x_size ASC",
                         osTableName.c_str());

            hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
            if (hSQLLyr != NULL)
            {
                nResolutions = OGR_L_GetFeatureCount(hSQLLyr, TRUE);
                if( nResolutions == 0 )
                {
                    OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
                    hSQLLyr = NULL;
                }
            }
        }
        else
            hSQLLyr = NULL;

        if( hSQLLyr == NULL )
        {
            osSQL.Printf("SELECT DISTINCT(pixel_x_size), pixel_y_size "
                         "FROM \"%s_metadata\" WHERE pixel_x_size != 0  "
                         "ORDER BY pixel_x_size ASC",
                         osTableName.c_str());

            hSQLLyr = OGR_DS_ExecuteSQL(hDS, osSQL.c_str(), NULL, NULL);
            if (hSQLLyr == NULL)
                goto end;

            nResolutions = OGR_L_GetFeatureCount(hSQLLyr, TRUE);

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
        hDS = NULL;
        
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
        poDS->padfXResolutions =
            (double*)CPLMalloc(sizeof(double) * poDS->nResolutions);
        poDS->padfYResolutions =
            (double*)CPLMalloc(sizeof(double) * poDS->nResolutions);

        i = 0;
        while((hFeat = OGR_L_GetNextFeature(hSQLLyr)) != NULL)
        {
            poDS->padfXResolutions[i] = OGR_F_GetFieldAsDouble(hFeat, 0);
            poDS->padfYResolutions[i] = OGR_F_GetFieldAsDouble(hFeat, 1);

            OGR_F_Destroy(hFeat);
            
            //printf("[%d] xres=%.15f yres=%.15f\n", i,
            //       poDS->padfXResolutions[i], poDS->padfYResolutions[i]);
            
            if (poDS->padfXResolutions[i] <= 0 || poDS->padfYResolutions[i] <= 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "res=%d, xres=%.15f, yres=%.15f",
                         i, poDS->padfXResolutions[i], poDS->padfYResolutions[i]);
                OGR_DS_ReleaseResultSet(hDS, hSQLLyr);
                delete poDS;
                poDS = NULL;
                goto end;
            }
            i ++;
        }

        OGR_DS_ReleaseResultSet(poDS->hDS, hSQLLyr);
        hSQLLyr = NULL;

/* -------------------------------------------------------------------- */
/*      Compute raster size, geotransform and projection                */
/* -------------------------------------------------------------------- */
        poDS->nRasterXSize =
            (int)((oEnvelope.MaxX - oEnvelope.MinX) / poDS->padfXResolutions[0] + 0.5);
        poDS->nRasterYSize =
            (int)((oEnvelope.MaxY - oEnvelope.MinY) / poDS->padfYResolutions[0] + 0.5);

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

        if (poDS->GetBlockParams(hRasterLyr, 0, &nBands, &eDataType,
                                 &nBlockXSize, &nBlockYSize) == FALSE)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find block characteristics");
            delete poDS;
            poDS = NULL;
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
        
        for(iBand=0;iBand<nBands;iBand++)
            poDS->SetBand(iBand+1, new RasterliteBand(poDS, iBand+1, eDataType, 
                                                  nBlockXSize, nBlockYSize));
        
/* -------------------------------------------------------------------- */
/*      Add overview levels as internal datasets                        */
/* -------------------------------------------------------------------- */
        if (nResolutions > 1)
        {
            poDS->papoOverviews = (RasterliteDataset**)
                CPLCalloc(nResolutions - 1, sizeof(RasterliteDataset*));
            int nLev;
            for(nLev=1;nLev<nResolutions;nLev++)
            {
                int nOvrBands;
                GDALDataType eOvrDataType;
                if (poDS->GetBlockParams(hRasterLyr, nLev, &nOvrBands, &eOvrDataType,
                                         &nBlockXSize, &nBlockYSize) == FALSE)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot find block characteristics for overview %d", nLev);
                    delete poDS;
                    poDS = NULL;
                    goto end;
                }
                
                if (eDataType == GDT_Byte && nOvrBands == 1 && nReqBands == 3)
                    nOvrBands = 3;
                    
                if (nBands != nOvrBands || eDataType != eOvrDataType)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Overview %d has not the same number characteristics as main band", nLev);
                    delete poDS;
                    poDS = NULL;
                    goto end;
                }
                
                poDS->papoOverviews[nLev-1] = new RasterliteDataset(poDS, nLev);
                    
                for(iBand=0;iBand<nBands;iBand++)
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
            poDS = NULL;
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
    CSLDestroy(papszTokens);
    
    return poDS;
}

/************************************************************************/
/*                     GDALRegister_Rasterlite()                        */
/************************************************************************/

void GDALRegister_Rasterlite()

{
    GDALDriver  *poDriver;
    
    if (! GDAL_CHECK_VERSION("Rasterlite driver"))
        return;

    if( GDALGetDriverByName( "Rasterlite" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "Rasterlite" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Rasterlite" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_rasterlite.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "sqlite" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16 Int16 UInt32 Int32 Float32 "
                                   "Float64 CInt16 CInt32 CFloat32 CFloat64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='WIPE' type='boolean' default='NO' description='Erase all prexisting data in the specified table'/>"
"   <Option name='TILED' type='boolean' default='YES' description='Use tiling'/>"
"   <Option name='BLOCKXSIZE' type='int' default='256' description='Tile Width'/>"
"   <Option name='BLOCKYSIZE' type='int' default='256' description='Tile Height'/>"
"   <Option name='DRIVER' type='string' default='GTiff' description='GDAL driver to use for storing tiles' default='GTiff'/>"
"   <Option name='COMPRESS' type='string' default='(GTiff driver) Compression method' default='NONE'/>"
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
"   <Option name='TARGET' type='int' description='(EPSILON driver) target size reduction as a percentage of the original (0-100)' default='96'/>"
"   <Option name='FILTER' type='string' description='(EPSILON driver) Filter ID' default='daub97lift'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = RasterliteDataset::Open;
        poDriver->pfnIdentify = RasterliteDataset::Identify;
        poDriver->pfnCreateCopy = RasterliteCreateCopy;
        poDriver->pfnDelete = RasterliteDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
