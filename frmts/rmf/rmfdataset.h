/******************************************************************************
 * $Id$
 *
 * Project:  Raster Matrix Format
 * Purpose:  Private class declarations for the RMF classes used to read/write
 *           GIS "Integratsia" raster files (also known as "Panorama" GIS).
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2023, NextGIS <info@nextgis.com>
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

#include <list>
#include "gdal_priv.h"
#include "cpl_worker_thread_pool.h"

#define RMF_HEADER_SIZE 320
#define RMF_EXT_HEADER_SIZE 320
#define RMF_MIN_EXT_HEADER_SIZE (36 + 4)
#define RMF_MAX_EXT_HEADER_SIZE 1000000

#define RMF_COMPRESSION_NONE 0
#define RMF_COMPRESSION_LZW 1
#define RMF_COMPRESSION_JPEG 2
#define RMF_COMPRESSION_DEM 32

enum RMFType
{
    RMFT_RSW,  // Raster map
    RMFT_MTW   // Digital elevation model
};

enum RMFVersion
{
    RMF_VERSION = 0x0200,      // Version for "small" files less than 4 Gb
    RMF_VERSION_HUGE = 0x0201  // Version for "huge" files less than 4 Tb. Since
                               // GIS Panorama v11
};

class RMFDataset;

#define RMF_HUGE_OFFSET_FACTOR 256

#define RMF_JPEG_BAND_COUNT 3
#define RMF_DEM_BAND_COUNT 1

/************************************************************************/
/*                            RMFHeader                                 */
/************************************************************************/

typedef struct
{
#define RMF_SIGNATURE_SIZE 4
    char bySignature[RMF_SIGNATURE_SIZE];  // "RSW" for raster
                                           // map or "MTW" for DEM
    uint32_t iVersion;
    uint32_t nSize;       // File size in bytes
    uint32_t nOvrOffset;  // Offset to overview
    uint32_t iUserID;
#define RMF_NAME_SIZE 32
    GByte byName[RMF_NAME_SIZE];
    uint32_t nBitDepth;  // Number of bits per pixel
    uint32_t nHeight;    // Image length
    uint32_t nWidth;     // Image width
    uint32_t nXTiles;    // Number of tiles in line
    uint32_t nYTiles;    // Number of tiles in column
    uint32_t nTileHeight;
    uint32_t nTileWidth;
    uint32_t nLastTileHeight;
    uint32_t nLastTileWidth;
    uint32_t nROIOffset;
    uint32_t nROISize;
    uint32_t nClrTblOffset;   // Position and size
    uint32_t nClrTblSize;     // of the colour table
    uint32_t nTileTblOffset;  // Position and size of the
    uint32_t nTileTblSize;    // tile offsets/sizes table
    int32_t iMapType;
    int32_t iProjection;
    int32_t iEPSGCode;
    double dfScale;
    double dfResolution;
    double dfPixelSize;
    double dfLLX;
    double dfLLY;
    double dfStdP1;
    double dfStdP2;
    double dfCenterLong;
    double dfCenterLat;
    GByte iCompression;
    GByte iMaskType;
    GByte iMaskStep;
    GByte iFrameFlag;
    uint32_t nFlagsTblOffset;
    uint32_t nFlagsTblSize;
    uint32_t nFileSize0;
    uint32_t nFileSize1;
    GByte iUnknown;
    GByte iGeorefFlag;
    GByte iInverse;
    GByte iJpegQuality;
#define RMF_INVISIBLE_COLORS_SIZE 32
    GByte abyInvisibleColors[RMF_INVISIBLE_COLORS_SIZE];
    double adfElevMinMax[2];
    double dfNoData;
    uint32_t iElevationUnit;
    GByte iElevationType;
    uint32_t nExtHdrOffset;
    uint32_t nExtHdrSize;
} RMFHeader;

/************************************************************************/
/*                            RMFExtHeader                              */
/************************************************************************/

typedef struct
{
    int32_t nEllipsoid;
    int32_t nVertDatum;
    int32_t nDatum;
    int32_t nZone;
} RMFExtHeader;

