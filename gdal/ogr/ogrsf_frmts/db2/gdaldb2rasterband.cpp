/******************************************************************************
 *
 * Project:  DB2 Translator
 * Purpose:  Implements GDALDB2RasterBand class
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2015, David Adler
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

#include "ogr_db2.h"
#include "memdataset.h"
#include "gdal_alg_priv.h"

CPL_CVSID("$Id$")

static char* GByteArrayToHexString( const GByte* pabyData, int nLen);
//#define DEBUG_VERBOSE

/************************************************************************/
/*                      GDALDB2RasterBand()                      */
/************************************************************************/

GDALDB2RasterBand::GDALDB2RasterBand(OGRDB2DataSource* poDSIn,
                                     int nBandIn,
                                     int nTileWidth, int nTileHeight)
{
    this->poDS = poDSIn;
    this->nBand = nBandIn;
    eDataType = GDT_Byte;
    nBlockXSize = nTileWidth;
    nBlockYSize = nTileHeight;
}

/************************************************************************/
/*                              FlushCache()                            */
/************************************************************************/

CPLErr GDALDB2RasterBand::FlushCache()
{
    CPLDebug("GDALDB2RasterBand::FlushCache","Entering");
    OGRDB2DataSource* poGDS = (OGRDB2DataSource* )poDS;
    CPLDebug("GDALDB2RasterBand::FlushCache","Calling FGDALPamRasterBand::FlushCache");

    if( GDALPamRasterBand::FlushCache() != CE_None )
        return CE_Failure;
    CPLDebug("GDALDB2RasterBand::FlushCache","Calling FlushCacheWithErrCode");
    return poGDS->FlushCacheWithErrCode();
}

/************************************************************************/
/*                             GetColorTable()                          */
/************************************************************************/

GDALColorTable* GDALDB2RasterBand::GetColorTable()
{

    OGRDB2DataSource* poGDS = (OGRDB2DataSource* )poDS;
    if( poGDS->nBands != 1 )
        return NULL;

    if( !poGDS->m_bTriedEstablishingCT )
    {
        poGDS->m_bTriedEstablishingCT = TRUE;
        if( poGDS->m_poParentDS != NULL )
        {
            poGDS->m_poCT = poGDS->m_poParentDS->GetRasterBand(1)->GetColorTable();
            if( poGDS->m_poCT )
                poGDS->m_poCT = poGDS->m_poCT->Clone();
            return poGDS->m_poCT;
        }
        CPL_SQLLEN nBlobLen = 0;
        SQLINTEGER nDataLen;
        OGRDB2Session *oSession =  poGDS->GetSession();
        OGRDB2Statement oStatement( poGDS->GetSession() );
        oStatement.Appendf("SELECT LENGTH(tile_data), tile_data FROM %s "
                           "WHERE zoom_level = %d FETCH FIRST ROW ONLY",
                           poGDS->m_osRasterTable.c_str(), poGDS->m_nZoomLevel);
#ifdef DEBUG_SQL
        CPLDebug("GDALDB2RasterBand::GetColorTable",
                 "stmt: '%s'", oStatement.GetCommand());
#endif

        int nRetCode = SQLExecDirect(oStatement.GetStatement(),
                                     (SQLCHAR *) oStatement.GetCommand(),
                                     SQL_NTS);

        if (nRetCode)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed reading color table; error: %s",
                     oSession->GetLastError());
            CPLDebug("GDALDB2RasterBand::GetColorTable",
                     "Failed reading color table; error: %s",
                     oSession->GetLastError());
            return NULL;
        }

        nRetCode = SQLFetch(oStatement.GetStatement() );
        CPLDebug("GDALDB2RasterBand::GetColorTable",
                 "Fetch nRetCode: %d", nRetCode);
        if( !(nRetCode == SQL_SUCCESS || nRetCode == SQL_NO_DATA))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed fetching color table; error: %s",
                     oSession->GetLastError());
            CPLDebug("GDALDB2RasterBand::GetColorTable",
                     "Failed fetching color table; error: %s",
                     oSession->GetLastError());
            return NULL;
        }

// If we got color data, process it.
        if( nRetCode == SQL_SUCCESS)
        {
// Get the length
            nRetCode = SQLGetData(oStatement.GetStatement(),
                                  (SQLUSMALLINT) 1,
                                  SQL_C_SLONG,
                                  (SQLPOINTER) &nDataLen,
                                  4,
                                  0);

            if (nRetCode != SQL_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed fetching tile_data; error: %s",
                         oSession->GetLastError());
                CPLDebug("OGRDB2DataSource::ReadTile",
                         "Failed fetching tile_data; error: %s",
                         oSession->GetLastError());
                return NULL;
            }

// Allocate a buffer to read the tile BLOB into based on the
// length(tile_data) value
            GByte* pabyBlob = (GByte*) VSIMalloc(nDataLen);
            CPLDebug("OGRDB2DataSource::ReadTile2",
                     "nDataLen: %d; pabyBlob: %p", nDataLen, pabyBlob);
            nRetCode = SQLGetData(oStatement.GetStatement(),
                                  (SQLUSMALLINT) 2,
                                  SQL_C_BINARY,
                                  (SQLPOINTER) pabyBlob,
                                  nDataLen,
                                  &nBlobLen);

            if (nRetCode != SQL_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed fetching tile_data; error: %s",
                         oSession->GetLastError());
                CPLDebug("OGRDB2DataSource::ReadTile",
                         "Failed fetching tile_data; error: %s",
                         oSession->GetLastError());
                return NULL;
            }

            CPLString osMemFileName;
            osMemFileName.Printf("/vsimem/gpkg_read_tile_%p", this);
            VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyBlob,
                                                  nDataLen, FALSE);
            VSIFCloseL(fp);

            /* Only PNG can have color table */
            const char* apszDrivers[] = { "PNG", NULL };
            GDALDataset* poDSTile = (GDALDataset*)GDALOpenEx(osMemFileName.c_str(),
                                    GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                    apszDrivers, NULL, NULL);
            if( poDSTile != NULL )
            {
                if( poDSTile->GetRasterCount() == 1 )
                {
                    poGDS->m_poCT = poDSTile->GetRasterBand(1)->GetColorTable();
                    if( poGDS->m_poCT != NULL )
                        poGDS->m_poCT = poGDS->m_poCT->Clone();
                }
                GDALClose( poDSTile );
            }

            VSIUnlink(osMemFileName);
        }
    }

    return poGDS->m_poCT;
}

/************************************************************************/
/*                             SetColorTable()                          */
/************************************************************************/

CPLErr GDALDB2RasterBand::SetColorTable(GDALColorTable* poCT)
{
    OGRDB2DataSource* poGDS = (OGRDB2DataSource* )poDS;
    if( poGDS->nBands != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetColorTable() only supported for a single band dataset");
        return CE_Failure;
    }
    if( !poGDS->m_bNew || poGDS->m_bTriedEstablishingCT )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetColorTable() only supported on a newly created dataset");
        return CE_Failure;
    }

    poGDS->m_bTriedEstablishingCT = TRUE;
    delete poGDS->m_poCT;
    if( poCT != NULL )
        poGDS->m_poCT = poCT->Clone();
    else
        poGDS->m_poCT = NULL;
    return CE_None;
}

/************************************************************************/
/*                        GetColorInterpretation()                      */
/************************************************************************/

GDALColorInterp GDALDB2RasterBand::GetColorInterpretation()
{
    OGRDB2DataSource* poGDS = (OGRDB2DataSource* )poDS;
    if( poGDS->nBands == 1 )
        return GetColorTable() ? GCI_PaletteIndex : GCI_GrayIndex;
    else if( poGDS->nBands == 2 )
        return (nBand == 1) ? GCI_GrayIndex : GCI_AlphaBand;
    else
        return (GDALColorInterp) (GCI_RedBand + (nBand - 1));
}

/************************************************************************/
/*                        SetColorInterpretation()                      */
/************************************************************************/

CPLErr GDALDB2RasterBand::SetColorInterpretation( GDALColorInterp eInterp )
{
    OGRDB2DataSource* poGDS = (OGRDB2DataSource* )poDS;
    if( eInterp == GCI_Undefined )
        return CE_None;
    if( poGDS->nBands == 1 && (eInterp == GCI_GrayIndex || eInterp == GCI_PaletteIndex) )
        return CE_None;
    if( poGDS->nBands == 2 &&
            ((nBand == 1 && eInterp == GCI_GrayIndex) || (nBand == 2 && eInterp == GCI_AlphaBand)) )
        return CE_None;
    if( poGDS->nBands >= 3 && eInterp == GCI_RedBand + nBand - 1 )
        return CE_None;
    CPLError(CE_Warning, CPLE_NotSupported, "%s color interpretation not supported. Will be ignored",
             GDALGetColorInterpretationName(eInterp));
    return CE_Warning;
}

/************************************************************************/
/*                        GPKGFindBestEntry()                           */
/************************************************************************/

static int GPKGFindBestEntry(GDALColorTable* poCT,
                             GByte c1, GByte c2, GByte c3, GByte c4,
                             int nTileBandCount)
{
    int nEntries = MIN(256, poCT->GetColorEntryCount());
    int iBestIdx = 0;
    int nBestDistance = 4 * 256 * 256;
    for(int i=0; i<nEntries; i++)
    {
        const GDALColorEntry* psEntry = poCT->GetColorEntry(i);
        int nDistance = (psEntry->c1 - c1) * (psEntry->c1 - c1) +
                        (psEntry->c2 - c2) * (psEntry->c2 - c2) +
                        (psEntry->c3 - c3) * (psEntry->c3 - c3);
        if( nTileBandCount == 4 )
            nDistance += (psEntry->c4 - c4) * (psEntry->c4 - c4);
        if( nDistance < nBestDistance )
        {
            iBestIdx = i;
            nBestDistance = nDistance;
        }
    }
    return iBestIdx;
}

/************************************************************************/
/*                           ReadTile()                                 */
/************************************************************************/

