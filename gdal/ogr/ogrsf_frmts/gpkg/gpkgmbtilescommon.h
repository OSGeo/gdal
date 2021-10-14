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
#include <sqlite3.h>

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
    GPKG_TF_WEBP,
    GPKG_TF_PNG_16BIT, // For GPKG elevation data
    GPKG_TF_TIFF_32BIT_FLOAT // For GPKG elevation data
} GPKGTileFormat;

GPKGTileFormat GDALGPKGMBTilesGetTileFormat(const char* pszTF );

class GDALGPKGMBTilesLikePseudoDataset
{
    friend class GDALGPKGMBTilesLikeRasterBand;

  protected:
    bool                m_bNew;
    bool                m_bHasModifiedTiles;

    CPLString           m_osRasterTable;
    GDALDataType        m_eDT;
    int                 m_nDTSize;
    double              m_dfOffset;
    double              m_dfScale;
    double              m_dfPrecision;
    GUInt16             m_usGPKGNull;
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

    sqlite3_vfs*        m_pMyVFS;
    sqlite3            *m_hTempDB;
    CPLString           m_osTempDBFilename;
    time_t              m_nLastSpaceCheckTimestamp;
    bool                m_bForceTempDBCompaction;
    GIntBig             m_nAge;

    int                 m_nTileInsertionCount;

    GDALGPKGMBTilesLikePseudoDataset* m_poParentDS;

  private:
        bool                    m_bInWriteTile;
        CPLErr                  WriteTileInternal(); /* should only be called by WriteTile() */
        GIntBig                 GetTileId(int nRow, int nCol);
        bool                    DeleteTile(int nRow, int nCol);
        bool                    DeleteFromGriddedTileAncillary(GIntBig nTileId);
        void                    GetTileOffsetAndScale(
                                    GIntBig nTileId,
                                    double& dfTileOffset, double& dfTileScale);
        void                    FillBuffer(GByte* pabyData, size_t nPixels);
        void                    FillEmptyTile(GByte* pabyData);
        void                    FillEmptyTileSingleBand(GByte* pabyData);

  public:
                                GDALGPKGMBTilesLikePseudoDataset();
        virtual                ~GDALGPKGMBTilesLikePseudoDataset();

        void                    SetDataType(GDALDataType eDT);
        void                    SetGlobalOffsetScale(double dfOffset,
                                                     double dfScale);

        CPLErr                  ReadTile(const CPLString& osMemFileName,
                                         GByte* pabyTileData,
                                         double dfTileOffset,
                                         double dfTileScale,
                                         bool* pbIsLossyFormat = nullptr);
        GByte*                  ReadTile(int nRow, int nCol);
        GByte*                  ReadTile(int nRow, int nCol, GByte* pabyData,
                                         bool* pbIsLossyFormat = nullptr);

        CPLErr                  WriteTile();

        CPLErr                  FlushTiles();
        CPLErr                  FlushRemainingShiftedTiles(bool bPartialFlush);
        CPLErr                  WriteShiftedTile(int nRow, int nCol, int iBand,
                                                 int nDstXOffset, int nDstYOffset,
                                                 int nDstXSize, int nDstYSize);
        CPLErr                  DoPartialFlushOfPartialTilesIfNecessary();

        virtual CPLErr                  IFlushCacheWithErrCode(bool bAtClosing) = 0;
        virtual int                     IGetRasterCount() = 0;
        virtual GDALRasterBand*         IGetRasterBand(int nBand) = 0;
        virtual sqlite3                *IGetDB() = 0;
        virtual bool                    IGetUpdate() = 0;
        virtual bool                    ICanIWriteBlock() = 0;
        virtual OGRErr                  IStartTransaction() = 0;
        virtual OGRErr                  ICommitTransaction() = 0;
        virtual const char             *IGetFilename() = 0;
        virtual int                     GetRowFromIntoTopConvention(int nRow) = 0;
};

class GDALGPKGMBTilesLikeRasterBand: public GDALPamRasterBand
{
    protected:
        GDALGPKGMBTilesLikePseudoDataset* m_poTPD;
        int                               m_nDTSize;
        bool                              m_bHasNoData;
        double                            m_dfNoDataValue;
        CPLString                         m_osUom;

    public:
                                GDALGPKGMBTilesLikeRasterBand(GDALGPKGMBTilesLikePseudoDataset* poTPD,
                                                              int nTileWidth, int nTileHeight);

        virtual CPLErr          IReadBlock(int nBlockXOff, int nBlockYOff,
                                           void* pData) override;
        virtual CPLErr          IWriteBlock(int nBlockXOff, int nBlockYOff,
                                           void* pData) override;
        virtual CPLErr          FlushCache(bool bAtClosing) override;

        virtual GDALColorTable* GetColorTable() override;
        virtual CPLErr          SetColorTable(GDALColorTable* poCT) override;

        virtual GDALColorInterp GetColorInterpretation() override;
        virtual CPLErr          SetColorInterpretation( GDALColorInterp ) override;

        virtual double          GetNoDataValue( int* pbSuccess = nullptr ) override;
        virtual const char*     GetUnitType() override { return m_osUom.c_str(); }

        void                    SetNoDataValueInternal( double dfNoDataValue );
        void                    SetUnitTypeInternal(const CPLString& osUom)
                                                        { m_osUom = osUom; }

    protected:
        friend class GDALGPKGMBTilesLikePseudoDataset;

        GDALRasterBlock*        AccessibleTryGetLockedBlockRef(int nBlockXOff, int nBlockYOff) { return TryGetLockedBlockRef(nBlockXOff, nBlockYOff); }
};

#endif // GPKGMBTILESCOMMON_H_INCLUDED