/************************************************************************/
/*                              RSWFrame                                */
/************************************************************************/

typedef struct
{
    int32_t nType;
    int32_t nSize;
    int32_t nSubCount;
    int32_t nCoordsSize;
} RSWFrame;

typedef struct
{
    int32_t nX, nY;
} RSWFrameCoord;

/************************************************************************/
/*                            RMFCompressionJob                         */
/************************************************************************/

struct RMFCompressionJob
{
    RMFDataset *poDS = nullptr;
    CPLErr eResult = CE_None;
    int nBlockXOff = -1;
    int nBlockYOff = -1;
    GByte *pabyUncompressedData = nullptr;
    size_t nUncompressedBytes = 0;
    GByte *pabyCompressedData = nullptr;
    size_t nCompressedBytes = 0;
    uint32_t nXSize = 0;
    uint32_t nYSize = 0;
};

/************************************************************************/
/*                            RMFCompressData                           */
/************************************************************************/

struct RMFCompressData
{
    CPLWorkerThreadPool oThreadPool;
    std::vector<RMFCompressionJob> asJobs;
    std::list<RMFCompressionJob *> asReadyJobs;
    GByte *pabyBuffers = nullptr;
    CPLMutex *hReadyJobMutex = nullptr;
    CPLMutex *hWriteTileMutex = nullptr;

    RMFCompressData(const RMFCompressData &) = delete;
    RMFCompressData &operator=(const RMFCompressData &) = delete;

    RMFCompressData();
    ~RMFCompressData();
};

/************************************************************************/
/*                            RMFTileData                               */
/************************************************************************/

struct RMFTileData
{
    std::vector<GByte> oData;
    int nBandsWritten = 0;
};

/************************************************************************/
/*                              RMFDataset                              */
/************************************************************************/

class RMFDataset final : public GDALDataset
{
    friend class RMFRasterBand;

  private:
    RMFHeader sHeader;
    RMFExtHeader sExtHeader;
    RMFType eRMFType;
    uint32_t nXTiles;
    uint32_t nYTiles;
    uint32_t *paiTiles;
    GByte *pabyDecompressBuffer;
    GByte *pabyCurrentTile;
    bool bCurrentTileIsNull;
    int nCurrentTileXOff;
    int nCurrentTileYOff;
    uint32_t nCurrentTileBytes;
    uint32_t nColorTableSize;
    GByte *pabyColorTable;
    GDALColorTable *poColorTable;
    double adfGeoTransform[6];
    OGRSpatialReference m_oSRS{};

    char *pszUnitType;

    bool bBigEndian;
    bool bHeaderDirty;

    VSILFILE *fp;

    std::shared_ptr<RMFCompressData> poCompressData;
    std::map<uint32_t, RMFTileData> oUnfinishedTiles;

    CPLErr WriteHeader();
    static size_t LZWDecompress(const GByte *, uint32_t, GByte *, uint32_t,
                                uint32_t, uint32_t);
    static size_t LZWCompress(const GByte *, uint32_t, GByte *, uint32_t,
                              uint32_t, uint32_t, const RMFDataset *);
#ifdef HAVE_LIBJPEG
    static size_t JPEGDecompress(const GByte *, uint32_t, GByte *, uint32_t,
                                 uint32_t, uint32_t);
    static size_t JPEGCompress(const GByte *, uint32_t, GByte *, uint32_t,
                               uint32_t, uint32_t, const RMFDataset *);
#endif  // HAVE_LIBJPEG
    static size_t DEMDecompress(const GByte *, uint32_t, GByte *, uint32_t,
                                uint32_t, uint32_t);
    static size_t DEMCompress(const GByte *, uint32_t, GByte *, uint32_t,
                              uint32_t, uint32_t, const RMFDataset *);

    /*!
        Tile decompress callback
        pabyIn - input compressed data
        nSizeIn - input compressed data size (in bytes)
        pabyOut - pointer to uncompressed data
        nSizeOut - maximum uncompressed data size
        nTileSx - width of uncompressed tile (in pixels)
        nTileSy - height of uncompressed tile (in pixels)

        Returns: actual uncompressed data size or 0 on error (if nSizeOut is
                 small returns 0 too).
    */
    size_t (*Decompress)(const GByte *pabyIn, uint32_t nSizeIn, GByte *pabyOut,
                         uint32_t nSizeOut, uint32_t nTileSx, uint32_t nTileSy);