CPLErr OGRDB2DataSource::ReadTile(const CPLString& osMemFileName,
                                  GByte* pabyTileData,
                                  int* pbIsLossyFormat)
{
    CPLDebug("OGRDB2DataSource::ReadTile0","Entering; memFile: %s", osMemFileName.c_str());
    const char* apszDrivers[] = { "JPEG", "PNG", "WEBP", NULL };
    int nBlockXSize, nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    GDALDataset* poDSTile = (GDALDataset*)GDALOpenEx(osMemFileName.c_str(),
                            GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                            apszDrivers, NULL, NULL);
    if( poDSTile == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot parse tile data");
        memset(pabyTileData, 0, nBands * nBlockXSize * nBlockYSize );
        return CE_Failure;
    }

    int nTileBandCount = poDSTile->GetRasterCount();

    if( !(poDSTile->GetRasterXSize() == nBlockXSize &&
            poDSTile->GetRasterYSize() == nBlockYSize &&
            (nTileBandCount >= 1 && nTileBandCount <= 4)) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent tiles characteristics");
        GDALClose(poDSTile);
        memset(pabyTileData, 0, nBands * nBlockXSize * nBlockYSize );
        return CE_Failure;
    }

    if( poDSTile->RasterIO(GF_Read, 0, 0, nBlockXSize, nBlockYSize,
                           pabyTileData,
                           nBlockXSize, nBlockYSize,
                           GDT_Byte,
                           poDSTile->GetRasterCount(), NULL,
                           0, 0, 0, NULL) != CE_None )
    {
        GDALClose(poDSTile);
        memset(pabyTileData, 0, nBands * nBlockXSize * nBlockYSize );
        return CE_Failure;
    }

    GDALColorTable* poCT = NULL;
    if( nBands == 1 || nTileBandCount == 1 )
    {
        poCT = poDSTile->GetRasterBand(1)->GetColorTable();
        GetRasterBand(1)->GetColorTable();
    }
    CPLDebug("DB2_RB","get description: '%s'", poDSTile->GetDriver()->GetDescription());
    if( pbIsLossyFormat )
        *pbIsLossyFormat = !EQUAL(poDSTile->GetDriver()->GetDescription(), "PNG") ||
                           (poCT != NULL && poCT->GetColorEntryCount() == 256) /* PNG8 */;

    /* Map RGB(A) tile to single-band color indexed */
    if( nBands == 1 && m_poCT != NULL && nTileBandCount != 1 )
    {
        std::map< GUInt32, int > oMapEntryToIndex;
        int nEntries = MIN(256, m_poCT->GetColorEntryCount());
        for(int i=0; i<nEntries; i++)
        {
            const GDALColorEntry* psEntry = m_poCT->GetColorEntry(i);
            GByte c1 = (GByte)psEntry->c1;
            GByte c2 = (GByte)psEntry->c2;
            GByte c3 = (GByte)psEntry->c3;
            GUInt32 nVal = c1 + (c2 << 8) + (c3 << 16);
            if( nTileBandCount == 4 ) nVal += ((GByte)psEntry->c4 << 24);
            oMapEntryToIndex[nVal] = i;
        }
        int iBestEntryFor0 = GPKGFindBestEntry(m_poCT, 0, 0, 0, 0, nTileBandCount);
        for(int i=0; i<nBlockXSize*nBlockYSize; i++)
        {
            GByte c1 = pabyTileData[i];
            GByte c2 = pabyTileData[i + nBlockXSize * nBlockYSize];
            GByte c3 = pabyTileData[i + 2 * nBlockXSize * nBlockYSize];
            GByte c4 = pabyTileData[i + 3 * nBlockXSize * nBlockYSize];
            GUInt32 nVal = c1 + (c2 << 8) + (c3 << 16);
            if( nTileBandCount == 4 ) nVal += (c4 << 24);
            if( nVal == 0 ) /* In most cases we will reach that point at partial tiles */
                pabyTileData[i] = (GByte) iBestEntryFor0;
            else
            {
                std::map< GUInt32, int >::iterator oMapEntryToIndexIter = oMapEntryToIndex.find(nVal);
                if( oMapEntryToIndexIter == oMapEntryToIndex.end() )
                    /* Could happen with JPEG tiles */
                    pabyTileData[i] = (GByte) GPKGFindBestEntry(m_poCT, c1, c2, c3, c4, nTileBandCount);
                else
                    pabyTileData[i] = (GByte) oMapEntryToIndexIter->second;
            }
        }
        GDALClose( poDSTile );
        return CE_None;
    }

    if( nBands == 1 && nTileBandCount == 1 && poCT != NULL && m_poCT != NULL &&
            !poCT->IsSame(m_poCT) )
    {
        CPLError(CE_Warning, CPLE_NotSupported, "Different color tables. Unhandled for now");
    }
    else if( (nBands == 1 && nTileBandCount >= 3) ||
             (nBands == 1 && nTileBandCount == 1 && m_poCT != NULL && poCT == NULL) ||
             ((nBands == 1 || nBands == 2) && nTileBandCount == 1 && m_poCT == NULL && poCT != NULL) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent dataset and tiles band characteristics");
    }

    if( nBands == 2 )
    {
        if( nTileBandCount == 1 || nTileBandCount == 3 /* assuming that the RGB is Grey,Grey,Grey */ )
        {
            /* Create fully opaque alpha */
            memset(pabyTileData + 1 * nBlockXSize * nBlockYSize,
                   255, nBlockXSize * nBlockYSize);
        }
        else if( nTileBandCount == 4 )
        {
            /* Transfer alpha band */
            memcpy(pabyTileData + 1 * nBlockXSize * nBlockYSize,
                   pabyTileData + 3 * nBlockXSize * nBlockYSize,
                   nBlockXSize * nBlockYSize);
        }
    }
    else if( nTileBandCount == 2 )
    {
        /* Do Grey+Alpha -> RGBA */
        memcpy(pabyTileData + 3 * nBlockXSize * nBlockYSize,
               pabyTileData + 1 * nBlockXSize * nBlockYSize,
               nBlockXSize * nBlockYSize);
        memcpy(pabyTileData + 1 * nBlockXSize * nBlockYSize,
               pabyTileData, nBlockXSize * nBlockYSize);
        memcpy(pabyTileData + 2 * nBlockXSize * nBlockYSize,
               pabyTileData, nBlockXSize * nBlockYSize);
    }
    else if( nTileBandCount == 1 && !(nBands == 1 && m_poCT != NULL) )
    {
        /* Expand color indexed to RGB(A) */
        if( poCT != NULL )
        {
            int i;
            GByte abyCT[4*256];
            int nEntries = MIN(256, poCT->GetColorEntryCount());
            for(i=0; i<nEntries; i++)
            {
                const GDALColorEntry* psEntry = poCT->GetColorEntry(i);
                abyCT[4*i] = (GByte)psEntry->c1;
                abyCT[4*i+1] = (GByte)psEntry->c2;
                abyCT[4*i+2] = (GByte)psEntry->c3;
                abyCT[4*i+3] = (GByte)psEntry->c4;
            }
            for(; i<256; i++)
            {
                abyCT[4*i] = 0;
                abyCT[4*i+1] = 0;
                abyCT[4*i+2] = 0;
                abyCT[4*i+3] = 0;
            }
            for(i=0; i<nBlockXSize * nBlockYSize; i++)
            {
                GByte byVal = pabyTileData[i];
                pabyTileData[i] = abyCT[4*byVal];
                pabyTileData[i + 1 * nBlockXSize * nBlockYSize] = abyCT[4*byVal+1];
                pabyTileData[i + 2 * nBlockXSize * nBlockYSize] = abyCT[4*byVal+2];
                pabyTileData[i + 3 * nBlockXSize * nBlockYSize] = abyCT[4*byVal+3];
            }
        }
        else
        {
            memcpy(pabyTileData + 1 * nBlockXSize * nBlockYSize,
                   pabyTileData, nBlockXSize * nBlockYSize);
            memcpy(pabyTileData + 2 * nBlockXSize * nBlockYSize,
                   pabyTileData, nBlockXSize * nBlockYSize);
            if( nBands == 4 )
            {
                memset(pabyTileData + 3 * nBlockXSize * nBlockYSize,
                       255, nBlockXSize * nBlockYSize);
            }
        }
    }
    else if( nTileBandCount == 3 && nBands == 4 )
    {
        /* Create fully opaque alpha */
        memset(pabyTileData + 3 * nBlockXSize * nBlockYSize,
               255, nBlockXSize * nBlockYSize);
    }

    GDALClose( poDSTile );

    return CE_None;
}

/************************************************************************/
/*                           ReadTile()                                 */
/************************************************************************/

GByte* OGRDB2DataSource::ReadTile(int nRow, int nCol)
{
    CPLDebug("OGRDB2DataSource::ReadTile1","Entering; nRow: %d; nCol: %d", nRow, nCol);
    GByte* pabyData = NULL;

    int nBlockXSize, nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    if( m_nShiftXPixelsMod )
    {
        int i;
        for(i = 0; i < 4; i ++)
        {
            if( m_asCachedTilesDesc[i].nRow == nRow &&
                    m_asCachedTilesDesc[i].nCol == nCol )
            {
                if( m_asCachedTilesDesc[i].nIdxWithinTileData >= 0 )
                {
                    return m_pabyCachedTiles +
                           m_asCachedTilesDesc[i].nIdxWithinTileData * 4 * nBlockXSize * nBlockYSize;
                }
                else
                {
                    if( i == 0 )
                        m_asCachedTilesDesc[i].nIdxWithinTileData =
                            (m_asCachedTilesDesc[1].nIdxWithinTileData == 0 ) ? 1 : 0;
                    else if( i == 1 )
                        m_asCachedTilesDesc[i].nIdxWithinTileData =
                            (m_asCachedTilesDesc[0].nIdxWithinTileData == 0 ) ? 1 : 0;
                    else if( i == 2 )
                        m_asCachedTilesDesc[i].nIdxWithinTileData =
                            (m_asCachedTilesDesc[3].nIdxWithinTileData == 2 ) ? 3 : 2;
                    else
                        m_asCachedTilesDesc[i].nIdxWithinTileData =
                            (m_asCachedTilesDesc[2].nIdxWithinTileData == 2 ) ? 3 : 2;
                    pabyData = m_pabyCachedTiles +
                               m_asCachedTilesDesc[i].nIdxWithinTileData * 4 * nBlockXSize * nBlockYSize;
                    break;
                }
            }
        }
        CPLAssert(i < 4);
    }
    else
        pabyData = m_pabyCachedTiles;

    return ReadTile(nRow, nCol, pabyData);
}

/************************************************************************/
/*                           ReadTile()                                 */
/************************************************************************/

