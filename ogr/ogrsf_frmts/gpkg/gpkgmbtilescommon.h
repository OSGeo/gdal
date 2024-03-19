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
    int nRow;
    int nCol;
    int nIdxWithinTileData;
    bool abBandDirty[4];
} CachedTileDesc;

typedef enum
{
    GPKG_TF_PNG_JPEG,
    GPKG_TF_PNG,
    GPKG_TF_PNG8,
    GPKG_TF_JPEG,
    GPKG_TF_WEBP,
    GPKG_TF_PNG_16BIT,        // For GPKG elevation data
    GPKG_TF_TIFF_32BIT_FLOAT  // For GPKG elevation data
} GPKGTileFormat;

GPKGTileFormat GDALGPKGMBTilesGetTileFormat(const char *pszTF);
const char *GDALMBTilesGetTileFormatName(GPKGTileFormat);

class GDALGPKGMBTilesLikePseudoDataset
{
    friend class GDALGPKGMBTilesLikeRasterBand;

    GDALGPKGMBTilesLikePseudoDataset(const GDALGPKGMBTilesLikePseudoDataset &) =
        delete;
    GDALGPKGMBTilesLikePseudoDataset &
    operator=(const GDALGPKGMBTilesLikePseudoDataset &) = delete;

  protected:
    bool m_bNew = false;
    bool m_bHasModifiedTiles = false;

    CPLString m_osRasterTable{};
    GDALDataType m_eDT = GDT_Byte;
    int m_nDTSize = 1;
    double m_dfOffset = 0.0;
    double m_dfScale = 1.0;
    double m_dfPrecision = 1.0;
    GUInt16 m_usGPKGNull = 0;
    int m_nZoomLevel = -1;
    GByte *m_pabyCachedTiles = nullptr;
    CachedTileDesc m_asCachedTilesDesc[4];
    int m_nShiftXTiles = 0;
    int m_nShiftXPixelsMod = 0;
    int m_nShiftYTiles = 0;
    int m_nShiftYPixelsMod = 0;
    int m_nTileMatrixWidth = 0;
    int m_nTileMatrixHeight = 0;

    GPKGTileFormat m_eTF = GPKG_TF_PNG_JPEG;
    bool m_bPNGSupports2Bands =
        true;  // for test/debug purposes only. true is the nominal value
    bool m_bPNGSupportsCT =
        true;  // for test/debug purposes only. true is the nominal value
    int m_nZLevel = 6;
    int m_nQuality = 75;
    bool m_bDither = false;

    GDALColorTable *m_poCT = nullptr;
    bool m_bTriedEstablishingCT = false;
    void *m_pabyHugeColorArray = nullptr;

    CPLString m_osWHERE{};

    sqlite3_vfs *m_pMyVFS = nullptr;
    sqlite3 *m_hTempDB = nullptr;
    CPLString m_osTempDBFilename{};
    time_t m_nLastSpaceCheckTimestamp = 0;
    bool m_bForceTempDBCompaction = false;
    GIntBig m_nAge = 0;

    int m_nTileInsertionCount = 0;

    GDALGPKGMBTilesLikePseudoDataset *m_poParentDS = nullptr;

  private:
    bool m_bInWriteTile = false;
    CPLErr WriteTileInternal(); /* should only be called by WriteTile() */
    GIntBig GetTileId(int nRow, int nCol);
    bool DeleteTile(int nRow, int nCol);
    bool DeleteFromGriddedTileAncillary(GIntBig nTileId);
    void GetTileOffsetAndScale(GIntBig nTileId, double &dfTileOffset,
                               double &dfTileScale);
    void FillBuffer(GByte *pabyData, size_t nPixels);
    void FillEmptyTile(GByte *pabyData);
    void FillEmptyTileSingleBand(GByte *pabyData);

  public:
    GDALGPKGMBTilesLikePseudoDataset();
    virtual ~GDALGPKGMBTilesLikePseudoDataset();

    void SetDataType(GDALDataType eDT);
    void SetGlobalOffsetScale(double dfOffset, double dfScale);

    CPLErr ReadTile(const CPLString &osMemFileName, GByte *pabyTileData,
                    double dfTileOffset, double dfTileScale,
                    bool *pbIsLossyFormat = nullptr);
    GByte *ReadTile(int nRow, int nCol);
    GByte *ReadTile(int nRow, int nCol, GByte *pabyData,
                    bool *pbIsLossyFormat = nullptr);

    CPLErr WriteTile();

    CPLErr FlushTiles();
    CPLErr FlushRemainingShiftedTiles(bool bPartialFlush);
    CPLErr WriteShiftedTile(int nRow, int nCol, int iBand, int nDstXOffset,
                            int nDstYOffset, int nDstXSize, int nDstYSize);
    CPLErr DoPartialFlushOfPartialTilesIfNecessary();

    virtual CPLErr IFlushCacheWithErrCode(bool bAtClosing) = 0;
    virtual int IGetRasterCount() = 0;
    virtual GDALRasterBand *IGetRasterBand(int nBand) = 0;
    virtual sqlite3 *IGetDB() = 0;
    virtual bool IGetUpdate() = 0;
    virtual bool ICanIWriteBlock() = 0;
    virtual OGRErr IStartTransaction() = 0;
    virtual OGRErr ICommitTransaction() = 0;
    virtual const char *IGetFilename() = 0;
    virtual int GetRowFromIntoTopConvention(int nRow) = 0;
};

class GDALGPKGMBTilesLikeRasterBand : public GDALPamRasterBand
{
    GDALGPKGMBTilesLikeRasterBand(const GDALGPKGMBTilesLikeRasterBand &) =
        delete;
    GDALGPKGMBTilesLikeRasterBand &
    operator=(const GDALGPKGMBTilesLikeRasterBand &) = delete;

  protected:
    GDALGPKGMBTilesLikePseudoDataset *m_poTPD = nullptr;
    int m_nDTSize = 0;
    bool m_bHasNoData = false;
    double m_dfNoDataValue = 0;
    CPLString m_osUom{};

  public:
    GDALGPKGMBTilesLikeRasterBand(GDALGPKGMBTilesLikePseudoDataset *poTPD,
                                  int nTileWidth, int nTileHeight);

    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff,
                              void *pData) override;
    virtual CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff,
                               void *pData) override;
    virtual CPLErr FlushCache(bool bAtClosing) override;

    virtual GDALColorTable *GetColorTable() override;
    virtual CPLErr SetColorTable(GDALColorTable *poCT) override;

    void AssignColorTable(const GDALColorTable *poCT);

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual CPLErr SetColorInterpretation(GDALColorInterp) override;

    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;

    virtual const char *GetUnitType() override
    {
        return m_osUom.c_str();
    }

    void SetNoDataValueInternal(double dfNoDataValue);

    void SetUnitTypeInternal(const CPLString &osUom)
    {
        m_osUom = osUom;
    }

    GDALRasterBlock *AccessibleTryGetLockedBlockRef(int nBlockXOff,
                                                    int nBlockYOff)
    {
        return TryGetLockedBlockRef(nBlockXOff, nBlockYOff);
    }
};

#endif  // GPKGMBTILESCOMMON_H_INCLUDED