    /*!
        Tile compress callback
        pabyIn - input uncompressed data
        nSizeIn - input uncompressed data size (in bytes)
        pabyOut - pointer to compressed data
        nSizeOut - maximum compressed data size
        nTileSx - width of uncompressed tile (in pixels)
        nTileSy - height of uncompressed tile (in pixels)
        poDS - pointer to parent dataset

        Returns: actual compressed data size or 0 on error (if nSizeOut is
                 small returns 0 too).
    */
    size_t (*Compress)(const GByte *pabyIn, uint32_t nSizeIn, GByte *pabyOut,
                       uint32_t nSizeOut, uint32_t nTileSx, uint32_t nTileSy,
                       const RMFDataset *poDS);

    std::vector<RMFDataset *> poOvrDatasets;
    uint64_t nHeaderOffset;
    RMFDataset *poParentDS;

  public:
    RMFDataset();
    virtual ~RMFDataset();

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *);
    static RMFDataset *Open(GDALOpenInfo *, RMFDataset *poParentDS,
                            uint64_t nNextHeaderOffset);
    static GDALDataset *Create(const char *, int, int, int, GDALDataType,
                               char **);
    static GDALDataset *Create(const char *, int, int, int, GDALDataType,
                               char **, RMFDataset *poParentDS,
                               double dfOvFactor);
    virtual CPLErr FlushCache(bool bAtClosing) override;

    virtual CPLErr GetGeoTransform(double *padfTransform) override;
    virtual CPLErr SetGeoTransform(double *) override;
    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual CPLErr IBuildOverviews(const char *pszResampling, int nOverviews,
                                   const int *panOverviewList, int nBandsIn,
                                   const int *panBandList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList papszOptions) override;
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, int *panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;
    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    uint64_t GetFileOffset(uint32_t iRMFOffset) const;
    uint32_t GetRMFOffset(uint64_t iFileOffset,
                          uint64_t *piNewFileOffset) const;
    RMFDataset *OpenOverview(RMFDataset *poParentDS, GDALOpenInfo *);
    uint64_t GetLastOffset() const;
    CPLErr CleanOverviews();
    static GByte GetCompressionType(const char *pszCompressName);
    int SetupCompression(GDALDataType eType, const char *pszFilename);
    static void WriteTileJobFunc(void *pData);
    CPLErr InitCompressorData(char **papszParamList);
    CPLErr WriteTile(int nBlockXOff, int nBlockYOff, GByte *pabyData,
                     size_t nBytes, uint32_t nRawXSize, uint32_t nRawYSize);
    CPLErr WriteRawTile(int nBlockXOff, int nBlockYOff, GByte *pabyData,
                        size_t nBytes);
    CPLErr ReadTile(int nBlockXOff, int nBlockYOff, GByte *pabyData,
                    size_t nBytes, uint32_t nRawXSize, uint32_t nRawYSize,
                    bool &bNullTile);
    void SetupNBits();
};

/************************************************************************/
/*                            RMFRasterBand                             */
/************************************************************************/

class RMFRasterBand final : public GDALRasterBand
{
    friend class RMFDataset;

  private:
    uint32_t nBytesPerPixel;
    uint32_t nBlockSize;
    uint32_t nBlockBytes;
    uint32_t nLastTileWidth;
    uint32_t nLastTileHeight;
    uint32_t nDataSize;

  public:
    RMFRasterBand(RMFDataset *, int, GDALDataType);
    virtual ~RMFRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
    virtual GDALSuggestedBlockAccessPattern
    GetSuggestedBlockAccessPattern() const override;
    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;
    virtual CPLErr SetNoDataValue(double dfNoData) override;
    virtual const char *GetUnitType() override;
    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;
    virtual CPLErr SetUnitType(const char *) override;
    virtual CPLErr SetColorTable(GDALColorTable *) override;
    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int i) override;
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;
};