GByte* OGRDB2DataSource::ReadTile(int nRow, int nCol, GByte* pabyData,
                                  int* pbIsLossyFormat)
{
    CPLDebug("OGRDB2DataSource::ReadTile2","Entering; nRow: %d; nCol: %d", nRow, nCol);

    int nBlockXSize, nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    SQLINTEGER nBlockSize = nBlockXSize * nBlockYSize;
    CPL_SQLLEN nBlobLen = 0;
    SQLINTEGER nDataLen;
    if( pbIsLossyFormat ) *pbIsLossyFormat = FALSE;

    if( nRow < 0 || nCol < 0 || nRow >= m_nTileMatrixHeight ||
            nCol >= m_nTileMatrixWidth )
    {
        memset(pabyData, 0, nBands * nBlockSize );
        return pabyData;
    }

    OGRDB2Statement oStatement( GetSession() );
    oStatement.Appendf("SELECT LENGTH(tile_data), tile_data FROM %s "
                       "WHERE zoom_level = %d AND tile_row = %d "
                       "AND tile_column = %d%s",
                       m_osRasterTable.c_str(), m_nZoomLevel, nRow, nCol,
                       !m_osWHERE.empty() ? CPLSPrintf(" AND (%s)", m_osWHERE.c_str()): "");

#ifdef DEBUG_SQL
    CPLDebug("OGRDB2DataSource::ReadTile2",
             "stmt: '%s'", oStatement.GetCommand());
#endif

// Use native ODBC / CLI functions here to avoid oStatement.Fetch()
// wanting to fetch tile data (BLOB) in multiple pieces and then
// returning as a character string
    int nRetCode = SQLExecDirect(oStatement.GetStatement(),
                                 (SQLCHAR *) oStatement.GetCommand(),
                                 SQL_NTS);
    if (nRetCode)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed reading tile_data; error: %s",
                 GetSession()->GetLastError());
        return NULL;
    }

    nRetCode = SQLFetch(oStatement.GetStatement() );
    CPLDebug("OGRDB2DataSource::ReadTile2",
             "Fetch nRetCode: %d", nRetCode);
    if( !(nRetCode == SQL_SUCCESS || nRetCode == SQL_NO_DATA))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed fetching tile_data; error: %s",
                 GetSession()->GetLastError());
        return NULL;
    }

// If we got tile_data, process it. SQL_NO_DATA means try partial
    if( nRetCode == SQL_SUCCESS)
    {
        nRetCode = SQLGetData(oStatement.GetStatement(),
                              (SQLUSMALLINT) 1,
                              SQL_C_SLONG,
                              (SQLPOINTER) &nDataLen,
                              4,
                              0);

        if (nRetCode != SQL_SUCCESS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed fetching tile_data; error: %s",
                     GetSession()->GetLastError());
            CPLDebug("OGRDB2DataSource::ReadTile",
                     "Failed fetching tile_data; error: %s",
                     GetSession()->GetLastError());
            return NULL;
        }

// Allocate a buffer to read the tile BLOB into based on the
// length(tile_data) value
        GByte* pabyBlob = (GByte*) VSIMalloc(nDataLen);
        CPLDebug("OGRDB2DataSource::ReadTile2",
                 "nDataLen: %d; pabyBlob: %p", nDataLen, pabyBlob);
        nRetCode = SQLGetData(oStatement.GetStatement(),
                              (SQLUSMALLINT) 2,
                              SQL_C_BINARY,
                              (SQLPOINTER) pabyBlob,
                              nDataLen,
                              &nBlobLen);

        if (nRetCode != SQL_SUCCESS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed fetching tile_data; error: %s",
                     GetSession()->GetLastError());
            CPLDebug("OGRDB2DataSource::ReadTile",
                     "Failed fetching tile_data; error: %s",
                     GetSession()->GetLastError());
            return NULL;
        }

        CPLString osMemFileName;
        osMemFileName.Printf("/vsimem/gpkg_read_tile_%p", this);
        VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyBlob,
                                              nDataLen, FALSE);
        VSIFCloseL(fp);
        CPLDebug("OGRDB2DataSource::ReadTile2",
                 "osMemFileName: %s; nDataLen %d", osMemFileName.c_str(), nDataLen);

        ReadTile(osMemFileName, pabyData, pbIsLossyFormat);
        VSIUnlink(osMemFileName);
        VSIFree(pabyBlob);
        return pabyData;
    }

    else
    {
        oStatement.Clear();

#ifdef LATER
        if( m_hTempDB && (m_nShiftXPixelsMod || m_nShiftYPixelsMod) )
        {
            oStatement.Appendf(
                "SELECT partial_flag, tile_data_band_1, tile_data_band_2, "
                "tile_data_band_3, tile_data_band_4 FROM partial_tiles WHERE "
                "zoom_level = %d AND tile_row = %d AND tile_column = %d",
                m_nZoomLevel, nRow, nCol);
#ifdef DEBUG_SQL
            CPLDebug("OGRDB2DataSource::ReadTile2",
                     "stmt: '%s'", oStatement.GetCommand());
#endif
            if( !oStatement.ExecuteSQL() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Read of partial_tiles failed; %s",
                         GetSession()->GetLastError());
                CPLDebug("OGRDB2DataSource::ReadTile2",
                         "Read of partial_tiles failed; %s",
                         GetSession()->GetLastError() );
                memset(pabyData, 0, nBands * nBlockXSize * nBlockYSize );
                return pabyData;
            }

            nRetCode = SQLFetch(oStatement.GetStatement() );
            CPLDebug("OGRDB2DataSource::ReadTile2",
                     "Fetch nRetCode: %d", nRetCode);

            if ( nRetCode!= SQL_SUCCESS )
            {
                SQLINTEGER nPartialFlag;
                nRetCode = SQLGetData(oStatement.GetStatement(),
                                      (SQLUSMALLINT) 1,
                                      SQL_C_SLONG,
                                      (SQLPOINTER) &nPartialFlag,
                                      4,
                                      0);

// Allocate a buffer to read the tile BLOB into based on the
// length(tile_data) value
                GByte* pabyBlob = (GByte*) VSIMalloc(nBlockSize);
                CPLDebug("OGRDB2DataSource::ReadTile2",
                         "nBlockSize: %d; pabyBlob: %p", nBlockSize, pabyBlob);
                for(int iBand = 1; iBand <= nBands; iBand ++ )
                {
                    GByte* pabyDestBand = pabyData + (iBand - 1) * nBlockSize;
                    nRetCode = SQLGetData(oStatement.GetStatement(),
                                          (SQLUSMALLINT) iBand + 1,
                                          SQL_C_BINARY,
                                          (SQLPOINTER) pabyBlob,
                                          nBlockSize,
                                          &nBlobLen);
                    if( nRetCode != SQL_SUCCESS )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Read of partial_tiles failed; %s",
                                 GetSession()->GetLastError());
                        CPLDebug("OGRDB2DataSource::ReadTile2",
                                 "Read of partial_tiles failed; %s",
                                 GetSession()->GetLastError() );
                        memset(pabyData, 0, nBands * nBlockSize );
                        return pabyData;
                    }
                    if( nPartialFlag & (((1 << 4)-1) << (4 * (iBand - 1))) )
                    {
                        CPLAssert( nBlobLen == nBlockXSize * nBlockYSize );
                        memcpy( pabyDestBand,
                                pabyBlob,
                                nBlockSize );
                    }
                    else
                    {
                        memset(pabyDestBand, 0, nBlockSize );
                    }
                }
                VSIFree(pabyBlob);
            }
            else
            {
                memset(pabyData, 0, nBands * nBlockSize );
            }
        }
        else
#endif
        {
            memset(pabyData, 0, nBands * nBlockSize );
        }
    }

    return pabyData;
}

/************************************************************************/
/*                         IReadBlock()                                 */
/************************************************************************/

