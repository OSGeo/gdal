/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef GTIFFDATASET_H_INCLUDED
#define GTIFFDATASET_H_INCLUDED

#include "gdal_pam.h"

#include <queue>

#include "cpl_mem_cache.h"
#include "cpl_worker_thread_pool.h"  // CPLJobQueue, CPLWorkerThreadPool
#include "fetchbufferdirectio.h"
#include "gtiff.h"
#include "gt_wkt_srs.h"  // GTIFFKeysFlavorEnum
#include "tiffio.h"      // TIFF*

class GTiffJPEGOverviewDS;

enum class GTiffProfile : GByte
{
    BASELINE,
    GEOTIFF,
    GDALGEOTIFF
};

// This must be a #define, since it is used in a XSTRINGIFY() macro
#define DEFAULT_WEBP_LEVEL 75

constexpr const char *const szJPEGGTiffDatasetTmpPrefix =
    "/vsimem/gtiffdataset_jpg_tmp_";

class GTiffBitmapBand;
class GTiffDataset;
class GTiffJPEGOverviewBand;
class GTiffJPEGOverviewDS;
class GTiffRasterBand;
class GTiffRGBABand;

typedef struct
{
    GTiffDataset *poDS;
    char *pszTmpFilename;
    GByte *pabyBuffer;
    GByte *pabyCompressedBuffer;  // Owned by pszTmpFilename.
    GPtrDiff_t nBufferSize;
    GPtrDiff_t nCompressedBufferSize;
    int nHeight;
    int nStripOrTile;
    uint16_t nPredictor;
    bool bTIFFIsBigEndian;
    bool bReady;
    uint16_t *pExtraSamples;
    uint16_t nExtraSampleCount;
} GTiffCompressionJob;

typedef enum
{
    GTIFFTAGTYPE_STRING,
    GTIFFTAGTYPE_SHORT,
    GTIFFTAGTYPE_FLOAT,
    GTIFFTAGTYPE_BYTE_STRING
} GTIFFTagTypes;

struct GTIFFTag
{
    const char *pszTagName;
    int nTagVal;
    GTIFFTagTypes eType;
};

/************************************************************************/
/* ==================================================================== */
/*                            GTiffDataset                              */
/* ==================================================================== */
/************************************************************************/

class GTiffDataset final : public GDALPamDataset
{
  public:
    struct MaskOffset
    {
        uint64_t nMask;
        uint64_t nRoundUpBitTest;
    };

  private:
    CPL_DISALLOW_COPY_ASSIGN(GTiffDataset)

    friend class GTiffBitmapBand;
    friend class GTiffJPEGOverviewDS;
    friend class GTiffJPEGOverviewBand;
    friend class GTiffOddBitsBand;
    friend class GTiffRasterBand;
    friend class GTiffRGBABand;
    friend class GTiffSplitBand;
    friend class GTiffSplitBitmapBand;

    friend void GTIFFSetJpegQuality(GDALDatasetH hGTIFFDS, int nJpegQuality);
    friend void GTIFFSetJpegTablesMode(GDALDatasetH hGTIFFDS,
                                       int nJpegTablesMode);
    friend void GTIFFSetWebPLevel(GDALDatasetH hGTIFFDS, int nWebPLevel);
    friend void GTIFFSetWebPLossless(GDALDatasetH hGTIFFDS, bool bWebPLossless);
    friend void GTIFFSetZLevel(GDALDatasetH hGTIFFDS, int nZLevel);
    friend void GTIFFSetZSTDLevel(GDALDatasetH hGTIFFDS, int nZSTDLevel);
    friend void GTIFFSetMaxZError(GDALDatasetH hGTIFFDS, double dfMaxZError);
#if HAVE_JXL
    friend void GTIFFSetJXLLossless(GDALDatasetH hGTIFFDS, bool bIsLossless);
    friend void GTIFFSetJXLEffort(GDALDatasetH hGTIFFDS, int nEffort);
    friend void GTIFFSetJXLDistance(GDALDatasetH hGTIFFDS, float fDistance);
    friend void GTIFFSetJXLAlphaDistance(GDALDatasetH hGTIFFDS,
                                         float fAlphaDistance);
#endif

