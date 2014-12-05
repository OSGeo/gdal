/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements GDALGeoPackageRasterBand class
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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
 * DEALINGS IN THE SOFpszFileNameTWARE.
 ****************************************************************************/

#include "ogr_geopackage.h"
#include "memdataset.h"

//#define DEBUG_VERBOSE

/************************************************************************/
/*                      GDALGeoPackageRasterBand()                      */
/************************************************************************/

GDALGeoPackageRasterBand::GDALGeoPackageRasterBand(GDALGeoPackageDataset* poDS,
                                                   int nBand,
                                                   int nTileWidth, int nTileHeight)
{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_Byte;
    nBlockXSize = nTileWidth;
    nBlockYSize = nTileHeight;
}

/************************************************************************/
/*                              FlushCache()                            */
/************************************************************************/

CPLErr GDALGeoPackageRasterBand::FlushCache()
{
    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
    if( GDALPamRasterBand::FlushCache() != CE_None )
        return CE_Failure;
    return poGDS->WriteTile();
}

/************************************************************************/
/*                             GetColorTable()                          */
/************************************************************************/

GDALColorTable* GDALGeoPackageRasterBand::GetColorTable()
{
    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
    if( poGDS->nBands == 1 )
    {
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

            char* pszSQL = sqlite3_mprintf("SELECT tile_data FROM '%q' "
                "WHERE zoom_level = %d LIMIT 1",
                poGDS->m_osRasterTable.c_str(), poGDS->m_nZoomLevel);
            sqlite3_stmt* hStmt = NULL;
            int rc = sqlite3_prepare(poGDS->GetDB(), pszSQL, -1, &hStmt, NULL);
            if ( rc == SQLITE_OK )
            {
                rc = sqlite3_step( hStmt );
                if( rc == SQLITE_ROW && sqlite3_column_type( hStmt, 0 ) == SQLITE_BLOB )
                {
                    const int nBytes = sqlite3_column_bytes( hStmt, 0 );
                    GByte* pabyRawData = (GByte*)sqlite3_column_blob( hStmt, 0 );
                    CPLString osMemFileName;
                    osMemFileName.Printf("/vsimem/gpkg_read_tile_%p", this);
                    VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyRawData,
                                                            nBytes, FALSE);
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
            sqlite3_free(pszSQL);
            sqlite3_finalize(hStmt);
        }

        return poGDS->m_poCT;
    }
    else
        return NULL;
}

/************************************************************************/
/*                             SetColorTable()                          */
/************************************************************************/