CPLErr GDALDB2RasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                     void* pData)
{
    OGRDB2DataSource* poGDS = (OGRDB2DataSource* )poDS;
    CPLDebug("GDALDB2RasterBand::IReadBlock", "IReadBlock(nBand=%d,nBlockXOff=%d,nBlockYOff=%d",
             nBand,nBlockXOff,nBlockYOff);

    int nRowMin = nBlockYOff + poGDS->m_nShiftYTiles;
    int nRowMax = nRowMin;
    if( poGDS->m_nShiftYPixelsMod )
        nRowMax ++;

    int nColMin = nBlockXOff + poGDS->m_nShiftXTiles;
    int nColMax = nColMin;
    if( poGDS->m_nShiftXPixelsMod )
        nColMax ++;

    /* Optimize for left to right reading at constant row */
    if( poGDS->m_nShiftXPixelsMod )
    {
        if( nRowMin == poGDS->m_asCachedTilesDesc[0].nRow &&
                nColMin == poGDS->m_asCachedTilesDesc[0].nCol + 1 &&
                poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData >= 0 )
        {
            CPLAssert(nRowMin == poGDS->m_asCachedTilesDesc[1].nRow);
            CPLAssert(nColMin == poGDS->m_asCachedTilesDesc[1].nCol);
            CPLAssert(poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData == 0 ||
                      poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData == 1);

            /* 0 1  --> 1 -1 */
            /* 2 3      3 -1 */
            /* or */
            /* 1 0  --> 0 -1 */
            /* 3 2      2 -1 */
            poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData = poGDS->m_asCachedTilesDesc[1].nIdxWithinTileData;
            poGDS->m_asCachedTilesDesc[2].nIdxWithinTileData = poGDS->m_asCachedTilesDesc[3].nIdxWithinTileData;
        }
        else
        {
            poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData = -1;
            poGDS->m_asCachedTilesDesc[2].nIdxWithinTileData = -1;
        }
        poGDS->m_asCachedTilesDesc[0].nRow = nRowMin;
        poGDS->m_asCachedTilesDesc[0].nCol = nColMin;
        poGDS->m_asCachedTilesDesc[1].nRow = nRowMin;
        poGDS->m_asCachedTilesDesc[1].nCol = nColMin + 1;
        poGDS->m_asCachedTilesDesc[2].nRow = nRowMin + 1;
        poGDS->m_asCachedTilesDesc[2].nCol = nColMin;
        poGDS->m_asCachedTilesDesc[3].nRow = nRowMin + 1;
        poGDS->m_asCachedTilesDesc[3].nCol = nColMin + 1;
        poGDS->m_asCachedTilesDesc[1].nIdxWithinTileData = -1;
        poGDS->m_asCachedTilesDesc[3].nIdxWithinTileData = -1;
    }

    for(int nRow = nRowMin; nRow <= nRowMax; nRow ++)
    {
        for(int nCol = nColMin; nCol <= nColMax; nCol++ )
        {
            GByte* pabyTileData = poGDS->ReadTile(nRow, nCol);
            if( pabyTileData == NULL )
                return CE_Failure;

            for(int iBand=1; iBand<=poGDS->nBands; iBand++)
            {
                GDALRasterBlock* poBlock = NULL;
                GByte* pabyDest;
                if( iBand == nBand )
                {
                    pabyDest = (GByte*)pData;
                }
                else
                {
                    poBlock =
                        poGDS->GetRasterBand(iBand)->GetLockedBlockRef(nBlockXOff, nBlockYOff, TRUE);
                    if( poBlock == NULL )
                        continue;
                    if( poBlock->GetDirty() )
                    {
                        poBlock->DropLock();
                        continue;
                    }
                    pabyDest = (GByte*) poBlock->GetDataRef();
                }

                // Composite tile data into block data
                if( poGDS->m_nShiftXPixelsMod == 0 && poGDS->m_nShiftYPixelsMod == 0 )
                {
                    memcpy(pabyDest,
                           pabyTileData + (iBand - 1) * nBlockXSize * nBlockYSize,
                           nBlockXSize * nBlockYSize);
                }
                else
                {
                    int nSrcXOffset, nSrcXSize, nSrcYOffset, nSrcYSize;
                    int nDstXOffset, nDstYOffset;
                    if( nCol == nColMin )
                    {
                        nSrcXOffset = poGDS->m_nShiftXPixelsMod;
                        nSrcXSize = nBlockXSize - poGDS->m_nShiftXPixelsMod;
                        nDstXOffset = 0;
                    }
                    else
                    {
                        nSrcXOffset = 0;
                        nSrcXSize = poGDS->m_nShiftXPixelsMod;
                        nDstXOffset = nBlockXSize - poGDS->m_nShiftXPixelsMod;
                    }
                    if( nRow == nRowMin )
                    {
                        nSrcYOffset = poGDS->m_nShiftYPixelsMod;
                        nSrcYSize = nBlockYSize - poGDS->m_nShiftYPixelsMod;
                        nDstYOffset = 0;
                    }
                    else
                    {
                        nSrcYOffset = 0;
                        nSrcYSize = poGDS->m_nShiftYPixelsMod;
                        nDstYOffset = nBlockYSize - poGDS->m_nShiftYPixelsMod;
                    }
                    CPLDebug("GDALDB2RasterBand::IReadBlock", "Copy source tile x=%d,w=%d,y=%d,h=%d into buffet at x=%d,y=%d",
                             nSrcXOffset, nSrcXSize, nSrcYOffset, nSrcYSize, nDstXOffset, nDstYOffset);
                    for( int y=0; y<nSrcYSize; y++ )
                    {
                        GByte* pSrc = pabyTileData + (iBand - 1) * nBlockXSize * nBlockYSize +
                                      (y + nSrcYOffset) * nBlockXSize + nSrcXOffset;
                        GByte* pDst = pabyDest + (y + nDstYOffset) * nBlockXSize + nDstXOffset;
                        GDALCopyWords(pSrc, GDT_Byte, 1,
                                      pDst, GDT_Byte, 1,
                                      nSrcXSize);
                    }
                }

                if( poBlock )
                    poBlock->DropLock();
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                       WEBPSupports4Bands()                           */
/************************************************************************/

static int WEBPSupports4Bands()
{
    static int bRes = -1;
    if( bRes < 0 )
    {
        GDALDriver* poDrv = (GDALDriver*) GDALGetDriverByName("WEBP");
        if( poDrv == NULL || CPLTestBool(CPLGetConfigOption("GPKG_SIMUL_WEBP_3BAND", "FALSE")) )
            bRes = FALSE;
        else
        {
            // LOSSLESS and RGBA support appeared in the same version
            bRes = strstr(poDrv->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST), "LOSSLESS") != NULL;
        }
        if( poDrv != NULL && !bRes )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "The version of WEBP available does not support 4-band RGBA");
        }
    }
    return bRes;
}

/************************************************************************/
/*                         WriteTile()                                  */
/************************************************************************/

CPLErr OGRDB2DataSource::WriteTile()
{
    CPLAssert(!m_bInWriteTile);
    m_bInWriteTile = TRUE;
    CPLErr eErr = WriteTileInternal();
    m_bInWriteTile = FALSE;
    CPLDebug("OGRDB2DataSource::WriteTile","exit; eErr: %d", eErr);
    return eErr;
}

/* should only be called by WriteTile() */
CPLErr OGRDB2DataSource::WriteTileInternal()
{
    CPLDebug("OGRDB2DataSource::WriteTileInternal","entering");

    if( !(m_bUpdate && m_asCachedTilesDesc[0].nRow >= 0 &&
            m_asCachedTilesDesc[0].nCol >= 0 &&
            m_asCachedTilesDesc[0].nIdxWithinTileData == 0) )
        return CE_None;

    int nRow = m_asCachedTilesDesc[0].nRow;
    int nCol = m_asCachedTilesDesc[0].nCol;

    int bAllDirty = TRUE;
    int bAllNonDirty = TRUE;
    int i;
    for(i=0; i<nBands; i++)
    {
        if( m_asCachedTilesDesc[0].abBandDirty[i] )
            bAllNonDirty = FALSE;
        else
            bAllDirty = FALSE;
    }
    if( bAllNonDirty )
        return CE_None;

    int nBlockXSize, nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

    /* If all bands for that block are not dirty/written, we need to */
    /* fetch the missing ones if the tile exists */
    int bIsLossyFormat = FALSE;
    if( !bAllDirty )
    {
        for(i=1; i<=3; i++)
        {
            m_asCachedTilesDesc[i].nRow = -1;
            m_asCachedTilesDesc[i].nCol = -1;
            m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
        }
        ReadTile(nRow, nCol, m_pabyCachedTiles + 4 * nBlockXSize * nBlockYSize,
                 &bIsLossyFormat);
        for(i=0; i<nBands; i++)
        {
            if( !m_asCachedTilesDesc[0].abBandDirty[i] )
            {
                memcpy(m_pabyCachedTiles + i * nBlockXSize * nBlockYSize,
                       m_pabyCachedTiles + (4 + i) * nBlockXSize * nBlockYSize,
                       nBlockXSize * nBlockYSize);
            }
        }
    }

    /* Compute origin of tile in GDAL raster space */
    int nXOff = (nCol - m_nShiftXTiles) * nBlockXSize - m_nShiftXPixelsMod;
    int nYOff = (nRow - m_nShiftYTiles) * nBlockYSize - m_nShiftYPixelsMod;

    /* Assert that the tile at least intersects some of the GDAL raster space */
    CPLAssert(nXOff + nBlockXSize > 0);
    CPLAssert(nYOff + nBlockYSize > 0);
    /* Can happen if the tile of the raster is less than the block size */
    if( nXOff >= nRasterXSize || nYOff >= nRasterYSize )
        return CE_None;

    /* Validity area of tile data in intra-tile coordinate space */
    int iXOff = 0;
    int iYOff = 0;
    int iXCount = nBlockXSize;
    int iYCount = nBlockYSize;

    int bPartialTile = FALSE;
    int nAlphaBand = (nBands == 2) ? 2 : (nBands == 4) ? 4 : 0;
    if( nAlphaBand == 0 )
    {
        if( nXOff < 0 )
        {
            bPartialTile = TRUE;
            iXOff = -nXOff;
            iXCount += nXOff;
        }
        if( nXOff + nBlockXSize > nRasterXSize )
        {
            bPartialTile = TRUE;
            iXCount -= nXOff + nBlockXSize - nRasterXSize;
        }
        if( nYOff < 0 )
        {
            bPartialTile = TRUE;
            iYOff = -nYOff;
            iYCount += nYOff;
        }
        if( nYOff + nBlockYSize > nRasterYSize )
        {
            bPartialTile = TRUE;
            iYCount -= nYOff + nBlockYSize - nRasterYSize;
        }
        CPLAssert(iXOff >= 0);
        CPLAssert(iYOff >= 0);
        CPLAssert(iXCount > 0);
        CPLAssert(iYCount > 0);
        CPLAssert(iXOff + iXCount <= nBlockXSize);
        CPLAssert(iYOff + iYCount <= nBlockYSize);
    }

    m_asCachedTilesDesc[0].nRow = -1;
    m_asCachedTilesDesc[0].nCol = -1;
    m_asCachedTilesDesc[0].nIdxWithinTileData = -1;
    m_asCachedTilesDesc[0].abBandDirty[0] = FALSE;
    m_asCachedTilesDesc[0].abBandDirty[1] = FALSE;
    m_asCachedTilesDesc[0].abBandDirty[2] = FALSE;
    m_asCachedTilesDesc[0].abBandDirty[3] = FALSE;

    CPLErr eErr = CE_Failure;

    int bAllOpaque = TRUE;
    if( m_poCT == NULL && nAlphaBand != 0 )
    {
        GByte byFirstAlphaVal =  m_pabyCachedTiles[(nAlphaBand-1) * nBlockXSize * nBlockYSize];
        for(i=1; i<nBlockXSize * nBlockYSize; i++)
        {
            if( m_pabyCachedTiles[(nAlphaBand-1) * nBlockXSize * nBlockYSize + i] != byFirstAlphaVal )
                break;
        }
        if( i == nBlockXSize * nBlockYSize )
        {
            // If tile is fully transparent, don't serialize it and remove it if it exists
            if( byFirstAlphaVal == 0 )
            {
                CPLDebug("OGRDB2DataSource::WriteTileInternal","want to do delete");
                OGRDB2Statement oStatement( GetSession() );
                oStatement.Appendf( "DELETE FROM %s "
                                    "WHERE zoom_level = %d AND tile_row = %d "
                                    "AND tile_column = %d",
                                    m_osRasterTable.c_str(), m_nZoomLevel,
                                    nRow, nCol);
                CPLDebug("OGRDB2DataSource::WriteTileInternal",
                         "stmt: '%s'", oStatement.GetCommand());
                if( !oStatement.ExecuteSQL() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Failure when deleting tile (row=%d,col=%d) "
                             "at zoom_level=%d : %s",
                             nRow, nCol, m_nZoomLevel,
                             GetSession()->GetLastError());
                    CPLDebug("OGRDB2DataSource::WriteTileInternal",
                             " insert failed; '%s'",
                             GetSession()->GetLastError() );
                    return CE_Failure;
                }
            }
            bAllOpaque = (byFirstAlphaVal == 255);
        }
        else
            bAllOpaque = FALSE;
    }

    if( bIsLossyFormat )
    {
        CPLDebug("OGRDB2DataSource::WriteTileInternal", "Had to read tile (row=%d,col=%d) at zoom_level=%d, "
                 "stored in a lossy format, before rewriting it, causing potential extra quality loss",
                 nRow, nCol, m_nZoomLevel);
    }

    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/gpkg_write_tile_%p", this);
    const char* pszDriverName = "PNG";
    int bTileDriverSupports1Band = FALSE;
    int bTileDriverSupports2Bands = FALSE;
    int bTileDriverSupports4Bands = FALSE;
    int bTileDriverSupportsCT = FALSE;

    if( nBands == 1 )
        GetRasterBand(1)->GetColorTable();

    if( m_eTF == GPKG_TF_PNG_JPEG )
    {
        bTileDriverSupports1Band = TRUE;

        if( bPartialTile || (nBands == 2 && !bAllOpaque)
                || (nBands == 4 && !bAllOpaque) || m_poCT != NULL )
        {
            pszDriverName = "PNG";
            bTileDriverSupports2Bands = TRUE;
            bTileDriverSupports4Bands = TRUE;
            bTileDriverSupportsCT = TRUE;
        }
        else
            pszDriverName = "JPEG";
    }
    else if( m_eTF == GPKG_TF_PNG ||
             m_eTF == GPKG_TF_PNG8 )
    {
        pszDriverName = "PNG";
        bTileDriverSupports1Band = TRUE;
        bTileDriverSupports2Bands = TRUE;
        bTileDriverSupports4Bands = TRUE;
        bTileDriverSupportsCT = TRUE;
    }
    else if( m_eTF == GPKG_TF_JPEG )
    {
        pszDriverName = "JPEG";
        bTileDriverSupports1Band = TRUE;
    }
    else if( m_eTF == GPKG_TF_WEBP )
    {
        pszDriverName = "WEBP";
        bTileDriverSupports4Bands = WEBPSupports4Bands();
    }
    else
    {
        CPLAssert(false);
    }

    GDALDriver* l_poDriver = (GDALDriver*) GDALGetDriverByName(pszDriverName);
    if( l_poDriver != NULL)
    {
        GDALDataset* poMEMDS = MEMDataset::Create("", nBlockXSize, nBlockYSize,
                               0, GDT_Byte, NULL);
        int nTileBands = nBands;
        if( bPartialTile && nBands == 1 && m_poCT == NULL && bTileDriverSupports2Bands )
            nTileBands = 2;
        else if( bPartialTile && bTileDriverSupports4Bands )
            nTileBands = 4;
        else if( m_eTF == GPKG_TF_PNG8 && nBands >= 3 && bAllOpaque && !bPartialTile )
            nTileBands = 1;
        else if( nBands == 2 )
        {
            if ( bAllOpaque )
            {
                if (bTileDriverSupports2Bands )
                    nTileBands = 1;
                else
                    nTileBands = 3;
            }
            else if( !bTileDriverSupports2Bands )
            {
                if( bTileDriverSupports4Bands )
                    nTileBands = 4;
                else
                    nTileBands = 3;
            }
        }
        else if( nBands == 4 && (bAllOpaque || !bTileDriverSupports4Bands) )
            nTileBands = 3;
        else if( nBands == 1 && m_poCT != NULL && !bTileDriverSupportsCT )
        {
            nTileBands = 3;
            if( bTileDriverSupports4Bands )
            {
                for(i=0; i<m_poCT->GetColorEntryCount(); i++)
                {
                    const GDALColorEntry* psEntry = m_poCT->GetColorEntry(i);
                    if( psEntry->c4 == 0 )
                    {
                        nTileBands = 4;
                        break;
                    }
                }
            }
        }
        else if( nBands == 1 && m_poCT == NULL && !bTileDriverSupports1Band )
            nTileBands = 3;

        if( bPartialTile && (nTileBands == 2 || nTileBands == 4) )
        {
            int nTargetAlphaBand = nTileBands;
            memset(m_pabyCachedTiles + (nTargetAlphaBand-1) * nBlockXSize * nBlockYSize, 0,
                   nBlockXSize * nBlockYSize);
            for(int iY = iYOff; iY < iYOff + iYCount; iY ++)
            {
                memset(m_pabyCachedTiles + ((nTargetAlphaBand-1) * nBlockYSize + iY) * nBlockXSize + iXOff,
                       255, iXCount);
            }
        }

        for(i=0; i<nTileBands; i++)
        {
            char** papszOptions = NULL;
            char szDataPointer[32];
            int iSrc = i;
            if( nBands == 1 && m_poCT == NULL && nTileBands == 3 )
                iSrc = 0;
            else if( nBands == 1 && m_poCT == NULL && bPartialTile && nTileBands == 4 )
                iSrc = (i < 3) ? 0 : 3;
            else if( nBands == 2 && nTileBands >= 3 )
                iSrc = (i < 3) ? 0 : 1;
            int nRet = CPLPrintPointer(szDataPointer,
                                       m_pabyCachedTiles + iSrc * nBlockXSize * nBlockYSize,
                                       sizeof(szDataPointer));
            szDataPointer[nRet] = '\0';
            papszOptions = CSLSetNameValue(papszOptions, "DATAPOINTER", szDataPointer);
            poMEMDS->AddBand(GDT_Byte, papszOptions);
            if( i == 0 && nTileBands == 1 && m_poCT != NULL )
                poMEMDS->GetRasterBand(1)->SetColorTable(m_poCT);
            CSLDestroy(papszOptions);
        }

        if( m_eTF == GPKG_TF_PNG8 && nTileBands == 1 && nBands >= 3 )
        {
            GDALDataset* poMEM_RGB_DS = MEMDataset::Create("", nBlockXSize, nBlockYSize,
                                        0, GDT_Byte, NULL);
            for(i=0; i<3; i++)
            {
                char** papszOptions = NULL;
                char szDataPointer[32];
                int nRet = CPLPrintPointer(szDataPointer,
                                           m_pabyCachedTiles + i * nBlockXSize * nBlockYSize,
                                           sizeof(szDataPointer));
                szDataPointer[nRet] = '\0';
                papszOptions = CSLSetNameValue(papszOptions, "DATAPOINTER", szDataPointer);
                poMEM_RGB_DS->AddBand(GDT_Byte, papszOptions);
                CSLDestroy(papszOptions);
            }

            if( m_pabyHugeColorArray == NULL )
            {
                if( nBlockXSize <= 65536 / nBlockYSize )
                    m_pabyHugeColorArray = (GByte*) VSIMalloc(MEDIAN_CUT_AND_DITHER_BUFFER_SIZE_65536);
                else
                    m_pabyHugeColorArray = (GByte*) VSIMalloc2(256 * 256 * 256, sizeof(GUInt32));
            }

            GDALColorTable* poCT = new GDALColorTable();
            GDALComputeMedianCutPCTInternal( poMEM_RGB_DS->GetRasterBand(1),
                                             poMEM_RGB_DS->GetRasterBand(2),
                                             poMEM_RGB_DS->GetRasterBand(3),
                                             /*NULL, NULL, NULL,*/
                                             m_pabyCachedTiles,
                                             m_pabyCachedTiles + nBlockXSize * nBlockYSize,
                                             m_pabyCachedTiles + 2 * nBlockXSize * nBlockYSize,
                                             NULL,
                                             256, /* max colors */
                                             8, /* bit depth */
                                             (GUInt32*)m_pabyHugeColorArray, /* preallocated histogram */
                                             poCT,
                                             NULL, NULL );

            GDALDitherRGB2PCTInternal( poMEM_RGB_DS->GetRasterBand(1),
                                       poMEM_RGB_DS->GetRasterBand(2),
                                       poMEM_RGB_DS->GetRasterBand(3),
                                       poMEMDS->GetRasterBand(1),
                                       poCT,
                                       8, /* bit depth */
                                       (GInt16*)m_pabyHugeColorArray, /* pasDynamicColorMap */
                                       m_bDither,
                                       NULL, NULL );
            poMEMDS->GetRasterBand(1)->SetColorTable(poCT);
            delete poCT;
            GDALClose( poMEM_RGB_DS );
        }
        else if( nBands == 1 && m_poCT != NULL && nTileBands > 1 )
        {
            GByte abyCT[4*256];
            int nEntries = MIN(256, m_poCT->GetColorEntryCount());
            for(i=0; i<nEntries; i++)
            {
                const GDALColorEntry* psEntry = m_poCT->GetColorEntry(i);
                abyCT[4*i] = (GByte)psEntry->c1;
                abyCT[4*i+1] = (GByte)psEntry->c2;
                abyCT[4*i+2] = (GByte)psEntry->c3;
                abyCT[4*i+3] = (GByte)psEntry->c4;
            }
            for(; i<256; i++)
            {
                abyCT[4*i] = 0;
                abyCT[4*i+1] = 0;
                abyCT[4*i+2] = 0;
                abyCT[4*i+3] = 0;
            }
            if( iYOff > 0 )
            {
                memset(m_pabyCachedTiles + 0 * nBlockXSize * nBlockYSize, 0, nBlockXSize * iYOff);
                memset(m_pabyCachedTiles + 1 * nBlockXSize * nBlockYSize, 0, nBlockXSize * iYOff);
                memset(m_pabyCachedTiles + 2 * nBlockXSize * nBlockYSize, 0, nBlockXSize * iYOff);
                memset(m_pabyCachedTiles + 3 * nBlockXSize * nBlockYSize, 0, nBlockXSize * iYOff);
            }
            for(int iY = iYOff; iY < iYOff + iYCount; iY ++)
            {
                if( iXOff > 0 )
                {
                    i = iY * nBlockXSize;
                    memset(m_pabyCachedTiles + 0 * nBlockXSize * nBlockYSize + i, 0, iXOff);
                    memset(m_pabyCachedTiles + 1 * nBlockXSize * nBlockYSize + i, 0, iXOff);
                    memset(m_pabyCachedTiles + 2 * nBlockXSize * nBlockYSize + i, 0, iXOff);
                    memset(m_pabyCachedTiles + 3 * nBlockXSize * nBlockYSize + i, 0, iXOff);
                }
                for(int iX = iXOff; iX < iXOff + iXCount; iX ++)
                {
                    i = iY * nBlockXSize + iX;
                    GByte byVal = m_pabyCachedTiles[i];
                    m_pabyCachedTiles[i] = abyCT[4*byVal];
                    m_pabyCachedTiles[i + 1 * nBlockXSize * nBlockYSize] = abyCT[4*byVal+1];
                    m_pabyCachedTiles[i + 2 * nBlockXSize * nBlockYSize] = abyCT[4*byVal+2];
                    m_pabyCachedTiles[i + 3 * nBlockXSize * nBlockYSize] = abyCT[4*byVal+3];
                }
                if( iXOff + iXCount < nBlockXSize )
                {
                    i = iY * nBlockXSize + iXOff + iXCount;
                    memset(m_pabyCachedTiles + 0 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize - (iXOff + iXCount));
                    memset(m_pabyCachedTiles + 1 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize - (iXOff + iXCount));
                    memset(m_pabyCachedTiles + 2 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize - (iXOff + iXCount));
                    memset(m_pabyCachedTiles + 3 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize - (iXOff + iXCount));
                }
            }
            if( iYOff + iYCount < nBlockYSize )
            {
                i = (iYOff + iYCount) * nBlockXSize;
                memset(m_pabyCachedTiles + 0 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize * (nBlockYSize - (iYOff + iYCount)));
                memset(m_pabyCachedTiles + 1 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize * (nBlockYSize - (iYOff + iYCount)));
                memset(m_pabyCachedTiles + 2 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize * (nBlockYSize - (iYOff + iYCount)));
                memset(m_pabyCachedTiles + 3 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize * (nBlockYSize - (iYOff + iYCount)));
            }
        }

        char** papszDriverOptions = CSLSetNameValue(NULL, "_INTERNAL_DATASET", "YES");
        if( EQUAL(pszDriverName, "JPEG") || EQUAL(pszDriverName, "WEBP") )
        {
            papszDriverOptions = CSLSetNameValue(
                                     papszDriverOptions, "QUALITY", CPLSPrintf("%d", m_nQuality));
        }
        else if( EQUAL(pszDriverName, "PNG") )
        {
            papszDriverOptions = CSLSetNameValue(
                                     papszDriverOptions, "ZLEVEL", CPLSPrintf("%d", m_nZLevel));
        }
#ifdef DEBUG
        VSIStatBufL sStat;
        CPLAssert(VSIStatL(osMemFileName, &sStat) != 0);
#endif
        GDALDataset* poOutDS = l_poDriver->CreateCopy(osMemFileName, poMEMDS,
                               FALSE, papszDriverOptions, NULL, NULL);
        CSLDestroy( papszDriverOptions );
        if( poOutDS )
        {
            GDALClose( poOutDS );
            vsi_l_offset nBlobSize;
            GByte* pabyBlob = VSIGetMemFileBuffer(osMemFileName, &nBlobSize, TRUE);

            /* Create or commit and recreate transaction */
            OGRDB2DataSource* poMainDS = m_poParentDS ? m_poParentDS : this;
            if( poMainDS->m_nTileInsertionCount == 0 )
            {
                poMainDS->SoftStartTransaction();
            }
            else if( poMainDS->m_nTileInsertionCount == 1000 )
            {
                poMainDS->SoftCommitTransaction();
                poMainDS->SoftStartTransaction();
                poMainDS->m_nTileInsertionCount = 0;
            }
            poMainDS->m_nTileInsertionCount ++;

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
            OGRDB2Statement oStatement( GetSession() );
            oStatement.Appendf( "INSERT INTO %s "
                                "(zoom_level, tile_row, tile_column, "
                                " tile_data) VALUES (%d, %d, %d, ?)",
                                m_osRasterTable.c_str(),
                                m_nZoomLevel, nRow, nCol);

            CPLDebug("OGRDB2DataSource::WriteTileInternal",
                     "stmt: '%s'", oStatement.GetCommand());
            CPL_SQLLEN  nBlobLen = (CPL_SQLLEN) nBlobSize;
            int nRetCode = SQLBindParameter(oStatement.GetStatement(),
                                            (SQLUSMALLINT)1,
                                            SQL_PARAM_INPUT,
                                            SQL_C_BINARY,
                                            SQL_LONGVARBINARY,
                                            MAXBLOB, 0,
                                            (SQLPOINTER)pabyBlob,
                                            MAXBLOB, &nBlobLen);

            char* pszBytes = GByteArrayToHexString( pabyBlob, 100);
            CPLDebug("OGRDB2DataSource::WriteTileInternal",
                     "nBlobSize: %d; pabyBlob: %s",
                     (int) nBlobSize, pszBytes);
            CPLFree(pszBytes);
            if ( !(nRetCode == SQL_SUCCESS
                    || nRetCode == SQL_SUCCESS_WITH_INFO) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failure binding BLOB value,error: '%s'",
                         GetSession()->GetLastError());
                CPLFree(pabyBlob);
                return CE_Failure;
            }

            if( !oStatement.ExecuteSQL() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failure when inserting tile (row=%d,col=%d) "
                         "at zoom_level=%d : %s",
                         nRow, nCol, m_nZoomLevel,
                         GetSession()->GetLastError());
                CPLDebug("OGRDB2DataSource::WriteTileInternal",
                         " insert failed; '%s'",
                         GetSession()->GetLastError() );
                return CE_Failure;
            }
        }
        eErr = CE_None;
        VSIUnlink(osMemFileName);
        delete poMEMDS;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot find driver %s", pszDriverName);
    }
    CPLDebug("OGRDB2DataSource::WriteTileInternal","exit; eErr: %d", eErr);
    return eErr;
}

