/******************************************************************************
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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef RMFDATASET_H_INCLUDED
#define RMFDATASET_H_INCLUDED

#include <array>
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

constexpr int RMF_JPEG_BAND_COUNT = 3;
constexpr int RMF_DEM_BAND_COUNT = 1;

/************************************************************************/
/*                            RMFHeader                                 */
/************************************************************************/

typedef struct
{
#define RMF_SIGNATURE_SIZE 4
    char bySignature[RMF_SIGNATURE_SIZE];  // "RSW" for raster
                                           // map or "MTW" for DEM
    GUInt32 iVersion;
    GUInt32 nSize;       // File size in bytes
    GUInt32 nOvrOffset;  // Offset to overview
    GUInt32 iUserID;
#define RMF_NAME_SIZE 32
    GByte byName[RMF_NAME_SIZE];
    GUInt32 nBitDepth;  // Number of bits per pixel
    GUInt32 nHeight;    // Image length
    GUInt32 nWidth;     // Image width
    GUInt32 nXTiles;    // Number of tiles in line
    GUInt32 nYTiles;    // Number of tiles in column
    GUInt32 nTileHeight;
    GUInt32 nTileWidth;
    GUInt32 nLastTileHeight;
    GUInt32 nLastTileWidth;
    GUInt32 nROIOffset;
    GUInt32 nROISize;
    GUInt32 nClrTblOffset;   // Position and size
    GUInt32 nClrTblSize;     // of the colour table
    GUInt32 nTileTblOffset;  // Position and size of the
    GUInt32 nTileTblSize;    // tile offsets/sizes table
    GInt32 iMapType;
    GInt32 iProjection;
    GInt32 iEPSGCode;
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
    GUInt32 nFlagsTblOffset;
    GUInt32 nFlagsTblSize;
    GUInt32 nFileSize0;
    GUInt32 nFileSize1;
    GByte iUnknown;
    GByte iGeorefFlag;
    GByte iInverse;
    GByte iJpegQuality;
#define RMF_INVISIBLE_COLORS_SIZE 32
    GByte abyInvisibleColors[RMF_INVISIBLE_COLORS_SIZE];
    double adfElevMinMax[2];
    double dfNoData;
    GUInt32 iElevationUnit;
    GByte iElevationType;
    GUInt32 nExtHdrOffset;
    GUInt32 nExtHdrSize;
} RMFHeader;

/************************************************************************/
/*                            RMFExtHeader                              */
/************************************************************************/

typedef struct
{
    GInt32 nEllipsoid;
    GInt32 nVertDatum;
    GInt32 nDatum;
    GInt32 nZone;
} RMFExtHeader;

/************************************************************************/
/*                              RSWFrame                                */
/************************************************************************/

typedef struct
{
    GInt32 nType;
    GInt32 nSize;
    GInt32 nSubCount;
    GInt32 nCoordsSize;
} RSWFrame;

typedef struct
{
    GInt32 nX, nY;
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
    GUInt32 nXSize = 0;
    GUInt32 nYSize = 0;

    RMFCompressionJob() = default;
    RMFCompressionJob(const RMFCompressionJob &) = delete;
    RMFCompressionJob &operator=(const RMFCompressionJob &) = delete;
    RMFCompressionJob(RMFCompressionJob &&) = default;
    RMFCompressionJob &operator=(RMFCompressionJob &&) = default;
};

/************************************************************************/
/*                            RMFCompressData                           */
/************************************************************************/

struct RMFCompressData
{
    CPLWorkerThreadPool oThreadPool{};
    std::vector<RMFCompressionJob> asJobs{};
    std::list<RMFCompressionJob *> asReadyJobs{};
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
    std::vector<GByte> oData{};
    int nBandsWritten = 0;
};

/************************************************************************/
/*                              RMFDataset                              */
/************************************************************************/

class RMFDataset final : public GDALDataset
{
    friend class RMFRasterBand;

  private:
    RMFHeader sHeader{};
    RMFExtHeader sExtHeader{};
    RMFType eRMFType = RMFT_RSW;
    GUInt32 nXTiles = 0;
    GUInt32 nYTiles = 0;
    GUInt32 *paiTiles = nullptr;
    GByte *pabyDecompressBuffer = nullptr;
    GByte *pabyCurrentTile = nullptr;
    bool bCurrentTileIsNull = false;
    int nCurrentTileXOff = -1;
    int nCurrentTileYOff = -1;
    GUInt32 nCurrentTileBytes = 0;
    GUInt32 nColorTableSize = 0;
    GByte *pabyColorTable = nullptr;
    GDALColorTable *poColorTable = nullptr;
    GDALGeoTransform m_gt{};
    OGRSpatialReference m_oSRS{};