    TIFF *m_hTIFF = nullptr;
    VSILFILE *m_fpL = nullptr;
    VSILFILE *m_fpToWrite = nullptr;
    GTiffDataset **m_papoOverviewDS = nullptr;
    GTiffDataset *m_poMaskDS = nullptr;  // For a non-mask dataset, points to
                                         // the corresponding (internal) mask
    GDALDataset *m_poExternalMaskDS =
        nullptr;  // Points to a dataset within m_poMaskExtOvrDS
    GTiffDataset *m_poImageryDS = nullptr;  // For a mask dataset, points to the
                                            // corresponding imagery dataset
    GTiffDataset *m_poBaseDS =
        nullptr;  // For an overview or mask dataset, points to the root dataset
    std::unique_ptr<GDALDataset>
        m_poMaskExtOvrDS{};  // Used with MASK_OVERVIEW_DATASET open option
    GTiffJPEGOverviewDS **m_papoJPEGOverviewDS = nullptr;
    std::vector<gdal::GCP> m_aoGCPs{};
    GDALColorTable *m_poColorTable = nullptr;
    char **m_papszMetadataFiles = nullptr;
    GByte *m_pabyBlockBuf = nullptr;
    char **m_papszCreationOptions = nullptr;
    void *m_pabyTempWriteBuffer = nullptr;
    CPLVirtualMem *m_pBaseMapping = nullptr;
    GByte *m_pTempBufferForCommonDirectIO = nullptr;
    CPLVirtualMem *m_psVirtualMemIOMapping = nullptr;
    CPLWorkerThreadPool *m_poThreadPool = nullptr;
    std::unique_ptr<CPLJobQueue> m_poCompressQueue{};
    CPLMutex *m_hCompressThreadPoolMutex = nullptr;

    lru11::Cache<int, std::pair<vsi_l_offset, vsi_l_offset>>
        m_oCacheStrileToOffsetByteCount{1024};

    MaskOffset *m_panMaskOffsetLsb = nullptr;
    char *m_pszVertUnit = nullptr;
    char *m_pszFilename = nullptr;
    char *m_pszTmpFilename = nullptr;
    char *m_pszGeorefFilename = nullptr;
    char *m_pszXMLFilename = nullptr;

    double m_adfGeoTransform[6]{0, 1, 0, 0, 0, 1};
    double m_dfMaxZError = 0.0;
    double m_dfMaxZErrorOverview = 0.0;
    uint32_t m_anLercAddCompressionAndVersion[2]{0, 0};
#if HAVE_JXL
    bool m_bJXLLossless = true;
    float m_fJXLDistance = 1.0f;
    float m_fJXLAlphaDistance = -1.0f;  // -1 = same as non-alpha channel
    uint32_t m_nJXLEffort = 5;
#endif
    double m_dfNoDataValue = DEFAULT_NODATA_VALUE;
    int64_t m_nNoDataValueInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;
    uint64_t m_nNoDataValueUInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;

    toff_t m_nDirOffset = 0;

    int m_nBlocksPerRow = 0;
    int m_nBlocksPerColumn = 0;
    int m_nBlocksPerBand = 0;
    int m_nBlockXSize = 0;
    int m_nBlockYSize = 0;
    int m_nLoadedBlock = -1;  // Or tile, or scanline
    uint32_t m_nRowsPerStrip = 0;
    int m_nLastBandRead = -1;        // Used for the all-in-on-strip case.
    int m_nLastWrittenBlockId = -1;  // used for m_bStreamingOut
    int m_nRefBaseMapping = 0;
    int m_nDisableMultiThreadedRead = 0;

    GTIFFKeysFlavorEnum m_eGeoTIFFKeysFlavor = GEOTIFF_KEYS_STANDARD;
    GeoTIFFVersionEnum m_eGeoTIFFVersion = GEOTIFF_VERSION_AUTO;

    uint16_t m_nPlanarConfig = 0;
    uint16_t m_nSamplesPerPixel = 0;
    uint16_t m_nBitsPerSample = 0;
    uint16_t m_nPhotometric = 0;
    uint16_t m_nSampleFormat = 0;
    uint16_t m_nCompression = 0;

    signed char m_nOverviewCount = 0;

    // If > 0, the implicit JPEG overviews are visible through
    // GetOverviewCount().
    signed char m_nJPEGOverviewVisibilityCounter = 0;
    // Currently visible overviews. Generally == nJPEGOverviewCountOri.
    signed char m_nJPEGOverviewCount = -1;
    signed char m_nJPEGOverviewCountOri = 0;  // Size of papoJPEGOverviewDS.
    signed char m_nPAMGeorefSrcIndex = -1;
    signed char m_nINTERNALGeorefSrcIndex = -1;
    signed char m_nTABFILEGeorefSrcIndex = -1;
    signed char m_nWORLDFILEGeorefSrcIndex = -1;
    signed char m_nXMLGeorefSrcIndex = -1;
    signed char m_nGeoTransformGeorefSrcIndex = -1;