/************************************************************************/
/*                     FlushRemainingShiftedTiles()                     */
/************************************************************************/

CPLErr OGRDB2DataSource::FlushRemainingShiftedTiles()
{
    CPLDebug("OGRDB2DataSource::FlushRemainingShiftedTiles","NO-OP");
#ifdef LATER
    if( m_hTempDB == NULL )
        return CE_None;

    for(int i=0; i<=3; i++)
    {
        m_asCachedTilesDesc[i].nRow = -1;
        m_asCachedTilesDesc[i].nCol = -1;
        m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
    }

    int nBlockXSize, nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

    CPLString osSQL = "SELECT tile_row, tile_column, partial_flag";
    for(int nBand = 1; nBand <= nBands; nBand++ )
    {
        osSQL += CPLSPrintf(", tile_data_band_%d", nBand);
    }
    osSQL += CPLSPrintf(" FROM partial_tiles WHERE "
                        "zoom_level = %d AND partial_flag != 0",
                        m_nZoomLevel);
    const char* pszSQL = osSQL.c_str();

#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "%s", pszSQL);
#endif
    sqlite3_stmt* hStmt = NULL;
    int rc = sqlite3_prepare_v2(m_hTempDB, pszSQL, strlen(pszSQL), &hStmt, NULL);
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_prepare_v2(%s) failed: %s",
                  pszSQL, sqlite3_errmsg( m_hTempDB ) );
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    int bGotPartialTiles = FALSE;
    do
    {
        int rc = sqlite3_step(hStmt);
        if ( rc == SQLITE_ROW )
        {
            bGotPartialTiles = TRUE;

            int nRow = sqlite3_column_int(hStmt, 0);
            int nCol = sqlite3_column_int(hStmt, 1);
            int nPartialFlags = sqlite3_column_int(hStmt, 2);
            for( int nBand = 1; nBand <= nBands; nBand++ )
            {
                if( nPartialFlags & (((1 << 4)-1) << (4*(nBand - 1))) )
                {
                    CPLAssert( sqlite3_column_bytes(hStmt, 2 + nBand) == nBlockXSize * nBlockYSize );
                    memcpy( m_pabyCachedTiles + (nBand-1) * nBlockXSize * nBlockYSize,
                            sqlite3_column_blob(hStmt, 2 + nBand),
                            nBlockXSize * nBlockYSize );
                }
                else
                {
                    memset( m_pabyCachedTiles + (nBand-1) * nBlockXSize * nBlockYSize,
                            0,
                            nBlockXSize * nBlockYSize );
                }
            }

            int nFullFlags = (1 << (4 * nBands)) - 1;

            // In case the partial flags indicate that there's some quadrant
            // missing, check in the main database if there is already a tile
            // In which case, use the parts of that tile that aren't in the
            // temporary database
            if( nPartialFlags != nFullFlags )
            {
                char* pszNewSQL = sqlite3_mprintf("SELECT tile_data FROM '%q' "
                                                  "WHERE zoom_level = %d AND tile_row = %d AND tile_column = %d%s",
                                                  m_osRasterTable.c_str(), m_nZoomLevel, nRow, nCol,
                                                  !m_osWHERE.empty() ? CPLSPrintf(" AND (%s)", m_osWHERE.c_str()): "");
#ifdef DEBUG_VERBOSE
                CPLDebug("GPKG", "%s", pszNewSQL);
#endif
                sqlite3_stmt* hNewStmt = NULL;
                rc = sqlite3_prepare_v2(GetDB(), pszNewSQL, -1, &hNewStmt, NULL);
                if ( rc == SQLITE_OK )
                {
                    rc = sqlite3_step( hNewStmt );
                    if( rc == SQLITE_ROW && sqlite3_column_type( hNewStmt, 0 ) == SQLITE_BLOB )
                    {
                        const int nBytes = sqlite3_column_bytes( hNewStmt, 0 );
                        GByte* pabyRawData = (GByte*)sqlite3_column_blob( hNewStmt, 0 );
                        CPLString osMemFileName;
                        osMemFileName.Printf("/vsimem/gpkg_read_tile_%p", this);
                        VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyRawData,
                                                              nBytes, FALSE);
                        VSIFCloseL(fp);

                        int bIsLossyFormat;
                        ReadTile(osMemFileName,
                                 m_pabyCachedTiles + 4 * nBlockXSize * nBlockYSize,
                                 &bIsLossyFormat);
                        VSIUnlink(osMemFileName);

                        int iYQuadrantMax = ( m_nShiftYPixelsMod ) ? 1 : 0;
                        int iXQuadrantMax = ( m_nShiftXPixelsMod ) ? 1 : 0;
                        for( int iYQuadrant = 0; iYQuadrant <= iYQuadrantMax; iYQuadrant ++ )
                        {
                            for( int iXQuadrant = 0; iXQuadrant <= iXQuadrantMax; iXQuadrant ++ )
                            {
                                for( int nBand = 1; nBand <= nBands; nBand ++ )
                                {
                                    int iQuadrantFlag = 0;
                                    if( iXQuadrant == 0 && iYQuadrant == 0 )
                                        iQuadrantFlag |= (1 << 0);
                                    if( iXQuadrant == iXQuadrantMax && iYQuadrant == 0  )
                                        iQuadrantFlag |= (1 << 1);
                                    if( iXQuadrant == 0 && iYQuadrant == iYQuadrantMax )
                                        iQuadrantFlag |= (1 << 2);
                                    if( iXQuadrant == iXQuadrantMax && iYQuadrant == iYQuadrantMax )
                                        iQuadrantFlag |= (1 << 3);
                                    int nLocalFlag = iQuadrantFlag << (4 * (nBand - 1));
                                    if( !(nPartialFlags & nLocalFlag) )
                                    {
                                        int nXOff, nYOff, nXSize, nYSize;
                                        if( iXQuadrant == 0 && m_nShiftXPixelsMod != 0 )
                                        {
                                            nXOff = 0;
                                            nXSize = m_nShiftXPixelsMod;
                                        }
                                        else
                                        {
                                            nXOff = m_nShiftXPixelsMod;
                                            nXSize = nBlockXSize - m_nShiftXPixelsMod;
                                        }
                                        if( iYQuadrant == 0 && m_nShiftYPixelsMod != 0 )
                                        {
                                            nYOff = 0;
                                            nYSize = m_nShiftYPixelsMod;
                                        }
                                        else
                                        {
                                            nYOff = m_nShiftYPixelsMod;
                                            nYSize = nBlockYSize - m_nShiftYPixelsMod;
                                        }
                                        for( int iY = nYOff; iY < nYOff + nYSize; iY ++ )
                                        {
                                            memcpy( m_pabyCachedTiles + ((nBand - 1) * nBlockYSize + iY) * nBlockXSize + nXOff,
                                                    m_pabyCachedTiles + ((4 + nBand - 1) * nBlockYSize + iY) * nBlockXSize + nXOff,
                                                    nXSize );
                                        }
                                    }
                                }
                            }
                        }
                    }
                    sqlite3_finalize(hNewStmt);
                }
                sqlite3_free(pszNewSQL);
            }

            m_asCachedTilesDesc[0].nRow = nRow;
            m_asCachedTilesDesc[0].nCol = nCol;
            m_asCachedTilesDesc[0].nIdxWithinTileData = 0;
            m_asCachedTilesDesc[0].abBandDirty[0] = TRUE;
            m_asCachedTilesDesc[0].abBandDirty[1] = TRUE;
            m_asCachedTilesDesc[0].abBandDirty[2] = TRUE;
            m_asCachedTilesDesc[0].abBandDirty[3] = TRUE;

            eErr = WriteTile();
        }
        else
            break;
    }
    while( eErr == CE_None);

    sqlite3_finalize(hStmt);

    if( bGotPartialTiles )
    {
        pszSQL = CPLSPrintf("UPDATE partial_tiles SET zoom_level = %d, "
                            "partial_flag = 0 WHERE zoom_level = %d AND partial_flag != 0",
                            -1-m_nZoomLevel, m_nZoomLevel);
#ifdef DEBUG_VERBOSE
        CPLDebug("GPKG", "%s", pszSQL);
#endif
        SQLCommand(m_hTempDB, pszSQL);
    }

    return eErr;