    char *pszUnitType = nullptr;

    bool bBigEndian = false;
    bool bHeaderDirty = false;

    VSILFILE *fp = nullptr;

    std::shared_ptr<RMFCompressData> poCompressData{};
    std::map<GUInt32, RMFTileData> oUnfinishedTiles{};

    CPLErr WriteHeader();
    static size_t LZWDecompress(const GByte *, GUInt32, GByte *, GUInt32,
                                GUInt32, GUInt32);
    static size_t LZWCompress(const GByte *, GUInt32, GByte *, GUInt32, GUInt32,
                              GUInt32, const RMFDataset *);
#ifdef HAVE_LIBJPEG
    static size_t JPEGDecompress(const GByte *, GUInt32, GByte *, GUInt32,
                                 GUInt32, GUInt32);
    static size_t JPEGCompress(const GByte *, GUInt32, GByte *, GUInt32,
                               GUInt32, GUInt32, const RMFDataset *);
#endif  // HAVE_LIBJPEG
    static size_t DEMDecompress(const GByte *, GUInt32, GByte *, GUInt32,
                                GUInt32, GUInt32);
    static size_t DEMCompress(const GByte *, GUInt32, GByte *, GUInt32, GUInt32,
                              GUInt32, const RMFDataset *);

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
    size_t (*Decompress)(const GByte *pabyIn, GUInt32 nSizeIn, GByte *pabyOut,
                         GUInt32 nSizeOut, GUInt32 nTileSx,
                         GUInt32 nTileSy) = nullptr;

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
    size_t (*Compress)(const GByte *pabyIn, GUInt32 nSizeIn, GByte *pabyOut,
                       GUInt32 nSizeOut, GUInt32 nTileSx, GUInt32 nTileSy,
                       const RMFDataset *poDS) = nullptr;

    std::vector<RMFDataset *> poOvrDatasets{};
    vsi_l_offset nHeaderOffset = 0;
    RMFDataset *poParentDS = nullptr;

    RMFDataset(const RMFDataset &) = delete;
    RMFDataset &operator=(const RMFDataset &) = delete;

  public:
    RMFDataset();
    virtual ~RMFDataset();

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *);
    static RMFDataset *Open(GDALOpenInfo *, RMFDataset *poParentDS,
                            vsi_l_offset nNextHeaderOffset);
    static GDALDataset *Create(const char *, int, int, int, GDALDataType,
                               char **);
    static GDALDataset *Create(const char *, int, int, int, GDALDataType,
                               char **, RMFDataset *poParentDS,
                               double dfOvFactor);
    virtual CPLErr FlushCache(bool bAtClosing) override;

    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    virtual CPLErr SetGeoTransform(const GDALGeoTransform &gt) override;
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
                             int nBandCount, BANDMAP_TYPE panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;
    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    // cppcheck-suppress functionStatic
    vsi_l_offset GetFileOffset(GUInt32 iRMFOffset) const;
    GUInt32 GetRMFOffset(vsi_l_offset iFileOffset,
                         vsi_l_offset *piNewFileOffset) const;
    RMFDataset *OpenOverview(RMFDataset *poParentDS, GDALOpenInfo *);
    vsi_l_offset GetLastOffset() const;
    CPLErr CleanOverviews();
    static GByte GetCompressionType(const char *pszCompressName);
    int SetupCompression(GDALDataType eType, const char *pszFilename);
    static void WriteTileJobFunc(void *pData);
    CPLErr InitCompressorData(char **papszParamList);
    CPLErr WriteTile(int nBlockXOff, int nBlockYOff, GByte *pabyData,
                     size_t nBytes, GUInt32 nRawXSize, GUInt32 nRawYSize);
    CPLErr WriteRawTile(int nBlockXOff, int nBlockYOff, GByte *pabyData,
                        size_t nBytes);
    CPLErr ReadTile(int nBlockXOff, int nBlockYOff, GByte *pabyData,
                    size_t nBytes, GUInt32 nRawXSize, GUInt32 nRawYSize,
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
    GUInt32 nBlockSize = 0;
    GUInt32 nBlockBytes = 0;
    GUInt32 nLastTileWidth = 0;
    GUInt32 nLastTileHeight = 0;
    GUInt32 nDataSize = 0;

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

#endif
