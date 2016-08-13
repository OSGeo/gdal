/******************************************************************************
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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_geopackage.h"
#include "memdataset.h"
#include "gdal_alg_priv.h"

CPL_CVSID("$Id$");

#if !defined(DEBUG_VERBOSE) && defined(DEBUG_VERBOSE_GPKG)
#define DEBUG_VERBOSE
#endif

/************************************************************************/
/*                    GDALGPKGMBTilesLikePseudoDataset()                */
/************************************************************************/

GDALGPKGMBTilesLikePseudoDataset::GDALGPKGMBTilesLikePseudoDataset() :
    m_bNew(false),
    m_bHasModifiedTiles(false),
    m_nZoomLevel(-1),
    m_pabyCachedTiles(NULL),
    m_nShiftXTiles(0),
    m_nShiftXPixelsMod(0),
    m_nShiftYTiles(0),
    m_nShiftYPixelsMod(0),
    m_nTileMatrixWidth(0),
    m_nTileMatrixHeight(0),
    m_eTF(GPKG_TF_PNG_JPEG),
    m_bPNGSupports2Bands(true),
    m_bPNGSupportsCT(true),
    m_nZLevel(6),
    m_nQuality(75),
    m_bDither(false),
    m_poCT(NULL),
    m_bTriedEstablishingCT(false),
    m_pabyHugeColorArray(NULL),
#ifdef HAVE_SQLITE_VFS
  m_pMyVFS(NULL),
#endif
    m_hTempDB(NULL),
    m_nLastSpaceCheckTimestamp(0),
    m_bForceTempDBCompaction(
        CPLTestBool(CPLGetConfigOption("GPKG_FORCE_TEMPDB_COMPACTION", "NO"))),
    m_nAge(0),
    m_nTileInsertionCount(0),
    m_poParentDS(NULL),
    m_bInWriteTile(false)
{
    for( int i = 0; i < 4; i++ )
    {
        m_asCachedTilesDesc[i].nRow = -1;
        m_asCachedTilesDesc[i].nCol = -1;
        m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
        m_asCachedTilesDesc[i].abBandDirty[0] = FALSE;
        m_asCachedTilesDesc[i].abBandDirty[1] = FALSE;
        m_asCachedTilesDesc[i].abBandDirty[2] = FALSE;
        m_asCachedTilesDesc[i].abBandDirty[3] = FALSE;
    }
}

/************************************************************************/
/*                 ~GDALGPKGMBTilesLikePseudoDataset()                  */
/************************************************************************/

GDALGPKGMBTilesLikePseudoDataset::~GDALGPKGMBTilesLikePseudoDataset()
{
    if( m_poParentDS == NULL && m_hTempDB != NULL )
    {
        sqlite3_close(m_hTempDB);
        m_hTempDB = NULL;
        VSIUnlink(m_osTempDBFilename);
#ifdef HAVE_SQLITE_VFS
        if( m_pMyVFS )
        {
            sqlite3_vfs_unregister(m_pMyVFS);
            CPLFree(m_pMyVFS->pAppData);
            CPLFree(m_pMyVFS);
        }
#endif
    }
    CPLFree(m_pabyCachedTiles);
    delete m_poCT;
    CPLFree(m_pabyHugeColorArray);
}

/************************************************************************/
/*                      GDALGPKGMBTilesLikeRasterBand()                 */
/************************************************************************/

GDALGPKGMBTilesLikeRasterBand::GDALGPKGMBTilesLikeRasterBand(
    GDALGPKGMBTilesLikePseudoDataset* poTPD, int nTileWidth, int nTileHeight) :
    m_poTPD(poTPD)
{
    eDataType = GDT_Byte;
    nBlockXSize = nTileWidth;
    nBlockYSize = nTileHeight;
}

/************************************************************************/
/*                              FlushCache()                            */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikeRasterBand::FlushCache()
{
    m_poTPD->m_nLastSpaceCheckTimestamp = -1; // disable partial flushes
    CPLErr eErr = GDALPamRasterBand::FlushCache();
    if( eErr == CE_None )
        eErr = m_poTPD->IFlushCacheWithErrCode();
    m_poTPD->m_nLastSpaceCheckTimestamp = 0;
    return eErr;
}

/************************************************************************/
/*                              FlushTiles()                            */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikePseudoDataset::FlushTiles()
{
    CPLErr eErr = CE_None;
    if( IGetUpdate() )
    {
        if( m_nShiftXPixelsMod || m_nShiftYPixelsMod )
        {
            eErr = FlushRemainingShiftedTiles(false /* total flush*/);
        }
        else
        {
            eErr = WriteTile();
        }
    }

    GDALGPKGMBTilesLikePseudoDataset* poMainDS = m_poParentDS ? m_poParentDS : this;
    if( poMainDS->m_nTileInsertionCount )
    {
        poMainDS->ICommitTransaction();
        poMainDS->m_nTileInsertionCount = 0;
    }
    return eErr;
}

/************************************************************************/
/*                             GetColorTable()                          */
/************************************************************************/

GDALColorTable* GDALGPKGMBTilesLikeRasterBand::GetColorTable()
{
    if( poDS->GetRasterCount() != 1 )
        return NULL;

    if( !m_poTPD->m_bTriedEstablishingCT )
    {
        m_poTPD->m_bTriedEstablishingCT = true;
        if( m_poTPD->m_poParentDS != NULL )
        {
            m_poTPD->m_poCT
                = m_poTPD->m_poParentDS->IGetRasterBand(1)->GetColorTable();
            if( m_poTPD->m_poCT )
                m_poTPD->m_poCT = m_poTPD->m_poCT->Clone();
            return m_poTPD->m_poCT;
        }

        for( int i = 0; i < 2; i++ )
        {
            bool bRetry = false;
            char* pszSQL = NULL;
            if( i == 0 )
            {
                pszSQL = sqlite3_mprintf("SELECT tile_data FROM '%q' "
                    "WHERE zoom_level = %d LIMIT 1",
                    m_poTPD->m_osRasterTable.c_str(), m_poTPD->m_nZoomLevel);
            }
            else
            {
                // Try a tile in the middle of the raster
                pszSQL = sqlite3_mprintf("SELECT tile_data FROM '%q' "
                    "WHERE zoom_level = %d AND tile_column = %d AND tile_row = %d",
                    m_poTPD->m_osRasterTable.c_str(), m_poTPD->m_nZoomLevel,
                    m_poTPD->m_nShiftXTiles + nRasterXSize / 2 / nBlockXSize,
                    m_poTPD->GetRowFromIntoTopConvention(m_poTPD->m_nShiftYTiles + nRasterYSize / 2 / nBlockYSize));
            }
            sqlite3_stmt* hStmt = NULL;
            int rc = sqlite3_prepare(m_poTPD->IGetDB(), pszSQL, -1, &hStmt, NULL);
            if( rc == SQLITE_OK )
            {
                rc = sqlite3_step( hStmt );
                if( rc == SQLITE_ROW
                    && sqlite3_column_type( hStmt, 0 ) == SQLITE_BLOB )
                {
                    const int nBytes = sqlite3_column_bytes( hStmt, 0 );
                    GByte* pabyRawData = reinterpret_cast<GByte *>(
                        const_cast<void *>( sqlite3_column_blob( hStmt, 0 ) ) );
                    CPLString osMemFileName;
                    osMemFileName.Printf("/vsimem/gpkg_read_tile_%p", this);
                    VSILFILE *fp = VSIFileFromMemBuffer( osMemFileName.c_str(),
                                                        pabyRawData,
                                                        nBytes, FALSE);
                    VSIFCloseL(fp);

                    // Only PNG can have color table.
                    const char* apszDrivers[] = { "PNG", NULL };
                    GDALDataset* poDSTile = reinterpret_cast<GDALDataset *>(
                        GDALOpenEx( osMemFileName.c_str(),
                                    GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                                    apszDrivers, NULL, NULL ) );
                    if( poDSTile != NULL )
                    {
                        if( poDSTile->GetRasterCount() == 1 )
                        {
                            m_poTPD->m_poCT
                                = poDSTile->GetRasterBand(1)->GetColorTable();
                            if( m_poTPD->m_poCT != NULL )
                                m_poTPD->m_poCT = m_poTPD->m_poCT->Clone();
                        }
                        else
                        {
                            bRetry = true;
                        }
                        GDALClose( poDSTile );
                    }
                    else
                        bRetry = true;

                    VSIUnlink(osMemFileName);
                }
            }
            sqlite3_free(pszSQL);
            sqlite3_finalize(hStmt);
            if( !bRetry )
                break;
        }
    }

    return m_poTPD->m_poCT;
}