    signed char m_nHasOptimizedReadMultiRange = -1;

    signed char m_nZLevel = -1;
    signed char m_nLZMAPreset = -1;
    signed char m_nZSTDLevel = -1;
    signed char m_nWebPLevel = DEFAULT_WEBP_LEVEL;
    signed char m_nJpegQuality = -1;
    signed char m_nJpegTablesMode = -1;

    enum class VirtualMemIOEnum : GByte
    {
        NO,
        YES,
        IF_ENOUGH_RAM
    };

    VirtualMemIOEnum m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;

    GTiffProfile m_eProfile = GTiffProfile::GDALGEOTIFF;

    OGRSpatialReference m_oSRS{};

    GDALMultiDomainMetadata m_oGTiffMDMD{};

    std::vector<GTiffCompressionJob> m_asCompressionJobs{};
    std::queue<int> m_asQueueJobIdx{};  // queue of index of m_asCompressionJobs
                                        // being compressed in worker threads

    bool m_bStreamingIn : 1;
    bool m_bStreamingOut : 1;
    bool m_bScanDeferred : 1;
    bool m_bSingleIFDOpened = false;
    bool m_bLoadedBlockDirty : 1;
    bool m_bWriteError : 1;
    bool m_bLookedForProjection : 1;
    bool m_bLookedForMDAreaOrPoint : 1;
    bool m_bGeoTransformValid : 1;
    bool m_bCrystalized : 1;
    bool m_bGeoTIFFInfoChanged : 1;
    bool m_bForceUnsetGTOrGCPs : 1;
    bool m_bForceUnsetProjection : 1;
    bool m_bNoDataChanged : 1;
    bool m_bNoDataSet : 1;
    bool m_bNoDataSetAsInt64 : 1;
    bool m_bNoDataSetAsUInt64 : 1;
    bool m_bMetadataChanged : 1;
    bool m_bColorProfileMetadataChanged : 1;
    bool m_bForceUnsetRPC : 1;
    bool m_bNeedsRewrite : 1;
    bool m_bLoadingOtherBands : 1;
    bool m_bIsOverview : 1;
    bool m_bWriteEmptyTiles : 1;  // Whether a write of a tile entirely at
                                  // nodata/0 should go to the disk. Default is
                                  // true, unless SPARSE_OK is set
    bool m_bFillEmptyTilesAtClosing : 1;  // Might only be set to true on newly
                                          // created files, when SPARSE_OK is
                                          // not set
    bool m_bTreatAsSplit : 1;
    bool m_bTreatAsSplitBitmap : 1;
    bool m_bClipWarn : 1;
    bool m_bIMDRPCMetadataLoaded : 1;
    bool m_bEXIFMetadataLoaded : 1;
    bool m_bICCMetadataLoaded : 1;
    bool m_bHasWarnedDisableAggressiveBandCaching : 1;
    bool m_bDontReloadFirstBlock : 1;  // Hack for libtiff 3.X and #3633.
    bool m_bWebPLossless : 1;
    bool m_bPromoteTo8Bits : 1;
    bool m_bDebugDontWriteBlocks : 1;
    bool m_bIsFinalized : 1;
    bool m_bIgnoreReadErrors : 1;
    bool m_bDirectIO : 1;
    bool m_bReadGeoTransform : 1;
    bool m_bLoadPam : 1;
    bool m_bHasGotSiblingFiles : 1;
    bool m_bHasIdentifiedAuthorizedGeoreferencingSources : 1;
    bool m_bLayoutIFDSBeforeData : 1;
    bool m_bBlockOrderRowMajor : 1;
    bool m_bLeaderSizeAsUInt4 : 1;
    bool m_bTrailerRepeatedLast4BytesRepeated : 1;
    bool m_bMaskInterleavedWithImagery : 1;
    bool m_bKnownIncompatibleEdition : 1;
    bool m_bWriteKnownIncompatibleEdition : 1;
    bool m_bHasUsedReadEncodedAPI : 1;  // for debugging
    bool m_bWriteCOGLayout : 1;

    void ScanDirectories();
    bool ReadStrile(int nBlockId, void *pOutputBuffer,
                    GPtrDiff_t nBlockReqSize);
    CPLErr LoadBlockBuf(int nBlockId, bool bReadFromDisk = true);
    CPLErr FlushBlockBuf();

    void LoadMDAreaOrPoint();
    void LookForProjection();
    void LookForProjectionFromGeoTIFF();
    void LookForProjectionFromXML();

    void Crystalize();  // TODO: Spelling.
    void RestoreVolatileParameters(TIFF *hTIFF);

