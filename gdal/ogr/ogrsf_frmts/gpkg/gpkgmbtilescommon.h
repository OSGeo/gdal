/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage/MBTiles Translator
 * Purpose:  Definition of common classes for GeoPackage and MBTiles drivers.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014-2016, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef GPKGMBTILESCOMMON_H_INCLUDED
#define GPKGMBTILESCOMMON_H_INCLUDED

#include "cpl_string.h"
#include "gdal_pam.h"
#include "ogr_sqlite.h" // for sqlite3*

typedef struct
{
    int     nRow;
    int     nCol;
    int     nIdxWithinTileData;
    bool    abBandDirty[4];
} CachedTileDesc;

typedef enum
{
    GPKG_TF_PNG_JPEG,
    GPKG_TF_PNG,
    GPKG_TF_PNG8,
    GPKG_TF_JPEG,
    GPKG_TF_WEBP
} GPKGTileFormat;

GPKGTileFormat GDALGPKGMBTilesGetTileFormat(const char* pszTF );

class GDALGPKGMBTilesLikePseudoDataset
{
    friend class GDALGPKGMBTilesLikeRasterBand;

  protected:
    bool                m_bNew;

    CPLString           m_osRasterTable;
    int                 m_nZoomLevel;
    GByte              *m_pabyCachedTiles;
    CachedTileDesc      m_asCachedTilesDesc[4];
    int                 m_nShiftXTiles;
    int                 m_nShiftXPixelsMod;
    int                 m_nShiftYTiles;
    int                 m_nShiftYPixelsMod;
    int                 m_nTileMatrixWidth;
    int                 m_nTileMatrixHeight;

    GPKGTileFormat      m_eTF;
    bool                m_bPNGSupports2Bands; // for test/debug purposes only. true is the nominal value
    bool                m_bPNGSupportsCT; // for test/debug purposes only. true is the nominal value
    int                 m_nZLevel;
    int                 m_nQuality;
    bool                m_bDither;

    GDALColorTable*     m_poCT;
    bool                m_bTriedEstablishingCT;
    GByte*              m_pabyHugeColorArray;

    CPLString           m_osWHERE;

#ifdef HAVE_SQLITE_VFS
    sqlite3_vfs*        m_pMyVFS;
#endif
    sqlite3            *m_hTempDB;
    CPLString           m_osTempDBFilename;
    time_t              m_nLastSpaceCheckTimestamp;
    bool                m_bForceTempDBCompaction;
    GIntBig             m_nAge;

    int                 m_nTileInsertionCount;

    GDALGPKGMBTilesLikePseudoDataset* m_poParentDS;

        bool                    m_bInWriteTile;
        CPLErr                  WriteTileInternal(); /* should only be called by WriteTile() */

  public:
                                GDALGPKGMBTilesLikePseudoDataset();
        virtual                ~GDALGPKGMBTilesLikePseudoDataset();

        CPLErr                  ReadTile(const CPLString& osMemFileName,
                                         GByte* pabyTileData,
                                         bool* pbIsLossyFormat = NULL);
        GByte*                  ReadTile(int nRow, int nCol);
        GByte*                  ReadTile(int nRow, int nCol, GByte* pabyData,
                                         bool* pbIsLossyFormat = NULL);

        CPLErr                  WriteTile();

        CPLErr                  FlushTiles();
        CPLErr                  FlushRemainingShiftedTiles(bool bPartialFlush);
        CPLErr                  WriteShiftedTile(int nRow, int nCol, int iBand,
                                                 int nDstXOffset, int nDstYOffset,
                                                 int nDstXSize, int nDstYSize);
        CPLErr                  DoPartialFlushOfPartialTilesIfNecessary();

        virtual CPLErr                  IFlushCacheWithErrCode() = 0;
        virtual int                     IGetRasterCount() = 0;
        virtual GDALRasterBand*         IGetRasterBand(int nBand) = 0;
        virtual sqlite3                *IGetDB() = 0;
        virtual bool                    IGetUpdate() = 0;
        virtual bool                    ICanIWriteBlock() = 0;
        virtual void                    IStartTransaction() = 0;
        virtual void                    ICommitTransaction() = 0;
        virtual const char             *IGetFilename() = 0;
        virtual int                     GetRowFromIntoTopConvention(int nRow) = 0;
};

class GDALGPKGMBTilesLikeRasterBand: public GDALPamRasterBand
{
    GDALGPKGMBTilesLikePseudoDataset* m_poTPD;

    public:
                                GDALGPKGMBTilesLikeRasterBand(GDALGPKGMBTilesLikePseudoDataset* poTPD,
                                                              int nTileWidth, int nTileHeight);

        virtual CPLErr          IReadBlock(int nBlockXOff, int nBlockYOff,
                                           void* pData);
        virtual CPLErr          IWriteBlock(int nBlockXOff, int nBlockYOff,
                                           void* pData);
        virtual CPLErr          FlushCache();

        virtual GDALColorTable* GetColorTable();
        virtual CPLErr          SetColorTable(GDALColorTable* poCT);

        virtual GDALColorInterp GetColorInterpretation();
        virtual CPLErr          SetColorInterpretation( GDALColorInterp );

    protected:
        friend class GDALGPKGMBTilesLikePseudoDataset;

        GDALRasterBlock*        AccessibleTryGetLockedBlockRef(int nBlockXOff, int nBlockYOff) { return TryGetLockedBlockRef(nBlockXOff, nBlockYOff); }

};

#endif // GPKGMBTILESCOMMON_H_INCLUDED