CPLErr GDALGeoPackageRasterBand::SetColorTable(GDALColorTable* poCT)
{
    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
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

GDALColorInterp GDALGeoPackageRasterBand::GetColorInterpretation()
{
    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
    if( poGDS->nBands == 1 )
        return GetColorTable() ? GCI_PaletteIndex : GCI_GrayIndex;
    else
        return (GDALColorInterp) (GCI_RedBand + (nBand - 1));
}

/************************************************************************/
/*                        SetColorInterpretation()                      */
/************************************************************************/

CPLErr GDALGeoPackageRasterBand::SetColorInterpretation( GDALColorInterp eInterp )
{
    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
    if( eInterp == GCI_Undefined )
        return CE_None;
    if( (eInterp == GCI_GrayIndex || eInterp == GCI_PaletteIndex) && poGDS->nBands == 1 )
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
    for(int i=0;i<nEntries;i++)
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

CPLErr GDALGeoPackageDataset::ReadTile(const CPLString& osMemFileName,
                                       GByte* pabyTileData,
                                       int* pbIsLossyFormat)
{
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
        memset(pabyTileData, 0, 4 * nBlockXSize * nBlockYSize );
        return CE_Failure;
    }

    if( pbIsLossyFormat )
        *pbIsLossyFormat = !EQUAL(poDSTile->GetDriver()->GetDescription(), "PNG");
    int nTileBandCount = poDSTile->GetRasterCount();

    if( !(poDSTile->GetRasterXSize() == nBlockXSize &&
          poDSTile->GetRasterYSize() == nBlockYSize &&
          (nTileBandCount == 1 || nTileBandCount == 3 || nTileBandCount == 4)) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent tiles characteristics");
        GDALClose(poDSTile);
        memset(pabyTileData, 0, 4 * nBlockXSize * nBlockYSize );
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
        memset(pabyTileData, 0, 4 * nBlockXSize * nBlockYSize );
        return CE_Failure;
    }

    GDALColorTable* poCT = NULL;
    if( nBands == 1 || nTileBandCount == 1 )
    {
        poCT = poDSTile->GetRasterBand(1)->GetColorTable();
        GetRasterBand(1)->GetColorTable();
    }

    /* Map RGB(A) tile to single-band color indexed */
    if( nBands == 1 && m_poCT != NULL && nTileBandCount != 1 )
    {
        std::map< GUInt32, int > oMapEntryToIndex;
        int nEntries = MIN(256, m_poCT->GetColorEntryCount());
        for(int i=0;i<nEntries;i++)
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
        for(int i=0;i<nBlockXSize*nBlockYSize;i++)
        {
            GByte c1 = pabyTileData[i];
            GByte c2 = pabyTileData[i + nBlockXSize * nBlockYSize];
            GByte c3 = pabyTileData[i + 2 * nBlockXSize * nBlockYSize];
            GByte c4 = pabyTileData[i + 3 * nBlockXSize * nBlockYSize];
            GUInt32 nVal = c1 + (c2 << 8) + (c3 << 16);
            if( nTileBandCount == 4 ) nVal += (c4 << 24);
            if( nVal == 0 ) /* In most cases we will reach that point at partial tiles */
                pabyTileData[i] = iBestEntryFor0;
            else
            {
                std::map< GUInt32, int >::iterator oMapEntryToIndexIter = oMapEntryToIndex.find(nVal);
                if( oMapEntryToIndexIter == oMapEntryToIndex.end() )
                    /* Could happen with JPEG tiles */
                    pabyTileData[i] = GPKGFindBestEntry(m_poCT, c1, c2, c3, c4, nTileBandCount);
                else
                    pabyTileData[i] = oMapEntryToIndexIter->second;
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
    else if( (nBands == 1 && nTileBandCount != 1) ||
        (nBands == 1 && nTileBandCount == 1 && m_poCT != NULL && poCT == NULL) ||
        (nBands == 1 && nTileBandCount == 1 && m_poCT == NULL && poCT != NULL) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent dataset and tiles band characteristics");
    }

    if( nTileBandCount == 1 && !(nBands == 1 && m_poCT != NULL) )
    {
        /* Expand color indexed to RGB(A) */
        if( poCT != NULL )
        {
            int i;
            GByte abyCT[4*256];
            int nEntries = MIN(256, poCT->GetColorEntryCount());
            for(i=0;i<nEntries;i++)
            {
                const GDALColorEntry* psEntry = poCT->GetColorEntry(i);
                abyCT[4*i] = (GByte)psEntry->c1;
                abyCT[4*i+1] = (GByte)psEntry->c2;
                abyCT[4*i+2] = (GByte)psEntry->c3;
                abyCT[4*i+3] = (GByte)psEntry->c4;
            }
            for(;i<256;i++)
            {
                abyCT[4*i] = 0;
                abyCT[4*i+1] = 0;
                abyCT[4*i+2] = 0;
                abyCT[4*i+3] = 0;
            }
            for(i=0;i<nBlockXSize * nBlockYSize;i++)
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
            memset(pabyTileData + 3 * nBlockXSize * nBlockYSize,
                255, nBlockXSize * nBlockYSize);
        }
    }
    else if( nTileBandCount == 3 )
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

GByte* GDALGeoPackageDataset::ReadTile(int nRow, int nCol)
{
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

GByte* GDALGeoPackageDataset::ReadTile(int nRow, int nCol, GByte* pabyData,
                                       int* pbIsLossyFormat)
{
    int rc;
    int nBlockXSize, nBlockYSize;
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    
    if( pbIsLossyFormat ) *pbIsLossyFormat = FALSE;

    //CPLDebug("GPKG", "For block (blocky=%d, blockx=%d) request tile (row=%d, col=%d)",
    //         nBlockYOff, nBlockXOff, nRow, nCol);
    char* pszSQL = sqlite3_mprintf("SELECT tile_data FROM '%q' "
        "WHERE zoom_level = %d AND tile_row = %d AND tile_column = %d%s",
        m_osRasterTable.c_str(), m_nZoomLevel, nRow, nCol,
        m_osWHERE.size() ? CPLSPrintf(" AND (%s)", m_osWHERE.c_str()): "");
#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "%s", pszSQL);
#endif
    sqlite3_stmt* hStmt = NULL;
    rc = sqlite3_prepare(GetDB(), pszSQL, -1, &hStmt, NULL);
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s", pszSQL);
        sqlite3_free(pszSQL);
        return NULL;
    }
    sqlite3_free(pszSQL);
    rc = sqlite3_step( hStmt );

    if( rc == SQLITE_ROW && sqlite3_column_type( hStmt, 0 ) == SQLITE_BLOB )
    {
        const int nBytes = sqlite3_column_bytes( hStmt, 0 );
        GByte* pabyRawData = (GByte*)sqlite3_column_blob( hStmt, 0 );
        CPLString osMemFileName;
        osMemFileName.Printf("/vsimem/gpkg_read_tile_%p", this);
        VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyRawData,
                                                nBytes, FALSE);
        VSIFCloseL(fp);

        ReadTile(osMemFileName, pabyData, pbIsLossyFormat);
        VSIUnlink(osMemFileName);
    }
    else
    {
        memset(pabyData, 0, 4 * nBlockXSize * nBlockYSize );
    }

    sqlite3_finalize(hStmt);
    
    return pabyData;
}

/************************************************************************/
/*                         IReadBlock()                                 */
/************************************************************************/

CPLErr GDALGeoPackageRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                            void* pData)
{
    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
    //CPLDebug("GPKG", "IReadBlock(nBand=%d,nBlockXOff=%d,nBlockYOff=%d",
    //         nBand,nBlockXOff,nBlockYOff);

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

            for(int iBand=1;iBand<=poGDS->nBands;iBand++)
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
                    //CPLDebug("GPKG", "Copy source tile x=%d,w=%d,y=%d,h=%d into buffet at x=%d,y=%d",
                    //         nSrcXOffset, nSrcXSize, nSrcYOffset, nSrcYSize, nDstXOffset, nDstYOffset);
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
        if( poDrv == NULL || CSLTestBoolean(CPLGetConfigOption("GPKG_SIMUL_WEBP_3BAND", "FALSE")) )
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

CPLErr GDALGeoPackageDataset::WriteTile()
{
    if( !(bUpdate && m_asCachedTilesDesc[0].nRow >= 0 &&
          m_asCachedTilesDesc[0].nCol >= 0 &&
          m_asCachedTilesDesc[0].nIdxWithinTileData == 0) )
        return CE_None;

    int nRow = m_asCachedTilesDesc[0].nRow;
    int nCol = m_asCachedTilesDesc[0].nCol;

    int bAllDirty = TRUE;
    int bAllNonDirty = TRUE;
    int i;
    for(i=0;i<nBands;i++)
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

    int bPartialTile = FALSE;
    if( nBands != 4 )
    {
        if( (nCol + 1) * nBlockXSize > nRasterXSize )
            bPartialTile = TRUE;
        if( (nRow + 1) * nBlockYSize > nRasterYSize )
            bPartialTile = TRUE;
    }
    
    /* If all bands for that block are not dirty/written, we need to */
    /* fetch the missing ones if the tile exists */
    int bIsLossyFormat = FALSE;
    if( !bAllDirty )
    {
        for(i=1;i<=3;i++)
        {
            m_asCachedTilesDesc[i].nRow = -1;
            m_asCachedTilesDesc[i].nCol = -1;
            m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
        }
        ReadTile(nRow, nCol, m_pabyCachedTiles + 4 * nBlockXSize * nBlockYSize,
                 &bIsLossyFormat);
        for(i=0;i<nBands;i++)
        {
            if( !m_asCachedTilesDesc[0].abBandDirty[i] )
            {
                memcpy(m_pabyCachedTiles + i * nBlockXSize * nBlockYSize,
                       m_pabyCachedTiles + (4 + i) * nBlockXSize * nBlockYSize,
                       nBlockXSize * nBlockYSize);
            }
        }
    }

    int iYCount = nBlockYSize;
    int iXCount = nBlockXSize;

    if( bPartialTile )
    {
        memset(m_pabyCachedTiles + 3 * nBlockXSize * nBlockYSize, 0,
               nBlockXSize * nBlockYSize);
        if( (nRow + 1) * nBlockYSize > nRasterYSize )
            iYCount = nRasterYSize - nRow * nBlockYSize;
        if( (nCol + 1) * nBlockXSize > nRasterXSize )
            iXCount = nRasterXSize - nCol * nBlockXSize;
        for(int iY = 0; iY < iYCount; iY ++)
        {
            memset(m_pabyCachedTiles + (3 * nBlockYSize + iY) * nBlockXSize,
                   255, iXCount);
        }
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
    if( nBands == 4 )
    {
        GByte byFirstAlphaVal =  m_pabyCachedTiles[3 * nBlockXSize * nBlockYSize];
        for(i=1;i<nBlockXSize * nBlockYSize;i++)
        {
            if( m_pabyCachedTiles[3 * nBlockXSize * nBlockYSize + i] != byFirstAlphaVal )
                break;
        }
        if( i == nBlockXSize * nBlockYSize )
        {
            // If tile is fully transparent, don't serialize it and remove it if it exists
            if( byFirstAlphaVal == 0 )
            {
                char* pszSQL = sqlite3_mprintf("DELETE FROM '%q' "
                    "WHERE zoom_level = %d AND tile_row = %d AND tile_column = %d",
                    m_osRasterTable.c_str(), m_nZoomLevel, nRow, nCol);
#ifdef DEBUG_VERBOSE
                CPLDebug("GPKG", "%s", pszSQL);
#endif
                char* pszErrMsg = NULL;
                int rc = sqlite3_exec(GetDB(), pszSQL, NULL, NULL, &pszErrMsg);
                if( rc == SQLITE_OK )
                    eErr = CE_None;
                else
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Failure when deleting tile (row=%d,col=%d) at zoom_level=%d : %s",
                            nRow, nCol, m_nZoomLevel, pszErrMsg ? pszErrMsg : "");
                sqlite3_free(pszSQL);
                sqlite3_free(pszErrMsg);
                return CE_None;
            }
            bAllOpaque = (byFirstAlphaVal == 255);
        }
        else
            bAllOpaque = FALSE;
    }

    if( bIsLossyFormat )
    {
        CPLDebug("GPKG", "Had to read tile (row=%d,col=%d) at zoom_level=%d, "
                 "stored in a lossy format, before rewriting it, causing potential extra quality loss",
                 nRow, nCol, m_nZoomLevel);
    }

    CPLString osMemFileName;
    osMemFileName.Printf("/vsimem/gpkg_write_tile_%p", this);
    const char* pszDriverName = "PNG";
    int bTileDriverSupports4Bands = FALSE;
    int bTileDriverSupports1Band = TRUE;
    int bTileDriverSupportsCT = FALSE;
    
    if( nBands == 1 )
        GetRasterBand(1)->GetColorTable();
    
    if( m_eTF == GPKG_TF_PNG_JPEG )
    {
        if( bPartialTile || (nBands == 4 && !bAllOpaque) || m_poCT != NULL )
        {
            pszDriverName = "PNG";
            bTileDriverSupports4Bands = TRUE;
            bTileDriverSupportsCT = TRUE;
        }
        else
            pszDriverName = "JPEG";
    }
    else if( m_eTF == GPKG_TF_PNG )
    {
        pszDriverName = "PNG";
        bTileDriverSupports4Bands = TRUE;
        bTileDriverSupportsCT = TRUE;
    }
    else if( m_eTF == GPKG_TF_JPEG )
    {
        pszDriverName = "JPEG";
    }
    else if( m_eTF == GPKG_TF_WEBP )
    {
        pszDriverName = "WEBP";
        bTileDriverSupports4Bands = WEBPSupports4Bands();
        bTileDriverSupports1Band = FALSE;
    }
    else
        CPLAssert(0);

    GDALDriver* poDriver = (GDALDriver*) GDALGetDriverByName(pszDriverName);
    if( poDriver != NULL)
    {
        GDALDataset* poMEMDS = MEMDataset::Create("", nBlockXSize, nBlockYSize,
                                                  0, GDT_Byte, NULL);
        int nTileBands = nBands;
        if( bPartialTile && bTileDriverSupports4Bands )
            nTileBands = 4;
        else if( nBands == 4 && (bAllOpaque || !bTileDriverSupports4Bands) )
            nTileBands = 3;
        else if( nBands == 1 && m_poCT != NULL && !bTileDriverSupportsCT )
        {
            nTileBands = 3;
            if( bTileDriverSupports4Bands )
            {
                for(i=0;i<m_poCT->GetColorEntryCount();i++)
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

        for(i=0;i<nTileBands;i++)
        {
            char** papszOptions = NULL;
            char szDataPointer[32];
            int iSrc = i;
            if( nBands == 1 && m_poCT == NULL && nTileBands == 3 )
                iSrc = 0;
            else if( nBands == 1 && m_poCT == NULL && bPartialTile )
                iSrc = (i < 3) ? 0 : 3;
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
        if( nBands == 1 && m_poCT != NULL && nTileBands > 1 )
        {
            GByte abyCT[4*256];
            int nEntries = MIN(256, m_poCT->GetColorEntryCount());
            for(i=0;i<nEntries;i++)
            {
                const GDALColorEntry* psEntry = m_poCT->GetColorEntry(i);
                abyCT[4*i] = (GByte)psEntry->c1;
                abyCT[4*i+1] = (GByte)psEntry->c2;
                abyCT[4*i+2] = (GByte)psEntry->c3;
                abyCT[4*i+3] = (GByte)psEntry->c4;
            }
            for(;i<256;i++)
            {
                abyCT[4*i] = 0;
                abyCT[4*i+1] = 0;
                abyCT[4*i+2] = 0;
                abyCT[4*i+3] = 0;
            }
            for(int iY = 0; iY < iYCount; iY ++)
            {
                for(int iX = 0; iX < iXCount; iX ++)
                {
                    i = iY * nBlockXSize + iX;
                    GByte byVal = m_pabyCachedTiles[i];
                    m_pabyCachedTiles[i] = abyCT[4*byVal];
                    m_pabyCachedTiles[i + 1 * nBlockXSize * nBlockYSize] = abyCT[4*byVal+1];
                    m_pabyCachedTiles[i + 2 * nBlockXSize * nBlockYSize] = abyCT[4*byVal+2];
                    m_pabyCachedTiles[i + 3 * nBlockXSize * nBlockYSize] = abyCT[4*byVal+3];
                }
                if( iXCount < nBlockXSize )
                {
                    i = iY * nBlockXSize + iXCount;
                    memset(m_pabyCachedTiles + 0 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize - iXCount);
                    memset(m_pabyCachedTiles + 1 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize - iXCount);
                    memset(m_pabyCachedTiles + 2 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize - iXCount);
                    memset(m_pabyCachedTiles + 3 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize - iXCount);
                }
            }
            if( iYCount < nBlockYSize )
            {
                i = iYCount * nBlockXSize;
                memset(m_pabyCachedTiles + 0 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize * (nBlockYSize - iYCount));
                memset(m_pabyCachedTiles + 1 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize * (nBlockYSize - iYCount));
                memset(m_pabyCachedTiles + 2 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize * (nBlockYSize - iYCount));
                memset(m_pabyCachedTiles + 3 * nBlockXSize * nBlockYSize + i, 0, nBlockXSize * (nBlockYSize - iYCount));
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

        VSIStatBufL sStat;
        CPLAssert(VSIStatL(osMemFileName, &sStat) != 0);
        GDALDataset* poOutDS = poDriver->CreateCopy(osMemFileName, poMEMDS,
                                                    FALSE, papszDriverOptions, NULL, NULL);
        CSLDestroy( papszDriverOptions );
        if( poOutDS )
        {
            GDALClose( poOutDS );
            vsi_l_offset nBlobSize;
            GByte* pabyBlob = VSIGetMemFileBuffer(osMemFileName, &nBlobSize, TRUE);

            char* pszSQL = sqlite3_mprintf("INSERT OR REPLACE INTO '%q' "
                "(zoom_level, tile_row, tile_column, tile_data) VALUES (%d, %d, %d, ?)",
                m_osRasterTable.c_str(), m_nZoomLevel, nRow, nCol);
#ifdef DEBUG_VERBOSE
            CPLDebug("GPKG", "%s", pszSQL);
#endif
            sqlite3_stmt* hStmt = NULL;
            int rc = sqlite3_prepare(GetDB(), pszSQL, -1, &hStmt, NULL);
            if ( rc != SQLITE_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s", pszSQL);
                CPLFree(pabyBlob);
            }
            else
            {
                sqlite3_bind_blob( hStmt, 1, pabyBlob, (int)nBlobSize, CPLFree);
                rc = sqlite3_step( hStmt );
                if( rc == SQLITE_DONE )
                    eErr = CE_None;
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Failure when inserting tile (row=%d,col=%d) at zoom_level=%d : %s",
                             nRow, nCol, m_nZoomLevel, sqlite3_errmsg(GetDB()));
                }
            }
            sqlite3_finalize(hStmt);
            sqlite3_free(pszSQL);
        }

        VSIUnlink(osMemFileName);
        delete poMEMDS;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot find driver %s", pszDriverName);
    }

    return eErr;
}

/************************************************************************/
/*                         IWriteBlock()                                */
/************************************************************************/

CPLErr GDALGeoPackageRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                             void* pData)
{
    //CPLDebug("GPKG", "IWriteBlock(nBand=%d,nBlockXOff=%d,nBlockYOff=%d",
    //         nBand,nBlockXOff,nBlockYOff);

    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
    if( !poGDS->bUpdate )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "IWriteBlock() not supported on dataset opened in read-only mode");
        return CE_Failure;
    }

    if( poGDS->m_nShiftXPixelsMod || poGDS->m_nShiftYPixelsMod )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "IWriteBlock() not supported when A.O.I is shifted w.r.t. tile matrix set origin");
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

    CPLErr eErr = CE_None;
    if( !(nRow == poGDS->m_asCachedTilesDesc[0].nRow &&
          nCol == poGDS->m_asCachedTilesDesc[0].nCol &&
          poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData == 0) )
    {
        eErr = poGDS->WriteTile();

        poGDS->m_asCachedTilesDesc[0].nRow = nRow;
        poGDS->m_asCachedTilesDesc[0].nCol = nCol;
        poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData = 0;
    }

    memcpy( poGDS->m_pabyCachedTiles + (nBand - 1) * nBlockXSize * nBlockYSize,
            pData, nBlockXSize * nBlockYSize );
    poGDS->m_asCachedTilesDesc[0].abBandDirty[nBand - 1] = TRUE;

    // Check if all bands for this block are dirty, and if so write the tile
    int bAllDirty = TRUE;
    for(int i=0;i<poGDS->nBands;i++)
    {
        // If the block for this band is not dirty, it might be dirty in cache
        if( !poGDS->m_asCachedTilesDesc[0].abBandDirty[i] )
        {
            GDALRasterBlock* poBlock =
                ((GDALGeoPackageRasterBand*)poGDS->GetRasterBand(i+1))->
                            TryGetLockedBlockRef(nBlockXOff, nBlockYOff);
            if( poBlock && poBlock->GetDirty() )
            {
                memcpy(poGDS->m_pabyCachedTiles + i * nBlockXSize * nBlockYSize,
                       poBlock->GetDataRef(),
                       nBlockXSize * nBlockYSize);
                poBlock->MarkClean();
                poBlock->DropLock();
                poGDS->m_asCachedTilesDesc[0].abBandDirty[i] = TRUE;
            }
            else
            {
                if( poBlock )
                    poBlock->DropLock();
                bAllDirty = FALSE;
                break;
            }
        }
    }
    if( bAllDirty ) 
    {
        CPLErr eErr2 = poGDS->WriteTile();
        if( eErr == CE_None )
            eErr = eErr2;
    }

    return eErr;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int GDALGeoPackageRasterBand::GetOverviewCount()
{
    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
    return poGDS->m_nOverviewCount;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

GDALRasterBand* GDALGeoPackageRasterBand::GetOverview(int nIdx)
{
    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
    if( nIdx < 0 || nIdx >= poGDS->m_nOverviewCount )
        return NULL;
    return poGDS->m_papoOverviewDS[nIdx]->GetRasterBand(nBand);
}