#endif
    return CE_Failure;
}

/************************************************************************/
/*                         WriteShiftedTile()                           */
/************************************************************************/

CPLErr OGRDB2DataSource::WriteShiftedTile(int /*nRow*/, int /*nCol*/, int /*nBand*/,
        int /*nDstXOffset*/, int /*nDstYOffset*/,
        int /*nDstXSize*/, int /*nDstYSize*/)
{
#ifdef LATER
    CPLAssert( m_nShiftXPixelsMod || m_nShiftYPixelsMod );
    CPLAssert( nRow >= 0 );
    CPLAssert( nCol >= 0 );
    CPLAssert( nRow < m_nTileMatrixHeight );
    CPLAssert( nCol < m_nTileMatrixWidth );

    if( m_hTempDB == NULL &&
            (m_poParentDS == NULL || m_poParentDS->m_hTempDB == NULL) )
    {
        const char* pszBaseFilename = m_poParentDS ?
                                      m_poParentDS->m_pszFilename : m_pszFilename;
        m_osTempDBFilename = CPLResetExtension(pszBaseFilename, "gpkg.tmp");
        CPLPushErrorHandler(CPLQuietErrorHandler);
        VSIUnlink(m_osTempDBFilename);
        CPLPopErrorHandler();
        m_hTempDB = NULL;
        sqlite3_open(m_osTempDBFilename, &m_hTempDB);
        if( m_hTempDB == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create temporary database %s",
                     m_osTempDBFilename.c_str());
            return CE_Failure;
        }
        SQLCommand(m_hTempDB, "PRAGMA synchronous = OFF");
        SQLCommand(m_hTempDB, "PRAGMA journal_mode = OFF");
        SQLCommand(m_hTempDB, "CREATE TABLE partial_tiles("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "zoom_level INTEGER NOT NULL,"
                   "tile_column INTEGER NOT NULL,"
                   "tile_row INTEGER NOT NULL,"
                   "tile_data_band_1 BLOB,"
                   "tile_data_band_2 BLOB,"
                   "tile_data_band_3 BLOB,"
                   "tile_data_band_4 BLOB,"
                   "partial_flag INTEGER NOT NULL,"
                   "UNIQUE (zoom_level, tile_column, tile_row))" );
        SQLCommand(m_hTempDB, "CREATE INDEX partial_tiles_partial_flag_idx "
                   "ON partial_tiles(partial_flag)");

        if( m_poParentDS != NULL )
        {
            m_poParentDS->m_osTempDBFilename = m_osTempDBFilename;
            m_poParentDS->m_hTempDB = m_hTempDB;
        }
    }
    if( m_poParentDS != NULL )
        m_hTempDB = m_poParentDS->m_hTempDB;

    int nBlockXSize, nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

    int iQuadrantFlag = 0;
    if( nDstXOffset == 0 && nDstYOffset == 0 )
        iQuadrantFlag |= (1 << 0);
    if( nDstXOffset + nDstXSize == nBlockXSize && nDstYOffset == 0  )
        iQuadrantFlag |= (1 << 1);
    if( nDstXOffset == 0 && nDstYOffset + nDstYSize == nBlockYSize )
        iQuadrantFlag |= (1 << 2);
    if( nDstXOffset + nDstXSize == nBlockXSize && nDstYOffset + nDstYSize == nBlockYSize )
        iQuadrantFlag |= (1 << 3);
    int nFlags = iQuadrantFlag << (4 * (nBand - 1));
    int nFullFlags = (1 << (4 * nBands)) - 1;
    int nOldFlags = 0;

    for(int i=1; i<=3; i++)
    {
        m_asCachedTilesDesc[i].nRow = -1;
        m_asCachedTilesDesc[i].nCol = -1;
        m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
    }

    int nExistingId = 0;
    const char* pszSQL = CPLSPrintf("SELECT id, partial_flag, tile_data_band_%d FROM partial_tiles WHERE "
                                    "zoom_level = %d AND tile_row = %d AND tile_column = %d",
                                    nBand, m_nZoomLevel, nRow, nCol);
