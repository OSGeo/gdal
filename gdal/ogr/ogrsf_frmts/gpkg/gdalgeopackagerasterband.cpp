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
    SetColorInterpretation((GDALColorInterp) (GCI_RedBand + (nBand - 1)));
}

/************************************************************************/
/*                           ReadTile()                                 */
/************************************************************************/

CPLErr GDALGeoPackageRasterBand::ReadTile(const CPLString& osMemFileName,
                                          GByte* pabyTileData)
{
    const char* apszDrivers[] = { "JPEG", "PNG", "WEBP", NULL };
    GDALDataset* poDSTile = (GDALDataset*)GDALOpenEx(osMemFileName.c_str(),
                                                     GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                                     apszDrivers, NULL, NULL);
    if( poDSTile == NULL )
    {
        memset(pabyTileData, 0, 4 * nBlockXSize * nBlockYSize );
        return CE_Failure;
    }

    if( !(poDSTile->GetRasterXSize() == nBlockXSize &&
          poDSTile->GetRasterYSize() == nBlockYSize &&
          (poDSTile->GetRasterCount() == 1 ||
           poDSTile->GetRasterCount() == 3 ||
           poDSTile->GetRasterCount() == 4)) )
    {
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

    if( poDSTile->GetRasterCount() == 1 )
    {
        GDALColorTable* poCT = poDSTile->GetRasterBand(1)->GetColorTable();
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
    else if( poDSTile->GetRasterCount() == 3 )
    {
        memset(pabyTileData + 3 * nBlockXSize * nBlockYSize,
                255, nBlockXSize * nBlockYSize);
    }

    delete poDSTile;
    
    return CE_None;
}

/************************************************************************/
/*                           ReadTile()                                 */
/************************************************************************/

GByte* GDALGeoPackageRasterBand::ReadTile(int nRow, int nCol)
{
    int rc;
    GDALGeoPackageDataset* poGDS = (GDALGeoPackageDataset* )poDS;
    
    GByte* pabyData = NULL;
    if( poGDS->m_nShiftXPixelsMod )
    {
        int i;
        for(i = 0; i < 4; i ++)
        {
            if( poGDS->m_asCachedTilesDesc[i].nRow == nRow &&
                poGDS->m_asCachedTilesDesc[i].nCol == nCol )
            {
                if( poGDS->m_asCachedTilesDesc[i].nIdxWithinTileData >= 0 )
                {
                    return poGDS->m_pabyCachedTiles +
                        poGDS->m_asCachedTilesDesc[i].nIdxWithinTileData * 4 * nBlockXSize * nBlockYSize;
                }
                else
                {
                    if( i == 0 )
                        poGDS->m_asCachedTilesDesc[i].nIdxWithinTileData =
                            (poGDS->m_asCachedTilesDesc[1].nIdxWithinTileData == 0 ) ? 1 : 0;
                    else if( i == 1 )
                        poGDS->m_asCachedTilesDesc[i].nIdxWithinTileData =
                            (poGDS->m_asCachedTilesDesc[0].nIdxWithinTileData == 0 ) ? 1 : 0;
                    else if( i == 2 )
                        poGDS->m_asCachedTilesDesc[i].nIdxWithinTileData =
                            (poGDS->m_asCachedTilesDesc[3].nIdxWithinTileData == 2 ) ? 3 : 2;
                    else
                        poGDS->m_asCachedTilesDesc[i].nIdxWithinTileData =
                            (poGDS->m_asCachedTilesDesc[2].nIdxWithinTileData == 2 ) ? 3 : 2;
                    pabyData = poGDS->m_pabyCachedTiles +
                                            poGDS->m_asCachedTilesDesc[i].nIdxWithinTileData * 4 * nBlockXSize * nBlockYSize;
                    break;
                }
            }
        }
        CPLAssert(i < 4);
    }
    else
        pabyData = poGDS->m_pabyCachedTiles;
    
    //CPLDebug("GPKG", "For block (blocky=%d, blockx=%d) request tile (row=%d, col=%d)",
    //         nBlockYOff, nBlockXOff, nRow, nCol);
    const char* pszSQL = CPLSPrintf("SELECT tile_data FROM %s "
        "WHERE zoom_level = %d AND tile_row = %d AND tile_column = %d",
        poGDS->m_osRasterTable.c_str(), poGDS->m_nZoomLevel, nRow, nCol);
    sqlite3_stmt* hStmt = NULL;
    rc = sqlite3_prepare(poGDS->GetDB(), pszSQL, -1, &hStmt, NULL);
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s", pszSQL);
        return NULL;
    }
    rc = sqlite3_step( hStmt );

    if( rc == SQLITE_ROW && sqlite3_column_type( hStmt, 0 ) == SQLITE_BLOB )
    {
        const int nBytes = sqlite3_column_bytes( hStmt, 0 );
        GByte* pabyRawData = (GByte*)sqlite3_column_blob( hStmt, 0 );
        CPLString osMemFileName;
        osMemFileName.Printf("/vsimem/%p", this);
        VSILFILE * fp = VSIFileFromMemBuffer( osMemFileName.c_str(), pabyRawData,
                                                nBytes, FALSE);
        VSIFCloseL(fp);

        ReadTile(osMemFileName, pabyData);
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
            GByte* pabyTileData = ReadTile(nRow, nCol);
            if( pabyTileData == NULL )
                return CE_Failure;

            for(int iBand=1;iBand<=4;iBand++)
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
       