    void WriteGeoTIFFInfo();
    bool SetDirectory();
    void ReloadDirectory(bool bReopenHandle = false);

    int GetJPEGOverviewCount();

    bool IsBlockAvailable(int nBlockId, vsi_l_offset *pnOffset = nullptr,
                          vsi_l_offset *pnSize = nullptr,
                          bool *pbErrOccurred = nullptr);

    void ApplyPamInfo();
    void PushMetadataToPam();

    bool WriteEncodedTile(uint32_t tile, GByte *pabyData,
                          int bPreserveDataBuffer);
    bool WriteEncodedStrip(uint32_t strip, GByte *pabyData,
                           int bPreserveDataBuffer);

    template <typename T>
    void WriteDealWithLercAndNan(T *pBuffer, int nActualBlockWidth,
                                 int nActualBlockHeight, int nStrileHeight);

    bool HasOnlyNoData(const void *pBuffer, int nWidth, int nHeight,
                       int nLineStride, int nComponents);
    inline bool IsFirstPixelEqualToNoData(const void *pBuffer);

    CPLErr FillEmptyTiles();

    CPLErr FlushDirectory();
    CPLErr CleanOverviews();

    void LoadMetadata();
    void LoadEXIFMetadata();
    void LoadICCProfile();

    CPLErr RegisterNewOverviewDataset(toff_t nOverviewOffset,
                                      int l_nJpegQuality,
                                      CSLConstList papszOptions);
    CPLErr CreateOverviewsFromSrcOverviews(GDALDataset *poSrcDS,
                                           GDALDataset *poOvrDS,
                                           int nOverviews);
    CPLErr CreateInternalMaskOverviews(int nOvrBlockXSize, int nOvrBlockYSize);
    std::tuple<CPLErr, bool> Finalize();

    void DiscardLsb(GByte *pabyBuffer, GPtrDiff_t nBytes, int iBand) const;
    void GetDiscardLsbOption(char **papszOptions);
    void InitCompressionThreads(bool bUpdateMode, CSLConstList papszOptions);
    void InitCreationOrOpenOptions(bool bUpdateMode, CSLConstList papszOptions);
    static void ThreadCompressionFunc(void *pData);
    void WaitCompletionForJobIdx(int i);
    void WaitCompletionForBlock(int nBlockId);
    void WriteRawStripOrTile(int nStripOrTile, GByte *pabyCompressedBuffer,
                             GPtrDiff_t nCompressedBufferSize);
    bool SubmitCompressionJob(int nStripOrTile, GByte *pabyData, GPtrDiff_t cc,
                              int nHeight);

    int GuessJPEGQuality(bool &bOutHasQuantizationTable,
                         bool &bOutHasHuffmanTable);

    void SetJPEGQualityAndTablesModeFromFile(int nQuality,
                                             bool bHasQuantizationTable,
                                             bool bHasHuffmanTable);