#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "%s", pszSQL);
#endif
    sqlite3_stmt* hStmt = NULL;
    int rc = sqlite3_prepare_v2(m_hTempDB, pszSQL, strlen(pszSQL), &hStmt, NULL);
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_prepare_v2(%s) failed: %s",
                  pszSQL, sqlite3_errmsg( m_hTempDB ) );
        return CE_Failure;
    }

    rc = sqlite3_step(hStmt);
    if ( rc == SQLITE_ROW )
    {
        nExistingId = sqlite3_column_int(hStmt, 0);
#ifdef DEBUG_VERBOSE
        CPLDebug("GPKG", "Using partial_tile id=%d", nExistingId);
#endif
        nOldFlags = sqlite3_column_int(hStmt, 1);
        CPLAssert(nOldFlags != 0);
        if( (nOldFlags & (((1 << 4)-1) << (4*(nBand - 1)))) == 0 )
        {
            memset( m_pabyCachedTiles + (4 + nBand - 1) * nBlockXSize * nBlockYSize,
                    0,
                    nBlockXSize * nBlockYSize );
        }
        else
        {
            CPLAssert( sqlite3_column_bytes(hStmt, 2) == nBlockXSize * nBlockYSize );
            memcpy( m_pabyCachedTiles + (4 + nBand - 1) * nBlockXSize * nBlockYSize,
                    sqlite3_column_blob(hStmt, 2),
                    nBlockXSize * nBlockYSize );
        }
    }
    else
    {
        memset( m_pabyCachedTiles + (4 + nBand - 1) * nBlockXSize * nBlockYSize,
                0,
                nBlockXSize * nBlockYSize );
    }
    sqlite3_finalize(hStmt);
    hStmt = NULL;

    /* Copy the updated rectangle into the full tile */
    for(int iY = nDstYOffset; iY < nDstYOffset + nDstYSize; iY ++ )
    {
        memcpy( m_pabyCachedTiles + (4 + nBand - 1) * nBlockXSize * nBlockYSize +
                iY * nBlockXSize + nDstXOffset,
                m_pabyCachedTiles + (nBand - 1) * nBlockXSize * nBlockYSize +
                iY * nBlockXSize + nDstXOffset,
                nDstXSize );
    }

#ifdef notdef
    static int nCounter = 1;
    GDALDataset* poLogDS = ((GDALDriver*)GDALGetDriverByName("GTiff"))->Create(
                               CPLSPrintf("/tmp/partial_band_%d_%d.tif", 1, nCounter++),
                               nBlockXSize, nBlockYSize, nBands, GDT_Byte, NULL);
    poLogDS->RasterIO(GF_Write, 0, 0, nBlockXSize, nBlockYSize,
                      m_pabyCachedTiles + (4 + nBand - 1) * nBlockXSize * nBlockYSize,
                      nBlockXSize, nBlockYSize,
                      GDT_Byte,
                      1, NULL,
                      0, 0, 0, NULL);
    GDALClose(poLogDS);
#endif

    if( (nOldFlags & nFlags) != 0 )
    {
        CPLDebug("GPKG",
                 "Rewriting quadrant %d of band %d of tile (row=%d,col=%d)",
                 iQuadrantFlag, nBand, nRow, nCol);
    }

    nFlags |= nOldFlags;
    if( nFlags == nFullFlags )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("GPKG", "Got all quadrants for that tile");
#endif
        for( int iBand = 1; iBand <= nBands; iBand ++ )
        {
            if( iBand != nBand && nExistingId )
            {
                pszSQL = CPLSPrintf("SELECT tile_data_band_%d FROM partial_tiles WHERE "
                                    "id = %d", iBand, nExistingId);
#ifdef DEBUG_VERBOSE
                CPLDebug("GPKG", "%s", pszSQL);
#endif
                hStmt = NULL;
                rc = sqlite3_prepare_v2(m_hTempDB, pszSQL, strlen(pszSQL), &hStmt, NULL);
                if ( rc != SQLITE_OK )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_prepare_v2(%s) failed: %s",
                              pszSQL, sqlite3_errmsg( m_hTempDB ) );
                    return CE_Failure;
                }

                rc = sqlite3_step(hStmt);
                if ( rc == SQLITE_ROW )
                {
                    CPLAssert( sqlite3_column_bytes(hStmt, 0) == nBlockXSize * nBlockYSize );
                    memcpy( m_pabyCachedTiles + (iBand - 1) * nBlockXSize * nBlockYSize,
                            sqlite3_column_blob(hStmt, 0),
                            nBlockXSize * nBlockYSize );
                }
                sqlite3_finalize(hStmt);
                hStmt = NULL;
            }
            else
            {
                memcpy( m_pabyCachedTiles + (iBand - 1) * nBlockXSize * nBlockYSize,
                        m_pabyCachedTiles + (4 + iBand - 1) * nBlockXSize * nBlockYSize,
                        nBlockXSize * nBlockYSize );
            }
        }

        m_asCachedTilesDesc[0].nRow = nRow;
        m_asCachedTilesDesc[0].nCol = nCol;
        m_asCachedTilesDesc[0].nIdxWithinTileData = 0;
        m_asCachedTilesDesc[0].abBandDirty[0] = TRUE;
        m_asCachedTilesDesc[0].abBandDirty[1] = TRUE;
        m_asCachedTilesDesc[0].abBandDirty[2] = TRUE;
        m_asCachedTilesDesc[0].abBandDirty[3] = TRUE;

        pszSQL = CPLSPrintf("UPDATE partial_tiles SET zoom_level = %d, "
                            "partial_flag = 0 WHERE id = %d",
                            -1-m_nZoomLevel, nExistingId);
        SQLCommand(m_hTempDB, pszSQL);
#ifdef DEBUG_VERBOSE
        CPLDebug("GPKG", "%s", pszSQL);