/************************************************************************/
/*                             SetColorTable()                          */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikeRasterBand::SetColorTable(GDALColorTable* poCT)
{
    if( poDS->GetRasterCount() != 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetColorTable() only supported for a single band dataset");
        return CE_Failure;
    }
    if( !m_poTPD->m_bNew || m_poTPD->m_bTriedEstablishingCT )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetColorTable() only supported on a newly created dataset");
        return CE_Failure;
    }

    m_poTPD->m_bTriedEstablishingCT = true;
    delete m_poTPD->m_poCT;
    if( poCT != NULL )
        m_poTPD->m_poCT = poCT->Clone();
    else
        m_poTPD->m_poCT = NULL;
    return CE_None;
}

/************************************************************************/
/*                        GetColorInterpretation()                      */
/************************************************************************/

GDALColorInterp GDALGPKGMBTilesLikeRasterBand::GetColorInterpretation()
{
    if( poDS->GetRasterCount() == 1 )
        return GetColorTable() ? GCI_PaletteIndex : GCI_GrayIndex;
    else if( poDS->GetRasterCount() == 2 )
        return (nBand == 1) ? GCI_GrayIndex : GCI_AlphaBand;
    else
        return (GDALColorInterp) (GCI_RedBand + (nBand - 1));
}

/************************************************************************/
/*                        SetColorInterpretation()                      */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikeRasterBand::SetColorInterpretation(
    GDALColorInterp eInterp )
{
    if( eInterp == GCI_Undefined )
        return CE_None;
    if( poDS->GetRasterCount() == 1
        && (eInterp == GCI_GrayIndex || eInterp == GCI_PaletteIndex) )
        return CE_None;
    if( poDS->GetRasterCount() == 2 &&
        ((nBand == 1
          && eInterp == GCI_GrayIndex)
         || (nBand == 2 && eInterp == GCI_AlphaBand)) )
        return CE_None;
    if( poDS->GetRasterCount() >= 3 && eInterp == GCI_RedBand + nBand - 1 )
        return CE_None;
    CPLError(CE_Warning, CPLE_NotSupported,
             "%s color interpretation not supported. Will be ignored",
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
    const int nEntries = MIN(256, poCT->GetColorEntryCount());
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

CPLErr GDALGPKGMBTilesLikePseudoDataset::ReadTile(const CPLString& osMemFileName,
                                       GByte* pabyTileData,
                                       bool* pbIsLossyFormat)
{
    const char* apszDrivers[] = { "JPEG", "PNG", "WEBP", NULL };
    int nBlockXSize, nBlockYSize;
    IGetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    const int nBands = IGetRasterCount();
    GDALDataset* poDSTile = reinterpret_cast<GDALDataset*>(
        GDALOpenEx( osMemFileName.c_str(),
                    GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    apszDrivers, NULL, NULL ) );
    if( poDSTile == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot parse tile data");
        memset(pabyTileData, 0, nBands * nBlockXSize * nBlockYSize );
        return CE_Failure;
    }

    const int nTileBandCount = poDSTile->GetRasterCount();

    if( !(poDSTile->GetRasterXSize() == nBlockXSize &&
          poDSTile->GetRasterYSize() == nBlockYSize &&
          (nTileBandCount >= 1 && nTileBandCount <= 4)) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Inconsistent tiles characteristics" );
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
        IGetRasterBand(1)->GetColorTable();
    }

    if( pbIsLossyFormat )
        *pbIsLossyFormat
            = !EQUAL(poDSTile->GetDriver()->GetDescription(), "PNG") ||
              (poCT != NULL && poCT->GetColorEntryCount() == 256) /* PNG8 */;

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
            const GByte c1 = pabyTileData[i];
            const GByte c2 = pabyTileData[i + nBlockXSize * nBlockYSize];
            const GByte c3 = pabyTileData[i + 2 * nBlockXSize * nBlockYSize];
            const GByte c4 = pabyTileData[i + 3 * nBlockXSize * nBlockYSize];
            GUInt32 nVal = c1 + (c2 << 8) + (c3 << 16);
            if( nTileBandCount == 4 ) nVal += (c4 << 24);
            if( nVal == 0 )
                // In most cases we will reach that point at partial tiles.
                pabyTileData[i] = static_cast<GByte>( iBestEntryFor0 );
            else
            {
                std::map< GUInt32, int >::iterator oMapEntryToIndexIter
                    = oMapEntryToIndex.find(nVal);
                if( oMapEntryToIndexIter == oMapEntryToIndex.end() )
                    /* Could happen with JPEG tiles */
                  pabyTileData[i] = static_cast<GByte>(
                      GPKGFindBestEntry(m_poCT, c1, c2, c3, c4,
                                        nTileBandCount) );
                else
                    pabyTileData[i] = static_cast<GByte>(
                        oMapEntryToIndexIter->second );
            }
        }
        GDALClose( poDSTile );
        return CE_None;
    }

    if( nBands == 1 && nTileBandCount == 1 && poCT != NULL && m_poCT != NULL &&
             !poCT->IsSame(m_poCT) )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Different color tables. Unhandled for now" );
    }
    else if( (nBands == 1 && nTileBandCount >= 3) ||
             (nBands == 1 && nTileBandCount == 1 && m_poCT != NULL
              && poCT == NULL) ||
             ((nBands == 1 || nBands == 2) && nTileBandCount == 1
              && m_poCT == NULL && poCT != NULL) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Inconsistent dataset and tiles band characteristics");
    }

    if( nBands == 2 )
    {
        // assuming that the RGB is Grey,Grey,Grey
        if( nTileBandCount == 1 || nTileBandCount == 3 )
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
            GByte abyCT[4*256];
            int nEntries = MIN(256, poCT->GetColorEntryCount());
            for( int i = 0; i < nEntries; i++ )
            {
                const GDALColorEntry* psEntry = poCT->GetColorEntry(i);
                abyCT[4*i] = (GByte)psEntry->c1;
                abyCT[4*i+1] = (GByte)psEntry->c2;
                abyCT[4*i+2] = (GByte)psEntry->c3;
                abyCT[4*i+3] = (GByte)psEntry->c4;
            }
            for( int i = nEntries; i < 256; i++ )
            {
                abyCT[4*i] = 0;
                abyCT[4*i+1] = 0;
                abyCT[4*i+2] = 0;
                abyCT[4*i+3] = 0;
            }
            for( int i = 0; i < nBlockXSize * nBlockYSize; i++ )
            {
                const GByte byVal = pabyTileData[i];
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

GByte* GDALGPKGMBTilesLikePseudoDataset::ReadTile(int nRow, int nCol)
{
    int nBlockXSize, nBlockYSize;
    IGetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    const int nBands = IGetRasterCount();
    if( m_nShiftXPixelsMod || m_nShiftYPixelsMod )
    {
        GByte* pabyData = NULL;
        int i = 0;
        for( ; i < 4; i++ )
        {
            if( m_asCachedTilesDesc[i].nRow == nRow &&
                m_asCachedTilesDesc[i].nCol == nCol )
            {
                if( m_asCachedTilesDesc[i].nIdxWithinTileData >= 0 )
                {
                    return m_pabyCachedTiles +
                        m_asCachedTilesDesc[i].nIdxWithinTileData * 4 *
                        nBlockXSize * nBlockYSize;
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
        return ReadTile(nRow, nCol, pabyData);
    }
    else
    {
        GByte* pabyDest = m_pabyCachedTiles + 8 * nBlockXSize * nBlockYSize;
        bool bAllNonDirty = true;
        for( int i = 0; i < nBands; i++ )
        {
            if( m_asCachedTilesDesc[0].abBandDirty[i] )
                bAllNonDirty = false;
        }
        if( bAllNonDirty )
        {
            return ReadTile(nRow, nCol, pabyDest);
        }

        /* If some bands of the blocks are dirty/written we need to fetch */
        /* the tile in a temporary buffer in order not to override dirty bands*/
        for( int i = 1; i <= 3; i++ )
        {
            m_asCachedTilesDesc[i].nRow = -1;
            m_asCachedTilesDesc[i].nCol = -1;
            m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
        }
        GByte* pabyTemp = m_pabyCachedTiles + 12 * nBlockXSize * nBlockYSize;
        if( ReadTile(nRow, nCol, pabyTemp) != NULL )
        {
            for( int i = 0; i < nBands; i++ )
            {
                if( !m_asCachedTilesDesc[0].abBandDirty[i] )
                {
                    memcpy(pabyDest + i * nBlockXSize * nBlockYSize,
                           pabyTemp + i * nBlockXSize * nBlockYSize,
                           nBlockXSize * nBlockYSize);
                }
            }
        }
        return pabyDest;
    }
}

/************************************************************************/
/*                           ReadTile()                                 */
/************************************************************************/

GByte* GDALGPKGMBTilesLikePseudoDataset::ReadTile( int nRow, int nCol, GByte *pabyData,
                                        bool *pbIsLossyFormat)
{
    int nBlockXSize;
    int nBlockYSize;
    IGetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    const int nBands = IGetRasterCount();

    if( pbIsLossyFormat ) *pbIsLossyFormat = false;

    if( nRow < 0 || nCol < 0 || nRow >= m_nTileMatrixHeight ||
        nCol >= m_nTileMatrixWidth )
    {
        memset( pabyData, 0, nBands * nBlockXSize * nBlockYSize );
        return pabyData;
    }

#ifdef DEBUG_VERBOSE
    CPLDebug( "GPKG", "ReadTile(row=%d, col=%d)", nRow, nCol );
#endif

    char *pszSQL = sqlite3_mprintf( "SELECT tile_data FROM '%q' "
        "WHERE zoom_level = %d AND tile_row = %d AND tile_column = %d%s",
        m_osRasterTable.c_str(), m_nZoomLevel, GetRowFromIntoTopConvention(nRow), nCol,
        m_osWHERE.size() ? CPLSPrintf(" AND (%s)", m_osWHERE.c_str()): "");

#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "%s", pszSQL);
#endif

    sqlite3_stmt *hStmt = NULL;
    int rc = sqlite3_prepare( IGetDB(), pszSQL, -1, &hStmt, NULL );
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to prepare SQL: %s", pszSQL );
        sqlite3_free(pszSQL);
        return NULL;
    }
    sqlite3_free( pszSQL );
    rc = sqlite3_step( hStmt );

    if( rc == SQLITE_ROW && sqlite3_column_type( hStmt, 0 ) == SQLITE_BLOB )
    {
        const int nBytes = sqlite3_column_bytes( hStmt, 0 );
        GByte* pabyRawData = static_cast<GByte *>( const_cast<void *>(
            sqlite3_column_blob( hStmt, 0 ) ) );
        CPLString osMemFileName;
        osMemFileName.Printf("/vsimem/gpkg_read_tile_%p", this);
        VSILFILE * fp = VSIFileFromMemBuffer(
            osMemFileName.c_str(), pabyRawData, nBytes, FALSE );
        VSIFCloseL(fp);

        ReadTile(osMemFileName, pabyData, pbIsLossyFormat);
        VSIUnlink(osMemFileName);
        sqlite3_finalize(hStmt);
    }
    else
    {
        sqlite3_finalize( hStmt );
        hStmt = NULL;

        if( m_hTempDB && (m_nShiftXPixelsMod || m_nShiftYPixelsMod) )
        {
            const char* pszSQLNew = CPLSPrintf(
                "SELECT partial_flag, tile_data_band_1, tile_data_band_2, "
                "tile_data_band_3, tile_data_band_4 FROM partial_tiles WHERE "
                "zoom_level = %d AND tile_row = %d AND tile_column = %d",
                m_nZoomLevel, nRow, nCol);

#ifdef DEBUG_VERBOSE
            CPLDebug("GPKG", "%s", pszSQLNew);
#endif

            rc = sqlite3_prepare_v2(m_hTempDB, pszSQLNew, -1, &hStmt, NULL);
            if ( rc != SQLITE_OK )
            {
                memset(pabyData, 0, nBands * nBlockXSize * nBlockYSize );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "sqlite3_prepare(%s) failed: %s",
                          pszSQLNew, sqlite3_errmsg( m_hTempDB ) );
                return pabyData;
            }

            rc = sqlite3_step(hStmt);
            if ( rc == SQLITE_ROW )
            {
                const int nPartialFlag = sqlite3_column_int(hStmt, 0);
                for(int iBand = 1; iBand <= nBands; iBand ++ )
                {
                    GByte* pabyDestBand
                        = pabyData + (iBand - 1) * nBlockXSize * nBlockYSize;
                    if( nPartialFlag & (((1 << 4)-1) << (4 * (iBand - 1))) )
                    {
                        CPLAssert( sqlite3_column_bytes(hStmt, iBand)
                                   == nBlockXSize * nBlockYSize );
                        memcpy( pabyDestBand,
                                sqlite3_column_blob(hStmt, iBand),
                                nBlockXSize * nBlockYSize );
                    }
                    else
                    {
                        memset(pabyDestBand, 0, nBlockXSize * nBlockYSize );
                    }
                }
            }
            else
            {
                memset(pabyData, 0, nBands * nBlockXSize * nBlockYSize );
            }
            sqlite3_finalize(hStmt);
        }
        else
        {
            memset(pabyData, 0, nBands * nBlockXSize * nBlockYSize );
        }
    }

    return pabyData;
}

/************************************************************************/
/*                         IReadBlock()                                 */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikeRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                            void* pData)
{
#ifdef DEBUG_VERBOSE
    CPLDebug( "GPKG", "IReadBlock(nBand=%d,nBlockXOff=%d,nBlockYOff=%d,m_nZoomLevel=%d)",
              nBand,nBlockXOff,nBlockYOff,m_poTPD->m_nZoomLevel);
#endif

    const int nRowMin = nBlockYOff + m_poTPD->m_nShiftYTiles;
    int nRowMax = nRowMin;
    if( m_poTPD->m_nShiftYPixelsMod )
        nRowMax ++;

    const int nColMin = nBlockXOff + m_poTPD->m_nShiftXTiles;
    int nColMax = nColMin;
    if( m_poTPD->m_nShiftXPixelsMod )
        nColMax ++;

retry:
    /* Optimize for left to right reading at constant row */
    if( m_poTPD->m_nShiftXPixelsMod || m_poTPD->m_nShiftYPixelsMod )
    {
        if( nRowMin == m_poTPD->m_asCachedTilesDesc[0].nRow &&
            nColMin == m_poTPD->m_asCachedTilesDesc[0].nCol + 1 &&
            m_poTPD->m_asCachedTilesDesc[0].nIdxWithinTileData >= 0 )
        {
            CPLAssert(nRowMin == m_poTPD->m_asCachedTilesDesc[1].nRow);
            CPLAssert(nColMin == m_poTPD->m_asCachedTilesDesc[1].nCol);
            CPLAssert(m_poTPD->m_asCachedTilesDesc[0].nIdxWithinTileData == 0 ||
                      m_poTPD->m_asCachedTilesDesc[0].nIdxWithinTileData == 1);

            /* 0 1  --> 1 -1 */
            /* 2 3      3 -1 */
            /* or */
            /* 1 0  --> 0 -1 */
            /* 3 2      2 -1 */
            m_poTPD->m_asCachedTilesDesc[0].nIdxWithinTileData
                = m_poTPD->m_asCachedTilesDesc[1].nIdxWithinTileData;
            m_poTPD->m_asCachedTilesDesc[2].nIdxWithinTileData
                = m_poTPD->m_asCachedTilesDesc[3].nIdxWithinTileData;
        }
        else
        {
            m_poTPD->m_asCachedTilesDesc[0].nIdxWithinTileData = -1;
            m_poTPD->m_asCachedTilesDesc[2].nIdxWithinTileData = -1;
        }
        m_poTPD->m_asCachedTilesDesc[0].nRow = nRowMin;
        m_poTPD->m_asCachedTilesDesc[0].nCol = nColMin;
        m_poTPD->m_asCachedTilesDesc[1].nRow = nRowMin;
        m_poTPD->m_asCachedTilesDesc[1].nCol = nColMin + 1;
        m_poTPD->m_asCachedTilesDesc[2].nRow = nRowMin + 1;
        m_poTPD->m_asCachedTilesDesc[2].nCol = nColMin;
        m_poTPD->m_asCachedTilesDesc[3].nRow = nRowMin + 1;
        m_poTPD->m_asCachedTilesDesc[3].nCol = nColMin + 1;
        m_poTPD->m_asCachedTilesDesc[1].nIdxWithinTileData = -1;
        m_poTPD->m_asCachedTilesDesc[3].nIdxWithinTileData = -1;

    }

    for(int nRow = nRowMin; nRow <= nRowMax; nRow ++)
    {
        for(int nCol = nColMin; nCol <= nColMax; nCol++ )
        {
            if( m_poTPD->m_nShiftXPixelsMod == 0 && m_poTPD->m_nShiftYPixelsMod == 0 )
            {
                if( !(nRow == m_poTPD->m_asCachedTilesDesc[0].nRow &&
                      nCol == m_poTPD->m_asCachedTilesDesc[0].nCol &&
                      m_poTPD->m_asCachedTilesDesc[0].nIdxWithinTileData == 0) )
                {
                    if( m_poTPD->WriteTile() != CE_None )
                        return CE_Failure;
                }
            }

            GByte* pabyTileData = m_poTPD->ReadTile(nRow, nCol);
            if( pabyTileData == NULL )
                return CE_Failure;

            for(int iBand=1;iBand<=poDS->GetRasterCount();iBand++)
            {
                GDALRasterBlock* poBlock = NULL;
                GByte* pabyDest = NULL;
                if( iBand == nBand )
                {
                    pabyDest = (GByte*)pData;
                }
                else
                {
                    poBlock =
                        poDS->GetRasterBand(iBand)->GetLockedBlockRef(
                            nBlockXOff, nBlockYOff, TRUE );
                    if( poBlock == NULL )
                        continue;
                    if( poBlock->GetDirty() )
                    {
                        poBlock->DropLock();
                        continue;
                    }
                    /* if we are short of GDAL cache max and there are dirty blocks */
                    /* of our dataset, the above GetLockedBlockRef() might have reset */
                    /* (at least part of) the 4 tiles we want to cache and have */
                    /* already read */
                    // FIXME this is way too fragile.
                    if( (m_poTPD->m_nShiftXPixelsMod != 0 ||
                         m_poTPD->m_nShiftYPixelsMod != 0) &&
                        (m_poTPD->m_asCachedTilesDesc[0].nRow != nRowMin ||
                         m_poTPD->m_asCachedTilesDesc[0].nCol != nColMin) )
                    {
                        poBlock->DropLock();
                        goto retry;
                    }
                    pabyDest = (GByte*) poBlock->GetDataRef();
                }

                // Composite tile data into block data
                if( m_poTPD->m_nShiftXPixelsMod == 0
                    && m_poTPD->m_nShiftYPixelsMod == 0 )
                {
                    memcpy( pabyDest,
                            pabyTileData +
                            (iBand - 1) * nBlockXSize * nBlockYSize,
                            nBlockXSize * nBlockYSize );
#ifdef DEBUG_VERBOSE
                    if( (nBlockXOff+1) * nBlockXSize <= nRasterXSize &&
                        (nBlockYOff+1) * nBlockYSize > nRasterYSize )
                    {
                        bool bFoundNonZero = false;
                        for(int y = nRasterYSize - nBlockYOff * nBlockYSize; y < nBlockYSize; y++)
                        {
                            for(int x=0;x<nBlockXSize;x++)
                            {
                                if( pabyDest[y*nBlockXSize+x] != 0 && !bFoundNonZero )
                                {
                                    CPLDebug("GPKG", "IReadBlock(): Found non-zero content in ghost part of tile(nBand=%d,nBlockXOff=%d,nBlockYOff=%d,m_nZoomLevel=%d)\n",
                                            iBand,nBlockXOff,nBlockYOff,m_poTPD->m_nZoomLevel);
                                    bFoundNonZero = true;
                                }
                            }
                        }
                    }
#endif

                }
                else
                {
                    int nSrcXOffset;
                    int nSrcXSize;
                    int nSrcYOffset;
                    int nSrcYSize;
                    int nDstXOffset;
                    int nDstYOffset;
                    if( nCol == nColMin )
                    {
                        nSrcXOffset = m_poTPD->m_nShiftXPixelsMod;
                        nSrcXSize = nBlockXSize - m_poTPD->m_nShiftXPixelsMod;
                        nDstXOffset = 0;
                    }
                    else
                    {
                        nSrcXOffset = 0;
                        nSrcXSize = m_poTPD->m_nShiftXPixelsMod;
                        nDstXOffset = nBlockXSize - m_poTPD->m_nShiftXPixelsMod;
                    }
                    if( nRow == nRowMin )
                    {
                        nSrcYOffset = m_poTPD->m_nShiftYPixelsMod;
                        nSrcYSize = nBlockYSize - m_poTPD->m_nShiftYPixelsMod;
                        nDstYOffset = 0;
                    }
                    else
                    {
                        nSrcYOffset = 0;
                        nSrcYSize = m_poTPD->m_nShiftYPixelsMod;
                        nDstYOffset = nBlockYSize - m_poTPD->m_nShiftYPixelsMod;
                    }

#ifdef DEBUG_VERBOSE
                    CPLDebug( "GPKG",
                              "Copy source tile x=%d,w=%d,y=%d,h=%d into "
                              "buffer at x=%d,y=%d",
                              nSrcXOffset, nSrcXSize, nSrcYOffset, nSrcYSize,
                              nDstXOffset, nDstYOffset);
#endif

                    for( int y=0; y<nSrcYSize; y++ )
                    {
                        GByte *pSrc =
                          pabyTileData + (iBand - 1) * nBlockXSize * nBlockYSize
                          + (y + nSrcYOffset) * nBlockXSize + nSrcXOffset;
                        GByte *pDst =
                          pabyDest + (y + nDstYOffset) * nBlockXSize
                          + nDstXOffset;
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

static bool WEBPSupports4Bands()
{
    static int bRes = -1;
    if( bRes < 0 )
    {
        GDALDriver* poDrv = (GDALDriver*) GDALGetDriverByName("WEBP");
        if( poDrv == NULL || CPLTestBool(CPLGetConfigOption("GPKG_SIMUL_WEBP_3BAND", "FALSE")) )
            bRes = false;
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
    return CPL_TO_BOOL(bRes);
}

/************************************************************************/
/*                         WriteTile()                                  */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikePseudoDataset::WriteTile()
{
    CPLAssert(!m_bInWriteTile);
    m_bInWriteTile = true;
    CPLErr eErr = WriteTileInternal();
    m_bInWriteTile = false;
    return eErr;
}

/* should only be called by WriteTile() */
CPLErr GDALGPKGMBTilesLikePseudoDataset::WriteTileInternal()
{
    if( !(IGetUpdate() && m_asCachedTilesDesc[0].nRow >= 0 &&
          m_asCachedTilesDesc[0].nCol >= 0 &&
          m_asCachedTilesDesc[0].nIdxWithinTileData == 0) )
        return CE_None;

    int nRow = m_asCachedTilesDesc[0].nRow;
    int nCol = m_asCachedTilesDesc[0].nCol;

    bool bAllDirty = true;
    bool bAllNonDirty = true;
    const int nBands = IGetRasterCount();
    for( int i = 0; i < nBands; i++ )
    {
        if( m_asCachedTilesDesc[0].abBandDirty[i] )
            bAllNonDirty = false;
        else
            bAllDirty = false;
    }
    if( bAllNonDirty )
        return CE_None;

    int nBlockXSize, nBlockYSize;
    IGetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

    /* If all bands for that block are not dirty/written, we need to */
    /* fetch the missing ones if the tile exists */
    bool bIsLossyFormat = false;
    if( !bAllDirty )
    {
        for( int i = 1; i <= 3; i++ )
        {
            m_asCachedTilesDesc[i].nRow = -1;
            m_asCachedTilesDesc[i].nCol = -1;
            m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
        }
        ReadTile(nRow, nCol, m_pabyCachedTiles + 4 * nBlockXSize * nBlockYSize,
                 &bIsLossyFormat);
        for( int i = 0; i < nBands; i++ )
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
    const int nRasterXSize = IGetRasterBand(1)->GetXSize();
    const int nRasterYSize = IGetRasterBand(1)->GetYSize();
    if( nXOff >= nRasterXSize || nYOff >= nRasterYSize )
        return CE_None;

#ifdef DEBUG_VERBOSE
    if( m_nShiftXPixelsMod == 0 && m_nShiftYPixelsMod == 0 )
    {
        int nBlockXOff = nCol;
        int nBlockYOff = nRow;
        if( (nBlockXOff+1) * nBlockXSize <= nRasterXSize &&
            (nBlockYOff+1) * nBlockYSize > nRasterYSize )
        {
            for(int i = 0; i < nBands; i++ )
            {
                bool bFoundNonZero = false;
                for(int y = nRasterYSize - nBlockYOff * nBlockYSize; y < nBlockYSize; y++)
                {
                    for(int x=0;x<nBlockXSize;x++)
                    {
                        if( m_pabyCachedTiles[y*nBlockXSize+x + i * nBlockXSize * nBlockYSize] != 0 && !bFoundNonZero )
                        {
                            CPLDebug("GPKG", "WriteTileInternal(): Found non-zero content in ghost part of tile(band=%d,nBlockXOff=%d,nBlockYOff=%d,m_nZoomLevel=%d)\n",
                                    i+1,nBlockXOff,nBlockYOff,m_nZoomLevel);
                            bFoundNonZero = true;
                        }
                    }
                }
            }
        }
    }
#endif

    /* Validity area of tile data in intra-tile coordinate space */
    int iXOff = 0;
    int iYOff = 0;
    int iXCount = nBlockXSize;
    int iYCount = nBlockYSize;

    bool bPartialTile = false;
    int nAlphaBand = (nBands == 2) ? 2 : (nBands == 4) ? 4 : 0;
    if( nAlphaBand == 0 )
    {
        if( nXOff < 0 )
        {
            bPartialTile = true;
            iXOff = -nXOff;
            iXCount += nXOff;
        }
        if( nXOff + nBlockXSize > nRasterXSize )
        {
            bPartialTile = true;
            iXCount -= nXOff + nBlockXSize - nRasterXSize;
        }
        if( nYOff < 0 )
        {
            bPartialTile = true;
            iYOff = -nYOff;
            iYCount += nYOff;
        }
        if( nYOff + nBlockYSize > nRasterYSize )
        {
            bPartialTile = true;
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
    m_asCachedTilesDesc[0].abBandDirty[0] = false;
    m_asCachedTilesDesc[0].abBandDirty[1] = false;
    m_asCachedTilesDesc[0].abBandDirty[2] = false;
    m_asCachedTilesDesc[0].abBandDirty[3] = false;

    CPLErr eErr = CE_Failure;

    bool bAllOpaque = true;
    if( m_poCT == NULL && nAlphaBand != 0 )
    {
        GByte byFirstAlphaVal =  m_pabyCachedTiles[(nAlphaBand-1) * nBlockXSize * nBlockYSize];
        int i = 1;
        for( ; i < nBlockXSize * nBlockYSize; i++ )
        {
            if( m_pabyCachedTiles[(nAlphaBand-1) * nBlockXSize * nBlockYSize + i] != byFirstAlphaVal )
                break;
        }
        if( i == nBlockXSize * nBlockYSize )
        {
            // If tile is fully transparent, don't serialize it and remove it if it exists
            if( byFirstAlphaVal == 0 )
            {
                char* pszSQL = sqlite3_mprintf("DELETE FROM '%q' "
                    "WHERE zoom_level = %d AND tile_row = %d AND tile_column = %d",
                    m_osRasterTable.c_str(), m_nZoomLevel, GetRowFromIntoTopConvention(nRow), nCol);
#ifdef DEBUG_VERBOSE
                CPLDebug("GPKG", "%s", pszSQL);
#endif
                char* pszErrMsg = NULL;
                int rc = sqlite3_exec(IGetDB(), pszSQL, NULL, NULL, &pszErrMsg);
                if( rc != SQLITE_OK )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Failure when deleting tile (row=%d,col=%d) at zoom_level=%d : %s",
                            GetRowFromIntoTopConvention(nRow), nCol, m_nZoomLevel, pszErrMsg ? pszErrMsg : "");
                }
                sqlite3_free(pszSQL);
                sqlite3_free(pszErrMsg);
                return CE_None;
            }
            bAllOpaque = (byFirstAlphaVal == 255);
        }
        else
            bAllOpaque = false;
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
    bool bTileDriverSupports1Band = false;
    bool bTileDriverSupports2Bands = false;
    bool bTileDriverSupports4Bands = false;
    bool bTileDriverSupportsCT = false;

    if( nBands == 1 )
        IGetRasterBand(1)->GetColorTable();

    if( m_eTF == GPKG_TF_PNG_JPEG )
    {
        bTileDriverSupports1Band = true;
        if( bPartialTile || (nBands == 2 && !bAllOpaque) || (nBands == 4 && !bAllOpaque) || m_poCT != NULL )
        {
            pszDriverName = "PNG";
            bTileDriverSupports2Bands = m_bPNGSupports2Bands;
            bTileDriverSupports4Bands = true;
            bTileDriverSupportsCT = m_bPNGSupportsCT;
        }
        else
            pszDriverName = "JPEG";
    }
    else if( m_eTF == GPKG_TF_PNG ||
             m_eTF == GPKG_TF_PNG8 )
    {
        pszDriverName = "PNG";
        bTileDriverSupports1Band = true;
        bTileDriverSupports2Bands = m_bPNGSupports2Bands;
        bTileDriverSupports4Bands = true;
        bTileDriverSupportsCT = m_bPNGSupportsCT;
    }
    else if( m_eTF == GPKG_TF_JPEG )
    {
        pszDriverName = "JPEG";
        bTileDriverSupports1Band = true;
    }
    else if( m_eTF == GPKG_TF_WEBP )
    {
        pszDriverName = "WEBP";
        bTileDriverSupports4Bands = WEBPSupports4Bands();
    }
    else
    {
        CPLAssert(0);
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
                for( int i = 0; i < m_poCT->GetColorEntryCount(); i++ )
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

        for( int i = 0; i < nTileBands; i++ )
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
            for( int i = 0; i < 3; i++ )
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
            for( int i = 0; i < nEntries; i++ )
            {
                const GDALColorEntry* psEntry = m_poCT->GetColorEntry(i);
                abyCT[4*i] = (GByte)psEntry->c1;
                abyCT[4*i+1] = (GByte)psEntry->c2;
                abyCT[4*i+2] = (GByte)psEntry->c3;
                abyCT[4*i+3] = (GByte)psEntry->c4;
            }
            for( int i = nEntries; i<256 ;i++ )
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
            int i;  // TODO: Rename the variable to make it clean what it is.
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
            GDALGPKGMBTilesLikePseudoDataset* poMainDS = m_poParentDS ? m_poParentDS : this;
            if( poMainDS->m_nTileInsertionCount == 0 )
            {
                poMainDS->IStartTransaction();
            }
            else if( poMainDS->m_nTileInsertionCount == 1000 )
            {
                poMainDS->ICommitTransaction();
                poMainDS->IStartTransaction();
                poMainDS->m_nTileInsertionCount = 0;
            }
            poMainDS->m_nTileInsertionCount ++;

            char* pszSQL = sqlite3_mprintf("INSERT OR REPLACE INTO '%q' "
                "(zoom_level, tile_row, tile_column, tile_data) VALUES (%d, %d, %d, ?)",
                m_osRasterTable.c_str(), m_nZoomLevel, GetRowFromIntoTopConvention(nRow), nCol);
#ifdef DEBUG_VERBOSE
            CPLDebug("GPKG", "%s", pszSQL);
#endif
            sqlite3_stmt* hStmt = NULL;
            int rc = sqlite3_prepare(IGetDB(), pszSQL, -1, &hStmt, NULL);
            if ( rc != SQLITE_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "failed to prepare SQL %s: %s",
                          pszSQL, sqlite3_errmsg(IGetDB()) );
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
                             GetRowFromIntoTopConvention(nRow), nCol, m_nZoomLevel, sqlite3_errmsg(IGetDB()));
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
/*                     FlushRemainingShiftedTiles()                     */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikePseudoDataset::FlushRemainingShiftedTiles(bool bPartialFlush)
{
    if( m_hTempDB == NULL )
        return CE_None;

    for(int i=0;i<=3;i++)
    {
        m_asCachedTilesDesc[i].nRow = -1;
        m_asCachedTilesDesc[i].nCol = -1;
        m_asCachedTilesDesc[i].nIdxWithinTileData = -1;
    }

    int nBlockXSize, nBlockYSize;
    IGetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    const int nBands = IGetRasterCount();
    const int nRasterXSize = IGetRasterBand(1)->GetXSize();
    const int nRasterYSize = IGetRasterBand(1)->GetYSize();
    const int nXBlocks = DIV_ROUND_UP( nRasterXSize , nBlockXSize );
    const int nYBlocks = DIV_ROUND_UP( nRasterYSize , nBlockYSize );

    int nPartialActiveTiles = 0;
    if( bPartialFlush )
    {
        sqlite3_stmt* hStmt = NULL;
        CPLString osSQL;
        osSQL.Printf("SELECT COUNT(*) FROM partial_tiles WHERE zoom_level = %d AND partial_flag != 0", m_nZoomLevel);
        if( sqlite3_prepare_v2(m_hTempDB, osSQL.c_str(), -1, &hStmt, NULL) == SQLITE_OK )
        {
            if( sqlite3_step(hStmt) == SQLITE_ROW )
            {
                nPartialActiveTiles = sqlite3_column_int(hStmt, 0);
                CPLDebug("GPKG", "Active partial tiles before flush: %d", nPartialActiveTiles);
            }
            sqlite3_finalize(hStmt);
        }
    }

    CPLString osSQL = "SELECT tile_row, tile_column, partial_flag";
    for(int nBand = 1; nBand <= nBands; nBand++ )
    {
        osSQL += CPLSPrintf(", tile_data_band_%d", nBand);
    }
    osSQL += CPLSPrintf(" FROM partial_tiles WHERE "
                        "zoom_level = %d AND partial_flag != 0",
                        m_nZoomLevel);
    if( bPartialFlush )
    {
        osSQL += " ORDER BY age";
    }
    const char* pszSQL = osSQL.c_str();

#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "%s", pszSQL);
#endif
    sqlite3_stmt* hStmt = NULL;
    int rc = sqlite3_prepare_v2(m_hTempDB, pszSQL, -1, &hStmt, NULL);
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_prepare(%s) failed: %s",
                  pszSQL, sqlite3_errmsg( m_hTempDB ) );
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    bool bGotPartialTiles = false;
    int nCountFlushedTiles = 0;
    do
    {
        rc = sqlite3_step(hStmt);
        if ( rc == SQLITE_ROW )
        {
            bGotPartialTiles = true;

            int nRow = sqlite3_column_int(hStmt, 0);
            int nCol = sqlite3_column_int(hStmt, 1);
            int nPartialFlags = sqlite3_column_int(hStmt, 2);

            if( bPartialFlush )
            {
                // This method assumes that there are no dirty blocks alive
                // so check this assumption.
                // When called with bPartialFlush = false, FlushCache() has already
                // been called, so no need to check.
                bool bFoundDirtyBlock = false;
                int nBlockXOff = nCol - m_nShiftXTiles;
                int nBlockYOff = nRow - m_nShiftYTiles;
                for( int iX = 0; !bFoundDirtyBlock && iX < (( m_nShiftXPixelsMod != 0 ) ? 2 : 1); iX ++ )
                {
                    if( nBlockXOff + iX < 0 || nBlockXOff + iX >= nXBlocks )
                        continue;
                    for( int iY = 0; !bFoundDirtyBlock && iY < (( m_nShiftYPixelsMod != 0 ) ? 2 : 1); iY ++ )
                    {
                        if( nBlockYOff + iY < 0 || nBlockYOff + iY >= nYBlocks )
                            continue;
                        for( int iBand = 1; !bFoundDirtyBlock && iBand <= nBands; iBand ++ )
                        {
                            GDALRasterBlock* poBlock =
                                        ((GDALGPKGMBTilesLikeRasterBand*)IGetRasterBand(iBand))->
                                                    AccessibleTryGetLockedBlockRef(nBlockXOff + iX, nBlockYOff + iY);
                            if( poBlock )
                            {
                                if( poBlock->GetDirty() )
                                    bFoundDirtyBlock = true;
                                poBlock->DropLock();
                            }
                        }
                    }
                }
                if( bFoundDirtyBlock )
                {
#ifdef DEBUG_VERBOSE
                    CPLDebug("GPKG", "Skipped flushing tile row = %d, column = %d because it has dirty block(s) in GDAL cache",
                             nRow, nCol);
#endif
                    continue;
                }
            }

            nCountFlushedTiles ++;
            if( bPartialFlush && nCountFlushedTiles >= nPartialActiveTiles / 2 )
            {
                CPLDebug("GPKG", "Flushed %d tiles", nCountFlushedTiles);
                break;
            }

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
                        m_osRasterTable.c_str(), m_nZoomLevel, GetRowFromIntoTopConvention(nRow), nCol,
                        m_osWHERE.size() ? CPLSPrintf(" AND (%s)", m_osWHERE.c_str()): "");
#ifdef DEBUG_VERBOSE
                CPLDebug("GPKG", "%s", pszNewSQL);
#endif
                sqlite3_stmt* hNewStmt = NULL;
                rc = sqlite3_prepare(IGetDB(), pszNewSQL, -1, &hNewStmt, NULL);
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

                        ReadTile(osMemFileName,
                                 m_pabyCachedTiles + 4 * nBlockXSize * nBlockYSize);
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
                    else if( rc != SQLITE_DONE )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_step(%s) failed: %s",
                                  pszNewSQL, sqlite3_errmsg( m_hTempDB ) );
                    }
                    sqlite3_finalize(hNewStmt);
                }
                else
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_prepare(%s) failed: %s",
                              pszNewSQL, sqlite3_errmsg( m_hTempDB ) );
                }
                sqlite3_free(pszNewSQL);
            }

            m_asCachedTilesDesc[0].nRow = nRow;
            m_asCachedTilesDesc[0].nCol = nCol;
            m_asCachedTilesDesc[0].nIdxWithinTileData = 0;
            m_asCachedTilesDesc[0].abBandDirty[0] = true;
            m_asCachedTilesDesc[0].abBandDirty[1] = true;
            m_asCachedTilesDesc[0].abBandDirty[2] = true;
            m_asCachedTilesDesc[0].abBandDirty[3] = true;

            eErr = WriteTile();

            if( eErr == CE_None && bPartialFlush )
            {
                pszSQL = CPLSPrintf("DELETE FROM partial_tiles WHERE zoom_level = %d AND tile_row = %d AND tile_column = %d",
                                    m_nZoomLevel, nRow, nCol);
#ifdef DEBUG_VERBOSE
                CPLDebug("GPKG", "%s", pszSQL);
#endif
                if( SQLCommand(m_hTempDB, pszSQL) != OGRERR_NONE )
                    eErr = CE_None;
            }
        }
        else
        {
            if( rc != SQLITE_DONE )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_step(%s) failed: %s",
                          pszSQL, sqlite3_errmsg( m_hTempDB ) );
            }
            break;
        }
    }
    while( eErr == CE_None);

    sqlite3_finalize(hStmt);

    if( bPartialFlush && nCountFlushedTiles < nPartialActiveTiles / 2 )
    {
        CPLDebug("GPKG", "Flushed %d tiles. Target was %d", nCountFlushedTiles, nPartialActiveTiles / 2);
    }

    if( bGotPartialTiles && !bPartialFlush )
    {
#ifdef DEBUG_VERBOSE
        pszSQL = CPLSPrintf("SELECT p1.id, p1.tile_row, p1.tile_column FROM partial_tiles p1, partial_tiles p2 "
                            "WHERE p1.zoom_level = %d AND p2.zoom_level = %d AND p1.tile_row = p2.tile_row AND p1.tile_column = p2.tile_column AND p2.partial_flag != 0",
                            -1-m_nZoomLevel, m_nZoomLevel);
        rc = sqlite3_prepare_v2(m_hTempDB, pszSQL, -1, &hStmt, NULL);
        CPLAssert( rc == SQLITE_OK );
        while( (rc = sqlite3_step(hStmt)) == SQLITE_ROW )
        {
            CPLDebug("GPKG", "Conflict: existing id = %d, tile_row = %d, tile_column = %d, zoom_level = %d",
                     sqlite3_column_int(hStmt, 0),
                     sqlite3_column_int(hStmt, 1),
                     sqlite3_column_int(hStmt, 2),
                     m_nZoomLevel);
        }
        sqlite3_finalize(hStmt);
#endif

        pszSQL = CPLSPrintf("UPDATE partial_tiles SET zoom_level = %d, "
                            "partial_flag = 0, age = -1 WHERE zoom_level = %d AND partial_flag != 0",
                            -1-m_nZoomLevel, m_nZoomLevel);
#ifdef DEBUG_VERBOSE
        CPLDebug("GPKG", "%s", pszSQL);
#endif
        SQLCommand(m_hTempDB, pszSQL);
    }

    return eErr;
}

/************************************************************************/
/*                DoPartialFlushOfPartialTilesIfNecessary()             */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikePseudoDataset::DoPartialFlushOfPartialTilesIfNecessary()
{
    time_t nCurTimeStamp = time(NULL);
    if( m_nLastSpaceCheckTimestamp == 0 )
        m_nLastSpaceCheckTimestamp = nCurTimeStamp;
    if( m_nLastSpaceCheckTimestamp > 0 &&
        (m_bForceTempDBCompaction || nCurTimeStamp - m_nLastSpaceCheckTimestamp > 10) )
    {
        m_nLastSpaceCheckTimestamp = nCurTimeStamp;
        GIntBig nFreeSpace = VSIGetDiskFreeSpace( CPLGetDirname(m_osTempDBFilename) );
        bool bTryFreeing = false;
        if( nFreeSpace >= 0 && nFreeSpace < 1024 * 1024 * 1024 )
        {
            CPLDebug("GPKG", "Free space below 1GB. Flushing part of partial tiles");
            bTryFreeing = true;
        }
        else
        {
            VSIStatBufL sStat;
            if( VSIStatL( m_osTempDBFilename, &sStat ) == 0 )
            {
                GIntBig nTempSpace = sStat.st_size;
                if( VSIStatL( (m_osTempDBFilename + "-journal").c_str(), &sStat ) == 0 )
                  nTempSpace += sStat.st_size;
                else if( VSIStatL( (m_osTempDBFilename + "-wal").c_str(), &sStat ) == 0 )
                  nTempSpace += sStat.st_size;

                int nBlockXSize, nBlockYSize;
                IGetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
                const int nBands = IGetRasterCount();

                if( nTempSpace > 4 * static_cast<GIntBig>(IGetRasterBand(1)->GetXSize())  * nBlockYSize * nBands )
                {
                    CPLDebug("GPKG", "Partial tiles DB is " CPL_FRMT_GIB " bytes. Flushing part of partial tiles",
                             nTempSpace);
                    bTryFreeing = true;
                }
            }
        }
        if( bTryFreeing )
        {
            if( FlushRemainingShiftedTiles(true /* partial flush*/) != CE_None )
            {
                return CE_Failure;
            }
            SQLCommand(m_hTempDB, "DELETE FROM partial_tiles WHERE zoom_level < 0");
            SQLCommand(m_hTempDB, "VACUUM");
        }
    }
    return CE_None;
}

/************************************************************************/
/*                         WriteShiftedTile()                           */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikePseudoDataset::WriteShiftedTile(int nRow, int nCol, int nBand,
                                               int nDstXOffset, int nDstYOffset,
                                               int nDstXSize, int nDstYSize)
{
    CPLAssert( m_nShiftXPixelsMod || m_nShiftYPixelsMod );
    CPLAssert( nRow >= 0 );
    CPLAssert( nCol >= 0 );
    CPLAssert( nRow < m_nTileMatrixHeight );
    CPLAssert( nCol < m_nTileMatrixWidth );

    if( m_hTempDB == NULL &&
        (m_poParentDS == NULL || m_poParentDS->m_hTempDB == NULL) )
    {
        const char* pszBaseFilename = m_poParentDS ?
                m_poParentDS->IGetFilename() : IGetFilename();
        m_osTempDBFilename = CPLResetExtension(pszBaseFilename, "partial_tiles.db");
        CPLPushErrorHandler(CPLQuietErrorHandler);
        VSIUnlink(m_osTempDBFilename);
        CPLPopErrorHandler();
        m_hTempDB = NULL;
        int rc;
#ifdef HAVE_SQLITE_VFS
        if (STARTS_WITH(m_osTempDBFilename, "/vsi"))
        {
            m_pMyVFS = OGRSQLiteCreateVFS(NULL, NULL);
            sqlite3_vfs_register(m_pMyVFS, 0);
            rc = sqlite3_open_v2( m_osTempDBFilename, &m_hTempDB,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, m_pMyVFS->zName );
        }
        else
#endif
        {
            rc = sqlite3_open(m_osTempDBFilename, &m_hTempDB);
        }
        if( rc != SQLITE_OK || m_hTempDB == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot create temporary database %s",
                        m_osTempDBFilename.c_str());
            return CE_Failure;
        }
        SQLCommand(m_hTempDB, "PRAGMA synchronous = OFF");
        /* coverity[tainted_string] */
        SQLCommand(m_hTempDB, (CPLString("PRAGMA journal_mode = ") + CPLGetConfigOption("PARTIAL_TILES_JOURNAL_MODE", "OFF")).c_str());
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
                                    "age INTEGER NOT NULL,"
                                    "UNIQUE (zoom_level, tile_column, tile_row))" );
        SQLCommand(m_hTempDB, "CREATE INDEX partial_tiles_partial_flag_idx "
                                "ON partial_tiles(partial_flag)");
        SQLCommand(m_hTempDB, "CREATE INDEX partial_tiles_age_idx "
                                "ON partial_tiles(age)");

        if( m_poParentDS != NULL )
        {
            m_poParentDS->m_osTempDBFilename = m_osTempDBFilename;
            m_poParentDS->m_hTempDB = m_hTempDB;
        }
    }

    if( m_poParentDS != NULL )
        m_hTempDB = m_poParentDS->m_hTempDB;

    int nBlockXSize, nBlockYSize;
    IGetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    const int nBands = IGetRasterCount();

    int iQuadrantFlag = 0;
    if( nDstXOffset == 0 && nDstYOffset == 0 )
        iQuadrantFlag |= (1 << 0);
    if( nDstXOffset + nDstXSize == nBlockXSize && nDstYOffset == 0  )
        iQuadrantFlag |= (1 << 1);
    if( nDstXOffset == 0 && nDstYOffset + nDstYSize == nBlockYSize )
        iQuadrantFlag |= (1 << 2);
    if( nDstXOffset + nDstXSize == nBlockXSize && nDstYOffset + nDstYSize == nBlockYSize )
        iQuadrantFlag |= (1 << 3);
    int l_nFlags = iQuadrantFlag << (4 * (nBand - 1));
    int nFullFlags = (1 << (4 * nBands)) - 1;
    int nOldFlags = 0;

    for(int i=1;i<=3;i++)
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
    int rc = sqlite3_prepare_v2(m_hTempDB, pszSQL, -1, &hStmt, NULL);
    if ( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_prepare(%s) failed: %s",
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

    if( (nOldFlags & l_nFlags) != 0 )
    {
        CPLDebug("GPKG",
                 "Rewriting quadrant %d of band %d of tile (row=%d,col=%d)",
                 iQuadrantFlag, nBand, nRow, nCol);
    }

    l_nFlags |= nOldFlags;
    if( l_nFlags == nFullFlags )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("GPKG", "Got all quadrants for that tile");
#endif
        for( int iBand = 1; iBand <= nBands; iBand ++ )
        {
            if( iBand != nBand )
            {
                pszSQL = CPLSPrintf("SELECT tile_data_band_%d FROM partial_tiles WHERE "
                                    "id = %d", iBand, nExistingId);
#ifdef DEBUG_VERBOSE
                CPLDebug("GPKG", "%s", pszSQL);
#endif
                hStmt = NULL;
                rc = sqlite3_prepare_v2(m_hTempDB, pszSQL, -1, &hStmt, NULL);
                if ( rc != SQLITE_OK )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "sqlite3_prepare(%s) failed: %s",
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
        m_asCachedTilesDesc[0].abBandDirty[0] = true;
        m_asCachedTilesDesc[0].abBandDirty[1] = true;
        m_asCachedTilesDesc[0].abBandDirty[2] = true;
        m_asCachedTilesDesc[0].abBandDirty[3] = true;

        pszSQL = CPLSPrintf("UPDATE partial_tiles SET zoom_level = %d, "
                            "partial_flag = 0, age = -1 WHERE id = %d",
                            -1-m_nZoomLevel, nExistingId);
        SQLCommand(m_hTempDB, pszSQL);
#ifdef DEBUG_VERBOSE
        CPLDebug("GPKG", "%s", pszSQL);
#endif
        CPLErr eErr = WriteTile();

        // Call DoPartialFlushOfPartialTilesIfNecessary() after using m_pabyCachedTiles
        // as it is going to mess with it.
        if( DoPartialFlushOfPartialTilesIfNecessary() != CE_None )
            eErr = CE_None;

        return eErr;
    }

    if( nExistingId == 0 )
    {
        OGRErr err;
        pszSQL = CPLSPrintf("SELECT id FROM partial_tiles WHERE "
                            "partial_flag = 0 AND zoom_level = %d "
                            "AND tile_row = %d AND tile_column = %d",
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

    const GIntBig nAge = ( m_poParentDS ) ? m_poParentDS->m_nAge : m_nAge;
    if( nExistingId == 0 )
    {
        pszSQL = CPLSPrintf("INSERT INTO partial_tiles "
                "(zoom_level, tile_row, tile_column, tile_data_band_%d, partial_flag, age) VALUES (%d, %d, %d, ?, %d, " CPL_FRMT_GIB ")",
                nBand, m_nZoomLevel, nRow, nCol, l_nFlags, nAge);
    }
    else
    {
        pszSQL = CPLSPrintf("UPDATE partial_tiles SET zoom_level = %d, "
                            "tile_row = %d, tile_column = %d, "
                            "tile_data_band_%d = ?, partial_flag = %d, age = " CPL_FRMT_GIB " WHERE id = %d",
                            m_nZoomLevel, nRow, nCol, nBand, l_nFlags, nAge, nExistingId);
    }
    if ( m_poParentDS )
        m_poParentDS->m_nAge ++;
    else
        m_nAge ++;

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

    // Call DoPartialFlushOfPartialTilesIfNecessary() after using m_pabyCachedTiles
    // as it is going to mess with it.
    if( DoPartialFlushOfPartialTilesIfNecessary() != CE_None )
          eErr = CE_None;

    return eErr;
}

/************************************************************************/
/*                         IWriteBlock()                                */
/************************************************************************/

CPLErr GDALGPKGMBTilesLikeRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                             void* pData)
{
#ifdef DEBUG_VERBOSE
    CPLDebug("GPKG", "IWriteBlock(nBand=%d,nBlockXOff=%d,nBlockYOff=%d,m_nZoomLevel=%d)",
             nBand,nBlockXOff,nBlockYOff,m_poTPD->m_nZoomLevel);
#endif

    if( !m_poTPD->ICanIWriteBlock() )
    {
        return CE_Failure;
    }
    if( m_poTPD->m_poParentDS )
        m_poTPD->m_poParentDS->m_bHasModifiedTiles = true;
    else
        m_poTPD->m_bHasModifiedTiles = true;

    int nRow = nBlockYOff + m_poTPD->m_nShiftYTiles;
    int nCol = nBlockXOff + m_poTPD->m_nShiftXTiles;

    int nRowMin = nRow;
    int nRowMax = nRowMin;
    if( m_poTPD->m_nShiftYPixelsMod )
        nRowMax ++;

    int nColMin = nCol;
    int nColMax = nColMin;
    if( m_poTPD->m_nShiftXPixelsMod )
        nColMax ++;

    CPLErr eErr = CE_None;

    for(nRow = nRowMin; eErr == CE_None && nRow <= nRowMax; nRow ++)
    {
        for(nCol = nColMin; eErr == CE_None && nCol <= nColMax; nCol++ )
        {
            if( nRow < 0 || nCol < 0 || nRow >= m_poTPD->m_nTileMatrixHeight ||
                nCol >= m_poTPD->m_nTileMatrixWidth )
            {
                continue;
            }

            if( m_poTPD->m_nShiftXPixelsMod == 0 && m_poTPD->m_nShiftYPixelsMod == 0 )
            {
                if( !(nRow == m_poTPD->m_asCachedTilesDesc[0].nRow &&
                    nCol == m_poTPD->m_asCachedTilesDesc[0].nCol &&
                    m_poTPD->m_asCachedTilesDesc[0].nIdxWithinTileData == 0) )
                {
                    eErr = m_poTPD->WriteTile();

                    m_poTPD->m_asCachedTilesDesc[0].nRow = nRow;
                    m_poTPD->m_asCachedTilesDesc[0].nCol = nCol;
                    m_poTPD->m_asCachedTilesDesc[0].nIdxWithinTileData = 0;
                }
            }

            // Composite block data into tile, and check if all bands for this block
            // are dirty, and if so write the tile
            bool bAllDirty = true;
            for(int iBand=1;iBand<=poDS->GetRasterCount();iBand++)
            {
                GDALRasterBlock* poBlock = NULL;
                GByte* pabySrc = NULL;
                if( iBand == nBand )
                {
                    pabySrc = (GByte*)pData;
                }
                else
                {
                    if( !(m_poTPD->m_nShiftXPixelsMod == 0 && m_poTPD->m_nShiftYPixelsMod == 0) )
                        continue;

                    // If the block for this band is not dirty, it might be dirty in cache
                    if( m_poTPD->m_asCachedTilesDesc[0].abBandDirty[iBand-1] )
                        continue;
                    else
                    {
                        poBlock =
                            ((GDALGPKGMBTilesLikeRasterBand*)poDS->GetRasterBand(iBand))->
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
                            bAllDirty = false;
                            continue;
                        }
                    }
                }

                if( m_poTPD->m_nShiftXPixelsMod == 0 && m_poTPD->m_nShiftYPixelsMod == 0 )
                    m_poTPD->m_asCachedTilesDesc[0].abBandDirty[iBand - 1] = true;

                int nDstXOffset = 0, nDstXSize = nBlockXSize,
                    nDstYOffset = 0, nDstYSize = nBlockYSize;
                int nSrcXOffset = 0, nSrcYOffset = 0;
                // Composite block data into tile data
                if( m_poTPD->m_nShiftXPixelsMod == 0 && m_poTPD->m_nShiftYPixelsMod == 0 )
                {

#ifdef DEBUG_VERBOSE
                    if( (nBlockXOff+1) * nBlockXSize <= nRasterXSize &&
                        (nBlockYOff+1) * nBlockYSize > nRasterYSize )
                    {
                        bool bFoundNonZero = false;
                        for(int y = nRasterYSize - nBlockYOff * nBlockYSize; y < nBlockYSize; y++)
                        {
                            for(int x=0;x<nBlockXSize;x++)
                            {
                                if( pabySrc[y*nBlockXSize+x] != 0 && !bFoundNonZero )
                                {
                                    CPLDebug("GPKG", "IWriteBlock(): Found non-zero content in ghost part of tile(nBand=%d,nBlockXOff=%d,nBlockYOff=%d,m_nZoomLevel=%d)\n",
                                            iBand,nBlockXOff,nBlockYOff,m_poTPD->m_nZoomLevel);
                                    bFoundNonZero = true;
                                }
                            }
                        }
                    }
#endif

                    memcpy( m_poTPD->m_pabyCachedTiles + (iBand - 1) * nBlockXSize * nBlockYSize,
                            pabySrc, nBlockXSize * nBlockYSize );

                    // Make sure partial blocks are zero'ed outside of the validity area
                    // but do that only when know that JPEG will not be used so as to
                    // avoid edge effects (although we should probably repeat last pixels
                    // if we really want to do that, but that only makes sense if readers
                    // only clip to the gpkg_contents extent). Well, ere on the safe side for now
                    if( m_poTPD->m_eTF != GPKG_TF_JPEG &&
                        ((nBlockXOff+1) * nBlockXSize >= nRasterXSize ||
                         (nBlockYOff+1) * nBlockYSize >= nRasterYSize) )
                    {
                        int nXEndValidity = nRasterXSize - nBlockXOff * nBlockXSize;
                        if( nXEndValidity > nBlockXSize )
                            nXEndValidity = nBlockXSize;
                        int nYEndValidity = nRasterYSize - nBlockYOff * nBlockYSize;
                        if( nYEndValidity > nBlockYSize )
                            nYEndValidity = nBlockYSize;
                        if( nXEndValidity < nBlockXSize )
                        {
                            for( int iY = 0; iY < nYEndValidity; iY++ )
                            {
                                memset( m_poTPD->m_pabyCachedTiles + ((iBand - 1) * nBlockYSize + iY) * nBlockXSize + nXEndValidity,
                                        0,
                                        nBlockXSize - nXEndValidity );
                            }
                        }
                        if( nYEndValidity < nBlockYSize )
                        {
                            memset( m_poTPD->m_pabyCachedTiles + ((iBand - 1) * nBlockYSize + nYEndValidity) * nBlockXSize,
                                    0,
                                    (nBlockYSize - nYEndValidity) * nBlockXSize );
                        }
                    }

                }
                else
                {
                    if( nCol == nColMin )
                    {
                        nDstXOffset = m_poTPD->m_nShiftXPixelsMod;
                        nDstXSize = nBlockXSize - m_poTPD->m_nShiftXPixelsMod;
                        nSrcXOffset = 0;
                    }
                    else
                    {
                        nDstXOffset = 0;
                        nDstXSize = m_poTPD->m_nShiftXPixelsMod;
                        nSrcXOffset = nBlockXSize - m_poTPD->m_nShiftXPixelsMod;
                    }
                    if( nRow == nRowMin )
                    {
                        nDstYOffset = m_poTPD->m_nShiftYPixelsMod;
                        nDstYSize = nBlockYSize - m_poTPD->m_nShiftYPixelsMod;
                        nSrcYOffset = 0;
                    }
                    else
                    {
                        nDstYOffset = 0;
                        nDstYSize = m_poTPD->m_nShiftYPixelsMod;
                        nSrcYOffset = nBlockYSize - m_poTPD->m_nShiftYPixelsMod;
                    }

#ifdef DEBUG_VERBOSE
                    CPLDebug( "GPKG",
                              "Copy source tile x=%d,w=%d,y=%d,h=%d into "
                              "buffer at x=%d,y=%d",
                              nDstXOffset, nDstXSize, nDstYOffset, nDstYSize,
                              nSrcXOffset, nSrcYOffset);
#endif

                    for( int y=0; y<nDstYSize; y++ )
                    {
                        GByte* pDst =
                          m_poTPD->m_pabyCachedTiles +
                          (iBand - 1) * nBlockXSize * nBlockYSize +
                          (y + nDstYOffset) * nBlockXSize + nDstXOffset;
                        GByte* pSrc =
                            pabySrc + (y + nSrcYOffset) * nBlockXSize +
                            nSrcXOffset;
                        GDALCopyWords(pSrc, GDT_Byte, 1,
                                    pDst, GDT_Byte, 1,
                                    nDstXSize);
                    }
                }

                if( poBlock )
                    poBlock->DropLock();

                if( !(m_poTPD->m_nShiftXPixelsMod == 0
                      && m_poTPD->m_nShiftYPixelsMod == 0) )
                {
                    m_poTPD->m_asCachedTilesDesc[0].nRow = -1;
                    m_poTPD->m_asCachedTilesDesc[0].nCol = -1;
                    m_poTPD->m_asCachedTilesDesc[0].nIdxWithinTileData = -1;
                    eErr = m_poTPD->WriteShiftedTile(nRow, nCol, iBand,
                                                   nDstXOffset, nDstYOffset,
                                                   nDstXSize, nDstYSize);
                }
            }

            if( m_poTPD->m_nShiftXPixelsMod == 0
                && m_poTPD->m_nShiftYPixelsMod == 0 )
            {
                if( bAllDirty )
                {
                    eErr = m_poTPD->WriteTile();
                }
            }
        }
    }

    return eErr;
}

/************************************************************************/
/*                      GDALGeoPackageRasterBand()                      */
/************************************************************************/

GDALGeoPackageRasterBand::GDALGeoPackageRasterBand(
    GDALGeoPackageDataset* poDSIn, int nTileWidth, int nTileHeight) :
          GDALGPKGMBTilesLikeRasterBand(poDSIn, nTileWidth, nTileHeight)
{
    poDS = poDSIn;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int GDALGeoPackageRasterBand::GetOverviewCount()
{
    GDALGeoPackageDataset *poGDS
        = reinterpret_cast<GDALGeoPackageDataset *>( poDS );
    return poGDS->m_nOverviewCount;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

GDALRasterBand* GDALGeoPackageRasterBand::GetOverview(int nIdx)
{
    GDALGeoPackageDataset *poGDS
        = reinterpret_cast<GDALGeoPackageDataset *>( poDS );
    if( nIdx < 0 || nIdx >= poGDS->m_nOverviewCount )
        return NULL;
    return poGDS->m_papoOverviewDS[nIdx]->GetRasterBand(nBand);
}