    int DirectIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                 int nYSize, void *pData, int nBufXSize, int nBufYSize,
                 GDALDataType eBufType, int nBandCount, int *panBandMap,
                 GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
                 GDALRasterIOExtraArg *psExtraArg);

    int VirtualMemIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount, int *panBandMap,
                     GSpacing nPixelSpace, GSpacing nLineSpace,
                     GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArg);

    void SetStructuralMDFromParent(GTiffDataset *poParentDS);

    template <class FetchBuffer>
    CPLErr CommonDirectIO(FetchBuffer &oFetcher, int nXOff, int nYOff,
                          int nXSize, int nYSize, void *pData, int nBufXSize,
                          int nBufYSize, GDALDataType eBufType, int nBandCount,
                          int *panBandMap, GSpacing nPixelSpace,
                          GSpacing nLineSpace, GSpacing nBandSpace);

    CPLErr CommonDirectIOClassic(FetchBufferDirectIO &oFetcher, int nXOff,
                                 int nYOff, int nXSize, int nYSize, void *pData,
                                 int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType, int nBandCount,
                                 int *panBandMap, GSpacing nPixelSpace,
                                 GSpacing nLineSpace, GSpacing nBandSpace);

    void LoadGeoreferencingAndPamIfNeeded();

    char **GetSiblingFiles();

    void IdentifyAuthorizedGeoreferencingSources();

    CPLErr FlushCacheInternal(bool bAtClosing, bool bFlushDirectory);
    bool HasOptimizedReadMultiRange();

    bool AssociateExternalMask();

    static bool MustCreateInternalMask();

    static CPLErr CopyImageryAndMask(GTiffDataset *poDstDS,
                                     GDALDataset *poSrcDS,
                                     GDALRasterBand *poSrcMaskBand,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData);

    bool GetOverviewParameters(int &nCompression, uint16_t &nPlanarConfig,
                               uint16_t &nPredictor, uint16_t &nPhotometric,
                               int &nOvrJpegQuality, std::string &osNoData,
                               uint16_t *&panExtraSampleValues,
                               uint16_t &nExtraSamples,
                               CSLConstList papszOptions) const;

    bool IsWholeBlock(int nXOff, int nYOff, int nXSize, int nYSize) const;

    static void ThreadDecompressionFunc(void *pData);

    static GTIF *GTIFNew(TIFF *hTIFF);

  protected:
    virtual int CloseDependentDatasets() override;

  public:
    GTiffDataset();
    virtual ~GTiffDataset();

    CPLErr Close() override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual CPLErr GetGeoTransform(double *) override;
    virtual CPLErr SetGeoTransform(double *) override;

    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    CPLErr SetGCPs(int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                   const OGRSpatialReference *poSRS) override;

    bool IsMultiThreadedReadCompatible() const;
    CPLErr MultiThreadedRead(int nXOff, int nYOff, int nXSize, int nYSize,
                             void *pData, GDALDataType eBufType, int nBandCount,
                             const int *panBandMap, GSpacing nPixelSpace,
                             GSpacing nLineSpace, GSpacing nBandSpace);

    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, int *panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

    virtual CPLStringList
    GetCompressionFormats(int nXOff, int nYOff, int nXSize, int nYSize,
                          int nBandCount, const int *panBandList) override;
    virtual CPLErr ReadCompressedData(const char *pszFormat, int nXOff,
                                      int nYOff, int nXSize, int nYSize,
                                      int nBandCount, const int *panBandList,
                                      void **ppBuffer, size_t *pnBufferSize,
                                      char **ppszDetailedFormat) override;

    virtual char **GetFileList() override;

    virtual CPLErr IBuildOverviews(const char *, int, const int *, int,
                                   const int *, GDALProgressFunc, void *,
                                   CSLConstList papszOptions) override;

    bool ComputeBlocksPerColRowAndBand(int l_nBands);

    CPLErr OpenOffset(TIFF *, toff_t nDirOffset, GDALAccess,
                      bool bAllowRGBAInterface = true,
                      bool bReadGeoTransform = false);

    static GDALDataset *OpenDir(GDALOpenInfo *);
    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszParamList);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
    virtual CPLErr FlushCache(bool bAtClosing) override;

    virtual char **GetMetadataDomainList() override;
    virtual CPLErr SetMetadata(char **, const char * = "") override;
    virtual char **GetMetadata(const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *, const char *,
                                   const char * = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    virtual void *GetInternalHandle(const char *) override;

    virtual CPLErr CreateMaskBand(int nFlags) override;

    bool GetRawBinaryLayout(GDALDataset::RawBinaryLayout &) override;

    // Only needed by createcopy and close code.
    static void WriteRPC(GDALDataset *, TIFF *, int, GTiffProfile, const char *,
                         CSLConstList papszCreationOptions,
                         bool bWriteOnlyInPAMIfNeeded = false);
    static bool WriteMetadata(GDALDataset *, TIFF *, bool, GTiffProfile,
                              const char *, CSLConstList papszCreationOptions,
                              bool bExcludeRPBandIMGFileWriting = false);
    static void WriteNoDataValue(TIFF *, double);
    static void WriteNoDataValue(TIFF *, int64_t);
    static void WriteNoDataValue(TIFF *, uint64_t);
    static void UnsetNoDataValue(TIFF *);

    static TIFF *CreateLL(const char *pszFilename, int nXSize, int nYSize,
                          int nBands, GDALDataType eType,
                          double dfExtraSpaceForOverviews,
                          char **papszParamList, VSILFILE **pfpL,
                          CPLString &osTmpFilename);

    CPLErr WriteEncodedTileOrStrip(uint32_t tile_or_strip, void *data,
                                   int bPreserveDataBuffer);

    static void SaveICCProfile(GTiffDataset *pDS, TIFF *hTIFF,
                               char **papszParamList, uint32_t nBitsPerSample);

    static const GTIFFTag *GetTIFFTags();
};

GTIFFKeysFlavorEnum GetGTIFFKeysFlavor(CSLConstList papszOptions);
GeoTIFFVersionEnum GetGeoTIFFVersion(CSLConstList papszOptions);
void GTiffSetDeflateSubCodec(TIFF *hTIFF);

#endif  // GTIFFDATASET_H_INCLUDED