#endif
        return WriteTile();
    }

    if( nExistingId == 0 )
    {
        OGRErr err;
        pszSQL = CPLSPrintf("SELECT id FROM partial_tiles WHERE "
                            "partial_flag = 0 AND zoom_level = %d "
                            "AND tile_column = %d AND tile_row = %d",
                            -1-m_nZoomLevel, nRow, nCol);
#ifdef DEBUG_VERBOSE
        CPLDebug("GPKG", "%s", pszSQL);
#endif
        nExistingId = SQLGetInteger(m_hTempDB, pszSQL, &err);
        if( nExistingId == 0 )
        {
            pszSQL = "SELECT id FROM partial_tiles WHERE partial_flag = 0 LIMIT 1";
#ifdef DEBUG_VERBOSE
            CPLDebug("GPKG", "%s", pszSQL);
#endif
            nExistingId = SQLGetInteger(m_hTempDB, pszSQL, &err);
        }
    }

    if( nExistingId == 0 )
    {
        pszSQL = CPLSPrintf("INSERT INTO partial_tiles "
                            "(zoom_level, tile_row, tile_column, tile_data_band_%d, partial_flag) VALUES (%d, %d, %d, ?, %d)",
                            nBand, m_nZoomLevel, nRow, nCol, nFlags);
    }
    else
    {
        pszSQL = CPLSPrintf("UPDATE partial_tiles SET zoom_level = %d, "
                            "tile_row = %d, tile_column = %d, "
                            "tile_data_band_%d = ?, partial_flag = %d WHERE id = %d",
                            m_nZoomLevel, nRow, nCol, nBand, nFlags, nExistingId);
    }
#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "%s", pszSQL);
#endif

    hStmt = NULL;
    rc = sqlite3_prepare_v2(m_hTempDB, pszSQL, -1, &hStmt, NULL);
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "failed to prepare SQL %s: %s",
                  pszSQL, sqlite3_errmsg(m_hTempDB) );
        return CE_Failure;
    }

    sqlite3_bind_blob( hStmt, 1,
                       m_pabyCachedTiles + (4 + nBand - 1) * nBlockXSize * nBlockYSize,
                       nBlockXSize * nBlockYSize,
                       SQLITE_TRANSIENT );
    rc = sqlite3_step( hStmt );
    CPLErr eErr = CE_Failure;
    if( rc == SQLITE_DONE )
        eErr = CE_None;
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failure when inserting partial tile (row=%d,col=%d) at zoom_level=%d : %s",
                 nRow, nCol, m_nZoomLevel, sqlite3_errmsg(m_hTempDB));
    }

    sqlite3_finalize(hStmt);

    return eErr;
#endif
    return CE_Failure;
}

/************************************************************************/
/*                         IWriteBlock()                                */
/************************************************************************/

CPLErr GDALDB2RasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                      void* pData)
{
    CPLDebug("GDALDB2RasterBand::IWriteBlock", "IWriteBlock(nBand=%d,nBlockXOff=%d,nBlockYOff=%d",
             nBand,nBlockXOff,nBlockYOff);

    OGRDB2DataSource* poGDS = (OGRDB2DataSource* )poDS;
    if( !poGDS->m_bUpdate )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "IWriteBlock() not supported on dataset opened in read-only mode");
        return CE_Failure;
    }

    if( !poGDS->m_bGeoTransformValid || poGDS->m_nSRID == UNKNOWN_SRID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "IWriteBlock() not supported if georeferencing not set");
        return CE_Failure;
    }

    int nRow = nBlockYOff + poGDS->m_nShiftYTiles;
    int nCol = nBlockXOff + poGDS->m_nShiftXTiles;

    int nRowMin = nRow;
    int nRowMax = nRowMin;
    if( poGDS->m_nShiftYPixelsMod )
        nRowMax ++;

    int nColMin = nCol;
    int nColMax = nColMin;
    if( poGDS->m_nShiftXPixelsMod )
        nColMax ++;

    CPLErr eErr = CE_None;

    for(nRow = nRowMin; eErr == CE_None && nRow <= nRowMax; nRow ++)
    {
        for(nCol = nColMin; eErr == CE_None && nCol <= nColMax; nCol++ )
        {
            CPLDebug("GDALDB2RasterBand::IWriteBlock1",
                    "nRow: %d,nCol: %d, height: %d, width: %d",
                    nRow,nCol,
                    poGDS->m_nTileMatrixHeight,poGDS->m_nTileMatrixWidth);
             if( nRow < 0 || nCol < 0 || nRow >= poGDS->m_nTileMatrixHeight ||
                    nCol >= poGDS->m_nTileMatrixWidth )
            {
                continue;
            }

            if( poGDS->m_nShiftXPixelsMod == 0
                && poGDS->m_nShiftYPixelsMod == 0 )
            {
                if( !(nRow == poGDS->m_asCachedTilesDesc[0].nRow &&
                        nCol == poGDS->m_asCachedTilesDesc[0].nCol &&
                        poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData == 0) )
                {
                    eErr = poGDS->WriteTile();

                    poGDS->m_asCachedTilesDesc[0].nRow = nRow;
                    poGDS->m_asCachedTilesDesc[0].nCol = nCol;
                    poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData = 0;
                }
            }

            // Composite block data into tile, and check if all bands for this block
            // are dirty, and if so write the tile
            int bAllDirty = TRUE;
            for(int iBand=1; iBand<=poGDS->nBands; iBand++)
            {
                GDALRasterBlock* poBlock = NULL;
                GByte* pabySrc;
                if( iBand == nBand )
                {
                    pabySrc = (GByte*)pData;
                }
                else
                {
                    if( !(poGDS->m_nShiftXPixelsMod == 0 && poGDS->m_nShiftYPixelsMod == 0) )
                        continue;

                    // If the block for this band is not dirty, it might be dirty in cache
                    if( poGDS->m_asCachedTilesDesc[0].abBandDirty[iBand-1] )
                        continue;
                    else
                    {
                        poBlock =
                            ((GDALDB2RasterBand*)poGDS->GetRasterBand(iBand))->
                            TryGetLockedBlockRef(nBlockXOff, nBlockYOff);
                        if( poBlock && poBlock->GetDirty() )
                        {
                            pabySrc = (GByte*)poBlock->GetDataRef(),
                            poBlock->MarkClean();
                        }
                        else
                        {
                            if( poBlock )
                                poBlock->DropLock();
                            bAllDirty = FALSE;
                            continue;
                        }
                    }
                }

                if( poGDS->m_nShiftXPixelsMod == 0 && poGDS->m_nShiftYPixelsMod == 0 )
                    poGDS->m_asCachedTilesDesc[0].abBandDirty[iBand - 1] = TRUE;

                int nDstXOffset = 0;
                int nDstXSize = nBlockXSize;
                int nDstYOffset = 0;
                int nDstYSize = nBlockYSize;
                int nSrcXOffset = 0;
                int nSrcYOffset = 0;
                // Composite block data into tile data
                if( poGDS->m_nShiftXPixelsMod == 0 && poGDS->m_nShiftYPixelsMod == 0 )
                {
                    memcpy( poGDS->m_pabyCachedTiles + (iBand - 1) * nBlockXSize * nBlockYSize,
                            pabySrc, nBlockXSize * nBlockYSize );
                }
                else
                {
                    if( nCol == nColMin )
                    {
                        nDstXOffset = poGDS->m_nShiftXPixelsMod;
                        nDstXSize = nBlockXSize - poGDS->m_nShiftXPixelsMod;
                        nSrcXOffset = 0;
                    }
                    else
                    {
                        nDstXOffset = 0;
                        nDstXSize = poGDS->m_nShiftXPixelsMod;
                        nSrcXOffset = nBlockXSize - poGDS->m_nShiftXPixelsMod;
                    }
                    if( nRow == nRowMin )
                    {
                        nDstYOffset = poGDS->m_nShiftYPixelsMod;
                        nDstYSize = nBlockYSize - poGDS->m_nShiftYPixelsMod;
                        nSrcYOffset = 0;
                    }
                    else
                    {
                        nDstYOffset = 0;
                        nDstYSize = poGDS->m_nShiftYPixelsMod;
                        nSrcYOffset = nBlockYSize - poGDS->m_nShiftYPixelsMod;
                    }
                    CPLDebug("GDALDB2RasterBand::IWriteBlock", "Copy source tile x=%d,w=%d,y=%d,h=%d into buffet at x=%d,y=%d",
                             nDstXOffset, nDstXSize, nDstYOffset, nDstYSize, nSrcXOffset, nSrcYOffset);
                    for( int y=0; y<nDstYSize; y++ )
                    {
                        GByte* pDst = poGDS->m_pabyCachedTiles + (iBand - 1) * nBlockXSize * nBlockYSize +
                                      (y + nDstYOffset) * nBlockXSize + nDstXOffset;
                        GByte* pSrc = pabySrc + (y + nSrcYOffset) * nBlockXSize + nSrcXOffset;
                        GDALCopyWords(pSrc, GDT_Byte, 1,
                                      pDst, GDT_Byte, 1,
                                      nDstXSize);
                    }
                }

                if( poBlock )
                    poBlock->DropLock();

                if( !(poGDS->m_nShiftXPixelsMod == 0 && poGDS->m_nShiftYPixelsMod == 0) )
                {
                    poGDS->m_asCachedTilesDesc[0].nRow = -1;
                    poGDS->m_asCachedTilesDesc[0].nCol = -1;
                    poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData = -1;
                    eErr = poGDS->WriteShiftedTile(nRow, nCol, iBand,
                                                   nDstXOffset, nDstYOffset,
                                                   nDstXSize, nDstYSize);
                }
            }

            if( poGDS->m_nShiftXPixelsMod == 0 && poGDS->m_nShiftYPixelsMod == 0 )
            {
                if( bAllDirty )
                {
                    eErr = poGDS->WriteTile();
                }
            }
        }
    }
    CPLDebug("GDALDB2RasterBand::IWriteBlock", "Exit, eErr: %d", eErr);

    return eErr;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int GDALDB2RasterBand::GetOverviewCount()
{
    OGRDB2DataSource* poGDS = (OGRDB2DataSource* )poDS;
    return poGDS->m_nOverviewCount;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

GDALRasterBand* GDALDB2RasterBand::GetOverview(int nIdx)
{
    OGRDB2DataSource* poGDS = (OGRDB2DataSource* )poDS;
    if( nIdx < 0 || nIdx >= poGDS->m_nOverviewCount )
        return NULL;
    return poGDS->m_papoOverviewDS[nIdx]->GetRasterBand(nBand);
}

/************************************************************************/
/*                        GByteArrayToHexString()                       */
/************************************************************************/

static char* GByteArrayToHexString( const GByte* pabyData, int nLen)
{
    char* pszTextBuf;
    const size_t nBufLen = nLen*2+3;

    pszTextBuf = (char *) CPLMalloc(nBufLen);

    int  iSrc, iDst=0;

    for( iSrc = 0; iSrc < nLen; iSrc++ )
    {
        if( iSrc == 0 )
        {
            snprintf( pszTextBuf+iDst, nBufLen - iDst, "0x%02x", pabyData[iSrc] );
            iDst += 4;
        }
        else
        {
            snprintf( pszTextBuf+iDst, nBufLen - iDst, "%02x", pabyData[iSrc] );
            iDst += 2;
        }
    }
    pszTextBuf[iDst] = 0;

    return pszTextBuf;
}
