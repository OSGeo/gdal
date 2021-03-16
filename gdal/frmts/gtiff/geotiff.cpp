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

// If we use sunpro compiler on linux. Weird idea indeed!
#if defined(__SUNPRO_CC) && defined(__linux__)
#define _GNU_SOURCE
#elif defined(__GNUC__) && !defined(_GNU_SOURCE)
// Required to use RTLD_DEFAULT of dlfcn.h.
#define _GNU_SOURCE
#endif

#include "cpl_port.h"  // Must be first.
#include "gtiff.h"

#include <cassert>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <queue>
#include <utility>
#include <vector>

#include "cpl_config.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_error_internal.h"
#include "cpl_mem_cache.h"
#include "cpl_md5.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_port.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_virtualmem.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "cpl_worker_thread_pool.h"
#include "cplkeywordparser.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_mdreader.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_priv_templates.hpp"
#include "gdal_thread_pool.h"
#include "geo_normalize.h"
#include "geotiff.h"
#include "geovalues.h"
#include "gt_jpeg_copy.h"
#include "gt_overview.h"
#include "gt_wkt_srs.h"
#include "gt_wkt_srs_priv.h"
#include "ogr_spatialref.h"
#include "ogr_proj_p.h"
#include "tiff.h"
#include "tif_float.h"
#include "tiffio.h"
#include "tiffvers.h"
#include "tifvsi.h"
#include "xtiffio.h"
#include "quant_table_md5sum.h"

CPL_CVSID("$Id$")

static bool bGlobalInExternalOvr = false;

// Only libtiff 4.0.4 can handle between 32768 and 65535 directories.
#if TIFFLIB_VERSION >= 20120922
#define SUPPORTS_MORE_THAN_32768_DIRECTORIES
#endif

#if TIFFLIB_VERSION > 20181110 // > 4.0.10
#define SUPPORTS_GET_OFFSET_BYTECOUNT
#endif

const char* const szJPEGGTiffDatasetTmpPrefix = "/vsimem/gtiffdataset_jpg_tmp_";

typedef enum
{
    GTIFFTAGTYPE_STRING,
    GTIFFTAGTYPE_SHORT,
    GTIFFTAGTYPE_FLOAT,
    GTIFFTAGTYPE_BYTE_STRING
} GTIFFTagTypes;

typedef struct
{
    const char    *pszTagName;
    int            nTagVal;
    GTIFFTagTypes  eType;
} GTIFFTags;

static const GTIFFTags asTIFFTags[] =
{
    { "TIFFTAG_DOCUMENTNAME", TIFFTAG_DOCUMENTNAME, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_IMAGEDESCRIPTION", TIFFTAG_IMAGEDESCRIPTION,
        GTIFFTAGTYPE_STRING },
    { "TIFFTAG_SOFTWARE", TIFFTAG_SOFTWARE, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_DATETIME", TIFFTAG_DATETIME, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_ARTIST", TIFFTAG_ARTIST, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_HOSTCOMPUTER", TIFFTAG_HOSTCOMPUTER, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_COPYRIGHT", TIFFTAG_COPYRIGHT, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_XRESOLUTION", TIFFTAG_XRESOLUTION, GTIFFTAGTYPE_FLOAT },
    { "TIFFTAG_YRESOLUTION", TIFFTAG_YRESOLUTION, GTIFFTAGTYPE_FLOAT },
    // Dealt as special case.
    { "TIFFTAG_RESOLUTIONUNIT", TIFFTAG_RESOLUTIONUNIT, GTIFFTAGTYPE_SHORT },
    { "TIFFTAG_MINSAMPLEVALUE", TIFFTAG_MINSAMPLEVALUE, GTIFFTAGTYPE_SHORT },
    { "TIFFTAG_MAXSAMPLEVALUE", TIFFTAG_MAXSAMPLEVALUE, GTIFFTAGTYPE_SHORT },

    // GeoTIFF DGIWG tags
    { "GEO_METADATA", TIFFTAG_GEO_METADATA, GTIFFTAGTYPE_BYTE_STRING },
    { "TIFF_RSID", TIFFTAG_TIFF_RSID, GTIFFTAGTYPE_STRING },
};

const char szPROFILE_BASELINE[] = "BASELINE";
const char szPROFILE_GeoTIFF[] = "GeoTIFF";
const char szPROFILE_GDALGeoTIFF[] = "GDALGeoTIFF";

/************************************************************************/
/*                          GTIFFSetInExternalOvr()                     */
/************************************************************************/

void GTIFFSetInExternalOvr( bool b )
{
    bGlobalInExternalOvr = b;
}

/************************************************************************/
/*                     GTIFFGetOverviewBlockSize()                      */
/************************************************************************/

void GTIFFGetOverviewBlockSize( GDALRasterBandH hBand, int* pnBlockXSize, int* pnBlockYSize )
{
    const char* pszVal = CPLGetConfigOption("GDAL_TIFF_OVR_BLOCKSIZE", nullptr);
    if( ! pszVal )
    {
        GDALRasterBand* const poBand = GDALRasterBand::FromHandle(hBand);
        poBand->GetBlockSize(pnBlockXSize,pnBlockYSize);
        if ( *pnBlockXSize != *pnBlockYSize ||
             *pnBlockXSize < 64 || *pnBlockXSize > 4096 ||
             !CPLIsPowerOfTwo(*pnBlockXSize) )
        {
            *pnBlockXSize=*pnBlockYSize=128;
        }
    }
    else
    {
        int nOvrBlockSize = atoi(pszVal);
        if( nOvrBlockSize < 64 || nOvrBlockSize > 4096 ||
            !CPLIsPowerOfTwo(nOvrBlockSize) )
        {
            static bool bHasWarned = false;
            if( !bHasWarned )
            {
                CPLError( CE_Warning, CPLE_NotSupported,
                          "Wrong value for GDAL_TIFF_OVR_BLOCKSIZE : %s. "
                          "Should be a power of 2 between 64 and 4096. "
                          "Defaulting to 128",
                          pszVal );
                bHasWarned = true;
            }
            nOvrBlockSize = 128;
        }

        *pnBlockXSize = nOvrBlockSize;
        *pnBlockYSize = nOvrBlockSize;
    }
}

enum
{
    ENDIANNESS_NATIVE,
    ENDIANNESS_LITTLE,
    ENDIANNESS_BIG
};

/************************************************************************/
/* ==================================================================== */
/*                          GTiffDataset                                */
/* ==================================================================== */
/************************************************************************/

class GTiffBitmapBand;
class GTiffDataset;
class GTiffJPEGOverviewBand;
class GTiffJPEGOverviewDS;
class GTiffRasterBand;
class GTiffRGBABand;

#if !defined(__MINGW32__)
namespace {
#endif
typedef struct
{
    GTiffDataset *poDS;
    char         *pszTmpFilename;
    GByte        *pabyBuffer;
    GByte        *pabyCompressedBuffer;  // Owned by pszTmpFilename.
    GPtrDiff_t    nBufferSize;
    GPtrDiff_t    nCompressedBufferSize;
    int           nHeight;
    int           nStripOrTile;
    uint16_t        nPredictor;
    bool          bTIFFIsBigEndian;
    bool          bReady;
} GTiffCompressionJob;
#if !defined(__MINGW32__)
}
#endif

enum class GTiffProfile: GByte
{
    BASELINE,
    GEOTIFF,
    GDALGEOTIFF
};

class GTiffDataset final : public GDALPamDataset
{
public:
    struct MaskOffset
    {
        int nMask;
        int nOffset;
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

    friend void  GTIFFSetJpegQuality( GDALDatasetH hGTIFFDS, int nJpegQuality );
    friend void  GTIFFSetJpegTablesMode( GDALDatasetH hGTIFFDS, int nJpegTablesMode );
    friend void  GTIFFSetWebPLevel( GDALDatasetH hGTIFFDS, int nWebPLevel );

    TIFF                 *m_hTIFF = nullptr;
    VSILFILE             *m_fpL = nullptr;
    VSILFILE             *m_fpToWrite = nullptr;
    GTiffDataset        **m_papoOverviewDS = nullptr;
    GTiffDataset         *m_poMaskDS = nullptr; // For a non-mask dataset, points to the corresponding (internal) mask
    GDALDataset          *m_poExternalMaskDS = nullptr; // Points to a dataset within m_poMaskExtOvrDS
    GTiffDataset         *m_poImageryDS = nullptr; // For a mask dataset, points to the corresponding imagery dataset
    GTiffDataset         *m_poBaseDS = nullptr; // For an overview or mask dataset, points to the root dataset
    std::unique_ptr<GDALDataset> m_poMaskExtOvrDS{}; // Used with MASK_OVERVIEW_DATASET open option
    GTiffJPEGOverviewDS **m_papoJPEGOverviewDS = nullptr;
    GDAL_GCP             *m_pasGCPList = nullptr;
    GDALColorTable       *m_poColorTable = nullptr;
    char                **m_papszMetadataFiles = nullptr;
    GByte                *m_pabyBlockBuf = nullptr;
    char                **m_papszCreationOptions = nullptr;
    void                 *m_pabyTempWriteBuffer = nullptr;
    CPLVirtualMem        *m_pBaseMapping = nullptr;
    GByte                *m_pTempBufferForCommonDirectIO = nullptr;
    CPLVirtualMem        *m_psVirtualMemIOMapping = nullptr;
    std::unique_ptr<CPLJobQueue> m_poCompressQueue{};
    CPLMutex             *m_hCompressThreadPoolMutex = nullptr;

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
    lru11::Cache<int, std::pair<vsi_l_offset, vsi_l_offset>> m_oCacheStrileToOffsetByteCount{1024};
#endif

    MaskOffset* m_panMaskOffsetLsb = nullptr;
    char       *m_pszVertUnit = nullptr;
    char       *m_pszFilename = nullptr;
    char       *m_pszTmpFilename = nullptr;
    char       *m_pszGeorefFilename = nullptr;

    double      m_adfGeoTransform[6]{0,1,0,0,0,1};
    double      m_dfMaxZError = 0.0;
    uint32_t      m_anLercAddCompressionAndVersion[2]{0,0};
    double      m_dfNoDataValue = -9999.0;

    toff_t      m_nDirOffset = 0;

    int         m_nBlocksPerBand = 0;
    int         m_nBlockXSize = 0;
    int         m_nBlockYSize = 0;
    int         m_nLoadedBlock = -1;  // Or tile, or scanline
    uint32_t      m_nRowsPerStrip = 0;
    int         m_nLastBandRead = -1; // Used for the all-in-on-strip case.
    int         m_nLastWrittenBlockId = -1; // used for m_bStreamingOut
    int         m_nRefBaseMapping = 0;
    int         m_nGCPCount = 0;

    GTIFFKeysFlavorEnum m_eGeoTIFFKeysFlavor = GEOTIFF_KEYS_STANDARD;
    GeoTIFFVersionEnum m_eGeoTIFFVersion = GEOTIFF_VERSION_AUTO;

    uint16_t      m_nPlanarConfig = 0;
    uint16_t      m_nSamplesPerPixel = 0;
    uint16_t      m_nBitsPerSample = 0;
    uint16_t      m_nPhotometric = 0;
    uint16_t      m_nSampleFormat = 0;
    uint16_t      m_nCompression = 0;

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
    signed char m_nGeoTransformGeorefSrcIndex = -1;

    signed char m_nHasOptimizedReadMultiRange = -1;

    signed char m_nZLevel = -1;
    signed char m_nLZMAPreset = -1;
    signed char m_nZSTDLevel = -1;
    signed char m_nWebPLevel = -1;
    signed char m_nJpegQuality = -1;
    signed char m_nJpegTablesMode = -1;

    enum class VirtualMemIOEnum: GByte
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
    std::queue<int> m_asQueueJobIdx{}; // queue of index of m_asCompressionJobs being compressed in worker threads

    bool        m_bStreamingIn:1;
    bool        m_bStreamingOut:1;
    bool        m_bScanDeferred:1;
    bool        m_bSingleIFDOpened = false;
    bool        m_bLoadedBlockDirty:1;
    bool        m_bWriteError:1;
    bool        m_bLookedForProjection:1;
    bool        m_bLookedForMDAreaOrPoint:1;
    bool        m_bGeoTransformValid:1;
    bool        m_bCrystalized:1;
    bool        m_bGeoTIFFInfoChanged:1;
    bool        m_bForceUnsetGTOrGCPs:1;
    bool        m_bForceUnsetProjection:1;
    bool        m_bNoDataChanged:1;
    bool        m_bNoDataSet:1;
    bool        m_bMetadataChanged:1;
    bool        m_bColorProfileMetadataChanged:1;
    bool        m_bForceUnsetRPC:1;
    bool        m_bNeedsRewrite:1;
    bool        m_bLoadingOtherBands:1;
    bool        m_bIsOverview:1;
    bool        m_bWriteEmptyTiles:1;
    bool        m_bFillEmptyTilesAtClosing:1;
    bool        m_bTreatAsSplit:1;
    bool        m_bTreatAsSplitBitmap:1;
    bool        m_bClipWarn:1;
    bool        m_bIMDRPCMetadataLoaded:1;
    bool        m_bEXIFMetadataLoaded:1;
    bool        m_bICCMetadataLoaded:1;
    bool        m_bHasWarnedDisableAggressiveBandCaching:1;
    bool        m_bDontReloadFirstBlock:1;  // Hack for libtiff 3.X and #3633.
    bool        m_bWebPLossless:1;
    bool        m_bPromoteTo8Bits:1;
    bool        m_bDebugDontWriteBlocks:1;
    bool        m_bIsFinalized:1;
    bool        m_bIgnoreReadErrors:1;
    bool        m_bDirectIO:1;
    bool        m_bReadGeoTransform:1;
    bool        m_bLoadPam:1;
    bool        m_bHasGotSiblingFiles:1;
    bool        m_bHasIdentifiedAuthorizedGeoreferencingSources:1;
    bool        m_bLayoutIFDSBeforeData:1;
    bool        m_bBlockOrderRowMajor:1;
    bool        m_bLeaderSizeAsUInt4:1;
    bool        m_bTrailerRepeatedLast4BytesRepeated:1;
    bool        m_bMaskInterleavedWithImagery:1;
    bool        m_bKnownIncompatibleEdition:1;
    bool        m_bWriteKnownIncompatibleEdition:1;
    bool        m_bHasUsedReadEncodedAPI:1; // for debugging
    bool        m_bWriteCOGLayout:1;

    void        ScanDirectories();
    bool        ReadStrile(int nBlockId,
                           void* pOutputBuffer, GPtrDiff_t nBlockReqSize);
    CPLErr      LoadBlockBuf( int nBlockId, bool bReadFromDisk = true );
    CPLErr      FlushBlockBuf();

    void        LoadMDAreaOrPoint();
    void        LookForProjection();
#ifdef ESRI_BUILD
    void        AdjustLinearUnit( short UOMLength );
#endif

    void        Crystalize();  // TODO: Spelling.
    void        RestoreVolatileParameters(TIFF* hTIFF);

    void        WriteGeoTIFFInfo();
    bool        SetDirectory();
    void        ReloadDirectory();

    int         GetJPEGOverviewCount();

    bool        IsBlockAvailable( int nBlockId,
                                  vsi_l_offset* pnOffset = nullptr,
                                  vsi_l_offset* pnSize = nullptr,
                                  bool *pbErrOccurred = nullptr );

    void        ApplyPamInfo();
    void        PushMetadataToPam();

    bool         WriteEncodedTile( uint32_t tile, GByte* pabyData,
                                   int bPreserveDataBuffer );
    bool         WriteEncodedStrip( uint32_t strip, GByte* pabyData,
                                    int bPreserveDataBuffer );
    template<class T>
    bool         HasOnlyNoDataT( const T* pBuffer, int nWidth, int nHeight,
                                int nLineStride, int nComponents ) const;
    bool         HasOnlyNoData( const void* pBuffer, int nWidth, int nHeight,
                                int nLineStride, int nComponents );
    inline bool  IsFirstPixelEqualToNoData( const void* pBuffer );

    void         FillEmptyTiles();

    void         FlushDirectory();
    CPLErr       CleanOverviews();

    void          LoadMetadata();
    void          LoadEXIFMetadata();
    void          LoadICCProfile();

    CPLErr        RegisterNewOverviewDataset( toff_t nOverviewOffset, int l_nJpegQuality,
                                              int l_nWebPLevel );
    CPLErr        CreateOverviewsFromSrcOverviews( GDALDataset* poSrcDS,
                                                   GDALDataset* poOvrDS );
    CPLErr        CreateInternalMaskOverviews( int nOvrBlockXSize,
                                               int nOvrBlockYSize );
    int           Finalize();

    void           DiscardLsb(GByte* pabyBuffer, GPtrDiff_t nBytes, int iBand) const;
    void           GetDiscardLsbOption( char** papszOptions );
    void           InitCompressionThreads( char** papszOptions );
    void           InitCreationOrOpenOptions( char** papszOptions );
    static void    ThreadCompressionFunc( void* pData );
    void           WaitCompletionForJobIdx( int i );
    void           WaitCompletionForBlock( int nBlockId );
    void           WriteRawStripOrTile( int nStripOrTile,
                                        GByte* pabyCompressedBuffer,
                                        GPtrDiff_t nCompressedBufferSize );
    bool           SubmitCompressionJob( int nStripOrTile, GByte* pabyData,
                                         GPtrDiff_t cc, int nHeight) ;

    int            GuessJPEGQuality( bool& bOutHasQuantizationTable,
                                     bool& bOutHasHuffmanTable );

    void           SetJPEGQualityAndTablesModeFromFile(int nQuality,
                                                       bool bHasQuantizationTable,
                                                       bool bHasHuffmanTable);

    int            DirectIO( GDALRWFlag eRWFlag,
                             int nXOff, int nYOff, int nXSize, int nYSize,
                             void * pData, int nBufXSize, int nBufYSize,
                             GDALDataType eBufType,
                             int nBandCount, int *panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg* psExtraArg );

    int            VirtualMemIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nBandCount, int *panBandMap,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GSpacing nBandSpace,
                                 GDALRasterIOExtraArg* psExtraArg );

    void            SetStructuralMDFromParent(GTiffDataset* poParentDS);

    template<class FetchBuffer> CPLErr CommonDirectIO(
        FetchBuffer& oFetcher,
        int nXOff, int nYOff, int nXSize, int nYSize,
        void * pData, int nBufXSize, int nBufYSize,
        GDALDataType eBufType,
        int nBandCount, int *panBandMap,
        GSpacing nPixelSpace, GSpacing nLineSpace,
        GSpacing nBandSpace );

    void        LoadGeoreferencingAndPamIfNeeded();

    char      **GetSiblingFiles();

    void        IdentifyAuthorizedGeoreferencingSources();

    void        FlushCacheInternal( bool bFlushDirectory );
    bool        HasOptimizedReadMultiRange();

    bool        AssociateExternalMask();

    static bool MustCreateInternalMask();

    static CPLErr CopyImageryAndMask(GTiffDataset* poDstDS,
                                     GDALDataset* poSrcDS,
                                     GDALRasterBand* poSrcMaskBand,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData);

  protected:
    virtual int         CloseDependentDatasets() override;

  public:
             GTiffDataset();
    virtual ~GTiffDataset();

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual CPLErr SetGeoTransform( double * ) override;

    virtual int    GetGCPCount() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    CPLErr SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                    const OGRSpatialReference* poSRS ) override;

    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;
    virtual char **GetFileList() override;

    virtual CPLErr IBuildOverviews( const char *, int, int *, int, int *,
                                    GDALProgressFunc, void * ) override;

    CPLErr         OpenOffset( TIFF *,
                               toff_t nDirOffset, GDALAccess,
                               bool bAllowRGBAInterface = true,
                               bool bReadGeoTransform = false );

    static GDALDataset *OpenDir( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParamList );
    static GDALDataset *CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );
    virtual void    FlushCache() override;

    virtual char  **GetMetadataDomainList() override;
    virtual CPLErr  SetMetadata( char **, const char * = "" ) override;
    virtual char  **GetMetadata( const char * pszDomain = "" ) override;
    virtual CPLErr  SetMetadataItem( const char*, const char*,
                                     const char* = "" ) override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;
    virtual void   *GetInternalHandle( const char * ) override;

    virtual CPLErr          CreateMaskBand( int nFlags ) override;

    bool GetRawBinaryLayout(GDALDataset::RawBinaryLayout&) override;

    // Only needed by createcopy and close code.
    static void     WriteRPC( GDALDataset *, TIFF *, int, GTiffProfile,
                              const char *, char **,
                              bool bWriteOnlyInPAMIfNeeded = false );
    static bool     WriteMetadata( GDALDataset *, TIFF *, bool, GTiffProfile,
                                   const char *, char **,
                                   bool bExcludeRPBandIMGFileWriting = false );
    static void     WriteNoDataValue( TIFF *, double );
    static void     UnsetNoDataValue( TIFF * );

    static TIFF *   CreateLL( const char * pszFilename,
                              int nXSize, int nYSize, int nBands,
                              GDALDataType eType,
                              double dfExtraSpaceForOverviews,
                              char **papszParamList,
                              VSILFILE** pfpL,
                              CPLString& osTmpFilename );

    CPLErr   WriteEncodedTileOrStrip( uint32_t tile_or_strip, void* data,
                                      int bPreserveDataBuffer );

    static void SaveICCProfile( GTiffDataset *pDS, TIFF *hTIFF,
                                char **papszParamList, uint32_t nBitsPerSample );
};

/************************************************************************/
/* ==================================================================== */
/*                        GTiffJPEGOverviewDS                           */
/* ==================================================================== */
/************************************************************************/

class GTiffJPEGOverviewDS final : public GDALDataset
{
    CPL_DISALLOW_COPY_ASSIGN(GTiffJPEGOverviewDS)

    friend class GTiffJPEGOverviewBand;
    GTiffDataset* m_poParentDS = nullptr;
    int           m_nOverviewLevel = 0;

    int        m_nJPEGTableSize = 0;
    GByte     *m_pabyJPEGTable = nullptr;
    CPLString  m_osTmpFilenameJPEGTable{};

    CPLString    m_osTmpFilename{};
    GDALDataset* m_poJPEGDS = nullptr;
    // Valid block id of the parent DS that match poJPEGDS.
    int          m_nBlockId = -1;

  public:
    GTiffJPEGOverviewDS( GTiffDataset* poParentDS, int nOverviewLevel,
                         const void* pJPEGTable, int nJPEGTableSize );
    virtual ~GTiffJPEGOverviewDS();

    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;
};

class GTiffJPEGOverviewBand final : public GDALRasterBand
{
  public:
    GTiffJPEGOverviewBand( GTiffJPEGOverviewDS* poDS, int nBand );
    virtual ~GTiffJPEGOverviewBand() {}

    virtual CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/*                        GTiffJPEGOverviewDS()                         */
/************************************************************************/

GTiffJPEGOverviewDS::GTiffJPEGOverviewDS( GTiffDataset* poParentDSIn,
                                          int nOverviewLevelIn,
                                          const void* pJPEGTable,
                                          int nJPEGTableSizeIn ) :
    m_poParentDS(poParentDSIn),
    m_nOverviewLevel(nOverviewLevelIn),
    m_nJPEGTableSize(nJPEGTableSizeIn)
{
    ShareLockWithParentDataset(poParentDSIn);

    m_osTmpFilenameJPEGTable.Printf("/vsimem/jpegtable_%p", this);

    const GByte abyAdobeAPP14RGB[] = {
        0xFF, 0xEE, 0x00, 0x0E, 0x41, 0x64, 0x6F, 0x62, 0x65, 0x00,
        0x64, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const bool bAddAdobe =
        m_poParentDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        m_poParentDS->m_nPhotometric != PHOTOMETRIC_YCBCR &&
        m_poParentDS->nBands == 3;
    m_pabyJPEGTable =
        static_cast<GByte*>( CPLMalloc(
            m_nJPEGTableSize + (bAddAdobe ? sizeof(abyAdobeAPP14RGB) : 0)) );
    memcpy(m_pabyJPEGTable, pJPEGTable, m_nJPEGTableSize);
    if( bAddAdobe )
    {
        memcpy( m_pabyJPEGTable + m_nJPEGTableSize, abyAdobeAPP14RGB,
                sizeof(abyAdobeAPP14RGB) );
        m_nJPEGTableSize += sizeof(abyAdobeAPP14RGB);
    }
    CPL_IGNORE_RET_VAL(
        VSIFCloseL(
            VSIFileFromMemBuffer(
                m_osTmpFilenameJPEGTable, m_pabyJPEGTable, m_nJPEGTableSize, TRUE )));

    const int nScaleFactor = 1 << m_nOverviewLevel;
    nRasterXSize = (m_poParentDS->nRasterXSize + nScaleFactor - 1) / nScaleFactor;
    nRasterYSize = (m_poParentDS->nRasterYSize + nScaleFactor - 1) / nScaleFactor;

    for( int i = 1; i <= m_poParentDS->nBands; ++i )
        SetBand(i, new GTiffJPEGOverviewBand(this, i));

    SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    if( m_poParentDS->m_nPhotometric == PHOTOMETRIC_YCBCR )
        SetMetadataItem( "COMPRESSION", "YCbCr JPEG", "IMAGE_STRUCTURE" );
    else
        SetMetadataItem( "COMPRESSION", "JPEG", "IMAGE_STRUCTURE" );
}

/************************************************************************/
/*                       ~GTiffJPEGOverviewDS()                         */
/************************************************************************/

GTiffJPEGOverviewDS::~GTiffJPEGOverviewDS()
{
    if( m_poJPEGDS != nullptr )
        GDALClose( m_poJPEGDS );
    VSIUnlink(m_osTmpFilenameJPEGTable);
    if( !m_osTmpFilename.empty() )
        VSIUnlink(m_osTmpFilename);
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GTiffJPEGOverviewDS::IRasterIO(
    GDALRWFlag eRWFlag,
    int nXOff, int nYOff, int nXSize, int nYSize,
    void * pData, int nBufXSize, int nBufYSize,
    GDALDataType eBufType,
    int nBandCount, int *panBandMap,
    GSpacing nPixelSpace, GSpacing nLineSpace,
    GSpacing nBandSpace,
    GDALRasterIOExtraArg* psExtraArg )

{
    // For non-single strip JPEG-IN-TIFF, the block based strategy will
    // be the most efficient one, to avoid decompressing the JPEG content
    // for each requested band.
    if( nBandCount > 1 && m_poParentDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        (m_poParentDS->m_nBlockXSize < m_poParentDS->nRasterXSize ||
         m_poParentDS->m_nBlockYSize > 1) )
    {
        return BlockBasedRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                   pData, nBufXSize, nBufYSize,
                                   eBufType, nBandCount, panBandMap,
                                   nPixelSpace, nLineSpace, nBandSpace,
                                   psExtraArg );
    }

    return GDALDataset::IRasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize,
        pData, nBufXSize, nBufYSize, eBufType,
        nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace,
        psExtraArg );
}

/************************************************************************/
/*                        GTiffJPEGOverviewBand()                       */
/************************************************************************/

GTiffJPEGOverviewBand::GTiffJPEGOverviewBand( GTiffJPEGOverviewDS* poDSIn,
                                              int nBandIn )
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType =
        poDSIn->m_poParentDS->GetRasterBand(nBandIn)->GetRasterDataType();
    poDSIn->m_poParentDS->GetRasterBand(nBandIn)->
        GetBlockSize(&nBlockXSize, &nBlockYSize);
    const int nScaleFactor = 1 << poDSIn->m_nOverviewLevel;
    nBlockXSize = (nBlockXSize + nScaleFactor - 1) / nScaleFactor;
    nBlockYSize = (nBlockYSize + nScaleFactor - 1) / nScaleFactor;
}

/************************************************************************/
/*                          IReadBlock()                                */
/************************************************************************/

CPLErr GTiffJPEGOverviewBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                          void *pImage )
{
    GTiffJPEGOverviewDS* m_poGDS = cpl::down_cast<GTiffJPEGOverviewDS *>(poDS);

    // Compute the source block ID.
    int nBlockId = 0;
    int nParentBlockXSize, nParentBlockYSize;
    m_poGDS->m_poParentDS->GetRasterBand(1)->
        GetBlockSize(&nParentBlockXSize, &nParentBlockYSize);
    const bool bIsSingleStripAsSplit = (nParentBlockYSize == 1 &&
                           m_poGDS->m_poParentDS->m_nBlockYSize != nParentBlockYSize);
    if( !bIsSingleStripAsSplit )
    {
        int l_nBlocksPerRow = DIV_ROUND_UP(m_poGDS->m_poParentDS->nRasterXSize,
                                               m_poGDS->m_poParentDS->m_nBlockXSize);
        nBlockId = nBlockYOff * l_nBlocksPerRow + nBlockXOff;
    }
    if( m_poGDS->m_poParentDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        nBlockId += (nBand-1) * m_poGDS->m_poParentDS->m_nBlocksPerBand;
    }

    // Make sure it is available.
    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eDataType);
    vsi_l_offset nOffset = 0;
    vsi_l_offset nByteCount = 0;
    bool bErrOccurred = false;
    if( !m_poGDS->m_poParentDS->IsBlockAvailable(nBlockId, &nOffset, &nByteCount, &bErrOccurred) )
    {
        memset(pImage, 0, static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize * nDataTypeSize );
        if( bErrOccurred )
            return CE_Failure;
        return CE_None;
    }

    const int nScaleFactor = 1 << m_poGDS->m_nOverviewLevel;
    if( m_poGDS->m_poJPEGDS == nullptr || nBlockId != m_poGDS->m_nBlockId )
    {
        if( nByteCount < 2 )
            return CE_Failure;
        nOffset += 2;  // Skip leading 0xFF 0xF8.
        nByteCount -= 2;

        // Special case for last strip that might be smaller than other strips
        // In which case we must invalidate the dataset.
        TIFF* hTIFF = m_poGDS->m_poParentDS->m_hTIFF;
        if( !TIFFIsTiled( hTIFF ) && !bIsSingleStripAsSplit &&
            (nBlockYOff + 1 ==
                 DIV_ROUND_UP( m_poGDS->m_poParentDS->nRasterYSize,
                               m_poGDS->m_poParentDS->m_nBlockYSize ) ||
             (m_poGDS->m_poJPEGDS != nullptr &&
              m_poGDS->m_poJPEGDS->GetRasterYSize() !=
              nBlockYSize * nScaleFactor)) )
        {
            if( m_poGDS->m_poJPEGDS != nullptr )
                GDALClose( m_poGDS->m_poJPEGDS );
            m_poGDS->m_poJPEGDS = nullptr;
        }

        CPLString osFileToOpen;
        m_poGDS->m_osTmpFilename.Printf("/vsimem/sparse_%p", m_poGDS);
        VSILFILE* fp = VSIFOpenL(m_poGDS->m_osTmpFilename, "wb+");

        // If the size of the JPEG strip/tile is small enough, we will
        // read it from the TIFF file and forge a in-memory JPEG file with
        // the JPEG table followed by the JPEG data.
        const bool bInMemoryJPEGFile = nByteCount < 256 * 256;
        if( bInMemoryJPEGFile )
        {
            // If the previous file was opened as a /vsisparse/, must re-open.
            if( m_poGDS->m_poJPEGDS != nullptr &&
                STARTS_WITH(m_poGDS->m_poJPEGDS->GetDescription(), "/vsisparse/") )
            {
                GDALClose( m_poGDS->m_poJPEGDS );
                m_poGDS->m_poJPEGDS = nullptr;
            }
            osFileToOpen = m_poGDS->m_osTmpFilename;

            bool bError = false;
            if( VSIFSeekL(fp, m_poGDS->m_nJPEGTableSize + nByteCount - 1, SEEK_SET)
                != 0 )
                bError = true;
            char ch = 0;
            if( !bError && VSIFWriteL(&ch, 1, 1, fp) != 1 )
                bError = true;
            GByte* pabyBuffer =
                VSIGetMemFileBuffer( m_poGDS->m_osTmpFilename, nullptr, FALSE);
            memcpy(pabyBuffer, m_poGDS->m_pabyJPEGTable, m_poGDS->m_nJPEGTableSize);
            VSILFILE* fpTIF = VSI_TIFFGetVSILFile(TIFFClientdata( hTIFF ));
            if( !bError && VSIFSeekL(fpTIF, nOffset, SEEK_SET) != 0 )
                bError = true;
            if( VSIFReadL( pabyBuffer + m_poGDS->m_nJPEGTableSize,
                           static_cast<size_t>(nByteCount), 1, fpTIF) != 1 )
                bError = true;
            if( bError )
            {
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                return CE_Failure;
            }
        }
        else
        {
            // If the JPEG strip/tile is too big (e.g. a single-strip
            // JPEG-in-TIFF), we will use /vsisparse mechanism to make a
            // fake JPEG file.

            // Always re-open.
            GDALClose( m_poGDS->m_poJPEGDS );
            m_poGDS->m_poJPEGDS = nullptr;

            osFileToOpen =
                CPLSPrintf("/vsisparse/%s", m_poGDS->m_osTmpFilename.c_str());

            if( VSIFPrintfL(
                    fp,
                    "<VSISparseFile><SubfileRegion>"
                    "<Filename relative='0'>%s</Filename>"
                    "<DestinationOffset>0</DestinationOffset>"
                    "<SourceOffset>0</SourceOffset>"
                    "<RegionLength>%d</RegionLength>"
                    "</SubfileRegion>"
                    "<SubfileRegion>"
                    "<Filename relative='0'>%s</Filename>"
                    "<DestinationOffset>%d</DestinationOffset>"
                    "<SourceOffset>" CPL_FRMT_GUIB "</SourceOffset>"
                    "<RegionLength>" CPL_FRMT_GUIB "</RegionLength>"
                    "</SubfileRegion></VSISparseFile>",
                    m_poGDS->m_osTmpFilenameJPEGTable.c_str(),
                    static_cast<int>(m_poGDS->m_nJPEGTableSize),
                    m_poGDS->m_poParentDS->GetDescription(),
                    static_cast<int>(m_poGDS->m_nJPEGTableSize),
                    nOffset,
                    nByteCount) < 0 )
            {
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                return CE_Failure;
            }
        }
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));

        if( m_poGDS->m_poJPEGDS == nullptr )
        {
            const char* apszDrivers[] = { "JPEG", nullptr };

            CPLString osOldVal;
            if( m_poGDS->m_poParentDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
                m_poGDS->nBands == 4 )
            {
                osOldVal =
                    CPLGetThreadLocalConfigOption("GDAL_JPEG_TO_RGB", "");
                CPLSetThreadLocalConfigOption("GDAL_JPEG_TO_RGB", "NO");
            }

            m_poGDS->m_poJPEGDS =
                static_cast<GDALDataset *>( GDALOpenEx(
                    osFileToOpen,
                    GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    apszDrivers, nullptr, nullptr) );

            if( m_poGDS->m_poJPEGDS != nullptr )
            {
                // Force all implicit overviews to be available, even for
                // small tiles.
                CPLSetThreadLocalConfigOption( "JPEG_FORCE_INTERNAL_OVERVIEWS",
                                               "YES");
                GDALGetOverviewCount(GDALGetRasterBand(m_poGDS->m_poJPEGDS, 1));
                CPLSetThreadLocalConfigOption( "JPEG_FORCE_INTERNAL_OVERVIEWS",
                                               nullptr);

                m_poGDS->m_nBlockId = nBlockId;
            }

            if( m_poGDS->m_poParentDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
                m_poGDS->nBands == 4 )
            {
                CPLSetThreadLocalConfigOption(
                    "GDAL_JPEG_TO_RGB",
                    !osOldVal.empty() ? osOldVal.c_str() : nullptr );
            }
        }
        else
        {
            // Trick: we invalidate the JPEG dataset to force a reload
            // of the new content.
            CPLErrorReset();
            m_poGDS->m_poJPEGDS->FlushCache();
            if( CPLGetLastErrorNo() != 0 )
            {
                GDALClose( m_poGDS->m_poJPEGDS );
                m_poGDS->m_poJPEGDS = nullptr;
                return CE_Failure;
            }
            m_poGDS->m_nBlockId = nBlockId;
        }
    }

    CPLErr eErr = CE_Failure;
    if( m_poGDS->m_poJPEGDS )
    {
        GDALDataset* l_poDS = m_poGDS->m_poJPEGDS;

        int nReqXOff = 0;
        int nReqYOff = 0;
        int nReqXSize = 0;
        int nReqYSize = 0;
        if( bIsSingleStripAsSplit )
        {
            nReqYOff = nBlockYOff * nScaleFactor;
            nReqXSize = l_poDS->GetRasterXSize();
            nReqYSize = nScaleFactor;
        }
        else
        {
            if( nBlockXSize == m_poGDS->GetRasterXSize() )
            {
                nReqXSize = l_poDS->GetRasterXSize();
            }
            else
            {
                nReqXSize = nBlockXSize * nScaleFactor;
            }
            nReqYSize = nBlockYSize * nScaleFactor;
        }
        int nBufXSize = nBlockXSize;
        int nBufYSize = nBlockYSize;
        if( nBlockXOff == DIV_ROUND_UP(m_poGDS->m_poParentDS->nRasterXSize,
                                       m_poGDS->m_poParentDS->m_nBlockXSize) - 1 )
        {
            nReqXSize = m_poGDS->m_poParentDS->nRasterXSize -
                                nBlockXOff * m_poGDS->m_poParentDS->m_nBlockXSize;
        }
        if( nReqXOff + nReqXSize > l_poDS->GetRasterXSize() )
        {
            nReqXSize = l_poDS->GetRasterXSize() - nReqXOff;
        }
        if( !bIsSingleStripAsSplit &&
            nBlockYOff == DIV_ROUND_UP(m_poGDS->m_poParentDS->nRasterYSize,
                                       m_poGDS->m_poParentDS->m_nBlockYSize) - 1 )
        {
            nReqYSize = m_poGDS->m_poParentDS->nRasterYSize -
                                nBlockYOff * m_poGDS->m_poParentDS->m_nBlockYSize;
        }
        if( nReqYOff + nReqYSize > l_poDS->GetRasterYSize() )
        {
            nReqYSize = l_poDS->GetRasterYSize() - nReqYOff;
        }
        if( nBlockXOff * nBlockXSize > m_poGDS->GetRasterXSize() - nBufXSize )
        {
            memset(pImage, 0, static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize * nDataTypeSize);
            nBufXSize = m_poGDS->GetRasterXSize() - nBlockXOff * nBlockXSize;
        }
        if( nBlockYOff * nBlockYSize > m_poGDS->GetRasterYSize() - nBufYSize )
        {
            memset(pImage, 0, static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize * nDataTypeSize);
            nBufYSize = m_poGDS->GetRasterYSize() - nBlockYOff * nBlockYSize;
        }

        const int nSrcBand =
            m_poGDS->m_poParentDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE ?
            1 : nBand;
        if( nSrcBand <= l_poDS->GetRasterCount() )
        {
            eErr = l_poDS->GetRasterBand(nSrcBand)->RasterIO(GF_Read,
                                 nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                                 pImage,
                                 nBufXSize, nBufYSize, eDataType,
                                 0, static_cast<GPtrDiff_t>(nBlockXSize) * nDataTypeSize, nullptr );
        }
    }

    return eErr;
}

/************************************************************************/
/*                        GTIFFSetJpegQuality()                         */
/* Called by GTIFFBuildOverviews() to set the jpeg quality on the IFD   */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetJpegQuality( GDALDatasetH hGTIFFDS, int nJpegQuality )
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset* const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_nJpegQuality = static_cast<signed char>(nJpegQuality);

    poDS->ScanDirectories();

    for( int i = 0; i < poDS->m_nOverviewCount; ++i )
        poDS->m_papoOverviewDS[i]->m_nJpegQuality = poDS->m_nJpegQuality;
}

/************************************************************************/
/*                        GTIFFSetWebPLevel()                         */
/* Called by GTIFFBuildOverviews() to set the jpeg quality on the IFD   */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetWebPLevel( GDALDatasetH hGTIFFDS, int nWebpLevel )
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset* const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_nWebPLevel = static_cast<signed char>(nWebpLevel);

    poDS->ScanDirectories();

    for( int i = 0; i < poDS->m_nOverviewCount; ++i )
        poDS->m_papoOverviewDS[i]->m_nWebPLevel = poDS->m_nWebPLevel;
}

/************************************************************************/
/*                     GTIFFSetJpegTablesMode()                         */
/* Called by GTIFFBuildOverviews() to set the jpeg tables mode on the   */
/* of the .ovr file.                                                    */
/************************************************************************/

void GTIFFSetJpegTablesMode( GDALDatasetH hGTIFFDS, int nJpegTablesMode )
{
    CPLAssert(
        EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset* const poDS = static_cast<GTiffDataset *>(hGTIFFDS);
    poDS->m_nJpegTablesMode = static_cast<signed char>(nJpegTablesMode);

    poDS->ScanDirectories();

    for( int i = 0; i < poDS->m_nOverviewCount; ++i )
        poDS->m_papoOverviewDS[i]->m_nJpegTablesMode = poDS->m_nJpegTablesMode;
}

/************************************************************************/
/* ==================================================================== */
/*                            GTiffRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class GTiffRasterBand CPL_NON_FINAL: public GDALPamRasterBand
{
    CPL_DISALLOW_COPY_ASSIGN(GTiffRasterBand)

    friend class GTiffDataset;

    double             m_dfOffset = 0;
    double             m_dfScale = 1;
    CPLString          m_osUnitType{};
    CPLString          m_osDescription{};
    GDALColorInterp    m_eBandInterp = GCI_Undefined;
    std::set<GTiffRasterBand **> m_aSetPSelf{};
    bool               m_bHaveOffsetScale = false;

    int                DirectIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg );

    static void     DropReferenceVirtualMem( void* pUserData );
    CPLVirtualMem * GetVirtualMemAutoInternal( GDALRWFlag eRWFlag,
                                               int *pnPixelSpace,
                                               GIntBig *pnLineSpace,
                                               char **papszOptions );

    void*           CacheMultiRange( int nXOff, int nYOff,
                                     int nXSize, int nYSize,
                                     int nBufXSize, int nBufYSize,
                                     GDALRasterIOExtraArg* psExtraArg );

protected:
    GTiffDataset       *m_poGDS = nullptr;
    GDALMultiDomainMetadata m_oGTiffMDMD{};

    double             m_dfNoDataValue = -9999.0;
    bool               m_bNoDataSet = false;

    void NullBlock( void *pData );
    CPLErr FillCacheForOtherBands( int nBlockXOff, int nBlockYOff );
#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
    void CacheMaskForBlock( int nBlockXOff, int nBlockYOff );
#endif

public:
             GTiffRasterBand( GTiffDataset *, int );
    virtual ~GTiffRasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;

    virtual int IGetDataCoverageStatus( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nMaskFlagStop,
                                        double* pdfDataPct) override;

    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override final;

    virtual const char *GetDescription() const override final;
    virtual void        SetDescription( const char * ) override final;

    virtual GDALColorInterp GetColorInterpretation() override /*final*/;
    virtual GDALColorTable *GetColorTable() override /*final*/;
    virtual CPLErr          SetColorTable( GDALColorTable * ) override final;
    virtual double          GetNoDataValue( int * ) override final;
    virtual CPLErr          SetNoDataValue( double ) override final;
    virtual CPLErr DeleteNoDataValue() override final;

    virtual double GetOffset( int *pbSuccess = nullptr ) override final;
    virtual CPLErr SetOffset( double dfNewValue ) override final;
    virtual double GetScale( int *pbSuccess = nullptr ) override final;
    virtual CPLErr SetScale( double dfNewValue ) override final;
    virtual const char* GetUnitType() override final;
    virtual CPLErr SetUnitType( const char *pszNewValue ) override final;
    virtual CPLErr SetColorInterpretation( GDALColorInterp ) override final;

    virtual char      **GetMetadataDomainList() override final;
    virtual CPLErr  SetMetadata( char **, const char * = "" ) override final;
    virtual char  **GetMetadata( const char * pszDomain = "" ) override final;
    virtual CPLErr  SetMetadataItem( const char*, const char*,
                                     const char* = "" ) override final;
    virtual const char *GetMetadataItem(
        const char * pszName, const char * pszDomain = "" ) override final;
    virtual int    GetOverviewCount()  override final;
    virtual GDALRasterBand *GetOverview( int ) override final;

    virtual GDALRasterBand *GetMaskBand() override final;
    virtual int             GetMaskFlags() override final;
    virtual CPLErr          CreateMaskBand( int nFlags )  override final;

    virtual CPLVirtualMem  *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                               int *pnPixelSpace,
                                               GIntBig *pnLineSpace,
                                               char **papszOptions )  override final;

    GDALRasterAttributeTable* GetDefaultRAT() override final;
    virtual CPLErr  GetHistogram(
        double dfMin, double dfMax,
        int nBuckets, GUIntBig * panHistogram,
        int bIncludeOutOfRange, int bApproxOK,
        GDALProgressFunc, void *pProgressData )  override final;

    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets,
                                        GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc,
                                        void *pProgressData)  override final;
};

/************************************************************************/
/*                           GTiffRasterBand()                          */
/************************************************************************/

GTiffRasterBand::GTiffRasterBand( GTiffDataset *poDSIn, int nBandIn ):
    m_poGDS(poDSIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

/* -------------------------------------------------------------------- */
/*      Get the GDAL data type.                                         */
/* -------------------------------------------------------------------- */
    const uint16_t nBitsPerSample = m_poGDS->m_nBitsPerSample;
    const uint16_t nSampleFormat = m_poGDS->m_nSampleFormat;

    eDataType = GDT_Unknown;

    if( nBitsPerSample <= 8 )
    {
        eDataType = GDT_Byte;
        if( nSampleFormat == SAMPLEFORMAT_INT )
            m_oGTiffMDMD.SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE",
                                        "IMAGE_STRUCTURE" );
    }
    else if( nBitsPerSample <= 16 )
    {
        if( nSampleFormat == SAMPLEFORMAT_INT )
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;
    }
    else if( nBitsPerSample == 32 )
    {
        if( nSampleFormat == SAMPLEFORMAT_COMPLEXINT )
            eDataType = GDT_CInt16;
        else if( nSampleFormat == SAMPLEFORMAT_IEEEFP )
            eDataType = GDT_Float32;
        else if( nSampleFormat == SAMPLEFORMAT_INT )
            eDataType = GDT_Int32;
        else
            eDataType = GDT_UInt32;
    }
    else if( nBitsPerSample == 64 )
    {
        if( nSampleFormat == SAMPLEFORMAT_IEEEFP )
            eDataType = GDT_Float64;
        else if( nSampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP )
            eDataType = GDT_CFloat32;
        else if( nSampleFormat == SAMPLEFORMAT_COMPLEXINT )
            eDataType = GDT_CInt32;
    }
    else if( nBitsPerSample == 128 )
    {
        if( nSampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP )
            eDataType = GDT_CFloat64;
    }

/* -------------------------------------------------------------------- */
/*      Try to work out band color interpretation.                      */
/* -------------------------------------------------------------------- */
    bool bLookForExtraSamples = false;

    if( m_poGDS->m_poColorTable != nullptr && nBand == 1 )
    {
        m_eBandInterp = GCI_PaletteIndex;
    }
    else if( m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB
             || (m_poGDS->m_nPhotometric == PHOTOMETRIC_YCBCR
                 && m_poGDS->m_nCompression == COMPRESSION_JPEG
                 && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                                    "YES") )) )
    {
        if( nBand == 1 )
            m_eBandInterp = GCI_RedBand;
        else if( nBand == 2 )
            m_eBandInterp = GCI_GreenBand;
        else if( nBand == 3 )
            m_eBandInterp = GCI_BlueBand;
        else
            bLookForExtraSamples = true;
    }
    else if( m_poGDS->m_nPhotometric == PHOTOMETRIC_YCBCR )
    {
        if( nBand == 1 )
            m_eBandInterp = GCI_YCbCr_YBand;
        else if( nBand == 2 )
            m_eBandInterp = GCI_YCbCr_CbBand;
        else if( nBand == 3 )
            m_eBandInterp = GCI_YCbCr_CrBand;
        else
            bLookForExtraSamples = true;
    }
    else if( m_poGDS->m_nPhotometric == PHOTOMETRIC_SEPARATED )
    {
        if( nBand == 1 )
            m_eBandInterp = GCI_CyanBand;
        else if( nBand == 2 )
            m_eBandInterp = GCI_MagentaBand;
        else if( nBand == 3 )
            m_eBandInterp = GCI_YellowBand;
        else if( nBand == 4 )
            m_eBandInterp = GCI_BlackBand;
        else
            bLookForExtraSamples = true;
    }
    else if( m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISBLACK && nBand == 1 )
    {
        m_eBandInterp = GCI_GrayIndex;
    }
    else
    {
        bLookForExtraSamples = true;
    }

    if( bLookForExtraSamples )
    {
        uint16_t *v = nullptr;
        uint16_t count = 0;

        if( TIFFGetField( m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            const int nBaseSamples = m_poGDS->m_nSamplesPerPixel - count;
            const int nExpectedBaseSamples =
                (m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISBLACK) ? 1 :
                (m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISWHITE) ? 1 :
                (m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB) ? 3 :
                (m_poGDS->m_nPhotometric == PHOTOMETRIC_YCBCR) ? 3 :
                (m_poGDS->m_nPhotometric == PHOTOMETRIC_SEPARATED) ? 4 : 0;

            if( nExpectedBaseSamples > 0 &&
                nBand == nExpectedBaseSamples + 1 &&
                nBaseSamples != nExpectedBaseSamples )
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                         "Wrong number of ExtraSamples : %d. %d were expected",
                         count, m_poGDS->m_nSamplesPerPixel - nExpectedBaseSamples);
            }

            if( nBand > nBaseSamples
                && nBand-nBaseSamples-1 < count
                && (v[nBand-nBaseSamples-1] == EXTRASAMPLE_ASSOCALPHA
                    || v[nBand-nBaseSamples-1] == EXTRASAMPLE_UNASSALPHA) )
                m_eBandInterp = GCI_AlphaBand;
            else
                m_eBandInterp = GCI_Undefined;
        }
        else
        {
            m_eBandInterp = GCI_Undefined;
        }
    }

/* -------------------------------------------------------------------- */
/*      Establish block size for strip or tiles.                        */
/* -------------------------------------------------------------------- */
    nBlockXSize = m_poGDS->m_nBlockXSize;
    nBlockYSize = m_poGDS->m_nBlockYSize;
}

/************************************************************************/
/*                          ~GTiffRasterBand()                          */
/************************************************************************/

GTiffRasterBand::~GTiffRasterBand()
{
    // So that any future DropReferenceVirtualMem() will not try to access the
    // raster band object, but this would not conform to the advertised
    // contract.
    if( !m_aSetPSelf.empty() )
    {
        ReportError( CE_Warning, CPLE_AppDefined,
                  "Virtual memory objects still exist at GTiffRasterBand "
                  "destruction" );
        std::set<GTiffRasterBand**>::iterator oIter = m_aSetPSelf.begin();
        for( ; oIter != m_aSetPSelf.end(); ++oIter )
            *(*oIter) = nullptr;
    }
}

/************************************************************************/
/*                        FetchBufferDirectIO                           */
/************************************************************************/

class FetchBufferDirectIO final
{
    VSILFILE*    fp;
    GByte       *pTempBuffer;
    size_t       nTempBufferSize;

public:
            FetchBufferDirectIO( VSILFILE* fpIn,
                                 GByte* pTempBufferIn,
                                 size_t nTempBufferSizeIn ) :
                fp(fpIn),
                pTempBuffer(pTempBufferIn),
                nTempBufferSize(nTempBufferSizeIn) {}

    const GByte* FetchBytes( vsi_l_offset nOffset,
                             int nPixels, int nDTSize,
                             bool bIsByteSwapped, bool bIsComplex,
                             int nBlockId )
    {
        if( !FetchBytes(pTempBuffer, nOffset, nPixels, nDTSize, bIsByteSwapped,
                        bIsComplex, nBlockId) )
        {
            return nullptr;
        }
        return pTempBuffer;
    }

    bool FetchBytes( GByte* pabyDstBuffer,
                     vsi_l_offset nOffset,
                     int nPixels, int nDTSize,
                     bool bIsByteSwapped, bool bIsComplex,
                     int nBlockId )
    {
        vsi_l_offset nSeekForward = 0;
        if( nOffset <= VSIFTellL(fp) ||
            (nSeekForward = nOffset - VSIFTellL(fp)) > nTempBufferSize )
        {
            if( VSIFSeekL(fp, nOffset, SEEK_SET) != 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot seek to block %d", nBlockId);
                return false;
            }
        }
        else
        {
            while( nSeekForward > 0 )
            {
                vsi_l_offset nToRead = nSeekForward;
                if( nToRead > nTempBufferSize )
                    nToRead = nTempBufferSize;
                if( VSIFReadL(pTempBuffer, static_cast<size_t>(nToRead),
                              1, fp) != 1 )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Cannot seek to block %d", nBlockId);
                    return false;
                }
                nSeekForward -= nToRead;
            }
        }
        if( VSIFReadL(pabyDstBuffer, nPixels * nDTSize, 1, fp) != 1 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Missing data for block %d", nBlockId);
            return false;
        }

        if( bIsByteSwapped )
        {
            if( bIsComplex )
                GDALSwapWords( pabyDstBuffer, nDTSize / 2, 2 * nPixels,
                               nDTSize / 2 );
            else
                GDALSwapWords( pabyDstBuffer, nDTSize, nPixels, nDTSize);
        }
        return true;
    }

    static const EMULATED_BOOL bMinimizeIO = true;
};

/************************************************************************/
/*                           DirectIO()                                 */
/************************************************************************/

// Reads directly bytes from the file using ReadMultiRange(), and by-pass
// block reading. Restricted to simple TIFF configurations
// (uncompressed data, standard data types). Particularly useful to extract
// sub-windows of data on a large /vsicurl dataset).
// Returns -1 if DirectIO() can't be supported on that file.

int GTiffRasterBand::DirectIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GDALRasterIOExtraArg* psExtraArg )
{
    const int nDTSizeBits = GDALGetDataTypeSizeBits(eDataType);
    if( !(eRWFlag == GF_Read &&
          m_poGDS->m_nCompression == COMPRESSION_NONE &&
          (m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISBLACK ||
           m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB ||
           m_poGDS->m_nPhotometric == PHOTOMETRIC_PALETTE) &&
          m_poGDS->m_nBitsPerSample == nDTSizeBits) )
    {
        return -1;
    }
    m_poGDS->Crystalize();

    // Only know how to deal with nearest neighbour in this optimized routine.
    if( (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg != nullptr &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
    {
        return -1;
    }

#if DEBUG_VERBOSE
    CPLDebug( "GTiff", "DirectIO(%d,%d,%d,%d -> %dx%d)",
              nXOff, nYOff, nXSize, nYSize,
              nBufXSize, nBufYSize );
#endif

    // Make sure that TIFFTAG_STRIPOFFSETS is up-to-date.
    if( m_poGDS->GetAccess() == GA_Update )
    {
        m_poGDS->FlushCache();
        VSI_TIFFFlushBufferedWrite( TIFFClientdata( m_poGDS->m_hTIFF ) );
    }

    if( TIFFIsTiled( m_poGDS->m_hTIFF ) )
    {
        const int nDTSize = nDTSizeBits / 8;
        const size_t nTempBufferForCommonDirectIOSize =
                static_cast<size_t>(
                    static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize * nDTSize *
                    (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG ?
                     m_poGDS->nBands : 1) );
        if( m_poGDS->m_pTempBufferForCommonDirectIO == nullptr )
        {
            m_poGDS->m_pTempBufferForCommonDirectIO =
                static_cast<GByte *>( VSI_MALLOC_VERBOSE(
                    nTempBufferForCommonDirectIOSize ) );
            if( m_poGDS->m_pTempBufferForCommonDirectIO == nullptr )
                return CE_Failure;
        }

        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( m_poGDS->m_hTIFF ));
        FetchBufferDirectIO oFetcher(fp, m_poGDS->m_pTempBufferForCommonDirectIO,
                                     nTempBufferForCommonDirectIOSize);

        return m_poGDS->CommonDirectIO(
            oFetcher,
            nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize,
            eBufType,
            1, &nBand,
            nPixelSpace, nLineSpace,
            0 );
    }

    // Get strip offsets.
    toff_t *panTIFFOffsets = nullptr;
    if( !TIFFGetField( m_poGDS->m_hTIFF, TIFFTAG_STRIPOFFSETS, &panTIFFOffsets ) ||
        panTIFFOffsets == nullptr )
    {
        return CE_Failure;
    }

    // Sub-sampling or over-sampling can only be done at last stage.
    int nReqXSize = nXSize;
    // Can do sub-sampling at the extraction stage.
    const int nReqYSize = std::min(nBufYSize, nYSize);
    // TODO(schwehr): Make ppData be GByte**.
    void** ppData = static_cast<void **>(
        VSI_MALLOC_VERBOSE(nReqYSize * sizeof(void*)) );
    vsi_l_offset* panOffsets = static_cast<vsi_l_offset *>(
        VSI_MALLOC_VERBOSE(nReqYSize * sizeof(vsi_l_offset)) );
    size_t* panSizes = static_cast<size_t *>(
        VSI_MALLOC_VERBOSE(nReqYSize * sizeof(size_t)) );
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    void* pTmpBuffer = nullptr;
    int eErr = CE_None;
    int nContigBands =
        m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG ? m_poGDS->nBands : 1;
    int nSrcPixelSize = nDTSize * nContigBands;

    if( ppData == nullptr || panOffsets == nullptr || panSizes == nullptr )
        eErr = CE_Failure;
    else if( nXSize != nBufXSize || nYSize != nBufYSize ||
             eBufType != eDataType ||
             nPixelSpace != GDALGetDataTypeSizeBytes(eBufType) ||
             nContigBands > 1 )
    {
        // We need a temporary buffer for over-sampling/sub-sampling
        // and/or data type conversion.
        pTmpBuffer = VSI_MALLOC_VERBOSE(nReqXSize * nReqYSize * nSrcPixelSize);
        if( pTmpBuffer == nullptr )
            eErr = CE_Failure;
    }

    // Prepare data extraction.
    const double dfSrcYInc = nYSize / static_cast<double>( nBufYSize );

    for( int iLine = 0; eErr == CE_None && iLine < nReqYSize; ++iLine )
    {
        if( pTmpBuffer == nullptr )
            ppData[iLine] = static_cast<GByte *>(pData) + iLine * nLineSpace;
        else
            ppData[iLine] =
                static_cast<GByte *>(pTmpBuffer) +
                iLine * nReqXSize * nSrcPixelSize;
        int nSrcLine = 0;
        if( nBufYSize < nYSize )  // Sub-sampling in y.
            nSrcLine = nYOff + static_cast<int>((iLine + 0.5) * dfSrcYInc);
        else
            nSrcLine = nYOff + iLine;

        const int nBlockXOff = 0;
        const int nBlockYOff = nSrcLine / nBlockYSize;
        const int nYOffsetInBlock = nSrcLine % nBlockYSize;
        nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
        int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
        if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
        {
            nBlockId += (nBand-1) * m_poGDS->m_nBlocksPerBand;
        }

        panOffsets[iLine] = panTIFFOffsets[nBlockId];
        if( panOffsets[iLine] == 0 )  // We don't support sparse files.
            eErr = -1;

        panOffsets[iLine] +=
            (nXOff + nYOffsetInBlock * nBlockXSize) * nSrcPixelSize;
        panSizes[iLine] = nReqXSize * nSrcPixelSize;
    }

    // Extract data from the file.
    if( eErr == CE_None )
    {
        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( m_poGDS->m_hTIFF ));
        const int nRet =
            VSIFReadMultiRangeL( nReqYSize, ppData, panOffsets, panSizes, fp );
        if( nRet != 0 )
            eErr = CE_Failure;
    }

    // Byte-swap if necessary.
    if( eErr == CE_None && TIFFIsByteSwapped(m_poGDS->m_hTIFF) )
    {
        for( int iLine = 0; iLine < nReqYSize; ++iLine )
        {
            if( GDALDataTypeIsComplex(eDataType) )
                GDALSwapWords( ppData[iLine], nDTSize / 2,
                               2 * nReqXSize * nContigBands, nDTSize / 2 );
            else
                GDALSwapWords( ppData[iLine], nDTSize,
                               nReqXSize * nContigBands, nDTSize );
        }
    }

    // Over-sampling/sub-sampling and/or data type conversion.
    const double dfSrcXInc = nXSize / static_cast<double>( nBufXSize );
    if( eErr == CE_None && pTmpBuffer != nullptr )
    {
        for( int iY=0; iY < nBufYSize; ++iY )
        {
            const int iSrcY =
                nBufYSize <= nYSize ?
                iY : static_cast<int>((iY + 0.5) * dfSrcYInc);

            GByte* pabySrcData =
                static_cast<GByte*>(ppData[iSrcY]) +
                (nContigBands > 1 ? (nBand-1) : 0) * nDTSize;
            GByte* pabyDstData =
                static_cast<GByte *>(pData) + iY * nLineSpace;
            if( nBufXSize == nXSize )
            {
                GDALCopyWords( pabySrcData,
                               eDataType,
                               nSrcPixelSize,
                               pabyDstData,
                               eBufType,
                               static_cast<int>(nPixelSpace),
                               nBufXSize );
            }
            else
            {
                if( eDataType == GDT_Byte && eBufType == GDT_Byte )
                {
                    double dfSrcX = 0.5 * dfSrcXInc;
                    for( int iX = 0; iX < nBufXSize; ++iX, dfSrcX += dfSrcXInc )
                    {
                        const int iSrcX = static_cast<int>(dfSrcX);
                        pabyDstData[iX * nPixelSpace] =
                            pabySrcData[iSrcX * nSrcPixelSize];
                    }
                }
                else
                {
                    double dfSrcX = 0.5 * dfSrcXInc;
                    for( int iX = 0; iX < nBufXSize; ++iX, dfSrcX += dfSrcXInc )
                    {
                        const int iSrcX = static_cast<int>(dfSrcX);
                        GDALCopyWords( pabySrcData + iSrcX * nSrcPixelSize,
                                       eDataType, 0,
                                       pabyDstData + iX * nPixelSpace,
                                       eBufType, 0, 1 );
                    }
                }
            }
        }
    }

    // Cleanup.
    CPLFree(pTmpBuffer);
    CPLFree(ppData);
    CPLFree(panOffsets);
    CPLFree(panSizes);

    return eErr;
}

/************************************************************************/
/*                           GetVirtualMemAuto()                        */
/************************************************************************/

CPLVirtualMem* GTiffRasterBand::GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                                   int *pnPixelSpace,
                                                   GIntBig *pnLineSpace,
                                                   char **papszOptions )
{
    const char* pszImpl = CSLFetchNameValueDef(
            papszOptions, "USE_DEFAULT_IMPLEMENTATION", "AUTO");
    if( EQUAL(pszImpl, "YES") || EQUAL(pszImpl, "ON") ||
        EQUAL(pszImpl, "1") || EQUAL(pszImpl, "TRUE") )
    {
        return GDALRasterBand::GetVirtualMemAuto( eRWFlag, pnPixelSpace,
                                                  pnLineSpace, papszOptions );
    }

    CPLVirtualMem *psRet =
        GetVirtualMemAutoInternal( eRWFlag, pnPixelSpace, pnLineSpace,
                                   papszOptions );
    if( psRet != nullptr )
    {
        CPLDebug("GTiff", "GetVirtualMemAuto(): Using memory file mapping");
        return psRet;
    }

    if( EQUAL(pszImpl, "NO") || EQUAL(pszImpl, "OFF") ||
        EQUAL(pszImpl, "0") || EQUAL(pszImpl, "FALSE") )
    {
        return nullptr;
    }

    CPLDebug("GTiff", "GetVirtualMemAuto(): Defaulting to base implementation");
    return GDALRasterBand::GetVirtualMemAuto( eRWFlag, pnPixelSpace,
                                              pnLineSpace, papszOptions );
}


/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *GTiffRasterBand::GetDefaultRAT()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    return GDALPamRasterBand::GetDefaultRAT();
}

/************************************************************************/
/*                           GetHistogram()                             */
/************************************************************************/

CPLErr GTiffRasterBand::GetHistogram(
    double dfMin, double dfMax,
    int nBuckets, GUIntBig * panHistogram,
    int bIncludeOutOfRange, int bApproxOK,
    GDALProgressFunc pfnProgress, void *pProgressData )
{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    return GDALPamRasterBand::GetHistogram( dfMin, dfMax,
                                            nBuckets, panHistogram,
                                            bIncludeOutOfRange, bApproxOK,
                                            pfnProgress, pProgressData );
}

/************************************************************************/
/*                       GetDefaultHistogram()                          */
/************************************************************************/

CPLErr GTiffRasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                             int *pnBuckets,
                                             GUIntBig ** ppanHistogram,
                                             int bForce,
                                             GDALProgressFunc pfnProgress,
                                             void *pProgressData )
{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    return GDALPamRasterBand::GetDefaultHistogram( pdfMin, pdfMax,
                                                   pnBuckets, ppanHistogram,
                                                   bForce,
                                                   pfnProgress, pProgressData );
}

/************************************************************************/
/*                     DropReferenceVirtualMem()                        */
/************************************************************************/

void GTiffRasterBand::DropReferenceVirtualMem( void* pUserData )
{
    // This function may also be called when the dataset and rasterband
    // objects have been destroyed.
    // If they are still alive, it updates the reference counter of the
    // base mapping to invalidate the pointer to it if needed.

    GTiffRasterBand** ppoSelf = static_cast<GTiffRasterBand **>( pUserData );
    GTiffRasterBand* poSelf = *ppoSelf;

    if( poSelf != nullptr )
    {
        if( --(poSelf->m_poGDS->m_nRefBaseMapping) == 0 )
        {
            poSelf->m_poGDS->m_pBaseMapping = nullptr;
        }
        poSelf->m_aSetPSelf.erase(ppoSelf);
    }
    CPLFree(pUserData);
}

/************************************************************************/
/*                     GetVirtualMemAutoInternal()                      */
/************************************************************************/

CPLVirtualMem* GTiffRasterBand::GetVirtualMemAutoInternal( GDALRWFlag eRWFlag,
                                                           int *pnPixelSpace,
                                                           GIntBig *pnLineSpace,
                                                           char **papszOptions )
{
    int nLineSize = nBlockXSize * GDALGetDataTypeSizeBytes(eDataType);
    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
        nLineSize *= m_poGDS->nBands;

    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        // In case of a pixel interleaved file, we save virtual memory space
        // by reusing a base mapping that embraces the whole imagery.
        if( m_poGDS->m_pBaseMapping != nullptr )
        {
            // Offset between the base mapping and the requested mapping.
            vsi_l_offset nOffset =
                static_cast<vsi_l_offset>(nBand - 1) *
                GDALGetDataTypeSizeBytes(eDataType);

            GTiffRasterBand** ppoSelf =
                static_cast<GTiffRasterBand** >(
                    CPLCalloc(1, sizeof(GTiffRasterBand*)) );
            *ppoSelf = this;

            CPLVirtualMem* pVMem = CPLVirtualMemDerivedNew(
                m_poGDS->m_pBaseMapping,
                nOffset,
                CPLVirtualMemGetSize(m_poGDS->m_pBaseMapping) - nOffset,
                GTiffRasterBand::DropReferenceVirtualMem,
                ppoSelf);
            if( pVMem == nullptr )
            {
                CPLFree(ppoSelf);
                return nullptr;
            }

            // Mechanism used so that the memory mapping object can be
            // destroyed after the raster band.
            m_aSetPSelf.insert(ppoSelf);
            ++m_poGDS->m_nRefBaseMapping;
            *pnPixelSpace = GDALGetDataTypeSizeBytes(eDataType);
            if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
                *pnPixelSpace *= m_poGDS->nBands;
            *pnLineSpace = nLineSize;
            return pVMem;
        }
    }

    VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( m_poGDS->m_hTIFF ));

    vsi_l_offset nLength = static_cast<vsi_l_offset>(nRasterYSize) * nLineSize;

    if( !(CPLIsVirtualMemFileMapAvailable() &&
          VSIFGetNativeFileDescriptorL(fp) != nullptr &&
#if SIZEOF_VOIDP == 4
          nLength == static_cast<size_t>(nLength) &&
#endif
          m_poGDS->m_nCompression == COMPRESSION_NONE &&
          (m_poGDS->m_nPhotometric == PHOTOMETRIC_MINISBLACK ||
           m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB ||
           m_poGDS->m_nPhotometric == PHOTOMETRIC_PALETTE) &&
          m_poGDS->m_nBitsPerSample == GDALGetDataTypeSizeBits(eDataType) &&
          !TIFFIsTiled( m_poGDS->m_hTIFF ) && !TIFFIsByteSwapped(m_poGDS->m_hTIFF)) )
    {
        return nullptr;
    }

    // Make sure that TIFFTAG_STRIPOFFSETS is up-to-date.
    if( m_poGDS->GetAccess() == GA_Update )
    {
        m_poGDS->FlushCache();
        VSI_TIFFFlushBufferedWrite( TIFFClientdata( m_poGDS->m_hTIFF ) );
    }

    // Get strip offsets.
    toff_t *panTIFFOffsets = nullptr;
    if( !TIFFGetField( m_poGDS->m_hTIFF, TIFFTAG_STRIPOFFSETS, &panTIFFOffsets ) ||
        panTIFFOffsets == nullptr )
    {
        return nullptr;
    }

    GPtrDiff_t nBlockSize =
        static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize * GDALGetDataTypeSizeBytes(eDataType);
    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
        nBlockSize *= m_poGDS->nBands;

    int nBlocks = m_poGDS->m_nBlocksPerBand;
    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlocks *= m_poGDS->nBands;
    int i = 0;  // Used after for.
    for( ; i < nBlocks; ++i )
    {
        if( panTIFFOffsets[i] != 0 )
            break;
    }
    if( i == nBlocks )
    {
        // All zeroes.
        if( m_poGDS->eAccess == GA_Update )
        {
            // Initialize the file with empty blocks so that the file has
            // the appropriate size.

            toff_t* panByteCounts = nullptr;
            if( !TIFFGetField( m_poGDS->m_hTIFF, TIFFTAG_STRIPBYTECOUNTS,
                               &panByteCounts ) ||
                panByteCounts == nullptr )
            {
                return nullptr;
            }
            if( VSIFSeekL(fp, 0, SEEK_END) != 0 )
                return nullptr;
            vsi_l_offset nBaseOffset = VSIFTellL(fp);

            // Just write one tile with libtiff to put it in appropriate state.
            GByte* pabyData =
                static_cast<GByte*>(VSI_CALLOC_VERBOSE(1, nBlockSize));
            if( pabyData == nullptr )
            {
                return nullptr;
            }
            const auto ret =
                    TIFFWriteEncodedStrip( m_poGDS->m_hTIFF, 0, pabyData,
                                           nBlockSize );
            VSI_TIFFFlushBufferedWrite( TIFFClientdata( m_poGDS->m_hTIFF ) );
            VSIFree(pabyData);
            if( ret != nBlockSize )
            {
                return nullptr;
            }
            CPLAssert(panTIFFOffsets[0] == nBaseOffset);
            CPLAssert(panByteCounts[0] == static_cast<toff_t>(nBlockSize));

            // Now simulate the writing of other blocks.
            const vsi_l_offset nDataSize =
                static_cast<vsi_l_offset>(nBlockSize) * nBlocks;
            if( VSIFTruncateL(fp, nBaseOffset + nDataSize) != 0 )
                return nullptr;

            for( i = 1; i < nBlocks; ++i)
            {
                panTIFFOffsets[i] =
                    nBaseOffset + i * static_cast<toff_t>(nBlockSize);
                panByteCounts[i] = nBlockSize;
            }
        }
        else
        {
            CPLDebug( "GTiff", "Sparse files not supported in file mapping" );
            return nullptr;
        }
    }

    GIntBig nBlockSpacing = 0;
    bool bCompatibleSpacing = true;
    toff_t nPrevOffset = 0;
    for( i = 0; i < m_poGDS->m_nBlocksPerBand; ++i )
    {
        toff_t nCurOffset = 0;
        if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
            nCurOffset =
                panTIFFOffsets[m_poGDS->m_nBlocksPerBand * (nBand - 1) + i];
        else
            nCurOffset = panTIFFOffsets[i];
        if( nCurOffset == 0 )
        {
            bCompatibleSpacing = false;
            break;
        }
        if( i > 0 )
        {
            const GIntBig nCurSpacing = nCurOffset - nPrevOffset;
            if( i == 1 )
            {
                if( nCurSpacing !=
                    static_cast<GIntBig>(nBlockYSize) * nLineSize )
                {
                    bCompatibleSpacing = false;
                    break;
                }
                nBlockSpacing = nCurSpacing;
            }
            else if( nBlockSpacing != nCurSpacing )
            {
                bCompatibleSpacing = false;
                break;
            }
        }
        nPrevOffset = nCurOffset;
    }

    if( !bCompatibleSpacing )
    {
        return nullptr;
    }

    vsi_l_offset nOffset = 0;
    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        CPLAssert( m_poGDS->m_pBaseMapping == nullptr );
        nOffset = panTIFFOffsets[0];
    }
    else
    {
        nOffset = panTIFFOffsets[m_poGDS->m_nBlocksPerBand * (nBand - 1)];
    }
    CPLVirtualMem* pVMem = CPLVirtualMemFileMapNew(
        fp, nOffset, nLength,
        eRWFlag == GF_Write ? VIRTUALMEM_READWRITE : VIRTUALMEM_READONLY,
        nullptr, nullptr);
    if( pVMem == nullptr )
    {
        return nullptr;
    }

    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        // TODO(schwehr): Revisit this block.
        m_poGDS->m_pBaseMapping = pVMem;
        pVMem = GetVirtualMemAutoInternal( eRWFlag,
                                           pnPixelSpace,
                                           pnLineSpace,
                                           papszOptions );
        // Drop ref on base mapping.
        CPLVirtualMemFree(m_poGDS->m_pBaseMapping);
        if( pVMem == nullptr )
            m_poGDS->m_pBaseMapping = nullptr;
    }
    else
    {
        *pnPixelSpace = GDALGetDataTypeSizeBytes(eDataType);
        if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
            *pnPixelSpace *= m_poGDS->nBands;
        *pnLineSpace = nLineSize;
    }
    return pVMem;
}

/************************************************************************/
/*                     HasOptimizedReadMultiRange()                     */
/************************************************************************/

bool GTiffDataset::HasOptimizedReadMultiRange()
{
    if( m_nHasOptimizedReadMultiRange >= 0 )
        return m_nHasOptimizedReadMultiRange != 0;
    m_nHasOptimizedReadMultiRange = static_cast<signed char>(
        VSIHasOptimizedReadMultiRange(m_pszFilename)
        // Config option for debug and testing purposes only
        || CPLTestBool(CPLGetConfigOption("GTIFF_HAS_OPTIMIZED_READ_MULTI_RANGE", "NO"))
    );
    return m_nHasOptimizedReadMultiRange != 0;
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GTiffDataset::IRasterIO( GDALRWFlag eRWFlag,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void * pData, int nBufXSize, int nBufYSize,
                                GDALDataType eBufType,
                                int nBandCount, int *panBandMap,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GSpacing nBandSpace,
                                GDALRasterIOExtraArg* psExtraArg )

{
    // Try to pass the request to the most appropriate overview dataset.
    if( nBufXSize < nXSize && nBufYSize < nYSize )
    {
        int bTried = FALSE;
        ++m_nJPEGOverviewVisibilityCounter;
        const CPLErr eErr =
            TryOverviewRasterIO( eRWFlag,
                                 nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize,
                                 eBufType,
                                 nBandCount, panBandMap,
                                 nPixelSpace, nLineSpace,
                                 nBandSpace,
                                 psExtraArg,
                                 &bTried );
        --m_nJPEGOverviewVisibilityCounter;
        if( bTried )
            return eErr;
    }

    if( m_eVirtualMemIOUsage != VirtualMemIOEnum::NO )
    {
        const int nErr = VirtualMemIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg );
        if( nErr >= 0 )
            return static_cast<CPLErr>(nErr);
    }
    if( m_bDirectIO )
    {
        const int nErr = DirectIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg );
        if( nErr >= 0 )
            return static_cast<CPLErr>(nErr);
    }

    void* pBufferedData = nullptr;
    if( eAccess == GA_ReadOnly &&
        eRWFlag == GF_Read &&
        m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        HasOptimizedReadMultiRange() )
    {
        pBufferedData = cpl::down_cast<GTiffRasterBand *>(
            GetRasterBand(1))->CacheMultiRange(nXOff, nYOff,
                                               nXSize, nYSize,
                                               nBufXSize, nBufYSize,
                                               psExtraArg);
    }

    ++m_nJPEGOverviewVisibilityCounter;
    const CPLErr eErr =
        GDALPamDataset::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg);
    m_nJPEGOverviewVisibilityCounter--;

    if( pBufferedData )
    {
        VSIFree( pBufferedData );
        VSI_TIFFSetCachedRanges( TIFFClientdata( m_hTIFF ),
                                 0, nullptr, nullptr, nullptr );
    }

    return eErr;
}

/************************************************************************/
/*                        FetchBufferVirtualMemIO                       */
/************************************************************************/

class FetchBufferVirtualMemIO final
{
    const GByte* pabySrcData;
    size_t       nMappingSize;
    GByte       *pTempBuffer;

public:
            FetchBufferVirtualMemIO( const GByte* pabySrcDataIn,
                                     size_t nMappingSizeIn,
                                     GByte* pTempBufferIn ) :
                pabySrcData(pabySrcDataIn),
                nMappingSize(nMappingSizeIn),
                pTempBuffer(pTempBufferIn) {}

    const GByte* FetchBytes( vsi_l_offset nOffset,
                             int nPixels, int nDTSize,
                             bool bIsByteSwapped, bool bIsComplex,
                             int nBlockId )
    {
        if( nOffset + nPixels * nDTSize > nMappingSize )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Missing data for block %d", nBlockId);
            return nullptr;
        }
        if( !bIsByteSwapped )
            return pabySrcData + nOffset;
        memcpy(pTempBuffer, pabySrcData + nOffset, nPixels * nDTSize);
        if( bIsComplex )
            GDALSwapWords( pTempBuffer, nDTSize / 2, 2 * nPixels, nDTSize / 2);
        else
            GDALSwapWords( pTempBuffer, nDTSize, nPixels, nDTSize);
        return pTempBuffer;
    }

    bool FetchBytes( GByte* pabyDstBuffer,
                     vsi_l_offset nOffset,
                     int nPixels, int nDTSize,
                     bool bIsByteSwapped, bool bIsComplex,
                     int nBlockId )
    {
        if( nOffset + nPixels * nDTSize > nMappingSize )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Missing data for block %d", nBlockId);
            return false;
        }
        memcpy(pabyDstBuffer, pabySrcData + nOffset, nPixels * nDTSize);
        if( bIsByteSwapped )
        {
            if( bIsComplex )
                GDALSwapWords( pabyDstBuffer, nDTSize / 2, 2 * nPixels,
                               nDTSize / 2);
            else
                GDALSwapWords( pabyDstBuffer, nDTSize, nPixels, nDTSize);
        }
        return true;
    }

    static const EMULATED_BOOL bMinimizeIO = false;
};

/************************************************************************/
/*                         VirtualMemIO()                               */
/************************************************************************/

int GTiffDataset::VirtualMemIO( GDALRWFlag eRWFlag,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void * pData, int nBufXSize, int nBufYSize,
                                GDALDataType eBufType,
                                int nBandCount, int *panBandMap,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GSpacing nBandSpace,
                                GDALRasterIOExtraArg* psExtraArg )
{
    if( eAccess == GA_Update || eRWFlag == GF_Write || m_bStreamingIn )
        return -1;

    // Only know how to deal with nearest neighbour in this optimized routine.
    if( (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg != nullptr &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
    {
        return -1;
    }

    const GDALDataType eDataType = GetRasterBand(1)->GetRasterDataType();
    const int nDTSizeBits = GDALGetDataTypeSizeBits(eDataType);
    if( !(m_nCompression == COMPRESSION_NONE &&
        (m_nPhotometric == PHOTOMETRIC_MINISBLACK ||
        m_nPhotometric == PHOTOMETRIC_RGB ||
        m_nPhotometric == PHOTOMETRIC_PALETTE) &&
        m_nBitsPerSample == nDTSizeBits) )
    {
        m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
        return -1;
    }

    size_t nMappingSize = 0;
    GByte* pabySrcData = nullptr;
    if( STARTS_WITH(m_pszFilename, "/vsimem/") )
    {
        vsi_l_offset nDataLength = 0;
        pabySrcData =
            VSIGetMemFileBuffer(m_pszFilename, &nDataLength, FALSE);
        nMappingSize = static_cast<size_t>(nDataLength);
        if( pabySrcData == nullptr )
            return -1;
    }
    else if( m_psVirtualMemIOMapping == nullptr )
    {
        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( m_hTIFF ));
        if( !CPLIsVirtualMemFileMapAvailable() ||
            VSIFGetNativeFileDescriptorL(fp) == nullptr )
        {
            m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
            return -1;
        }
        if( VSIFSeekL(fp, 0, SEEK_END) != 0 )
        {
            m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
            return -1;
        }
        const vsi_l_offset nLength = VSIFTellL(fp);
        if( static_cast<size_t>(nLength) != nLength )
        {
            m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
            return -1;
        }
        if( m_eVirtualMemIOUsage == VirtualMemIOEnum::IF_ENOUGH_RAM )
        {
            GIntBig nRAM = CPLGetUsablePhysicalRAM();
            if( static_cast<GIntBig>(nLength) > nRAM )
            {
                CPLDebug( "GTiff",
                          "Not enough RAM to map whole file into memory." );
                m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
                return -1;
            }
        }
        m_psVirtualMemIOMapping = CPLVirtualMemFileMapNew(
            fp, 0, nLength, VIRTUALMEM_READONLY, nullptr, nullptr);
        if( m_psVirtualMemIOMapping == nullptr )
        {
            m_eVirtualMemIOUsage = VirtualMemIOEnum::NO;
            return -1;
        }
        m_eVirtualMemIOUsage = VirtualMemIOEnum::YES;
    }

    if( m_psVirtualMemIOMapping )
    {
#ifdef DEBUG
        CPLDebug("GTiff", "Using VirtualMemIO");
#endif
        nMappingSize = CPLVirtualMemGetSize(m_psVirtualMemIOMapping);
        pabySrcData = static_cast<GByte *>(
            CPLVirtualMemGetAddr(m_psVirtualMemIOMapping) );
    }

    if( TIFFIsByteSwapped(m_hTIFF) && m_pTempBufferForCommonDirectIO == nullptr )
    {
        const int nDTSize = nDTSizeBits / 8;
        size_t nTempBufferForCommonDirectIOSize =
            static_cast<size_t>(m_nBlockXSize * nDTSize *
                (m_nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1));
        if( TIFFIsTiled(m_hTIFF) )
            nTempBufferForCommonDirectIOSize *= m_nBlockYSize;

        m_pTempBufferForCommonDirectIO =
            static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(nTempBufferForCommonDirectIOSize) );
        if( m_pTempBufferForCommonDirectIO == nullptr )
            return CE_Failure;
    }
    FetchBufferVirtualMemIO oFetcher( pabySrcData, nMappingSize,
                                      m_pTempBufferForCommonDirectIO );

    return CommonDirectIO( oFetcher,
                           nXOff, nYOff, nXSize, nYSize,
                           pData, nBufXSize, nBufYSize,
                           eBufType,
                           nBandCount, panBandMap,
                           nPixelSpace, nLineSpace,
                           nBandSpace );
}

/************************************************************************/
/*                   CopyContigByteMultiBand()                          */
/************************************************************************/

static inline void CopyContigByteMultiBand(
    const GByte* CPL_RESTRICT pabySrc, int nSrcStride,
    GByte* CPL_RESTRICT pabyDest, int nDestStride,
    int nIters, int nBandCount )
{
    if( nBandCount == 3 )
    {
        if( nSrcStride == 3 && nDestStride == 4 )
        {
            while( nIters >= 8 )
            {
                pabyDest[4*0+0] = pabySrc[3*0+0];
                pabyDest[4*0+1] = pabySrc[3*0+1];
                pabyDest[4*0+2] = pabySrc[3*0+2];
                pabyDest[4*1+0] = pabySrc[3*1+0];
                pabyDest[4*1+1] = pabySrc[3*1+1];
                pabyDest[4*1+2] = pabySrc[3*1+2];
                pabyDest[4*2+0] = pabySrc[3*2+0];
                pabyDest[4*2+1] = pabySrc[3*2+1];
                pabyDest[4*2+2] = pabySrc[3*2+2];
                pabyDest[4*3+0] = pabySrc[3*3+0];
                pabyDest[4*3+1] = pabySrc[3*3+1];
                pabyDest[4*3+2] = pabySrc[3*3+2];
                pabyDest[4*4+0] = pabySrc[3*4+0];
                pabyDest[4*4+1] = pabySrc[3*4+1];
                pabyDest[4*4+2] = pabySrc[3*4+2];
                pabyDest[4*5+0] = pabySrc[3*5+0];
                pabyDest[4*5+1] = pabySrc[3*5+1];
                pabyDest[4*5+2] = pabySrc[3*5+2];
                pabyDest[4*6+0] = pabySrc[3*6+0];
                pabyDest[4*6+1] = pabySrc[3*6+1];
                pabyDest[4*6+2] = pabySrc[3*6+2];
                pabyDest[4*7+0] = pabySrc[3*7+0];
                pabyDest[4*7+1] = pabySrc[3*7+1];
                pabyDest[4*7+2] = pabySrc[3*7+2];
                pabySrc += 3 * 8;
                pabyDest += 4 * 8;
                nIters -= 8;
            }
            while( nIters-- > 0 )
            {
                pabyDest[0] = pabySrc[0];
                pabyDest[1] = pabySrc[1];
                pabyDest[2] = pabySrc[2];
                pabySrc += 3;
                pabyDest += 4;
            }
        }
        else
        {
            while( nIters-- > 0 )
            {
                pabyDest[0] = pabySrc[0];
                pabyDest[1] = pabySrc[1];
                pabyDest[2] = pabySrc[2];
                pabySrc += nSrcStride;
                pabyDest += nDestStride;
            }
        }
    }
    else
    {
        while( nIters-- > 0 )
        {
            for( int iBand = 0; iBand < nBandCount; ++iBand )
                pabyDest[iBand] = pabySrc[iBand];
            pabySrc += nSrcStride;
            pabyDest += nDestStride;
        }
    }
}

/************************************************************************/
/*                         CommonDirectIO()                             */
/************************************************************************/

// #define DEBUG_REACHED_VIRTUAL_MEM_IO
#ifdef DEBUG_REACHED_VIRTUAL_MEM_IO
static int anReachedVirtualMemIO[52] = { 0 };
#define REACHED(x) anReachedVirtualMemIO[x] = 1
#else
#define REACHED(x)
#endif

template<class FetchBuffer> CPLErr GTiffDataset::CommonDirectIO(
    FetchBuffer& oFetcher,
    int nXOff, int nYOff, int nXSize, int nYSize,
    void * pData, int nBufXSize, int nBufYSize,
    GDALDataType eBufType,
    int nBandCount, int *panBandMap,
    GSpacing nPixelSpace, GSpacing nLineSpace,
    GSpacing nBandSpace )
{
    const GDALDataType eDataType = GetRasterBand(1)->GetRasterDataType();
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    const bool bIsComplex = CPL_TO_BOOL(GDALDataTypeIsComplex(eDataType));
    const int nBufDTSize = GDALGetDataTypeSizeBytes(eBufType);

    // Get strip offsets.
    toff_t *panOffsets = nullptr;
    if( !TIFFGetField( m_hTIFF, (TIFFIsTiled( m_hTIFF )) ?
                       TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS,
                       &panOffsets ) ||
        panOffsets == nullptr )
    {
        return CE_Failure;
    }

    bool bUseContigImplementation =
        m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        nBandCount > 1 &&
        nBandSpace == nBufDTSize;
    if( bUseContigImplementation )
    {
        for( int iBand = 0; iBand < nBandCount; ++iBand )
        {
            const int nBand = panBandMap[iBand];
            if( nBand != iBand + 1 )
            {
                bUseContigImplementation = false;
                break;
            }
        }
    }

    const int nBandsPerBlock =
        m_nPlanarConfig == PLANARCONFIG_SEPARATE ? 1 : nBands;
    const int nBandsPerBlockDTSize = nBandsPerBlock * nDTSize;
    const int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, m_nBlockXSize);
    const bool bNoTypeChange = (eDataType == eBufType);
    const bool bNoXResampling = (nXSize == nBufXSize );
    const bool bNoXResamplingNoTypeChange = (bNoTypeChange && bNoXResampling);
    const bool bByteOnly = (bNoTypeChange && nDTSize == 1 );
    const bool bByteNoXResampling = ( bByteOnly && bNoXResamplingNoTypeChange );
    const bool bIsByteSwapped = CPL_TO_BOOL(TIFFIsByteSwapped(m_hTIFF));
    const double dfSrcXInc = nXSize / static_cast<double>( nBufXSize );
    const double dfSrcYInc = nYSize / static_cast<double>( nBufYSize );

    int bNoDataSetIn = FALSE;
    double dfNoData = GetRasterBand(1)->GetNoDataValue( &bNoDataSetIn );
    GByte abyNoData = 0;
    if( !bNoDataSetIn )
        dfNoData = 0;
    else if( dfNoData >= 0 && dfNoData <= 255 )
        abyNoData = static_cast<GByte>(dfNoData + 0.5);

    if( FetchBuffer::bMinimizeIO &&
             TIFFIsTiled( m_hTIFF ) && bNoXResampling && (nYSize == nBufYSize ) &&
             m_nPlanarConfig == PLANARCONFIG_CONTIG && nBandCount > 1 )
    {
        GByte* pabyData = static_cast<GByte *>(pData);
        for( int y = 0; y < nBufYSize; )
        {
            const int nSrcLine = nYOff + y;
            const int nBlockYOff = nSrcLine / m_nBlockYSize;
            const int nYOffsetInBlock = nSrcLine % m_nBlockYSize;
            const int nUsedBlockHeight =
                std::min( nBufYSize - y,
                          m_nBlockYSize - nYOffsetInBlock );

            int nBlockXOff = nXOff / m_nBlockXSize;
            int nXOffsetInBlock = nXOff % m_nBlockXSize;
            int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

            int x = 0;
            while( x < nBufXSize )
            {
                const toff_t nCurOffset = panOffsets[nBlockId];
                const int nUsedBlockWidth =
                    std::min( m_nBlockXSize - nXOffsetInBlock,
                              nBufXSize - x );

                if( nCurOffset == 0 )
                {
                    REACHED(30);
                    for( int k = 0; k < nUsedBlockHeight; ++k )
                    {
                        GByte* pabyLocalData =
                            pabyData + (y + k) * nLineSpace + x * nPixelSpace;
                        for( int iBand = 0; iBand < nBandCount; ++iBand )
                        {
                            GByte* pabyLocalDataBand =
                                pabyLocalData + iBand * nBandSpace;

                            GDALCopyWords(
                                &dfNoData, GDT_Float64, 0,
                                pabyLocalDataBand, eBufType,
                                static_cast<int>(nPixelSpace),
                                nUsedBlockWidth );
                        }
                    }
                }
                else
                {
                    const int nByteOffsetInBlock =
                        nYOffsetInBlock * m_nBlockXSize * nBandsPerBlockDTSize;
                    const GByte* pabyLocalSrcDataK0 = oFetcher.FetchBytes(
                            nCurOffset + nByteOffsetInBlock,
                            m_nBlockXSize *
                            nUsedBlockHeight * nBandsPerBlock,
                            nDTSize, bIsByteSwapped, bIsComplex, nBlockId);
                    if( pabyLocalSrcDataK0 == nullptr )
                        return CE_Failure;

                    for( int k = 0; k < nUsedBlockHeight; ++k )
                    {
                        GByte* pabyLocalData =
                            pabyData + (y + k) * nLineSpace + x * nPixelSpace;
                        const GByte* pabyLocalSrcData =
                            pabyLocalSrcDataK0 +
                            (k * m_nBlockXSize + nXOffsetInBlock) *
                            nBandsPerBlockDTSize;

                        if( bUseContigImplementation && nBands == nBandCount &&
                            nPixelSpace == nBandsPerBlockDTSize )
                        {
                            REACHED(31);
                            GDALCopyWords( pabyLocalSrcData,
                                           eDataType, nDTSize,
                                           pabyLocalData,
                                           eBufType, nBufDTSize,
                                           nUsedBlockWidth * nBands );
                        }
                        else
                        {
                            REACHED(32);
                            for( int iBand = 0; iBand < nBandCount; ++iBand )
                            {
                                GByte* pabyLocalDataBand =
                                    pabyLocalData + iBand * nBandSpace;
                                const GByte* pabyLocalSrcDataBand =
                                    pabyLocalSrcData +
                                    (panBandMap[iBand]-1) * nDTSize;

                                GDALCopyWords(
                                    pabyLocalSrcDataBand,
                                    eDataType, nBandsPerBlockDTSize,
                                    pabyLocalDataBand,
                                    eBufType, static_cast<int>(nPixelSpace),
                                    nUsedBlockWidth );
                            }
                        }
                    }
                }

                nXOffsetInBlock = 0;
                ++nBlockXOff;
                ++nBlockId;
                x += nUsedBlockWidth;
            }

            y += nUsedBlockHeight;
        }
    }
    else if( FetchBuffer::bMinimizeIO &&
             TIFFIsTiled( m_hTIFF ) && bNoXResampling &&
             (nYSize == nBufYSize ) )
             // && (m_nPlanarConfig == PLANARCONFIG_SEPARATE || nBandCount == 1) )
    {
        for( int iBand = 0; iBand < nBandCount; ++iBand )
        {
            GByte* pabyData = static_cast<GByte *>(pData) + iBand * nBandSpace;
            const int nBand = panBandMap[iBand];
            for( int y = 0; y < nBufYSize; )
            {
                const int nSrcLine = nYOff + y;
                const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                const int nUsedBlockHeight =
                    std::min( nBufYSize - y,
                              m_nBlockYSize - nYOffsetIm_nBlock);

                int nBlockXOff = nXOff / m_nBlockXSize;
                int nXOffsetInBlock = nXOff % m_nBlockXSize;
                int nBlockId = nBlockXOff + m_nBlockYOff * nBlocksPerRow;
                if( m_nPlanarConfig == PLANARCONFIG_SEPARATE )
                {
                    REACHED(33);
                    nBlockId += m_nBlocksPerBand * (nBand - 1);
                }
                else
                {
                    REACHED(34);
                }

                int x = 0;
                while( x < nBufXSize )
                {
                    const toff_t nCurOffset = panOffsets[nBlockId];
                    const int nUsedBlockWidth =
                        std::min(
                            m_nBlockXSize - nXOffsetInBlock,
                            nBufXSize - x);

                    if( nCurOffset == 0 )
                    {
                        REACHED(35);
                        for( int k = 0; k < nUsedBlockHeight; ++k )
                        {
                            GByte* pabyLocalData =
                                pabyData + (y + k) * nLineSpace + x * nPixelSpace;

                            GDALCopyWords(
                                &dfNoData, GDT_Float64, 0,
                                pabyLocalData, eBufType,
                                static_cast<int>(nPixelSpace),
                                nUsedBlockWidth );
                        }
                    }
                    else
                    {
                        const int nByteOffsetIm_nBlock =
                            nYOffsetIm_nBlock * m_nBlockXSize *
                            nBandsPerBlockDTSize;
                        const GByte* pabyLocalSrcDataK0 =
                            oFetcher.FetchBytes(
                                nCurOffset + nByteOffsetIm_nBlock,
                                m_nBlockXSize *
                                nUsedBlockHeight * nBandsPerBlock,
                                nDTSize, bIsByteSwapped, bIsComplex, nBlockId);
                        if( pabyLocalSrcDataK0 == nullptr )
                            return CE_Failure;

                        if( m_nPlanarConfig == PLANARCONFIG_CONTIG )
                        {
                            REACHED(36);
                            pabyLocalSrcDataK0 += (nBand - 1) * nDTSize;
                        }
                        else
                        {
                            REACHED(37);
                        }

                        for( int k = 0; k < nUsedBlockHeight; ++k )
                        {
                            GByte* pabyLocalData =
                                pabyData + (y + k) * nLineSpace +
                                x * nPixelSpace;
                            const GByte* pabyLocalSrcData =
                                pabyLocalSrcDataK0 +
                                (k * m_nBlockXSize + nXOffsetInBlock) *
                                nBandsPerBlockDTSize;

                            GDALCopyWords(
                                pabyLocalSrcData,
                                eDataType, nBandsPerBlockDTSize,
                                pabyLocalData,
                                eBufType, static_cast<int>(nPixelSpace),
                                nUsedBlockWidth);
                        }
                    }

                    nXOffsetInBlock = 0;
                    ++nBlockXOff;
                    ++nBlockId;
                    x += nUsedBlockWidth;
                }

                y += nUsedBlockHeight;
            }
        }
    }
    else if( FetchBuffer::bMinimizeIO &&
             TIFFIsTiled( m_hTIFF ) &&
             m_nPlanarConfig == PLANARCONFIG_CONTIG && nBandCount > 1 )
    {
        GByte* pabyData = static_cast<GByte *>(pData);
        int anSrcYOffset[256] = { 0 };
        for( int y = 0; y < nBufYSize; )
        {
            const double dfYOffStart = nYOff + (y + 0.5) * dfSrcYInc;
            const int nSrcLine = static_cast<int>(dfYOffStart);
            const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
            const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
            const int nBaseByteOffsetIm_nBlock =
                nYOffsetIm_nBlock * m_nBlockXSize * nBandsPerBlockDTSize;
            int ychunk = 1;
            int nLastSrcLineK = nSrcLine;
            anSrcYOffset[0] = 0;
            for( int k = 1; k < nBufYSize - y; ++k )
            {
                int nSrcLineK =
                    nYOff + static_cast<int>((y + k + 0.5) * dfSrcYInc);
                const int nBlockYOffK = nSrcLineK / m_nBlockYSize;
                if( k < 256)
                    anSrcYOffset[k] =
                        ((nSrcLineK % m_nBlockYSize) - nYOffsetIm_nBlock) *
                        m_nBlockXSize * nBandsPerBlockDTSize;
                if( nBlockYOffK != m_nBlockYOff )
                {
                    break;
                }
                ++ychunk;
                nLastSrcLineK = nSrcLineK;
            }
            const int nUsedBlockHeight = nLastSrcLineK - nSrcLine + 1;
            // CPLAssert(nUsedBlockHeight <= m_nBlockYSize);

            double dfSrcX = nXOff + 0.5 * dfSrcXInc;
            int nCurBlockXOff = 0;
            int nNextBlockXOff = 0;
            toff_t nCurOffset = 0;
            const GByte* pabyLocalSrcDataStartLine = nullptr;
            for( int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc)
            {
                const int nSrcPixel = static_cast<int>(dfSrcX);
                if( nSrcPixel >= nNextBlockXOff )
                {
                    const int nBlockXOff = nSrcPixel / m_nBlockXSize;
                    nCurBlockXOff = nBlockXOff * m_nBlockXSize;
                    nNextBlockXOff = nCurBlockXOff + m_nBlockXSize;
                    const int nBlockId =
                        nBlockXOff + m_nBlockYOff * nBlocksPerRow;
                    nCurOffset = panOffsets[nBlockId];
                    if( nCurOffset != 0 )
                    {
                        pabyLocalSrcDataStartLine =
                            oFetcher.FetchBytes(
                                nCurOffset + nBaseByteOffsetIm_nBlock,
                                m_nBlockXSize *
                                nBandsPerBlock * nUsedBlockHeight,
                                nDTSize,
                                bIsByteSwapped, bIsComplex, nBlockId);
                        if( pabyLocalSrcDataStartLine == nullptr )
                            return CE_Failure;
                    }
                }

                if( nCurOffset == 0 )
                {
                    REACHED(38);

                    for( int k = 0; k < ychunk; ++k )
                    {
                        GByte* const pabyLocalData =
                            pabyData + (y + k) * nLineSpace + x * nPixelSpace;
                        for( int iBand = 0; iBand < nBandCount; ++iBand )
                        {
                            GDALCopyWords(
                                &dfNoData, GDT_Float64, 0,
                                pabyLocalData + nBandSpace * iBand, eBufType, 0,
                                1);
                        }
                    }
                }
                else
                {
                    const int nXOffsetInBlock = nSrcPixel - nCurBlockXOff;
                    double dfYOff = dfYOffStart;
                    const GByte* const pabyLocalSrcDataK0 =
                        pabyLocalSrcDataStartLine +
                        nXOffsetInBlock * nBandsPerBlockDTSize;
                    GByte* pabyLocalData =
                        pabyData + y * nLineSpace + x * nPixelSpace;
                    for( int k = 0;
                         k < ychunk;
                         ++k, pabyLocalData += nLineSpace )
                    {
                        const GByte* pabyLocalSrcData = nullptr;
                        if( ychunk <= 256 )
                        {
                            REACHED(39);
                            pabyLocalSrcData =
                                pabyLocalSrcDataK0 + anSrcYOffset[k];
                        }
                        else
                        {
                            REACHED(40);
                            const int nYOffsetIm_nBlockK =
                                static_cast<int>(dfYOff) % m_nBlockYSize;
                            // CPLAssert(
                            //     nYOffsetIm_nBlockK - nYOffsetIm_nBlock <=
                            //     nUsedBlockHeight);
                            pabyLocalSrcData =
                                pabyLocalSrcDataK0 +
                                (nYOffsetIm_nBlockK - nYOffsetIm_nBlock) *
                                m_nBlockXSize * nBandsPerBlockDTSize;
                            dfYOff += dfSrcYInc;
                        }

                        if( bByteOnly )
                        {
                            REACHED(41);
                            for( int iBand=0; iBand < nBandCount; ++iBand )
                            {
                                GByte* pabyLocalDataBand =
                                    pabyLocalData + iBand * nBandSpace;
                                const GByte* pabyLocalSrcDataBand =
                                    pabyLocalSrcData + (panBandMap[iBand]-1);
                                *pabyLocalDataBand = *pabyLocalSrcDataBand;
                            }
                        }
                        else
                        {
                            REACHED(42);
                            for( int iBand = 0; iBand < nBandCount; ++iBand )
                            {
                                GByte* pabyLocalDataBand =
                                    pabyLocalData + iBand * nBandSpace;
                                const GByte* pabyLocalSrcDataBand =
                                    pabyLocalSrcData +
                                    (panBandMap[iBand]-1) * nDTSize;

                                GDALCopyWords( pabyLocalSrcDataBand,
                                               eDataType, 0,
                                               pabyLocalDataBand,
                                               eBufType, 0,
                                               1 );
                            }
                        }
                    }
                }
            }

            y += ychunk;
        }
    }
    else if( FetchBuffer::bMinimizeIO &&
             TIFFIsTiled( m_hTIFF ) )
             // && (m_nPlanarConfig == PLANARCONFIG_SEPARATE || nBandCount == 1) )
    {
        for( int iBand = 0; iBand < nBandCount; ++iBand )
        {
            GByte* pabyData = static_cast<GByte*>(pData) + iBand * nBandSpace;
            const int nBand = panBandMap[iBand];
            int anSrcYOffset[256] = { 0 };
            for( int y = 0; y < nBufYSize; )
            {
                const double dfYOffStart = nYOff + (y + 0.5) * dfSrcYInc;
                const int nSrcLine = static_cast<int>(dfYOffStart);
                const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                const int nBaseByteOffsetIm_nBlock =
                    nYOffsetIm_nBlock * m_nBlockXSize * nBandsPerBlockDTSize;
                int ychunk = 1;
                int nLastSrcLineK = nSrcLine;
                anSrcYOffset[0] = 0;
                for( int k = 1; k < nBufYSize - y; ++k )
                {
                    const int nSrcLineK =
                        nYOff + static_cast<int>((y + k + 0.5) * dfSrcYInc);
                    const int nBlockYOffK = nSrcLineK / m_nBlockYSize;
                    if( k < 256)
                        anSrcYOffset[k] =
                            ((nSrcLineK % m_nBlockYSize) - nYOffsetIm_nBlock) *
                            m_nBlockXSize * nBandsPerBlockDTSize;
                    if( nBlockYOffK != m_nBlockYOff )
                    {
                        break;
                    }
                    ++ychunk;
                    nLastSrcLineK = nSrcLineK;
                }
                const int nUsedBlockHeight = nLastSrcLineK - nSrcLine + 1;
                // CPLAssert(nUsedBlockHeight <= m_nBlockYSize);

                double dfSrcX = nXOff + 0.5 * dfSrcXInc;
                int nCurBlockXOff = 0;
                int nNextBlockXOff = 0;
                toff_t nCurOffset = 0;
                const GByte* pabyLocalSrcDataStartLine = nullptr;
                for( int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc )
                {
                    int nSrcPixel = static_cast<int>(dfSrcX);
                    if( nSrcPixel >= nNextBlockXOff )
                    {
                        const int nBlockXOff = nSrcPixel / m_nBlockXSize;
                        nCurBlockXOff = nBlockXOff * m_nBlockXSize;
                        nNextBlockXOff = nCurBlockXOff + m_nBlockXSize;
                        int nBlockId = nBlockXOff + m_nBlockYOff * nBlocksPerRow;
                        if( m_nPlanarConfig == PLANARCONFIG_SEPARATE )
                        {
                            REACHED(43);
                            nBlockId += m_nBlocksPerBand * (nBand - 1);
                        }
                        else
                        {
                            REACHED(44);
                        }
                        nCurOffset = panOffsets[nBlockId];
                        if( nCurOffset != 0 )
                        {
                            pabyLocalSrcDataStartLine =
                                oFetcher.FetchBytes(
                                    nCurOffset + nBaseByteOffsetIm_nBlock,
                                    m_nBlockXSize *
                                    nBandsPerBlock * nUsedBlockHeight,
                                    nDTSize,
                                    bIsByteSwapped, bIsComplex, nBlockId);
                            if( pabyLocalSrcDataStartLine == nullptr )
                                return CE_Failure;

                            if( m_nPlanarConfig == PLANARCONFIG_CONTIG )
                            {
                                REACHED(45);
                                pabyLocalSrcDataStartLine +=
                                    (nBand - 1) * nDTSize;
                            }
                            else
                            {
                                REACHED(46);
                            }
                        }
                    }

                    if( nCurOffset == 0 )
                    {
                        REACHED(47);

                        for( int k = 0; k < ychunk; ++k )
                        {
                            GByte* const pabyLocalData =
                                pabyData + (y + k) * nLineSpace + x * nPixelSpace;

                            GDALCopyWords( &dfNoData, GDT_Float64, 0,
                                           pabyLocalData, eBufType, 0,
                                           1 );
                        }
                    }
                    else
                    {
                        const int nXOffsetInBlock = nSrcPixel - nCurBlockXOff;
                        double dfYOff = dfYOffStart;
                        const GByte* const pabyLocalSrcDataK0 =
                            pabyLocalSrcDataStartLine +
                            nXOffsetInBlock * nBandsPerBlockDTSize;
                        GByte* pabyLocalData =
                            pabyData + y * nLineSpace + x * nPixelSpace;
                        for( int k = 0;
                             k < ychunk;
                             ++k, pabyLocalData += nLineSpace )
                        {
                            const GByte* pabyLocalSrcData = nullptr;
                            if( ychunk <= 256 )
                            {
                                REACHED(48);
                                pabyLocalSrcData =
                                    pabyLocalSrcDataK0 + anSrcYOffset[k];
                            }
                            else
                            {
                                REACHED(49);
                                const int nYOffsetIm_nBlockK =
                                    static_cast<int>(dfYOff) % m_nBlockYSize;
                                // CPLAssert(
                                //     nYOffsetIm_nBlockK - nYOffsetIm_nBlock <=
                                //     nUsedBlockHeight);
                                pabyLocalSrcData = pabyLocalSrcDataK0 +
                                    (nYOffsetIm_nBlockK - nYOffsetIm_nBlock) *
                                    m_nBlockXSize * nBandsPerBlockDTSize;
                                dfYOff += dfSrcYInc;
                            }

                            if( bByteOnly )
                            {
                                REACHED(50);

                                *pabyLocalData = *pabyLocalSrcData;
                            }
                            else
                            {
                                REACHED(51);

                                GDALCopyWords( pabyLocalSrcData,
                                               eDataType, 0,
                                               pabyLocalData,
                                               eBufType, 0,
                                               1 );
                            }
                        }
                    }
                }

                y += ychunk;
            }
        }
    }
    else if( bUseContigImplementation )
    {
        if( !FetchBuffer::bMinimizeIO && TIFFIsTiled( m_hTIFF ) )
        {
            GByte* pabyData = static_cast<GByte *>(pData);
            for( int y = 0; y < nBufYSize; ++y )
            {
                const int nSrcLine =
                    nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                const int nBaseByteOffsetIm_nBlock =
                    nYOffsetIm_nBlock * m_nBlockXSize * nBandsPerBlockDTSize;

                if( bNoXResampling )
                {
                    GByte* pabyLocalData = pabyData + y * nLineSpace;
                    int nBlockXOff = nXOff / m_nBlockXSize;
                    int nXOffsetInBlock = nXOff % m_nBlockXSize;
                    int nBlockId = nBlockXOff + m_nBlockYOff * nBlocksPerRow;

                    int x = 0;
                    while( x < nBufXSize )
                    {
                        const int nByteOffsetIm_nBlock = nBaseByteOffsetIm_nBlock +
                                        nXOffsetInBlock * nBandsPerBlockDTSize;
                        const toff_t nCurOffset = panOffsets[nBlockId];
                        const int nUsedBlockWidth =
                            std::min(
                                m_nBlockXSize - nXOffsetInBlock,
                                nBufXSize - x);

                        int nIters = nUsedBlockWidth;
                        if( nCurOffset == 0 )
                        {
                            if( bByteNoXResampling )
                            {
                                REACHED(0);
                                while( nIters-- > 0 )
                                {
                                    for( int iBand = 0;
                                         iBand < nBandCount;
                                         ++iBand )
                                    {
                                        pabyLocalData[iBand] = abyNoData;
                                    }
                                    pabyLocalData += nPixelSpace;
                                }
                            }
                            else
                            {
                                REACHED(1);
                                while( nIters-- > 0 )
                                {
                                    GDALCopyWords(
                                        &dfNoData, GDT_Float64, 0,
                                        pabyLocalData, eBufType,
                                        static_cast<int>(nBandSpace),
                                        nBandCount);
                                    pabyLocalData += nPixelSpace;
                                }
                            }
                        }
                        else
                        {
                            if( bNoTypeChange && nBands == nBandCount &&
                                nPixelSpace == nBandsPerBlockDTSize )
                            {
                                REACHED(2);
                                if( !oFetcher.FetchBytes(
                                        pabyLocalData,
                                        nCurOffset + nByteOffsetIm_nBlock,
                                        nIters * nBandsPerBlock, nDTSize,
                                        bIsByteSwapped, bIsComplex, nBlockId) )
                                {
                                    return CE_Failure;
                                }
                                pabyLocalData +=
                                    nIters * nBandsPerBlock * nDTSize;
                            }
                            else
                            {
                                const GByte* pabyLocalSrcData =
                                    oFetcher.FetchBytes(
                                        nCurOffset + nByteOffsetIm_nBlock,
                                        nIters * nBandsPerBlock, nDTSize,
                                        bIsByteSwapped, bIsComplex, nBlockId);
                                if( pabyLocalSrcData == nullptr )
                                    return CE_Failure;
                                if( bByteNoXResampling )
                                {
                                    REACHED(3);
                                    CopyContigByteMultiBand(
                                        pabyLocalSrcData,
                                        nBandsPerBlockDTSize,
                                        pabyLocalData,
                                        static_cast<int>(nPixelSpace),
                                        nIters,
                                        nBandCount);
                                    pabyLocalData += nIters * nPixelSpace;
                                }
                                else
                                {
                                    REACHED(4);
                                    while( nIters-- > 0 )
                                    {
                                        GDALCopyWords(
                                            pabyLocalSrcData,
                                            eDataType, nDTSize,
                                            pabyLocalData,
                                            eBufType,
                                            static_cast<int>(nBandSpace),
                                            nBandCount);
                                        pabyLocalSrcData +=
                                            nBandsPerBlockDTSize;
                                        pabyLocalData += nPixelSpace;
                                    }
                                }
                            }
                        }

                        nXOffsetInBlock = 0;
                        ++nBlockXOff;
                        ++nBlockId;
                        x += nUsedBlockWidth;
                    }
                }
                else  // Contig, tiled, potential resampling & data type change.
                {
                    const GByte* pabyLocalSrcDataStartLine = nullptr;
                    GByte* pabyLocalData = pabyData + y * nLineSpace;
                    double dfSrcX = nXOff + 0.5 * dfSrcXInc;
                    int nCurBlockXOff = 0;
                    int nNextBlockXOff = 0;
                    toff_t nCurOffset = 0;
                    for( int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc )
                    {
                        int nSrcPixel = static_cast<int>(dfSrcX);
                        if( nSrcPixel >= nNextBlockXOff )
                        {
                            const int nBlockXOff = nSrcPixel / m_nBlockXSize;
                            nCurBlockXOff = nBlockXOff * m_nBlockXSize;
                            nNextBlockXOff = nCurBlockXOff + m_nBlockXSize;
                            const int nBlockId =
                                nBlockXOff + m_nBlockYOff * nBlocksPerRow;
                            nCurOffset = panOffsets[nBlockId];
                            if( nCurOffset != 0 )
                            {
                                pabyLocalSrcDataStartLine =
                                    oFetcher.FetchBytes(
                                        nCurOffset + nBaseByteOffsetIm_nBlock,
                                        m_nBlockXSize *
                                        nBandsPerBlock,
                                        nDTSize,
                                        bIsByteSwapped, bIsComplex, nBlockId);
                                if( pabyLocalSrcDataStartLine == nullptr )
                                    return CE_Failure;
                            }
                        }
                        const int nXOffsetInBlock = nSrcPixel - nCurBlockXOff;

                        if( nCurOffset == 0 )
                        {
                            REACHED(5);
                            GDALCopyWords(
                                &dfNoData, GDT_Float64, 0,
                                pabyLocalData,
                                eBufType,
                                static_cast<int>(nBandSpace),
                                nBandCount );
                            pabyLocalData += nPixelSpace;
                        }
                        else
                        {
                            const GByte* pabyLocalSrcData =
                                pabyLocalSrcDataStartLine +
                                nXOffsetInBlock * nBandsPerBlockDTSize;

                            REACHED(6);
                            if( bByteOnly )
                            {
                                for( int iBand = 0; iBand < nBands; ++iBand )
                                    pabyLocalData[iBand] =
                                        pabyLocalSrcData[iBand];
                            }
                            else
                            {
                                GDALCopyWords(
                                    pabyLocalSrcData,
                                    eDataType, nDTSize,
                                    pabyLocalData,
                                    eBufType,
                                    static_cast<int>(nBandSpace),
                                    nBandCount );
                            }
                            pabyLocalData += nPixelSpace;
                        }
                    }
                }
            }
        }
        else  // Contig, striped organized.
        {
            GByte* pabyData = static_cast<GByte*>(pData);
            for( int y = 0; y < nBufYSize; ++y )
            {
                const int nSrcLine =
                    nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                const int nBlockId = m_nBlockYOff;
                const toff_t nCurOffset = panOffsets[nBlockId];
                if( nCurOffset == 0 )
                {
                    REACHED(7);
                    for( int x = 0; x < nBufXSize; ++x )
                    {
                        GDALCopyWords(
                            &dfNoData, GDT_Float64, 0,
                            pabyData + y * nLineSpace + x * nPixelSpace,
                            eBufType, static_cast<int>(nBandSpace),
                            nBandCount);
                    }
                }
                else
                {
                    GByte* pabyLocalData = pabyData + y * nLineSpace;
                    const int nBaseByteOffsetIm_nBlock =
                        (nYOffsetIm_nBlock * m_nBlockXSize + nXOff) *
                        nBandsPerBlockDTSize;

                    if( bNoXResamplingNoTypeChange && nBands == nBandCount &&
                        nPixelSpace == nBandsPerBlockDTSize )
                    {
                        REACHED(8);
                        if( !oFetcher.FetchBytes(
                               pabyLocalData,
                               nCurOffset + nBaseByteOffsetIm_nBlock,
                               nXSize * nBandsPerBlock, nDTSize, bIsByteSwapped,
                               bIsComplex, nBlockId) )
                        {
                            return CE_Failure;
                        }
                    }
                    else
                    {
                        const GByte* pabyLocalSrcData = oFetcher.FetchBytes(
                            nCurOffset + nBaseByteOffsetIm_nBlock,
                            nXSize * nBandsPerBlock, nDTSize, bIsByteSwapped,
                            bIsComplex, nBlockId);
                        if( pabyLocalSrcData == nullptr )
                            return CE_Failure;

                        if( bByteNoXResampling )
                        {
                            REACHED(9);
                            CopyContigByteMultiBand(
                                pabyLocalSrcData,
                                nBandsPerBlockDTSize,
                                pabyLocalData,
                                static_cast<int>(nPixelSpace),
                                nBufXSize,
                                nBandCount);
                        }
                        else if( bByteOnly )
                        {
                            REACHED(10);
                            double dfSrcX = 0.5 * dfSrcXInc;
                            for( int x = 0;
                                 x < nBufXSize;
                                 ++x, dfSrcX += dfSrcXInc )
                            {
                                const int nSrcPixelMinusXOff =
                                    static_cast<int>(dfSrcX);
                                for( int iBand = 0;
                                     iBand < nBandCount;
                                     ++iBand )
                                {
                                    pabyLocalData[x * nPixelSpace + iBand] =
                                        pabyLocalSrcData[nSrcPixelMinusXOff *
                                                         nBandsPerBlockDTSize +
                                                         iBand];
                                }
                            }
                        }
                        else
                        {
                            REACHED(11);
                            double dfSrcX = 0.5 * dfSrcXInc;
                            for( int x = 0;
                                 x < nBufXSize;
                                 ++x, dfSrcX += dfSrcXInc )
                            {
                                int nSrcPixelMinusXOff =
                                    static_cast<int>(dfSrcX);
                                GDALCopyWords(
                                    pabyLocalSrcData +
                                    nSrcPixelMinusXOff * nBandsPerBlockDTSize,
                                    eDataType, nDTSize,
                                    pabyLocalData + x * nPixelSpace,
                                    eBufType,
                                    static_cast<int>(nBandSpace),
                                    nBandCount );
                            }
                        }
                    }
                }
            }
        }
    }
    else  // Non-contig reading case.
    {
        if( !FetchBuffer::bMinimizeIO && TIFFIsTiled( m_hTIFF ) )
        {
            for( int iBand = 0; iBand < nBandCount; ++iBand )
            {
                const int nBand = panBandMap[iBand];
                GByte* const pabyData =
                    static_cast<GByte*>(pData) + iBand * nBandSpace;
                for( int y = 0; y < nBufYSize; ++y )
                {
                    const int nSrcLine =
                        nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                    const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                    const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;

                    int nBaseByteOffsetIm_nBlock =
                        nYOffsetIm_nBlock * m_nBlockXSize * nBandsPerBlockDTSize;
                    if( m_nPlanarConfig == PLANARCONFIG_CONTIG )
                    {
                        REACHED(12);
                        nBaseByteOffsetIm_nBlock += (nBand - 1) * nDTSize;
                    }
                    else
                    {
                        REACHED(13);
                    }

                    if( bNoXResampling )
                    {
                        GByte* pabyLocalData = pabyData + y * nLineSpace;
                        int nBlockXOff = nXOff / m_nBlockXSize;
                        int nBlockId = nBlockXOff + m_nBlockYOff * nBlocksPerRow;
                        if( m_nPlanarConfig == PLANARCONFIG_SEPARATE )
                        {
                            REACHED(14);
                            nBlockId += m_nBlocksPerBand * (nBand - 1);
                        }
                        else
                        {
                            REACHED(15);
                        }
                        int nXOffsetInBlock = nXOff % m_nBlockXSize;

                        int x = 0;
                        while( x < nBufXSize )
                        {
                            const int nByteOffsetIm_nBlock =
                                nBaseByteOffsetIm_nBlock +
                                nXOffsetInBlock * nBandsPerBlockDTSize;
                            const toff_t nCurOffset = panOffsets[nBlockId];
                            const int nUsedBlockWidth =
                                std::min(
                                    m_nBlockXSize - nXOffsetInBlock,
                                    nBufXSize - x );
                            int nIters = nUsedBlockWidth;

                            if( nCurOffset == 0 )
                            {
                                REACHED(16);
                                GDALCopyWords(
                                    &dfNoData, GDT_Float64, 0,
                                    pabyLocalData, eBufType,
                                    static_cast<int>(nPixelSpace),
                                    nIters);
                                pabyLocalData += nIters * nPixelSpace;
                            }
                            else
                            {
                                if( bNoTypeChange &&
                                    nPixelSpace == nBandsPerBlockDTSize )
                                {
                                    REACHED(17);
                                    if( !oFetcher.FetchBytes(
                                           pabyLocalData,
                                           nCurOffset + nByteOffsetIm_nBlock,
                                           (nIters - 1) * nBandsPerBlock + 1,
                                           nDTSize,
                                           bIsByteSwapped, bIsComplex,
                                           nBlockId) )
                                    {
                                        return CE_Failure;
                                    }
                                    pabyLocalData += nIters * nPixelSpace;
                                }
                                else
                                {
                                    const GByte* pabyLocalSrcData =
                                        oFetcher.FetchBytes(
                                            nCurOffset + nByteOffsetIm_nBlock,
                                            (nIters - 1) * nBandsPerBlock + 1,
                                            nDTSize,
                                            bIsByteSwapped,
                                            bIsComplex,
                                            nBlockId );
                                    if( pabyLocalSrcData == nullptr )
                                        return CE_Failure;

                                    REACHED(18);
                                    GDALCopyWords(
                                        pabyLocalSrcData, eDataType,
                                        nBandsPerBlockDTSize,
                                        pabyLocalData, eBufType,
                                        static_cast<int>(nPixelSpace),
                                        nIters );
                                    pabyLocalData += nIters * nPixelSpace;
                                }
                            }

                            nXOffsetInBlock = 0;
                            ++nBlockXOff;
                            ++nBlockId;
                            x += nUsedBlockWidth;
                        }
                    }
                    else
                    {
                        // Non-contig reading, tiled, potential resampling and
                        // data type change.

                        const GByte* pabyLocalSrcDataStartLine = nullptr;
                        GByte* pabyLocalData = pabyData + y * nLineSpace;
                        double dfSrcX = nXOff + 0.5 * dfSrcXInc;
                        int nCurBlockXOff = 0;
                        int nNextBlockXOff = 0;
                        toff_t nCurOffset = 0;
                        for( int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc)
                        {
                            const int nSrcPixel = static_cast<int>(dfSrcX);
                            if( nSrcPixel >= nNextBlockXOff )
                            {
                                const int nBlockXOff = nSrcPixel / m_nBlockXSize;
                                nCurBlockXOff = nBlockXOff * m_nBlockXSize;
                                nNextBlockXOff = nCurBlockXOff + m_nBlockXSize;
                                int nBlockId =
                                    nBlockXOff + m_nBlockYOff * nBlocksPerRow;
                                if( m_nPlanarConfig == PLANARCONFIG_SEPARATE )
                                {
                                    REACHED(19);
                                    nBlockId += m_nBlocksPerBand * (nBand - 1);
                                }
                                else
                                {
                                    REACHED(20);
                                }
                                nCurOffset = panOffsets[nBlockId];
                                if( nCurOffset != 0 )
                                {
                                    pabyLocalSrcDataStartLine =
                                        oFetcher.FetchBytes(
                                            nCurOffset + nBaseByteOffsetIm_nBlock,
                                            m_nBlockXSize * nBandsPerBlock,
                                            nDTSize,
                                            bIsByteSwapped,
                                            bIsComplex,
                                            nBlockId);
                                    if( pabyLocalSrcDataStartLine == nullptr )
                                        return CE_Failure;
                                }
                            }
                            const int nXOffsetInBlock =
                                nSrcPixel - nCurBlockXOff;

                            if( nCurOffset == 0 )
                            {
                                REACHED(21);
                                GDALCopyWords( &dfNoData, GDT_Float64, 0,
                                               pabyLocalData,
                                               eBufType, 0,
                                               1 );
                                pabyLocalData += nPixelSpace;
                            }
                            else
                            {
                                const GByte* pabyLocalSrcData =
                                    pabyLocalSrcDataStartLine +
                                    nXOffsetInBlock * nBandsPerBlockDTSize;

                                REACHED(22);
                                if( bByteOnly )
                                {
                                    *pabyLocalData = *pabyLocalSrcData;
                                }
                                else
                                {
                                    GDALCopyWords(pabyLocalSrcData,
                                                eDataType, 0,
                                                pabyLocalData,
                                                eBufType, 0,
                                                1);
                                }
                                pabyLocalData += nPixelSpace;
                            }
                        }
                    }
                }
            }
        }
        else  // Non-contig reading, striped.
        {
            for( int iBand = 0; iBand < nBandCount; ++iBand )
            {
                const int nBand = panBandMap[iBand];
                GByte* pabyData =
                    static_cast<GByte *>(pData) + iBand * nBandSpace;
                for( int y = 0; y < nBufYSize; ++y )
                {
                    const int nSrcLine =
                        nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                    const int m_nBlockYOff = nSrcLine / m_nBlockYSize;
                    const int nYOffsetIm_nBlock = nSrcLine % m_nBlockYSize;
                    int nBlockId = m_nBlockYOff;
                    if( m_nPlanarConfig == PLANARCONFIG_SEPARATE )
                    {
                        REACHED(23);
                        nBlockId += m_nBlocksPerBand * (nBand - 1);
                    }
                    else
                    {
                        REACHED(24);
                    }
                    const toff_t nCurOffset = panOffsets[nBlockId];
                    if( nCurOffset == 0 )
                    {
                        REACHED(25);
                        GDALCopyWords(
                            &dfNoData, GDT_Float64, 0,
                            pabyData + y * nLineSpace,
                            eBufType,
                            static_cast<int>(nPixelSpace),
                            nBufXSize);
                    }
                    else
                    {
                        int nBaseByteOffsetIm_nBlock =
                            (nYOffsetIm_nBlock * m_nBlockXSize + nXOff) *
                            nBandsPerBlockDTSize;
                        if( m_nPlanarConfig == PLANARCONFIG_CONTIG )
                            nBaseByteOffsetIm_nBlock += (nBand - 1) * nDTSize;

                        GByte* pabyLocalData = pabyData + y * nLineSpace;
                        if( bNoXResamplingNoTypeChange &&
                            nPixelSpace == nBandsPerBlockDTSize )
                        {
                            REACHED(26);
                            if( !oFetcher.FetchBytes(
                                pabyLocalData,
                                nCurOffset + nBaseByteOffsetIm_nBlock,
                                (nXSize - 1) * nBandsPerBlock + 1, nDTSize,
                                bIsByteSwapped, bIsComplex, nBlockId) )
                            {
                                return CE_Failure;
                            }
                        }
                        else
                        {
                            const GByte* pabyLocalSrcData = oFetcher.FetchBytes(
                                nCurOffset + nBaseByteOffsetIm_nBlock,
                                (nXSize - 1) * nBandsPerBlock + 1, nDTSize,
                                bIsByteSwapped, bIsComplex, nBlockId);
                            if( pabyLocalSrcData == nullptr )
                                return CE_Failure;

                            if( bNoXResamplingNoTypeChange )
                            {
                                REACHED(27);
                                GDALCopyWords(pabyLocalSrcData,
                                              eDataType, nBandsPerBlockDTSize,
                                              pabyLocalData, eBufType,
                                              static_cast<int>(nPixelSpace),
                                              nBufXSize);
                            }
                            else if( bByteOnly )
                            {
                                REACHED(28);
                                double dfSrcX = 0.5 * dfSrcXInc;
                                for( int x = 0;
                                     x < nBufXSize;
                                     ++x, dfSrcX += dfSrcXInc )
                                {
                                    const int nSrcPixelMinusXOff =
                                        static_cast<int>(dfSrcX);
                                    pabyLocalData[x * nPixelSpace] =
                                        pabyLocalSrcData[nSrcPixelMinusXOff *
                                                         nBandsPerBlockDTSize];
                                }
                            }
                            else
                            {
                                REACHED(29);
                                double dfSrcX = 0.5 * dfSrcXInc;
                                for( int x = 0;
                                     x < nBufXSize;
                                     ++x, dfSrcX += dfSrcXInc )
                                {
                                    const int nSrcPixelMinusXOff =
                                        static_cast<int>(dfSrcX);
                                    GDALCopyWords(
                                        pabyLocalSrcData +
                                        nSrcPixelMinusXOff *
                                        nBandsPerBlockDTSize,
                                        eDataType, 0,
                                        pabyLocalData + x * nPixelSpace,
                                        eBufType, 0,
                                        1 );
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                           DirectIO()                                 */
/************************************************************************/

// Reads directly bytes from the file using ReadMultiRange(), and by-pass
// block reading. Restricted to simple TIFF configurations
// (uncompressed data, standard data types). Particularly useful to extract
// sub-windows of data on a large /vsicurl dataset).
// Returns -1 if DirectIO() can't be supported on that file.

int GTiffDataset::DirectIO( GDALRWFlag eRWFlag,
                            int nXOff, int nYOff, int nXSize, int nYSize,
                            void * pData, int nBufXSize, int nBufYSize,
                            GDALDataType eBufType,
                            int nBandCount, int *panBandMap,
                            GSpacing nPixelSpace, GSpacing nLineSpace,
                            GSpacing nBandSpace,
                            GDALRasterIOExtraArg* psExtraArg )
{
    const GDALDataType eDataType = GetRasterBand(1)->GetRasterDataType();
    const int nDTSizeBits = GDALGetDataTypeSizeBits(eDataType);
    if( !(eRWFlag == GF_Read &&
          m_nCompression == COMPRESSION_NONE &&
          (m_nPhotometric == PHOTOMETRIC_MINISBLACK ||
           m_nPhotometric == PHOTOMETRIC_RGB ||
           m_nPhotometric == PHOTOMETRIC_PALETTE) &&
          m_nBitsPerSample == nDTSizeBits) )
    {
        return -1;
    }
    Crystalize();

    // Only know how to deal with nearest neighbour in this optimized routine.
    if( (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg != nullptr &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
    {
        return -1;
    }

    // If the file is band interleave or only one band is requested, then
    // fallback to band DirectIO.
    bool bUseBandRasterIO = false;
    if( m_nPlanarConfig == PLANARCONFIG_SEPARATE || nBandCount == 1 )
    {
        bUseBandRasterIO = true;
    }
    else
    {
        // For simplicity, only deals with "naturally ordered" bands.
        for( int iBand = 0; iBand < nBandCount; ++iBand )
        {
            if( panBandMap[iBand] != iBand + 1)
            {
                bUseBandRasterIO = true;
                break;
            }
        }
    }
    if( bUseBandRasterIO )
    {
        CPLErr eErr = CE_None;
        for( int iBand = 0; eErr == CE_None && iBand < nBandCount; ++iBand )
        {
            eErr = GetRasterBand(panBandMap[iBand])->RasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                static_cast<GByte *>(pData) + iBand * nBandSpace,
                nBufXSize, nBufYSize,
                eBufType,
                nPixelSpace, nLineSpace,
                psExtraArg );
        }
        return eErr;
    }

#if DEBUG_VERBOSE
    CPLDebug( "GTiff", "DirectIO(%d,%d,%d,%d -> %dx%d)",
              nXOff, nYOff, nXSize, nYSize,
              nBufXSize, nBufYSize );
#endif

    // No need to look if overviews can satisfy the request as it has already */
    // been done in GTiffDataset::IRasterIO().

    // Make sure that TIFFTAG_STRIPOFFSETS is up-to-date.
    if( GetAccess() == GA_Update )
    {
        FlushCache();
        VSI_TIFFFlushBufferedWrite( TIFFClientdata( m_hTIFF ) );
    }

    if( TIFFIsTiled( m_hTIFF ) )
    {
        const int nDTSize = nDTSizeBits / 8;
        const size_t nTempBufferForCommonDirectIOSize =
            static_cast<size_t>(static_cast<GPtrDiff_t>(m_nBlockXSize) * m_nBlockYSize * nDTSize *
            ((m_nPlanarConfig == PLANARCONFIG_CONTIG) ? nBands : 1));
        if( m_pTempBufferForCommonDirectIO == nullptr )
        {
            m_pTempBufferForCommonDirectIO =
                static_cast<GByte *>(
                    VSI_MALLOC_VERBOSE(nTempBufferForCommonDirectIOSize) );
            if( m_pTempBufferForCommonDirectIO == nullptr )
                return CE_Failure;
        }

        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( m_hTIFF ));
        FetchBufferDirectIO oFetcher(fp, m_pTempBufferForCommonDirectIO,
                                     nTempBufferForCommonDirectIOSize);

        return CommonDirectIO( oFetcher,
                               nXOff, nYOff, nXSize, nYSize,
                               pData, nBufXSize, nBufYSize,
                               eBufType,
                               nBandCount, panBandMap,
                               nPixelSpace, nLineSpace,
                              nBandSpace );
    }

    // Get strip offsets.
    toff_t *panTIFFOffsets = nullptr;
    if( !TIFFGetField( m_hTIFF, TIFFTAG_STRIPOFFSETS, &panTIFFOffsets ) ||
        panTIFFOffsets == nullptr )
    {
        return CE_Failure;
    }

    // Sub-sampling or over-sampling can only be done at last stage.
    int nReqXSize = nXSize;
    // Can do sub-sampling at the extraction stage.
    const int nReqYSize = std::min(nBufYSize, nYSize);
    void** ppData =
        static_cast<void **>( VSI_MALLOC_VERBOSE(nReqYSize * sizeof(void*)) );
    vsi_l_offset* panOffsets =
        static_cast<vsi_l_offset *>(
            VSI_MALLOC_VERBOSE(nReqYSize * sizeof(vsi_l_offset)) );
    size_t* panSizes =
        static_cast<size_t *>( VSI_MALLOC_VERBOSE(nReqYSize * sizeof(size_t)) );
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    void* pTmpBuffer = nullptr;
    int eErr = CE_None;
    int nContigBands = nBands;
    const int nSrcPixelSize = nDTSize * nContigBands;

    if( ppData == nullptr || panOffsets == nullptr || panSizes == nullptr )
    {
        eErr = CE_Failure;
    }
    // For now we always allocate a temp buffer as it is easier.
    else
        // if( nXSize != nBufXSize || nYSize != nBufYSize ||
        //   eBufType != eDataType ||
        //   nPixelSpace != GDALGetDataTypeSizeBytes(eBufType) ||
        //   check if the user buffer is large enough )
    {
        // We need a temporary buffer for over-sampling/sub-sampling
        // and/or data type conversion.
        pTmpBuffer = VSI_MALLOC_VERBOSE(nReqXSize * nReqYSize * nSrcPixelSize);
        if( pTmpBuffer == nullptr )
            eErr = CE_Failure;
    }

    // Prepare data extraction.
    const double dfSrcYInc = nYSize / static_cast<double>( nBufYSize );

    for( int iLine = 0; eErr == CE_None && iLine < nReqYSize; ++iLine )
    {
        ppData[iLine] =
            static_cast<GByte *>(pTmpBuffer) +
            iLine * nReqXSize * nSrcPixelSize;
        int nSrcLine = 0;
        if( nBufYSize < nYSize )  // Sub-sampling in y.
            nSrcLine = nYOff + static_cast<int>((iLine + 0.5) * dfSrcYInc);
        else
            nSrcLine = nYOff + iLine;

        const int nBlockXOff = 0;
        const int nBlockYOff = nSrcLine / m_nBlockYSize;
        const int nYOffsetInBlock = nSrcLine % m_nBlockYSize;
        const int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, m_nBlockXSize);
        const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

        panOffsets[iLine] = panTIFFOffsets[nBlockId];
        if( panOffsets[iLine] == 0)  // We don't support sparse files.
            eErr = -1;

        panOffsets[iLine] +=
            (nXOff + nYOffsetInBlock * m_nBlockXSize) * nSrcPixelSize;
        panSizes[iLine] = nReqXSize * nSrcPixelSize;
    }

    // Extract data from the file.
    if( eErr == CE_None )
    {
        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( m_hTIFF ));
        const int nRet =
            VSIFReadMultiRangeL(nReqYSize, ppData, panOffsets, panSizes, fp);
        if( nRet != 0 )
            eErr = CE_Failure;
    }

    // Byte-swap if necessary.
    if( eErr == CE_None && TIFFIsByteSwapped(m_hTIFF) )
    {
        for( int iLine = 0; iLine < nReqYSize; ++iLine )
        {
            if( GDALDataTypeIsComplex(eDataType) )
                GDALSwapWords( ppData[iLine], nDTSize / 2,
                               2 * nReqXSize * nContigBands, nDTSize / 2);
            else
                GDALSwapWords( ppData[iLine], nDTSize,
                               nReqXSize * nContigBands, nDTSize);
        }
    }

    // Over-sampling/sub-sampling and/or data type conversion.
    const double dfSrcXInc = nXSize / static_cast<double>( nBufXSize );
    if( eErr == CE_None && pTmpBuffer != nullptr )
    {
        for( int iY = 0; iY < nBufYSize; ++iY )
        {
            const int iSrcY =
                nBufYSize <= nYSize ?
                iY : static_cast<int>((iY + 0.5) * dfSrcYInc);
            // Optimization: no resampling, no data type change, number of
            // bands requested == number of bands and buffer is packed
            // pixel-interleaved.
            if( nBufXSize == nXSize && nContigBands == nBandCount &&
                eDataType == eBufType &&
                nBandSpace == nDTSize &&
                nPixelSpace == nBandCount * nBandSpace )
            {
                memcpy(
                    static_cast<GByte *>(pData) + iY * nLineSpace,
                    ppData[iSrcY],
                    static_cast<size_t>(nReqXSize * nPixelSpace) );
            }
            // Other optimization: no resampling, no data type change,
            // data type is Byte.
            else if( nBufXSize == nXSize &&
                     eDataType == eBufType && eDataType == GDT_Byte )
            {
                GByte* pabySrcData = static_cast<GByte *>(ppData[iSrcY]);
                GByte* pabyDstData =
                    static_cast<GByte *>(pData) + iY * nLineSpace;
                if( nBandSpace == 1 && nPixelSpace > nBandCount )
                {
                    // Buffer is pixel-interleaved (with some stridding
                    // between pixels).
                    CopyContigByteMultiBand( pabySrcData, nSrcPixelSize,
                                             pabyDstData,
                                             static_cast<int>(nPixelSpace),
                                             nBufXSize, nBandCount );
                }
                else
                {
                    for( int iBand = 0; iBand < nBandCount; ++iBand )
                    {
                        GDALCopyWords(
                            pabySrcData + iBand, GDT_Byte, nSrcPixelSize,
                            pabyDstData + iBand * nBandSpace,
                            GDT_Byte, static_cast<int>(nPixelSpace),
                            nBufXSize );
                    }
                }
            }
            else  // General case.
            {
                for( int iBand = 0; iBand < nBandCount; ++iBand )
                {
                    GByte* pabySrcData =
                        static_cast<GByte *>(ppData[iSrcY]) + iBand * nDTSize;
                    GByte* pabyDstData =
                        static_cast<GByte *>(pData) +
                        iBand * nBandSpace + iY * nLineSpace;
                    if( eDataType == GDT_Byte && eBufType == GDT_Byte )
                    {
                        double dfSrcX = 0.5 * dfSrcXInc;
                        for( int iX = 0;
                             iX < nBufXSize;
                             ++iX, dfSrcX += dfSrcXInc)
                        {
                            int iSrcX = static_cast<int>(dfSrcX);
                            pabyDstData[iX * nPixelSpace] =
                                pabySrcData[iSrcX * nSrcPixelSize];
                        }
                    }
                    else
                    {
                        double dfSrcX = 0.5 * dfSrcXInc;
                        for( int iX = 0;
                             iX < nBufXSize;
                             ++iX, dfSrcX += dfSrcXInc)
                        {
                            int iSrcX = static_cast<int>(dfSrcX);
                            GDALCopyWords( pabySrcData + iSrcX * nSrcPixelSize,
                                        eDataType, 0,
                                        pabyDstData + iX * nPixelSpace,
                                        eBufType, 0, 1);
                        }
                    }
                }
            }
        }
    }

    CPLFree(pTmpBuffer);
    CPLFree(ppData);
    CPLFree(panOffsets);
    CPLFree(panSizes);

    return eErr;
}

/************************************************************************/
/*                         CacheMultiRange()                            */
/************************************************************************/

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
static bool CheckTrailer(const GByte* strileData, vsi_l_offset nStrileSize)
{
    GByte abyTrailer[4];
    memcpy(abyTrailer,strileData + nStrileSize, 4);
    GByte abyLastBytes[4] = {};
    if( nStrileSize >= 4 )
        memcpy(abyLastBytes, strileData + nStrileSize - 4, 4);
    else
    {
        // The last bytes will be zero due to the above {} initialization,
        // and that's what should be in abyTrailer too when the trailer is
        // correct.
        memcpy(abyLastBytes, strileData, static_cast<size_t>(nStrileSize));
    }
    return memcmp(abyTrailer, abyLastBytes, 4) == 0;
}
#endif

void* GTiffRasterBand::CacheMultiRange( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nBufXSize, int nBufYSize,
                                        GDALRasterIOExtraArg* psExtraArg )
{
    void* pBufferedData = nullptr;
    // Same logic as in GDALRasterBand::IRasterIO()
    double dfXOff = nXOff;
    double dfYOff = nYOff;
    double dfXSize = nXSize;
    double dfYSize = nYSize;
    if( psExtraArg->bFloatingPointWindowValidity )
    {
        dfXOff = psExtraArg->dfXOff;
        dfYOff = psExtraArg->dfYOff;
        dfXSize = psExtraArg->dfXSize;
        dfYSize = psExtraArg->dfYSize;
    }
    const double dfSrcXInc = dfXSize / static_cast<double>( nBufXSize );
    const double dfSrcYInc = dfYSize / static_cast<double>( nBufYSize );
    const double EPS = 1e-10;
    const int nBlockX1 = static_cast<int>(std::max(0.0, (0+0.5) * dfSrcXInc + dfXOff + EPS)) / nBlockXSize;
    const int nBlockY1 = static_cast<int>(std::max(0.0, (0+0.5) * dfSrcYInc + dfYOff + EPS)) / nBlockYSize;
    const int nBlockX2 = static_cast<int>(std::min(static_cast<double>(nRasterXSize - 1), (nBufXSize-1+0.5) * dfSrcXInc + dfXOff + EPS)) / nBlockXSize;
    const int nBlockY2 = static_cast<int>(std::min(static_cast<double>(nRasterYSize - 1), (nBufYSize-1+0.5) * dfSrcYInc + dfYOff + EPS)) / nBlockYSize;
#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
    const int nBlockXCount = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    const int nBlockYCount = DIV_ROUND_UP(nRasterYSize, nBlockYSize);
    const int nBlockCount = nBlockXCount * nBlockYCount;
    struct StrileData
    {
        vsi_l_offset nOffset;
        vsi_l_offset nByteCount;
        bool         bTryMask;
    };
    std::map<int, StrileData> oMapStrileToOffsetByteCount;

    // Dedicated method to retrieved the offset and size in an efficient way
    // when m_bBlockOrderRowMajor and m_bLeaderSizeAsUInt4 conditions are
    // met.
    // Except for the last block, we just read the offset from the TIFF offset
    // array, and retrieve the size in the leader 4 bytes that come before the
    // payload.
    auto OptimizedRetrievalOfOffsetSize = [&](int nBlockId,
                                               vsi_l_offset& nOffset,
                                               vsi_l_offset& nSize,
                                               size_t nTotalSize,
                                               size_t nMaxRawBlockCacheSize)
    {
        bool bTryMask = m_poGDS->m_bMaskInterleavedWithImagery;
        nOffset = TIFFGetStrileOffset(m_poGDS->m_hTIFF, nBlockId);
        if( nOffset >= 4 )
        {
            if( nBlockId == nBlockCount - 1 )
            {
                // Special case for the last block. As there is no next block
                // from which to retrieve an offset, use the good old method
                // that consists in reading the ByteCount array.
                if( bTryMask &&
                    m_poGDS->GetRasterBand(1)->GetMaskBand() &&
                    m_poGDS->m_poMaskDS )
                {
                    auto nMaskOffset = TIFFGetStrileOffset(m_poGDS->m_poMaskDS->m_hTIFF, nBlockId);
                    if( nMaskOffset )
                    {
                        nSize = nMaskOffset + TIFFGetStrileByteCount(m_poGDS->m_poMaskDS->m_hTIFF, nBlockId) - nOffset;
                    }
                    else
                    {
                        bTryMask = false;
                    }
                }
                if( nSize == 0 )
                {
                    nSize = TIFFGetStrileByteCount(m_poGDS->m_hTIFF, nBlockId);
                }
                if( nSize && m_poGDS->m_bTrailerRepeatedLast4BytesRepeated )
                {
                    nSize += 4;
                }
            }
            else
            {
                auto nOffsetNext = TIFFGetStrileOffset(m_poGDS->m_hTIFF, nBlockId + 1);
                if( nOffsetNext > nOffset )
                {
                    nSize = nOffsetNext - nOffset;
                }
                else
                {
                    // Shouldn't happen for a compliant file
                    if( nOffsetNext != 0 )
                    {
                        CPLDebug("GTiff",
                                    "Tile %d is not located after %d", nBlockId + 1, nBlockId);
                    }
                    bTryMask = false;
                    nSize = TIFFGetStrileByteCount(m_poGDS->m_hTIFF, nBlockId);
                    if( m_poGDS->m_bTrailerRepeatedLast4BytesRepeated )
                        nSize += 4;
                }
            }
            if( nSize )
            {
                nOffset -= 4;
                nSize += 4;
                if( nTotalSize + nSize < nMaxRawBlockCacheSize )
                {
                    StrileData data;
                    data.nOffset = nOffset;
                    data.nByteCount = nSize;
                    data.bTryMask = bTryMask;
                    oMapStrileToOffsetByteCount[nBlockId] = data;
                }
            }
        }
        else
        {
            // Sparse tile
            StrileData data;
            data.nOffset = 0;
            data.nByteCount = 0;
            data.bTryMask = false;
            oMapStrileToOffsetByteCount[nBlockId] = data;
        }
    };

    // This lambda fills m_poDS->m_oCacheStrileToOffsetByteCount (and
    // m_poDS->m_poMaskDS->m_oCacheStrileToOffsetByteCount, when there is a mask)
    // from the temporary oMapStrileToOffsetByteCount.
    auto FillCacheStrileToOffsetByteCount = [&](
        const std::vector<vsi_l_offset>& anOffsets,
        const std::vector<size_t>& anSizes,
        const std::vector<void*> apData)
    {
        CPLAssert( m_poGDS->m_bLeaderSizeAsUInt4 );
        size_t i = 0;
        vsi_l_offset nLastOffset = 0;
        for( const auto& entry: oMapStrileToOffsetByteCount )
        {
            const auto nBlockId = entry.first;
            const auto nOffset = entry.second.nOffset;
            const auto nSize = entry.second.nByteCount;
            if( nOffset == 0 )
            {
                // Sparse tile
                m_poGDS->m_oCacheStrileToOffsetByteCount.insert(
                    nBlockId,
                    std::pair<vsi_l_offset, vsi_l_offset>(0, 0));
                continue;
            }

            if( nOffset < nLastOffset )
            {
                // shouldn't happen normally if tiles are sorted
                i = 0;
            }
            nLastOffset = nOffset;
            while( i < anOffsets.size() && !(
                    nOffset >= anOffsets[i] &&
                    nOffset + nSize <= anOffsets[i] + anSizes[i]) )
            {
                i++;
            }
            CPLAssert( i < anOffsets.size() );
            CPLAssert( nOffset >= anOffsets[i] );
            CPLAssert( nOffset + nSize <= anOffsets[i] + anSizes[i] );
            GUInt32 nSizeFromLeader;
            memcpy(&nSizeFromLeader,
                    static_cast<GByte*>(apData[i]) + nOffset - anOffsets[i],
                    sizeof(nSizeFromLeader));
            CPL_LSBPTR32(&nSizeFromLeader);
            bool bOK = true;
            constexpr int nLeaderSize = 4;
            const int nTrailerSize =
                (m_poGDS->m_bTrailerRepeatedLast4BytesRepeated ? 4 : 0);
            if( nSizeFromLeader > nSize - nLeaderSize - nTrailerSize )
            {
                CPLDebug("GTiff",
                            "Inconsistent block size from in leader of block %d", nBlockId);
                bOK = false;
            }
            else if( m_poGDS->m_bTrailerRepeatedLast4BytesRepeated )
            {
                // Check trailer consistency
                const GByte* strileData = static_cast<GByte*>(
                    apData[i]) + nOffset - anOffsets[i] + nLeaderSize;
                if( !CheckTrailer(strileData, nSizeFromLeader) )
                {
                    CPLDebug("GTiff",
                            "Inconsistent trailer of block %d", nBlockId);
                    bOK = false;
                }
            }
            if( !bOK )
            {
                return false;
            }

            {
                const vsi_l_offset nRealOffset = nOffset + nLeaderSize;
                const vsi_l_offset nRealSize = nSizeFromLeader;
#ifdef DEBUG_VERBOSE
                CPLDebug("GTiff", "Block %d found at offset "
                            CPL_FRMT_GUIB " with size " CPL_FRMT_GUIB,
                            nBlockId, nRealOffset, nRealSize);
#endif
                m_poGDS->m_oCacheStrileToOffsetByteCount.insert(
                    nBlockId,
                    std::pair<vsi_l_offset, vsi_l_offset>(nRealOffset, nRealSize));
            }

            // Processing of mask
            if( !(entry.second.bTryMask &&
                  m_poGDS->m_bMaskInterleavedWithImagery &&
                  m_poGDS->GetRasterBand(1)->GetMaskBand() &&
                  m_poGDS->m_poMaskDS ) )
            {
                continue;
            }

            bOK = false;
            const vsi_l_offset nMaskOffsetWithLeader =
                nOffset + nLeaderSize + nSizeFromLeader + nTrailerSize;
            if( nMaskOffsetWithLeader + nLeaderSize <= anOffsets[i] + anSizes[i] )
            {
                GUInt32 nMaskSizeFromLeader;
                memcpy(&nMaskSizeFromLeader,
                        static_cast<GByte*>(apData[i]) + nMaskOffsetWithLeader - anOffsets[i],
                        sizeof(nMaskSizeFromLeader));
                CPL_LSBPTR32(&nMaskSizeFromLeader);
                if( nMaskOffsetWithLeader + nLeaderSize + nMaskSizeFromLeader + nTrailerSize <= anOffsets[i] + anSizes[i] )
                {
                    bOK = true;
                    if( m_poGDS->m_bTrailerRepeatedLast4BytesRepeated )
                    {
                        // Check trailer consistency
                        const GByte* strileMaskData = static_cast<GByte*>(
                            apData[i]) + nOffset - anOffsets[i] + nLeaderSize + nSizeFromLeader + nTrailerSize + nLeaderSize;
                        if( !CheckTrailer(strileMaskData, nMaskSizeFromLeader) )
                        {
                            CPLDebug("GTiff",
                                "Inconsistent trailer of mask of block %d", nBlockId);
                            bOK = false;
                        }
                    }
                }
                if( bOK )
                {
                    const vsi_l_offset nRealOffset = nOffset + nLeaderSize + nSizeFromLeader + nTrailerSize + nLeaderSize;
                    const vsi_l_offset nRealSize = nMaskSizeFromLeader;
#ifdef DEBUG_VERBOSE
                    CPLDebug("GTiff", "Mask of block %d found at offset "
                            CPL_FRMT_GUIB " with size " CPL_FRMT_GUIB,
                            nBlockId, nRealOffset, nRealSize);
#endif

                    m_poGDS->m_poMaskDS->m_oCacheStrileToOffsetByteCount.insert(
                        nBlockId,
                        std::pair<vsi_l_offset, vsi_l_offset>(nRealOffset, nRealSize));
                }
            }
            if( !bOK )
            {
                CPLDebug("GTiff",
                          "Mask for block %d is not properly interleaved with imagery block",
                          nBlockId);
            }
        }
        return true;
    };
#endif

    thandle_t th = TIFFClientdata( m_poGDS->m_hTIFF );
    if( !VSI_TIFFHasCachedRanges(th) )
    {
        std::vector< std::pair<vsi_l_offset, size_t> > aOffsetSize;
        size_t nTotalSize = 0;
        nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
        const unsigned int nMaxRawBlockCacheSize =
            atoi(CPLGetConfigOption("GDAL_MAX_RAW_BLOCK_CACHE_SIZE",
                                    "10485760"));
        bool bGoOn = true;
        for( int iY = nBlockY1; bGoOn && iY <= nBlockY2; iY ++)
        {
            for( int iX = nBlockX1; bGoOn && iX <= nBlockX2; iX ++)
            {
                GDALRasterBlock* poBlock = TryGetLockedBlockRef(iX, iY);
                if( poBlock != nullptr )
                {
                    poBlock->DropLock();
                    continue;
                }
                int nBlockId = iX + iY * nBlocksPerRow;
                if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
                    nBlockId += (nBand - 1) * m_poGDS->m_nBlocksPerBand;
                vsi_l_offset nOffset = 0;
                vsi_l_offset nSize = 0;

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
                if( (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG || m_poGDS->nBands == 1) &&
                    !m_poGDS->m_bStreamingIn &&
                    m_poGDS->m_bBlockOrderRowMajor && m_poGDS->m_bLeaderSizeAsUInt4 )
                {
                    OptimizedRetrievalOfOffsetSize(nBlockId, nOffset, nSize, nTotalSize, nMaxRawBlockCacheSize);
                }
                else
#endif
                {
                    CPL_IGNORE_RET_VAL(m_poGDS->IsBlockAvailable(nBlockId, &nOffset, &nSize));
                }
                if( nSize )
                {
                    if( nTotalSize + nSize < nMaxRawBlockCacheSize )
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("GTiff",
                                 "Precaching for block (%d, %d), "
                                 CPL_FRMT_GUIB "-" CPL_FRMT_GUIB,
                                 iX, iY,
                                 nOffset,
                                 nOffset + static_cast<size_t>(nSize) - 1);
#endif
                        aOffsetSize.push_back(
                            std::pair<vsi_l_offset, size_t>
                                (nOffset, static_cast<size_t>(nSize)) );
                        nTotalSize += static_cast<size_t>(nSize);
                    }
                    else
                    {
                        bGoOn = false;
                    }
                }
            }
        }

        std::sort(aOffsetSize.begin(), aOffsetSize.end());

        if( nTotalSize > 0 )
        {
            pBufferedData = VSI_MALLOC_VERBOSE(nTotalSize);
            if( pBufferedData )
            {
                std::vector<vsi_l_offset> anOffsets;
                std::vector<size_t> anSizes;
                std::vector<void*> apData;
                anOffsets.push_back(aOffsetSize[0].first);
                apData.push_back(static_cast<GByte *>(pBufferedData));
                size_t nChunkSize = aOffsetSize[0].second;
                size_t nAccOffset = 0;
                // Try to merge contiguous or slightly overlapping ranges
                for( size_t i = 0; i < aOffsetSize.size()-1; i++ )
                {
                    if ( aOffsetSize[i].first < aOffsetSize[i+1].first &&
                         aOffsetSize[i].first + aOffsetSize[i].second >= aOffsetSize[i+1].first )
                    {
                        const auto overlap = aOffsetSize[i].first + aOffsetSize[i].second - aOffsetSize[i+1].first;
                        // That should always be the case for well behaved
                        // TIFF files.
                        if( aOffsetSize[i+1].second > overlap )
                        {
                            nChunkSize += static_cast<size_t>(
                                aOffsetSize[i+1].second - overlap);
                        }
                    }
                    else
                    {
                        //terminate current block
                        anSizes.push_back(nChunkSize);
#ifdef DEBUG_VERBOSE
                        CPLDebug("GTiff", "Requesting range [" CPL_FRMT_GUIB "-" CPL_FRMT_GUIB "]",
                                 anOffsets.back(), anOffsets.back() + anSizes.back() - 1);
#endif
                        nAccOffset += nChunkSize;
                        //start a new range
                        anOffsets.push_back(aOffsetSize[i+1].first);
                        apData.push_back(static_cast<GByte*>(pBufferedData) + nAccOffset);
                        nChunkSize = aOffsetSize[i+1].second;
                    }
                }
                //terminate last block
                anSizes.push_back(nChunkSize);
#ifdef DEBUG_VERBOSE
                CPLDebug("GTiff", "Requesting range [" CPL_FRMT_GUIB "-" CPL_FRMT_GUIB "]",
                            anOffsets.back(), anOffsets.back() + anSizes.back() - 1);
#endif

                VSILFILE* fp = VSI_TIFFGetVSILFile(th);


                if( VSIFReadMultiRangeL(
                                    static_cast<int>(anSizes.size()),
                                    &apData[0],
                                    &anOffsets[0],
                                    &anSizes[0],
                                    fp ) == 0 )
                {
#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
                    if( !oMapStrileToOffsetByteCount.empty() &&
                        !FillCacheStrileToOffsetByteCount(
                                                anOffsets, anSizes, apData) )
                    {
                        // Retry without optimization
                        CPLFree(pBufferedData);
                        m_poGDS->m_bLeaderSizeAsUInt4 = false;
                        void* pRet = CacheMultiRange(
                            nXOff, nYOff, nXSize, nYSize,
                            nBufXSize, nBufYSize, psExtraArg );
                        m_poGDS->m_bLeaderSizeAsUInt4 = true;
                        return pRet;
                    }

#endif
                    VSI_TIFFSetCachedRanges( th,
                                             static_cast<int>(anSizes.size()),
                                             &apData[0],
                                             &anOffsets[0],
                                             &anSizes[0] );
                }
            }
        }
    }
    return pBufferedData;
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GTiffRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                   int nXOff, int nYOff, int nXSize, int nYSize,
                                   void * pData, int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType,
                                   GSpacing nPixelSpace, GSpacing nLineSpace,
                                   GDALRasterIOExtraArg* psExtraArg )
{
#if DEBUG_VERBOSE
    CPLDebug( "GTiff", "RasterIO(%d, %d, %d, %d, %d, %d)",
              nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );
#endif

    // Try to pass the request to the most appropriate overview dataset.
    if( nBufXSize < nXSize && nBufYSize < nYSize )
    {
        int bTried = FALSE;
        ++m_poGDS->m_nJPEGOverviewVisibilityCounter;
        const CPLErr eErr =
            TryOverviewRasterIO( eRWFlag,
                                 nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize,
                                 eBufType,
                                 nPixelSpace, nLineSpace,
                                 psExtraArg,
                                 &bTried );
        --m_poGDS->m_nJPEGOverviewVisibilityCounter;
        if( bTried )
            return eErr;
    }

    if( m_poGDS->m_eVirtualMemIOUsage != GTiffDataset::VirtualMemIOEnum::NO )
    {
        const int nErr = m_poGDS->VirtualMemIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            1, &nBand, nPixelSpace, nLineSpace, 0, psExtraArg);
        if( nErr >= 0 )
            return static_cast<CPLErr>(nErr);
    }
    if( m_poGDS->m_bDirectIO )
    {
        int nErr = DirectIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                            pData, nBufXSize, nBufYSize, eBufType,
                            nPixelSpace, nLineSpace, psExtraArg);
        if( nErr >= 0 )
            return static_cast<CPLErr>(nErr);
    }

    void* pBufferedData = nullptr;
    if( m_poGDS->eAccess == GA_ReadOnly &&
        eRWFlag == GF_Read &&
        m_poGDS->HasOptimizedReadMultiRange() )
    {
        GTiffRasterBand* poBandForCache = this;

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
        if( !m_poGDS->m_bStreamingIn &&
            m_poGDS->m_bBlockOrderRowMajor &&
            m_poGDS->m_bLeaderSizeAsUInt4 &&
            m_poGDS->m_bMaskInterleavedWithImagery &&
            m_poGDS->m_poImageryDS )
        {
            poBandForCache = cpl::down_cast<GTiffRasterBand*>(
                m_poGDS->m_poImageryDS->GetRasterBand(1));
        }
#endif
        pBufferedData = poBandForCache->CacheMultiRange(
                                        nXOff, nYOff, nXSize, nYSize,
                                        nBufXSize, nBufYSize,
                                        psExtraArg);
    }

    if( m_poGDS->nBands != 1 &&
        m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        eRWFlag == GF_Read &&
        nXSize == nBufXSize && nYSize == nBufYSize )
    {
        const int nBlockX1 = nXOff / nBlockXSize;
        const int nBlockY1 = nYOff / nBlockYSize;
        const int nBlockX2 = (nXOff + nXSize - 1) / nBlockXSize;
        const int nBlockY2 = (nYOff + nYSize - 1) / nBlockYSize;
        const int nXBlocks = nBlockX2 - nBlockX1 + 1;
        const int nYBlocks = nBlockY2 - nBlockY1 + 1;
        const GIntBig nRequiredMem =
            static_cast<GIntBig>(m_poGDS->nBands) * nXBlocks * nYBlocks *
            nBlockXSize * nBlockYSize *
            GDALGetDataTypeSizeBytes(eDataType);
        if( nRequiredMem > GDALGetCacheMax64() )
        {
            if( !m_poGDS->m_bHasWarnedDisableAggressiveBandCaching )
            {
                CPLDebug( "GTiff",
                          "Disable aggressive band caching. "
                          "Cache not big enough. "
                          "At least " CPL_FRMT_GIB " bytes necessary",
                          nRequiredMem );
                m_poGDS->m_bHasWarnedDisableAggressiveBandCaching = true;
            }
            m_poGDS->m_bLoadingOtherBands = true;
        }
    }

    ++m_poGDS->m_nJPEGOverviewVisibilityCounter;
    const CPLErr eErr =
        GDALPamRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize, eBufType,
                                      nPixelSpace, nLineSpace, psExtraArg );
    --m_poGDS->m_nJPEGOverviewVisibilityCounter;

    m_poGDS->m_bLoadingOtherBands = false;

    if( pBufferedData )
    {
        VSIFree( pBufferedData );
        VSI_TIFFSetCachedRanges( TIFFClientdata( m_poGDS->m_hTIFF ),
                                 0, nullptr, nullptr, nullptr );
    }

    return eErr;
}

/************************************************************************/
/*                       IGetDataCoverageStatus()                       */
/************************************************************************/

int GTiffRasterBand::IGetDataCoverageStatus( int nXOff, int nYOff,
                                             int nXSize, int nYSize,
                                             int nMaskFlagStop,
                                             double* pdfDataPct)
{
    if( eAccess == GA_Update )
        m_poGDS->FlushCache();

    const int iXBlockStart = nXOff / nBlockXSize;
    const int iXBlockEnd = (nXOff + nXSize - 1) / nBlockXSize;
    const int iYBlockStart = nYOff / nBlockYSize;
    const int iYBlockEnd = (nYOff + nYSize - 1) / nBlockYSize;
    int nStatus = 0;
    VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( m_poGDS->m_hTIFF ));
    GIntBig nPixelsData = 0;
    // We need to compute this here as it might not have been computed
    // previously (which sucks...)
    nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    for( int iY = iYBlockStart; iY <= iYBlockEnd; ++iY )
    {
        for( int iX = iXBlockStart; iX <= iXBlockEnd; ++iX )
        {
            const int nBlockIdBand0 =
                iX + iY * nBlocksPerRow;
            int nBlockId = nBlockIdBand0;
            if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
                nBlockId = nBlockIdBand0 + (nBand - 1) * m_poGDS->m_nBlocksPerBand;
            vsi_l_offset nOffset = 0;
            vsi_l_offset nLength = 0;
            bool bHasData = false;
            if( !m_poGDS->IsBlockAvailable(nBlockId,&nOffset,&nLength) )
            {
                nStatus |= GDAL_DATA_COVERAGE_STATUS_EMPTY;
            }
            else
            {
                if( m_poGDS->m_nCompression == COMPRESSION_NONE &&
                    m_poGDS->eAccess == GA_ReadOnly &&
                    (!m_bNoDataSet || m_dfNoDataValue == 0.0) )
                {
                    VSIRangeStatus eStatus =
                          VSIFGetRangeStatusL( fp, nOffset, nLength );
                    if( eStatus == VSI_RANGE_STATUS_HOLE )
                    {
                        nStatus |= GDAL_DATA_COVERAGE_STATUS_EMPTY;
                    }
                    else
                    {
                        bHasData = true;
                    }
                }
                else
                {
                    bHasData = true;
                }
            }
            if( bHasData )
            {
                const int nXBlockRight =
                    ( iX * nBlockXSize > INT_MAX - nBlockXSize ) ? INT_MAX :
                    (iX + 1) * nBlockXSize;
                const int nYBlockBottom =
                    ( iY * nBlockYSize > INT_MAX - nBlockYSize ) ? INT_MAX :
                    (iY + 1) * nBlockYSize;

                nPixelsData +=
                    (std::min( nXBlockRight, nXOff + nXSize ) -
                     std::max( iX * nBlockXSize, nXOff )) *
                    (std::min( nYBlockBottom, nYOff + nYSize ) -
                     std::max( iY * nBlockYSize, nYOff ));
                nStatus |= GDAL_DATA_COVERAGE_STATUS_DATA;
            }
            if( nMaskFlagStop != 0 && (nMaskFlagStop & nStatus) != 0 )
            {
                if( pdfDataPct )
                    *pdfDataPct = -1.0;
                return nStatus;
            }
        }
    }
    if( pdfDataPct )
        *pdfDataPct =
          100.0 * nPixelsData /
          (static_cast<GIntBig>(nXSize) * nYSize);
    return nStatus;
}

/************************************************************************/
/*                             ReadStrile()                             */
/************************************************************************/

bool GTiffDataset::ReadStrile(int nBlockId,
                              void* pOutputBuffer,
                              GPtrDiff_t nBlockReqSize)
{
#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
    std::pair<vsi_l_offset, vsi_l_offset> oPair;
    if( m_oCacheStrileToOffsetByteCount.tryGet(nBlockId, oPair) )
    {
        // For the mask, use the parent TIFF handle to get cached ranges
        auto th = TIFFClientdata(
            m_poImageryDS && m_bMaskInterleavedWithImagery ?
                m_poImageryDS->m_hTIFF : m_hTIFF);
        void* pInputBuffer = VSI_TIFFGetCachedRange( th, oPair.first,
                                                static_cast<size_t>(oPair.second) );
        if( pInputBuffer &&
            TIFFReadFromUserBuffer( m_hTIFF, nBlockId,
                                    pInputBuffer, static_cast<size_t>(oPair.second),
                                    pOutputBuffer, nBlockReqSize ) )
        {
            return true;
        }
    }
#endif

    // For debugging
    if( m_poBaseDS )
        m_poBaseDS->m_bHasUsedReadEncodedAPI = true;
    else
        m_bHasUsedReadEncodedAPI = true;

    if( TIFFIsTiled( m_hTIFF ) )
    {
        if( TIFFReadEncodedTile( m_hTIFF, nBlockId, pOutputBuffer,
                                    nBlockReqSize ) == -1
            && !m_bIgnoreReadErrors )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "TIFFReadEncodedTile() failed." );

            return false;
        }
    }
    else
    {
        if( TIFFReadEncodedStrip( m_hTIFF, nBlockId, pOutputBuffer,
                                    nBlockReqSize ) == -1
            && !m_bIgnoreReadErrors )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "TIFFReadEncodedStrip() failed." );

            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    m_poGDS->Crystalize();

    GPtrDiff_t nBlockBufSize = 0;
    if( TIFFIsTiled(m_poGDS->m_hTIFF) )
    {
        nBlockBufSize = static_cast<GPtrDiff_t>(TIFFTileSize( m_poGDS->m_hTIFF ));
    }
    else
    {
        CPLAssert( nBlockXOff == 0 );
        nBlockBufSize = static_cast<GPtrDiff_t>(TIFFStripSize( m_poGDS->m_hTIFF ));
    }

    CPLAssert(nBlocksPerRow != 0);
    const int nBlockIdBand0 =
        nBlockXOff + nBlockYOff * nBlocksPerRow;
    int nBlockId = nBlockIdBand0;
    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId = nBlockIdBand0 + (nBand - 1) * m_poGDS->m_nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*      The bottom most partial tiles and strips are sometimes only     */
/*      partially encoded.  This code reduces the requested data so     */
/*      an error won't be reported in this case. (#1179)                */
/* -------------------------------------------------------------------- */
    auto nBlockReqSize = nBlockBufSize;

    if( nBlockYOff * nBlockYSize > nRasterYSize - nBlockYSize )
    {
        nBlockReqSize = (nBlockBufSize / nBlockYSize)
            * (nBlockYSize - static_cast<int>(
                (static_cast<GIntBig>(nBlockYOff + 1) * nBlockYSize)
                    % nRasterYSize));
    }

/* -------------------------------------------------------------------- */
/*      Handle the case of a strip or tile that doesn't exist yet.      */
/*      Just set to zeros and return.                                   */
/* -------------------------------------------------------------------- */
    vsi_l_offset nOffset = 0;
    bool bErrOccurred = false;
    if( nBlockId != m_poGDS->m_nLoadedBlock &&
        !m_poGDS->IsBlockAvailable(nBlockId, &nOffset, nullptr, &bErrOccurred) )
    {
        NullBlock( pImage );
        if( bErrOccurred )
            return CE_Failure;
        return CE_None;
    }

    if( m_poGDS->m_bStreamingIn &&
        !(m_poGDS->nBands > 1 &&
          m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
          nBlockId == m_poGDS->m_nLoadedBlock) )
    {
        if( nOffset < VSIFTellL(m_poGDS->m_fpL) )
        {
            ReportError( CE_Failure, CPLE_NotSupported,
                      "Trying to load block %d at offset " CPL_FRMT_GUIB
                      " whereas current pos is " CPL_FRMT_GUIB
                      " (backward read not supported)",
                      nBlockId, static_cast<GUIntBig>(nOffset),
                      static_cast<GUIntBig>(VSIFTellL(m_poGDS->m_fpL)) );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle simple case (separate, onesampleperpixel)                */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    if( m_poGDS->nBands == 1
        || m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        if( nBlockReqSize < nBlockBufSize )
            memset( pImage, 0, nBlockBufSize );

        if( !m_poGDS->ReadStrile(nBlockId, pImage, nBlockReqSize) )
        {
            memset( pImage, 0, nBlockBufSize );
            return CE_Failure;
        }
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Load desired block                                              */
/* -------------------------------------------------------------------- */
        eErr = m_poGDS->LoadBlockBuf( nBlockId );
        if( eErr != CE_None )
        {
            memset( pImage, 0,
                    static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize
                    * GDALGetDataTypeSizeBytes(eDataType) );
            return eErr;
        }

        const int nWordBytes = m_poGDS->m_nBitsPerSample / 8;
        GByte* pabyImage = m_poGDS->m_pabyBlockBuf + (nBand - 1) * nWordBytes;

        GDALCopyWords64(pabyImage, eDataType, m_poGDS->nBands * nWordBytes,
                    pImage, eDataType, nWordBytes,
                    static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize);

        eErr = FillCacheForOtherBands(nBlockXOff, nBlockYOff);
    }

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
    CacheMaskForBlock(nBlockXOff,nBlockYOff);
#endif
    return eErr;
}

/************************************************************************/
/*                           CacheMaskForBlock()                       */
/************************************************************************/

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
void GTiffRasterBand::CacheMaskForBlock( int nBlockXOff, int nBlockYOff )

{
    // Preload mask data if layout compatible and we have cached ranges
    if( m_poGDS->m_bMaskInterleavedWithImagery &&
        m_poGDS->GetRasterBand(1)->GetMaskBand() &&
        m_poGDS->m_poMaskDS &&
        VSI_TIFFHasCachedRanges( TIFFClientdata(m_poGDS->m_hTIFF) ) &&
        m_poGDS->m_poMaskDS->m_oCacheStrileToOffsetByteCount.contains(
            nBlockXOff + nBlockYOff * nBlocksPerRow) )
    {
        GDALRasterBlock *poBlock = m_poGDS->m_poMaskDS->GetRasterBand(1)->
            GetLockedBlockRef(nBlockXOff,nBlockYOff);
        if( poBlock )
            poBlock->DropLock();
    }
}
#endif

/************************************************************************/
/*                       FillCacheForOtherBands()                       */
/************************************************************************/

CPLErr GTiffRasterBand::FillCacheForOtherBands( int nBlockXOff, int nBlockYOff )

{
/* -------------------------------------------------------------------- */
/*      In the fairly common case of pixel interleaved 8bit data        */
/*      that is multi-band, lets push the rest of the data into the     */
/*      block cache too, to avoid (hopefully) having to redecode it.    */
/*                                                                      */
/*      Our following logic actually depends on the fact that the       */
/*      this block is already loaded, so subsequent calls will end      */
/*      up back in this method and pull from the loaded block.          */
/*                                                                      */
/*      Be careful not entering this portion of code from               */
/*      the other bands, otherwise we'll get very deep nested calls     */
/*      and O(nBands^2) performance !                                   */
/*                                                                      */
/*      If there are many bands and the block cache size is not big     */
/*      enough to accommodate the size of all the blocks, don't enter   */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    if( m_poGDS->nBands != 1 &&
        m_poGDS->nBands < 128 && // avoid caching for datasets with too many bands
        !m_poGDS->m_bLoadingOtherBands &&
        static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize * GDALGetDataTypeSizeBytes(eDataType) <
        GDALGetCacheMax64() / m_poGDS->nBands )
    {
        m_poGDS->m_bLoadingOtherBands = true;

        for( int iOtherBand = 1; iOtherBand <= m_poGDS->nBands; ++iOtherBand )
        {
            if( iOtherBand == nBand )
                continue;

            GDALRasterBlock *poBlock = m_poGDS->GetRasterBand(iOtherBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff);
            if( poBlock == nullptr )
            {
                eErr = CE_Failure;
                break;
            }
            poBlock->DropLock();
        }

        m_poGDS->m_bLoadingOtherBands = false;
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    m_poGDS->Crystalize();

    if( m_poGDS->m_bDebugDontWriteBlocks )
        return CE_None;

    if( m_poGDS->m_bWriteError )
    {
        // Report as an error if a previously loaded block couldn't be written
        // correctly.
        return CE_Failure;
    }

    CPLAssert(nBlocksPerRow != 0);

/* -------------------------------------------------------------------- */
/*      Handle case of "separate" images                                */
/* -------------------------------------------------------------------- */
    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE
        || m_poGDS->nBands == 1 )
    {
        const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
            + (nBand - 1) * m_poGDS->m_nBlocksPerBand;

        const CPLErr eErr =
            m_poGDS->WriteEncodedTileOrStrip(nBlockId, pImage, true);

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Handle case of pixel interleaved (PLANARCONFIG_CONTIG) images.  */
/* -------------------------------------------------------------------- */
    const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
     // Why 10 ? Somewhat arbitrary
    constexpr int MAX_BANDS_FOR_DIRTY_CHECK = 10;
    GDALRasterBlock* apoBlocks[MAX_BANDS_FOR_DIRTY_CHECK] = {};
    const int nBands = m_poGDS->nBands;
    bool bAllBlocksDirty = false;

/* -------------------------------------------------------------------- */
/*     If all blocks are cached and dirty then we do not need to reload */
/*     the tile/strip from disk                                         */
/* -------------------------------------------------------------------- */
    if( nBands <= MAX_BANDS_FOR_DIRTY_CHECK )
    {
        bAllBlocksDirty = true;
        for( int iBand = 0; iBand < nBands; ++iBand )
        {
            if( iBand + 1 != nBand )
            {
                apoBlocks[iBand] =
                    cpl::down_cast<GTiffRasterBand *>(
                        m_poGDS->GetRasterBand( iBand + 1 ))
                            ->TryGetLockedBlockRef( nBlockXOff, nBlockYOff );

                if( apoBlocks[iBand] == nullptr )
                {
                    bAllBlocksDirty = false;
                }
                else if( !apoBlocks[iBand]->GetDirty() )
                {
                    apoBlocks[iBand]->DropLock();
                    apoBlocks[iBand] = nullptr;
                    bAllBlocksDirty = false;
                }
            }
            else
                apoBlocks[iBand] = nullptr;
        }
#if DEBUG_VERBOSE
        if( bAllBlocksDirty )
            CPLDebug("GTIFF", "Saved reloading block %d", nBlockId);
        else
            CPLDebug("GTIFF", "Must reload block %d", nBlockId);
#endif
    }

    {
        const CPLErr eErr = m_poGDS->LoadBlockBuf( nBlockId, !bAllBlocksDirty );
        if( eErr != CE_None )
        {
            if( nBands <= MAX_BANDS_FOR_DIRTY_CHECK )
            {
                for( int iBand = 0; iBand < nBands; ++iBand )
                {
                    if( apoBlocks[iBand] != nullptr )
                        apoBlocks[iBand]->DropLock();
                }
            }
            return eErr;
        }
    }

/* -------------------------------------------------------------------- */
/*      On write of pixel interleaved data, we might as well flush      */
/*      out any other bands that are dirty in our cache.  This is       */
/*      especially helpful when writing compressed blocks.              */
/* -------------------------------------------------------------------- */
    const int nWordBytes = m_poGDS->m_nBitsPerSample / 8;

    for( int iBand = 0; iBand < nBands; ++iBand )
    {
        const GByte *pabyThisImage = nullptr;
        GDALRasterBlock *poBlock = nullptr;

        if( iBand + 1 == nBand )
        {
            pabyThisImage = static_cast<GByte *>( pImage );
        }
        else
        {
            if( nBands <= MAX_BANDS_FOR_DIRTY_CHECK )
                poBlock = apoBlocks[iBand];
            else
                poBlock = cpl::down_cast<GTiffRasterBand *>(
                    m_poGDS->GetRasterBand( iBand + 1 ))
                        ->TryGetLockedBlockRef( nBlockXOff, nBlockYOff );

            if( poBlock == nullptr )
                continue;

            if( !poBlock->GetDirty() )
            {
                poBlock->DropLock();
                continue;
            }

            pabyThisImage = static_cast<GByte *>( poBlock->GetDataRef() );
        }

        GByte *pabyOut = m_poGDS->m_pabyBlockBuf + iBand*nWordBytes;

        GDALCopyWords64(pabyThisImage, eDataType, nWordBytes,
                      pabyOut, eDataType, nWordBytes * nBands,
                      static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize);

        if( poBlock != nullptr )
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    if( bAllBlocksDirty )
    {
        // We can synchronously write the block now.
        const CPLErr eErr =
            m_poGDS->WriteEncodedTileOrStrip(nBlockId, m_poGDS->m_pabyBlockBuf, true);
        m_poGDS->m_bLoadedBlockDirty = false;
        return eErr;
    }

    m_poGDS->m_bLoadedBlockDirty = true;

    return CE_None;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void GTiffRasterBand::SetDescription( const char *pszDescription )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( pszDescription == nullptr )
        pszDescription = "";

    if( m_osDescription != pszDescription )
        m_poGDS->m_bMetadataChanged = true;

    m_osDescription = pszDescription;
}

/************************************************************************/
/*                           GetDescription()                           */
/************************************************************************/

const char *GTiffRasterBand::GetDescription() const
{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    return m_osDescription;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GTiffRasterBand::GetOffset( int *pbSuccess )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( pbSuccess )
        *pbSuccess = m_bHaveOffsetScale;
    return m_dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetOffset( double dfNewValue )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( !m_bHaveOffsetScale || dfNewValue != m_dfOffset )
        m_poGDS->m_bMetadataChanged = true;

    m_bHaveOffsetScale = true;
    m_dfOffset = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GTiffRasterBand::GetScale( int *pbSuccess )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( pbSuccess )
        *pbSuccess = m_bHaveOffsetScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetScale( double dfNewValue )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( !m_bHaveOffsetScale || dfNewValue != m_dfScale )
        m_poGDS->m_bMetadataChanged = true;

    m_bHaveOffsetScale = true;
    m_dfScale = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char* GTiffRasterBand::GetUnitType()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    if( m_osUnitType.empty() )
    {
        m_poGDS->LookForProjection();
        if( m_poGDS->m_pszVertUnit )
            return m_poGDS->m_pszVertUnit;
    }

    return m_osUnitType.c_str();
}

/************************************************************************/
/*                           SetUnitType()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetUnitType( const char* pszNewValue )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    CPLString osNewValue(pszNewValue ? pszNewValue : "");
    if( osNewValue.compare(m_osUnitType) != 0 )
        m_poGDS->m_bMetadataChanged = true;

    m_osUnitType = osNewValue;
    return CE_None;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GTiffRasterBand::GetMetadataDomainList()
{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    return CSLDuplicate(m_oGTiffMDMD.GetDomainList());
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GTiffRasterBand::GetMetadata( const char * pszDomain )

{
    if( pszDomain == nullptr || !EQUAL(pszDomain, "IMAGE_STRUCTURE") )
    {
        m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    }

    return m_oGTiffMDMD.GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GTiffRasterBand::SetMetadata( char ** papszMD, const char *pszDomain )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized )
    {
        ReportError( CE_Failure, CPLE_NotSupported,
                  "Cannot modify metadata at that point in a streamed "
                  "output file" );
        return CE_Failure;
    }

    if( pszDomain == nullptr || !EQUAL(pszDomain,"_temporary_") )
    {
        if( papszMD != nullptr || GetMetadata(pszDomain) != nullptr )
        {
            m_poGDS->m_bMetadataChanged = true;
            // Cancel any existing metadata from PAM file.
            if( eAccess == GA_Update &&
                GDALPamRasterBand::GetMetadata(pszDomain) != nullptr )
                GDALPamRasterBand::SetMetadata(nullptr, pszDomain);
        }
    }

    return m_oGTiffMDMD.SetMetadata( papszMD, pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GTiffRasterBand::GetMetadataItem( const char * pszName,
                                              const char * pszDomain )

{
    if( pszDomain == nullptr || !EQUAL(pszDomain, "IMAGE_STRUCTURE") )
    {
        m_poGDS->LoadGeoreferencingAndPamIfNeeded();
    }

    if( pszName != nullptr && pszDomain != nullptr && EQUAL(pszDomain, "TIFF") )
    {
        int nBlockXOff = 0;
        int nBlockYOff = 0;

        if( EQUAL(pszName, "JPEGTABLES") )
        {
            uint32_t nJPEGTableSize = 0;
            void* pJPEGTable = nullptr;
            if( TIFFGetField( m_poGDS->m_hTIFF, TIFFTAG_JPEGTABLES,
                              &nJPEGTableSize, &pJPEGTable ) != 1 ||
                pJPEGTable == nullptr || nJPEGTableSize > INT_MAX )
            {
                return nullptr;
            }
            char* const pszHex =
                CPLBinaryToHex( nJPEGTableSize, static_cast<const GByte*>(pJPEGTable) );
            const char* pszReturn = CPLSPrintf("%s", pszHex);
            CPLFree(pszHex);

            return pszReturn;
        }

        if( EQUAL(pszName, "IFD_OFFSET") )
        {
            return CPLSPrintf( CPL_FRMT_GUIB,
                               static_cast<GUIntBig>(m_poGDS->m_nDirOffset) );
        }

        if( sscanf( pszName, "BLOCK_OFFSET_%d_%d",
                         &nBlockXOff, &nBlockYOff ) == 2 )
        {
            nBlocksPerRow =
                DIV_ROUND_UP(m_poGDS->nRasterXSize, m_poGDS->m_nBlockXSize);
            nBlocksPerColumn =
                DIV_ROUND_UP(m_poGDS->nRasterYSize, m_poGDS->m_nBlockYSize);
            if( nBlockXOff < 0 || nBlockXOff >= nBlocksPerRow ||
                nBlockYOff < 0 || nBlockYOff >= nBlocksPerColumn )
                return nullptr;

            int nBlockId = nBlockYOff * nBlocksPerRow + nBlockXOff;
            if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
            {
                nBlockId += (nBand - 1) * m_poGDS->m_nBlocksPerBand;
            }

            vsi_l_offset nOffset = 0;
            if( !m_poGDS->IsBlockAvailable(nBlockId, &nOffset) )
            {
                return nullptr;
            }

            return CPLSPrintf( CPL_FRMT_GUIB, static_cast<GUIntBig>(nOffset) );
        }

        if( sscanf( pszName, "BLOCK_SIZE_%d_%d",
                    &nBlockXOff, &nBlockYOff ) == 2 )
        {
            nBlocksPerRow =
                DIV_ROUND_UP(m_poGDS->nRasterXSize, m_poGDS->m_nBlockXSize);
            nBlocksPerColumn =
                DIV_ROUND_UP(m_poGDS->nRasterYSize, m_poGDS->m_nBlockYSize);
            if( nBlockXOff < 0 || nBlockXOff >= nBlocksPerRow ||
                nBlockYOff < 0 || nBlockYOff >= nBlocksPerColumn )
                return nullptr;

            int nBlockId = nBlockYOff * nBlocksPerRow + nBlockXOff;
            if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
            {
                nBlockId += (nBand - 1) * m_poGDS->m_nBlocksPerBand;
            }

            vsi_l_offset nByteCount = 0;
            if( !m_poGDS->IsBlockAvailable(nBlockId, nullptr, &nByteCount) )
            {
                return nullptr;
            }

            return CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nByteCount));
        }
    }
    return m_oGTiffMDMD.GetMetadataItem( pszName, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GTiffRasterBand::SetMetadataItem( const char *pszName,
                                         const char *pszValue,
                                         const char *pszDomain )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized )
    {
        ReportError( CE_Failure, CPLE_NotSupported,
                  "Cannot modify metadata at that point in a streamed "
                  "output file" );
        return CE_Failure;
    }

    if( pszDomain == nullptr || !EQUAL(pszDomain,"_temporary_") )
    {
        m_poGDS->m_bMetadataChanged = true;
        // Cancel any existing metadata from PAM file.
        if( eAccess == GA_Update &&
            GDALPamRasterBand::GetMetadataItem(pszName, pszDomain) != nullptr )
            GDALPamRasterBand::SetMetadataItem(pszName, nullptr, pszDomain);
    }

    return m_oGTiffMDMD.SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRasterBand::GetColorInterpretation()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    return m_eBandInterp;
}

/************************************************************************/
/*                         GTiffGetAlphaValue()                         */
/************************************************************************/

uint16_t GTiffGetAlphaValue(const char* pszValue, uint16_t nDefault)
{
    if( pszValue == nullptr )
        return nDefault;
    if( EQUAL(pszValue, "YES") )
        return DEFAULT_ALPHA_TYPE;
    if( EQUAL(pszValue, "PREMULTIPLIED") )
        return EXTRASAMPLE_ASSOCALPHA;
    if( EQUAL(pszValue, "NON-PREMULTIPLIED") )
        return EXTRASAMPLE_UNASSALPHA;
    if( EQUAL(pszValue, "NO") ||
        EQUAL(pszValue, "UNSPECIFIED") )
        return EXTRASAMPLE_UNSPECIFIED;

    return nDefault;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr GTiffRasterBand::SetColorInterpretation( GDALColorInterp eInterp )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( eInterp == m_eBandInterp )
        return CE_None;

    m_eBandInterp = eInterp;

    if( eAccess != GA_Update )
    {
        CPLDebug( "GTIFF", "ColorInterpretation %s for band %d goes to PAM "
                  "instead of TIFF tag",
                  GDALGetColorInterpretationName(eInterp), nBand );
        return GDALPamRasterBand::SetColorInterpretation( eInterp );
    }

    m_poGDS->m_bNeedsRewrite = true;
    m_poGDS->m_bMetadataChanged = true;

    // Try to autoset TIFFTAG_PHOTOMETRIC = PHOTOMETRIC_RGB if possible.
    if( m_poGDS->nBands >= 3 &&
        m_poGDS->m_nCompression != COMPRESSION_JPEG &&
        m_poGDS->m_nPhotometric != PHOTOMETRIC_RGB &&
        CSLFetchNameValue( m_poGDS->m_papszCreationOptions,
                           "PHOTOMETRIC" ) == nullptr &&
        ((nBand == 1 && eInterp == GCI_RedBand) ||
         (nBand == 2 && eInterp == GCI_GreenBand) ||
         (nBand == 3 && eInterp == GCI_BlueBand)) )
    {
        if( m_poGDS->GetRasterBand(1)->GetColorInterpretation() == GCI_RedBand &&
            m_poGDS->GetRasterBand(2)->GetColorInterpretation() == GCI_GreenBand &&
            m_poGDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand )
        {
            m_poGDS->m_nPhotometric = PHOTOMETRIC_RGB;
            TIFFSetField( m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC,
                          m_poGDS->m_nPhotometric );

            // We need to update the number of extra samples.
            uint16_t *v = nullptr;
            uint16_t count = 0;
            const uint16_t nNewExtraSamplesCount =
                static_cast<uint16_t>(m_poGDS->nBands - 3);
            if( m_poGDS->nBands >= 4 &&
                TIFFGetField( m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES,
                              &count, &v ) &&
                count > nNewExtraSamplesCount )
            {
                uint16_t * const pasNewExtraSamples =
                    static_cast<uint16_t *>( CPLMalloc(
                        nNewExtraSamplesCount * sizeof(uint16_t) ) );
                memcpy( pasNewExtraSamples, v + count - nNewExtraSamplesCount,
                        nNewExtraSamplesCount * sizeof(uint16_t) );

                TIFFSetField( m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES,
                              nNewExtraSamplesCount, pasNewExtraSamples );

                CPLFree(pasNewExtraSamples);
            }
        }
        return CE_None;
    }

    // On the contrary, cancel the above if needed
    if( m_poGDS->m_nCompression != COMPRESSION_JPEG &&
        m_poGDS->m_nPhotometric == PHOTOMETRIC_RGB &&
        CSLFetchNameValue( m_poGDS->m_papszCreationOptions,
                           "PHOTOMETRIC") == nullptr &&
        ((nBand == 1 && eInterp != GCI_RedBand) ||
         (nBand == 2 && eInterp != GCI_GreenBand) ||
         (nBand == 3 && eInterp != GCI_BlueBand)) )
    {
        m_poGDS->m_nPhotometric = PHOTOMETRIC_MINISBLACK;
        TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC, m_poGDS->m_nPhotometric);

        // We need to update the number of extra samples.
        uint16_t *v = nullptr;
        uint16_t count = 0;
        const uint16_t nNewExtraSamplesCount =
            static_cast<uint16_t>(m_poGDS->nBands - 1);
        if( m_poGDS->nBands >= 2 )
        {
            TIFFGetField( m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v );
            if( nNewExtraSamplesCount > count )
            {
                uint16_t * const pasNewExtraSamples =
                    static_cast<uint16_t *>( CPLMalloc(
                        nNewExtraSamplesCount * sizeof(uint16_t) ) );
                for( int i = 0;
                     i < static_cast<int>(nNewExtraSamplesCount - count);
                     ++i )
                    pasNewExtraSamples[i] = EXTRASAMPLE_UNSPECIFIED;
                if( count > 0 )
                {
                    memcpy( pasNewExtraSamples + nNewExtraSamplesCount - count,
                            v,
                            count * sizeof(uint16_t) );
                }

                TIFFSetField( m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES,
                              nNewExtraSamplesCount, pasNewExtraSamples );

                CPLFree(pasNewExtraSamples);
            }
        }
    }

    // Mark alpha band / undefined in extrasamples.
    if( eInterp == GCI_AlphaBand || eInterp == GCI_Undefined )
    {
        uint16_t *v = nullptr;
        uint16_t count = 0;
        if( TIFFGetField( m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            const int nBaseSamples = m_poGDS->m_nSamplesPerPixel - count;

            if( eInterp == GCI_AlphaBand )
            {
                for( int i = 1; i <= m_poGDS->nBands; ++i )
                {
                    if( i != nBand &&
                        m_poGDS->GetRasterBand(i)->GetColorInterpretation() ==
                        GCI_AlphaBand )
                    {
                        if( i == nBaseSamples + 1 &&
                            CSLFetchNameValue( m_poGDS->m_papszCreationOptions,
                                            "ALPHA" ) != nullptr )
                        {
                            ReportError(
                                CE_Warning, CPLE_AppDefined,
                                "Band %d was already identified as alpha band, "
                                "and band %d is now marked as alpha too. "
                                "Presumably ALPHA creation option is not needed",
                                i, nBand );
                        }
                        else
                        {
                            ReportError(
                                CE_Warning, CPLE_AppDefined,
                                "Band %d was already identified as alpha band, "
                                "and band %d is now marked as alpha too",
                                i, nBand );
                        }
                    }
                }
            }

            if( nBand > nBaseSamples && nBand - nBaseSamples - 1 < count )
            {
                // We need to allocate a new array as (current) libtiff
                // versions will not like that we reuse the array we got from
                // TIFFGetField().

                uint16_t* pasNewExtraSamples =
                    static_cast<uint16_t *>(
                        CPLMalloc( count * sizeof(uint16_t) ) );
                memcpy( pasNewExtraSamples, v, count * sizeof(uint16_t) );
                if( eInterp == GCI_AlphaBand )
                {
                    pasNewExtraSamples[nBand - nBaseSamples - 1] =
                        GTiffGetAlphaValue(CPLGetConfigOption("GTIFF_ALPHA", nullptr),
                                            DEFAULT_ALPHA_TYPE);
                }
                else
                {
                    pasNewExtraSamples[nBand - nBaseSamples - 1] =
                        EXTRASAMPLE_UNSPECIFIED;
                }

                TIFFSetField( m_poGDS->m_hTIFF, TIFFTAG_EXTRASAMPLES,
                              count, pasNewExtraSamples);

                CPLFree(pasNewExtraSamples);

                return CE_None;
            }
        }
    }

    if( m_poGDS->m_nPhotometric != PHOTOMETRIC_MINISBLACK &&
        CSLFetchNameValue( m_poGDS->m_papszCreationOptions, "PHOTOMETRIC") == nullptr )
    {
        m_poGDS->m_nPhotometric = PHOTOMETRIC_MINISBLACK;
        TIFFSetField(m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC, m_poGDS->m_nPhotometric);
    }

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffRasterBand::GetColorTable()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( nBand == 1 )
        return m_poGDS->m_poColorTable;

    return nullptr;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr GTiffRasterBand::SetColorTable( GDALColorTable * poCT )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

/* -------------------------------------------------------------------- */
/*      Check if this is even a candidate for applying a PCT.           */
/* -------------------------------------------------------------------- */
    if( nBand != 1)
    {
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetColorTable() can only be called on band 1." );
        return CE_Failure;
    }

    if( m_poGDS->m_nSamplesPerPixel != 1 && m_poGDS->m_nSamplesPerPixel != 2)
    {
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetColorTable() not supported for multi-sample TIFF "
                  "files." );
        return CE_Failure;
    }

    if( eDataType != GDT_Byte && eDataType != GDT_UInt16 )
    {
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetColorTable() only supported for Byte or UInt16 bands "
                  "in TIFF format." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Is this really a request to clear the color table?              */
/* -------------------------------------------------------------------- */
    if( poCT == nullptr || poCT->GetColorEntryCount() == 0 )
    {
        TIFFSetField( m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC,
                      PHOTOMETRIC_MINISBLACK );

        TIFFUnsetField( m_poGDS->m_hTIFF, TIFFTAG_COLORMAP );

        if( m_poGDS->m_poColorTable )
        {
            delete m_poGDS->m_poColorTable;
            m_poGDS->m_poColorTable = nullptr;
        }

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Write out the colortable, and update the configuration.         */
/* -------------------------------------------------------------------- */
    int nColors = 65536;

    if( eDataType == GDT_Byte )
        nColors = 256;

    unsigned short *panTRed = static_cast<unsigned short *>(
        CPLMalloc(sizeof(unsigned short)*nColors) );
    unsigned short *panTGreen = static_cast<unsigned short *>(
        CPLMalloc(sizeof(unsigned short)*nColors) );
    unsigned short *panTBlue = static_cast<unsigned short *>(
        CPLMalloc(sizeof(unsigned short)*nColors) );

    for( int iColor = 0; iColor < nColors; ++iColor )
    {
        if( iColor < poCT->GetColorEntryCount() )
        {
            GDALColorEntry sRGB;
            poCT->GetColorEntryAsRGB( iColor, &sRGB );

            panTRed[iColor] = static_cast<unsigned short>(257 * sRGB.c1);
            panTGreen[iColor] = static_cast<unsigned short>(257 * sRGB.c2);
            panTBlue[iColor] = static_cast<unsigned short>(257 * sRGB.c3);
        }
        else
        {
            panTRed[iColor] = 0;
            panTGreen[iColor] = 0;
            panTBlue[iColor] = 0;
        }
    }

    TIFFSetField( m_poGDS->m_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
    TIFFSetField( m_poGDS->m_hTIFF, TIFFTAG_COLORMAP,
                  panTRed, panTGreen, panTBlue );

    CPLFree( panTRed );
    CPLFree( panTGreen );
    CPLFree( panTBlue );

    if( m_poGDS->m_poColorTable )
        delete m_poGDS->m_poColorTable;

    // libtiff 3.X needs setting this in all cases (creation or update)
    // whereas libtiff 4.X would just need it if there
    // was no color table before.
    m_poGDS->m_bNeedsRewrite = true;

    m_poGDS->m_poColorTable = poCT->Clone();
    m_eBandInterp = GCI_PaletteIndex;

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GTiffRasterBand::GetNoDataValue( int * pbSuccess )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( m_bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return m_dfNoDataValue;
    }

    if( m_poGDS->m_bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return m_poGDS->m_dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr GTiffRasterBand::SetNoDataValue( double dfNoData )

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( m_poGDS->m_bNoDataSet && m_poGDS->m_dfNoDataValue == dfNoData )
    {
        m_bNoDataSet = true;
        m_dfNoDataValue = dfNoData;
        return CE_None;
    }

    if( m_poGDS->nBands > 1 && m_poGDS->m_eProfile == GTiffProfile::GDALGEOTIFF )
    {
        int bOtherBandHasNoData = FALSE;
        const int nOtherBand = nBand > 1 ? 1 : 2;
        double dfOtherNoData = m_poGDS->GetRasterBand(nOtherBand)->
                                    GetNoDataValue(&bOtherBandHasNoData);
        if( bOtherBandHasNoData && dfOtherNoData != dfNoData )
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                 "Setting nodata to %.18g on band %d, but band %d has nodata "
                 "at %.18g. The TIFFTAG_GDAL_NODATA only support one value "
                 "per dataset. This value of %.18g will be used for all bands "
                 "on re-opening",
                 dfNoData, nBand, nOtherBand, dfOtherNoData, dfNoData);
        }
    }

    if( m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized )
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify nodata at that point in a streamed output file" );
        return CE_Failure;
    }

    m_poGDS->m_bNoDataSet = true;
    m_poGDS->m_dfNoDataValue = dfNoData;

    m_poGDS->m_bNoDataChanged = true;

    m_bNoDataSet = true;
    m_dfNoDataValue = dfNoData;
    return CE_None;
}

/************************************************************************/
/*                        DeleteNoDataValue()                           */
/************************************************************************/

CPLErr GTiffRasterBand::DeleteNoDataValue()

{
    m_poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( !m_poGDS->m_bNoDataSet )
        return CE_None;

    if( m_poGDS->m_bStreamingOut && m_poGDS->m_bCrystalized )
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify nodata at that point in a streamed output file" );
        return CE_Failure;
    }

    m_poGDS->m_bNoDataSet = false;
    m_poGDS->m_dfNoDataValue = -9999.0;

    m_poGDS->m_bNoDataChanged = true;

    m_bNoDataSet = false;
    m_dfNoDataValue = -9999.0;
    return CE_None;
}

/************************************************************************/
/*                             NullBlock()                              */
/*                                                                      */
/*      Set the block data to the null value if it is set, or zero      */
/*      if there is no null data value.                                 */
/************************************************************************/

void GTiffRasterBand::NullBlock( void *pData )

{
    const GPtrDiff_t nWords = static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize;
    const int nChunkSize = std::max(1, GDALGetDataTypeSizeBytes(eDataType));

    int bNoDataSetIn = FALSE;
    const double dfNoData = GetNoDataValue( &bNoDataSetIn );
    if( !bNoDataSetIn )
    {
#ifdef ESRI_BUILD
        if( m_poGDS->m_nBitsPerSample >= 2 )
            memset( pData, 0, nWords * nChunkSize );
        else
            memset( pData, 1, nWords * nChunkSize );
#else
        memset( pData, 0, nWords * nChunkSize );
#endif
    }
    else
    {
        // Will convert nodata value to the right type and copy efficiently.
        GDALCopyWords64( &dfNoData, GDT_Float64, 0,
                       pData, eDataType, nChunkSize, nWords);
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GTiffRasterBand::GetOverviewCount()

{
    m_poGDS->ScanDirectories();

    if( m_poGDS->m_nOverviewCount > 0 )
    {
        return m_poGDS->m_nOverviewCount;
    }

    const int nOverviewCount = GDALRasterBand::GetOverviewCount();
    if( nOverviewCount > 0 )
        return nOverviewCount;

    // Implicit JPEG overviews are normally hidden, except when doing
    // IRasterIO() operations.
    if( m_poGDS->m_nJPEGOverviewVisibilityCounter )
        return m_poGDS->GetJPEGOverviewCount();

    return 0;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetOverview( int i )

{
    m_poGDS->ScanDirectories();

    if( m_poGDS->m_nOverviewCount > 0 )
    {
        // Do we have internal overviews?
        if( i < 0 || i >= m_poGDS->m_nOverviewCount )
            return nullptr;

        return m_poGDS->m_papoOverviewDS[i]->GetRasterBand(nBand);
    }

    GDALRasterBand* const poOvrBand = GDALRasterBand::GetOverview( i );
    if( poOvrBand != nullptr )
        return poOvrBand;

    // For consistency with GetOverviewCount(), we should also test
    // m_nJPEGOverviewVisibilityCounter, but it is also convenient to be able
    // to query them for testing purposes.
    if( i >= 0 && i < m_poGDS->GetJPEGOverviewCount() )
        return m_poGDS->m_papoJPEGOverviewDS[i]->GetRasterBand(nBand);

    return nullptr;
}

/************************************************************************/
/*                           GetMaskFlags()                             */
/************************************************************************/

int GTiffRasterBand::GetMaskFlags()
{
    m_poGDS->ScanDirectories();

    if( m_poGDS->m_poExternalMaskDS != nullptr )
    {
        return GMF_PER_DATASET;
    }

    if( m_poGDS->m_poMaskDS != nullptr )
    {
        if( m_poGDS->m_poMaskDS->GetRasterCount() == 1)
        {
            return GMF_PER_DATASET;
        }

        return 0;
    }

    if( m_poGDS->m_bIsOverview )
    {
        return m_poGDS->m_poBaseDS->GetRasterBand(nBand)->GetMaskFlags();
    }

    return GDALPamRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetMaskBand()
{
    m_poGDS->ScanDirectories();

    if( m_poGDS->m_poExternalMaskDS != nullptr )
    {
        return m_poGDS->m_poExternalMaskDS->GetRasterBand(1);
    }

    if( m_poGDS->m_poMaskDS != nullptr )
    {
        if( m_poGDS->m_poMaskDS->GetRasterCount() == 1 )
            return m_poGDS->m_poMaskDS->GetRasterBand(1);

        return m_poGDS->m_poMaskDS->GetRasterBand(nBand);
    }

    if( m_poGDS->m_bIsOverview )
    {
        GDALRasterBand* poBaseMask =
            m_poGDS->m_poBaseDS->GetRasterBand(nBand)->GetMaskBand();
        if( poBaseMask )
        {
            const int nOverviews = poBaseMask->GetOverviewCount();
            for( int i = 0; i < nOverviews; i++ )
            {
                GDALRasterBand* poOvr = poBaseMask->GetOverview(i);
                if( poOvr &&
                    poOvr->GetXSize() == GetXSize() &&
                    poOvr->GetYSize() == GetYSize() )
                {
                    return poOvr;
                }
            }
        }
    }

    return GDALPamRasterBand::GetMaskBand();
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffSplitBand                           */
/* ==================================================================== */
/************************************************************************/

class GTiffSplitBand final : public GTiffRasterBand
{
    friend class GTiffDataset;

  public:
             GTiffSplitBand( GTiffDataset *, int );
    virtual ~GTiffSplitBand() {}

    virtual int IGetDataCoverageStatus( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nMaskFlagStop,
                                        double* pdfDataPct) override;

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           GTiffSplitBand()                           */
/************************************************************************/

GTiffSplitBand::GTiffSplitBand( GTiffDataset *poDSIn, int nBandIn ) :
    GTiffRasterBand( poDSIn, nBandIn )
{
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                       IGetDataCoverageStatus()                       */
/************************************************************************/

int GTiffSplitBand::IGetDataCoverageStatus( int , int ,
                                             int , int ,
                                             int ,
                                             double* )
{
     return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
            GDAL_DATA_COVERAGE_STATUS_DATA;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBand::IReadBlock( int /* nBlockXOff */, int nBlockYOff,
                                   void * pImage )

{
    m_poGDS->Crystalize();

    // Optimization when reading the same line in a contig multi-band TIFF.
    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG && m_poGDS->nBands > 1 &&
        m_poGDS->m_nLoadedBlock == nBlockYOff )
    {
        goto extract_band_data;
    }

    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        m_poGDS->nBands > 1 )
    {
        if( m_poGDS->m_pabyBlockBuf == nullptr )
        {
            m_poGDS->m_pabyBlockBuf =
                static_cast<GByte *>(
                    VSI_MALLOC_VERBOSE(TIFFScanlineSize(m_poGDS->m_hTIFF)) );
            if( m_poGDS->m_pabyBlockBuf == nullptr )
            {
                return CE_Failure;
            }
        }
    }
    else
    {
        CPLAssert(TIFFScanlineSize(m_poGDS->m_hTIFF) == nBlockXSize);
    }

/* -------------------------------------------------------------------- */
/*      Read through to target scanline.                                */
/* -------------------------------------------------------------------- */
    if( m_poGDS->m_nLoadedBlock >= nBlockYOff )
        m_poGDS->m_nLoadedBlock = -1;

    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE && m_poGDS->nBands > 1 )
    {
        // If we change of band, we must start reading the
        // new strip from its beginning.
        if( m_poGDS->m_nLastBandRead != nBand )
            m_poGDS->m_nLoadedBlock = -1;
        m_poGDS->m_nLastBandRead = nBand;
    }

    while( m_poGDS->m_nLoadedBlock < nBlockYOff )
    {
        ++m_poGDS->m_nLoadedBlock;
        if( TIFFReadScanline(
                m_poGDS->m_hTIFF,
                m_poGDS->m_pabyBlockBuf ? m_poGDS->m_pabyBlockBuf : pImage,
                m_poGDS->m_nLoadedBlock,
                (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE) ?
                 static_cast<uint16_t>(nBand - 1) : 0 ) == -1
            && !m_poGDS->m_bIgnoreReadErrors )
        {
            ReportError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadScanline() failed." );
            m_poGDS->m_nLoadedBlock = -1;
            return CE_Failure;
        }
    }

extract_band_data:
/* -------------------------------------------------------------------- */
/*      Extract band data from contig buffer.                           */
/* -------------------------------------------------------------------- */
    if( m_poGDS->m_pabyBlockBuf != nullptr )
    {
        for( int iPixel = 0, iSrcOffset= nBand - 1, iDstOffset = 0;
             iPixel < nBlockXSize;
             ++iPixel, iSrcOffset += m_poGDS->nBands, ++iDstOffset )
        {
            static_cast<GByte *>(pImage)[iDstOffset] =
                m_poGDS->m_pabyBlockBuf[iSrcOffset];
        }
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBand::IWriteBlock( int /* nBlockXOff */, int /* nBlockYOff */,
                                    void * /* pImage */ )

{
    ReportError( CE_Failure, CPLE_AppDefined,
              "Split bands are read-only." );
    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffRGBABand                            */
/* ==================================================================== */
/************************************************************************/

class GTiffRGBABand final : public GTiffRasterBand
{
    friend class GTiffDataset;

  public:
                   GTiffRGBABand( GTiffDataset *, int );
    virtual ~GTiffRGBABand() {}

    virtual int IGetDataCoverageStatus( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nMaskFlagStop,
                                        double* pdfDataPct) override;

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;

    virtual GDALColorInterp GetColorInterpretation() override;
};

/************************************************************************/
/*                           GTiffRGBABand()                            */
/************************************************************************/

GTiffRGBABand::GTiffRGBABand( GTiffDataset *poDSIn, int nBandIn ) :
    GTiffRasterBand( poDSIn, nBandIn )
{
    eDataType = GDT_Byte;
}

/************************************************************************/
/*                       IGetDataCoverageStatus()                       */
/************************************************************************/

int GTiffRGBABand::IGetDataCoverageStatus( int , int ,
                                             int , int ,
                                             int ,
                                             double* )
{
     return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
            GDAL_DATA_COVERAGE_STATUS_DATA;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffRGBABand::IWriteBlock( int, int, void * )

{
    ReportError( CE_Failure, CPLE_AppDefined,
              "RGBA interpreted raster bands are read-only." );
    return CE_Failure;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRGBABand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    m_poGDS->Crystalize();

    CPLAssert( nBlocksPerRow != 0 );
    const auto nBlockBufSize = 4 * static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize;
    const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        for( int iBand = 0; iBand < m_poGDS->m_nSamplesPerPixel; iBand ++ )
        {
            int nBlockIdBand = nBlockId + iBand * m_poGDS->m_nBlocksPerBand;
            if( !m_poGDS->IsBlockAvailable(nBlockIdBand) )
                return CE_Failure;
        }
    }
    else
    {
        if( !m_poGDS->IsBlockAvailable(nBlockId) )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( m_poGDS->m_pabyBlockBuf == nullptr )
    {
        m_poGDS->m_pabyBlockBuf =
            static_cast<GByte *>(
                VSI_MALLOC3_VERBOSE( 4, nBlockXSize, nBlockYSize ) );
        if( m_poGDS->m_pabyBlockBuf == nullptr )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the strip                                                  */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( m_poGDS->m_nLoadedBlock != nBlockId )
    {
        if( TIFFIsTiled( m_poGDS->m_hTIFF ) )
        {
#if TIFFLIB_VERSION > 20161119
            if( TIFFReadRGBATileExt(
                   m_poGDS->m_hTIFF,
                   nBlockXOff * nBlockXSize,
                   nBlockYOff * nBlockYSize,
                   reinterpret_cast<uint32_t *>(m_poGDS->m_pabyBlockBuf),
                   !m_poGDS->m_bIgnoreReadErrors) == 0
                && !m_poGDS->m_bIgnoreReadErrors )
#else
            if( TIFFReadRGBATile(
                   m_poGDS->m_hTIFF,
                   nBlockXOff * nBlockXSize,
                   nBlockYOff * nBlockYSize,
                   reinterpret_cast<uint32_t *>(m_poGDS->m_pabyBlockBuf)) == 0
                && !m_poGDS->m_bIgnoreReadErrors )
#endif
            {
                // Once TIFFError() is properly hooked, this can go away.
                ReportError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadRGBATile() failed." );

                memset( m_poGDS->m_pabyBlockBuf, 0, nBlockBufSize );

                eErr = CE_Failure;
            }
        }
        else
        {
#if TIFFLIB_VERSION > 20161119
            if( TIFFReadRGBAStripExt(
                   m_poGDS->m_hTIFF,
                   nBlockId * nBlockYSize,
                   reinterpret_cast<uint32_t *>(m_poGDS->m_pabyBlockBuf),
                   !m_poGDS->m_bIgnoreReadErrors) == 0
                && !m_poGDS->m_bIgnoreReadErrors )
#else
            if( TIFFReadRGBAStrip(
                   m_poGDS->m_hTIFF,
                   nBlockId * nBlockYSize,
                   reinterpret_cast<uint32_t *>(m_poGDS->m_pabyBlockBuf)) == 0
                && !m_poGDS->m_bIgnoreReadErrors )
#endif
            {
                // Once TIFFError() is properly hooked, this can go away.
                ReportError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadRGBAStrip() failed." );

                memset( m_poGDS->m_pabyBlockBuf, 0, nBlockBufSize );

                eErr = CE_Failure;
            }
        }
    }

    m_poGDS->m_nLoadedBlock = nBlockId;

/* -------------------------------------------------------------------- */
/*      Handle simple case of eight bit data, and pixel interleaving.   */
/* -------------------------------------------------------------------- */
    int nThisBlockYSize = nBlockYSize;

    if( nBlockYOff * nBlockYSize > GetYSize() - nBlockYSize
        && !TIFFIsTiled( m_poGDS->m_hTIFF ) )
        nThisBlockYSize = GetYSize() - nBlockYOff * nBlockYSize;

#ifdef CPL_LSB
    const int nBO = nBand - 1;
#else
    const int nBO = 4 - nBand;
#endif

    for( int iDestLine = 0; iDestLine < nThisBlockYSize; ++iDestLine )
    {
        const auto nSrcOffset =
            static_cast<GPtrDiff_t>(nThisBlockYSize - iDestLine - 1) * nBlockXSize * 4;

        GDALCopyWords(
            m_poGDS->m_pabyBlockBuf + nBO + nSrcOffset, GDT_Byte, 4,
            static_cast<GByte *>(pImage)+static_cast<GPtrDiff_t>(iDestLine)*nBlockXSize, GDT_Byte, 1,
            nBlockXSize );
    }

    if( eErr == CE_None )
        eErr = FillCacheForOtherBands(nBlockXOff, nBlockYOff);

    return eErr;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRGBABand::GetColorInterpretation()

{
    if( nBand == 1 )
        return GCI_RedBand;
    if( nBand == 2 )
        return GCI_GreenBand;
    if( nBand == 3 )
        return GCI_BlueBand;

    return GCI_AlphaBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffOddBitsBand                         */
/* ==================================================================== */
/************************************************************************/

class GTiffOddBitsBand CPL_NON_FINAL: public GTiffRasterBand
{
    friend class GTiffDataset;
  public:

                   GTiffOddBitsBand( GTiffDataset *, int );
    virtual ~GTiffOddBitsBand() {}

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           GTiffOddBitsBand()                         */
/************************************************************************/

GTiffOddBitsBand::GTiffOddBitsBand( GTiffDataset *m_poGDSIn, int nBandIn )
        : GTiffRasterBand( m_poGDSIn, nBandIn )

{
    eDataType = GDT_Unknown;
    if( (m_poGDS->m_nBitsPerSample == 16 || m_poGDS->m_nBitsPerSample == 24) &&
        m_poGDS->m_nSampleFormat == SAMPLEFORMAT_IEEEFP )
        eDataType = GDT_Float32;
    // FIXME ? in autotest we currently open gcore/data/int24.tif
    // which is declared as signed, but we consider it as unsigned
    else if( (m_poGDS->m_nSampleFormat == SAMPLEFORMAT_UINT ||
              m_poGDS->m_nSampleFormat == SAMPLEFORMAT_INT) &&
             m_poGDS->m_nBitsPerSample < 8 )
        eDataType = GDT_Byte;
    else if( (m_poGDS->m_nSampleFormat == SAMPLEFORMAT_UINT ||
              m_poGDS->m_nSampleFormat == SAMPLEFORMAT_INT) &&
             m_poGDS->m_nBitsPerSample > 8 && m_poGDS->m_nBitsPerSample < 16 )
        eDataType = GDT_UInt16;
    else if( (m_poGDS->m_nSampleFormat == SAMPLEFORMAT_UINT ||
              m_poGDS->m_nSampleFormat == SAMPLEFORMAT_INT) &&
             m_poGDS->m_nBitsPerSample > 16 && m_poGDS->m_nBitsPerSample < 32 )
        eDataType = GDT_UInt32;
}

/************************************************************************/
/*                            FloatToHalf()                             */
/************************************************************************/

static GUInt16 FloatToHalf( GUInt32 iFloat32, bool& bHasWarned )
{
    GUInt32 iSign =     (iFloat32 >> 31) & 0x00000001;
    GUInt32 iExponent = (iFloat32 >> 23) & 0x000000ff;
    GUInt32 iMantissa = iFloat32         & 0x007fffff;

    if (iExponent == 255)
    {
        if (iMantissa == 0)
        {
/* -------------------------------------------------------------------- */
/*       Positive or negative infinity.                                 */
/* -------------------------------------------------------------------- */

            return static_cast<GUInt16>((iSign << 15) | 0x7C00);
        }
        else
        {
/* -------------------------------------------------------------------- */
/*       NaN -- preserve sign and significand bits.                     */
/* -------------------------------------------------------------------- */
            if( iMantissa >> 13 )
                return static_cast<GUInt16>((iSign << 15) | 0x7C00 |
                                                            (iMantissa >> 13));

            return static_cast<GUInt16>((iSign << 15) | 0x7E00);
        }
    }

    if( iExponent <= 127 - 15 )
    {
        // Zero, float32 denormalized number or float32 too small normalized
        // number
        if( 13 + 1 + 127 - 15 - iExponent >= 32 )
            return static_cast<GUInt16>(iSign << 15);

        // Return a denormalized number
        return static_cast<GUInt16>((iSign << 15) |
                ((iMantissa | 0x00800000) >> (13 + 1 + 127 - 15 - iExponent)));
    }
    if( iExponent - (127 - 15) >= 31 )
    {
        if( !bHasWarned )
        {
            bHasWarned = true;
            float fVal = 0.0f;
            memcpy(&fVal, &iFloat32, 4);
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Value %.8g is beyond range of float16. Converted to %sinf",
                fVal, (fVal > 0) ? "+" : "-");
        }
        return static_cast<GUInt16>((iSign << 15) | 0x7C00);  // Infinity
    }

/* -------------------------------------------------------------------- */
/*       Normalized number.                                             */
/* -------------------------------------------------------------------- */

    iExponent = iExponent - (127 - 15);
    iMantissa = iMantissa >> 13;

/* -------------------------------------------------------------------- */
/*       Assemble sign, exponent and mantissa.                          */
/* -------------------------------------------------------------------- */

    // coverity[overflow_sink]
    return static_cast<GUInt16>((iSign << 15) | (iExponent << 10) | iMantissa);
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffOddBitsBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                      void *pImage )

{
    m_poGDS->Crystalize();

    if( m_poGDS->m_bWriteError )
    {
        // Report as an error if a previously loaded block couldn't be written
        // correctly.
        return CE_Failure;
    }

    if( eDataType == GDT_Float32 && m_poGDS->m_nBitsPerSample != 16 )
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                 "Writing float data with nBitsPerSample = %d is unsupported",
                 m_poGDS->m_nBitsPerSample);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    CPLAssert(nBlocksPerRow != 0);
    int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId += (nBand - 1) * m_poGDS->m_nBlocksPerBand;

    // Only read content from disk in the CONTIG case.
    {
        const CPLErr eErr =
            m_poGDS->LoadBlockBuf( nBlockId,
                                 m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
                                 m_poGDS->nBands > 1 );
        if( eErr != CE_None )
            return eErr;
    }

    const GUInt32 nMaxVal = (1U << m_poGDS->m_nBitsPerSample) - 1;

/* -------------------------------------------------------------------- */
/*      Handle case of "separate" images or single band images where    */
/*      no interleaving with other data is required.                    */
/* -------------------------------------------------------------------- */
    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE
        || m_poGDS->nBands == 1 )
    {
        // TODO(schwehr): Create a CplNumBits8Aligned.
        // Bits per line rounds up to next byte boundary.
        GInt64 nBitsPerLine = static_cast<GInt64>(nBlockXSize) * m_poGDS->m_nBitsPerSample;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        GPtrDiff_t iPixel = 0;

        // Small optimization in 1 bit case.
        if( m_poGDS->m_nBitsPerSample == 1 )
        {
            for( int iY = 0; iY < nBlockYSize; ++iY, iPixel += nBlockXSize )
            {
                GInt64 iBitOffset = iY * nBitsPerLine;

                const GByte* pabySrc =
                    static_cast<const GByte*>(pImage) + iPixel;
                auto iByteOffset = iBitOffset / 8;
                int iX = 0;  // Used after for.
                for( ; iX + 7 < nBlockXSize; iX += 8, iByteOffset++ )
                {
                    int nRes = (!(!pabySrc[iX+0])) << 7;
                    nRes |= (!(!pabySrc[iX+1])) << 6;
                    nRes |= (!(!pabySrc[iX+2])) << 5;
                    nRes |= (!(!pabySrc[iX+3])) << 4;
                    nRes |= (!(!pabySrc[iX+4])) << 3;
                    nRes |= (!(!pabySrc[iX+5])) << 2;
                    nRes |= (!(!pabySrc[iX+6])) << 1;
                    nRes |= (!(!pabySrc[iX+7])) << 0;
                    m_poGDS->m_pabyBlockBuf[iByteOffset] = static_cast<GByte>(nRes);
                }
                iBitOffset = iByteOffset * 8;
                if( iX < nBlockXSize )
                {
                    int nRes = 0;
                    for( ; iX < nBlockXSize; ++iX )
                    {
                        if( pabySrc[iX] )
                            nRes |= (0x80 >>(iBitOffset & 7) );
                        ++iBitOffset;
                    }
                    m_poGDS->m_pabyBlockBuf[iBitOffset>>3] =
                        static_cast<GByte>(nRes);
                }
            }

            m_poGDS->m_bLoadedBlockDirty = true;

            return CE_None;
        }

        if( eDataType == GDT_Float32 && m_poGDS->m_nBitsPerSample == 16 )
        {
            for( ; iPixel < static_cast<GPtrDiff_t>(nBlockYSize) * nBlockXSize; iPixel++ )
            {
                GUInt32 nInWord = static_cast<GUInt32 *>(pImage)[iPixel];
                bool bClipWarn = m_poGDS->m_bClipWarn;
                GUInt16 nHalf = FloatToHalf(nInWord, bClipWarn);
                m_poGDS->m_bClipWarn = bClipWarn;
                reinterpret_cast<GUInt16*>(m_poGDS->m_pabyBlockBuf)[iPixel] = nHalf;
            }

            m_poGDS->m_bLoadedBlockDirty = true;

            return CE_None;
        }

        // Initialize to zero as we set the buffer with binary or operations.
        if( m_poGDS->m_nBitsPerSample != 24 )
            memset(m_poGDS->m_pabyBlockBuf, 0, static_cast<size_t>((nBitsPerLine / 8) * nBlockYSize));

        for( int iY = 0; iY < nBlockYSize; ++iY )
        {
            GInt64 iBitOffset = iY * nBitsPerLine;

            if( m_poGDS->m_nBitsPerSample == 12 )
            {
                for( int iX = 0; iX < nBlockXSize; ++iX )
                {
                    GUInt32 nInWord = static_cast<GUInt16 *>(pImage)[iPixel++];
                    if( nInWord > nMaxVal )
                    {
                        nInWord = nMaxVal;
                        if( !m_poGDS->m_bClipWarn )
                        {
                            m_poGDS->m_bClipWarn = true;
                            ReportError(
                                CE_Warning, CPLE_AppDefined,
                                "One or more pixels clipped to fit %d bit "
                                "domain.", m_poGDS->m_nBitsPerSample );
                        }
                    }

                    if( (iBitOffset % 8) == 0 )
                    {
                        m_poGDS->m_pabyBlockBuf[iBitOffset>>3] =
                            static_cast<GByte>(nInWord >> 4);
                        // Let 4 lower bits to zero as they're going to be
                        // overridden by the next word.
                        m_poGDS->m_pabyBlockBuf[(iBitOffset>>3)+1] =
                            static_cast<GByte>((nInWord & 0xf) << 4);
                    }
                    else
                    {
                        // Must or to preserve the 4 upper bits written
                        // for the previous word.
                        m_poGDS->m_pabyBlockBuf[iBitOffset>>3] |=
                            static_cast<GByte>(nInWord >> 8);
                        m_poGDS->m_pabyBlockBuf[(iBitOffset>>3)+1] =
                            static_cast<GByte>(nInWord & 0xff);
                    }

                    iBitOffset += m_poGDS->m_nBitsPerSample;
                }
                continue;
            }

            for( int iX = 0; iX < nBlockXSize; ++iX )
            {
                GUInt32 nInWord = 0;
                if( eDataType == GDT_Byte )
                {
                    nInWord = static_cast<GByte *>(pImage)[iPixel++];
                }
                else if( eDataType == GDT_UInt16 )
                {
                    nInWord = static_cast<GUInt16 *>(pImage)[iPixel++];
                }
                else if( eDataType == GDT_UInt32 )
                {
                    nInWord = static_cast<GUInt32 *>(pImage)[iPixel++];
                }
                else
                {
                    CPLAssert(false);
                }

                if( nInWord > nMaxVal )
                {
                    nInWord = nMaxVal;
                    if( !m_poGDS->m_bClipWarn )
                    {
                        m_poGDS->m_bClipWarn = true;
                        ReportError(
                            CE_Warning, CPLE_AppDefined,
                            "One or more pixels clipped to fit %d bit domain.",
                            m_poGDS->m_nBitsPerSample );
                    }
                }

                if( m_poGDS->m_nBitsPerSample == 24 )
                {
/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugh (#2361).              */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 0] =
                        static_cast<GByte>( nInWord );
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 1] =
                        static_cast<GByte>( nInWord >> 8 );
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 2] =
                        static_cast<GByte>( nInWord >> 16 );
#else
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 0] =
                        static_cast<GByte>( nInWord >> 16 );
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 1] =
                        static_cast<GByte>( nInWord >> 8 );
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 2] =
                        static_cast<GByte>( nInWord );
#endif
                    iBitOffset += 24;
                }
                else
                {
                    for( int iBit = 0; iBit < m_poGDS->m_nBitsPerSample; ++iBit )
                    {
                        if( nInWord &
                            (1 << (m_poGDS->m_nBitsPerSample - 1 - iBit)) )
                            m_poGDS->m_pabyBlockBuf[iBitOffset>>3] |=
                                ( 0x80 >> (iBitOffset & 7) );
                        ++iBitOffset;
                    }
                }
            }
        }

        m_poGDS->m_bLoadedBlockDirty = true;

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Handle case of pixel interleaved (PLANARCONFIG_CONTIG) images.  */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      On write of pixel interleaved data, we might as well flush      */
/*      out any other bands that are dirty in our cache.  This is       */
/*      especially helpful when writing compressed blocks.              */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < m_poGDS->nBands; ++iBand )
    {
        const GByte *pabyThisImage = nullptr;
        GDALRasterBlock *poBlock = nullptr;

        if( iBand + 1 == nBand )
        {
            pabyThisImage = static_cast<GByte *>( pImage );
        }
        else
        {
            poBlock =
                cpl::down_cast<GTiffOddBitsBand *>(
                    m_poGDS->GetRasterBand( iBand + 1 ))
                        ->TryGetLockedBlockRef( nBlockXOff, nBlockYOff );

            if( poBlock == nullptr )
                continue;

            if( !poBlock->GetDirty() )
            {
                poBlock->DropLock();
                continue;
            }

            pabyThisImage = static_cast<GByte *>(poBlock->GetDataRef());
        }

        const int iPixelBitSkip = m_poGDS->m_nBitsPerSample * m_poGDS->nBands;
        const int iBandBitOffset = iBand * m_poGDS->m_nBitsPerSample;

        // Bits per line rounds up to next byte boundary.
        GInt64 nBitsPerLine = static_cast<GInt64>(nBlockXSize) * iPixelBitSkip;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        GPtrDiff_t iPixel = 0;

        if( eDataType == GDT_Float32 && m_poGDS->m_nBitsPerSample == 16 )
        {
            for( ; iPixel < static_cast<GPtrDiff_t>(nBlockYSize) * nBlockXSize; iPixel++ )
            {
                GUInt32 nInWord = reinterpret_cast<const GUInt32 *>(
                                                        pabyThisImage)[iPixel];
                bool bClipWarn = m_poGDS->m_bClipWarn;
                GUInt16 nHalf = FloatToHalf(nInWord, bClipWarn);
                m_poGDS->m_bClipWarn = bClipWarn;
                reinterpret_cast<GUInt16*>(m_poGDS->m_pabyBlockBuf)[
                                    iPixel * m_poGDS->nBands + iBand] = nHalf;
            }

            if( poBlock != nullptr )
            {
                poBlock->MarkClean();
                poBlock->DropLock();
            }
            continue;
        }

        for( int iY = 0; iY < nBlockYSize; ++iY )
        {
            GInt64 iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            if( m_poGDS->m_nBitsPerSample == 12 )
            {
                for( int iX = 0; iX < nBlockXSize; ++iX )
                {
                    GUInt32 nInWord =
                        reinterpret_cast<const GUInt16 *>(
                            pabyThisImage)[iPixel++];
                    if( nInWord > nMaxVal )
                    {
                        nInWord = nMaxVal;
                        if( !m_poGDS->m_bClipWarn )
                        {
                            m_poGDS->m_bClipWarn = true;
                            ReportError(
                                CE_Warning, CPLE_AppDefined,
                                "One or more pixels clipped to fit %d bit "
                                "domain.", m_poGDS->m_nBitsPerSample );
                        }
                    }

                    if( (iBitOffset % 8) == 0 )
                    {
                        m_poGDS->m_pabyBlockBuf[iBitOffset>>3] =
                            static_cast<GByte>( nInWord >> 4 );
                        m_poGDS->m_pabyBlockBuf[(iBitOffset>>3)+1] =
                            static_cast<GByte>(
                                ((nInWord & 0xf) << 4) |
                                (m_poGDS->m_pabyBlockBuf[(iBitOffset>>3)+1] &
                                 0xf) );
                    }
                    else
                    {
                        m_poGDS->m_pabyBlockBuf[iBitOffset>>3] =
                            static_cast<GByte>(
                                (m_poGDS->m_pabyBlockBuf[iBitOffset>>3] &
                                 0xf0) |
                                (nInWord >> 8));
                        m_poGDS->m_pabyBlockBuf[(iBitOffset>>3)+1] =
                            static_cast<GByte>(nInWord & 0xff);
                    }

                    iBitOffset += iPixelBitSkip;
                }
                continue;
            }

            for( int iX = 0; iX < nBlockXSize; ++iX )
            {
                GUInt32 nInWord = 0;
                if( eDataType == GDT_Byte )
                {
                    nInWord =
                        static_cast<const GByte *>(pabyThisImage)[iPixel++];
                }
                else if( eDataType == GDT_UInt16 )
                {
                    nInWord = reinterpret_cast<const GUInt16 *>(
                        pabyThisImage)[iPixel++];
                }
                else if( eDataType == GDT_UInt32 )
                {
                    nInWord = reinterpret_cast<const GUInt32 *>(
                        pabyThisImage)[iPixel++];
                }
                else
                {
                    CPLAssert(false);
                }

                if( nInWord > nMaxVal )
                {
                    nInWord = nMaxVal;
                    if( !m_poGDS->m_bClipWarn )
                    {
                        m_poGDS->m_bClipWarn = true;
                        ReportError(
                            CE_Warning, CPLE_AppDefined,
                            "One or more pixels clipped to fit %d bit domain.",
                            m_poGDS->m_nBitsPerSample );
                    }
                }

                if( m_poGDS->m_nBitsPerSample == 24 )
                {
/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugh (#2361).              */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 0] =
                        static_cast<GByte>(nInWord);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 1] =
                        static_cast<GByte>(nInWord >> 8);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 2] =
                        static_cast<GByte>(nInWord >> 16);
#else
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 0] =
                        static_cast<GByte>(nInWord >> 16);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 1] =
                        static_cast<GByte>(nInWord >> 8);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset>>3) + 2] =
                        static_cast<GByte>(nInWord);
#endif
                    iBitOffset += 24;
                }
                else
                {
                    for( int iBit = 0; iBit < m_poGDS->m_nBitsPerSample; ++iBit )
                    {
                        // TODO(schwehr): Revisit this block.
                        if( nInWord &
                            (1 << (m_poGDS->m_nBitsPerSample - 1 - iBit)) )
                        {
                            m_poGDS->m_pabyBlockBuf[iBitOffset>>3] |=
                                ( 0x80 >> (iBitOffset & 7) );
                        }
                        else
                        {
                            // We must explicitly unset the bit as we
                            // may update an existing block.
                            m_poGDS->m_pabyBlockBuf[iBitOffset>>3] &=
                                ~(0x80 >>(iBitOffset & 7));
                        }

                        ++iBitOffset;
                    }
                }

                iBitOffset = iBitOffset + iPixelBitSkip - m_poGDS->m_nBitsPerSample;
            }
        }

        if( poBlock != nullptr )
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    m_poGDS->m_bLoadedBlockDirty = true;

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

static void ExpandPacked8ToByte1( const GByte * const CPL_RESTRICT pabySrc,
                                  GByte* const CPL_RESTRICT pabyDest,
                                  GPtrDiff_t nBytes )
{
    for( decltype(nBytes) i = 0, j = 0; i < nBytes; i++, j+= 8 )
    {
        const GByte byVal = pabySrc[i];
        pabyDest[j+0] = (byVal >> 7) & 0x1;
        pabyDest[j+1] = (byVal >> 6) & 0x1;
        pabyDest[j+2] = (byVal >> 5) & 0x1;
        pabyDest[j+3] = (byVal >> 4) & 0x1;
        pabyDest[j+4] = (byVal >> 3) & 0x1;
        pabyDest[j+5] = (byVal >> 2) & 0x1;
        pabyDest[j+6] = (byVal >> 1) & 0x1;
        pabyDest[j+7] = (byVal >> 0) & 0x1;
    }
}

#if defined(__GNUC__) || defined(_MSC_VER)
// Signedness of char implementation dependent, so be explicit.
// Assumes 2-complement integer types and sign extension of right shifting
// GCC guarantees such:
// https://gcc.gnu.org/onlinedocs/gcc/Integers-implementation.html#Integers-implementation
static inline GByte ExtractBitAndConvertTo255(GByte byVal, int nBit)
{
    return
        static_cast<GByte>(static_cast<signed char>(byVal << (7 - nBit)) >> 7);
}
#else
// Portable way
static inline GByte ExtractBitAndConvertTo255(GByte byVal, int nBit)
{
    return (byVal & (1 << nBit)) ? 255 : 0;
}
#endif

static void ExpandPacked8ToByte255( const GByte * const CPL_RESTRICT pabySrc,
                                    GByte* const CPL_RESTRICT pabyDest,
                                    GPtrDiff_t nBytes )
{
    for( decltype(nBytes) i = 0, j = 0; i < nBytes; i++, j += 8 )
    {
        const GByte byVal = pabySrc[i];
        pabyDest[j+0] = ExtractBitAndConvertTo255(byVal, 7);
        pabyDest[j+1] = ExtractBitAndConvertTo255(byVal, 6);
        pabyDest[j+2] = ExtractBitAndConvertTo255(byVal, 5);
        pabyDest[j+3] = ExtractBitAndConvertTo255(byVal, 4);
        pabyDest[j+4] = ExtractBitAndConvertTo255(byVal, 3);
        pabyDest[j+5] = ExtractBitAndConvertTo255(byVal, 2);
        pabyDest[j+6] = ExtractBitAndConvertTo255(byVal, 1);
        pabyDest[j+7] = ExtractBitAndConvertTo255(byVal, 0);
    }
}

CPLErr GTiffOddBitsBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    m_poGDS->Crystalize();

    CPLAssert(nBlocksPerRow != 0);
    int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId += (nBand - 1) * m_poGDS->m_nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*      Handle the case of a strip in a writable file that doesn't      */
/*      exist yet, but that we want to read.  Just set to zeros and     */
/*      return.                                                         */
/* -------------------------------------------------------------------- */
    if( nBlockId != m_poGDS->m_nLoadedBlock )
    {
        bool bErrOccurred = false;
        if( !m_poGDS->IsBlockAvailable(nBlockId, nullptr, nullptr, &bErrOccurred) )
        {
            NullBlock( pImage );
            if( bErrOccurred )
                return CE_Failure;
            return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    {
        const CPLErr eErr = m_poGDS->LoadBlockBuf( nBlockId );
        if( eErr != CE_None )
            return eErr;
    }

    if( m_poGDS->m_nBitsPerSample == 1 &&
        (m_poGDS->nBands == 1 || m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE ) )
    {
/* -------------------------------------------------------------------- */
/*      Translate 1bit data to eight bit.                               */
/* -------------------------------------------------------------------- */
        GPtrDiff_t iDstOffset = 0;
        const GByte * const CPL_RESTRICT m_pabyBlockBuf = m_poGDS->m_pabyBlockBuf;
        GByte* CPL_RESTRICT pabyDest = static_cast<GByte *>(pImage);

        for( int iLine = 0; iLine < nBlockYSize; ++iLine )
        {
            GPtrDiff_t iSrcOffsetByte = static_cast<GPtrDiff_t>((nBlockXSize + 7) >> 3) * iLine;

            if( !m_poGDS->m_bPromoteTo8Bits )
            {
                ExpandPacked8ToByte1( m_pabyBlockBuf + iSrcOffsetByte,
                                      pabyDest + iDstOffset,
                                      nBlockXSize / 8 );
            }
            else
            {
                ExpandPacked8ToByte255( m_pabyBlockBuf + iSrcOffsetByte,
                                        pabyDest + iDstOffset,
                                        nBlockXSize / 8 );
            }
            GPtrDiff_t iSrcOffsetBit = (iSrcOffsetByte + nBlockXSize / 8) * 8;
            iDstOffset += nBlockXSize & ~0x7;
            const GByte bSetVal = m_poGDS->m_bPromoteTo8Bits ? 255 : 1;
            for( int iPixel = nBlockXSize & ~0x7 ;
                 iPixel < nBlockXSize;
                 ++iPixel, ++iSrcOffsetBit )
            {
                if( m_pabyBlockBuf[iSrcOffsetBit >>3] &
                    (0x80 >> (iSrcOffsetBit & 0x7)) )
                    static_cast<GByte *>(pImage)[iDstOffset++] = bSetVal;
                else
                    static_cast<GByte *>(pImage)[iDstOffset++] = 0;
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      Handle the case of 16- and 24-bit floating point data as per    */
/*      TIFF Technical Note 3.                                          */
/* -------------------------------------------------------------------- */
    else if( eDataType == GDT_Float32 )
    {
        const int nWordBytes = m_poGDS->m_nBitsPerSample / 8;
        const GByte *pabyImage = m_poGDS->m_pabyBlockBuf +
            ( ( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE ) ? 0 :
              (nBand - 1) * nWordBytes );
        const int iSkipBytes =
            ( m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE ) ?
            nWordBytes : m_poGDS->nBands * nWordBytes;

        const auto nBlockPixels = static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize;
        if( m_poGDS->m_nBitsPerSample == 16 )
        {
            for( GPtrDiff_t i = 0; i < nBlockPixels; ++i )
            {
                static_cast<GUInt32 *>(pImage)[i] =
                    HalfToFloat( *reinterpret_cast<const GUInt16 *>(pabyImage) );
                pabyImage += iSkipBytes;
            }
        }
        else if( m_poGDS->m_nBitsPerSample == 24 )
        {
            for( GPtrDiff_t i = 0; i < nBlockPixels; ++i )
            {
#ifdef CPL_MSB
                static_cast<GUInt32 *>(pImage)[i] =
                    TripleToFloat(
                        ( static_cast<GUInt32>(*(pabyImage + 0)) << 16)
                        | (static_cast<GUInt32>(*(pabyImage + 1)) << 8)
                        | static_cast<GUInt32>(*(pabyImage + 2)) );
#else
                static_cast<GUInt32 *>(pImage)[i] =
                    TripleToFloat(
                        ( static_cast<GUInt32>(*(pabyImage + 2)) << 16)
                        | (static_cast<GUInt32>(*(pabyImage + 1)) << 8)
                        | static_cast<GUInt32>(*pabyImage) );
#endif
                pabyImage += iSkipBytes;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for moving 12bit data somewhat more efficiently.   */
/* -------------------------------------------------------------------- */
    else if( m_poGDS->m_nBitsPerSample == 12 )
    {
        int iPixelBitSkip = 0;
        int iBandBitOffset = 0;

        if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelBitSkip = m_poGDS->nBands * m_poGDS->m_nBitsPerSample;
            iBandBitOffset = (nBand - 1) * m_poGDS->m_nBitsPerSample;
        }
        else
        {
            iPixelBitSkip = m_poGDS->m_nBitsPerSample;
        }

        // Bits per line rounds up to next byte boundary.
        GPtrDiff_t nBitsPerLine = static_cast<GPtrDiff_t>(nBlockXSize) * iPixelBitSkip;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        int iPixel = 0;
        for( int iY = 0; iY < nBlockYSize; ++iY )
        {
            GPtrDiff_t iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for( int iX = 0; iX < nBlockXSize; ++iX )
            {
                const auto iByte = iBitOffset >> 3;

                if( (iBitOffset & 0x7) == 0 )
                {
                    // Starting on byte boundary.

                    static_cast<GUInt16 *>(pImage)[iPixel++] =
                        (m_poGDS->m_pabyBlockBuf[iByte] << 4)
                        | (m_poGDS->m_pabyBlockBuf[iByte+1] >> 4);
                }
                else
                {
                    // Starting off byte boundary.

                    static_cast<GUInt16 *>(pImage)[iPixel++] =
                        ((m_poGDS->m_pabyBlockBuf[iByte] & 0xf) << 8)
                        | (m_poGDS->m_pabyBlockBuf[iByte+1]);
                }
                iBitOffset += iPixelBitSkip;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugh (#2361).              */
/* -------------------------------------------------------------------- */
    else if( m_poGDS->m_nBitsPerSample == 24 )
    {
        int iPixelByteSkip = 0;
        int iBandByteOffset = 0;

        if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelByteSkip = (m_poGDS->nBands * m_poGDS->m_nBitsPerSample) / 8;
            iBandByteOffset = ((nBand - 1) * m_poGDS->m_nBitsPerSample) / 8;
        }
        else
        {
            iPixelByteSkip = m_poGDS->m_nBitsPerSample / 8;
        }

        const GPtrDiff_t nBytesPerLine = static_cast<GPtrDiff_t>(nBlockXSize) * iPixelByteSkip;

        GPtrDiff_t iPixel = 0;
        for( int iY = 0; iY < nBlockYSize; ++iY )
        {
            GByte *pabyImage =
                m_poGDS->m_pabyBlockBuf + iBandByteOffset + iY * nBytesPerLine;

            for( int iX = 0; iX < nBlockXSize; ++iX )
            {
#ifdef CPL_MSB
                static_cast<GUInt32 *>(pImage)[iPixel++] =
                    ( static_cast<GUInt32>(*(pabyImage + 2)) << 16)
                    | (static_cast<GUInt32>(*(pabyImage + 1)) << 8)
                    | static_cast<GUInt32>(*(pabyImage + 0));
#else
                static_cast<GUInt32 *>(pImage)[iPixel++] =
                    ( static_cast<GUInt32>(*(pabyImage + 0)) << 16)
                    | (static_cast<GUInt32>(*(pabyImage + 1)) << 8)
                    | static_cast<GUInt32>(*(pabyImage + 2));
#endif
                pabyImage += iPixelByteSkip;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle 1-32 bit integer data.                                   */
/* -------------------------------------------------------------------- */
    else
    {
        unsigned iPixelBitSkip = 0;
        unsigned iBandBitOffset = 0;

        if( m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelBitSkip = m_poGDS->nBands * m_poGDS->m_nBitsPerSample;
            iBandBitOffset = (nBand - 1) * m_poGDS->m_nBitsPerSample;
        }
        else
        {
            iPixelBitSkip = m_poGDS->m_nBitsPerSample;
        }

        // Bits per line rounds up to next byte boundary.
        GUIntBig nBitsPerLine = static_cast<GUIntBig>(nBlockXSize) * iPixelBitSkip;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        const GByte * const m_pabyBlockBuf = m_poGDS->m_pabyBlockBuf;
        const unsigned nBitsPerSample = m_poGDS->m_nBitsPerSample;
        GPtrDiff_t iPixel = 0;

        if( nBitsPerSample == 1 && eDataType == GDT_Byte )
        {
          for( unsigned iY = 0; iY < static_cast<unsigned>(nBlockYSize); ++iY )
          {
            GUIntBig iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for( unsigned iX = 0; iX < static_cast<unsigned>(nBlockXSize); ++iX )
            {
                if( m_pabyBlockBuf[iBitOffset>>3] & (0x80 >>(iBitOffset & 7)) )
                    static_cast<GByte *>(pImage)[iPixel] = 1;
                else
                    static_cast<GByte *>(pImage)[iPixel] = 0;
                iBitOffset += iPixelBitSkip;
                iPixel++;
            }
          }
        }
        else
        {
          for( unsigned iY = 0; iY < static_cast<unsigned>(nBlockYSize); ++iY )
          {
            GUIntBig iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for( unsigned iX = 0; iX < static_cast<unsigned>(nBlockXSize); ++iX )
            {
                unsigned nOutWord = 0;

                for( unsigned iBit = 0; iBit < nBitsPerSample; ++iBit )
                {
                    if( m_pabyBlockBuf[iBitOffset>>3]
                        & (0x80 >>(iBitOffset & 7)) )
                        nOutWord |= (1 << (nBitsPerSample - 1 - iBit));
                    ++iBitOffset;
                }

                iBitOffset = iBitOffset + iPixelBitSkip - nBitsPerSample;

                if( eDataType == GDT_Byte )
                {
                    static_cast<GByte *>(pImage)[iPixel++] =
                        static_cast<GByte>(nOutWord);
                }
                else if( eDataType == GDT_UInt16 )
                {
                  static_cast<GUInt16 *>(pImage)[iPixel++] =
                      static_cast<GUInt16>(nOutWord);
                }
                else if( eDataType == GDT_UInt32 )
                {
                  static_cast<GUInt32 *>(pImage)[iPixel++] = nOutWord;
                }
                else
                {
                    CPLAssert(false);
                }
            }
          }
        }
    }

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
    CacheMaskForBlock(nBlockXOff,nBlockYOff);
#endif

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffBitmapBand                          */
/* ==================================================================== */
/************************************************************************/

class GTiffBitmapBand : public GTiffOddBitsBand
{
    friend class GTiffDataset;

    GDALColorTable *m_poColorTable = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GTiffBitmapBand)

  public:

                   GTiffBitmapBand( GTiffDataset *, int );
    virtual       ~GTiffBitmapBand();

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;
};

/************************************************************************/
/*                           GTiffBitmapBand()                          */
/************************************************************************/

GTiffBitmapBand::GTiffBitmapBand( GTiffDataset *poDSIn, int nBandIn )
        : GTiffOddBitsBand( poDSIn, nBandIn )

{
    eDataType = GDT_Byte;

    if( poDSIn->m_poColorTable != nullptr )
    {
        m_poColorTable = poDSIn->m_poColorTable->Clone();
    }
    else
    {
#ifdef ESRI_BUILD
        m_poColorTable = nullptr;
#else
        const GDALColorEntry oWhite = { 255, 255, 255, 255 };
        const GDALColorEntry oBlack = { 0, 0, 0, 255 };

        m_poColorTable = new GDALColorTable();

        if( poDSIn->m_nPhotometric == PHOTOMETRIC_MINISWHITE )
        {
            m_poColorTable->SetColorEntry( 0, &oWhite );
            m_poColorTable->SetColorEntry( 1, &oBlack );
        }
        else
        {
            m_poColorTable->SetColorEntry( 0, &oBlack );
            m_poColorTable->SetColorEntry( 1, &oWhite );
        }
#endif  // not defined ESRI_BUILD.
    }
}

/************************************************************************/
/*                          ~GTiffBitmapBand()                          */
/************************************************************************/

GTiffBitmapBand::~GTiffBitmapBand()

{
    delete m_poColorTable;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffBitmapBand::GetColorInterpretation()

{
    if( m_poGDS->m_bPromoteTo8Bits )
        return GCI_Undefined;

    return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffBitmapBand::GetColorTable()

{
    if( m_poGDS->m_bPromoteTo8Bits )
        return nullptr;

    return m_poColorTable;
}

/************************************************************************/
/* ==================================================================== */
/*                          GTiffSplitBitmapBand                        */
/* ==================================================================== */
/************************************************************************/

class GTiffSplitBitmapBand final : public GTiffBitmapBand
{
    friend class GTiffDataset;
    int m_nLastLineValid = -1;

  public:

                   GTiffSplitBitmapBand( GTiffDataset *, int );
    virtual       ~GTiffSplitBitmapBand();

    virtual int IGetDataCoverageStatus( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nMaskFlagStop,
                                        double* pdfDataPct) override;

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;
};

/************************************************************************/
/*                       GTiffSplitBitmapBand()                         */
/************************************************************************/

GTiffSplitBitmapBand::GTiffSplitBitmapBand( GTiffDataset *poDSIn, int nBandIn )
        : GTiffBitmapBand( poDSIn, nBandIn )

{
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                      ~GTiffSplitBitmapBand()                         */
/************************************************************************/

GTiffSplitBitmapBand::~GTiffSplitBitmapBand() {}


/************************************************************************/
/*                       IGetDataCoverageStatus()                       */
/************************************************************************/

int GTiffSplitBitmapBand::IGetDataCoverageStatus( int , int ,
                                             int , int ,
                                             int ,
                                             double* )
{
     return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
            GDAL_DATA_COVERAGE_STATUS_DATA;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBitmapBand::IReadBlock( int /* nBlockXOff */, int nBlockYOff,
                                         void * pImage )

{
    m_poGDS->Crystalize();

    if( m_nLastLineValid >= 0 && nBlockYOff > m_nLastLineValid )
        return CE_Failure;

    if( m_poGDS->m_pabyBlockBuf == nullptr )
    {
        m_poGDS->m_pabyBlockBuf =
            static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(TIFFScanlineSize(m_poGDS->m_hTIFF)) );
        if( m_poGDS->m_pabyBlockBuf == nullptr )
        {
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Read through to target scanline.                                */
/* -------------------------------------------------------------------- */
    if( m_poGDS->m_nLoadedBlock >= nBlockYOff )
        m_poGDS->m_nLoadedBlock = -1;

    while( m_poGDS->m_nLoadedBlock < nBlockYOff )
    {
        ++m_poGDS->m_nLoadedBlock;

        std::vector<CPLErrorHandlerAccumulatorStruct> aoErrors;
        CPLInstallErrorHandlerAccumulator(aoErrors);
        int nRet = TIFFReadScanline( m_poGDS->m_hTIFF, m_poGDS->m_pabyBlockBuf,
                                     m_poGDS->m_nLoadedBlock, 0 );
        CPLUninstallErrorHandlerAccumulator();

        for( size_t iError = 0; iError < aoErrors.size(); ++iError )
        {
            ReportError( aoErrors[iError].type,
                      aoErrors[iError].no,
                      "%s",
                      aoErrors[iError].msg.c_str() );
            // FAX decoding only handles EOF condition as a warning, so
            // catch it so as to turn on error when attempting to read
            // following lines, to avoid performance issues.
            if(  !m_poGDS->m_bIgnoreReadErrors &&
                    aoErrors[iError].msg.find("Premature EOF") !=
                                                    std::string::npos )
            {
                m_nLastLineValid = nBlockYOff;
                nRet = -1;
            }
        }

        if( nRet == -1
            && !m_poGDS->m_bIgnoreReadErrors )
        {
            ReportError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadScanline() failed." );
            m_poGDS->m_nLoadedBlock = -1;
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate 1bit data to eight bit.                               */
/* -------------------------------------------------------------------- */
    int iSrcOffset = 0;
    int iDstOffset = 0;

    for( int iPixel = 0; iPixel < nBlockXSize; ++iPixel, ++iSrcOffset )
    {
        if( m_poGDS->m_pabyBlockBuf[iSrcOffset >>3] & (0x80 >> (iSrcOffset & 0x7)) )
            static_cast<GByte *>(pImage)[iDstOffset++] = 1;
        else
            static_cast<GByte *>(pImage)[iDstOffset++] = 0;
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBitmapBand::IWriteBlock( int /* nBlockXOff */,
                                          int /* nBlockYOff */,
                                          void * /* pImage */ )

{
    ReportError( CE_Failure, CPLE_AppDefined,
              "Split bitmap bands are read-only." );
    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                            GTiffDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            GTiffDataset()                            */
/************************************************************************/

GTiffDataset::GTiffDataset():
    m_bStreamingIn(false),
    m_bStreamingOut(false),
    m_bScanDeferred(true),
    m_bSingleIFDOpened(false),
    m_bLoadedBlockDirty(false),
    m_bWriteError(false),
    m_bLookedForProjection(false),
    m_bLookedForMDAreaOrPoint(false),
    m_bGeoTransformValid(false),
    m_bCrystalized(true),
    m_bGeoTIFFInfoChanged(false),
    m_bForceUnsetGTOrGCPs(false),
    m_bForceUnsetProjection(false),
    m_bNoDataChanged(false),
    m_bNoDataSet(false),
    m_bMetadataChanged(false),
    m_bColorProfileMetadataChanged(false),
    m_bForceUnsetRPC(false),
    m_bNeedsRewrite(false),
    m_bLoadingOtherBands(false),
    m_bIsOverview(false),
    m_bWriteEmptyTiles(true),
    m_bFillEmptyTilesAtClosing(false),
    m_bTreatAsSplit(false),
    m_bTreatAsSplitBitmap(false),
    m_bClipWarn(false),
    m_bIMDRPCMetadataLoaded(false),
    m_bEXIFMetadataLoaded(false),
    m_bICCMetadataLoaded(false),
    m_bHasWarnedDisableAggressiveBandCaching(false),
    m_bDontReloadFirstBlock(false),
    m_bWebPLossless(false),
    m_bPromoteTo8Bits(false),
    m_bDebugDontWriteBlocks(CPLTestBool(CPLGetConfigOption("GTIFF_DONT_WRITE_BLOCKS", "NO"))),
    m_bIsFinalized(false),
    m_bIgnoreReadErrors(CPLTestBool(CPLGetConfigOption("GTIFF_IGNORE_READ_ERRORS", "NO"))),
    m_bDirectIO(CPLTestBool(CPLGetConfigOption("GTIFF_DIRECT_IO", "NO"))),
    m_bReadGeoTransform(false),
    m_bLoadPam(false),
    m_bHasGotSiblingFiles(false),
    m_bHasIdentifiedAuthorizedGeoreferencingSources(false),
    m_bLayoutIFDSBeforeData(false),
    m_bBlockOrderRowMajor(false),
    m_bLeaderSizeAsUInt4(false),
    m_bTrailerRepeatedLast4BytesRepeated(false),
    m_bMaskInterleavedWithImagery(false),
    m_bKnownIncompatibleEdition(false),
    m_bWriteKnownIncompatibleEdition(false),
    m_bHasUsedReadEncodedAPI(false),
    m_bWriteCOGLayout(false)
{
    //CPLDebug("GDAL", "sizeof(GTiffDataset) = %d bytes", static_cast<int>(
    //    sizeof(GTiffDataset)));

    const char* pszVirtualMemIO =
        CPLGetConfigOption("GTIFF_VIRTUAL_MEM_IO", "NO");
    if( EQUAL(pszVirtualMemIO, "IF_ENOUGH_RAM") )
        m_eVirtualMemIOUsage = VirtualMemIOEnum::IF_ENOUGH_RAM;
    else if( CPLTestBool(pszVirtualMemIO) )
        m_eVirtualMemIOUsage = VirtualMemIOEnum::YES;

    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

/************************************************************************/
/*                           ~GTiffDataset()                            */
/************************************************************************/

GTiffDataset::~GTiffDataset()

{
    Finalize();
    if( m_pszTmpFilename )
    {
        VSIUnlink(m_pszTmpFilename);
        CPLFree(m_pszTmpFilename);
    }
}

/************************************************************************/
/*                             Finalize()                               */
/************************************************************************/

int GTiffDataset::Finalize()
{
    if( m_bIsFinalized )
        return FALSE;

    bool bHasDroppedRef = false;

    Crystalize();

    if( m_bColorProfileMetadataChanged )
    {
        SaveICCProfile(this, nullptr, nullptr, 0);
        m_bColorProfileMetadataChanged = false;
    }

/* -------------------------------------------------------------------- */
/*      Handle forcing xml:ESRI data to be written to PAM.              */
/* -------------------------------------------------------------------- */
    if( CPLTestBool(CPLGetConfigOption( "ESRI_XML_PAM", "NO" )) )
    {
        char **papszESRIMD = GTiffDataset::GetMetadata("xml:ESRI");
        if( papszESRIMD )
        {
            GDALPamDataset::SetMetadata( papszESRIMD, "xml:ESRI");
        }
    }

    if( m_psVirtualMemIOMapping )
        CPLVirtualMemFree( m_psVirtualMemIOMapping );
    m_psVirtualMemIOMapping = nullptr;

/* -------------------------------------------------------------------- */
/*      Fill in missing blocks with empty data.                         */
/* -------------------------------------------------------------------- */
    if( m_bFillEmptyTilesAtClosing )
    {
/* -------------------------------------------------------------------- */
/*  Ensure any blocks write cached by GDAL gets pushed through libtiff. */
/* -------------------------------------------------------------------- */
        FlushCacheInternal( false /* do not call FlushDirectory */ );

        FillEmptyTiles();
        m_bFillEmptyTilesAtClosing = false;
    }

/* -------------------------------------------------------------------- */
/*      Force a complete flush, including either rewriting(moving)      */
/*      of writing in place the current directory.                      */
/* -------------------------------------------------------------------- */
    FlushCacheInternal( true );

    // Destroy compression queue
    if( m_poCompressQueue )
    {
        m_poCompressQueue->WaitCompletion();

        for( int i = 0; i < static_cast<int>(m_asCompressionJobs.size()); ++i )
        {
            CPLFree(m_asCompressionJobs[i].pabyBuffer);
            if( m_asCompressionJobs[i].pszTmpFilename )
            {
                VSIUnlink(m_asCompressionJobs[i].pszTmpFilename);
                CPLFree(m_asCompressionJobs[i].pszTmpFilename);
            }
        }
        CPLDestroyMutex(m_hCompressThreadPoolMutex);
        m_poCompressQueue.reset();
    }

/* -------------------------------------------------------------------- */
/*      If there is still changed metadata, then presumably we want     */
/*      to push it into PAM.                                            */
/* -------------------------------------------------------------------- */
    if( m_bMetadataChanged )
    {
        PushMetadataToPam();
        m_bMetadataChanged = false;
        GDALPamDataset::FlushCache();
    }

/* -------------------------------------------------------------------- */
/*      Cleanup overviews.                                              */
/* -------------------------------------------------------------------- */
    if( !m_poBaseDS )
    {
        for( int i = 0; i < m_nOverviewCount; ++i )
        {
            delete m_papoOverviewDS[i];
            bHasDroppedRef = true;
        }
        m_nOverviewCount = 0;

        for( int i = 0; i < m_nJPEGOverviewCountOri; ++i )
        {
            delete m_papoJPEGOverviewDS[i];
            bHasDroppedRef = true;
        }
        m_nJPEGOverviewCount = 0;
        m_nJPEGOverviewCountOri = 0;
        CPLFree( m_papoJPEGOverviewDS );
        m_papoJPEGOverviewDS = nullptr;
    }

    // If we are a mask dataset, we can have overviews, but we don't
    // own them. We can only free the array, not the overviews themselves.
    CPLFree( m_papoOverviewDS );
    m_papoOverviewDS = nullptr;

    // m_poMaskDS is owned by the main image and the overviews
    // so because of the latter case, we can delete it even if
    // we are not the base image.
    if( m_poMaskDS )
    {
        delete m_poMaskDS;
        m_poMaskDS = nullptr;
        bHasDroppedRef = true;
    }

    if( m_poColorTable != nullptr )
        delete m_poColorTable;
    m_poColorTable = nullptr;

    if( m_hTIFF )
    {
        XTIFFClose( m_hTIFF );
        m_hTIFF = nullptr;
    }

    if ( !m_poBaseDS )
    {
        if( m_fpL != nullptr )
        {
            if( m_bWriteKnownIncompatibleEdition )
            {
                GByte abyHeader[4096];
                VSIFSeekL( m_fpL, 0, SEEK_SET );
                VSIFReadL( abyHeader, 1, sizeof(abyHeader), m_fpL );
                const char* szKeyToLook = "KNOWN_INCOMPATIBLE_EDITION=NO\n "; // trailing space intended
                for( size_t i = 0; i < sizeof(abyHeader) - strlen(szKeyToLook); i++ )
                {
                    if( memcmp(abyHeader + i, szKeyToLook, strlen(szKeyToLook)) == 0 )
                    {
                        const char* szNewKey = "KNOWN_INCOMPATIBLE_EDITION=YES\n";
                        CPLAssert( strlen(szKeyToLook) == strlen(szNewKey) );
                        memcpy(abyHeader + i, szNewKey, strlen(szNewKey));
                        VSIFSeekL( m_fpL, 0, SEEK_SET );
                        VSIFWriteL( abyHeader, 1, sizeof(abyHeader), m_fpL );
                        break;
                    }
                }
            }
            if( VSIFCloseL( m_fpL ) != 0 )
            {
                ReportError(CE_Failure, CPLE_FileIO, "I/O error");
            }
            m_fpL = nullptr;
        }
    }

    if( m_fpToWrite != nullptr )
    {
        if( VSIFCloseL( m_fpToWrite ) != 0 )
        {
            ReportError(CE_Failure, CPLE_FileIO, "I/O error");
        }
        m_fpToWrite = nullptr;
    }

    if( m_nGCPCount > 0 )
    {
        GDALDeinitGCPs( m_nGCPCount, m_pasGCPList );
        CPLFree( m_pasGCPList );
        m_pasGCPList = nullptr;
        m_nGCPCount = 0;
    }

    CSLDestroy( m_papszCreationOptions );
    m_papszCreationOptions = nullptr;

    CPLFree(m_pabyTempWriteBuffer);
    m_pabyTempWriteBuffer = nullptr;

    m_bIMDRPCMetadataLoaded = false;
    CSLDestroy(m_papszMetadataFiles);
    m_papszMetadataFiles = nullptr;

    VSIFree(m_pTempBufferForCommonDirectIO);
    m_pTempBufferForCommonDirectIO = nullptr;

    CPLFree(m_panMaskOffsetLsb);
    m_panMaskOffsetLsb = nullptr;

    CPLFree(m_pszVertUnit);
    m_pszVertUnit = nullptr;

    CPLFree(m_pszFilename);
    m_pszFilename = nullptr;

    CPLFree(m_pszGeorefFilename);
    m_pszGeorefFilename = nullptr;

    m_bIsFinalized = true;

    return bHasDroppedRef;
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int GTiffDataset::CloseDependentDatasets()
{
    if( m_poBaseDS )
        return FALSE;

    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    bHasDroppedRef |= Finalize();

    return bHasDroppedRef;
}

/************************************************************************/
/*                        GetJPEGOverviewCount()                        */
/************************************************************************/

int GTiffDataset::GetJPEGOverviewCount()
{
    if( m_nJPEGOverviewCount >= 0 )
        return m_nJPEGOverviewCount;

    m_nJPEGOverviewCount = 0;
    if( m_poBaseDS || eAccess != GA_ReadOnly || m_nCompression != COMPRESSION_JPEG ||
        (nRasterXSize < 256 && nRasterYSize < 256) ||
        !CPLTestBool(CPLGetConfigOption("GTIFF_IMPLICIT_JPEG_OVR", "YES")) ||
        GDALGetDriverByName("JPEG") == nullptr )
    {
        return 0;
    }
    const char* pszSourceColorSpace =
        m_oGTiffMDMD.GetMetadataItem( "SOURCE_COLOR_SPACE", "IMAGE_STRUCTURE" );
    if( pszSourceColorSpace != nullptr && EQUAL(pszSourceColorSpace, "CMYK") )
    {
        // We cannot handle implicit overviews on JPEG CMYK datasets converted
        // to RGBA This would imply doing the conversion in
        // GTiffJPEGOverviewBand.
        return 0;
    }

    // libjpeg-6b only supports 2, 4 and 8 scale denominators.
    // TODO: Later versions support more.
    for( signed char i = 2; i >= 0; i-- )
    {
        if( nRasterXSize >= (256 << i) || nRasterYSize >= (256 << i) )
        {
            m_nJPEGOverviewCount = i + 1;
            break;
        }
    }
    if( m_nJPEGOverviewCount == 0 )
        return 0;

    // Get JPEG tables.
    uint32_t nJPEGTableSize = 0;
    void* pJPEGTable = nullptr;
    GByte abyFFD8[] = { 0xFF, 0xD8 };
    if( TIFFGetField(m_hTIFF, TIFFTAG_JPEGTABLES, &nJPEGTableSize, &pJPEGTable) )
    {
        if( pJPEGTable == nullptr ||
            nJPEGTableSize > INT_MAX ||
            static_cast<GByte*>(pJPEGTable)[nJPEGTableSize-1] != 0xD9 )
        {
            m_nJPEGOverviewCount = 0;
            return 0;
        }
        nJPEGTableSize--;  // Remove final 0xD9.
    }
    else
    {
        pJPEGTable = abyFFD8;
        nJPEGTableSize = 2;
    }

    m_papoJPEGOverviewDS =
        static_cast<GTiffJPEGOverviewDS **>(
            CPLMalloc( sizeof(GTiffJPEGOverviewDS*) * m_nJPEGOverviewCount ) );
    for( int i = 0; i < m_nJPEGOverviewCount; ++i )
    {
        m_papoJPEGOverviewDS[i] =
            new GTiffJPEGOverviewDS(
                this, i + 1,
                pJPEGTable, static_cast<int>(nJPEGTableSize) );
    }

    m_nJPEGOverviewCountOri = m_nJPEGOverviewCount;

    return m_nJPEGOverviewCount;
}

/************************************************************************/
/*                           FillEmptyTiles()                           */
/************************************************************************/

void GTiffDataset::FillEmptyTiles()

{
/* -------------------------------------------------------------------- */
/*      How many blocks are there in this file?                         */
/* -------------------------------------------------------------------- */
    const int nBlockCount =
        m_nPlanarConfig == PLANARCONFIG_SEPARATE ?
        m_nBlocksPerBand * nBands :
        m_nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*      Fetch block maps.                                               */
/* -------------------------------------------------------------------- */
    toff_t *panByteCounts = nullptr;

    if( TIFFIsTiled( m_hTIFF ) )
        TIFFGetField( m_hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts );
    else
        TIFFGetField( m_hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts );

    if( panByteCounts == nullptr )
    {
        // Got here with libtiff 3.9.3 and tiff_write_8 test.
        ReportError( CE_Failure, CPLE_AppDefined,
                  "FillEmptyTiles() failed because panByteCounts == NULL" );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Prepare a blank data buffer to write for uninitialized blocks.  */
/* -------------------------------------------------------------------- */
    const GPtrDiff_t nBlockBytes =
        TIFFIsTiled( m_hTIFF ) ?
        static_cast<GPtrDiff_t>(TIFFTileSize(m_hTIFF)) :
        static_cast<GPtrDiff_t>(TIFFStripSize(m_hTIFF));

    GByte *pabyData =
        static_cast<GByte *>( VSI_CALLOC_VERBOSE(nBlockBytes, 1) );
    if( pabyData == nullptr )
    {
        return;
    }

    // Force tiles completely filled with the nodata value to be written.
    m_bWriteEmptyTiles = true;

/* -------------------------------------------------------------------- */
/*      If set, fill data buffer with no data value.                    */
/* -------------------------------------------------------------------- */
    if( m_bNoDataSet && m_dfNoDataValue != 0.0 )
    {
        const GDALDataType eDataType = GetRasterBand( 1 )->GetRasterDataType();
        const int nDataTypeSize = GDALGetDataTypeSizeBytes( eDataType );
        if( nDataTypeSize &&
            nDataTypeSize * 8 == static_cast<int>(m_nBitsPerSample) )
        {
            GDALCopyWords64( &m_dfNoDataValue, GDT_Float64, 0,
                           pabyData, eDataType,
                           nDataTypeSize,
                           nBlockBytes / nDataTypeSize );
        }
        else if( nDataTypeSize )
        {
            // Handle non power-of-two depths.
            // Ideally make a packed buffer, but that is a bit tedious,
            // so use the normal I/O interfaces.

            CPLFree( pabyData );

            pabyData = static_cast<GByte *>(
                VSI_MALLOC3_VERBOSE(m_nBlockXSize, m_nBlockYSize, nDataTypeSize) );
            if( pabyData == nullptr )
                return;
            GDALCopyWords64( &m_dfNoDataValue, GDT_Float64, 0,
                           pabyData, eDataType,
                           nDataTypeSize,
                           static_cast<GPtrDiff_t>(m_nBlockXSize) * m_nBlockYSize );
            const int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, m_nBlockYSize);
            for( int iBlock = 0; iBlock < nBlockCount; ++iBlock )
            {
                if( panByteCounts[iBlock] == 0 )
                {
                    if( m_nPlanarConfig == PLANARCONFIG_SEPARATE || nBands == 1 )
                    {
                        CPL_IGNORE_RET_VAL( GetRasterBand(
                            1 + iBlock / m_nBlocksPerBand )->WriteBlock(
                                (iBlock % m_nBlocksPerBand) % nBlocksPerRow,
                                (iBlock % m_nBlocksPerBand) / nBlocksPerRow,
                                pabyData ) );
                    }
                    else
                    {
                        // In contig case, don't directly call WriteBlock(), as
                        // it could cause useless decompression-recompression.
                        const int nXOff =
                            (iBlock % nBlocksPerRow) * m_nBlockXSize;
                        const int nYOff =
                            (iBlock / nBlocksPerRow) * m_nBlockYSize;
                        const int nXSize =
                            (nXOff + m_nBlockXSize <= nRasterXSize) ?
                            m_nBlockXSize : nRasterXSize - nXOff;
                        const int nYSize =
                            (nYOff + m_nBlockYSize <= nRasterYSize) ?
                            m_nBlockYSize : nRasterYSize - nYOff;
                        for( int iBand = 1; iBand <= nBands; ++iBand )
                        {
                            CPL_IGNORE_RET_VAL( GetRasterBand( iBand )->
                                RasterIO(
                                    GF_Write, nXOff, nYOff, nXSize, nYSize,
                                    pabyData, nXSize, nYSize,
                                    eDataType, 0, 0, nullptr ) );
                        }
                    }
                }
            }
            CPLFree( pabyData );
            return;
        }
    }

/* -------------------------------------------------------------------- */
/*      When we must fill with zeroes, try to create non-sparse file    */
/*      w.r.t TIFF spec ... as a sparse file w.r.t filesystem, ie by    */
/*      seeking to end of file instead of writing zero blocks.          */
/* -------------------------------------------------------------------- */
    else if( m_nCompression == COMPRESSION_NONE && (m_nBitsPerSample % 8) == 0 )
    {
        // Only use libtiff to write the first sparse block to ensure that it
        // will serialize offset and count arrays back to disk.
        int nCountBlocksToZero = 0;
        for( int iBlock = 0; iBlock < nBlockCount; ++iBlock )
        {
            if( panByteCounts[iBlock] == 0 )
            {
                if( nCountBlocksToZero == 0 )
                {
                    const bool bWriteEmptyTilesBak = m_bWriteEmptyTiles;
                    m_bWriteEmptyTiles = true;
                    const bool bOK =
                        WriteEncodedTileOrStrip( iBlock, pabyData,
                                                 FALSE ) == CE_None;
                    m_bWriteEmptyTiles = bWriteEmptyTilesBak;
                    if( !bOK )
                        break;
                }
                nCountBlocksToZero++;
            }
        }
        CPLFree( pabyData );

        --nCountBlocksToZero;

        // And then seek to end of file for other ones.
        if( nCountBlocksToZero > 0 )
        {
            toff_t *panByteOffsets = nullptr;

            if( TIFFIsTiled( m_hTIFF ) )
                TIFFGetField( m_hTIFF, TIFFTAG_TILEOFFSETS, &panByteOffsets );
            else
                TIFFGetField( m_hTIFF, TIFFTAG_STRIPOFFSETS, &panByteOffsets );

            if( panByteOffsets == nullptr )
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "FillEmptyTiles() failed because panByteOffsets == NULL");
                return;
            }

            VSILFILE* fpTIF = VSI_TIFFGetVSILFile(TIFFClientdata( m_hTIFF ));
            VSIFSeekL( fpTIF, 0, SEEK_END );
            const vsi_l_offset nOffset = VSIFTellL(fpTIF);

            vsi_l_offset iBlockToZero = 0;
            for( int iBlock = 0; iBlock < nBlockCount; ++iBlock )
            {
                if( panByteCounts[iBlock] == 0 )
                {
                    panByteOffsets[iBlock] = static_cast<toff_t>(
                                        nOffset + iBlockToZero * nBlockBytes);
                    panByteCounts[iBlock] = nBlockBytes;
                    iBlockToZero++;
                }
            }
            CPLAssert( iBlockToZero ==
                       static_cast<vsi_l_offset>(nCountBlocksToZero) );

            if( VSIFTruncateL( fpTIF,
                               nOffset + iBlockToZero * nBlockBytes ) != 0 )
            {
                ReportError(CE_Failure, CPLE_FileIO,
                         "Cannot initialize empty blocks");
            }
        }

        return;
    }

/* -------------------------------------------------------------------- */
/*      Check all blocks, writing out data for uninitialized blocks.    */
/* -------------------------------------------------------------------- */

    GByte* pabyRaw = nullptr;
    vsi_l_offset nRawSize = 0;
    for( int iBlock = 0; iBlock < nBlockCount; ++iBlock )
    {
        if( panByteCounts[iBlock] == 0 )
        {
            if( pabyRaw == nullptr )
            {
                if( WriteEncodedTileOrStrip( iBlock, pabyData, FALSE
                                                                ) != CE_None )
                    break;

                vsi_l_offset nOffset = 0;
                bool b = IsBlockAvailable( iBlock, &nOffset, &nRawSize);
                CPL_IGNORE_RET_VAL(b);
                CPLAssert(b);

                // When using compression, get back the compressed block
                // so we can use the raw API to write it faster.
                if( m_nCompression != COMPRESSION_NONE )
                {
                    pabyRaw = static_cast<GByte*>(
                            VSI_MALLOC_VERBOSE(static_cast<size_t>(nRawSize)));
                    if( pabyRaw )
                    {
                        VSILFILE* fp = VSI_TIFFGetVSILFile(
                                                    TIFFClientdata( m_hTIFF ));
                        const vsi_l_offset nCurOffset = VSIFTellL(fp);
                        VSIFSeekL(fp, nOffset, SEEK_SET);
                        VSIFReadL(pabyRaw, 1, static_cast<size_t>(nRawSize), fp);
                        VSIFSeekL(fp, nCurOffset, SEEK_SET);
                    }
                }
            }
            else
            {
                WriteRawStripOrTile( iBlock, pabyRaw,
                                     static_cast<GPtrDiff_t>(nRawSize) );
            }
        }
    }

    CPLFree( pabyData );
    VSIFree( pabyRaw );
}

/************************************************************************/
/*                         HasOnlyNoData()                              */
/************************************************************************/

template<class T>
static inline bool IsEqualToNoData( T value, T noDataValue )
{
    return value == noDataValue;
}

template<> bool IsEqualToNoData<float>( float value, float noDataValue )
{
    return
        CPLIsNan(noDataValue) ?
        CPL_TO_BOOL(CPLIsNan(value)) : value == noDataValue;
}

template<> bool IsEqualToNoData<double>( double value, double noDataValue )
{
    return
        CPLIsNan(noDataValue) ?
        CPL_TO_BOOL(CPLIsNan(value)) : value == noDataValue;
}

template<class T>
bool GTiffDataset::HasOnlyNoDataT( const T* pBuffer, int nWidth, int nHeight,
                                   int nLineStride, int nComponents ) const
{
    const T noDataValue = static_cast<T>((m_bNoDataSet) ? m_dfNoDataValue : 0.0);

    CPLAssert(m_nBitsPerSample != 1 || noDataValue == 0);

    // Fast test: check the 4 corners and the middle pixel.
    for( int iBand = 0; iBand < nComponents; iBand++ )
    {
        if( !(IsEqualToNoData(pBuffer[iBand], noDataValue) &&
              IsEqualToNoData(
                  pBuffer[static_cast<size_t>(nWidth - 1) * nComponents +
                          iBand],
                  noDataValue) &&
              IsEqualToNoData(
                  pBuffer[(static_cast<size_t>(nHeight-1)/2 * nLineStride +
                           (nWidth - 1)/2) * nComponents + iBand],
                  noDataValue) &&
              IsEqualToNoData(
                  pBuffer[static_cast<size_t>(nHeight - 1) * nLineStride *
                          nComponents + iBand], noDataValue) &&
              IsEqualToNoData(
                  pBuffer[(static_cast<size_t>(nHeight - 1) * nLineStride +
                           nWidth - 1) * nComponents + iBand], noDataValue) ) )
        {
            return false;
        }
    }

    // Test all pixels.
    for( int iY = 0; iY < nHeight; iY++ )
    {
        for( int iX = 0; iX < nWidth * nComponents; iX++ )
        {
            if( !IsEqualToNoData(
                   pBuffer[iY * static_cast<size_t>(nLineStride) * nComponents +
                           iX], noDataValue) )
            {
                return false;
            }
        }
    }
    return true;
}

bool GTiffDataset::HasOnlyNoData( const void* pBuffer, int nWidth, int nHeight,
                                  int nLineStride, int nComponents )
{
    const GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();

    // In the case where the nodata is 0, we can compare several bytes at
    // once. Select the largest natural integer type for the architecture.
#if SIZEOF_VOIDP == 8 || defined(__x86_64__)
    // We test __x86_64__ for x32 arch where SIZEOF_VOIDP == 4
    typedef GUInt64 WordType;
#else
    typedef unsigned int WordType;
#endif
    if( (!m_bNoDataSet || m_dfNoDataValue == 0.0) && nWidth == nLineStride )
    {
        const GByte* pabyBuffer = static_cast<const GByte*>(pBuffer);
        const size_t nSize = (static_cast<size_t>(nWidth) * nHeight *
                                nComponents * m_nBitsPerSample + 7) / 8;
        size_t i = 0;
        const size_t nInitialIters = std::min(
            sizeof(WordType) -
                (reinterpret_cast<std::uintptr_t>(pabyBuffer) % sizeof(WordType)),
            nSize);
        for( ; i < nInitialIters; i++ )
        {
            if( pabyBuffer[i] )
                return false;
        }
        for( ; i + sizeof(WordType) - 1 < nSize; i += sizeof(WordType) )
        {
            if( *(reinterpret_cast<const WordType*>(pabyBuffer + i)) )
                return false;
        }
        for( ; i < nSize; i++ )
        {
            if( pabyBuffer[i] )
                return false;
        }
        return true;
    }

    if( m_nBitsPerSample == 8 )
    {
        if( m_nSampleFormat == SAMPLEFORMAT_INT )
        {
            return HasOnlyNoDataT(static_cast<const signed char*>(pBuffer),
                                  nWidth, nHeight, nLineStride, nComponents);
        }
        return HasOnlyNoDataT(static_cast<const GByte*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( m_nBitsPerSample == 16 && eDT == GDT_UInt16 )
    {
        return HasOnlyNoDataT(static_cast<const GUInt16*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( m_nBitsPerSample == 16 && eDT== GDT_Int16 )
    {
        return HasOnlyNoDataT(static_cast<const GInt16*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( m_nBitsPerSample == 32 && eDT == GDT_UInt32 )
    {
        return HasOnlyNoDataT(static_cast<const GUInt32*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( m_nBitsPerSample == 32 && eDT == GDT_Int32 )
    {
        return HasOnlyNoDataT(static_cast<const GInt32*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( m_nBitsPerSample == 32 && eDT == GDT_Float32 )
    {
        return HasOnlyNoDataT(static_cast<const float*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( m_nBitsPerSample == 64 && eDT == GDT_Float64 )
    {
        return HasOnlyNoDataT(static_cast<const double*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    return false;
}

/************************************************************************/
/*                     IsFirstPixelEqualToNoData()                      */
/************************************************************************/

inline bool GTiffDataset::IsFirstPixelEqualToNoData( const void* pBuffer )
{
    const GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
    const double dfEffectiveNoData = (m_bNoDataSet) ? m_dfNoDataValue : 0.0;
    if( m_nBitsPerSample == 8 || (m_nBitsPerSample < 8 && dfEffectiveNoData == 0) )
    {
        if( m_nSampleFormat == SAMPLEFORMAT_INT )
        {
            return GDALIsValueInRange<signed char>(dfEffectiveNoData) &&
                   *(static_cast<const signed char*>(pBuffer)) ==
                        static_cast<signed char>(dfEffectiveNoData);
        }
        return GDALIsValueInRange<GByte>(dfEffectiveNoData) &&
               *(static_cast<const GByte*>(pBuffer)) ==
                        static_cast<GByte>(dfEffectiveNoData);
    }
    if( m_nBitsPerSample == 16 && eDT == GDT_UInt16 )
    {
        return GDALIsValueInRange<GUInt16>(dfEffectiveNoData) &&
               *(static_cast<const GUInt16*>(pBuffer)) ==
                        static_cast<GUInt16>(dfEffectiveNoData);
    }
    if( m_nBitsPerSample == 16 && eDT == GDT_Int16 )
    {
        return GDALIsValueInRange<GInt16>(dfEffectiveNoData) &&
               *(static_cast<const GInt16*>(pBuffer)) ==
                        static_cast<GInt16>(dfEffectiveNoData);
    }
    if( m_nBitsPerSample == 32 && eDT == GDT_UInt32 )
    {
        return GDALIsValueInRange<GUInt32>(dfEffectiveNoData) &&
               *(static_cast<const GUInt32*>(pBuffer)) ==
                        static_cast<GUInt32>(dfEffectiveNoData);
    }
    if( m_nBitsPerSample == 32 && eDT == GDT_Int32 )
    {
        return GDALIsValueInRange<GInt32>(dfEffectiveNoData) &&
               *(static_cast<const GInt32*>(pBuffer)) ==
                        static_cast<GInt32>(dfEffectiveNoData);
    }
    if( m_nBitsPerSample == 32 && eDT == GDT_Float32 )
    {
        if( CPLIsNan(m_dfNoDataValue) )
            return CPL_TO_BOOL(
                CPLIsNan(*(static_cast<const float*>(pBuffer))));
        return GDALIsValueInRange<float>(dfEffectiveNoData) &&
               *(static_cast<const float*>(pBuffer)) ==
                        static_cast<float>(dfEffectiveNoData);
    }
    if( m_nBitsPerSample == 64 && eDT == GDT_Float64 )
    {
        if( CPLIsNan(dfEffectiveNoData) )
            return CPL_TO_BOOL(
                CPLIsNan(*(static_cast<const double*>(pBuffer))));
        return *(static_cast<const double*>(pBuffer)) == dfEffectiveNoData;
    }
    return false;
}

/************************************************************************/
/*                        WriteEncodedTile()                            */
/************************************************************************/

bool GTiffDataset::WriteEncodedTile( uint32_t tile, GByte *pabyData,
                                     int bPreserveDataBuffer )
{
    int iRow = 0;
    int iColumn = 0;
    int nBlocksPerRow = 1;
    int nBlocksPerColumn = 1;

/* -------------------------------------------------------------------- */
/*      Don't write empty blocks in some cases.                         */
/* -------------------------------------------------------------------- */
    if( !m_bWriteEmptyTiles && IsFirstPixelEqualToNoData(pabyData) )
    {
        if( !IsBlockAvailable(tile) )
        {
            const int nComponents =
                m_nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;
            nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, m_nBlockXSize);
            nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, m_nBlockYSize);

            iColumn = (tile % m_nBlocksPerBand) % nBlocksPerRow;
            iRow = (tile % m_nBlocksPerBand) / nBlocksPerRow;

            const int nActualBlockWidth =
                ( iColumn == nBlocksPerRow - 1 ) ?
                nRasterXSize - iColumn * m_nBlockXSize : m_nBlockXSize;
            const int nActualBlockHeight =
                ( iRow == nBlocksPerColumn - 1 ) ?
                nRasterYSize - iRow * m_nBlockYSize : m_nBlockYSize;

            if( HasOnlyNoData(pabyData,
                              nActualBlockWidth, nActualBlockHeight,
                              m_nBlockXSize, nComponents ) )
            {
                return true;
            }
        }
    }

    // Do we need to spread edge values right or down for a partial
    // JPEG encoded tile?  We do this to avoid edge artifacts.
    bool bNeedTileFill = false;
    if( m_nCompression == COMPRESSION_JPEG )
    {
        nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, m_nBlockXSize);
        nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, m_nBlockYSize);

        iColumn = (tile % m_nBlocksPerBand) % nBlocksPerRow;
        iRow = (tile % m_nBlocksPerBand) / nBlocksPerRow;

        // Is this a partial right edge tile?
        if( iRow == nBlocksPerRow - 1
            && nRasterXSize % m_nBlockXSize != 0 )
            bNeedTileFill = true;

        // Is this a partial bottom edge tile?
        if( iColumn == nBlocksPerColumn - 1
            && nRasterYSize % m_nBlockYSize != 0 )
            bNeedTileFill = true;
    }

    // If we need to fill out the tile, or if we want to prevent
    // TIFFWriteEncodedTile from altering the buffer as part of
    // byte swapping the data on write then we will need a temporary
    // working buffer.  If not, we can just do a direct write.
    const GPtrDiff_t cc = static_cast<GPtrDiff_t>(TIFFTileSize( m_hTIFF ));

    if( bPreserveDataBuffer
        && (TIFFIsByteSwapped(m_hTIFF) || bNeedTileFill || m_panMaskOffsetLsb) )
    {
        if( m_pabyTempWriteBuffer == nullptr )
        {
            m_pabyTempWriteBuffer = CPLMalloc(cc);
        }
        memcpy(m_pabyTempWriteBuffer, pabyData, cc);

        pabyData = static_cast<GByte *>( m_pabyTempWriteBuffer );
    }

    // Perform tile fill if needed.
    // TODO: we should also handle the case of nBitsPerSample == 12
    // but this is more involved.
    if( bNeedTileFill && m_nBitsPerSample == 8 )
    {
        const int nComponents =
            m_nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;

        CPLDebug( "GTiff", "Filling out jpeg edge tile on write." );

        const int nRightPixelsToFill =
            iColumn == nBlocksPerRow - 1 ?
            m_nBlockXSize * (iColumn + 1) - nRasterXSize :
            0;
        const int nBottomPixelsToFill =
            iRow == nBlocksPerColumn - 1 ?
            m_nBlockYSize * (iRow + 1) - nRasterYSize :
            0;

        // Fill out to the right.
        const int iSrcX = m_nBlockXSize - nRightPixelsToFill - 1;

        for( int iX = iSrcX + 1; iX < m_nBlockXSize; ++iX )
        {
            for( int iY = 0; iY < m_nBlockYSize; ++iY )
            {
                memcpy( pabyData + (static_cast<GPtrDiff_t>(m_nBlockXSize) * iY + iX) * nComponents,
                        pabyData + (static_cast<GPtrDiff_t>(m_nBlockXSize) * iY + iSrcX) * nComponents,
                        nComponents );
            }
        }

        // Now fill out the bottom.
        const int iSrcY = m_nBlockYSize - nBottomPixelsToFill - 1;
        for( int iY = iSrcY + 1; iY < m_nBlockYSize; ++iY )
        {
            memcpy( pabyData + static_cast<GPtrDiff_t>(m_nBlockXSize) * nComponents * iY,
                    pabyData + static_cast<GPtrDiff_t>(m_nBlockXSize) * nComponents * iSrcY,
                    static_cast<GPtrDiff_t>(m_nBlockXSize) * nComponents );
        }
    }

    if( m_panMaskOffsetLsb )
    {
        const int iBand =
            m_nPlanarConfig == PLANARCONFIG_SEPARATE ?
            static_cast<int>(tile) / m_nBlocksPerBand : -1;
        DiscardLsb(pabyData, cc, iBand);
    }

    if( m_bStreamingOut )
    {
        if( tile != static_cast<uint32_t>(m_nLastWrittenBlockId + 1) )
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                     "Attempt to write block %d whereas %d was expected",
                     tile, m_nLastWrittenBlockId + 1);
            return false;
        }
        if( static_cast<GPtrDiff_t>( VSIFWriteL(pabyData, 1, cc, m_fpToWrite) ) != cc )
        {
            ReportError( CE_Failure, CPLE_FileIO, "Could not write " CPL_FRMT_GUIB " bytes",
                      static_cast<GUIntBig>(cc) );
            return false;
        }
        m_nLastWrittenBlockId = tile;
        return true;
    }

/* -------------------------------------------------------------------- */
/*      Should we do compression in a worker thread ?                   */
/* -------------------------------------------------------------------- */
    if( SubmitCompressionJob(tile, pabyData, cc, m_nBlockYSize) )
        return true;

    // libtiff 4.0.6 or older do not always properly report write errors.
#if TIFFLIB_VERSION <= 20150912
    const CPLErr eBefore = CPLGetLastErrorType();
#endif
    const bool bRet =
        TIFFWriteEncodedTile(m_hTIFF, tile, pabyData, cc) == cc;
#if TIFFLIB_VERSION <= 20150912
    if( eBefore == CE_None && CPLGetLastErrorType() == CE_Failure )
        return false;
#endif
    return bRet;
}

/************************************************************************/
/*                        WriteEncodedStrip()                           */
/************************************************************************/

bool GTiffDataset::WriteEncodedStrip( uint32_t strip, GByte* pabyData,
                                      int bPreserveDataBuffer )
{
    GPtrDiff_t cc = static_cast<GPtrDiff_t>(TIFFStripSize( m_hTIFF ));
    const auto ccFull = cc;

/* -------------------------------------------------------------------- */
/*      If this is the last strip in the image, and is partial, then    */
/*      we need to trim the number of scanlines written to the          */
/*      amount of valid data we have. (#2748)                           */
/* -------------------------------------------------------------------- */
    const int nStripWithinBand = strip % m_nBlocksPerBand;
    int nStripHeight = m_nRowsPerStrip;

    if( nStripWithinBand * nStripHeight > GetRasterYSize() - nStripHeight )
    {
        nStripHeight = GetRasterYSize() - nStripWithinBand * m_nRowsPerStrip;
        cc = (cc / m_nRowsPerStrip) * nStripHeight;
        CPLDebug( "GTiff", "Adjusted bytes to write from " CPL_FRMT_GUIB " to " CPL_FRMT_GUIB ".",
                  static_cast<GUIntBig>(TIFFStripSize(m_hTIFF)),
                  static_cast<GUIntBig>(cc) );
    }

/* -------------------------------------------------------------------- */
/*      Don't write empty blocks in some cases.                         */
/* -------------------------------------------------------------------- */
    if( !m_bWriteEmptyTiles && IsFirstPixelEqualToNoData(pabyData) )
    {
        if( !IsBlockAvailable(strip) )
        {
            const int nComponents =
                m_nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;

            if( HasOnlyNoData(pabyData,
                              m_nBlockXSize, nStripHeight,
                              m_nBlockXSize, nComponents ) )
            {
                return true;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      TIFFWriteEncodedStrip can alter the passed buffer if            */
/*      byte-swapping is necessary so we use a temporary buffer         */
/*      before calling it.                                              */
/* -------------------------------------------------------------------- */
    if( bPreserveDataBuffer && (TIFFIsByteSwapped(m_hTIFF) || m_panMaskOffsetLsb) )
    {
        if( m_pabyTempWriteBuffer == nullptr )
        {
            m_pabyTempWriteBuffer = CPLMalloc(ccFull);
        }
        memcpy(m_pabyTempWriteBuffer, pabyData, cc);
        pabyData = static_cast<GByte *>( m_pabyTempWriteBuffer );
    }

    if( m_panMaskOffsetLsb )
    {
        int iBand =
            m_nPlanarConfig == PLANARCONFIG_SEPARATE ?
            static_cast<int>(strip) / m_nBlocksPerBand : -1;
        DiscardLsb(pabyData, cc, iBand);
    }

    if( m_bStreamingOut )
    {
        if( strip != static_cast<uint32_t>(m_nLastWrittenBlockId + 1) )
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                     "Attempt to write block %d whereas %d was expected",
                     strip, m_nLastWrittenBlockId + 1);
            return false;
        }
        if( static_cast<GPtrDiff_t>(VSIFWriteL(pabyData, 1, cc, m_fpToWrite)) != cc )
        {
            ReportError(CE_Failure, CPLE_FileIO, "Could not write " CPL_FRMT_GUIB " bytes",
                     static_cast<GUIntBig>(cc));
            return false;
        }
        m_nLastWrittenBlockId = strip;
        return true;
    }

/* -------------------------------------------------------------------- */
/*      Should we do compression in a worker thread ?                   */
/* -------------------------------------------------------------------- */
    if( SubmitCompressionJob(strip, pabyData, cc, nStripHeight) )
        return true;

    // libtiff 4.0.6 or older do not always properly report write errors.
#if TIFFLIB_VERSION <= 20150912
    CPLErr eBefore = CPLGetLastErrorType();
#endif
    bool bRet = TIFFWriteEncodedStrip( m_hTIFF, strip, pabyData, cc) == cc;
#if TIFFLIB_VERSION <= 20150912
    if( eBefore == CE_None && CPLGetLastErrorType() == CE_Failure )
        bRet = FALSE;
#endif
    return bRet;
}

/************************************************************************/
/*                        InitCompressionThreads()                      */
/************************************************************************/

void GTiffDataset::InitCompressionThreads( char** papszOptions )
{
    // Raster == tile, then no need for threads
    if( m_nBlockXSize == nRasterXSize && m_nBlockYSize == nRasterYSize )
        return;

    const char* pszValue = CSLFetchNameValue( papszOptions, "NUM_THREADS" );
    if( pszValue == nullptr )
        pszValue = CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
    if( pszValue )
    {
        int nThreads =
            EQUAL(pszValue, "ALL_CPUS") ? CPLGetNumCPUs() : atoi(pszValue);
        if( nThreads > 1024 )
            nThreads = 1024; // to please Coverity
        if( nThreads > 1 )
        {
            if( m_nCompression == COMPRESSION_NONE )
            {
                CPLDebug( "GTiff",
                          "NUM_THREADS ignored with uncompressed" );
            }
            else
            {
                CPLDebug("GTiff", "Using %d threads for compression", nThreads);

                auto poThreadPool = GDALGetGlobalThreadPool(nThreads);
                if( poThreadPool )
                    m_poCompressQueue = poThreadPool->CreateJobQueue();

                if( m_poCompressQueue != nullptr )
                {
                    // Add a margin of an extra job w.r.t thread number
                    // so as to optimize compression time (enables the main
                    // thread to do boring I/O while all CPUs are working).
                    m_asCompressionJobs.resize(nThreads + 1);
                    memset(&m_asCompressionJobs[0], 0,
                           m_asCompressionJobs.size() *
                           sizeof(GTiffCompressionJob));
                    for( int i = 0;
                         i < static_cast<int>(m_asCompressionJobs.size());
                         ++i )
                    {
                        m_asCompressionJobs[i].pszTmpFilename =
                            CPLStrdup(CPLSPrintf("/vsimem/gtiff/thread/job/%p",
                                                 &m_asCompressionJobs[i]));
                        m_asCompressionJobs[i].nStripOrTile = -1;
                    }
                    m_hCompressThreadPoolMutex = CPLCreateMutex();
                    CPLReleaseMutex(m_hCompressThreadPoolMutex);

                    // This is kind of a hack, but basically using
                    // TIFFWriteRawStrip/Tile and then TIFFReadEncodedStrip/Tile
                    // does not work on a newly created file, because
                    // TIFF_MYBUFFER is not set in tif_flags
                    // (if using TIFFWriteEncodedStrip/Tile first,
                    // TIFFWriteBufferSetup() is automatically called).
                    // This should likely rather fixed in libtiff itself.
                    CPL_IGNORE_RET_VAL(
                        TIFFWriteBufferSetup(m_hTIFF, nullptr, -1));
                }
            }
        }
        else if( nThreads < 0 ||
                 (!EQUAL(pszValue, "0") &&
                  !EQUAL(pszValue, "1") &&
                  !EQUAL(pszValue, "ALL_CPUS")) )
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                     "Invalid value for NUM_THREADS: %s", pszValue);
        }
    }
}

/************************************************************************/
/*                       GetGTIFFKeysFlavor()                           */
/************************************************************************/

static GTIFFKeysFlavorEnum GetGTIFFKeysFlavor( char** papszOptions )
{
    const char* pszGeoTIFFKeysFlavor =
        CSLFetchNameValueDef( papszOptions, "GEOTIFF_KEYS_FLAVOR", "STANDARD" );
    if( EQUAL(pszGeoTIFFKeysFlavor, "ESRI_PE") )
        return GEOTIFF_KEYS_ESRI_PE;
    return GEOTIFF_KEYS_STANDARD;
}

/************************************************************************/
/*                       GetGeoTIFFVersion()                            */
/************************************************************************/

static GeoTIFFVersionEnum GetGeoTIFFVersion( char** papszOptions )
{
    const char* pszVersion =
        CSLFetchNameValueDef( papszOptions, "GEOTIFF_VERSION", "AUTO" );
    if( EQUAL(pszVersion, "1.0") )
        return GEOTIFF_VERSION_1_0;
    if( EQUAL(pszVersion, "1.1") )
        return GEOTIFF_VERSION_1_1;
    return GEOTIFF_VERSION_AUTO;
}

/************************************************************************/
/*                      InitCreationOrOpenOptions()                     */
/************************************************************************/

void GTiffDataset::InitCreationOrOpenOptions( char** papszOptions )
{
    InitCompressionThreads(papszOptions);

    m_eGeoTIFFKeysFlavor = GetGTIFFKeysFlavor(papszOptions);
    m_eGeoTIFFVersion = GetGeoTIFFVersion(papszOptions);
}

/************************************************************************/
/*                      ThreadCompressionFunc()                         */
/************************************************************************/

void GTiffDataset::ThreadCompressionFunc( void* pData )
{
    GTiffCompressionJob* psJob = static_cast<GTiffCompressionJob *>(pData);
    GTiffDataset* poDS = psJob->poDS;

    VSILFILE* fpTmp = VSIFOpenL(psJob->pszTmpFilename, "wb+");
    TIFF* hTIFFTmp = VSI_TIFFOpen(psJob->pszTmpFilename,
        psJob->bTIFFIsBigEndian ? "wb+" : "wl+", fpTmp);
    CPLAssert( hTIFFTmp != nullptr );
    TIFFSetField(hTIFFTmp, TIFFTAG_IMAGEWIDTH, poDS->m_nBlockXSize);
    TIFFSetField(hTIFFTmp, TIFFTAG_IMAGELENGTH, psJob->nHeight);
    TIFFSetField(hTIFFTmp, TIFFTAG_BITSPERSAMPLE, poDS->m_nBitsPerSample);
    TIFFSetField(hTIFFTmp, TIFFTAG_COMPRESSION, poDS->m_nCompression);
    TIFFSetField(hTIFFTmp, TIFFTAG_PHOTOMETRIC, poDS->m_nPhotometric);
    TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLEFORMAT, poDS->m_nSampleFormat);
    TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLESPERPIXEL, poDS->m_nSamplesPerPixel);
    TIFFSetField(hTIFFTmp, TIFFTAG_ROWSPERSTRIP, poDS->m_nBlockYSize);
    TIFFSetField(hTIFFTmp, TIFFTAG_PLANARCONFIG, poDS->m_nPlanarConfig);
    if( psJob->nPredictor != PREDICTOR_NONE )
        TIFFSetField(hTIFFTmp, TIFFTAG_PREDICTOR, psJob->nPredictor);
    if( poDS->m_nCompression == COMPRESSION_LERC )
    {
        TIFFSetField(hTIFFTmp, TIFFTAG_LERC_PARAMETERS, 2,
                    poDS->m_anLercAddCompressionAndVersion);
    }

    TIFFSetField(hTIFFTmp, TIFFTAG_PHOTOMETRIC, poDS->m_nPhotometric);
    TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLEFORMAT, poDS->m_nSampleFormat);
    TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLESPERPIXEL, poDS->m_nSamplesPerPixel);
    TIFFSetField(hTIFFTmp, TIFFTAG_ROWSPERSTRIP, poDS->m_nBlockYSize);
    TIFFSetField(hTIFFTmp, TIFFTAG_PLANARCONFIG, poDS->m_nPlanarConfig);

    poDS->RestoreVolatileParameters(hTIFFTmp);

    bool bOK =
        TIFFWriteEncodedStrip(hTIFFTmp, 0, psJob->pabyBuffer,
                              psJob->nBufferSize) == psJob->nBufferSize;

    toff_t nOffset = 0;
    if( bOK )
    {
        toff_t* panOffsets = nullptr;
        toff_t* panByteCounts = nullptr;
        TIFFGetField(hTIFFTmp, TIFFTAG_STRIPOFFSETS, &panOffsets);
        TIFFGetField(hTIFFTmp, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts);

        nOffset = panOffsets[0];
        psJob->nCompressedBufferSize = static_cast<GPtrDiff_t>(panByteCounts[0]);
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error when compressing strip/tile %d",
                  psJob->nStripOrTile);
    }

    XTIFFClose(hTIFFTmp);
    if( VSIFCloseL(fpTmp) != 0 )
    {
        if( bOK )
        {
            bOK = false;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Error when compressing strip/tile %d",
                      psJob->nStripOrTile);
        }
    }

    if( bOK )
    {
        vsi_l_offset nFileSize = 0;
        GByte* pabyCompressedBuffer = VSIGetMemFileBuffer(psJob->pszTmpFilename,
                                                          &nFileSize,
                                                          FALSE);
        CPLAssert( static_cast<vsi_l_offset>(nOffset + psJob->nCompressedBufferSize) <= nFileSize );
        psJob->pabyCompressedBuffer = pabyCompressedBuffer + nOffset;
    }
    else
    {
        psJob->pabyCompressedBuffer = nullptr;
        psJob->nCompressedBufferSize = 0;
    }

    auto mutex = poDS->m_poBaseDS ?
        poDS->m_poBaseDS->m_hCompressThreadPoolMutex : poDS->m_hCompressThreadPoolMutex;
    if( mutex )
    {
        CPLAcquireMutex(mutex, 1000.0);
        psJob->bReady = true;
        CPLReleaseMutex(mutex);
    }
}

/************************************************************************/
/*                        WriteRawStripOrTile()                         */
/************************************************************************/

void GTiffDataset::WriteRawStripOrTile( int nStripOrTile,
                                        GByte* pabyCompressedBuffer,
                                        GPtrDiff_t nCompressedBufferSize )
{
#ifdef DEBUG_VERBOSE
    CPLDebug("GTIFF", "Writing raw strip/tile %d, size " CPL_FRMT_GUIB,
             nStripOrTile, static_cast<GUIntBig>(nCompressedBufferSize));
#endif
    toff_t *panOffsets = nullptr;
    toff_t* panByteCounts = nullptr;
    bool bWriteAtEnd = true;
    bool bWriteLeader = m_bLeaderSizeAsUInt4;
    bool bWriteTrailer = m_bTrailerRepeatedLast4BytesRepeated;
    if( TIFFGetField(
            m_hTIFF,
            TIFFIsTiled( m_hTIFF ) ?
            TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS, &panOffsets ) &&
            panOffsets != nullptr &&
            panOffsets[nStripOrTile] != 0 )
    {
        // Forces TIFFAppendStrip() to consider if the location of the tile/strip
        // can be reused or if the strile should be written at end of file.
        TIFFSetWriteOffset(m_hTIFF, 0);

        if( m_bBlockOrderRowMajor )
        {
            if( TIFFGetField(
                m_hTIFF,
                TIFFIsTiled( m_hTIFF ) ?
                TIFFTAG_TILEBYTECOUNTS : TIFFTAG_STRIPBYTECOUNTS, &panByteCounts ) &&
                panByteCounts != nullptr )
            {
                if( static_cast<GUIntBig>(nCompressedBufferSize) >
                        panByteCounts[nStripOrTile] )
                {
                    GTiffDataset* poRootDS = m_poBaseDS ? m_poBaseDS : this;
                    if( !poRootDS->m_bKnownIncompatibleEdition &&
                        !poRootDS->m_bWriteKnownIncompatibleEdition )
                    {
                        ReportError(CE_Warning, CPLE_AppDefined,
                            "A strile cannot be rewritten in place, which "
                            "invalidates the BLOCK_ORDER optimization.");
                        poRootDS->m_bKnownIncompatibleEdition = true;
                        poRootDS->m_bWriteKnownIncompatibleEdition = true;
                    }
                }
                // For mask interleaving, if the size is not exactly the same,
                // completely give up (we could potentially move the mask in
                // case the imagery is smaller)
                else if( m_poMaskDS && m_bMaskInterleavedWithImagery &&
                         static_cast<GUIntBig>(nCompressedBufferSize) !=
                            panByteCounts[nStripOrTile] )
                {
                    GTiffDataset* poRootDS = m_poBaseDS ? m_poBaseDS : this;
                    if( !poRootDS->m_bKnownIncompatibleEdition &&
                        !poRootDS->m_bWriteKnownIncompatibleEdition )
                    {
                        ReportError(CE_Warning, CPLE_AppDefined,
                            "A strile cannot be rewritten in place, which "
                            "invalidates the MASK_INTERLEAVED_WITH_IMAGERY "
                            "optimization.");
                        poRootDS->m_bKnownIncompatibleEdition = true;
                        poRootDS->m_bWriteKnownIncompatibleEdition = true;
                    }
                    bWriteLeader = false;
                    bWriteTrailer = false;
                    if( m_bLeaderSizeAsUInt4 )
                    {
                        // If there was a valid leader, invalidat it
                        VSI_TIFFSeek( m_hTIFF, panOffsets[nStripOrTile] - 4, SEEK_SET );
                        uint32_t nOldSize;
                        VSIFReadL(&nOldSize, 1, 4, VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF)));
                        CPL_LSBPTR32(&nOldSize);
                        if( nOldSize == panByteCounts[nStripOrTile] )
                        {
                            uint32_t nInvalidatedSize = 0;
                            VSI_TIFFSeek( m_hTIFF, panOffsets[nStripOrTile] - 4, SEEK_SET );
                            VSI_TIFFWrite(m_hTIFF, &nInvalidatedSize, sizeof(nInvalidatedSize));
                        }
                    }
                }
                else
                {
                    bWriteAtEnd = false;
                }
            }
        }
    }
    if( bWriteLeader &&
        static_cast<GUIntBig>(nCompressedBufferSize) <= 0xFFFFFFFFU )
    {
        // cppcheck-suppress knownConditionTrueFalse
        if( bWriteAtEnd )
        {
            VSI_TIFFSeek( m_hTIFF, 0, SEEK_END );
        }
        else
        {
            // If we rewrite an existing strile in place with an existing leader,
            // check that the leader is valid, before rewriting it.
            // And if it is not valid, then do not write the trailer, as we
            // could corrupt other data.
            VSI_TIFFSeek( m_hTIFF, panOffsets[nStripOrTile] - 4, SEEK_SET );
            uint32_t nOldSize;
            VSIFReadL(&nOldSize, 1, 4, VSI_TIFFGetVSILFile(TIFFClientdata(m_hTIFF)));
            CPL_LSBPTR32(&nOldSize);
            bWriteLeader = panByteCounts && nOldSize == panByteCounts[nStripOrTile];
            bWriteTrailer = bWriteLeader;
            VSI_TIFFSeek( m_hTIFF, panOffsets[nStripOrTile] - 4, SEEK_SET );
        }
        // cppcheck-suppress knownConditionTrueFalse
        if( bWriteLeader )
        {
            uint32_t nSize = static_cast<uint32_t>(nCompressedBufferSize);
            CPL_LSBPTR32(&nSize);
            if( !VSI_TIFFWrite(m_hTIFF, &nSize, sizeof(nSize)) )
                m_bWriteError = true;
        }
    }
    tmsize_t written;
    if( TIFFIsTiled( m_hTIFF ) )
        written = TIFFWriteRawTile( m_hTIFF, nStripOrTile, pabyCompressedBuffer,
                          nCompressedBufferSize );
    else
        written = TIFFWriteRawStrip( m_hTIFF, nStripOrTile, pabyCompressedBuffer,
                           nCompressedBufferSize );
    if( written != nCompressedBufferSize )
        m_bWriteError = true;
    if( bWriteTrailer &&
        static_cast<GUIntBig>(nCompressedBufferSize) <= 0xFFFFFFFFU )
    {
        GByte abyLastBytes[4] = {};
        if( nCompressedBufferSize >= 4 )
            memcpy(abyLastBytes, pabyCompressedBuffer + nCompressedBufferSize - 4, 4);
        else
            memcpy(abyLastBytes, pabyCompressedBuffer, nCompressedBufferSize);
        if( !VSI_TIFFWrite(m_hTIFF, abyLastBytes, 4) )
            m_bWriteError = true;
    }
}

/************************************************************************/
/*                        WaitCompletionForJobIdx()                     */
/************************************************************************/

void GTiffDataset::WaitCompletionForJobIdx(int i)
{
    auto poQueue = m_poBaseDS ?
        m_poBaseDS->m_poCompressQueue.get() : m_poCompressQueue.get();
    auto& oQueue = m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
    auto& asJobs = m_poBaseDS ? m_poBaseDS->m_asCompressionJobs : m_asCompressionJobs;
    auto mutex = m_poBaseDS ? m_poBaseDS->m_hCompressThreadPoolMutex : m_hCompressThreadPoolMutex;

    CPLAssert( i >= 0 && static_cast<size_t>(i) < asJobs.size() );
    CPLAssert( asJobs[i].nStripOrTile >= 0 );
    CPLAssert( !oQueue.empty() );

    bool bHasWarned = false;
    while( true )
    {
        CPLAcquireMutex(mutex, 1000.0);
        const bool bReady = asJobs[i].bReady;
        CPLReleaseMutex(mutex);
        if( !bReady )
        {
            if( !bHasWarned )
            {
                CPLDebug("GTIFF",
                        "Waiting for worker job to finish handling block %d",
                        asJobs[i].nStripOrTile);
                bHasWarned = true;
            }
            poQueue->GetPool()->WaitEvent();
        }
        else
        {
            break;
        }
    }

    if( asJobs[i].nCompressedBufferSize )
    {
        asJobs[i].poDS->WriteRawStripOrTile(asJobs[i].nStripOrTile,
                        asJobs[i].pabyCompressedBuffer,
                        asJobs[i].nCompressedBufferSize);
    }
    asJobs[i].pabyCompressedBuffer = nullptr;
    asJobs[i].nBufferSize = 0;
    asJobs[i].bReady = false;
    asJobs[i].nStripOrTile = -1;
    oQueue.pop();
}

/************************************************************************/
/*                        WaitCompletionForBlock()                      */
/************************************************************************/

void GTiffDataset::WaitCompletionForBlock(int nBlockId)
{
    auto poQueue = m_poBaseDS ?
        m_poBaseDS->m_poCompressQueue.get() : m_poCompressQueue.get();
    auto& oQueue = m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
    auto& asJobs = m_poBaseDS ? m_poBaseDS->m_asCompressionJobs : m_asCompressionJobs;

    if( poQueue != nullptr )
    {
        for( int i = 0; i < static_cast<int>(asJobs.size()); ++i )
        {
            if( asJobs[i].poDS == this && asJobs[i].nStripOrTile == nBlockId )
            {
                while( !oQueue.empty() &&
                       !(asJobs[oQueue.front()].poDS == this &&
                         asJobs[oQueue.front()].nStripOrTile == nBlockId) )
                {
                    WaitCompletionForJobIdx(oQueue.front());
                }
                CPLAssert( !oQueue.empty() &&
                          asJobs[oQueue.front()].poDS == this &&
                          asJobs[oQueue.front()].nStripOrTile == nBlockId );
                WaitCompletionForJobIdx(oQueue.front());
            }
        }
    }
}

/************************************************************************/
/*                      SubmitCompressionJob()                          */
/************************************************************************/

bool GTiffDataset::SubmitCompressionJob( int nStripOrTile, GByte* pabyData,
                                         GPtrDiff_t cc, int nHeight )
{
/* -------------------------------------------------------------------- */
/*      Should we do compression in a worker thread ?                   */
/* -------------------------------------------------------------------- */
    auto poQueue = m_poBaseDS ?
        m_poBaseDS->m_poCompressQueue.get() : m_poCompressQueue.get();

    if( poQueue == nullptr ||
          !(m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
            m_nCompression == COMPRESSION_LZW ||
            m_nCompression == COMPRESSION_PACKBITS ||
            m_nCompression == COMPRESSION_LZMA ||
            m_nCompression == COMPRESSION_ZSTD ||
            m_nCompression == COMPRESSION_LERC ||
            m_nCompression == COMPRESSION_WEBP ||
            m_nCompression == COMPRESSION_JPEG) )
    {
        if( m_bBlockOrderRowMajor || m_bLeaderSizeAsUInt4 ||
            m_bTrailerRepeatedLast4BytesRepeated )
        {
            GTiffCompressionJob sJob;
            memset(&sJob, 0, sizeof(sJob));
            sJob.poDS = this;
            sJob.pszTmpFilename = CPLStrdup(CPLSPrintf("/vsimem/gtiff/%p", this));
            sJob.bTIFFIsBigEndian = CPL_TO_BOOL( TIFFIsBigEndian(m_hTIFF) );
            sJob.pabyBuffer =
                static_cast<GByte*>( CPLRealloc(sJob.pabyBuffer, cc) );
            memcpy(sJob.pabyBuffer, pabyData, cc);
            sJob.nBufferSize = cc;
            sJob.nHeight = nHeight;
            sJob.nStripOrTile = nStripOrTile;
            sJob.nPredictor = PREDICTOR_NONE;
            if( m_nCompression == COMPRESSION_LZW ||
                m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
                m_nCompression == COMPRESSION_ZSTD )
            {
                TIFFGetField( m_hTIFF, TIFFTAG_PREDICTOR, &sJob.nPredictor );
            }

            ThreadCompressionFunc(&sJob);

            if( sJob.nCompressedBufferSize )
            {
                sJob.poDS->
                    WriteRawStripOrTile(sJob.nStripOrTile,
                                sJob.pabyCompressedBuffer,
                                sJob.nCompressedBufferSize);
            }

            CPLFree(sJob.pabyBuffer);
            VSIUnlink(sJob.pszTmpFilename);
            CPLFree(sJob.pszTmpFilename);
            return sJob.nCompressedBufferSize > 0 && !m_bWriteError;
        }

        return false;
    }

    auto& oQueue = m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
    auto& asJobs = m_poBaseDS ? m_poBaseDS->m_asCompressionJobs : m_asCompressionJobs;

    int nNextCompressionJobAvail = -1;

    if( oQueue.size() == asJobs.size() )
    {
        CPLAssert( !oQueue.empty() );
        nNextCompressionJobAvail = oQueue.front();
        WaitCompletionForJobIdx(nNextCompressionJobAvail);
    }
    else
    {
        const int nJobs = static_cast<int>(asJobs.size());
        for( int i = 0; i < nJobs; ++i )
        {
            if( asJobs[i].nBufferSize == 0 )
            {
                nNextCompressionJobAvail = i;
                break;
            }
        }
    }
    CPLAssert(nNextCompressionJobAvail >= 0);

    GTiffCompressionJob* psJob = &asJobs[nNextCompressionJobAvail];
    psJob->poDS = this;
    psJob->bTIFFIsBigEndian = CPL_TO_BOOL( TIFFIsBigEndian(m_hTIFF) );
    psJob->pabyBuffer =
        static_cast<GByte*>( CPLRealloc(psJob->pabyBuffer, cc) );
    memcpy(psJob->pabyBuffer, pabyData, cc);
    psJob->nBufferSize = cc;
    psJob->nHeight = nHeight;
    psJob->nStripOrTile = nStripOrTile;
    psJob->nPredictor = PREDICTOR_NONE;
    if( m_nCompression == COMPRESSION_LZW ||
        m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        m_nCompression == COMPRESSION_ZSTD )
    {
        TIFFGetField( m_hTIFF, TIFFTAG_PREDICTOR, &psJob->nPredictor );
    }

    poQueue->SubmitJob(ThreadCompressionFunc, psJob);
    oQueue.push(nNextCompressionJobAvail);

    return true;
}

/************************************************************************/
/*                          DiscardLsb()                                */
/************************************************************************/

template<class T> static void DiscardLsbT(GByte* pabyBuffer,
                                         size_t nBytes,
                                         int iBand,
                                         int nBands,
                                         uint16_t nPlanarConfig,
                                         const GTiffDataset::MaskOffset* panMaskOffsetLsb)
{
    if( nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        const int nMask = panMaskOffsetLsb[iBand].nMask;
        const int nOffset = panMaskOffsetLsb[iBand].nOffset;
        for( size_t i = 0; i < nBytes/sizeof(T); ++i )
        {
            reinterpret_cast<T*>(pabyBuffer)[i] =
                static_cast<T>(
                    (reinterpret_cast<T *>(pabyBuffer)[i] & nMask) |
                    nOffset);
        }
    }
    else
    {
        for( size_t i = 0; i < nBytes/sizeof(T); i += nBands )
        {
            for( int j = 0; j < nBands; ++j )
            {
                reinterpret_cast<T*>(pabyBuffer)[i + j] =
                    static_cast<T>(
                        (reinterpret_cast<T*>(pabyBuffer)[i + j] &
                            panMaskOffsetLsb[j].nMask) |
                        panMaskOffsetLsb[j].nOffset);
            }
        }
    }
}

void GTiffDataset::DiscardLsb( GByte* pabyBuffer, GPtrDiff_t nBytes, int iBand ) const
{
    if( m_nBitsPerSample == 8 )
    {
        if( m_nPlanarConfig == PLANARCONFIG_SEPARATE )
        {
            const int nMask = m_panMaskOffsetLsb[iBand].nMask;
            const int nOffset = m_panMaskOffsetLsb[iBand].nOffset;
            for( decltype(nBytes) i = 0; i < nBytes; ++i )
            {
                // Keep 255 in case it is alpha.
                if( pabyBuffer[i] != 255 )
                    pabyBuffer[i] =
                        static_cast<GByte>((pabyBuffer[i] & nMask) | nOffset);
            }
        }
        else
        {
            for( decltype(nBytes) i = 0; i < nBytes; i += nBands )
            {
                for( int j = 0; j < nBands; ++j )
                {
                    // Keep 255 in case it is alpha.
                    if( pabyBuffer[i + j] != 255 )
                        pabyBuffer[i + j] =
                            static_cast<GByte>((pabyBuffer[i + j] &
                                                m_panMaskOffsetLsb[j].nMask) | m_panMaskOffsetLsb[j].nOffset);
                }
            }
        }
    }
    else if( m_nBitsPerSample == 16 )
    {
        DiscardLsbT<GUInt16>(pabyBuffer, nBytes, iBand, nBands, m_nPlanarConfig,
                            m_panMaskOffsetLsb);
    }
    else if( m_nBitsPerSample == 32 )
    {
        DiscardLsbT<GUInt32>(pabyBuffer, nBytes, iBand, nBands, m_nPlanarConfig,
                            m_panMaskOffsetLsb);
    }
}

/************************************************************************/
/*                  WriteEncodedTileOrStrip()                           */
/************************************************************************/

CPLErr GTiffDataset::WriteEncodedTileOrStrip( uint32_t tile_or_strip, void* data,
                                              int bPreserveDataBuffer )
{
    CPLErr eErr = CE_None;

    if( TIFFIsTiled( m_hTIFF ) )
    {
        if( !(WriteEncodedTile(
               tile_or_strip,
               static_cast<GByte *>(data),
               bPreserveDataBuffer)) )
        {
            eErr = CE_Failure;
        }
    }
    else
    {
        if( !(WriteEncodedStrip(
               tile_or_strip,
               static_cast<GByte *>(data),
               bPreserveDataBuffer)) )
        {
            eErr = CE_Failure;
        }
    }

    return eErr;
}

/************************************************************************/
/*                           FlushBlockBuf()                            */
/************************************************************************/

CPLErr GTiffDataset::FlushBlockBuf()

{
    if( m_nLoadedBlock < 0 || !m_bLoadedBlockDirty )
        return CE_None;

    m_bLoadedBlockDirty = false;

    const CPLErr eErr =
        WriteEncodedTileOrStrip(m_nLoadedBlock, m_pabyBlockBuf, true);
    if( eErr != CE_None )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                    "WriteEncodedTile/Strip() failed." );
        m_bWriteError = true;
    }

    return eErr;
}

/************************************************************************/
/*                            LoadBlockBuf()                            */
/*                                                                      */
/*      Load working block buffer with request block (tile/strip).      */
/************************************************************************/

CPLErr GTiffDataset::LoadBlockBuf( int nBlockId, bool bReadFromDisk )

{
    if( m_nLoadedBlock == nBlockId && m_pabyBlockBuf != nullptr )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      If we have a dirty loaded block, flush it out first.            */
/* -------------------------------------------------------------------- */
    if( m_nLoadedBlock != -1 && m_bLoadedBlockDirty )
    {
        const CPLErr eErr = FlushBlockBuf();
        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Get block size.                                                 */
/* -------------------------------------------------------------------- */
    const GPtrDiff_t nBlockBufSize =
        static_cast<GPtrDiff_t>(
            TIFFIsTiled(m_hTIFF) ? TIFFTileSize(m_hTIFF) : TIFFStripSize(m_hTIFF));
    if( !nBlockBufSize )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                  "Bogus block size; unable to allocate a buffer." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( m_pabyBlockBuf == nullptr )
    {
        m_pabyBlockBuf =
            static_cast<GByte *>( VSI_CALLOC_VERBOSE( 1, nBlockBufSize ) );
        if( m_pabyBlockBuf == nullptr )
        {
            return CE_Failure;
        }
    }

    if( m_nLoadedBlock == nBlockId )
        return CE_None;

/* -------------------------------------------------------------------- */
/*  When called from ::IWriteBlock in separate cases (or in single band */
/*  geotiffs), the ::IWriteBlock will override the content of the buffer*/
/*  with pImage, so we don't need to read data from disk                */
/* -------------------------------------------------------------------- */
    if( !bReadFromDisk || m_bStreamingOut )
    {
        m_nLoadedBlock = nBlockId;
        return CE_None;
    }

    // libtiff 3.X doesn't like mixing read&write of JPEG compressed blocks
    // The below hack is necessary due to another hack that consist in
    // writing zero block to force creation of JPEG tables.
    if( nBlockId == 0 && m_bDontReloadFirstBlock )
    {
        m_bDontReloadFirstBlock = false;
        memset( m_pabyBlockBuf, 0, nBlockBufSize );
        m_nLoadedBlock = nBlockId;
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      The bottom most partial tiles and strips are sometimes only     */
/*      partially encoded.  This code reduces the requested data so     */
/*      an error won't be reported in this case. (#1179)                */
/* -------------------------------------------------------------------- */
    auto nBlockReqSize = nBlockBufSize;
    const int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, m_nBlockXSize);
    const int nBlockYOff = (nBlockId % m_nBlocksPerBand) / nBlocksPerRow;

    if( nBlockYOff * m_nBlockYSize > nRasterYSize - m_nBlockYSize )
    {
        nBlockReqSize = (nBlockBufSize / m_nBlockYSize)
            * (m_nBlockYSize - static_cast<int>(
                (static_cast<GIntBig>(nBlockYOff + 1) * m_nBlockYSize) %
                    nRasterYSize));
        memset( m_pabyBlockBuf, 0, nBlockBufSize );
    }

/* -------------------------------------------------------------------- */
/*      If we don't have this block already loaded, and we know it      */
/*      doesn't yet exist on disk, just zero the memory buffer and      */
/*      pretend we loaded it.                                           */
/* -------------------------------------------------------------------- */
    bool bErrOccurred = false;
    if( !IsBlockAvailable( nBlockId, nullptr, nullptr, &bErrOccurred ) )
    {
        memset( m_pabyBlockBuf, 0, nBlockBufSize );
        m_nLoadedBlock = nBlockId;
        if( bErrOccurred )
            return CE_Failure;
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Load the block, if it isn't our current block.                  */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( !ReadStrile(nBlockId, m_pabyBlockBuf, nBlockReqSize) )
    {
        memset( m_pabyBlockBuf, 0, nBlockBufSize );
        eErr = CE_Failure;
    }

    if( eErr == CE_None )
    {
        m_nLoadedBlock = nBlockId;
    }
    else
    {
        m_nLoadedBlock = -1;
    }
    m_bLoadedBlockDirty = false;

    return eErr;
}

/************************************************************************/
/*                   GTiffFillStreamableOffsetAndCount()                */
/************************************************************************/

static void GTiffFillStreamableOffsetAndCount( TIFF* hTIFF, int nSize )
{
    uint32_t nXSize = 0;
    uint32_t nYSize = 0;
    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
    const bool bIsTiled = CPL_TO_BOOL( TIFFIsTiled(hTIFF) );
    const int nBlockCount =
        bIsTiled ? TIFFNumberOfTiles(hTIFF) : TIFFNumberOfStrips(hTIFF);

    toff_t *panOffset = nullptr;
    TIFFGetField( hTIFF, bIsTiled ? TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS,
                  &panOffset );
    toff_t *panSize = nullptr;
    TIFFGetField( hTIFF,
                  bIsTiled ? TIFFTAG_TILEBYTECOUNTS : TIFFTAG_STRIPBYTECOUNTS,
                  &panSize );
    toff_t nOffset = nSize;
    // Trick to avoid clang static analyzer raising false positive about
    // divide by zero later.
    int nBlocksPerBand = 1;
    uint32_t nRowsPerStrip = 0;
    if( !bIsTiled )
    {
        TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP, &nRowsPerStrip);
        if( nRowsPerStrip > static_cast<uint32_t>(nYSize) )
            nRowsPerStrip = nYSize;
        nBlocksPerBand = DIV_ROUND_UP(nYSize, nRowsPerStrip);
    }
    for( int i = 0; i < nBlockCount; ++i )
    {
        GPtrDiff_t cc = bIsTiled ? static_cast<GPtrDiff_t>(TIFFTileSize(hTIFF)) :
                            static_cast<GPtrDiff_t>(TIFFStripSize(hTIFF));
        if( !bIsTiled )
        {
/* -------------------------------------------------------------------- */
/*      If this is the last strip in the image, and is partial, then    */
/*      we need to trim the number of scanlines written to the          */
/*      amount of valid data we have. (#2748)                           */
/* -------------------------------------------------------------------- */
            int nStripWithinBand = i % nBlocksPerBand;
            if( nStripWithinBand * nRowsPerStrip > nYSize - nRowsPerStrip )
            {
                cc = (cc / nRowsPerStrip)
                    * (nYSize - nStripWithinBand * nRowsPerStrip);
            }
        }
        panOffset[i] = nOffset;
        panSize[i] = cc;
        nOffset += cc;
    }
}

/************************************************************************/
/*                             Crystalize()                             */
/*                                                                      */
/*      Make sure that the directory information is written out for     */
/*      a new file, require before writing any imagery data.            */
/************************************************************************/

void GTiffDataset::Crystalize()

{
    if( m_bCrystalized )
        return;

    // TODO: libtiff writes extended tags in the order they are specified
    // and not in increasing order.
    WriteMetadata( this, m_hTIFF, true, m_eProfile, m_pszFilename,
                   m_papszCreationOptions );
    WriteGeoTIFFInfo();
    if( m_bNoDataSet )
        WriteNoDataValue( m_hTIFF, m_dfNoDataValue );

    m_bMetadataChanged = false;
    m_bGeoTIFFInfoChanged = false;
    m_bNoDataChanged = false;
    m_bNeedsRewrite = false;

    m_bCrystalized = true;

    TIFFWriteCheck( m_hTIFF, TIFFIsTiled(m_hTIFF), "GTiffDataset::Crystalize");

    TIFFWriteDirectory( m_hTIFF );
    if( m_bStreamingOut )
    {
        // We need to write twice the directory to be sure that custom
        // TIFF tags are correctly sorted and that padding bytes have been
        // added.
        TIFFSetDirectory( m_hTIFF, 0 );
        TIFFWriteDirectory( m_hTIFF );

        if( VSIFSeekL( m_fpL, 0, SEEK_END ) != 0 )
        {
            ReportError(CE_Failure, CPLE_FileIO, "Could not seek");
        }
        const int nSize = static_cast<int>( VSIFTellL(m_fpL) );

        TIFFSetDirectory( m_hTIFF, 0 );
        GTiffFillStreamableOffsetAndCount( m_hTIFF, nSize );
        TIFFWriteDirectory( m_hTIFF );

        vsi_l_offset nDataLength = 0;
        void* pabyBuffer =
            VSIGetMemFileBuffer( m_pszTmpFilename, &nDataLength, FALSE);
        if( static_cast<int>(
                VSIFWriteL( pabyBuffer, 1,
                            static_cast<int>(nDataLength), m_fpToWrite ) ) !=
            static_cast<int>(nDataLength) )
        {
            ReportError( CE_Failure, CPLE_FileIO, "Could not write %d bytes",
                      static_cast<int>(nDataLength) );
        }
        // In case of single strip file, there's a libtiff check that would
        // issue a warning since the file hasn't the required size.
        CPLPushErrorHandler(CPLQuietErrorHandler);
        TIFFSetDirectory( m_hTIFF, 0 );
        CPLPopErrorHandler();
    }
    else
    {
        TIFFSetDirectory( m_hTIFF,
                      static_cast<tdir_t>(TIFFNumberOfDirectories(m_hTIFF) - 1) );
    }

    RestoreVolatileParameters( m_hTIFF );

    m_nDirOffset = TIFFCurrentDirOffset( m_hTIFF );
}

/************************************************************************/
/*                          IsBlockAvailable()                          */
/*                                                                      */
/*      Return true if the indicated strip/tile is available.  We       */
/*      establish this by testing if the stripbytecount is zero.  If    */
/*      zero then the block has never been committed to disk.           */
/************************************************************************/

bool GTiffDataset::IsBlockAvailable( int nBlockId,
                                     vsi_l_offset* pnOffset,
                                     vsi_l_offset* pnSize,
                                     bool *pbErrOccurred )

{
    if( pbErrOccurred )
        *pbErrOccurred = false;

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
    std::pair<vsi_l_offset, vsi_l_offset> oPair;
    if( m_oCacheStrileToOffsetByteCount.tryGet(nBlockId, oPair) )
    {
        if( pnOffset )
            *pnOffset = oPair.first;
        if( pnSize )
            *pnSize = oPair.second;
        return oPair.first != 0;
    }
#endif

    WaitCompletionForBlock(nBlockId);

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
    // Optimization to avoid fetching the whole Strip/TileCounts and
    // Strip/TileOffsets arrays.
    if( eAccess == GA_ReadOnly && !m_bStreamingIn )
    {
        int nErrOccurred = 0;
        auto bytecount = TIFFGetStrileByteCountWithErr(m_hTIFF, nBlockId, &nErrOccurred);
        if( nErrOccurred && pbErrOccurred )
            *pbErrOccurred = true;
        if( pnOffset )
        {
            *pnOffset = TIFFGetStrileOffsetWithErr(m_hTIFF, nBlockId, &nErrOccurred);
            if( nErrOccurred && pbErrOccurred )
                *pbErrOccurred = true;
        }
        if( pnSize )
            *pnSize = bytecount;
        return bytecount != 0;
    }
#endif

    toff_t *panByteCounts = nullptr;
    toff_t *panOffsets = nullptr;
    const bool bIsTiled = CPL_TO_BOOL( TIFFIsTiled(m_hTIFF) );

    if( ( bIsTiled
          && TIFFGetField( m_hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts )
          && (pnOffset == nullptr ||
              TIFFGetField( m_hTIFF, TIFFTAG_TILEOFFSETS, &panOffsets )) )
        || ( !bIsTiled
          && TIFFGetField( m_hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts )
          && (pnOffset == nullptr ||
              TIFFGetField( m_hTIFF, TIFFTAG_STRIPOFFSETS, &panOffsets )) ) )
    {
        if( panByteCounts == nullptr || (pnOffset != nullptr && panOffsets == nullptr) )
        {
            if( pbErrOccurred )
                *pbErrOccurred = true;
            return false;
        }
        const int nBlockCount =
            bIsTiled ? TIFFNumberOfTiles(m_hTIFF) : TIFFNumberOfStrips(m_hTIFF);
        if( nBlockId >= nBlockCount )
        {
            if( pbErrOccurred )
                *pbErrOccurred = true;
            return false;
        }

        if( pnOffset )
            *pnOffset = panOffsets[nBlockId];
        if( pnSize )
            *pnSize = panByteCounts[nBlockId];
        return panByteCounts[nBlockId] != 0;
    }
    else
    {
        if( pbErrOccurred )
            *pbErrOccurred = true;
    }

    return false;
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we can also flush out local tiff strip      */
/*      cache if need be.                                               */
/************************************************************************/

void GTiffDataset::FlushCache()

{
    FlushCacheInternal( true );
}

void GTiffDataset::FlushCacheInternal( bool bFlushDirectory )
{
    if( m_bIsFinalized )
        return;

    GDALPamDataset::FlushCache();

    if( m_bLoadedBlockDirty && m_nLoadedBlock != -1 )
        FlushBlockBuf();

    CPLFree( m_pabyBlockBuf );
    m_pabyBlockBuf = nullptr;
    m_nLoadedBlock = -1;
    m_bLoadedBlockDirty = false;

    // Finish compression
    auto poQueue = m_poBaseDS ? m_poBaseDS->m_poCompressQueue.get() : m_poCompressQueue.get();
    if( poQueue )
    {
        poQueue->WaitCompletion();

        // Flush remaining data
        auto& oQueue = m_poBaseDS ? m_poBaseDS->m_asQueueJobIdx : m_asQueueJobIdx;
        while( !oQueue.empty() )
        {
            WaitCompletionForJobIdx(oQueue.front());
        }
    }

    if( bFlushDirectory && GetAccess() == GA_Update )
    {
        FlushDirectory();
    }
}

/************************************************************************/
/*                           FlushDirectory()                           */
/************************************************************************/

void GTiffDataset::FlushDirectory()

{
    if( GetAccess() == GA_Update )
    {
        if( m_bMetadataChanged )
        {
            m_bNeedsRewrite =
                    WriteMetadata( this, m_hTIFF, true, m_eProfile, m_pszFilename,
                                   m_papszCreationOptions );
            m_bMetadataChanged = false;

            if( m_bForceUnsetRPC )
            {
                double *padfRPCTag = nullptr;
                uint16_t nCount;
                if( TIFFGetField( m_hTIFF, TIFFTAG_RPCCOEFFICIENT, &nCount, &padfRPCTag ) )
                {
                    std::vector<double> zeroes(92);
                    TIFFSetField( m_hTIFF, TIFFTAG_RPCCOEFFICIENT, 92, zeroes.data() );
                    TIFFUnsetField( m_hTIFF, TIFFTAG_RPCCOEFFICIENT );
                    m_bNeedsRewrite = true;
                }

                GDALWriteRPCTXTFile( m_pszFilename, nullptr );
                GDALWriteRPBFile( m_pszFilename, nullptr );
            }
        }

        if( m_bGeoTIFFInfoChanged )
        {
            WriteGeoTIFFInfo();
            m_bGeoTIFFInfoChanged = false;
        }

        if( m_bNoDataChanged )
        {
            if( m_bNoDataSet )
            {
                WriteNoDataValue( m_hTIFF, m_dfNoDataValue );
            }
            else
            {
                UnsetNoDataValue( m_hTIFF );
            }
            m_bNeedsRewrite = true;
            m_bNoDataChanged = false;
        }

        if( m_bNeedsRewrite )
        {
            if( !m_bCrystalized)
            {
                Crystalize();
            }
            else
            {
                const TIFFSizeProc pfnSizeProc = TIFFGetSizeProc( m_hTIFF );

                m_nDirOffset = pfnSizeProc( TIFFClientdata( m_hTIFF ) );
                if( (m_nDirOffset % 2) == 1 )
                    ++m_nDirOffset;

                TIFFRewriteDirectory( m_hTIFF );

                TIFFSetSubDirectory( m_hTIFF, m_nDirOffset );

                if( m_bLayoutIFDSBeforeData &&
                    m_bBlockOrderRowMajor &&
                    m_bLeaderSizeAsUInt4 &&
                    m_bTrailerRepeatedLast4BytesRepeated &&
                    !m_bKnownIncompatibleEdition &&
                    !m_bWriteKnownIncompatibleEdition )
                {
                    ReportError(CE_Warning, CPLE_AppDefined,
                                "The IFD has been rewritten at the end of "
                                "the file, which breaks COG layout.");
                    m_bKnownIncompatibleEdition = true;
                    m_bWriteKnownIncompatibleEdition = true;
                }
            }
            m_bNeedsRewrite = false;
        }
    }

    // There are some circumstances in which we can reach this point
    // without having made this our directory (SetDirectory()) in which
    // case we should not risk a flush.
    if( GetAccess() == GA_Update && TIFFCurrentDirOffset(m_hTIFF) == m_nDirOffset )
    {
        const TIFFSizeProc pfnSizeProc = TIFFGetSizeProc( m_hTIFF );

        toff_t nNewDirOffset = pfnSizeProc( TIFFClientdata( m_hTIFF ) );
        if( (nNewDirOffset % 2) == 1 )
            ++nNewDirOffset;

        TIFFFlush( m_hTIFF );

        if( m_nDirOffset != TIFFCurrentDirOffset( m_hTIFF ) )
        {
            m_nDirOffset = nNewDirOffset;
            CPLDebug( "GTiff",
                      "directory moved during flush in FlushDirectory()" );
        }
    }

    SetDirectory();
}

/************************************************************************/
/*                           CleanOverviews()                           */
/************************************************************************/

CPLErr GTiffDataset::CleanOverviews()

{
    CPLAssert( !m_poBaseDS );

    ScanDirectories();

    FlushDirectory();

/* -------------------------------------------------------------------- */
/*      Cleanup overviews objects, and get offsets to all overview      */
/*      directories.                                                    */
/* -------------------------------------------------------------------- */
    std::vector<toff_t> anOvDirOffsets;

    for( int i = 0; i < m_nOverviewCount; ++i )
    {
        anOvDirOffsets.push_back( m_papoOverviewDS[i]->m_nDirOffset );
        delete m_papoOverviewDS[i];
    }

/* -------------------------------------------------------------------- */
/*      Loop through all the directories, translating the offsets       */
/*      into indexes we can use with TIFFUnlinkDirectory().             */
/* -------------------------------------------------------------------- */
    std::vector<uint16_t> anOvDirIndexes;
    int iThisOffset = 1;

    TIFFSetDirectory( m_hTIFF, 0 );

    while( true )
    {
        for( int i = 0; i < m_nOverviewCount; ++i )
        {
            if( anOvDirOffsets[i] == TIFFCurrentDirOffset( m_hTIFF ) )
            {
                CPLDebug( "GTiff", "%d -> %d",
                          static_cast<int>(anOvDirOffsets[i]), iThisOffset );
                anOvDirIndexes.push_back( static_cast<uint16_t>(iThisOffset) );
            }
        }

        if( TIFFLastDirectory( m_hTIFF ) )
            break;

        TIFFReadDirectory( m_hTIFF );
        ++iThisOffset;
    }

/* -------------------------------------------------------------------- */
/*      Actually unlink the target directories.  Note that we do        */
/*      this from last to first so as to avoid renumbering any of       */
/*      the earlier directories we need to remove.                      */
/* -------------------------------------------------------------------- */
    while( !anOvDirIndexes.empty() )
    {
        TIFFUnlinkDirectory( m_hTIFF, anOvDirIndexes.back() );
        anOvDirIndexes.pop_back();
    }

    CPLFree( m_papoOverviewDS );

    m_nOverviewCount = 0;
    m_papoOverviewDS = nullptr;

    if( !SetDirectory() )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                   RegisterNewOverviewDataset()                       */
/************************************************************************/

CPLErr GTiffDataset::RegisterNewOverviewDataset(toff_t nOverviewOffset,
                                                int l_nJpegQuality,
                                                int l_nWebPLevel)
{
    if( m_nOverviewCount == 127 )
        return CE_Failure;

    GTiffDataset* poODS = new GTiffDataset();
    poODS->ShareLockWithParentDataset(this);
    poODS->m_pszFilename = CPLStrdup(m_pszFilename);
    poODS->m_nJpegQuality = static_cast<signed char>(l_nJpegQuality);
    poODS->m_nWebPLevel = static_cast<signed char>(l_nWebPLevel);
    poODS->m_nZLevel = m_nZLevel;
    poODS->m_nLZMAPreset = m_nLZMAPreset;
    poODS->m_nZSTDLevel = m_nZSTDLevel;
    poODS->m_bWebPLossless = m_bWebPLossless;
    poODS->m_nJpegTablesMode = m_nJpegTablesMode;
    poODS->m_dfMaxZError = m_dfMaxZError;
    memcpy(poODS->m_anLercAddCompressionAndVersion, m_anLercAddCompressionAndVersion,
           sizeof(m_anLercAddCompressionAndVersion));

    if( poODS->OpenOffset( VSI_TIFFOpenChild(m_hTIFF), nOverviewOffset,
                            GA_Update ) != CE_None )
    {
        delete poODS;
        return CE_Failure;
    }

    // Do that now that m_nCompression is set
    poODS->RestoreVolatileParameters( poODS->m_hTIFF );

    ++m_nOverviewCount;
    m_papoOverviewDS = static_cast<GTiffDataset **>(
        CPLRealloc( m_papoOverviewDS,
                    m_nOverviewCount * (sizeof(void*))) );
    m_papoOverviewDS[m_nOverviewCount-1] = poODS;
    poODS->m_poBaseDS = this;
    poODS->m_bIsOverview = true;
    return CE_None;
}

/************************************************************************/
/*                     CreateTIFFColorTable()                           */
/************************************************************************/

static void CreateTIFFColorTable(GDALColorTable* poColorTable,
                                 int nBits,
                                 std::vector<unsigned short>& anTRed,
                                 std::vector<unsigned short>& anTGreen,
                                 std::vector<unsigned short>& anTBlue,
                                 unsigned short*& panRed,
                                 unsigned short*& panGreen,
                                 unsigned short*& panBlue)
{
    int nColors;

    if( nBits == 8 )
        nColors = 256;
    else if( nBits < 8 )
        nColors = 1 << nBits;
    else
        nColors = 65536;

    anTRed.resize(nColors,0);
    anTGreen.resize(nColors,0);
    anTBlue.resize(nColors,0);

    for( int iColor = 0; iColor < nColors; ++iColor )
    {
        if( iColor < poColorTable->GetColorEntryCount() )
        {
            GDALColorEntry sRGB;

            poColorTable->GetColorEntryAsRGB( iColor, &sRGB );

            anTRed[iColor] = static_cast<unsigned short>(257 * sRGB.c1);
            anTGreen[iColor] = static_cast<unsigned short>(257 * sRGB.c2);
            anTBlue[iColor] = static_cast<unsigned short>(257 * sRGB.c3);
        }
        else
        {
            anTRed[iColor] = 0;
            anTGreen[iColor] = 0;
            anTBlue[iColor] = 0;
        }
    }

    panRed = &(anTRed[0]);
    panGreen = &(anTGreen[0]);
    panBlue = &(anTBlue[0]);
}

/************************************************************************/
/*                  CreateOverviewsFromSrcOverviews()                   */
/************************************************************************/

// If poOvrDS is not null, it is used and poSrcDS is ignored.

CPLErr GTiffDataset::CreateOverviewsFromSrcOverviews(GDALDataset* poSrcDS,
                                                     GDALDataset* poOvrDS)
{
    CPLAssert(poSrcDS->GetRasterCount() != 0);
    CPLAssert(m_nOverviewCount == 0);

    ScanDirectories();

    FlushDirectory();

    int nOvBitsPerSample = m_nBitsPerSample;

    int l_nPhotometric = m_nPhotometric;
    const char* psvPhotometricValue = CPLGetConfigOption( "PHOTOMETRIC_OVERVIEW", nullptr );
    if( psvPhotometricValue != nullptr )
    {
        if ( EQUAL (psvPhotometricValue, "YCBCR" ) && nBands == 3)
        {
            l_nPhotometric = PHOTOMETRIC_YCBCR;
        }
        else
        {
            ReportError(CE_Warning, CPLE_NotSupported,
            "Building external overviews with PHOTOMETRIC_OVERVIEW's other than YCBCR are not supported "
            );
        }
    }



/* -------------------------------------------------------------------- */
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    std::vector<unsigned short> anTRed;
    std::vector<unsigned short> anTGreen;
    std::vector<unsigned short> anTBlue;
    unsigned short *panRed = nullptr;
    unsigned short *panGreen = nullptr;
    unsigned short *panBlue = nullptr;

    if( l_nPhotometric == PHOTOMETRIC_PALETTE && m_poColorTable != nullptr )
    {
        CreateTIFFColorTable(m_poColorTable, nOvBitsPerSample,
                             anTRed, anTGreen, anTBlue,
                             panRed, panGreen, panBlue);
    }

/* -------------------------------------------------------------------- */
/*      Do we need some metadata for the overviews?                     */
/* -------------------------------------------------------------------- */
    CPLString osMetadata;

    GTIFFBuildOverviewMetadata( "NONE", this, osMetadata );

/* -------------------------------------------------------------------- */
/*      Fetch extra sample tag                                          */
/* -------------------------------------------------------------------- */
    uint16_t *panExtraSampleValues = nullptr;
    uint16_t nExtraSamples = 0;

    if( TIFFGetField( m_hTIFF, TIFFTAG_EXTRASAMPLES, &nExtraSamples,
                      &panExtraSampleValues) )
    {
        uint16_t* panExtraSampleValuesNew =
            static_cast<uint16_t*>(
                CPLMalloc(nExtraSamples * sizeof(uint16_t)) );
        memcpy( panExtraSampleValuesNew, panExtraSampleValues,
                nExtraSamples * sizeof(uint16_t));
        panExtraSampleValues = panExtraSampleValuesNew;
    }
    else
    {
        panExtraSampleValues = nullptr;
        nExtraSamples = 0;
    }

    int l_nCompression = m_nCompression;
    const char* pszCompressValue = CPLGetConfigOption( "COMPRESS_OVERVIEW", nullptr );
    if( pszCompressValue != nullptr )
    {
        l_nCompression = GTIFFGetCompressionMethod(pszCompressValue, "COMPRESS_OVERVIEW");
        if( l_nCompression < 0 )
        {
            l_nCompression = m_nCompression;
        }
    }


/* -------------------------------------------------------------------- */
/*      Fetch predictor tag                                             */
/* -------------------------------------------------------------------- */
    uint16_t nPredictor = PREDICTOR_NONE;
    if( l_nCompression == COMPRESSION_LZW ||
        l_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        l_nCompression == COMPRESSION_ZSTD )
    {
        if ( CPLGetConfigOption( "PREDICTOR_OVERVIEW", nullptr ) != nullptr )
        {
            nPredictor = static_cast<uint16_t>(atoi(CPLGetConfigOption("PREDICTOR_OVERVIEW","1")));
        }
        else
        {
            TIFFGetField( m_hTIFF, TIFFTAG_PREDICTOR, &nPredictor );
        }
    }

    int nOvrBlockXSize = 0;
    int nOvrBlockYSize = 0;
    GTIFFGetOverviewBlockSize(GDALRasterBand::ToHandle(GetRasterBand(1)),
                              &nOvrBlockXSize, &nOvrBlockYSize);

    int nSrcOverviews = poOvrDS ?
        poOvrDS->GetRasterBand(1)->GetOverviewCount() + 1:
        poSrcDS->GetRasterBand(1)->GetOverviewCount();
    CPLErr eErr = CE_None;

    for( int i = 0; i < nSrcOverviews && eErr == CE_None; ++i )
    {
        GDALRasterBand* poOvrBand = poOvrDS ?
            ((i == 0) ? poOvrDS->GetRasterBand(1) :
                        poOvrDS->GetRasterBand(1)->GetOverview(i-1)):
            poSrcDS->GetRasterBand(1)->GetOverview(i);

        int nOXSize = poOvrBand->GetXSize();
        int nOYSize = poOvrBand->GetYSize();

        int nOvrJpegQuality = m_nJpegQuality;
        if( l_nCompression == COMPRESSION_JPEG &&
            CPLGetConfigOption( "JPEG_QUALITY_OVERVIEW", nullptr ) != nullptr )
        {
            nOvrJpegQuality =
                atoi(CPLGetConfigOption("JPEG_QUALITY_OVERVIEW","75"));
        }
        int nOvrWebpLevel = m_nWebPLevel;
        if( l_nCompression == COMPRESSION_WEBP &&
            CPLGetConfigOption( "WEBP_LEVEL_OVERVIEW", nullptr ) != nullptr )
        {
            nOvrWebpLevel =
                atoi(CPLGetConfigOption("WEBP_LEVEL_OVERVIEW","75"));
        }

        CPLString osNoData; // don't move this in inner scope
        const char* pszNoData = nullptr;
        if( m_bNoDataSet )
        {
            osNoData = GTiffFormatGDALNoDataTagValue(m_dfNoDataValue);
            pszNoData = osNoData.c_str();
        }

        toff_t nOverviewOffset =
                GTIFFWriteDirectory(m_hTIFF, FILETYPE_REDUCEDIMAGE,
                                    nOXSize, nOYSize,
                                    nOvBitsPerSample, m_nPlanarConfig,
                                    m_nSamplesPerPixel,
                                    nOvrBlockXSize,
                                    nOvrBlockYSize,
                                    TRUE,
                                    l_nCompression, l_nPhotometric, m_nSampleFormat,
                                    nPredictor,
                                    panRed, panGreen, panBlue,
                                    nExtraSamples, panExtraSampleValues,
                                    osMetadata,
                                    nOvrJpegQuality >= 0 ?
                                        CPLSPrintf("%d", nOvrJpegQuality) : nullptr,
                                    CPLSPrintf("%d", m_nJpegTablesMode),
                                    pszNoData,
                                    m_anLercAddCompressionAndVersion,
                                    m_bWriteCOGLayout,
                                    nOvrWebpLevel >= 0 ?
                                        CPLSPrintf("%d", nOvrWebpLevel) : nullptr
                                   );

        if( nOverviewOffset == 0 )
            eErr = CE_Failure;
        else
            eErr = RegisterNewOverviewDataset(nOverviewOffset, nOvrJpegQuality, nOvrWebpLevel);
    }

    // For directory reloading, so that the chaining to the next directory is
    // reloaded, as well as compression parameters.
    ReloadDirectory();

    CPLFree(panExtraSampleValues);
    panExtraSampleValues = nullptr;

    return eErr;
}

/************************************************************************/
/*                           ReloadDirectory()                          */
/************************************************************************/

void GTiffDataset::ReloadDirectory()
{
    TIFFSetSubDirectory( m_hTIFF, 0 );
    CPL_IGNORE_RET_VAL( SetDirectory() );
}

/************************************************************************/
/*                       CreateInternalMaskOverviews()                  */
/************************************************************************/

CPLErr GTiffDataset::CreateInternalMaskOverviews(int nOvrBlockXSize,
                                                 int nOvrBlockYSize)
{
    ScanDirectories();

/* -------------------------------------------------------------------- */
/*      Create overviews for the mask.                                  */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    const char* pszInternalMask =
        CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", nullptr);
    if( m_poMaskDS != nullptr &&
        m_poMaskDS->GetRasterCount() == 1 &&
        (pszInternalMask == nullptr || CPLTestBool(pszInternalMask)) )
    {
        int nMaskOvrCompression;
        if( strstr(GDALGetMetadataItem(GDALGetDriverByName( "GTiff" ),
                                       GDAL_DMD_CREATIONOPTIONLIST, nullptr ),
                   "<Value>DEFLATE</Value>") != nullptr )
            nMaskOvrCompression = COMPRESSION_ADOBE_DEFLATE;
        else
            nMaskOvrCompression = COMPRESSION_PACKBITS;

        for( int i = 0; i < m_nOverviewCount; ++i )
        {
            if( m_papoOverviewDS[i]->m_poMaskDS == nullptr )
            {
                const toff_t nOverviewOffset =
                    GTIFFWriteDirectory(
                        m_hTIFF, FILETYPE_REDUCEDIMAGE | FILETYPE_MASK,
                        m_papoOverviewDS[i]->nRasterXSize,
                        m_papoOverviewDS[i]->nRasterYSize,
                        1, PLANARCONFIG_CONTIG,
                        1, nOvrBlockXSize, nOvrBlockYSize, TRUE,
                        nMaskOvrCompression, PHOTOMETRIC_MASK,
                        SAMPLEFORMAT_UINT, PREDICTOR_NONE,
                        nullptr, nullptr, nullptr, 0, nullptr,
                        "",
                        nullptr, nullptr, nullptr, nullptr,
                        m_bWriteCOGLayout,
                        nullptr );

                if( nOverviewOffset == 0 )
                {
                    eErr = CE_Failure;
                    continue;
                }

                GTiffDataset *poODS = new GTiffDataset();
                poODS->ShareLockWithParentDataset(this);
                poODS->m_pszFilename = CPLStrdup(m_pszFilename);
                if( poODS->OpenOffset( VSI_TIFFOpenChild(m_hTIFF),
                                       nOverviewOffset,
                                       GA_Update ) != CE_None )
                {
                    delete poODS;
                    eErr = CE_Failure;
                }
                else
                {
                    poODS->m_bPromoteTo8Bits =
                        CPLTestBool(
                            CPLGetConfigOption(
                                "GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES" ) );
                    poODS->m_poBaseDS = this;
                    poODS->m_poImageryDS = m_papoOverviewDS[i];
                    m_papoOverviewDS[i]->m_poMaskDS = poODS;
                    ++m_poMaskDS->m_nOverviewCount;
                    m_poMaskDS->m_papoOverviewDS = static_cast<GTiffDataset **>(
                        CPLRealloc(
                            m_poMaskDS->m_papoOverviewDS,
                            m_poMaskDS->m_nOverviewCount * (sizeof(void*))) );
                    m_poMaskDS->m_papoOverviewDS[m_poMaskDS->m_nOverviewCount-1] =
                        poODS;
                }
            }
        }
    }

    ReloadDirectory();

    return eErr;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr GTiffDataset::IBuildOverviews(
    const char * pszResampling,
    int nOverviews, int * panOverviewList,
    int nBandsIn, int * panBandList,
    GDALProgressFunc pfnProgress, void * pProgressData )

{
    ScanDirectories();

    // Make implicit JPEG overviews invisible, but do not destroy
    // them in case they are already used (not sure that the client
    // has the right to do that.  Behavior maybe undefined in GDAL API.
    m_nJPEGOverviewCount = 0;

/* -------------------------------------------------------------------- */
/*      If RRD or external OVR overviews requested, then invoke         */
/*      generic handling.                                               */
/* -------------------------------------------------------------------- */
    bool bUseGenericHandling = false;

    if( CPLTestBool(CPLGetConfigOption( "USE_RRD", "NO" ))
        || CPLTestBool(CPLGetConfigOption( "TIFF_USE_OVR", "NO" )) )
    {
        bUseGenericHandling = true;
    }

/* -------------------------------------------------------------------- */
/*      If we don't have read access, then create the overviews         */
/*      externally.                                                     */
/* -------------------------------------------------------------------- */
    if( GetAccess() != GA_Update )
    {
        CPLDebug( "GTiff",
                  "File open for read-only accessing, "
                  "creating overviews externally." );

        bUseGenericHandling = true;
    }

    if( bUseGenericHandling )
    {
        if( m_nOverviewCount != 0 )
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "Cannot add external overviews when there are already "
                "internal overviews" );
            return CE_Failure;
        }

        CPLErr eErr = GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList,
            nBandsIn, panBandList, pfnProgress, pProgressData );
        if( eErr == CE_None && m_poMaskDS )
        {
            ReportError(CE_Warning, CPLE_NotSupported,
                     "Building external overviews whereas there is an internal "
                     "mask is not fully supported. "
                     "The overviews of the non-mask bands will be created, "
                     "but not the overviews of the mask band.");
        }
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Our TIFF overview support currently only works safely if all    */
/*      bands are handled at the same time.                             */
/* -------------------------------------------------------------------- */
    if( nBandsIn != GetRasterCount() )
    {
        ReportError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in TIFF currently only "
                  "supported when operating on all bands.  "
                  "Operation failed." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If zero overviews were requested, we need to clear all          */
/*      existing overviews.                                             */
/* -------------------------------------------------------------------- */
    if( nOverviews == 0 )
    {
        if( m_nOverviewCount == 0 )
            return GDALDataset::IBuildOverviews(
                pszResampling, nOverviews, panOverviewList,
                nBandsIn, panBandList, pfnProgress, pProgressData );

        return CleanOverviews();
    }

    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, nullptr, pProgressData ) )
    {
        ReportError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    FlushDirectory();

/* -------------------------------------------------------------------- */
/*      If we are averaging bit data to grayscale we need to create     */
/*      8bit overviews.                                                 */
/* -------------------------------------------------------------------- */
    int nOvBitsPerSample = m_nBitsPerSample;

    if( STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2") )
        nOvBitsPerSample = 8;

/* -------------------------------------------------------------------- */
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    std::vector<unsigned short> anTRed;
    std::vector<unsigned short> anTGreen;
    std::vector<unsigned short> anTBlue;
    unsigned short *panRed = nullptr;
    unsigned short *panGreen = nullptr;
    unsigned short *panBlue = nullptr;

    if( m_nPhotometric == PHOTOMETRIC_PALETTE && m_poColorTable != nullptr )
    {
        CreateTIFFColorTable(m_poColorTable, nOvBitsPerSample,
                             anTRed, anTGreen, anTBlue,
                             panRed, panGreen, panBlue);
    }

/* -------------------------------------------------------------------- */
/*      Do we need some metadata for the overviews?                     */
/* -------------------------------------------------------------------- */
    CPLString osMetadata;

    GTIFFBuildOverviewMetadata( pszResampling, this, osMetadata );

/* -------------------------------------------------------------------- */
/*      Fetch extra sample tag                                          */
/* -------------------------------------------------------------------- */
    uint16_t *panExtraSampleValues = nullptr;
    uint16_t nExtraSamples = 0;

    if( TIFFGetField( m_hTIFF, TIFFTAG_EXTRASAMPLES, &nExtraSamples,
                      &panExtraSampleValues) )
    {
        uint16_t* panExtraSampleValuesNew =
            static_cast<uint16_t*>( CPLMalloc(nExtraSamples * sizeof(uint16_t)) );
        memcpy( panExtraSampleValuesNew, panExtraSampleValues,
                nExtraSamples * sizeof(uint16_t) );
        panExtraSampleValues = panExtraSampleValuesNew;
    }
    else
    {
        panExtraSampleValues = nullptr;
        nExtraSamples = 0;
    }

/* -------------------------------------------------------------------- */
/*      Fetch predictor tag                                             */
/* -------------------------------------------------------------------- */
    uint16_t nPredictor = PREDICTOR_NONE;
    if( m_nCompression == COMPRESSION_LZW ||
        m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        m_nCompression == COMPRESSION_ZSTD )
        TIFFGetField( m_hTIFF, TIFFTAG_PREDICTOR, &nPredictor );

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    int nOvrBlockXSize = 0;
    int nOvrBlockYSize = 0;
    GTIFFGetOverviewBlockSize(GDALRasterBand::ToHandle(GetRasterBand(1)),
                              &nOvrBlockXSize, &nOvrBlockYSize);
    std::vector<bool> abRequireNewOverview(nOverviews, true);
    for( int i = 0; i < nOverviews && eErr == CE_None; ++i )
    {
        for( int j = 0; j < m_nOverviewCount && eErr == CE_None; ++j )
        {
            GTiffDataset *poODS = m_papoOverviewDS[j];

            const int nOvFactor =
                GDALComputeOvFactor(poODS->GetRasterXSize(),
                                    GetRasterXSize(),
                                    poODS->GetRasterYSize(),
                                    GetRasterYSize());

            // If we already have a 1x1 overview and this new one would result
            // in it too, then don't create it.
            if( poODS->GetRasterXSize() == 1 &&
                poODS->GetRasterYSize() == 1 &&
                (GetRasterXSize() + panOverviewList[i] - 1)
                    / panOverviewList[i] == 1 &&
                (GetRasterYSize() + panOverviewList[i] - 1)
                    / panOverviewList[i] == 1 )
            {
                abRequireNewOverview[i] = false;
                break;
            }

            if( nOvFactor == panOverviewList[i]
                || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                                    GetRasterXSize(),
                                                    GetRasterYSize() ) )
            {
                abRequireNewOverview[i] = false;
                break;
            }
        }

        if( abRequireNewOverview[i] )
        {
            if( m_bLayoutIFDSBeforeData && !m_bKnownIncompatibleEdition &&
                !m_bWriteKnownIncompatibleEdition )
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                         "Adding new overviews invalidates the "
                         "LAYOUT=IFDS_BEFORE_DATA property");
                m_bKnownIncompatibleEdition = true;
                m_bWriteKnownIncompatibleEdition = true;
            }

            const int nOXSize =
                (GetRasterXSize() + panOverviewList[i] - 1)
                / panOverviewList[i];
            const int nOYSize =
                (GetRasterYSize() + panOverviewList[i] - 1)
                / panOverviewList[i];

            int nOvrJpegQuality = m_nJpegQuality;
            if( m_nCompression == COMPRESSION_JPEG &&
                CPLGetConfigOption( "JPEG_QUALITY_OVERVIEW", nullptr ) != nullptr )
            {
                nOvrJpegQuality =
                    atoi(CPLGetConfigOption("JPEG_QUALITY_OVERVIEW","75"));
            }
            int nOvrWebpLevel = m_nWebPLevel;
            if( m_nCompression == COMPRESSION_WEBP &&
                CPLGetConfigOption( "WEBP_LEVEL_OVERVIEW", nullptr ) != nullptr )
            {
                nOvrWebpLevel =
                    atoi(CPLGetConfigOption("WEBP_LEVEL_OVERVIEW","75"));
            }

            CPLString osNoData; // don't move this in inner scope
            const char* pszNoData = nullptr;
            if( m_bNoDataSet )
            {
                osNoData = GTiffFormatGDALNoDataTagValue(m_dfNoDataValue);
                pszNoData = osNoData.c_str();
            }

            const toff_t nOverviewOffset =
                GTIFFWriteDirectory(
                    m_hTIFF, FILETYPE_REDUCEDIMAGE,
                    nOXSize, nOYSize,
                    nOvBitsPerSample, m_nPlanarConfig,
                    m_nSamplesPerPixel, nOvrBlockXSize, nOvrBlockYSize, TRUE,
                    m_nCompression, m_nPhotometric, m_nSampleFormat,
                    nPredictor,
                    panRed, panGreen, panBlue,
                    nExtraSamples, panExtraSampleValues,
                    osMetadata,
                    nOvrJpegQuality >= 0 ?
                                CPLSPrintf("%d", nOvrJpegQuality) : nullptr,
                    CPLSPrintf("%d", m_nJpegTablesMode),
                    pszNoData,
                    m_anLercAddCompressionAndVersion,
                    false,
                    nOvrWebpLevel >= 0 ?
                                CPLSPrintf("%d", nOvrWebpLevel) : nullptr
            );

            if( nOverviewOffset == 0 )
                eErr = CE_Failure;
            else
                eErr = RegisterNewOverviewDataset(nOverviewOffset,
                                                  nOvrJpegQuality,
                                                  nOvrWebpLevel);
        }
    }

    CPLFree(panExtraSampleValues);
    panExtraSampleValues = nullptr;

    ReloadDirectory();

/* -------------------------------------------------------------------- */
/*      Create overviews for the mask.                                  */
/* -------------------------------------------------------------------- */
    if( eErr != CE_None )
        return eErr;

    eErr = CreateInternalMaskOverviews(nOvrBlockXSize, nOvrBlockYSize);

/* -------------------------------------------------------------------- */
/*      Refresh overviews for the mask                                  */
/* -------------------------------------------------------------------- */
    if( m_poMaskDS != nullptr &&
        m_poMaskDS->GetRasterCount() == 1 )
    {
        int nMaskOverviews = 0;

        GDALRasterBand **papoOverviewBands = static_cast<GDALRasterBand **>(
            CPLCalloc(sizeof(void*),m_nOverviewCount) );
        for( int i = 0; i < m_nOverviewCount; ++i )
        {
            if( m_papoOverviewDS[i]->m_poMaskDS != nullptr )
            {
                papoOverviewBands[nMaskOverviews++] =
                        m_papoOverviewDS[i]->m_poMaskDS->GetRasterBand(1);
            }
        }
        eErr = GDALRegenerateOverviews(
            m_poMaskDS->GetRasterBand(1),
            nMaskOverviews,
            reinterpret_cast<GDALRasterBandH *>( papoOverviewBands ),
            pszResampling, GDALDummyProgress, nullptr );
        CPLFree(papoOverviewBands);
    }

/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
    if( m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        GDALDataTypeIsComplex(GetRasterBand( panBandList[0] )->
                              GetRasterDataType()) == FALSE &&
        GetRasterBand( panBandList[0] )->GetColorTable() == nullptr &&
        (STARTS_WITH_CI(pszResampling, "NEAR") ||
         EQUAL(pszResampling, "AVERAGE") ||
         EQUAL(pszResampling, "GAUSS") ||
         EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") ||
         EQUAL(pszResampling, "BILINEAR")) )
    {
        // In the case of pixel interleaved compressed overviews, we want to
        // generate the overviews for all the bands block by block, and not
        // band after band, in order to write the block once and not loose
        // space in the TIFF file.  We also use that logic for uncompressed
        // overviews, since GDALRegenerateOverviewsMultiBand() will be able to
        // trigger cascading overview regeneration even in the presence
        // of an alpha band.

        int nNewOverviews = 0;

        GDALRasterBand ***papapoOverviewBands =
            static_cast<GDALRasterBand ***>(CPLCalloc(sizeof(void*),nBandsIn));
        GDALRasterBand **papoBandList =
            static_cast<GDALRasterBand **>(CPLCalloc(sizeof(void*),nBandsIn));
        for( int iBand = 0; iBand < nBandsIn; ++iBand )
        {
            GDALRasterBand* poBand = GetRasterBand( panBandList[iBand] );

            papoBandList[iBand] = poBand;
            papapoOverviewBands[iBand] =
                static_cast<GDALRasterBand **>( CPLCalloc(
                    sizeof(void*), poBand->GetOverviewCount()) );

            int iCurOverview = 0;
            std::vector<bool> abAlreadyUsedOverviewBand(
                poBand->GetOverviewCount(), false);

            for( int i = 0; i < nOverviews; ++i )
            {
                for( int j = 0; j < poBand->GetOverviewCount(); ++j )
                {
                    if( abAlreadyUsedOverviewBand[j] )
                        continue;

                    int    nOvFactor;
                    GDALRasterBand * poOverview = poBand->GetOverview( j );

                    nOvFactor = GDALComputeOvFactor(poOverview->GetXSize(),
                                                     poBand->GetXSize(),
                                                     poOverview->GetYSize(),
                                                     poBand->GetYSize());

                    int bHasNoData = FALSE;
                    double noDataValue = poBand->GetNoDataValue(&bHasNoData);

                    if( bHasNoData )
                        poOverview->SetNoDataValue(noDataValue);

                    if( nOvFactor == panOverviewList[i]
                        || nOvFactor == GDALOvLevelAdjust2(
                                            panOverviewList[i],
                                            poBand->GetXSize(),
                                            poBand->GetYSize() ) )
                    {
                        abAlreadyUsedOverviewBand[j] = true;
                        CPLAssert(iCurOverview < poBand->GetOverviewCount());
                        papapoOverviewBands[iBand][iCurOverview] = poOverview;
                        ++iCurOverview ;
                        break;
                    }
                }
            }

            if( nNewOverviews == 0 )
            {
                nNewOverviews = iCurOverview;
            }
            else if( nNewOverviews != iCurOverview )
            {
                CPLAssert(false);
                return CE_Failure;
            }
        }

        GDALRegenerateOverviewsMultiBand( nBandsIn, papoBandList,
                                          nNewOverviews, papapoOverviewBands,
                                          pszResampling, pfnProgress,
                                          pProgressData );

        for( int iBand = 0; iBand < nBandsIn; ++iBand )
        {
            CPLFree(papapoOverviewBands[iBand]);
        }
        CPLFree(papapoOverviewBands);
        CPLFree(papoBandList);
    }
    else
    {
        GDALRasterBand **papoOverviewBands = static_cast<GDALRasterBand **>(
            CPLCalloc(sizeof(void*), nOverviews) );

        for( int iBand = 0; iBand < nBandsIn && eErr == CE_None; ++iBand )
        {
            GDALRasterBand *poBand = GetRasterBand( panBandList[iBand] );
            if( poBand == nullptr )
            {
                eErr = CE_Failure;
                break;
            }

            std::vector<bool> abAlreadyUsedOverviewBand(
                poBand->GetOverviewCount(), false);

            int nNewOverviews = 0;
            for( int i = 0; i < nOverviews; ++i )
            {
                for( int j = 0; j < poBand->GetOverviewCount(); ++j )
                {
                    if( abAlreadyUsedOverviewBand[j] )
                        continue;

                    GDALRasterBand * poOverview = poBand->GetOverview( j );

                    int bHasNoData = FALSE;
                    double noDataValue = poBand->GetNoDataValue(&bHasNoData);

                    if( bHasNoData )
                        poOverview->SetNoDataValue(noDataValue);

                    const int nOvFactor =
                        GDALComputeOvFactor(poOverview->GetXSize(),
                                            poBand->GetXSize(),
                                            poOverview->GetYSize(),
                                            poBand->GetYSize());

                    if( nOvFactor == panOverviewList[i]
                        || nOvFactor == GDALOvLevelAdjust2(
                                            panOverviewList[i],
                                            poBand->GetXSize(),
                                            poBand->GetYSize() ) )
                    {
                        abAlreadyUsedOverviewBand[j] = true;
                        CPLAssert(nNewOverviews < poBand->GetOverviewCount());
                        papoOverviewBands[nNewOverviews++] = poOverview;
                        break;
                    }
                }
            }

            void *pScaledProgressData =
                GDALCreateScaledProgress(
                    iBand / static_cast<double>( nBandsIn ),
                    (iBand + 1) / static_cast<double>( nBandsIn ),
                    pfnProgress, pProgressData );

            eErr = GDALRegenerateOverviews(
                poBand,
                nNewOverviews,
                reinterpret_cast<GDALRasterBandH *>( papoOverviewBands ),
                pszResampling,
                GDALScaledProgress,
                pScaledProgressData );

            GDALDestroyScaledProgress( pScaledProgressData );
        }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
        CPLFree( papoOverviewBands );
    }

    pfnProgress( 1.0, nullptr, pProgressData );

    return eErr;
}

/************************************************************************/
/*                      GTiffWriteDummyGeokeyDirectory()                */
/************************************************************************/

static void GTiffWriteDummyGeokeyDirectory( TIFF* hTIFF )
{
    // If we have existing geokeys, try to wipe them
    // by writing a dummy geokey directory. (#2546)
    uint16_t *panVI = nullptr;
    uint16_t nKeyCount = 0;

    if( TIFFGetField( hTIFF, TIFFTAG_GEOKEYDIRECTORY,
                        &nKeyCount, &panVI ) )
    {
        GUInt16 anGKVersionInfo[4] = { 1, 1, 0, 0 };
        double adfDummyDoubleParams[1] = { 0.0 };
        TIFFSetField( hTIFF, TIFFTAG_GEOKEYDIRECTORY,
                        4, anGKVersionInfo );
        TIFFSetField( hTIFF, TIFFTAG_GEODOUBLEPARAMS,
                        1, adfDummyDoubleParams );
        TIFFSetField( hTIFF, TIFFTAG_GEOASCIIPARAMS, "" );
    }
}

/************************************************************************/
/*               GTiffDatasetLibGeotiffErrorCallback()                  */
/************************************************************************/

static void GTiffDatasetLibGeotiffErrorCallback(GTIF*,
                                                int level,
                                                const char* pszMsg, ...)
{
    va_list ap;
    va_start(ap, pszMsg);
    CPLErrorV( (level == LIBGEOTIFF_WARNING ) ? CE_Warning : CE_Failure,
               CPLE_AppDefined, pszMsg, ap );
    va_end(ap);
}

/************************************************************************/
/*                           GTiffDatasetGTIFNew()                      */
/************************************************************************/

static GTIF* GTiffDatasetGTIFNew( TIFF* hTIFF )
{
    GTIF* gtif = GTIFNewEx(hTIFF, GTiffDatasetLibGeotiffErrorCallback, nullptr);
    if( gtif )
    {
        GTIFAttachPROJContext(gtif, OSRGetProjTLSContext());
    }
    return gtif;
}

/************************************************************************/
/*                          WriteGeoTIFFInfo()                          */
/************************************************************************/

void GTiffDataset::WriteGeoTIFFInfo()

{
    bool bPixelIsPoint = false;
    bool bPointGeoIgnore = false;

    const char* pszAreaOrPoint =
        GTiffDataset::GetMetadataItem( GDALMD_AREA_OR_POINT );
    if( pszAreaOrPoint && EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT) )
    {
        bPixelIsPoint = true;
        bPointGeoIgnore =
            CPLTestBool( CPLGetConfigOption( "GTIFF_POINT_GEO_IGNORE",
                                             "FALSE") );
    }

    if( m_bForceUnsetGTOrGCPs )
    {
        m_bNeedsRewrite = true;
        m_bForceUnsetGTOrGCPs = false;

        TIFFUnsetField( m_hTIFF, TIFFTAG_GEOPIXELSCALE );
        TIFFUnsetField( m_hTIFF, TIFFTAG_GEOTIEPOINTS );
        TIFFUnsetField( m_hTIFF, TIFFTAG_GEOTRANSMATRIX );
    }

    if( m_bForceUnsetProjection )
    {
        m_bNeedsRewrite = true;
        m_bForceUnsetProjection = false;

        TIFFUnsetField( m_hTIFF, TIFFTAG_GEOKEYDIRECTORY );
        TIFFUnsetField( m_hTIFF, TIFFTAG_GEODOUBLEPARAMS );
        TIFFUnsetField( m_hTIFF, TIFFTAG_GEOASCIIPARAMS );
    }

/* -------------------------------------------------------------------- */
/*      Write geotransform if valid.                                    */
/* -------------------------------------------------------------------- */
    if( m_bGeoTransformValid )
    {
        m_bNeedsRewrite = true;

/* -------------------------------------------------------------------- */
/*      Clear old tags to ensure we don't end up with conflicting       */
/*      information. (#2625)                                            */
/* -------------------------------------------------------------------- */
        TIFFUnsetField( m_hTIFF, TIFFTAG_GEOPIXELSCALE );
        TIFFUnsetField( m_hTIFF, TIFFTAG_GEOTIEPOINTS );
        TIFFUnsetField( m_hTIFF, TIFFTAG_GEOTRANSMATRIX );

/* -------------------------------------------------------------------- */
/*      Write the transform.  If we have a normal north-up image we     */
/*      use the tiepoint plus pixelscale otherwise we use a matrix.     */
/* -------------------------------------------------------------------- */
        if( m_adfGeoTransform[2] == 0.0 && m_adfGeoTransform[4] == 0.0
                && m_adfGeoTransform[5] < 0.0 )
        {
            double dfOffset = 0.0;
            if( m_eProfile != GTiffProfile::BASELINE )
            {
                // In the case the SRS has a vertical component and we have
                // a single band, encode its scale/offset in the GeoTIFF tags
                int bHasScale = FALSE;
                double dfScale = GetRasterBand(1)->GetScale(&bHasScale);
                int bHasOffset = FALSE;
                dfOffset = GetRasterBand(1)->GetOffset(&bHasOffset);
                const bool bApplyScaleOffset =
                    m_oSRS.IsVertical() &&
                    GetRasterCount() == 1;
                if( bApplyScaleOffset && !bHasScale )
                    dfScale = 1.0;
                if( !bApplyScaleOffset || !bHasOffset )
                    dfOffset = 0.0;
                const double adfPixelScale[3] = {
                    m_adfGeoTransform[1], fabs(m_adfGeoTransform[5]),
                    bApplyScaleOffset ? dfScale  : 0.0 };
                TIFFSetField( m_hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );
            }

            double adfTiePoints[6] = {
                0.0, 0.0, 0.0, m_adfGeoTransform[0], m_adfGeoTransform[3], dfOffset };

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                adfTiePoints[3] +=
                    m_adfGeoTransform[1] * 0.5 + m_adfGeoTransform[2] * 0.5;
                adfTiePoints[4] +=
                    m_adfGeoTransform[4] * 0.5 + m_adfGeoTransform[5] * 0.5;
            }

            if( m_eProfile != GTiffProfile::BASELINE )
                TIFFSetField( m_hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
        }
        else
        {
            double adfMatrix[16] = {};

            adfMatrix[0] = m_adfGeoTransform[1];
            adfMatrix[1] = m_adfGeoTransform[2];
            adfMatrix[3] = m_adfGeoTransform[0];
            adfMatrix[4] = m_adfGeoTransform[4];
            adfMatrix[5] = m_adfGeoTransform[5];
            adfMatrix[7] = m_adfGeoTransform[3];
            adfMatrix[15] = 1.0;

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                adfMatrix[3] +=
                    m_adfGeoTransform[1] * 0.5 + m_adfGeoTransform[2] * 0.5;
                adfMatrix[7] +=
                    m_adfGeoTransform[4] * 0.5 + m_adfGeoTransform[5] * 0.5;
            }

            if( m_eProfile != GTiffProfile::BASELINE )
                TIFFSetField( m_hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix );
        }

        // Do we need a world file?
        if( CPLFetchBool( m_papszCreationOptions, "TFW", false ) )
            GDALWriteWorldFile( m_pszFilename, "tfw", m_adfGeoTransform );
        else if( CPLFetchBool( m_papszCreationOptions, "WORLDFILE", false ) )
            GDALWriteWorldFile( m_pszFilename, "wld", m_adfGeoTransform );
    }
    else if( GetGCPCount() > 0 )
    {
        m_bNeedsRewrite = true;

        double *padfTiePoints = static_cast<double *>(
            CPLMalloc( 6 * sizeof(double) * GetGCPCount() ) );

        for( int iGCP = 0; iGCP < GetGCPCount(); ++iGCP )
        {

            padfTiePoints[iGCP*6+0] = m_pasGCPList[iGCP].dfGCPPixel;
            padfTiePoints[iGCP*6+1] = m_pasGCPList[iGCP].dfGCPLine;
            padfTiePoints[iGCP*6+2] = 0;
            padfTiePoints[iGCP*6+3] = m_pasGCPList[iGCP].dfGCPX;
            padfTiePoints[iGCP*6+4] = m_pasGCPList[iGCP].dfGCPY;
            padfTiePoints[iGCP*6+5] = m_pasGCPList[iGCP].dfGCPZ;

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                padfTiePoints[iGCP*6+0] += 0.5;
                padfTiePoints[iGCP*6+1] += 0.5;
            }
        }

        if( m_eProfile != GTiffProfile::BASELINE )
            TIFFSetField( m_hTIFF, TIFFTAG_GEOTIEPOINTS,
                          6 * GetGCPCount(), padfTiePoints );
        CPLFree( padfTiePoints );
    }

/* -------------------------------------------------------------------- */
/*      Write out projection definition.                                */
/* -------------------------------------------------------------------- */
    const bool bHasProjection = !m_oSRS.IsEmpty();
    if( (bHasProjection || bPixelIsPoint)
        && m_eProfile != GTiffProfile::BASELINE )
    {
        m_bNeedsRewrite = true;

        // If we have existing geokeys, try to wipe them
        // by writing a dummy geokey directory. (#2546)
        GTiffWriteDummyGeokeyDirectory(m_hTIFF);

        GTIF *psGTIF = GTiffDatasetGTIFNew( m_hTIFF );

        // Set according to coordinate system.
        if( bHasProjection )
        {
            char* pszProjection = nullptr;
            {
                CPLErrorStateBackuper oErrorStateBackuper;
                CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
                m_oSRS.exportToWkt(&pszProjection);
            }
            if( pszProjection && pszProjection[0] &&
                strstr(pszProjection, "custom_proj4") == nullptr )
            {
                GTIFSetFromOGISDefnEx( psGTIF, pszProjection,
                                       m_eGeoTIFFKeysFlavor,
                                       m_eGeoTIFFVersion );
            }
            else
            {
                GDALPamDataset::SetSpatialRef(&m_oSRS);
            }
            CPLFree(pszProjection);
        }

        if( bPixelIsPoint )
        {
            GTIFKeySet(psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1,
                       RasterPixelIsPoint);
        }

        GTIFWriteKeys( psGTIF );
        GTIFFree( psGTIF );
    }
}

/************************************************************************/
/*                         AppendMetadataItem()                         */
/************************************************************************/

static void AppendMetadataItem( CPLXMLNode **ppsRoot, CPLXMLNode **ppsTail,
                                const char *pszKey, const char *pszValue,
                                int nBand, const char *pszRole,
                                const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      Create the Item element, and subcomponents.                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psItem = CPLCreateXMLNode( nullptr, CXT_Element, "Item" );
    CPLCreateXMLNode( CPLCreateXMLNode( psItem, CXT_Attribute, "name"),
                      CXT_Text, pszKey );

    if( nBand > 0 )
    {
        char szBandId[32] = {};
        snprintf( szBandId, sizeof(szBandId), "%d", nBand - 1 );
        CPLCreateXMLNode( CPLCreateXMLNode( psItem,CXT_Attribute,"sample"),
                          CXT_Text, szBandId );
    }

    if( pszRole != nullptr )
        CPLCreateXMLNode( CPLCreateXMLNode( psItem,CXT_Attribute,"role"),
                          CXT_Text, pszRole );

    if( pszDomain != nullptr && strlen(pszDomain) > 0 )
        CPLCreateXMLNode( CPLCreateXMLNode( psItem,CXT_Attribute,"domain"),
                          CXT_Text, pszDomain );

    char *pszEscapedItemValue = CPLEscapeString(pszValue,-1,CPLES_XML);
    CPLCreateXMLNode( psItem, CXT_Text, pszEscapedItemValue );
    CPLFree( pszEscapedItemValue );

/* -------------------------------------------------------------------- */
/*      Create root, if missing.                                        */
/* -------------------------------------------------------------------- */
    if( *ppsRoot == nullptr )
        *ppsRoot = CPLCreateXMLNode( nullptr, CXT_Element, "GDALMetadata" );

/* -------------------------------------------------------------------- */
/*      Append item to tail.  We keep track of the tail to avoid        */
/*      O(nsquared) time as the list gets longer.                       */
/* -------------------------------------------------------------------- */
    if( *ppsTail == nullptr )
        CPLAddXMLChild( *ppsRoot, psItem );
    else
        CPLAddXMLSibling( *ppsTail, psItem );

    *ppsTail = psItem;
}

/************************************************************************/
/*                         WriteMDMetadata()                            */
/************************************************************************/

static void WriteMDMetadata( GDALMultiDomainMetadata *poMDMD, TIFF *hTIFF,
                             CPLXMLNode **ppsRoot, CPLXMLNode **ppsTail,
                             int nBand, GTiffProfile eProfile )

{

/* ==================================================================== */
/*      Process each domain.                                            */
/* ==================================================================== */
    char **papszDomainList = poMDMD->GetDomainList();
    for( int iDomain = 0;
         papszDomainList && papszDomainList[iDomain];
         ++iDomain )
    {
        char **papszMD = poMDMD->GetMetadata( papszDomainList[iDomain] );
        bool bIsXML = false;

        if( EQUAL(papszDomainList[iDomain], "IMAGE_STRUCTURE") )
            continue;  // Ignored.
        if( EQUAL(papszDomainList[iDomain], "COLOR_PROFILE") )
            continue;  // Ignored.
        if( EQUAL(papszDomainList[iDomain], MD_DOMAIN_RPC) )
            continue;  // Handled elsewhere.
        if( EQUAL(papszDomainList[iDomain], "xml:ESRI")
            && CPLTestBool(CPLGetConfigOption( "ESRI_XML_PAM", "NO" )) )
            continue;  // Handled elsewhere.
        if( EQUAL(papszDomainList[iDomain], "xml:XMP") )
            continue;  // Handled in SetMetadata.

        if( STARTS_WITH_CI(papszDomainList[iDomain], "xml:") )
            bIsXML = true;

/* -------------------------------------------------------------------- */
/*      Process each item in this domain.                               */
/* -------------------------------------------------------------------- */
        for( int iItem = 0; papszMD && papszMD[iItem]; ++iItem )
        {
            const char *pszItemValue = nullptr;
            char *pszItemName = nullptr;

            if( bIsXML )
            {
                pszItemName = CPLStrdup("doc");
                pszItemValue = papszMD[iItem];
            }
            else
            {
                pszItemValue = CPLParseNameValue( papszMD[iItem], &pszItemName);
                if( pszItemName == nullptr )
                {
                    CPLDebug( "GTiff",
                              "Invalid metadata item : %s", papszMD[iItem] );
                    continue;
                }
            }

/* -------------------------------------------------------------------- */
/*      Convert into XML item or handle as a special TIFF tag.          */
/* -------------------------------------------------------------------- */
            if( strlen(papszDomainList[iDomain]) == 0
                && nBand == 0 &&
                (STARTS_WITH_CI(pszItemName, "TIFFTAG_") ||
                 (EQUAL(pszItemName, "GEO_METADATA") &&
                  eProfile == GTiffProfile::GDALGEOTIFF) ||
                 (EQUAL(pszItemName, "TIFF_RSID") &&
                  eProfile == GTiffProfile::GDALGEOTIFF)) )
            {
                if( EQUAL(pszItemName, "TIFFTAG_RESOLUTIONUNIT") )
                {
                    // ResolutionUnit can't be 0, which is the default if
                    // atoi() fails.  Set to 1=Unknown.
                    int v = atoi(pszItemValue);
                    if( !v ) v = RESUNIT_NONE;
                    TIFFSetField( hTIFF, TIFFTAG_RESOLUTIONUNIT, v);
                }
                else
                {
                    bool bFoundTag = false;
                    size_t iTag = 0;  // Used after for.
                    for( ;
                         iTag < sizeof(asTIFFTags) / sizeof(asTIFFTags[0]);
                         ++iTag )
                    {
                        if( EQUAL(pszItemName, asTIFFTags[iTag].pszTagName) )
                        {
                            bFoundTag = true;
                            break;
                        }
                    }

                    if( bFoundTag &&
                        asTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING )
                        TIFFSetField( hTIFF, asTIFFTags[iTag].nTagVal,
                                      pszItemValue );
                    else if( bFoundTag &&
                             asTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT )
                        TIFFSetField( hTIFF, asTIFFTags[iTag].nTagVal,
                                      CPLAtof(pszItemValue) );
                    else if( bFoundTag &&
                             asTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT )
                        TIFFSetField( hTIFF, asTIFFTags[iTag].nTagVal,
                                      atoi(pszItemValue) );
                    else if( bFoundTag &&
                             asTIFFTags[iTag].eType == GTIFFTAGTYPE_BYTE_STRING )
                    {
                        uint32_t nLen = static_cast<uint32_t>(strlen(pszItemValue));
                        if( nLen )
                        {
                            TIFFSetField( hTIFF, asTIFFTags[iTag].nTagVal,
                                          nLen,
                                          pszItemValue );
                        }
                    }
                    else
                        CPLError(
                            CE_Warning, CPLE_NotSupported,
                            "%s metadata item is unhandled and "
                            "will not be written",
                            pszItemName);
                }
            }
            else if( nBand == 0 && EQUAL(pszItemName,GDALMD_AREA_OR_POINT) )
            {
                /* Do nothing, handled elsewhere. */;
            }
            else
            {
                AppendMetadataItem( ppsRoot, ppsTail,
                                    pszItemName, pszItemValue,
                                    nBand, nullptr, papszDomainList[iDomain] );
            }

            CPLFree( pszItemName );
        }

/* -------------------------------------------------------------------- */
/*      Remove TIFFTAG_xxxxxx that are already set but no longer in     */
/*      the metadata list (#5619)                                       */
/* -------------------------------------------------------------------- */
        if( strlen(papszDomainList[iDomain]) == 0 && nBand == 0 )
        {
            for( size_t iTag = 0;
                 iTag < sizeof(asTIFFTags) / sizeof(asTIFFTags[0]);
                 ++iTag )
            {
                uint32_t nCount = 0;
                char* pszText = nullptr;
                int16_t nVal = 0;
                float fVal = 0.0f;
                const char* pszVal =
                    CSLFetchNameValue(papszMD, asTIFFTags[iTag].pszTagName);
                if( pszVal == nullptr &&
                    ((asTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING &&
                      TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal,
                                    &pszText )) ||
                     (asTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT &&
                      TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &nVal )) ||
                     (asTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT &&
                      TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &fVal )) ||
                     (asTIFFTags[iTag].eType == GTIFFTAGTYPE_BYTE_STRING &&
                      TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &nCount, &pszText ))) )
                {
                    TIFFUnsetField( hTIFF, asTIFFTags[iTag].nTagVal );
                }
            }
        }
    }
}

/************************************************************************/
/*                           WriteRPC()                                 */
/************************************************************************/

void GTiffDataset::WriteRPC( GDALDataset *poSrcDS, TIFF *l_hTIFF,
                             int bSrcIsGeoTIFF,
                             GTiffProfile eProfile,
                             const char *pszTIFFFilename,
                             char **l_papszCreationOptions,
                             bool bWriteOnlyInPAMIfNeeded )
{
/* -------------------------------------------------------------------- */
/*      Handle RPC data written to TIFF RPCCoefficient tag, RPB file,   */
/*      RPCTEXT file or PAM.                                            */
/* -------------------------------------------------------------------- */
    char **papszRPCMD = poSrcDS->GetMetadata(MD_DOMAIN_RPC);
    if( papszRPCMD != nullptr )
    {
        bool bRPCSerializedOtherWay = false;

        if( eProfile == GTiffProfile::GDALGEOTIFF )
        {
            if( !bWriteOnlyInPAMIfNeeded )
                GTiffDatasetWriteRPCTag( l_hTIFF, papszRPCMD );
            bRPCSerializedOtherWay = true;
        }

        // Write RPB file if explicitly asked, or if a non GDAL specific
        // profile is selected and RPCTXT is not asked.
        bool bRPBExplicitlyAsked =
            CPLFetchBool( l_papszCreationOptions, "RPB", false );
        bool bRPBExplicitlyDenied =
            !CPLFetchBool( l_papszCreationOptions, "RPB", true );
        if( (eProfile != GTiffProfile::GDALGEOTIFF &&
             !CPLFetchBool( l_papszCreationOptions, "RPCTXT", false ) &&
             !bRPBExplicitlyDenied )
            || bRPBExplicitlyAsked )
        {
            if( !bWriteOnlyInPAMIfNeeded )
                GDALWriteRPBFile( pszTIFFFilename, papszRPCMD );
            bRPCSerializedOtherWay = true;
        }

        if( CPLFetchBool( l_papszCreationOptions, "RPCTXT", false ) )
        {
            if( !bWriteOnlyInPAMIfNeeded )
                GDALWriteRPCTXTFile( pszTIFFFilename, papszRPCMD );
            bRPCSerializedOtherWay = true;
        }

        if( !bRPCSerializedOtherWay && bWriteOnlyInPAMIfNeeded &&
            bSrcIsGeoTIFF )
            cpl::down_cast<GTiffDataset*>(poSrcDS)->
                GDALPamDataset::SetMetadata( papszRPCMD, MD_DOMAIN_RPC );
    }
}

/************************************************************************/
/*                  IsStandardColorInterpretation()                     */
/************************************************************************/

static bool IsStandardColorInterpretation(GDALDataset* poSrcDS,
                                          uint16_t nPhotometric,
                                          char** papszCreationOptions)
{
    bool bStandardColorInterp = true;
    if( nPhotometric == PHOTOMETRIC_MINISBLACK )
    {
        for( int i = 0; i < poSrcDS->GetRasterCount(); ++i )
        {
            const GDALColorInterp eInterp =
                poSrcDS->GetRasterBand(i + 1)->GetColorInterpretation();
            if( !(eInterp == GCI_GrayIndex || eInterp == GCI_Undefined ||
                    (i > 0 && eInterp == GCI_AlphaBand)) )
            {
                bStandardColorInterp = false;
                break;
            }
        }
    }
    else if( nPhotometric == PHOTOMETRIC_PALETTE )
    {
        bStandardColorInterp =
            poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
                GCI_PaletteIndex;
    }
    else if( nPhotometric == PHOTOMETRIC_RGB )
    {
        int iStart = 0;
        if( EQUAL(CSLFetchNameValueDef(papszCreationOptions,
                                       "PHOTOMETRIC", ""), "RGB") )
        {
            iStart = 3;
            if( poSrcDS->GetRasterCount() == 4 &&
                CSLFetchNameValue(papszCreationOptions, "ALPHA") != nullptr )
            {
                iStart = 4;
            }
        }
        for( int i = iStart; i < poSrcDS->GetRasterCount(); ++i )
        {
            const GDALColorInterp eInterp =
                poSrcDS->GetRasterBand(i+1)->GetColorInterpretation();
            if( !((i == 0 && eInterp == GCI_RedBand) ||
                    (i == 1 && eInterp == GCI_GreenBand) ||
                    (i == 2 && eInterp == GCI_BlueBand) ||
                    (i >= 3 && (eInterp == GCI_Undefined ||
                                eInterp == GCI_AlphaBand))) )
            {
                bStandardColorInterp = false;
                break;
            }
        }
    }
    else if( nPhotometric == PHOTOMETRIC_YCBCR &&
             poSrcDS->GetRasterCount() == 3 )
    {
        // do nothing
    }
    else
    {
        bStandardColorInterp = false;
    }
    return bStandardColorInterp;
}

/************************************************************************/
/*                           WriteMetadata()                            */
/************************************************************************/

bool GTiffDataset::WriteMetadata( GDALDataset *poSrcDS, TIFF *l_hTIFF,
                                  bool bSrcIsGeoTIFF,
                                  GTiffProfile eProfile,
                                  const char *pszTIFFFilename,
                                  char **l_papszCreationOptions,
                                  bool bExcludeRPBandIMGFileWriting)

{
/* -------------------------------------------------------------------- */
/*      Convert all the remaining metadata into a simple XML            */
/*      format.                                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot = nullptr;
    CPLXMLNode *psTail = nullptr;

    if( bSrcIsGeoTIFF )
    {
        GTiffDataset* poSrcDSGTiff = cpl::down_cast<GTiffDataset *>(poSrcDS);
        assert(poSrcDSGTiff);
        WriteMDMetadata(
            &poSrcDSGTiff->m_oGTiffMDMD,
            l_hTIFF, &psRoot, &psTail, 0, eProfile );
    }
    else
    {
        char **papszMD = poSrcDS->GetMetadata();

        if( CSLCount(papszMD) > 0 )
        {
            GDALMultiDomainMetadata l_oMDMD;
            l_oMDMD.SetMetadata( papszMD );

            WriteMDMetadata( &l_oMDMD, l_hTIFF, &psRoot, &psTail,
                             0, eProfile );
        }
    }

    if( !bExcludeRPBandIMGFileWriting )
    {
        WriteRPC(poSrcDS, l_hTIFF, bSrcIsGeoTIFF,
                 eProfile, pszTIFFFilename,
                 l_papszCreationOptions);

/* -------------------------------------------------------------------- */
/*      Handle metadata data written to an IMD file.                    */
/* -------------------------------------------------------------------- */
        char **papszIMDMD = poSrcDS->GetMetadata(MD_DOMAIN_IMD);
        if( papszIMDMD != nullptr )
        {
            GDALWriteIMDFile( pszTIFFFilename, papszIMDMD );
        }
    }

    uint16_t nPhotometric = 0;
    if( !TIFFGetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, &(nPhotometric) ) )
        nPhotometric = PHOTOMETRIC_MINISBLACK;

    const bool bStandardColorInterp =
        IsStandardColorInterpretation(poSrcDS, nPhotometric,
                                      l_papszCreationOptions);

/* -------------------------------------------------------------------- */
/*      We also need to address band specific metadata, and special     */
/*      "role" metadata.                                                */
/* -------------------------------------------------------------------- */
    for( int nBand = 1; nBand <= poSrcDS->GetRasterCount(); ++nBand )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( nBand );

        if( bSrcIsGeoTIFF )
        {
            GTiffRasterBand* poSrcBandGTiff = cpl::down_cast<GTiffRasterBand *>(poBand);
            assert(poSrcBandGTiff);
            WriteMDMetadata(
                &poSrcBandGTiff->m_oGTiffMDMD,
                l_hTIFF, &psRoot, &psTail, nBand, eProfile );
        }
        else
        {
            char **papszMD = poBand->GetMetadata();

            if( CSLCount(papszMD) > 0 )
            {
                GDALMultiDomainMetadata l_oMDMD;
                l_oMDMD.SetMetadata( papszMD );

                WriteMDMetadata( &l_oMDMD, l_hTIFF, &psRoot, &psTail, nBand,
                                 eProfile );
            }
        }

        const double dfOffset = poBand->GetOffset();
        const double dfScale = poBand->GetScale();
        bool bGeoTIFFScaleOffsetInZ = false;
        double adfGeoTransform[6];
        // Check if we have already encoded scale/offset in the GeoTIFF tags
        if( poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None &&
            adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0
            && adfGeoTransform[5] < 0.0 &&
            poSrcDS->GetSpatialRef() &&
            poSrcDS->GetSpatialRef()->IsVertical() &&
            poSrcDS->GetRasterCount() == 1 )
        {
            bGeoTIFFScaleOffsetInZ = true;
        }

        if( (dfOffset != 0.0 || dfScale != 1.0) && !bGeoTIFFScaleOffsetInZ )
        {
            char szValue[128] = {};

            CPLsnprintf( szValue, sizeof(szValue), "%.18g", dfOffset );
            AppendMetadataItem( &psRoot, &psTail, "OFFSET", szValue, nBand,
                                "offset", "" );
            CPLsnprintf( szValue, sizeof(szValue), "%.18g", dfScale );
            AppendMetadataItem( &psRoot, &psTail, "SCALE", szValue, nBand,
                                "scale", "" );
        }

        const char* pszUnitType = poBand->GetUnitType();
        if( pszUnitType != nullptr && pszUnitType[0] != '\0' )
        {
            bool bWriteUnit = true;
            auto poSRS = poSrcDS->GetSpatialRef();
            if( poSRS && poSRS->IsCompound() )
            {
                const char* pszVertUnit = nullptr;
                poSRS->GetTargetLinearUnits("COMPD_CS|VERT_CS", &pszVertUnit);
                if( pszVertUnit && EQUAL(pszVertUnit, pszUnitType) )
                {
                    bWriteUnit = false;
                }
            }
            if( bWriteUnit )
            {
                AppendMetadataItem( &psRoot, &psTail, "UNITTYPE",
                                    pszUnitType, nBand,
                                    "unittype", "" );
            }
        }

        if( strlen(poBand->GetDescription()) > 0 )
        {
            AppendMetadataItem( &psRoot, &psTail, "DESCRIPTION",
                                poBand->GetDescription(), nBand,
                                "description", "" );
        }

        if( !bStandardColorInterp &&
            !(nBand <= 3 &&  EQUAL(CSLFetchNameValueDef(
                l_papszCreationOptions, "PHOTOMETRIC", ""), "RGB") ) )
        {
            AppendMetadataItem( &psRoot, &psTail, "COLORINTERP",
                                GDALGetColorInterpretationName(
                                    poBand->GetColorInterpretation()),
                                nBand,
                                "colorinterp", "" );
        }
    }

    const char* pszTilingSchemeName =
        CSLFetchNameValue(l_papszCreationOptions, "@TILING_SCHEME_NAME");
    if( pszTilingSchemeName )
    {
        AppendMetadataItem( &psRoot, &psTail,
                            "NAME", pszTilingSchemeName,
                            0, nullptr, "TILING_SCHEME" );

        const char* pszZoomLevel = CSLFetchNameValue(
            l_papszCreationOptions, "@TILING_SCHEME_ZOOM_LEVEL");
        if( pszZoomLevel )
        {
            AppendMetadataItem( &psRoot, &psTail,
                                "ZOOM_LEVEL", pszZoomLevel,
                                0, nullptr, "TILING_SCHEME" );
        }

        const char* pszAlignedLevels = CSLFetchNameValue(
            l_papszCreationOptions, "@TILING_SCHEME_ALIGNED_LEVELS");
        if( pszAlignedLevels )
        {
            AppendMetadataItem( &psRoot, &psTail,
                                "ALIGNED_LEVELS", pszAlignedLevels,
                                0, nullptr, "TILING_SCHEME" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write out the generic XML metadata if there is any.             */
/* -------------------------------------------------------------------- */
    if( psRoot != nullptr )
    {
        bool bRet = true;

        if( eProfile == GTiffProfile::GDALGEOTIFF )
        {
            char *pszXML_MD = CPLSerializeXMLTree( psRoot );
            if( strlen(pszXML_MD) > 32000 )
            {
                if( bSrcIsGeoTIFF )
                {
                    if( cpl::down_cast<GTiffDataset *>(
                           poSrcDS)->GetPamFlags() & GPF_DISABLED )
                    {
                        ReportError(
                            pszTIFFFilename, CE_Warning, CPLE_AppDefined,
                            "Metadata exceeding 32000 bytes cannot be written "
                            "into GeoTIFF." );
                    }
                    else
                    {
                        cpl::down_cast<GTiffDataset *>(poSrcDS)->
                            PushMetadataToPam();
                        ReportError(
                            pszTIFFFilename, CE_Warning, CPLE_AppDefined,
                            "Metadata exceeding 32000 bytes cannot be written "
                            "into GeoTIFF. Transferred to PAM instead." );
                    }
                }
                else
                {
                    bRet = false;
                }
            }
            else
            {
                TIFFSetField( l_hTIFF, TIFFTAG_GDAL_METADATA, pszXML_MD );
            }
            CPLFree( pszXML_MD );
        }
        else
        {
            if( bSrcIsGeoTIFF )
                cpl::down_cast<GTiffDataset *>(poSrcDS)->PushMetadataToPam();
            else
                bRet = false;
        }

        CPLDestroyXMLNode( psRoot );

        return bRet;
    }

    // If we have no more metadata but it existed before,
    // remove the GDAL_METADATA tag.
    if( eProfile == GTiffProfile::GDALGEOTIFF )
    {
        char* pszText = nullptr;
        if( TIFFGetField( l_hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) )
        {
            TIFFUnsetField( l_hTIFF, TIFFTAG_GDAL_METADATA );
        }
    }

    return true;
}

/************************************************************************/
/*                         PushMetadataToPam()                          */
/*                                                                      */
/*      When producing a strict profile TIFF or if our aggregate        */
/*      metadata is too big for a single tiff tag we may end up         */
/*      needing to write it via the PAM mechanisms.  This method        */
/*      copies all the appropriate metadata into the PAM level          */
/*      metadata object but with special care to avoid copying          */
/*      metadata handled in other ways in TIFF format.                  */
/************************************************************************/

void GTiffDataset::PushMetadataToPam()

{
    if( GetPamFlags() & GPF_DISABLED )
        return;

    const bool bStandardColorInterp =
        IsStandardColorInterpretation(this, m_nPhotometric, m_papszCreationOptions);

    for( int nBand = 0; nBand <= GetRasterCount(); ++nBand )
    {
        GDALMultiDomainMetadata *poSrcMDMD = nullptr;
        GTiffRasterBand *poBand = nullptr;

        if( nBand == 0 )
        {
            poSrcMDMD = &(this->m_oGTiffMDMD);
        }
        else
        {
            poBand = cpl::down_cast<GTiffRasterBand *>(GetRasterBand(nBand));
            poSrcMDMD = &(poBand->m_oGTiffMDMD);
        }

/* -------------------------------------------------------------------- */
/*      Loop over the available domains.                                */
/* -------------------------------------------------------------------- */
        char **papszDomainList = poSrcMDMD->GetDomainList();
        for( int iDomain = 0;
             papszDomainList && papszDomainList[iDomain];
             ++iDomain )
        {
            char **papszMD = poSrcMDMD->GetMetadata( papszDomainList[iDomain] );

            if( EQUAL(papszDomainList[iDomain],MD_DOMAIN_RPC)
                || EQUAL(papszDomainList[iDomain],MD_DOMAIN_IMD)
                || EQUAL(papszDomainList[iDomain],"_temporary_")
                || EQUAL(papszDomainList[iDomain],"IMAGE_STRUCTURE")
                || EQUAL(papszDomainList[iDomain],"COLOR_PROFILE") )
                continue;

            papszMD = CSLDuplicate(papszMD);

            for( int i = CSLCount(papszMD)-1; i >= 0; --i )
            {
                if( STARTS_WITH_CI(papszMD[i], "TIFFTAG_")
                    || EQUALN(papszMD[i],GDALMD_AREA_OR_POINT,
                              strlen(GDALMD_AREA_OR_POINT)) )
                    papszMD = CSLRemoveStrings( papszMD, i, 1, nullptr );
            }

            if( nBand == 0 )
                GDALPamDataset::SetMetadata( papszMD, papszDomainList[iDomain]);
            else
                poBand->
                    GDALPamRasterBand::SetMetadata( papszMD,
                                                    papszDomainList[iDomain]);

            CSLDestroy( papszMD );
        }

/* -------------------------------------------------------------------- */
/*      Handle some "special domain" stuff.                             */
/* -------------------------------------------------------------------- */
        if( poBand != nullptr )
        {
            poBand->GDALPamRasterBand::SetOffset( poBand->GetOffset() );
            poBand->GDALPamRasterBand::SetScale( poBand->GetScale() );
            poBand->GDALPamRasterBand::SetUnitType( poBand->GetUnitType() );
            poBand->
                GDALPamRasterBand::SetDescription( poBand->GetDescription() );
            if( !bStandardColorInterp )
            {
                poBand->GDALPamRasterBand::SetColorInterpretation(
                                        poBand->GetColorInterpretation() );
            }
        }
    }
    MarkPamDirty();
}

/************************************************************************/
/*                     GTiffDatasetWriteRPCTag()                        */
/*                                                                      */
/*      Format a TAG according to:                                      */
/*                                                                      */
/*      http://geotiff.maptools.org/rpc_prop.html                       */
/************************************************************************/

void GTiffDatasetWriteRPCTag( TIFF *hTIFF, char **papszRPCMD )

{
    GDALRPCInfoV2 sRPC;

    if( !GDALExtractRPCInfoV2( papszRPCMD, &sRPC ) )
        return;

    double adfRPCTag[92] = {};
    adfRPCTag[0] = sRPC.dfERR_BIAS;  // Error Bias
    adfRPCTag[1] = sRPC.dfERR_RAND;  // Error Random

    adfRPCTag[2] = sRPC.dfLINE_OFF;
    adfRPCTag[3] = sRPC.dfSAMP_OFF;
    adfRPCTag[4] = sRPC.dfLAT_OFF;
    adfRPCTag[5] = sRPC.dfLONG_OFF;
    adfRPCTag[6] = sRPC.dfHEIGHT_OFF;
    adfRPCTag[7] = sRPC.dfLINE_SCALE;
    adfRPCTag[8] = sRPC.dfSAMP_SCALE;
    adfRPCTag[9] = sRPC.dfLAT_SCALE;
    adfRPCTag[10] = sRPC.dfLONG_SCALE;
    adfRPCTag[11] = sRPC.dfHEIGHT_SCALE;

    memcpy( adfRPCTag + 12, sRPC.adfLINE_NUM_COEFF, sizeof(double) * 20 );
    memcpy( adfRPCTag + 32, sRPC.adfLINE_DEN_COEFF, sizeof(double) * 20 );
    memcpy( adfRPCTag + 52, sRPC.adfSAMP_NUM_COEFF, sizeof(double) * 20 );
    memcpy( adfRPCTag + 72, sRPC.adfSAMP_DEN_COEFF, sizeof(double) * 20 );

    TIFFSetField( hTIFF, TIFFTAG_RPCCOEFFICIENT, 92, adfRPCTag );
}

/************************************************************************/
/*                             ReadRPCTag()                             */
/*                                                                      */
/*      Format a TAG according to:                                      */
/*                                                                      */
/*      http://geotiff.maptools.org/rpc_prop.html                       */
/************************************************************************/

char** GTiffDatasetReadRPCTag(TIFF* hTIFF)

{
    double *padfRPCTag = nullptr;
    uint16_t nCount;

    if( !TIFFGetField( hTIFF, TIFFTAG_RPCCOEFFICIENT, &nCount, &padfRPCTag )
        || nCount != 92 )
        return nullptr;

    CPLStringList asMD;
    asMD.SetNameValue(RPC_ERR_BIAS, CPLOPrintf("%.15g", padfRPCTag[0]));
    asMD.SetNameValue(RPC_ERR_RAND, CPLOPrintf("%.15g", padfRPCTag[1]));
    asMD.SetNameValue(RPC_LINE_OFF, CPLOPrintf("%.15g", padfRPCTag[2]));
    asMD.SetNameValue(RPC_SAMP_OFF, CPLOPrintf("%.15g", padfRPCTag[3]));
    asMD.SetNameValue(RPC_LAT_OFF, CPLOPrintf("%.15g", padfRPCTag[4]));
    asMD.SetNameValue(RPC_LONG_OFF, CPLOPrintf("%.15g", padfRPCTag[5]));
    asMD.SetNameValue(RPC_HEIGHT_OFF, CPLOPrintf("%.15g", padfRPCTag[6]));
    asMD.SetNameValue(RPC_LINE_SCALE, CPLOPrintf("%.15g", padfRPCTag[7]));
    asMD.SetNameValue(RPC_SAMP_SCALE, CPLOPrintf("%.15g", padfRPCTag[8]));
    asMD.SetNameValue(RPC_LAT_SCALE, CPLOPrintf("%.15g", padfRPCTag[9]));
    asMD.SetNameValue(RPC_LONG_SCALE, CPLOPrintf("%.15g", padfRPCTag[10]));
    asMD.SetNameValue(RPC_HEIGHT_SCALE, CPLOPrintf("%.15g", padfRPCTag[11]));

    CPLString osField;
    CPLString osMultiField;

    for( int i = 0; i < 20; ++i )
    {
        osField.Printf( "%.15g", padfRPCTag[12+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue(RPC_LINE_NUM_COEFF, osMultiField );

    for( int i = 0; i < 20; ++i )
    {
        osField.Printf( "%.15g", padfRPCTag[32+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue( RPC_LINE_DEN_COEFF, osMultiField );

    for( int i = 0; i < 20; ++i )
    {
        osField.Printf( "%.15g", padfRPCTag[52+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue( RPC_SAMP_NUM_COEFF, osMultiField );

    for( int i = 0; i < 20; ++i )
    {
        osField.Printf( "%.15g", padfRPCTag[72+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue( RPC_SAMP_DEN_COEFF, osMultiField );

    return asMD.StealList();
}

/************************************************************************/
/*                  GTiffFormatGDALNoDataTagValue()                     */
/************************************************************************/

CPLString GTiffFormatGDALNoDataTagValue( double dfNoData )
{
    CPLString osVal;
    if( CPLIsNan(dfNoData) )
        osVal = "nan";
    else
        osVal.Printf("%.18g", dfNoData);
    return osVal;
}

/************************************************************************/
/*                         WriteNoDataValue()                           */
/************************************************************************/

void GTiffDataset::WriteNoDataValue( TIFF *l_hTIFF, double dfNoData )

{
    CPLString osVal( GTiffFormatGDALNoDataTagValue(dfNoData) );
    TIFFSetField( l_hTIFF, TIFFTAG_GDAL_NODATA, osVal.c_str() );
}

/************************************************************************/
/*                         UnsetNoDataValue()                           */
/************************************************************************/

void GTiffDataset::UnsetNoDataValue( TIFF *l_hTIFF )

{
    TIFFUnsetField( l_hTIFF, TIFFTAG_GDAL_NODATA );
}

/************************************************************************/
/*                            SetDirectory()                            */
/************************************************************************/

bool GTiffDataset::SetDirectory()

{
    Crystalize();

    if( TIFFCurrentDirOffset(m_hTIFF) == m_nDirOffset )
    {
        return true;
    }

    const int nSetDirResult = TIFFSetSubDirectory( m_hTIFF, m_nDirOffset );
    if( !nSetDirResult )
        return false;

    RestoreVolatileParameters( m_hTIFF );

    return true;
}

/************************************************************************/
/*                     GTiffSetDeflateSubCodec()                        */
/************************************************************************/

static void GTiffSetDeflateSubCodec(TIFF* hTIFF)
{
    (void)hTIFF;

#if defined(TIFFTAG_DEFLATE_SUBCODEC) && defined(LIBDEFLATE_SUPPORT)
    // Mostly for strict reproducibility purposes
    if( EQUAL(CPLGetConfigOption("GDAL_TIFF_DEFLATE_SUBCODEC", ""), "ZLIB") )
    {
        TIFFSetField(hTIFF, TIFFTAG_DEFLATE_SUBCODEC,
                     DEFLATE_SUBCODEC_ZLIB);
    }
#endif
}

/************************************************************************/
/*                     RestoreVolatileParameters()                      */
/************************************************************************/

void GTiffDataset::RestoreVolatileParameters(TIFF* hTIFF)
{

/* -------------------------------------------------------------------- */
/*      YCbCr JPEG compressed images should be translated on the fly    */
/*      to RGB by libtiff/libjpeg unless specifically requested         */
/*      otherwise.                                                      */
/* -------------------------------------------------------------------- */
    if( m_nCompression == COMPRESSION_JPEG
        && m_nPhotometric == PHOTOMETRIC_YCBCR
        && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                              "YES") ) )
    {
        int nColorMode = JPEGCOLORMODE_RAW;  // Initialize to 0;

        TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode );
        if( nColorMode != JPEGCOLORMODE_RGB )
        {
            TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }
    }

    if( m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        m_nCompression == COMPRESSION_LERC )
    {
        GTiffSetDeflateSubCodec(hTIFF);
    }

/* -------------------------------------------------------------------- */
/*      Propagate any quality settings.                                 */
/* -------------------------------------------------------------------- */
    if( GetAccess() == GA_Update )
    {
        // Now, reset zip and jpeg quality.
        if(m_nJpegQuality > 0 && m_nCompression == COMPRESSION_JPEG)
        {
#ifdef DEBUG_VERBOSE
            CPLDebug( "GTiff", "Propagate JPEG_QUALITY(%d) in SetDirectory()",
                      m_nJpegQuality );
#endif
            TIFFSetField(hTIFF, TIFFTAG_JPEGQUALITY, m_nJpegQuality);
        }
        if(m_nJpegTablesMode >= 0 && m_nCompression == COMPRESSION_JPEG)
            TIFFSetField(hTIFF, TIFFTAG_JPEGTABLESMODE, m_nJpegTablesMode);
        if(m_nZLevel > 0 && (m_nCompression == COMPRESSION_ADOBE_DEFLATE ||
                           m_nCompression == COMPRESSION_LERC) )
            TIFFSetField(hTIFF, TIFFTAG_ZIPQUALITY, m_nZLevel);
        if(m_nLZMAPreset > 0 && m_nCompression == COMPRESSION_LZMA)
            TIFFSetField(hTIFF, TIFFTAG_LZMAPRESET, m_nLZMAPreset);
        if( m_nZSTDLevel > 0 && (m_nCompression == COMPRESSION_ZSTD ||
                               m_nCompression == COMPRESSION_LERC) )
            TIFFSetField(hTIFF, TIFFTAG_ZSTD_LEVEL, m_nZSTDLevel);
        if( m_nCompression == COMPRESSION_LERC )
        {
            TIFFSetField(hTIFF, TIFFTAG_LERC_MAXZERROR, m_dfMaxZError);
        }
        if( m_nWebPLevel > 0 && m_nCompression == COMPRESSION_WEBP)
            TIFFSetField(hTIFF, TIFFTAG_WEBP_LEVEL, m_nWebPLevel);
        if( m_bWebPLossless && m_nCompression == COMPRESSION_WEBP)
            TIFFSetField(hTIFF, TIFFTAG_WEBP_LOSSLESS, 1);
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int GTiffDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    const char *pszFilename = poOpenInfo->pszFilename;
    if( STARTS_WITH_CI(pszFilename, "GTIFF_RAW:") )
    {
        pszFilename += strlen("GTIFF_RAW:");
        GDALOpenInfo oOpenInfo( pszFilename, poOpenInfo->eAccess );
        return Identify(&oOpenInfo);
    }

/* -------------------------------------------------------------------- */
/*      We have a special hook for handling opening a specific          */
/*      directory of a TIFF file.                                       */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszFilename, "GTIFF_DIR:") )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      First we check to see if the file has the expected header       */
/*      bytes.                                                          */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes < 2 )
        return FALSE;

    if( (poOpenInfo->pabyHeader[0] != 'I' || poOpenInfo->pabyHeader[1] != 'I')
        && (poOpenInfo->pabyHeader[0] != 'M'
        || poOpenInfo->pabyHeader[1] != 'M'))
        return FALSE;

    if( (poOpenInfo->pabyHeader[2] != 0x2A || poOpenInfo->pabyHeader[3] != 0)
        && (poOpenInfo->pabyHeader[3] != 0x2A || poOpenInfo->pabyHeader[2] != 0)
        && (poOpenInfo->pabyHeader[2] != 0x2B || poOpenInfo->pabyHeader[3] != 0)
        && (poOpenInfo->pabyHeader[3] != 0x2B ||
            poOpenInfo->pabyHeader[2] != 0))
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                          GTIFFExtendMemoryFile()                     */
/************************************************************************/

static bool GTIFFExtendMemoryFile( const CPLString& osTmpFilename,
                                   VSILFILE* fpTemp,
                                   VSILFILE* fpL,
                                   int nNewLength,
                                   GByte*& pabyBuffer,
                                   vsi_l_offset& nDataLength )
{
    if( nNewLength <= static_cast<int>(nDataLength) )
        return true;
    if( VSIFSeekL(fpTemp, nNewLength - 1, SEEK_SET) != 0 )
        return false;
    char ch = 0;
    if( VSIFWriteL(&ch, 1, 1, fpTemp) != 1 )
        return false;
    const int nOldDataLength = static_cast<int>(nDataLength);
    pabyBuffer = static_cast<GByte*>(
        VSIGetMemFileBuffer( osTmpFilename, &nDataLength, FALSE) );
    const int nToRead = nNewLength - nOldDataLength;
    const int nRead = static_cast<int>(
        VSIFReadL( pabyBuffer + nOldDataLength, 1, nToRead, fpL) );
    if( nRead != nToRead )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Needed to read %d bytes. Only %d got", nToRead, nRead);
        return false;
    }
    return true;
}

/************************************************************************/
/*                         GTIFFMakeBufferedStream()                    */
/************************************************************************/

static bool GTIFFMakeBufferedStream(GDALOpenInfo* poOpenInfo)
{
    CPLString osTmpFilename;
    static int nCounter = 0;
    osTmpFilename.Printf("/vsimem/stream_%d.tif", ++nCounter);
    VSILFILE* fpTemp = VSIFOpenL(osTmpFilename, "wb+");
    if( fpTemp == nullptr )
        return false;
    // The seek is needed for /vsistdin/ that has some rewind capabilities.
    if( VSIFSeekL(poOpenInfo->fpL, poOpenInfo->nHeaderBytes, SEEK_SET) != 0 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
        return false;
    }
    CPLAssert( static_cast<int>( VSIFTellL(poOpenInfo->fpL) ) ==
               poOpenInfo->nHeaderBytes );
    if( VSIFWriteL(poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes,
                   1, fpTemp) != 1 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
        return false;
    }
    vsi_l_offset nDataLength = 0;
    GByte* pabyBuffer =
        static_cast<GByte*>(
            VSIGetMemFileBuffer( osTmpFilename, &nDataLength, FALSE) );
    const bool bLittleEndian = (pabyBuffer[0] == 'I');
#if CPL_IS_LSB
    const bool bSwap = !bLittleEndian;
#else
    const bool bSwap = bLittleEndian;
#endif
    const bool bBigTIFF = pabyBuffer[2] == 43 || pabyBuffer[3] == 43;
    vsi_l_offset nMaxOffset = 0;
    if( bBigTIFF )
    {
        GUInt64 nTmp = 0;
        memcpy(&nTmp, pabyBuffer + 8, 8);
        if( bSwap ) CPL_SWAP64PTR(&nTmp);
        if( nTmp != 16 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "IFD start should be at offset 16 for a streamed BigTIFF");
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        memcpy(&nTmp, pabyBuffer + 16, 8);
        if( bSwap ) CPL_SWAP64PTR(&nTmp);
        if( nTmp > 1024 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many tags : " CPL_FRMT_GIB, nTmp);
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        const int nTags = static_cast<int>(nTmp);
        const int nSpaceForTags = nTags * 20;
        if( !GTIFFExtendMemoryFile(osTmpFilename, fpTemp, poOpenInfo->fpL,
                                    24 + nSpaceForTags,
                                    pabyBuffer, nDataLength) )
        {
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        nMaxOffset = 24 + nSpaceForTags + 8;
        for( int i = 0; i < nTags; ++i )
        {
            GUInt16 nTmp16 = 0;
            memcpy(&nTmp16, pabyBuffer + 24 + i * 20, 2);
            if( bSwap ) CPL_SWAP16PTR(&nTmp16);
            const int nTag = nTmp16;
            memcpy(&nTmp16, pabyBuffer + 24 + i * 20 + 2, 2);
            if( bSwap ) CPL_SWAP16PTR(&nTmp16);
            const int nDataType = nTmp16;
            memcpy(&nTmp, pabyBuffer + 24 + i * 20 + 4, 8);
            if( bSwap ) CPL_SWAP64PTR(&nTmp);
            if( nTmp >= 16 * 1024 * 1024 )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "Too many elements for tag %d : " CPL_FRMT_GUIB,
                    nTag, nTmp );
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
                VSIUnlink(osTmpFilename);
                return false;
            }
            const GUInt32 nCount = static_cast<GUInt32>(nTmp);
            const GUInt32 nTagSize =
                TIFFDataWidth(static_cast<TIFFDataType>(nDataType)) * nCount;
            if( nTagSize > 8 )
            {
                memcpy(&nTmp, pabyBuffer + 24 + i * 20 + 12, 8);
                if( bSwap ) CPL_SWAP64PTR(&nTmp);
                if( nTmp > GUINT64_MAX - nTagSize )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Overflow with tag %d", nTag);
                    CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
                    VSIUnlink(osTmpFilename);
                    return false;
                }
                if( static_cast<vsi_l_offset>(nTmp + nTagSize) > nMaxOffset )
                    nMaxOffset = nTmp + nTagSize;
            }
        }
    }
    else
    {
        GUInt32 nTmp = 0;
        memcpy(&nTmp, pabyBuffer + 4, 4);
        if( bSwap ) CPL_SWAP32PTR(&nTmp);
        if( nTmp != 8 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "IFD start should be at offset 8 for a streamed TIFF");
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        GUInt16 nTmp16 = 0;
        memcpy(&nTmp16, pabyBuffer + 8, 2);
        if( bSwap ) CPL_SWAP16PTR(&nTmp16);
        if( nTmp16 > 1024 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many tags : %d", nTmp16);
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        const int nTags = nTmp16;
        const int nSpaceForTags = nTags * 12;
        if( !GTIFFExtendMemoryFile(osTmpFilename, fpTemp, poOpenInfo->fpL,
                                   10 + nSpaceForTags,
                                   pabyBuffer, nDataLength) )
        {
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
            VSIUnlink(osTmpFilename);
            return false;
        }
        nMaxOffset = 10 + nSpaceForTags + 4;
        for( int i = 0; i < nTags; ++i )
        {
            memcpy(&nTmp16, pabyBuffer + 10 + i * 12, 2);
            if( bSwap ) CPL_SWAP16PTR(&nTmp16);
            const int nTag = nTmp16;
            memcpy(&nTmp16, pabyBuffer + 10 + i * 12 + 2, 2);
            if( bSwap ) CPL_SWAP16PTR(&nTmp16);
            const int nDataType = nTmp16;
            memcpy(&nTmp, pabyBuffer + 10 + i * 12 + 4, 4);
            if( bSwap ) CPL_SWAP32PTR(&nTmp);
            if( nTmp >= 16 * 1024 * 1024 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Too many elements for tag %d : %u", nTag, nTmp);
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
                VSIUnlink(osTmpFilename);
                return false;
            }
            const GUInt32 nCount = nTmp;
            const GUInt32 nTagSize =
                TIFFDataWidth(static_cast<TIFFDataType>(nDataType)) * nCount;
            if( nTagSize > 4 )
            {
                memcpy(&nTmp, pabyBuffer + 10 + i * 12 + 8, 4);
                if( bSwap ) CPL_SWAP32PTR(&nTmp);
                if( nTmp > static_cast<GUInt32>(UINT_MAX - nTagSize) )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Overflow with tag %d", nTag);
                    CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
                    VSIUnlink(osTmpFilename);
                    return false;
                }
                if( nTmp + nTagSize > nMaxOffset )
                    nMaxOffset = nTmp + nTagSize;
            }
        }
    }
    if( nMaxOffset > 10 * 1024 * 1024 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
        VSIUnlink(osTmpFilename);
        return false;
    }
    if( !GTIFFExtendMemoryFile(
           osTmpFilename, fpTemp, poOpenInfo->fpL,
           static_cast<int>(nMaxOffset), pabyBuffer, nDataLength) )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
        VSIUnlink(osTmpFilename);
        return false;
    }
    CPLAssert(nDataLength == VSIFTellL(poOpenInfo->fpL));
    poOpenInfo->fpL = reinterpret_cast<VSILFILE *>(
        VSICreateBufferedReaderHandle(
            reinterpret_cast<VSIVirtualHandle*>(poOpenInfo->fpL),
            pabyBuffer,
            static_cast<vsi_l_offset>(INT_MAX) << 32 ) );
    if( VSIFCloseL(fpTemp) != 0 )
        return false;
    VSIUnlink(osTmpFilename);

    return true;
}

/************************************************************************/
/*                       AssociateExternalMask()                        */
/************************************************************************/

// Used by GTIFFBuildOverviewsEx() for the COG driver
bool GTiffDataset::AssociateExternalMask()
{
    if( m_poMaskExtOvrDS->GetRasterBand(1)->GetOverviewCount() !=
        GetRasterBand(1)->GetOverviewCount() )
        return false;
    if( m_papoOverviewDS == nullptr )
        return false;
    if( m_poMaskDS )
        return false;
    if( m_poMaskExtOvrDS->GetRasterXSize() != nRasterXSize ||
        m_poMaskExtOvrDS->GetRasterYSize() != nRasterYSize )
        return false;
    m_poExternalMaskDS = m_poMaskExtOvrDS.get();
    for(int i = 0; i < m_nOverviewCount; i++ )
    {
        if( m_papoOverviewDS[i]->m_poMaskDS )
            return false;
        m_papoOverviewDS[i]->m_poExternalMaskDS =
            m_poMaskExtOvrDS->GetRasterBand(1)->GetOverview(i)->GetDataset();
        if( !m_papoOverviewDS[i]->m_poExternalMaskDS)
            return false;
        auto poOvrBand = m_papoOverviewDS[i]->GetRasterBand(1);
        if( m_papoOverviewDS[i]->m_poExternalMaskDS->GetRasterXSize() !=
                poOvrBand->GetXSize() ||
            m_papoOverviewDS[i]->m_poExternalMaskDS->GetRasterYSize() !=
                poOvrBand->GetYSize() )
            return false;
    }
    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTiffDataset::Open( GDALOpenInfo * poOpenInfo )

{
    const char *pszFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Check if it looks like a TIFF file.                             */
/* -------------------------------------------------------------------- */
    if( !Identify(poOpenInfo) )
        return nullptr;

    bool bAllowRGBAInterface = true;
    if( STARTS_WITH_CI(pszFilename, "GTIFF_RAW:") )
    {
        bAllowRGBAInterface = false;
        pszFilename += strlen("GTIFF_RAW:");
    }

/* -------------------------------------------------------------------- */
/*      We have a special hook for handling opening a specific          */
/*      directory of a TIFF file.                                       */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszFilename, "GTIFF_DIR:") )
        return OpenDir( poOpenInfo );

    if( !GTiffOneTimeInit() )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    // Disable strip chop for now.
    bool bStreaming = false;
    const char* pszReadStreaming =
        CPLGetConfigOption("TIFF_READ_STREAMING", nullptr);
    if( poOpenInfo->fpL == nullptr )
    {
        poOpenInfo->fpL =
            VSIFOpenL( pszFilename,
                       poOpenInfo->eAccess == GA_ReadOnly ? "rb" : "r+b" );
        if( poOpenInfo->fpL == nullptr )
            return nullptr;
    }
    else if( !(pszReadStreaming && !CPLTestBool(pszReadStreaming)) &&
             poOpenInfo->nHeaderBytes >= 24 &&
             // A pipe has no seeking capability, so its position is 0 despite
             // having read bytes.
             (static_cast<int>( VSIFTellL(poOpenInfo->fpL) ) ==
              poOpenInfo->nHeaderBytes ||
              strcmp(pszFilename, "/vsistdin/") == 0 ||
              // STARTS_WITH(pszFilename, "/vsicurl_streaming/") ||
              (pszReadStreaming && CPLTestBool(pszReadStreaming))) )
    {
        bStreaming = true;
        if( !GTIFFMakeBufferedStream(poOpenInfo) )
            return nullptr;
    }

    // Store errors/warnings and emit them later.
    std::vector<CPLErrorHandlerAccumulatorStruct> aoErrors;
    CPLInstallErrorHandlerAccumulator(aoErrors);
    CPLSetCurrentErrorHandlerCatchDebug( FALSE );
    const bool bDeferStrileLoading = CPLTestBool(
        CPLGetConfigOption("GTIFF_USE_DEFER_STRILE_LOADING", "YES"));
    TIFF *l_hTIFF =
        VSI_TIFFOpen( pszFilename,
                      poOpenInfo->eAccess == GA_ReadOnly ?
                        ((bStreaming || !bDeferStrileLoading) ? "r" : "rDO") :
                        (!bDeferStrileLoading ? "r+" : "r+D"),
                      poOpenInfo->fpL );
    CPLUninstallErrorHandlerAccumulator();

    // Now emit errors and change their criticality if needed
    // We only emit failures if we didn't manage to open the file.
    // Otherwise it makes Python bindings unhappy (#5616).
    for( size_t iError = 0; iError < aoErrors.size(); ++iError )
    {
        ReportError( pszFilename,
                  (l_hTIFF == nullptr && aoErrors[iError].type == CE_Failure) ?
                  CE_Failure : CE_Warning,
                  aoErrors[iError].no,
                  "%s",
                  aoErrors[iError].msg.c_str() );
    }
    aoErrors.resize(0);

    if( l_hTIFF == nullptr )
        return nullptr;

    uint32_t nXSize = 0;
    TIFFGetField( l_hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    uint32_t nYSize = 0;
    TIFFGetField( l_hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );

    if( nXSize > INT_MAX || nYSize > INT_MAX )
    {
        // GDAL only supports signed 32bit dimensions.
        ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                    "Too large image size: %u x %u",
                    nXSize, nYSize);
        XTIFFClose( l_hTIFF );
        return nullptr;
    }

    uint16_t l_nCompression = 0;
    if( !TIFFGetField( l_hTIFF, TIFFTAG_COMPRESSION, &(l_nCompression) ) )
        l_nCompression = COMPRESSION_NONE;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->SetDescription( pszFilename );
    poDS->m_pszFilename = CPLStrdup(pszFilename);
    poDS->m_fpL = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    poDS->m_bStreamingIn = bStreaming;
    poDS->m_nCompression = l_nCompression;

    // Check structural metadata (for COG)
    const int nOffsetOfStructuralMetadata =
        poOpenInfo->nHeaderBytes &&
        ((poOpenInfo->pabyHeader[2] == 0x2B ||
         poOpenInfo->pabyHeader[3] == 0x2B )) ? 16 : 8;
    if( poOpenInfo->nHeaderBytes > nOffsetOfStructuralMetadata +
            static_cast<int>(strlen("GDAL_STRUCTURAL_METADATA_SIZE=")) &&
        memcmp(poOpenInfo->pabyHeader + nOffsetOfStructuralMetadata,
               "GDAL_STRUCTURAL_METADATA_SIZE=",
               strlen("GDAL_STRUCTURAL_METADATA_SIZE=")) == 0 )
    {
        const char* pszStructuralMD = reinterpret_cast<const char*>(
            poOpenInfo->pabyHeader + nOffsetOfStructuralMetadata);
        poDS->m_bLayoutIFDSBeforeData = strstr(pszStructuralMD,
                            "LAYOUT=IFDS_BEFORE_DATA") != nullptr;
        poDS->m_bBlockOrderRowMajor = strstr(pszStructuralMD,
                            "BLOCK_ORDER=ROW_MAJOR") != nullptr;
        poDS->m_bLeaderSizeAsUInt4 = strstr(pszStructuralMD,
                            "BLOCK_LEADER=SIZE_AS_UINT4") != nullptr;
        poDS->m_bTrailerRepeatedLast4BytesRepeated = strstr(pszStructuralMD,
                            "BLOCK_TRAILER=LAST_4_BYTES_REPEATED") != nullptr;
        poDS->m_bMaskInterleavedWithImagery = strstr(pszStructuralMD,
                            "MASK_INTERLEAVED_WITH_IMAGERY=YES") != nullptr;
        poDS->m_bKnownIncompatibleEdition = strstr(pszStructuralMD,
                            "KNOWN_INCOMPATIBLE_EDITION=YES") != nullptr;
        if( poDS->m_bKnownIncompatibleEdition )
        {
            poDS->ReportError(CE_Warning, CPLE_AppDefined,
                     "This file used to have optimizations in its layout, "
                     "but those have been, at least partly, invalidated by "
                     "later changes");
        }
        else if( poDS->m_bLayoutIFDSBeforeData &&
                 poDS->m_bBlockOrderRowMajor &&
                 poDS->m_bLeaderSizeAsUInt4 &&
                 poDS->m_bTrailerRepeatedLast4BytesRepeated )
        {
            poDS->m_oGTiffMDMD.SetMetadataItem("LAYOUT", "COG", "IMAGE_STRUCTURE");
        }
    }

    // In the case of GDAL_DISABLE_READDIR_ON_OPEN = NO / EMPTY_DIR
    if( poOpenInfo->AreSiblingFilesLoaded() &&
        CSLCount( poOpenInfo->GetSiblingFiles() ) <= 1 )
    {
        poDS->oOvManager.TransferSiblingFiles( CSLDuplicate(
                                            poOpenInfo->GetSiblingFiles() ) );
        poDS->m_bHasGotSiblingFiles = true;
    }

    if( poDS->OpenOffset( l_hTIFF,
                          TIFFCurrentDirOffset(l_hTIFF),
                          poOpenInfo->eAccess,
                          bAllowRGBAInterface, true) != CE_None )
    {
        delete poDS;
        return nullptr;
    }

    // Do we want blocks that are set to zero and that haven't yet being
    // allocated as tile/strip to remain implicit?
    if( CPLFetchBool( poOpenInfo->papszOpenOptions, "SPARSE_OK", false ) )
        poDS->m_bWriteEmptyTiles = false;

    if( poOpenInfo->eAccess == GA_Update )
    {
        poDS->InitCreationOrOpenOptions(poOpenInfo->papszOpenOptions);
    }

    poDS->m_bLoadPam = true;
    poDS->m_bColorProfileMetadataChanged = false;
    poDS->m_bMetadataChanged = false;
    poDS->m_bGeoTIFFInfoChanged = false;
    poDS->m_bNoDataChanged = false;
    poDS->m_bForceUnsetGTOrGCPs = false;
    poDS->m_bForceUnsetProjection = false;

    // Used by GTIFFBuildOverviewsEx() for the COG driver
    const char* pszMaskOverviewDS = CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                                      "MASK_OVERVIEW_DATASET");
    if( pszMaskOverviewDS )
    {
        poDS->m_poMaskExtOvrDS.reset(GDALDataset::Open(pszMaskOverviewDS,
                                            GDAL_OF_RASTER | GDAL_OF_INTERNAL));
        if( !poDS->m_poMaskExtOvrDS || !poDS->AssociateExternalMask() )
        {
            CPLDebug("GTiff",
                     "Association with external mask overview file failed");
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize info for external overviews.                         */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, pszFilename );
    if( poOpenInfo->AreSiblingFilesLoaded() )
        poDS->oOvManager.TransferSiblingFiles(
            poOpenInfo->StealSiblingFiles() );

    // For backward compatibility, in case GTIFF_POINT_GEO_IGNORE is defined
    // load georeferencing right now so as to not require it to be defined
    // at the GetGeoTransform() time.
    if( CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE", nullptr) != nullptr )
    {
        poDS->LoadGeoreferencingAndPamIfNeeded();
    }

    return poDS;
}

/************************************************************************/
/*                      GTiffDatasetSetAreaOrPointMD()                  */
/************************************************************************/

static void GTiffDatasetSetAreaOrPointMD( GTIF* hGTIF,
                                          GDALMultiDomainMetadata& m_oGTiffMDMD )
{
    // Is this a pixel-is-point dataset?
    unsigned short nRasterType = 0;

    if( GDALGTIFKeyGetSHORT(hGTIF, GTRasterTypeGeoKey, &nRasterType,
                    0, 1 ) == 1 )
    {
        if( nRasterType == static_cast<short>(RasterPixelIsPoint) )
            m_oGTiffMDMD.SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);
        else
            m_oGTiffMDMD.SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_AREA);
    }
}

/************************************************************************/
/*                         LoadMDAreaOrPoint()                          */
/************************************************************************/

// This is a light version of LookForProjection(), which saves the
// potential costly cost of GTIFGetOGISDefn(), since we just need to
// access to a raw GeoTIFF key, and not build the full projection object.

void GTiffDataset::LoadMDAreaOrPoint()
{
    if( m_bLookedForProjection || m_bLookedForMDAreaOrPoint ||
        m_oGTiffMDMD.GetMetadataItem( GDALMD_AREA_OR_POINT ) != nullptr )
        return;

    m_bLookedForMDAreaOrPoint = true;

    GTIF* hGTIF = GTiffDatasetGTIFNew(m_hTIFF);

    if( !hGTIF )
    {
        ReportError( CE_Warning, CPLE_AppDefined,
                  "GeoTIFF tags apparently corrupt, they are being ignored." );
    }
    else
    {
        GTiffDatasetSetAreaOrPointMD( hGTIF, m_oGTiffMDMD );

        GTIFFree( hGTIF );
    }
}

/************************************************************************/
/*                         LookForProjection()                          */
/************************************************************************/

void GTiffDataset::LookForProjection()

{
    if( m_bLookedForProjection )
        return;

    m_bLookedForProjection = true;

    IdentifyAuthorizedGeoreferencingSources();
    if( m_nINTERNALGeorefSrcIndex < 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Capture the GeoTIFF projection, if available.                   */
/* -------------------------------------------------------------------- */
    m_oSRS.Clear();

    GTIF *hGTIF = GTiffDatasetGTIFNew(m_hTIFF);

    if( !hGTIF )
    {
        ReportError( CE_Warning, CPLE_AppDefined,
                  "GeoTIFF tags apparently corrupt, they are being ignored." );
    }
    else
    {
        GTIFDefn *psGTIFDefn = GTIFAllocDefn();

        if( GTIFGetDefn( hGTIF, psGTIFDefn ) )
        {
            OGRSpatialReferenceH hSRS = GTIFGetOGISDefnAsOSR( hGTIF, psGTIFDefn );
            if( hSRS )
            {
                m_oSRS = *(OGRSpatialReference::FromHandle(hSRS));
                OSRDestroySpatialReference(hSRS);
            }

            if( m_oSRS.IsCompound() )
            {
                const char* pszVertUnit = nullptr;
                m_oSRS.GetTargetLinearUnits("COMPD_CS|VERT_CS", &pszVertUnit);
                if( pszVertUnit && !EQUAL(pszVertUnit, "unknown") )
                {
                    CPLFree(m_pszVertUnit);
                    m_pszVertUnit = CPLStrdup(pszVertUnit);
                }

                int versions[3];
                GTIFDirectoryInfo(hGTIF, versions, nullptr);

                // If GeoTIFF 1.0, strip vertical by default
                const char* pszDefaultReportCompdCS =
                    ( versions[0] == 1 && versions[1]== 1 && versions[2] == 0 ) ? "NO" : "YES";

                // Should we simplify away vertical CS stuff?
                if( !CPLTestBool( CPLGetConfigOption("GTIFF_REPORT_COMPD_CS",
                                            pszDefaultReportCompdCS) ) )
                {
                    CPLDebug( "GTiff", "Got COMPD_CS, but stripping it." );

                    m_oSRS.StripVertical();
                }
            }
        }

        // Check the tif linear unit and the CS linear unit.
#ifdef ESRI_BUILD
        AdjustLinearUnit(psGTIFDefn.UOMLength);
#endif

        GTIFFreeDefn(psGTIFDefn);

        GTiffDatasetSetAreaOrPointMD( hGTIF, m_oGTiffMDMD );

        GTIFFree( hGTIF );
    }

    m_bGeoTIFFInfoChanged = false;
    m_bForceUnsetGTOrGCPs = false;
    m_bForceUnsetProjection = false;
}

/************************************************************************/
/*                          AdjustLinearUnit()                          */
/*                                                                      */
/*      The following code is only used in ESRI Builds and there is     */
/*      outstanding discussion on whether it is even appropriate        */
/*      then.                                                           */
/************************************************************************/
#ifdef ESRI_BUILD

void GTiffDataset::AdjustLinearUnit( short UOMLength )
{
    if( !pszProjection || strlen(pszProjection) == 0 )
        return;
    if( UOMLength == 9001 )
    {
        char* pstr = strstr(pszProjection, "PARAMETER");
        if( !pstr )
            return;
        pstr = strstr(pstr, "UNIT[");
        if( !pstr )
            return;
        pstr = strchr(pstr, ',') + 1;
        if( !pstr )
            return;
        char* pstr1 = strchr(pstr, ']');
        if( !pstr1 || pstr1 - pstr >= 128 )
            return;
        char csUnitStr[128];
        strncpy(csUnitStr, pstr, pstr1 - pstr);
        csUnitStr[pstr1-pstr] = '\0';
        const double csUnit = CPLAtof(csUnitStr);
        if( fabs(csUnit - 1.0) > 0.000001 )
        {
            for( long i = 0; i < 6; ++i )
                adfGeoTransform[i] /= csUnit;
        }
    }
}

#endif  // def ESRI_BUILD

/************************************************************************/
/*                            ApplyPamInfo()                            */
/*                                                                      */
/*      PAM Information, if available, overrides the GeoTIFF            */
/*      geotransform and projection definition.  Check for them         */
/*      now.                                                            */
/************************************************************************/

void GTiffDataset::ApplyPamInfo()

{
    if( m_nPAMGeorefSrcIndex >= 0 &&
        ((m_bGeoTransformValid &&
          m_nPAMGeorefSrcIndex < m_nGeoTransformGeorefSrcIndex) ||
          m_nGeoTransformGeorefSrcIndex < 0 || !m_bGeoTransformValid) )
    {
        double adfPamGeoTransform[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        if( GDALPamDataset::GetGeoTransform( adfPamGeoTransform ) == CE_None )
        {
            if( m_nGeoTransformGeorefSrcIndex == m_nWORLDFILEGeorefSrcIndex )
            {
                CPLFree(m_pszGeorefFilename);
                m_pszGeorefFilename = nullptr;
            }
            memcpy(m_adfGeoTransform, adfPamGeoTransform, sizeof(double) * 6);
            m_bGeoTransformValid = true;
        }
    }

    if( m_nPAMGeorefSrcIndex >= 0 )
    {
        if( (m_nTABFILEGeorefSrcIndex < 0 ||
             m_nPAMGeorefSrcIndex < m_nTABFILEGeorefSrcIndex) &&
            (m_nINTERNALGeorefSrcIndex < 0 ||
             m_nPAMGeorefSrcIndex < m_nINTERNALGeorefSrcIndex) )
        {
            const auto* poPamSRS = GDALPamDataset::GetSpatialRef();
            if( poPamSRS )
            {
                m_oSRS = *poPamSRS;
                m_bLookedForProjection = true;
                // m_nProjectionGeorefSrcIndex = m_nPAMGeorefSrcIndex;
            }
        }
        else
        {
            if( m_nINTERNALGeorefSrcIndex >= 0 )
                LookForProjection();
            if( m_oSRS.IsEmpty() )
            {
                const auto* poPamSRS = GDALPamDataset::GetSpatialRef();
                if( poPamSRS )
                {
                    m_oSRS = *poPamSRS;
                    m_bLookedForProjection = true;
                    // m_nProjectionGeorefSrcIndex = m_nPAMGeorefSrcIndex;
                }
            }
        }
    }

    int nPamGCPCount;
    if( m_nPAMGeorefSrcIndex >= 0 &&
        (nPamGCPCount = GDALPamDataset::GetGCPCount()) > 0 &&
        ( (m_nGCPCount > 0 &&
           m_nPAMGeorefSrcIndex < m_nGeoTransformGeorefSrcIndex) ||
          m_nGeoTransformGeorefSrcIndex < 0 || m_nGCPCount == 0 ) )
    {
        if( m_nGCPCount > 0 )
        {
            GDALDeinitGCPs( m_nGCPCount, m_pasGCPList );
            CPLFree( m_pasGCPList );
            m_pasGCPList = nullptr;
        }

        m_nGCPCount = nPamGCPCount;
        m_pasGCPList = GDALDuplicateGCPs(m_nGCPCount, GDALPamDataset::GetGCPs());

        // m_nProjectionGeorefSrcIndex = m_nPAMGeorefSrcIndex;

        const auto* poPamGCPSRS = GDALPamDataset::GetGCPSpatialRef();
        if( poPamGCPSRS )
            m_oSRS = *poPamGCPSRS;
        else
            m_oSRS.Clear();

        m_bLookedForProjection = true;
    }

    if( m_nPAMGeorefSrcIndex >= 0 )
    {
        CPLXMLNode *psValueAsXML = nullptr;
        CPLXMLNode *psGeodataXform = nullptr;
        char** papszXML = oMDMD.GetMetadata( "xml:ESRI" );
        if (CSLCount(papszXML) == 1)
        {
            psValueAsXML = CPLParseXMLString( papszXML[0] );
            if( psValueAsXML )
                psGeodataXform = CPLGetXMLNode(psValueAsXML, "=GeodataXform");
        }

        const char* pszTIFFTagResUnit = GetMetadataItem("TIFFTAG_RESOLUTIONUNIT");
        const char* pszTIFFTagXRes = GetMetadataItem("TIFFTAG_XRESOLUTION");
        const char* pszTIFFTagYRes = GetMetadataItem("TIFFTAG_YRESOLUTION");
        if (psGeodataXform && pszTIFFTagResUnit &&pszTIFFTagXRes &&
            pszTIFFTagYRes && atoi(pszTIFFTagResUnit) == 2 )
        {
            CPLXMLNode* psSourceGCPs = CPLGetXMLNode(psGeodataXform,
                                                        "SourceGCPs");
            CPLXMLNode* psTargetGCPs = CPLGetXMLNode(psGeodataXform,
                                                        "TargetGCPs");
            if( psSourceGCPs && psTargetGCPs )
            {
                std::vector<double> adfSourceGCPs, adfTargetGCPs;
                for( CPLXMLNode* psIter = psSourceGCPs->psChild;
                                    psIter != nullptr;
                                    psIter = psIter->psNext )
                {
                    if( psIter->eType == CXT_Element &&
                        EQUAL(psIter->pszValue, "Double") )
                    {
                        adfSourceGCPs.push_back(
                            CPLAtof( CPLGetXMLValue(psIter, nullptr, "") ) );
                    }
                }
                for( CPLXMLNode* psIter = psTargetGCPs->psChild;
                                    psIter != nullptr;
                                    psIter = psIter->psNext )
                {
                    if( psIter->eType == CXT_Element &&
                        EQUAL(psIter->pszValue, "Double") )
                    {
                        adfTargetGCPs.push_back(
                            CPLAtof( CPLGetXMLValue(psIter, nullptr, "") ) );
                    }
                }
                if( adfSourceGCPs.size() == adfTargetGCPs.size() &&
                    (adfSourceGCPs.size() % 2) == 0 )
                {
                    if( m_nGCPCount > 0 )
                    {
                        GDALDeinitGCPs( m_nGCPCount, m_pasGCPList );
                        CPLFree( m_pasGCPList );
                        m_pasGCPList = nullptr;
                        m_nGCPCount = 0;
                    }
                    m_nGCPCount = static_cast<int>(
                                            adfSourceGCPs.size() / 2);
                    m_pasGCPList = static_cast<GDAL_GCP *>(
                            CPLCalloc(sizeof(GDAL_GCP), m_nGCPCount) );
                    for( int i = 0; i < m_nGCPCount; ++i )
                    {
                        m_pasGCPList[i].pszId = CPLStrdup("");
                        m_pasGCPList[i].pszInfo = CPLStrdup("");
                        // The origin used is the bottom left corner,
                        // and raw values are in inches!
                        m_pasGCPList[i].dfGCPPixel = adfSourceGCPs[2*i] *
                                                        CPLAtof(pszTIFFTagXRes);
                        m_pasGCPList[i].dfGCPLine = nRasterYSize -
                                adfSourceGCPs[2*i+1] * CPLAtof(pszTIFFTagYRes);
                        m_pasGCPList[i].dfGCPX = adfTargetGCPs[2*i];
                        m_pasGCPList[i].dfGCPY = adfTargetGCPs[2*i+1];
                    }
                }
            }
        }
        if( psValueAsXML )
            CPLDestroyXMLNode(psValueAsXML);
    }

/* -------------------------------------------------------------------- */
/*      Copy any PAM metadata into our GeoTIFF context, and with        */
/*      the PAM info overriding the GeoTIFF context.                    */
/* -------------------------------------------------------------------- */
    char **papszPamDomains = oMDMD.GetDomainList();

    for( int iDomain = 0;
         papszPamDomains && papszPamDomains[iDomain] != nullptr;
         ++iDomain )
    {
        const char *pszDomain = papszPamDomains[iDomain];
        char **papszGT_MD = CSLDuplicate(m_oGTiffMDMD.GetMetadata( pszDomain ));
        char **papszPAM_MD = oMDMD.GetMetadata( pszDomain );

        papszGT_MD = CSLMerge( papszGT_MD, papszPAM_MD );

        m_oGTiffMDMD.SetMetadata( papszGT_MD, pszDomain );
        CSLDestroy( papszGT_MD );
    }

    for( int i = 1; i <= GetRasterCount(); ++i )
    {
        GTiffRasterBand* poBand =
            cpl::down_cast<GTiffRasterBand *>(GetRasterBand(i));
        papszPamDomains = poBand->oMDMD.GetDomainList();

        for( int iDomain = 0;
             papszPamDomains && papszPamDomains[iDomain] != nullptr;
             ++iDomain )
        {
            const char *pszDomain = papszPamDomains[iDomain];
            char **papszGT_MD =
                CSLDuplicate(poBand->m_oGTiffMDMD.GetMetadata( pszDomain ));
            char **papszPAM_MD = poBand->oMDMD.GetMetadata( pszDomain );

            papszGT_MD = CSLMerge( papszGT_MD, papszPAM_MD );

            poBand->m_oGTiffMDMD.SetMetadata( papszGT_MD, pszDomain );
            CSLDestroy( papszGT_MD );
        }
    }
}

/************************************************************************/
/*                              OpenDir()                               */
/*                                                                      */
/*      Open a specific directory as encoded into a filename.           */
/************************************************************************/

GDALDataset *GTiffDataset::OpenDir( GDALOpenInfo * poOpenInfo )

{
    bool bAllowRGBAInterface = true;
    const char* pszFilename = poOpenInfo->pszFilename;
    if( STARTS_WITH_CI(pszFilename, "GTIFF_RAW:") )
    {
        bAllowRGBAInterface = false;
        pszFilename += strlen("GTIFF_RAW:");
    }

    if( !STARTS_WITH_CI(pszFilename, "GTIFF_DIR:") ||
        pszFilename[strlen("GTIFF_DIR:")] == '\0' )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Split out filename, and dir#/offset.                            */
/* -------------------------------------------------------------------- */
    pszFilename += strlen("GTIFF_DIR:");
    bool bAbsolute = false;

    if( STARTS_WITH_CI(pszFilename, "off:") )
    {
        bAbsolute = true;
        pszFilename += 4;
    }

    toff_t nOffset = atol(pszFilename);
    pszFilename += 1;

    while( *pszFilename != '\0' && pszFilename[-1] != ':' )
        ++pszFilename;

    if( *pszFilename == '\0' || nOffset == 0 )
    {
        ReportError(pszFilename, CE_Failure, CPLE_OpenFailed,
            "Unable to extract offset or filename, should take the form:\n"
            "GTIFF_DIR:<dir>:filename or GTIFF_DIR:off:<dir_offset>:filename" );
        return nullptr;
    }

    if( poOpenInfo->eAccess == GA_Update )
    {
        ReportError(
            pszFilename, CE_Warning, CPLE_AppDefined,
            "Opening a specific TIFF directory is not supported in "
            "update mode. Switching to read-only" );
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    if( !GTiffOneTimeInit() )
        return nullptr;

    const char* pszFlag = poOpenInfo->eAccess == GA_Update ? "r+D" : "rDO";
    VSILFILE* l_fpL = VSIFOpenL(pszFilename, pszFlag);
    if( l_fpL == nullptr )
        return nullptr;
    TIFF *l_hTIFF = VSI_TIFFOpen( pszFilename, pszFlag, l_fpL );
    if( l_hTIFF == nullptr )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      If a directory was requested by index, advance to it now.       */
/* -------------------------------------------------------------------- */
    if( !bAbsolute )
    {
        const toff_t nOffsetRequested = nOffset;
        while( nOffset > 1 )
        {
            if( TIFFReadDirectory( l_hTIFF ) == 0 )
            {
                XTIFFClose( l_hTIFF );
                ReportError(
                    pszFilename, CE_Failure, CPLE_OpenFailed,
                    "Requested directory %lu not found.",
                    static_cast<long unsigned int>(nOffsetRequested));
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }
            nOffset--;
        }

        nOffset = TIFFCurrentDirOffset( l_hTIFF );
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->m_pszFilename = CPLStrdup(pszFilename);
    poDS->m_fpL = l_fpL;
    poDS->m_hTIFF = l_hTIFF;
    poDS->m_bSingleIFDOpened = true;

    if( !EQUAL(pszFilename,poOpenInfo->pszFilename)
        && !STARTS_WITH_CI(poOpenInfo->pszFilename, "GTIFF_RAW:") )
    {
        poDS->SetPhysicalFilename( pszFilename );
        poDS->SetSubdatasetName( poOpenInfo->pszFilename );
    }

    if( poOpenInfo->AreSiblingFilesLoaded() )
        poDS->oOvManager.TransferSiblingFiles(
            poOpenInfo->StealSiblingFiles() );

    if( poDS->OpenOffset( l_hTIFF,
                          nOffset, poOpenInfo->eAccess,
                          bAllowRGBAInterface, true ) != CE_None )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                   ConvertTransferFunctionToString()                  */
/*                                                                      */
/*      Convert a transfer function table into a string.                */
/*      Used by LoadICCProfile().                                       */
/************************************************************************/
static CPLString ConvertTransferFunctionToString( const uint16_t *pTable,
                                                  uint32_t nTableEntries )
{
    CPLString sValue;

    for( uint32_t i = 0; i < nTableEntries; ++i )
    {
        if( i == 0 )
            sValue = sValue.Printf("%d", static_cast<uint32_t>(pTable[i]));
        else
            sValue = sValue.Printf( "%s, %d",
                                    sValue.c_str(),
                                    static_cast<uint32_t>(pTable[i]));
    }

    return sValue;
}

/************************************************************************/
/*                             LoadICCProfile()                         */
/*                                                                      */
/*      Load ICC Profile or colorimetric data into metadata             */
/************************************************************************/

void GTiffDataset::LoadICCProfile()
{
    if( m_bICCMetadataLoaded )
        return;
    m_bICCMetadataLoaded = true;

    uint32_t nEmbedLen = 0;
    uint8_t* pEmbedBuffer = nullptr;


    if( TIFFGetField(m_hTIFF, TIFFTAG_ICCPROFILE, &nEmbedLen, &pEmbedBuffer) )
    {
        char *pszBase64Profile =
            CPLBase64Encode(nEmbedLen, reinterpret_cast<const GByte*>(pEmbedBuffer));

        m_oGTiffMDMD.SetMetadataItem( "SOURCE_ICC_PROFILE", pszBase64Profile,
                                    "COLOR_PROFILE" );

        CPLFree(pszBase64Profile);

        return;
    }

    // Check for colorimetric tiff.
    float* pCHR = nullptr;
    float* pWP = nullptr;
    uint16_t *pTFR = nullptr;
    uint16_t *pTFG = nullptr;
    uint16_t *pTFB = nullptr;
    uint16_t *pTransferRange = nullptr;

    if( TIFFGetField(m_hTIFF, TIFFTAG_PRIMARYCHROMATICITIES, &pCHR) )
    {
        if( TIFFGetField(m_hTIFF, TIFFTAG_WHITEPOINT, &pWP) )
        {
            if( !TIFFGetFieldDefaulted( m_hTIFF, TIFFTAG_TRANSFERFUNCTION, &pTFR,
                                        &pTFG, &pTFB) ||
                pTFR == nullptr || pTFG == nullptr || pTFB == nullptr )
            {
                return;
            }

            const int TIFFTAG_TRANSFERRANGE = 0x0156;
            TIFFGetFieldDefaulted( m_hTIFF, TIFFTAG_TRANSFERRANGE,
                                   &pTransferRange);

            // Set all the colorimetric metadata.
            m_oGTiffMDMD.SetMetadataItem(
                "SOURCE_PRIMARIES_RED",
                CPLString().Printf( "%.9f, %.9f, 1.0",
                                    static_cast<double>(pCHR[0]),
                                    static_cast<double>(pCHR[1]) ),
                "COLOR_PROFILE" );
            m_oGTiffMDMD.SetMetadataItem(
                "SOURCE_PRIMARIES_GREEN",
                CPLString().Printf( "%.9f, %.9f, 1.0",
                                    static_cast<double>(pCHR[2]),
                                    static_cast<double>(pCHR[3]) ),
                "COLOR_PROFILE" );
            m_oGTiffMDMD.SetMetadataItem(
                "SOURCE_PRIMARIES_BLUE",
                CPLString().Printf( "%.9f, %.9f, 1.0",
                                    static_cast<double>(pCHR[4]),
                                    static_cast<double>(pCHR[5]) ),
                "COLOR_PROFILE" );

            m_oGTiffMDMD.SetMetadataItem(
                "SOURCE_WHITEPOINT",
                CPLString().Printf( "%.9f, %.9f, 1.0",
                                    static_cast<double>(pWP[0]),
                                    static_cast<double>(pWP[1]) ),
                "COLOR_PROFILE" );

            // Set transfer function metadata.

            // Get length of table.
            const uint32_t nTransferFunctionLength = 1 << m_nBitsPerSample;

            m_oGTiffMDMD.SetMetadataItem(
                "TIFFTAG_TRANSFERFUNCTION_RED",
                ConvertTransferFunctionToString( pTFR, nTransferFunctionLength),
                "COLOR_PROFILE" );

            m_oGTiffMDMD.SetMetadataItem(
                "TIFFTAG_TRANSFERFUNCTION_GREEN",
                ConvertTransferFunctionToString( pTFG, nTransferFunctionLength),
                "COLOR_PROFILE" );

            m_oGTiffMDMD.SetMetadataItem(
                "TIFFTAG_TRANSFERFUNCTION_BLUE",
                ConvertTransferFunctionToString( pTFB, nTransferFunctionLength),
                "COLOR_PROFILE" );

            // Set transfer range.
            if( pTransferRange )
            {
                m_oGTiffMDMD.SetMetadataItem(
                    "TIFFTAG_TRANSFERRANGE_BLACK",
                    CPLString().Printf( "%d, %d, %d",
                                        static_cast<int>(pTransferRange[0]),
                                        static_cast<int>(pTransferRange[2]),
                                        static_cast<int>(pTransferRange[4])),
                    "COLOR_PROFILE" );
                m_oGTiffMDMD.SetMetadataItem(
                    "TIFFTAG_TRANSFERRANGE_WHITE",
                    CPLString().Printf( "%d, %d, %d",
                                        static_cast<int>(pTransferRange[1]),
                                        static_cast<int>(pTransferRange[3]),
                                        static_cast<int>(pTransferRange[5])),
                    "COLOR_PROFILE" );
            }
        }
    }
}

/************************************************************************/
/*                             SaveICCProfile()                         */
/*                                                                      */
/*      Save ICC Profile or colorimetric data into file                 */
/* pDS:                                                                 */
/*      Dataset that contains the metadata with the ICC or colorimetric */
/*      data. If this argument is specified, all other arguments are    */
/*      ignored. Set them to NULL or 0.                                 */
/* hTIFF:                                                               */
/*      Pointer to TIFF handle. Only needed if pDS is NULL or           */
/*      pDS->m_hTIFF is NULL.                                             */
/* papszParamList:                                                       */
/*      Options containing the ICC profile or colorimetric metadata.    */
/*      Ignored if pDS is not NULL.                                     */
/* nBitsPerSample:                                                      */
/*      Bits per sample. Ignored if pDS is not NULL.                    */
/************************************************************************/

void GTiffDataset::SaveICCProfile( GTiffDataset *pDS, TIFF *l_hTIFF,
                                   char **papszParamList,
                                   uint32_t l_nBitsPerSample )
{
    if( (pDS != nullptr) && (pDS->eAccess != GA_Update) )
        return;

    if( l_hTIFF == nullptr )
    {
        if( pDS == nullptr )
            return;

        l_hTIFF = pDS->m_hTIFF;
        if( l_hTIFF == nullptr )
            return;
    }

    if( (papszParamList == nullptr) && (pDS == nullptr) )
        return;

    const char *pszValue = nullptr;
    if( pDS != nullptr )
        pszValue = pDS->GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE");
    else
        pszValue = CSLFetchNameValue(papszParamList, "SOURCE_ICC_PROFILE");
    if( pszValue != nullptr )
    {
        char *pEmbedBuffer = CPLStrdup(pszValue);
        int32_t nEmbedLen =
            CPLBase64DecodeInPlace(reinterpret_cast<GByte *>(pEmbedBuffer));

        TIFFSetField(l_hTIFF, TIFFTAG_ICCPROFILE, nEmbedLen, pEmbedBuffer);

        CPLFree(pEmbedBuffer);
    }
    else
    {
        // Output colorimetric data.
        float pCHR[6] = {};  // Primaries.
        uint16_t pTXR[6] = {};  // Transfer range.
        const char* pszCHRNames[] = {
            "SOURCE_PRIMARIES_RED",
            "SOURCE_PRIMARIES_GREEN",
            "SOURCE_PRIMARIES_BLUE"
        };
        const char* pszTXRNames[] = {
            "TIFFTAG_TRANSFERRANGE_BLACK",
            "TIFFTAG_TRANSFERRANGE_WHITE"
        };

        // Output chromacities.
        bool bOutputCHR = true;
        for( int i = 0; i < 3 && bOutputCHR; ++i )
        {
            if( pDS != nullptr )
                pszValue =
                    pDS->GetMetadataItem(pszCHRNames[i], "COLOR_PROFILE");
            else
                pszValue = CSLFetchNameValue(papszParamList, pszCHRNames[i]);
            if( pszValue == nullptr )
            {
                bOutputCHR = false;
                break;
            }

            char** papszTokens =
                CSLTokenizeString2(
                    pszValue, ",",
                    CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES |
                    CSLT_STRIPENDSPACES );

            if( CSLCount( papszTokens ) != 3 )
            {
                bOutputCHR = false;
                CSLDestroy( papszTokens );
                break;
            }

            for( int j = 0; j < 3; ++j )
            {
                float v = static_cast<float>(CPLAtof(papszTokens[j]));

                if( j == 2 )
                {
                    // Last term of xyY color must be 1.0.
                    if( v != 1.0 )
                    {
                        bOutputCHR = false;
                        break;
                    }
                }
                else
                {
                    pCHR[i * 2 + j] = v;
                }
            }

            CSLDestroy( papszTokens );
        }

        if( bOutputCHR )
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PRIMARYCHROMATICITIES, pCHR);
        }

        // Output whitepoint.
        if( pDS != nullptr )
            pszValue =
                pDS->GetMetadataItem("SOURCE_WHITEPOINT", "COLOR_PROFILE");
        else
            pszValue = CSLFetchNameValue(papszParamList, "SOURCE_WHITEPOINT");
        if( pszValue != nullptr )
        {
            char** papszTokens =
                CSLTokenizeString2(
                    pszValue, ",",
                    CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES |
                    CSLT_STRIPENDSPACES );

            bool bOutputWhitepoint = true;
            float pWP[2] = { 0.0f, 0.0f };  // Whitepoint
            if( CSLCount( papszTokens ) != 3 )
            {
                bOutputWhitepoint = false;
            }
            else
            {
                for( int j = 0; j < 3; ++j )
                {
                    const float v = static_cast<float>(CPLAtof(papszTokens[j]));

                    if( j == 2 )
                    {
                        // Last term of xyY color must be 1.0.
                        if( v != 1.0 )
                        {
                            bOutputWhitepoint = false;
                            break;
                        }
                    }
                    else
                    {
                        pWP[j] = v;
                    }
                }
            }
            CSLDestroy( papszTokens );

            if( bOutputWhitepoint )
            {
                TIFFSetField(l_hTIFF, TIFFTAG_WHITEPOINT, pWP);
            }
        }

        // Set transfer function metadata.
        char const *pszTFRed = nullptr;
        if( pDS != nullptr )
            pszTFRed =
                pDS->GetMetadataItem( "TIFFTAG_TRANSFERFUNCTION_RED",
                                      "COLOR_PROFILE" );
        else
            pszTFRed =
                CSLFetchNameValue( papszParamList,
                                   "TIFFTAG_TRANSFERFUNCTION_RED" );

        char const *pszTFGreen = nullptr;
        if( pDS != nullptr )
            pszTFGreen =
                pDS->GetMetadataItem( "TIFFTAG_TRANSFERFUNCTION_GREEN",
                                      "COLOR_PROFILE" );
        else
            pszTFGreen =
                CSLFetchNameValue( papszParamList,
                                   "TIFFTAG_TRANSFERFUNCTION_GREEN" );

        char const *pszTFBlue = nullptr;
        if( pDS != nullptr )
            pszTFBlue =
                pDS->GetMetadataItem( "TIFFTAG_TRANSFERFUNCTION_BLUE",
                                      "COLOR_PROFILE" );
        else
            pszTFBlue =
                CSLFetchNameValue( papszParamList,
                                   "TIFFTAG_TRANSFERFUNCTION_BLUE" );

        if( (pszTFRed != nullptr) && (pszTFGreen != nullptr) && (pszTFBlue != nullptr) )
        {
            // Get length of table.
            const int nTransferFunctionLength =
                1 << ((pDS!=nullptr)?pDS->m_nBitsPerSample:l_nBitsPerSample);

            char** papszTokensRed =
                CSLTokenizeString2(
                    pszTFRed, ",",
                    CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES |
                    CSLT_STRIPENDSPACES );
            char** papszTokensGreen =
                CSLTokenizeString2(
                    pszTFGreen, ",",
                    CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES |
                    CSLT_STRIPENDSPACES );
            char** papszTokensBlue =
                CSLTokenizeString2(
                    pszTFBlue, ",",
                    CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES |
                    CSLT_STRIPENDSPACES );

            if( (CSLCount( papszTokensRed ) == nTransferFunctionLength) &&
                (CSLCount( papszTokensGreen ) == nTransferFunctionLength) &&
                (CSLCount( papszTokensBlue ) == nTransferFunctionLength) )
            {
                uint16_t *pTransferFuncRed =
                    static_cast<uint16_t*>( CPLMalloc(
                        sizeof(uint16_t) * nTransferFunctionLength ) );
                uint16_t *pTransferFuncGreen =
                    static_cast<uint16_t*>( CPLMalloc(
                        sizeof(uint16_t) * nTransferFunctionLength ) );
                uint16_t *pTransferFuncBlue =
                    static_cast<uint16_t*>( CPLMalloc(
                        sizeof(uint16_t) * nTransferFunctionLength ) );

                // Convert our table in string format into int16_t format.
                for( int i = 0; i < nTransferFunctionLength; ++i )
                {
                    pTransferFuncRed[i] =
                        static_cast<uint16_t>(atoi(papszTokensRed[i]));
                    pTransferFuncGreen[i] =
                        static_cast<uint16_t>(atoi(papszTokensGreen[i]));
                    pTransferFuncBlue[i] =
                        static_cast<uint16_t>(atoi(papszTokensBlue[i]));
                }

                TIFFSetField(l_hTIFF, TIFFTAG_TRANSFERFUNCTION,
                    pTransferFuncRed, pTransferFuncGreen, pTransferFuncBlue);

                CPLFree(pTransferFuncRed);
                CPLFree(pTransferFuncGreen);
                CPLFree(pTransferFuncBlue);
            }

            CSLDestroy( papszTokensRed );
            CSLDestroy( papszTokensGreen );
            CSLDestroy( papszTokensBlue );
        }

        // Output transfer range.
        bool bOutputTransferRange = true;
        for( int i = 0; (i < 2) && bOutputTransferRange; ++i )
        {
            if( pDS != nullptr )
                pszValue = pDS->GetMetadataItem( pszTXRNames[i],
                                                 "COLOR_PROFILE" );
            else
                pszValue = CSLFetchNameValue(papszParamList, pszTXRNames[i]);
            if( pszValue == nullptr )
            {
                bOutputTransferRange = false;
                break;
            }

            char** papszTokens =
                CSLTokenizeString2(
                    pszValue, ",",
                    CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES |
                    CSLT_STRIPENDSPACES );

            if( CSLCount( papszTokens ) != 3 )
            {
                bOutputTransferRange = false;
                CSLDestroy( papszTokens );
                break;
            }

            for( int j = 0; j < 3; ++j )
            {
                pTXR[i + j * 2] = static_cast<uint16_t>(atoi(papszTokens[j]));
            }

            CSLDestroy( papszTokens );
        }

        if( bOutputTransferRange )
        {
            const int TIFFTAG_TRANSFERRANGE = 0x0156;
            TIFFSetField(l_hTIFF, TIFFTAG_TRANSFERRANGE, pTXR);
        }
    }
}

/************************************************************************/
/*                             OpenOffset()                             */
/*                                                                      */
/*      Initialize the GTiffDataset based on a passed in file           */
/*      handle, and directory offset to utilize.  This is called for    */
/*      full res, and overview pages.                                   */
/************************************************************************/

CPLErr GTiffDataset::OpenOffset( TIFF *hTIFFIn,
                                 toff_t nDirOffsetIn,
                                 GDALAccess eAccessIn,
                                 bool bAllowRGBAInterface,
                                 bool bReadGeoTransform )

{
    if( !hTIFFIn )
        return CE_Failure;

    eAccess = eAccessIn;

    m_hTIFF = hTIFFIn;

    m_nDirOffset = nDirOffsetIn;

    if( !SetDirectory() )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    uint32_t nXSize = 0;
    uint32_t nYSize = 0;
    TIFFGetField( m_hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( m_hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
    if( nXSize > INT_MAX || nYSize > INT_MAX )
    {
        // GDAL only supports signed 32bit dimensions.
        ReportError(CE_Failure, CPLE_NotSupported,
                 "Too large image size: %u x %u",
                 nXSize, nYSize);
        return CE_Failure;
    }
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    if( !TIFFGetField(m_hTIFF, TIFFTAG_SAMPLESPERPIXEL, &m_nSamplesPerPixel ) )
        nBands = 1;
    else
        nBands = m_nSamplesPerPixel;

    if( !TIFFGetField(m_hTIFF, TIFFTAG_BITSPERSAMPLE, &(m_nBitsPerSample)) )
        m_nBitsPerSample = 1;

    if( !TIFFGetField( m_hTIFF, TIFFTAG_PLANARCONFIG, &(m_nPlanarConfig) ) )
        m_nPlanarConfig = PLANARCONFIG_CONTIG;

    if( !TIFFGetField( m_hTIFF, TIFFTAG_PHOTOMETRIC, &(m_nPhotometric) ) )
        m_nPhotometric = PHOTOMETRIC_MINISBLACK;

    if( !TIFFGetField( m_hTIFF, TIFFTAG_SAMPLEFORMAT, &(m_nSampleFormat) ) )
        m_nSampleFormat = SAMPLEFORMAT_UINT;

    if( !TIFFGetField( m_hTIFF, TIFFTAG_COMPRESSION, &(m_nCompression) ) )
        m_nCompression = COMPRESSION_NONE;

    if( m_nCompression != COMPRESSION_NONE &&
        !TIFFIsCODECConfigured(m_nCompression) )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                  "Cannot open TIFF file due to missing codec." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      YCbCr JPEG compressed images should be translated on the fly    */
/*      to RGB by libtiff/libjpeg unless specifically requested         */
/*      otherwise.                                                      */
/* -------------------------------------------------------------------- */
    if( m_nCompression == COMPRESSION_JPEG
        && m_nPhotometric == PHOTOMETRIC_YCBCR
        && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES") ) )
    {
        m_oGTiffMDMD.SetMetadataItem( "SOURCE_COLOR_SPACE", "YCbCr",
                                    "IMAGE_STRUCTURE" );
        int nColorMode = 0;
        if( !TIFFGetField( m_hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode ) ||
            nColorMode != JPEGCOLORMODE_RGB )
        {
            TIFFSetField(m_hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }
    }

/* -------------------------------------------------------------------- */
/*      Get strip/tile layout.                                          */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled(m_hTIFF) )
    {
        uint32_t l_nBlockXSize = 0;
        uint32_t l_nBlockYSize = 0;
        TIFFGetField( m_hTIFF, TIFFTAG_TILEWIDTH, &(l_nBlockXSize) );
        TIFFGetField( m_hTIFF, TIFFTAG_TILELENGTH, &(l_nBlockYSize) );
        if( l_nBlockXSize > INT_MAX || l_nBlockYSize > INT_MAX )
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                     "Too large block size: %u x %u",
                     l_nBlockXSize, l_nBlockYSize);
            return CE_Failure;
        }
        m_nBlockXSize = static_cast<int>(l_nBlockXSize);
        m_nBlockYSize = static_cast<int>(l_nBlockYSize);
    }
    else
    {
        if( !TIFFGetField( m_hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(m_nRowsPerStrip) ) )
        {
            ReportError( CE_Warning, CPLE_AppDefined,
                      "RowsPerStrip not defined ... assuming all one strip." );
            m_nRowsPerStrip = nYSize;  // Dummy value.
        }

        // If the rows per strip is larger than the file we will get
        // confused.  libtiff internally will treat the rowsperstrip as
        // the image height and it is best if we do too. (#4468)
        if( m_nRowsPerStrip > static_cast<uint32_t>(nRasterYSize) )
            m_nRowsPerStrip = nRasterYSize;

        m_nBlockXSize = nRasterXSize;
        m_nBlockYSize = m_nRowsPerStrip;
    }

    const int l_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, m_nBlockYSize);
    const int l_nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, m_nBlockXSize);
    if( l_nBlocksPerColumn > INT_MAX / l_nBlocksPerRow )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                  "Too many blocks: %d x %d",
                  l_nBlocksPerRow, l_nBlocksPerColumn );
        return CE_Failure;
    }

    // Note: we could potentially go up to UINT_MAX blocks, but currently
    // we use a int nBlockId
    m_nBlocksPerBand = l_nBlocksPerColumn * l_nBlocksPerRow;
    if( m_nPlanarConfig == PLANARCONFIG_SEPARATE &&
        m_nBlocksPerBand > INT_MAX / nBands )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                  "Too many blocks: %d x %d x %d bands",
                  l_nBlocksPerRow, l_nBlocksPerColumn, nBands );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Should we handle this using the GTiffBitmapBand?                */
/* -------------------------------------------------------------------- */
    bool bTreatAsBitmap = false;

    if( m_nBitsPerSample == 1 && nBands == 1 )
    {
        bTreatAsBitmap = true;

        // Lets treat large "one row" bitmaps using the scanline api.
        if( !TIFFIsTiled(m_hTIFF)
            && m_nBlockYSize == nRasterYSize
            && nRasterYSize > 2000
            // libtiff does not support reading JBIG files with
            // TIFFReadScanline().
            && m_nCompression != COMPRESSION_JBIG )
        {
            m_bTreatAsSplitBitmap = true;
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we treat this via the RGBA interface?                    */
/* -------------------------------------------------------------------- */
    bool bTreatAsRGBA = false;
    if(
#ifdef DEBUG
        CPLTestBool(CPLGetConfigOption("GTIFF_FORCE_RGBA", "NO")) ||
#endif
        (bAllowRGBAInterface &&
        !bTreatAsBitmap && !(m_nBitsPerSample > 8)
        && (m_nPhotometric == PHOTOMETRIC_CIELAB ||
            m_nPhotometric == PHOTOMETRIC_LOGL ||
            m_nPhotometric == PHOTOMETRIC_LOGLUV ||
            m_nPhotometric == PHOTOMETRIC_SEPARATED ||
            ( m_nPhotometric == PHOTOMETRIC_YCBCR
              && m_nCompression != COMPRESSION_JPEG ))) )
    {
        char szMessage[1024] = {};

        if( TIFFRGBAImageOK( m_hTIFF, szMessage ) == 1 )
        {
            const char* pszSourceColorSpace = nullptr;
            nBands = 4;
            switch( m_nPhotometric )
            {
                case PHOTOMETRIC_CIELAB:
                    pszSourceColorSpace = "CIELAB";
                    break;
                case PHOTOMETRIC_LOGL:
                    pszSourceColorSpace = "LOGL";
                    break;
                case PHOTOMETRIC_LOGLUV:
                    pszSourceColorSpace = "LOGLUV";
                    break;
                case PHOTOMETRIC_SEPARATED:
                    pszSourceColorSpace = "CMYK";
                    break;
                case PHOTOMETRIC_YCBCR:
                    pszSourceColorSpace = "YCbCr";
                    nBands = 3; // probably true for other photometric values
                    break;
            }
            if( pszSourceColorSpace )
                m_oGTiffMDMD.SetMetadataItem( "SOURCE_COLOR_SPACE",
                                            pszSourceColorSpace,
                                            "IMAGE_STRUCTURE" );
            bTreatAsRGBA = true;

        }
        else
        {
            CPLDebug( "GTiff", "TIFFRGBAImageOK says:\n%s", szMessage );
        }
    }

    // libtiff has various issues with OJPEG compression and chunky-strip
    // support with the "classic" scanline/strip/tile interfaces, and that
    // wouldn't work either, so better bail out.
    if( m_nCompression == COMPRESSION_OJPEG &&
        !bTreatAsRGBA )
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                 "Old-JPEG compression only supported through RGBA interface, "
                 "which cannot be used probably because the file is corrupted");
        return CE_Failure;
    }

    // If photometric is YCbCr, scanline/strip/tile interfaces assumes that
    // we are ready with downsampled data. And we are not.
    if( m_nCompression != COMPRESSION_JPEG &&
        m_nCompression != COMPRESSION_OJPEG &&
        m_nPhotometric == PHOTOMETRIC_YCBCR &&
        m_nPlanarConfig == PLANARCONFIG_CONTIG &&
        !bTreatAsRGBA )
    {
        uint16_t nF1, nF2;
        TIFFGetFieldDefaulted(m_hTIFF,TIFFTAG_YCBCRSUBSAMPLING,&nF1,&nF2);
        if( nF1 != 1 || nF2 != 1 )
        {
            ReportError( CE_Failure, CPLE_AppDefined,
                      "Cannot open TIFF file with YCbCr, subsampling and "
                      "BitsPerSample > 8 that is not JPEG compressed" );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we treat this via the split interface?                   */
/* -------------------------------------------------------------------- */
    if( !TIFFIsTiled(m_hTIFF)
        && m_nBitsPerSample == 8
        && m_nBlockYSize == nRasterYSize
        && nRasterYSize > 2000
        && !bTreatAsRGBA
        && CPLTestBool(CPLGetConfigOption("GDAL_ENABLE_TIFF_SPLIT", "YES")) )
    {
        // libtiff 4.0.0beta5 (also
        // 20091104) and older will crash when trying to open a
        // all-in-one-strip YCbCr JPEG compressed TIFF (see #3259).
#if (TIFFLIB_VERSION <= 20091104)
        if( m_nPhotometric == PHOTOMETRIC_YCBCR &&
            m_nCompression == COMPRESSION_JPEG )
        {
            CPLDebug(
                "GTiff",
                "Avoid using split band to open all-in-one-strip "
                "YCbCr JPEG compressed TIFF because of older libtiff" );
        }
        else
#endif
        {
            m_bTreatAsSplit = true;
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we treat this via the odd bits interface?                */
/* -------------------------------------------------------------------- */
    bool bTreatAsOdd = false;
    if( m_nSampleFormat == SAMPLEFORMAT_IEEEFP )
    {
        if( m_nBitsPerSample == 16 || m_nBitsPerSample == 24 )
            bTreatAsOdd = true;
    }
    else if( !bTreatAsRGBA && !bTreatAsBitmap
             && m_nBitsPerSample != 8
             && m_nBitsPerSample != 16
             && m_nBitsPerSample != 32
             && m_nBitsPerSample != 64
             && m_nBitsPerSample != 128 )
    {
        bTreatAsOdd = true;
    }

/* -------------------------------------------------------------------- */
/*      We can't support 'chunks' bigger than 2GB on 32 bit builds      */
/* -------------------------------------------------------------------- */
#if SIZEOF_VOIDP == 4
    uint64_t nChunkSize = 0;
    if( m_bTreatAsSplit || m_bTreatAsSplitBitmap )
    {
        nChunkSize = TIFFScanlineSize64( m_hTIFF );
    }
    else
    {
        if( TIFFIsTiled(m_hTIFF) )
            nChunkSize = TIFFTileSize64( m_hTIFF );
        else
            nChunkSize = TIFFStripSize64( m_hTIFF );
    }
    if( bTreatAsRGBA )
    {
        nChunkSize = std::max(nChunkSize,
                        4 * static_cast<uint64_t>(m_nBlockXSize) * m_nBlockYSize);
    }
    if( nChunkSize > static_cast<uint64_t>(INT_MAX) )
    {
        ReportError( CE_Failure, CPLE_NotSupported,
                  "Scanline/tile/strip size bigger than 2GB unsupported "
                  "on 32-bit builds." );
        return CE_Failure;
    }
#endif

    const bool bMinIsWhite = m_nPhotometric == PHOTOMETRIC_MINISWHITE;

/* -------------------------------------------------------------------- */
/*      Check for NODATA                                                */
/* -------------------------------------------------------------------- */
    char *pszText = nullptr;
    if( TIFFGetField( m_hTIFF, TIFFTAG_GDAL_NODATA, &pszText ) &&
        !EQUAL(pszText, "") )
    {
        m_bNoDataSet = true;
        m_dfNoDataValue = CPLAtofM( pszText );
        if( m_nBitsPerSample == 32 && m_nSampleFormat == SAMPLEFORMAT_IEEEFP )
        {
            m_dfNoDataValue = GDALAdjustNoDataCloseToFloatMax(m_dfNoDataValue);
        }
    }

/* -------------------------------------------------------------------- */
/*      Capture the color table if there is one.                        */
/* -------------------------------------------------------------------- */
    unsigned short *panRed = nullptr;
    unsigned short *panGreen = nullptr;
    unsigned short *panBlue = nullptr;

    if( bTreatAsRGBA || m_nBitsPerSample > 16
        || TIFFGetField( m_hTIFF, TIFFTAG_COLORMAP,
                         &panRed, &panGreen, &panBlue) == 0 )
    {
        // Build inverted palette if we have inverted photometric.
        // Pixel values remains unchanged.  Avoid doing this for *deep*
        // data types (per #1882)
        if( m_nBitsPerSample <= 16 && m_nPhotometric == PHOTOMETRIC_MINISWHITE )
        {
            m_poColorTable = new GDALColorTable();
            const int nColorCount = 1 << m_nBitsPerSample;

            for( int iColor = 0; iColor < nColorCount; ++iColor )
            {
                const short nValue =
                    static_cast<short>(((255 * (nColorCount - 1 - iColor)) /
                                        (nColorCount - 1)));
                const GDALColorEntry oEntry =
                    { nValue, nValue, nValue, static_cast<short>(255) };
                m_poColorTable->SetColorEntry( iColor, &oEntry );
            }

            m_nPhotometric = PHOTOMETRIC_PALETTE;
        }
        else
        {
            m_poColorTable = nullptr;
        }
    }
    else
    {
        unsigned short nMaxColor = 0;

        m_poColorTable = new GDALColorTable();

        const int nColorCount = 1 << m_nBitsPerSample;

        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            // TODO(schwehr): Ensure the color entries are never negative?
            const unsigned short divisor = 257;
            const GDALColorEntry oEntry = {
                static_cast<short>(panRed[iColor] / divisor),
                static_cast<short>(panGreen[iColor] / divisor),
                static_cast<short>(panBlue[iColor] / divisor),
                static_cast<short>(
                    m_bNoDataSet && static_cast<int>(m_dfNoDataValue) == iColor
                    ? 0
                    : 255)
            };

            m_poColorTable->SetColorEntry( iColor, &oEntry );

            nMaxColor = std::max(nMaxColor, panRed[iColor]);
            nMaxColor = std::max(nMaxColor, panGreen[iColor]);
            nMaxColor = std::max(nMaxColor, panBlue[iColor]);
        }

        // Bug 1384 - Some TIFF files are generated with color map entry
        // values in range 0-255 instead of 0-65535 - try to handle these
        // gracefully.
        if( nMaxColor > 0 && nMaxColor < 256 )
        {
            CPLDebug(
                "GTiff",
                "TIFF ColorTable seems to be improperly scaled, fixing up." );

            for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
            {
                // TODO(schwehr): Ensure the color entries are never negative?
                const GDALColorEntry oEntry = {
                    static_cast<short>(panRed[iColor]),
                    static_cast<short>(panGreen[iColor]),
                    static_cast<short>(panBlue[iColor]),
                    m_bNoDataSet &&
                    static_cast<int>(m_dfNoDataValue) == iColor
                    ? static_cast<short>(0)
                    : static_cast<short>(255)
                };

                m_poColorTable->SetColorEntry( iColor, &oEntry );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; ++iBand )
    {
        if( bTreatAsRGBA )
            SetBand( iBand + 1, new GTiffRGBABand( this, iBand + 1 ) );
        else if( m_bTreatAsSplitBitmap )
            SetBand( iBand + 1, new GTiffSplitBitmapBand( this, iBand + 1 ) );
        else if( m_bTreatAsSplit )
            SetBand( iBand + 1, new GTiffSplitBand( this, iBand + 1 ) );
        else if( bTreatAsBitmap )
            SetBand( iBand + 1, new GTiffBitmapBand( this, iBand + 1 ) );
        else if( bTreatAsOdd )
            SetBand( iBand + 1, new GTiffOddBitsBand( this, iBand + 1 ) );
        else
            SetBand( iBand + 1, new GTiffRasterBand( this, iBand + 1 ) );
    }

    if( GetRasterBand(1)->GetRasterDataType() == GDT_Unknown )
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                 "Unsupported TIFF configuration: BitsPerSample(=%d) and "
                 "SampleType(=%d)",
                 m_nBitsPerSample,
                 m_nSampleFormat);
        return CE_Failure;
    }

    m_bReadGeoTransform = bReadGeoTransform;

/* -------------------------------------------------------------------- */
/*      Capture some other potentially interesting information.         */
/* -------------------------------------------------------------------- */
    char szWorkMDI[200] = {};
    uint16_t nShort = 0;

    for( size_t iTag = 0;
         iTag < sizeof(asTIFFTags) / sizeof(asTIFFTags[0]);
         ++iTag )
    {
        if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING )
        {
            if( TIFFGetField( m_hTIFF, asTIFFTags[iTag].nTagVal, &pszText ) )
                m_oGTiffMDMD.SetMetadataItem( asTIFFTags[iTag].pszTagName,
                                            pszText );
        }
        else if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT )
        {
            float fVal = 0.0;
            if( TIFFGetField( m_hTIFF, asTIFFTags[iTag].nTagVal, &fVal ) )
            {
                CPLsnprintf( szWorkMDI, sizeof(szWorkMDI), "%.8g", fVal );
                m_oGTiffMDMD.SetMetadataItem( asTIFFTags[iTag].pszTagName,
                                            szWorkMDI );
            }
        }
        else if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT &&
                 asTIFFTags[iTag].nTagVal != TIFFTAG_RESOLUTIONUNIT )
        {
            if( TIFFGetField( m_hTIFF, asTIFFTags[iTag].nTagVal, &nShort ) )
            {
                snprintf( szWorkMDI, sizeof(szWorkMDI), "%d", nShort );
                m_oGTiffMDMD.SetMetadataItem( asTIFFTags[iTag].pszTagName,
                                            szWorkMDI );
            }
        }
        else if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_BYTE_STRING )
        {
            uint32_t nCount = 0;
            if( TIFFGetField( m_hTIFF, asTIFFTags[iTag].nTagVal,
                              &nCount, &pszText ) )
            {
                std::string osStr;
                osStr.assign(pszText, nCount);
                m_oGTiffMDMD.SetMetadataItem( asTIFFTags[iTag].pszTagName,
                                            osStr.c_str() );
            }
        }
    }

    if( TIFFGetField( m_hTIFF, TIFFTAG_RESOLUTIONUNIT, &nShort ) )
    {
        if( nShort == RESUNIT_NONE )
            snprintf( szWorkMDI, sizeof(szWorkMDI), "%d (unitless)", nShort );
        else if( nShort == RESUNIT_INCH )
            snprintf( szWorkMDI, sizeof(szWorkMDI),
                      "%d (pixels/inch)", nShort );
        else if( nShort == RESUNIT_CENTIMETER )
            snprintf( szWorkMDI, sizeof(szWorkMDI), "%d (pixels/cm)", nShort );
        else
            snprintf( szWorkMDI, sizeof(szWorkMDI), "%d", nShort );
        m_oGTiffMDMD.SetMetadataItem( "TIFFTAG_RESOLUTIONUNIT", szWorkMDI );
    }

    int nTagSize = 0;
    void* pData = nullptr;
    if( TIFFGetField( m_hTIFF, TIFFTAG_XMLPACKET, &nTagSize, &pData ) )
    {
        char* pszXMP =
            static_cast<char *>( VSI_MALLOC_VERBOSE(nTagSize + 1) );
        if( pszXMP )
        {
            memcpy(pszXMP, pData, nTagSize);
            pszXMP[nTagSize] = '\0';

            char *apszMDList[2] = { pszXMP, nullptr };
            m_oGTiffMDMD.SetMetadata(apszMDList, "xml:XMP");

            CPLFree(pszXMP);
        }
    }

    if( m_nCompression == COMPRESSION_NONE )
        /* no compression tag */;
    else if( m_nCompression == COMPRESSION_CCITTRLE )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "CCITTRLE",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_CCITTFAX3 )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "CCITTFAX3",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_CCITTFAX4 )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "CCITTFAX4",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_LZW )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "LZW", "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_OJPEG )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "OJPEG", "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_JPEG )
    {
        if( m_nPhotometric == PHOTOMETRIC_YCBCR )
            m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "YCbCr JPEG",
                                        "IMAGE_STRUCTURE" );
        else
            m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "JPEG",
                                        "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_NEXT )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "NEXT", "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_CCITTRLEW )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "CCITTRLEW",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_PACKBITS )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "PACKBITS",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_THUNDERSCAN )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "THUNDERSCAN",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_PIXARFILM )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "PIXARFILM",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_PIXARLOG )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "PIXARLOG",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_DEFLATE )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "DEFLATE",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_ADOBE_DEFLATE )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "DEFLATE",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_DCS )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "DCS", "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_JBIG )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "JBIG", "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_SGILOG )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "SGILOG",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_SGILOG24 )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "SGILOG24",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_JP2000 )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "JP2000",
                                    "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_LZMA )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "LZMA", "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_ZSTD )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "ZSTD", "IMAGE_STRUCTURE" );
    }
    else if( m_nCompression == COMPRESSION_LERC )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "LERC", "IMAGE_STRUCTURE" );

        uint32_t nLercParamCount = 0;
        uint32_t* panLercParams = nullptr;
        if( TIFFGetField( m_hTIFF, TIFFTAG_LERC_PARAMETERS, &nLercParamCount,
                          &panLercParams ) &&
            nLercParamCount == 2 )
        {
            memcpy( m_anLercAddCompressionAndVersion, panLercParams,
                    sizeof(m_anLercAddCompressionAndVersion) );
        }

        uint32_t nAddVersion = LERC_ADD_COMPRESSION_NONE;
        if( TIFFGetField( m_hTIFF, TIFFTAG_LERC_ADD_COMPRESSION, &nAddVersion ) &&
            nAddVersion != LERC_ADD_COMPRESSION_NONE )
        {
            if( nAddVersion == LERC_ADD_COMPRESSION_DEFLATE )
            {
                m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "LERC_DEFLATE",
                                            "IMAGE_STRUCTURE" );
            }
            else if( nAddVersion == LERC_ADD_COMPRESSION_ZSTD )
            {
                m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "LERC_ZSTD",
                                            "IMAGE_STRUCTURE" );
            }
        }
        uint32_t nLercVersion = LERC_VERSION_2_4;
        if( TIFFGetField( m_hTIFF, TIFFTAG_LERC_VERSION, &nLercVersion) )
        {
            if( nLercVersion == LERC_VERSION_2_4 )
            {
                m_oGTiffMDMD.SetMetadataItem( "LERC_VERSION", "2.4",
                                            "IMAGE_STRUCTURE" );
            }
            else
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                         "Unknown Lerc version: %d", nLercVersion);
            }
        }
    }
    else if( m_nCompression == COMPRESSION_WEBP )
    {
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", "WEBP", "IMAGE_STRUCTURE" );
    }
    else
    {
        CPLString oComp;
        oComp.Printf( "%d", m_nCompression);
        m_oGTiffMDMD.SetMetadataItem( "COMPRESSION", oComp.c_str());
    }

    if( m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands != 1 )
        m_oGTiffMDMD.SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    else
        m_oGTiffMDMD.SetMetadataItem( "INTERLEAVE", "BAND", "IMAGE_STRUCTURE" );

    if( (GetRasterBand(1)->GetRasterDataType() == GDT_Byte &&
         m_nBitsPerSample != 8 ) ||
        (GetRasterBand(1)->GetRasterDataType() == GDT_UInt16 &&
         m_nBitsPerSample != 16) ||
        ((GetRasterBand(1)->GetRasterDataType() == GDT_UInt32 ||
          GetRasterBand(1)->GetRasterDataType() == GDT_Float32) &&
         m_nBitsPerSample != 32) )
    {
        for( int i = 0; i < nBands; ++i )
            cpl::down_cast<GTiffRasterBand*>(GetRasterBand(i + 1))->
                m_oGTiffMDMD.SetMetadataItem(
                    "NBITS",
                    CPLString().Printf(
                        "%d", static_cast<int>(m_nBitsPerSample) ),
                    "IMAGE_STRUCTURE" );
    }

    if( bMinIsWhite )
        m_oGTiffMDMD.SetMetadataItem( "MINISWHITE", "YES", "IMAGE_STRUCTURE" );

    if( TIFFGetField( m_hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) )
    {
        CPLXMLNode *psRoot = CPLParseXMLString( pszText );
        CPLXMLNode *psItem = nullptr;

        if( psRoot != nullptr && psRoot->eType == CXT_Element
            && EQUAL(psRoot->pszValue,"GDALMetadata") )
            psItem = psRoot->psChild;

        for( ; psItem != nullptr; psItem = psItem->psNext )
        {

            if( psItem->eType != CXT_Element
                || !EQUAL(psItem->pszValue,"Item") )
                continue;

            const char *pszKey = CPLGetXMLValue( psItem, "name", nullptr );
            const char *pszValue = CPLGetXMLValue( psItem, nullptr, nullptr );
            int nBand =
                atoi(CPLGetXMLValue( psItem, "sample", "-1" ));
            if( nBand < -1 || nBand > 65535 )
                continue;
            nBand ++;
            const char *pszRole = CPLGetXMLValue( psItem, "role", "" );
            const char *pszDomain = CPLGetXMLValue( psItem, "domain", "" );

            if( pszKey == nullptr || pszValue == nullptr )
                continue;
            if( EQUAL(pszDomain, "IMAGE_STRUCTURE") )
                continue;

            bool bIsXML = false;

            if( STARTS_WITH_CI(pszDomain, "xml:") )
                bIsXML = TRUE;

            char *pszUnescapedValue =
                CPLUnescapeString( pszValue, nullptr, CPLES_XML );
            if( nBand == 0 )
            {
                if( bIsXML )
                {
                    char *apszMD[2] = { pszUnescapedValue, nullptr };
                    m_oGTiffMDMD.SetMetadata( apszMD, pszDomain );
                }
                else
                {
                    m_oGTiffMDMD.SetMetadataItem( pszKey, pszUnescapedValue,
                                                pszDomain );
                }
            }
            else
            {
                GTiffRasterBand *poBand =
                    cpl::down_cast<GTiffRasterBand*>(GetRasterBand(nBand));
                if( poBand != nullptr )
                {
                    if( EQUAL(pszRole,"scale") )
                    {
                        poBand->m_bHaveOffsetScale = true;
                        poBand->m_dfScale = CPLAtofM(pszUnescapedValue);
                    }
                    else if( EQUAL(pszRole,"offset") )
                    {
                        poBand->m_bHaveOffsetScale = true;
                        poBand->m_dfOffset = CPLAtofM(pszUnescapedValue);
                    }
                    else if( EQUAL(pszRole,"unittype") )
                    {
                        poBand->m_osUnitType = pszUnescapedValue;
                    }
                    else if( EQUAL(pszRole,"description") )
                    {
                        poBand->m_osDescription = pszUnescapedValue;
                    }
                    else if( EQUAL(pszRole, "colorinterp") )
                    {
                        poBand->m_eBandInterp =
                            GDALGetColorInterpretationByName(pszUnescapedValue);
                    }
                    else
                    {
                        if( bIsXML )
                        {
                            char *apszMD[2] = { pszUnescapedValue, nullptr };
                            poBand->m_oGTiffMDMD.SetMetadata( apszMD, pszDomain );
                        }
                        else
                        {
                            poBand->m_oGTiffMDMD.SetMetadataItem(
                                pszKey,
                                pszUnescapedValue,
                                pszDomain );
                        }
                    }
                }
            }
            CPLFree( pszUnescapedValue );
        }

        CPLDestroyXMLNode( psRoot );
    }

    if( m_bStreamingIn )
    {
        toff_t* panOffsets = nullptr;
        TIFFGetField( m_hTIFF,
                      TIFFIsTiled( m_hTIFF ) ?
                      TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS,
                      &panOffsets );
        if( panOffsets )
        {
            int nBlockCount =
                TIFFIsTiled(m_hTIFF) ?
                TIFFNumberOfTiles(m_hTIFF) : TIFFNumberOfStrips(m_hTIFF);
            for( int i = 1; i < nBlockCount; ++i )
            {
                if( panOffsets[i] < panOffsets[i-1] )
                {
                    m_oGTiffMDMD.SetMetadataItem( "UNORDERED_BLOCKS", "YES",
                                                "TIFF");
                    CPLDebug(
                        "GTIFF",
                        "Offset of block %d is lower than previous block. "
                        "Reader must be careful",
                        i );
                    break;
                }
            }
        }
    }

    if( m_nCompression == COMPRESSION_JPEG )
    {
        bool bHasQuantizationTable = false;
        bool bHasHuffmanTable = false;
        int nQuality = GuessJPEGQuality( bHasQuantizationTable,
                                         bHasHuffmanTable );
        if( nQuality > 0 )
        {
            m_oGTiffMDMD.SetMetadataItem( "JPEG_QUALITY",
                                          CPLSPrintf("%d", nQuality),
                                          "IMAGE_STRUCTURE" );
            int nJpegTablesMode = JPEGTABLESMODE_QUANT;
            if( bHasHuffmanTable )
            {
                nJpegTablesMode |= JPEGTABLESMODE_HUFF;
            }
            m_oGTiffMDMD.SetMetadataItem( "JPEGTABLESMODE",
                                          CPLSPrintf("%d", nJpegTablesMode),
                                          "IMAGE_STRUCTURE" );
        }
        if( eAccess == GA_Update )
        {
            SetJPEGQualityAndTablesModeFromFile(nQuality, bHasQuantizationTable,
                                                bHasHuffmanTable);
        }
    }

    CPLAssert(m_bReadGeoTransform == bReadGeoTransform);
    CPLAssert(!m_bMetadataChanged);
    m_bMetadataChanged = false;

    return CE_None;
}

/************************************************************************/
/*                         GetSiblingFiles()                            */
/************************************************************************/

char** GTiffDataset::GetSiblingFiles()
{
    if( m_bHasGotSiblingFiles )
    {
        return oOvManager.GetSiblingFiles();
    }

    m_bHasGotSiblingFiles = true;
    const int nMaxFiles =
        atoi(CPLGetConfigOption("GDAL_READDIR_LIMIT_ON_OPEN", "1000"));
    char** papszSiblingFiles =
        VSIReadDirEx(CPLGetDirname(m_pszFilename), nMaxFiles);
    if( nMaxFiles > 0 && CSLCount(papszSiblingFiles) > nMaxFiles )
    {
        CPLDebug("GTiff", "GDAL_READDIR_LIMIT_ON_OPEN reached on %s",
                 CPLGetDirname(m_pszFilename));
        CSLDestroy(papszSiblingFiles);
        papszSiblingFiles = nullptr;
    }
    oOvManager.TransferSiblingFiles( papszSiblingFiles );

    return papszSiblingFiles;
}

/************************************************************************/
/*                   IdentifyAuthorizedGeoreferencingSources()          */
/************************************************************************/

void GTiffDataset::IdentifyAuthorizedGeoreferencingSources()
{
    if( m_bHasIdentifiedAuthorizedGeoreferencingSources )
        return;
    m_bHasIdentifiedAuthorizedGeoreferencingSources = true;
    CPLString osGeorefSources = CSLFetchNameValueDef( papszOpenOptions,
        "GEOREF_SOURCES",
        CPLGetConfigOption("GDAL_GEOREF_SOURCES",
                           "PAM,INTERNAL,TABFILE,WORLDFILE") );
    char** papszTokens = CSLTokenizeString2(osGeorefSources, ",", 0);
    m_nPAMGeorefSrcIndex = static_cast<signed char>(CSLFindString(papszTokens, "PAM"));
    m_nINTERNALGeorefSrcIndex = static_cast<signed char>(CSLFindString(papszTokens, "INTERNAL"));
    m_nTABFILEGeorefSrcIndex = static_cast<signed char>(CSLFindString(papszTokens, "TABFILE"));
    m_nWORLDFILEGeorefSrcIndex = static_cast<signed char>(CSLFindString(papszTokens, "WORLDFILE"));
    CSLDestroy(papszTokens);
}

/************************************************************************/
/*                     LoadGeoreferencingAndPamIfNeeded()               */
/************************************************************************/

void GTiffDataset::LoadGeoreferencingAndPamIfNeeded()

{
    if( !m_bReadGeoTransform && !m_bLoadPam )
        return;

    IdentifyAuthorizedGeoreferencingSources();

/* -------------------------------------------------------------------- */
/*      Get the transform or gcps from the GeoTIFF file.                */
/* -------------------------------------------------------------------- */
    if( m_bReadGeoTransform )
    {
        m_bReadGeoTransform = false;

        char *pszTabWKT = nullptr;
        double *padfTiePoints = nullptr;
        double *padfScale = nullptr;
        double *padfMatrix = nullptr;
        uint16_t nCount = 0;
        bool bPixelIsPoint = false;
        unsigned short nRasterType = 0;
        bool bPointGeoIgnore = false;

        std::set<signed char> aoSetPriorities;
        if( m_nINTERNALGeorefSrcIndex >= 0 )
            aoSetPriorities.insert(m_nINTERNALGeorefSrcIndex);
        if( m_nTABFILEGeorefSrcIndex >= 0 )
            aoSetPriorities.insert(m_nTABFILEGeorefSrcIndex);
        if( m_nWORLDFILEGeorefSrcIndex >= 0 )
            aoSetPriorities.insert(m_nWORLDFILEGeorefSrcIndex);
        for(const auto nIndex: aoSetPriorities )
        {
            if( m_nINTERNALGeorefSrcIndex == nIndex )
            {
                GTIF *psGTIF = GTiffDatasetGTIFNew( m_hTIFF );  // How expensive this is?

                if( psGTIF )
                {
                    if( GDALGTIFKeyGetSHORT(psGTIF, GTRasterTypeGeoKey,
                                            &nRasterType, 0, 1 ) == 1
                        && nRasterType ==
                           static_cast<short>(RasterPixelIsPoint) )
                    {
                        bPixelIsPoint = true;
                        bPointGeoIgnore =
                            CPLTestBool(
                                CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE",
                                                   "FALSE") );
                    }

                    GTIFFree( psGTIF );
                }

                m_adfGeoTransform[0] = 0.0;
                m_adfGeoTransform[1] = 1.0;
                m_adfGeoTransform[2] = 0.0;
                m_adfGeoTransform[3] = 0.0;
                m_adfGeoTransform[4] = 0.0;
                m_adfGeoTransform[5] = 1.0;

                uint16_t nCountScale = 0;
                if( TIFFGetField(m_hTIFF, TIFFTAG_GEOPIXELSCALE,
                                 &nCountScale, &padfScale )
                    && nCountScale >= 2
                    && padfScale[0] != 0.0 && padfScale[1] != 0.0 )
                {
                    m_adfGeoTransform[1] = padfScale[0];
                    if( padfScale[1] < 0 )
                    {
                        const char* pszOptionVal =
                            CPLGetConfigOption("GTIFF_HONOUR_NEGATIVE_SCALEY",
                                               nullptr);
                        if( pszOptionVal == nullptr )
                        {
                            ReportError(CE_Warning, CPLE_AppDefined,
                                "File with negative value for ScaleY in "
                                "GeoPixelScale tag. This is rather "
                                "unusual. GDAL, contrary to the GeoTIFF "
                                "specification, assumes that the file "
                                "was intended to be north-up, and will "
                                "treat this file as if ScaleY was "
                                "positive. You may override this behavior "
                                "by setting the GTIFF_HONOUR_NEGATIVE_SCALEY "
                                "configuration option to YES");
                            m_adfGeoTransform[5] = padfScale[1];
                        }
                        else if( CPLTestBool(pszOptionVal) )
                        {
                            m_adfGeoTransform[5] = -padfScale[1];
                        }
                        else
                        {
                            m_adfGeoTransform[5] = padfScale[1];
                        }
                    }
                    else
                    {
                        m_adfGeoTransform[5] = -padfScale[1];
                    }

                    if( TIFFGetField(m_hTIFF, TIFFTAG_GEOTIEPOINTS,
                                     &nCount, &padfTiePoints )
                        && nCount >= 6 )
                    {
                        m_adfGeoTransform[0] =
                            padfTiePoints[3] -
                            padfTiePoints[0] * m_adfGeoTransform[1];
                        m_adfGeoTransform[3] =
                            padfTiePoints[4] -
                            padfTiePoints[1] * m_adfGeoTransform[5];

                        if( bPixelIsPoint && !bPointGeoIgnore )
                        {
                            m_adfGeoTransform[0] -=
                                (m_adfGeoTransform[1] * 0.5 +
                                 m_adfGeoTransform[2] * 0.5);
                            m_adfGeoTransform[3] -=
                                (m_adfGeoTransform[4] * 0.5 +
                                 m_adfGeoTransform[5] * 0.5);
                        }

                        m_bGeoTransformValid = true;
                        m_nGeoTransformGeorefSrcIndex = nIndex;

                        if( nCountScale >= 3 && GetRasterCount() == 1 &&
                            (padfScale[2] != 0.0 ||
                             padfTiePoints[2] != 0.0 ||
                             padfTiePoints[5] != 0.0) )
                        {
                            LookForProjection();
                            if( !m_oSRS.IsEmpty() && m_oSRS.IsVertical() )
                            {
                                /* modelTiePointTag = (pixel, line, z0, X, Y, Z0) */
                                /* thus Z(some_point) = (z(some_point) - z0) * scaleZ + Z0 */
                                /* equivalently written as */
                                /* Z(some_point) = z(some_point) * scaleZ + offsetZ with */
                                /* offsetZ = - z0 * scaleZ + Z0 */
                                double dfScale = padfScale[2];
                                double dfOffset =
                                    -padfTiePoints[2] * dfScale + padfTiePoints[5];
                                GTiffRasterBand* poBand =
                                    cpl::down_cast<GTiffRasterBand*>(GetRasterBand(1));
                                poBand->m_bHaveOffsetScale = true;
                                poBand->m_dfScale = dfScale;
                                poBand->m_dfOffset = dfOffset;
                            }
                        }
                    }
                }

                else if( TIFFGetField(m_hTIFF, TIFFTAG_GEOTRANSMATRIX,
                                      &nCount, &padfMatrix )
                        && nCount == 16 )
                {
                    m_adfGeoTransform[0] = padfMatrix[3];
                    m_adfGeoTransform[1] = padfMatrix[0];
                    m_adfGeoTransform[2] = padfMatrix[1];
                    m_adfGeoTransform[3] = padfMatrix[7];
                    m_adfGeoTransform[4] = padfMatrix[4];
                    m_adfGeoTransform[5] = padfMatrix[5];

                    if( bPixelIsPoint && !bPointGeoIgnore )
                    {
                        m_adfGeoTransform[0] -=
                            m_adfGeoTransform[1] * 0.5 + m_adfGeoTransform[2] * 0.5;
                        m_adfGeoTransform[3] -=
                            m_adfGeoTransform[4] * 0.5 + m_adfGeoTransform[5] * 0.5;
                    }

                    m_bGeoTransformValid = true;
                    m_nGeoTransformGeorefSrcIndex = nIndex;
                }
                if( m_bGeoTransformValid )
                    break;
            }

/* -------------------------------------------------------------------- */
/*      Otherwise try looking for a .tab, .tfw, .tifw or .wld file.     */
/* -------------------------------------------------------------------- */
            if( m_nTABFILEGeorefSrcIndex == nIndex )
            {
                char* pszGeorefFilename = nullptr;

                char** papszSiblingFiles = GetSiblingFiles();

                // Begin with .tab since it can also have projection info.
                const int bTabFileOK =
                    GDALReadTabFile2( m_pszFilename, m_adfGeoTransform,
                                        &pszTabWKT, &m_nGCPCount, &m_pasGCPList,
                                        papszSiblingFiles, &pszGeorefFilename );

                if( bTabFileOK )
                {
                    m_nGeoTransformGeorefSrcIndex = nIndex;
                    // if( pszTabWKT )
                    // {
                    //     m_nProjectionGeorefSrcIndex = nIndex;
                    // }
                    if( m_nGCPCount == 0 )
                    {
                        m_bGeoTransformValid = true;
                    }
                }

                if( pszGeorefFilename )
                {
                    CPLFree(m_pszGeorefFilename);
                    m_pszGeorefFilename = pszGeorefFilename;
                    pszGeorefFilename = nullptr;
                }
                if( m_bGeoTransformValid )
                    break;
            }

            if( m_nWORLDFILEGeorefSrcIndex == nIndex )
            {
                char* pszGeorefFilename = nullptr;

                char** papszSiblingFiles = GetSiblingFiles();

                m_bGeoTransformValid = CPL_TO_BOOL( GDALReadWorldFile2(
                                m_pszFilename, nullptr, m_adfGeoTransform,
                                papszSiblingFiles, &pszGeorefFilename) );

                if( !m_bGeoTransformValid )
                {
                    m_bGeoTransformValid =
                        CPL_TO_BOOL( GDALReadWorldFile2(
                            m_pszFilename, "wld", m_adfGeoTransform,
                            papszSiblingFiles, &pszGeorefFilename ) );
                }
                if( m_bGeoTransformValid )
                    m_nGeoTransformGeorefSrcIndex = nIndex;

                if( pszGeorefFilename )
                {
                    CPLFree(m_pszGeorefFilename);
                    m_pszGeorefFilename = pszGeorefFilename;
                    pszGeorefFilename = nullptr;
                }
                if( m_bGeoTransformValid )
                    break;
            }
        }

/* -------------------------------------------------------------------- */
/*      Check for GCPs.                                                 */
/* -------------------------------------------------------------------- */
        if( m_nINTERNALGeorefSrcIndex >= 0 &&
            TIFFGetField(m_hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
            && !m_bGeoTransformValid )
        {
            if( m_nGCPCount > 0 )
            {
                GDALDeinitGCPs( m_nGCPCount, m_pasGCPList );
                CPLFree( m_pasGCPList );
            }
            m_nGCPCount = nCount / 6;
            m_pasGCPList =
                static_cast<GDAL_GCP *>(CPLCalloc(sizeof(GDAL_GCP), m_nGCPCount));

            for( int iGCP = 0; iGCP < m_nGCPCount; ++iGCP )
            {
                char szID[32] = {};

                snprintf( szID, sizeof(szID), "%d", iGCP + 1 );
                m_pasGCPList[iGCP].pszId = CPLStrdup( szID );
                m_pasGCPList[iGCP].pszInfo = CPLStrdup("");
                m_pasGCPList[iGCP].dfGCPPixel = padfTiePoints[iGCP*6+0];
                m_pasGCPList[iGCP].dfGCPLine = padfTiePoints[iGCP*6+1];
                m_pasGCPList[iGCP].dfGCPX = padfTiePoints[iGCP*6+3];
                m_pasGCPList[iGCP].dfGCPY = padfTiePoints[iGCP*6+4];
                m_pasGCPList[iGCP].dfGCPZ = padfTiePoints[iGCP*6+5];

                if( bPixelIsPoint && !bPointGeoIgnore )
                {
                    m_pasGCPList[iGCP].dfGCPPixel += 0.5;
                    m_pasGCPList[iGCP].dfGCPLine += 0.5;
                }
            }
            m_nGeoTransformGeorefSrcIndex = m_nINTERNALGeorefSrcIndex;
        }

/* -------------------------------------------------------------------- */
/*      Did we find a tab file?  If so we will use its coordinate       */
/*      system and give it precedence.                                  */
/* -------------------------------------------------------------------- */
        if( pszTabWKT != nullptr && m_oSRS.IsEmpty() )
        {
            m_oSRS.SetFromUserInput(pszTabWKT);
            m_bLookedForProjection = true;
        }

        CPLFree( pszTabWKT );
    }

    if( m_bLoadPam && m_nPAMGeorefSrcIndex >= 0 )
    {
/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
        CPLAssert(!m_bColorProfileMetadataChanged);
        CPLAssert(!m_bMetadataChanged);
        CPLAssert(!m_bGeoTIFFInfoChanged);
        CPLAssert(!m_bNoDataChanged);

        // We must absolutely unset m_bLoadPam now, otherwise calling
        // GetFileList() on a .tif with a .aux will result in an (almost)
        // endless sequence of calls.
        m_bLoadPam = false;

        TryLoadXML( GetSiblingFiles() );
        ApplyPamInfo();

        m_bColorProfileMetadataChanged = false;
        m_bMetadataChanged = false;
        m_bGeoTIFFInfoChanged = false;
        m_bNoDataChanged = false;

        for( int i = 1; i <= nBands; ++i )
        {
            GTiffRasterBand* poBand =
                cpl::down_cast<GTiffRasterBand *>(GetRasterBand(i));

            /* Load scale, offset and unittype from PAM if available */
            if( !poBand->m_bHaveOffsetScale )
            {
                int nHaveOffsetScale = FALSE;
                poBand->m_dfScale =
                    poBand->GDALPamRasterBand::GetScale( &nHaveOffsetScale );
                poBand->m_bHaveOffsetScale = CPL_TO_BOOL(nHaveOffsetScale);
                poBand->m_dfOffset = poBand->GDALPamRasterBand::GetOffset();
            }
            if( poBand->m_osUnitType.empty() )
            {
                const char* pszUnitType =
                    poBand->GDALPamRasterBand::GetUnitType();
                if( pszUnitType )
                    poBand->m_osUnitType = pszUnitType;
            }
            if( poBand->m_osDescription.empty() )
                poBand->m_osDescription =
                    poBand->GDALPamRasterBand::GetDescription();

            GDALColorInterp ePAMColorInterp =
                poBand->GDALPamRasterBand::GetColorInterpretation();
            if( ePAMColorInterp != GCI_Undefined )
                poBand->m_eBandInterp = ePAMColorInterp;
        }
    }
    m_bLoadPam = false;
}

/************************************************************************/
/*                   SetStructuralMDFromParent()                        */
/************************************************************************/

void GTiffDataset::SetStructuralMDFromParent(GTiffDataset* poParentDS)
{
    m_bBlockOrderRowMajor = poParentDS->m_bBlockOrderRowMajor;
    m_bLeaderSizeAsUInt4 = poParentDS->m_bLeaderSizeAsUInt4;
    m_bTrailerRepeatedLast4BytesRepeated = poParentDS->m_bTrailerRepeatedLast4BytesRepeated;
    m_bMaskInterleavedWithImagery = poParentDS->m_bMaskInterleavedWithImagery;
    m_bWriteEmptyTiles = poParentDS->m_bWriteEmptyTiles;
}

/************************************************************************/
/*                          ScanDirectories()                           */
/*                                                                      */
/*      Scan through all the directories finding overviews, masks       */
/*      and subdatasets.                                                */
/************************************************************************/

void GTiffDataset::ScanDirectories()

{
/* -------------------------------------------------------------------- */
/*      We only scan once.  We do not scan for non-base datasets.       */
/* -------------------------------------------------------------------- */
    if( !m_bScanDeferred )
        return;

    m_bScanDeferred = false;

    if( m_poBaseDS )
        return;

    Crystalize();

    CPLDebug( "GTiff", "ScanDirectories()" );

/* ==================================================================== */
/*      Scan all directories.                                           */
/* ==================================================================== */
    CPLStringList aosSubdatasets;
    int iDirIndex = 0;

    FlushDirectory();

    do
    {
        // Only libtiff 4.0.4 can handle between 32768 and 65535 directories.
#if !defined(SUPPORTS_MORE_THAN_32768_DIRECTORIES)
        if( iDirIndex == 32768 )
            break;
#endif
        toff_t nTopDir = TIFFCurrentDirOffset(m_hTIFF);
        uint32_t nSubType = 0;

        ++iDirIndex;

        toff_t *tmpSubIFDOffsets = nullptr;
        toff_t *subIFDOffsets = nullptr;
        uint16_t nSubIFDs = 0;
        if (TIFFGetField(m_hTIFF, TIFFTAG_SUBIFD, &nSubIFDs, &tmpSubIFDOffsets) && iDirIndex == 1)
        {
            subIFDOffsets = static_cast<toff_t *>(CPLMalloc(nSubIFDs * sizeof(toff_t)));
            for (uint16_t iSubIFD = 0; iSubIFD < nSubIFDs; iSubIFD++)
            {
                subIFDOffsets[iSubIFD] = tmpSubIFDOffsets[iSubIFD];
            }
        }

        //early break for backwards compatibility: if the first directory read is also the last, and there are no subIFDs, no use continuing
        if( iDirIndex==1 && nSubIFDs==0 && TIFFLastDirectory( m_hTIFF )) {
            CPLFree(subIFDOffsets);
            break;
        }


        for( uint16_t iSubIFD = 0; iSubIFD<=nSubIFDs; iSubIFD++ ) {
            toff_t nThisDir = nTopDir;
            if ( iSubIFD > 0 && iDirIndex > 1 ) //don't read subIFDs if we are not in the original directory
                break;
            if ( iSubIFD > 0 ) {
                // make static analyzer happy. subIFDOffsets cannot be null if iSubIFD>0
                assert(subIFDOffsets != nullptr);
                nThisDir = subIFDOffsets[iSubIFD-1];
                //CPLDebug("GTiff", "Opened subIFD %d/%d at offset %llu.", iSubIFD, nSubIFDs, nThisDir);
                if (!TIFFSetSubDirectory(m_hTIFF,nThisDir))
                    break;
            }


            if( !TIFFGetField(m_hTIFF, TIFFTAG_SUBFILETYPE, &nSubType) )
                nSubType = 0;

            /* Embedded overview of the main image */
            if( (nSubType & FILETYPE_REDUCEDIMAGE) != 0 &&
                (nSubType & FILETYPE_MASK) == 0 &&
                ((nSubIFDs==0 && iDirIndex != 1) || iSubIFD>0) &&
                m_nOverviewCount < 30 /* to avoid DoS */ )
            {
                GTiffDataset *poODS = new GTiffDataset();
                poODS->ShareLockWithParentDataset(this);
                poODS->SetStructuralMDFromParent(this);
                poODS->m_pszFilename = CPLStrdup(m_pszFilename);
                if( poODS->OpenOffset( VSI_TIFFOpenChild(m_hTIFF), nThisDir,
                                    eAccess ) != CE_None
                    || poODS->GetRasterCount() != GetRasterCount() )
                {
                    delete poODS;
                }
                else
                {
                    CPLDebug( "GTiff", "Opened %dx%d overview.",
                            poODS->GetRasterXSize(), poODS->GetRasterYSize());
                    ++m_nOverviewCount;
                    m_papoOverviewDS = static_cast<GTiffDataset **>(
                        CPLRealloc(m_papoOverviewDS,
                                m_nOverviewCount * (sizeof(void*))) );
                    m_papoOverviewDS[m_nOverviewCount-1] = poODS;
                    poODS->m_poBaseDS = this;
                    poODS->m_bIsOverview = true;
                }
            }
            // Embedded mask of the main image.
            else if( (nSubType & FILETYPE_MASK) != 0 &&
                    (nSubType & FILETYPE_REDUCEDIMAGE) == 0 &&
                    ((nSubIFDs==0 && iDirIndex != 1) || iSubIFD>0) &&
                    m_poMaskDS == nullptr )
            {
                m_poMaskDS = new GTiffDataset();
                m_poMaskDS->ShareLockWithParentDataset(this);
                m_poMaskDS->SetStructuralMDFromParent(this);
                m_poMaskDS->m_pszFilename = CPLStrdup(m_pszFilename);

                // The TIFF6 specification - page 37 - only allows 1
                // SamplesPerPixel and 1 BitsPerSample Here we support either 1 or
                // 8 bit per sample and we support either 1 sample per pixel or as
                // many samples as in the main image We don't check the value of
                // the PhotometricInterpretation tag, which should be set to
                // "Transparency mask" (4) according to the specification (page
                // 36).  However, the TIFF6 specification allows image masks to
                // have a higher resolution than the main image, what we don't
                // support here.

                if( m_poMaskDS->OpenOffset( VSI_TIFFOpenChild(m_hTIFF), nThisDir,
                                        eAccess ) != CE_None
                    || m_poMaskDS->GetRasterCount() == 0
                    || !(m_poMaskDS->GetRasterCount() == 1
                        || m_poMaskDS->GetRasterCount() == GetRasterCount())
                    || m_poMaskDS->GetRasterXSize() != GetRasterXSize()
                    || m_poMaskDS->GetRasterYSize() != GetRasterYSize()
                    || m_poMaskDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
                {
                    delete m_poMaskDS;
                    m_poMaskDS = nullptr;
                }
                else
                {
                    CPLDebug( "GTiff", "Opened band mask.");
                    m_poMaskDS->m_poBaseDS = this;
                    m_poMaskDS->m_poImageryDS = this;

                    m_poMaskDS->m_bPromoteTo8Bits =
                        CPLTestBool(
                            CPLGetConfigOption( "GDAL_TIFF_INTERNAL_MASK_TO_8BIT",
                                                "YES" ) );
                }
            }

            // Embedded mask of an overview.  The TIFF6 specification allows the
            // combination of the FILETYPE_xxxx masks.
            else if( (nSubType & FILETYPE_REDUCEDIMAGE) != 0 &&
                    (nSubType & FILETYPE_MASK) != 0 &&
                    ((nSubIFDs==0 && iDirIndex != 1) || iSubIFD>0))
            {
                GTiffDataset* poDS = new GTiffDataset();
                poDS->ShareLockWithParentDataset(this);
                poDS->SetStructuralMDFromParent(this);
                poDS->m_pszFilename = CPLStrdup(m_pszFilename);
                if( poDS->OpenOffset( VSI_TIFFOpenChild(m_hTIFF), nThisDir,
                                    eAccess ) != CE_None
                    || poDS->GetRasterCount() == 0
                    || poDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
                {
                    delete poDS;
                }
                else
                {
                    int i = 0;  // Used after for.
                    for( ; i < m_nOverviewCount; ++i )
                    {
                        auto poOvrDS = cpl::down_cast<GTiffDataset*>(
                                GDALDataset::FromHandle(m_papoOverviewDS[i]));
                        if( poOvrDS->m_poMaskDS == nullptr &&
                            poDS->GetRasterXSize() ==
                            m_papoOverviewDS[i]->GetRasterXSize() &&
                            poDS->GetRasterYSize() ==
                            m_papoOverviewDS[i]->GetRasterYSize() &&
                            (poDS->GetRasterCount() == 1 ||
                            poDS->GetRasterCount() == GetRasterCount()))
                        {
                            CPLDebug(
                                "GTiff", "Opened band mask for %dx%d overview.",
                                poDS->GetRasterXSize(), poDS->GetRasterYSize());
                            poDS->m_poImageryDS = poOvrDS;
                            poOvrDS->m_poMaskDS = poDS;
                            poDS->m_bPromoteTo8Bits =
                                CPLTestBool(
                                    CPLGetConfigOption(
                                        "GDAL_TIFF_INTERNAL_MASK_TO_8BIT",
                                        "YES" ) );
                            poDS->m_poBaseDS = this;
                            break;
                        }
                    }
                    if( i == m_nOverviewCount )
                    {
                        delete poDS;
                    }
                }
            }
            else if( !m_bSingleIFDOpened && (nSubType == 0 || nSubType == FILETYPE_PAGE) )
            {
                uint32_t nXSize = 0;
                uint32_t nYSize = 0;

                TIFFGetField( m_hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
                TIFFGetField( m_hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );

                // For Geodetic TIFF grids (GTG)
                // (https://proj.org/specifications/geodetictiffgrids.html)
                // extract the grid_name to put it in the description
                std::string osFriendlyName;
                char* pszText = nullptr;
                if( TIFFGetField( m_hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) &&
                    strstr(pszText, "grid_name") != nullptr )
                {
                    CPLXMLNode *psRoot = CPLParseXMLString( pszText );
                    CPLXMLNode *psItem = nullptr;

                    if( psRoot != nullptr && psRoot->eType == CXT_Element
                        && EQUAL(psRoot->pszValue,"GDALMetadata") )
                        psItem = psRoot->psChild;

                    for( ; psItem != nullptr; psItem = psItem->psNext )
                    {

                        if( psItem->eType != CXT_Element
                            || !EQUAL(psItem->pszValue,"Item") )
                            continue;

                        const char *pszKey = CPLGetXMLValue( psItem, "name", nullptr );
                        const char *pszValue = CPLGetXMLValue( psItem, nullptr, nullptr );
                        int nBand =
                            atoi(CPLGetXMLValue( psItem, "sample", "-1" ));
                        if( pszKey && pszValue && nBand <= 0 &&
                            EQUAL(pszKey, "grid_name") )
                        {
                            osFriendlyName = ": ";
                            osFriendlyName += pszValue;
                            break;
                        }
                    }

                    CPLDestroyXMLNode(psRoot);
                }

                if( nXSize > INT_MAX || nYSize > INT_MAX )
                {
                    CPLDebug("GTiff",
                            "Skipping directory with too large image: %u x %u",
                            nXSize, nYSize);
                }
                else
                {
                    uint16_t nSPP = 0;
                    if( !TIFFGetField(m_hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSPP ) )
                        nSPP = 1;

                    CPLString osName, osDesc;
                    osName.Printf( "SUBDATASET_%d_NAME=GTIFF_DIR:%d:%s",
                                iDirIndex, iDirIndex, m_pszFilename );
                    osDesc.Printf( "SUBDATASET_%d_DESC=Page %d (%dP x %dL x %dB)",
                                iDirIndex, iDirIndex,
                                static_cast<int>(nXSize),
                                static_cast<int>(nYSize),
                                nSPP );
                    osDesc += osFriendlyName;

                    aosSubdatasets.AddString(osName);
                    aosSubdatasets.AddString(osDesc);
                }
            }
        }
        CPLFree(subIFDOffsets);

        // Make sure we are stepping from the expected directory regardless
        // of churn done processing the above.
        if( TIFFCurrentDirOffset(m_hTIFF) != nTopDir )
            TIFFSetSubDirectory( m_hTIFF, nTopDir );
    } while( !m_bSingleIFDOpened && !TIFFLastDirectory( m_hTIFF ) && TIFFReadDirectory( m_hTIFF ) != 0 );


    ReloadDirectory();

    // If we have a mask for the main image, loop over the overviews, and if
    // they have a mask, let's set this mask as an overview of the main mask.
    if( m_poMaskDS != nullptr )
    {
        for( int i = 0; i < m_nOverviewCount; ++i )
        {
            if( cpl::down_cast<GTiffDataset *>(GDALDataset::FromHandle(
                   m_papoOverviewDS[i]))->m_poMaskDS != nullptr)
            {
                ++m_poMaskDS->m_nOverviewCount;
                m_poMaskDS->m_papoOverviewDS = static_cast<GTiffDataset **>(
                    CPLRealloc(m_poMaskDS->m_papoOverviewDS,
                               m_poMaskDS->m_nOverviewCount * (sizeof(void*))) );
                m_poMaskDS->m_papoOverviewDS[m_poMaskDS->m_nOverviewCount-1] =
                    cpl::down_cast<GTiffDataset*>(GDALDataset::FromHandle(
                        m_papoOverviewDS[i]))->m_poMaskDS;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Only keep track of subdatasets if we have more than one         */
/*      subdataset (pair).                                              */
/* -------------------------------------------------------------------- */
    if( aosSubdatasets.size() > 2 )
    {
        m_oGTiffMDMD.SetMetadata( aosSubdatasets.List(), "SUBDATASETS" );
    }
}

static signed char GTiffGetLZMAPreset(char** papszOptions)
{
    int nLZMAPreset = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "LZMA_PRESET" );
    if( pszValue != nullptr )
    {
        nLZMAPreset = atoi( pszValue );
        if( !(nLZMAPreset >= 0 && nLZMAPreset <= 9) )
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "LZMA_PRESET=%s value not recognised, ignoring.",
                      pszValue );
            nLZMAPreset = -1;
        }
    }
    return static_cast<signed char>(nLZMAPreset);
}

static signed char GTiffGetZSTDPreset(char** papszOptions)
{
    int nZSTDLevel = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "ZSTD_LEVEL" );
    if( pszValue != nullptr )
    {
        nZSTDLevel = atoi( pszValue );
        if( !(nZSTDLevel >= 1 && nZSTDLevel <= 22) )
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "ZSTD_LEVEL=%s value not recognised, ignoring.",
                      pszValue );
            nZSTDLevel = -1;
        }
    }
    return static_cast<signed char>(nZSTDLevel);
}

static double GTiffGetLERCMaxZError(char** papszOptions)
{
    return CPLAtof(CSLFetchNameValueDef( papszOptions, "MAX_Z_ERROR", "0.0") );
}

static signed char GTiffGetWebPLevel(char** papszOptions)
{
    int nWebPLevel = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "WEBP_LEVEL" );
    if( pszValue != nullptr )
    {
        nWebPLevel = atoi( pszValue );
        if( !(nWebPLevel >= 1 && nWebPLevel <= 100) )
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "WEBP_LEVEL=%s value not recognised, ignoring.",
                      pszValue );
            nWebPLevel = -1;
        }
    }
    return static_cast<signed char>(nWebPLevel);
}

static bool GTiffGetWebPLossless(char** papszOptions)
{
    return CPLFetchBool( papszOptions, "WEBP_LOSSLESS", false);
}

static signed char GTiffGetZLevel(char** papszOptions)
{
    int nZLevel = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "ZLEVEL" );
    if( pszValue != nullptr )
    {
        nZLevel = atoi( pszValue );
#ifdef TIFFTAG_DEFLATE_SUBCODEC
        constexpr int nMaxLevel = 12;
#ifndef LIBDEFLATE_SUPPORT
        if( nZLevel > 9 && nZLevel <= nMaxLevel )
        {
            CPLDebug("GTiff",
                     "ZLEVEL=%d not supported in a non-libdeflate enabled "
                     "libtiff build. Capping to 9",
                     nZLevel);
            nZLevel = 9;
        }
#endif
#else
        constexpr int nMaxLevel = 9;
#endif
        if( nZLevel < 1 || nZLevel > nMaxLevel )
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "ZLEVEL=%s value not recognised, ignoring.",
                      pszValue );
            nZLevel = -1;
        }
    }
    return static_cast<signed char>(nZLevel);
}

static signed char GTiffGetJpegQuality(char** papszOptions)
{
    int nJpegQuality = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "JPEG_QUALITY" );
    if( pszValue != nullptr )
    {
        nJpegQuality = atoi( pszValue );
        if( nJpegQuality < 1 || nJpegQuality > 100 )
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "JPEG_QUALITY=%s value not recognised, ignoring.",
                      pszValue );
            nJpegQuality = -1;
        }
    }
    return static_cast<signed char>(nJpegQuality);
}

static signed char GTiffGetJpegTablesMode(char** papszOptions)
{
    return static_cast<signed char>(
        atoi(CSLFetchNameValueDef( papszOptions, "JPEGTABLESMODE",
                                      CPLSPrintf("%d",
                                                knGTIFFJpegTablesModeDefault))));
}

/************************************************************************/
/*                        GetDiscardLsbOption()                         */
/************************************************************************/

void GTiffDataset::GetDiscardLsbOption(char** papszOptions)
{
    const char* pszBits = CSLFetchNameValue( papszOptions, "DISCARD_LSB" );
    if( pszBits == nullptr)
        return;

    if( m_nPhotometric == PHOTOMETRIC_PALETTE )
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                 "DISCARD_LSB ignored on a paletted image");
        return;
    }
    if( !(m_nBitsPerSample == 8 || m_nBitsPerSample == 16 || m_nBitsPerSample == 32) )
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                 "DISCARD_LSB ignored on non 8, 16 or 32 bits integer images");
        return;
    }

    char** papszTokens = CSLTokenizeString2( pszBits, ",", 0 );
    const int nTokens = CSLCount(papszTokens);
    if( nTokens == 1 || nTokens == nBands )
    {
        m_panMaskOffsetLsb = static_cast<MaskOffset*>(CPLCalloc(nBands, sizeof(MaskOffset)));
        for( int i = 0; i < nBands; ++i )
        {
            int nBits = atoi(papszTokens[nTokens == 1 ? 0 : i]);
            m_panMaskOffsetLsb[i].nMask = ~((1 << nBits)-1);
            if( nBits > 1 )
                m_panMaskOffsetLsb[i].nOffset = 1 << (nBits - 1);
        }
    }
    else
    {
        ReportError(CE_Warning, CPLE_AppDefined,
                 "DISCARD_LSB ignored: wrong number of components");
    }
    CSLDestroy(papszTokens);
}

/************************************************************************/
/*                             GetProfile()                             */
/************************************************************************/

static GTiffProfile GetProfile(const char* pszProfile)
{
    GTiffProfile eProfile = GTiffProfile::GDALGEOTIFF;
    if( pszProfile != nullptr )
    {
        if( EQUAL(pszProfile, szPROFILE_BASELINE) )
            eProfile = GTiffProfile::BASELINE;
        else if( EQUAL(pszProfile, szPROFILE_GeoTIFF) )
            eProfile = GTiffProfile::GEOTIFF;
        else if( !EQUAL(pszProfile, szPROFILE_GDALGeoTIFF) )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for PROFILE: %s", pszProfile);
        }
    }
    return eProfile;
}

/************************************************************************/
/*                            GTiffCreate()                             */
/*                                                                      */
/*      Shared functionality between GTiffDataset::Create() and         */
/*      GTiffCreateCopy() for creating TIFF file based on a set of      */
/*      options and a configuration.                                    */
/************************************************************************/

TIFF *GTiffDataset::CreateLL( const char * pszFilename,
                              int nXSize, int nYSize, int l_nBands,
                              GDALDataType eType,
                              double dfExtraSpaceForOverviews,
                              char **papszParamList,
                              VSILFILE** pfpL,
                              CPLString& l_osTmpFilename )

{
    if( !GTiffOneTimeInit() )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Blow on a few errors.                                           */
/* -------------------------------------------------------------------- */
    if( nXSize < 1 || nYSize < 1 || l_nBands < 1 )
    {
        ReportError( pszFilename, CE_Failure, CPLE_AppDefined,
            "Attempt to create %dx%dx%d TIFF file, but width, height and bands"
            "must be positive.",
            nXSize, nYSize, l_nBands );

        return nullptr;
    }

    if( l_nBands > 65535 )
    {
        ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
                  "Attempt to create %dx%dx%d TIFF file, but bands "
                  "must be lesser or equal to 65535.",
                  nXSize, nYSize, l_nBands );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Setup values based on options.                                  */
/* -------------------------------------------------------------------- */
    const GTiffProfile eProfile = GetProfile(
        CSLFetchNameValue(papszParamList, "PROFILE"));

    const bool bTiled = CPLFetchBool( papszParamList, "TILED", false );

    int l_nBlockXSize = 0;
    const char *pszValue = CSLFetchNameValue(papszParamList, "BLOCKXSIZE");
    if( pszValue != nullptr )
    {
        l_nBlockXSize = atoi( pszValue );
        if( l_nBlockXSize < 0 )
        {
            ReportError( pszFilename,CE_Failure, CPLE_IllegalArg,
                     "Invalid value for BLOCKXSIZE");
            return nullptr;
        }
    }

    int l_nBlockYSize = 0;
    pszValue = CSLFetchNameValue(papszParamList, "BLOCKYSIZE");
    if( pszValue != nullptr )
    {
        l_nBlockYSize = atoi( pszValue );
        if( l_nBlockYSize < 0 )
        {
            ReportError( pszFilename, CE_Failure, CPLE_IllegalArg,
                     "Invalid value for BLOCKYSIZE");
            return nullptr;
        }
    }

    if( bTiled )
    {
        if( l_nBlockXSize == 0 )
            l_nBlockXSize = 256;

        if( l_nBlockYSize == 0 )
            l_nBlockYSize = 256;
    }

    int nPlanar = 0;
    pszValue = CSLFetchNameValue(papszParamList, "INTERLEAVE");
    if( pszValue != nullptr )
    {
        if( EQUAL( pszValue, "PIXEL" ) )
            nPlanar = PLANARCONFIG_CONTIG;
        else if( EQUAL( pszValue, "BAND" ) )
        {
            nPlanar = PLANARCONFIG_SEPARATE;
        }
        else
        {
            ReportError( pszFilename,CE_Failure, CPLE_IllegalArg,
                      "INTERLEAVE=%s unsupported, value must be PIXEL or BAND.",
                      pszValue );
            return nullptr;
        }
    }
    else
    {
        nPlanar = PLANARCONFIG_CONTIG;
    }

    int l_nCompression = COMPRESSION_NONE;
    pszValue = CSLFetchNameValue( papszParamList, "COMPRESS" );
    if( pszValue != nullptr )
    {
        l_nCompression = GTIFFGetCompressionMethod(pszValue, "COMPRESS");
        if( l_nCompression < 0 )
            return nullptr;
    }

    int nPredictor = PREDICTOR_NONE;
    pszValue = CSLFetchNameValue( papszParamList, "PREDICTOR" );
    if( pszValue != nullptr )
        nPredictor = atoi( pszValue );

    const int l_nZLevel = GTiffGetZLevel(papszParamList);
    const int l_nLZMAPreset = GTiffGetLZMAPreset(papszParamList);
    const int l_nZSTDLevel = GTiffGetZSTDPreset(papszParamList);
    const int l_nWebPLevel = GTiffGetWebPLevel(papszParamList);
    const bool l_bWebPLossless = GTiffGetWebPLossless(papszParamList);
    const int l_nJpegQuality = GTiffGetJpegQuality(papszParamList);
    const int l_nJpegTablesMode = GTiffGetJpegTablesMode(papszParamList);
    const double l_dfMaxZError = GTiffGetLERCMaxZError(papszParamList);

/* -------------------------------------------------------------------- */
/*      Streaming related code                                          */
/* -------------------------------------------------------------------- */
    const CPLString osOriFilename(pszFilename);
    bool bStreaming =
        strcmp(pszFilename, "/vsistdout/") == 0 ||
        CPLFetchBool(papszParamList, "STREAMABLE_OUTPUT", false);
#ifdef S_ISFIFO
    if( !bStreaming )
    {
        VSIStatBufL sStat;
        if( VSIStatExL( pszFilename, &sStat,
                        VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
             S_ISFIFO(sStat.st_mode) )
        {
            bStreaming = true;
        }
    }
#endif
    if( bStreaming &&
        !EQUAL( "NONE",
                CSLFetchNameValueDef(papszParamList, "COMPRESS", "NONE")) )
    {
        ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
            "Streaming only supported to uncompressed TIFF" );
        return nullptr;
    }
    if( bStreaming && CPLFetchBool(papszParamList, "SPARSE_OK", false) )
    {
        ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
            "Streaming not supported with SPARSE_OK" );
        return nullptr;
    }
    const bool bCopySrcOverviews = CPLFetchBool(papszParamList, "COPY_SRC_OVERVIEWS", false);
    if( bStreaming && bCopySrcOverviews )
    {
        ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
            "Streaming not supported with COPY_SRC_OVERVIEWS" );
        return nullptr;
    }
    if( bStreaming )
    {
        static int nCounter = 0;
        l_osTmpFilename = CPLSPrintf("/vsimem/vsistdout_%d.tif", ++nCounter);
        pszFilename = l_osTmpFilename.c_str();
    }

/* -------------------------------------------------------------------- */
/*      Compute the uncompressed size.                                  */
/* -------------------------------------------------------------------- */
    const double dfUncompressedImageSize =
        nXSize * static_cast<double>(nYSize) * l_nBands *
        GDALGetDataTypeSizeBytes(eType)
        + dfExtraSpaceForOverviews;

/* -------------------------------------------------------------------- */
/*      Should the file be created as a bigtiff file?                   */
/* -------------------------------------------------------------------- */
    const char *pszBIGTIFF = CSLFetchNameValue(papszParamList, "BIGTIFF");

    if( pszBIGTIFF == nullptr )
        pszBIGTIFF = "IF_NEEDED";

    bool bCreateBigTIFF = false;
    if( EQUAL(pszBIGTIFF, "IF_NEEDED") )
    {
        if( l_nCompression == COMPRESSION_NONE
            && dfUncompressedImageSize > 4200000000.0 )
            bCreateBigTIFF = true;
    }
    else if( EQUAL(pszBIGTIFF, "IF_SAFER") )
    {
        if( dfUncompressedImageSize > 2000000000.0 )
            bCreateBigTIFF = true;
    }
    else
    {
        bCreateBigTIFF = CPLTestBool( pszBIGTIFF );
        if( !bCreateBigTIFF && l_nCompression == COMPRESSION_NONE &&
             dfUncompressedImageSize > 4200000000.0 )
        {
            ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
                "The TIFF file will be larger than 4GB, so BigTIFF is "
                "necessary.  Creation failed.");
            return nullptr;
        }
    }

    if( bCreateBigTIFF )
        CPLDebug( "GTiff", "File being created as a BigTIFF." );

/* -------------------------------------------------------------------- */
/*      Sanity check.                                                   */
/* -------------------------------------------------------------------- */
    if( bTiled )
    {
        unsigned nTileXCount = DIV_ROUND_UP(nXSize, l_nBlockXSize);
        unsigned nTileYCount = DIV_ROUND_UP(nYSize, l_nBlockYSize);
        // libtiff implementation limitation
        if( nTileXCount > 0x80000000U / (bCreateBigTIFF ? 8 : 4) / nTileYCount )
        {
            ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
                     "File too large regarding tile size. This would result "
                     "in a file with tile arrays larger than 2GB");
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check free space (only for big, non sparse, uncompressed)       */
/* -------------------------------------------------------------------- */
    if( l_nCompression == COMPRESSION_NONE &&
        dfUncompressedImageSize >= 1e9 &&
        !CPLFetchBool(papszParamList, "SPARSE_OK", false) &&
        osOriFilename != "/vsistdout/" &&
        osOriFilename != "/vsistdout_redirect/" &&
        CPLTestBool(CPLGetConfigOption("CHECK_DISK_FREE_SPACE", "TRUE")) )
    {
        GIntBig nFreeDiskSpace =
            VSIGetDiskFreeSpace(CPLGetDirname(pszFilename));
        if( nFreeDiskSpace >= 0 &&
            nFreeDiskSpace < dfUncompressedImageSize )
        {
            ReportError( pszFilename, CE_Failure, CPLE_FileIO,
                      "Free disk space available is " CPL_FRMT_GIB " bytes, "
                      "whereas " CPL_FRMT_GIB " are at least necessary. "
                      "You can disable this check by defining the "
                      "CHECK_DISK_FREE_SPACE configuration option to FALSE.",
                      nFreeDiskSpace,
                      static_cast<GIntBig>(dfUncompressedImageSize) );
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if the user wishes a particular endianness                */
/* -------------------------------------------------------------------- */

    int eEndianness = ENDIANNESS_NATIVE;
    pszValue = CSLFetchNameValue(papszParamList, "ENDIANNESS");
    if( pszValue == nullptr )
        pszValue = CPLGetConfigOption( "GDAL_TIFF_ENDIANNESS", nullptr );
    if( pszValue != nullptr )
    {
        if( EQUAL(pszValue, "LITTLE") )
        {
            eEndianness = ENDIANNESS_LITTLE;
        }
        else if( EQUAL(pszValue, "BIG") )
        {
            eEndianness = ENDIANNESS_BIG;
        }
        else if( EQUAL(pszValue, "INVERTED") )
        {
#ifdef CPL_LSB
            eEndianness = ENDIANNESS_BIG;
#else
            eEndianness = ENDIANNESS_LITTLE;
#endif
        }
        else if( !EQUAL(pszValue, "NATIVE") )
        {
            ReportError( pszFilename, CE_Warning, CPLE_NotSupported,
                "ENDIANNESS=%s not supported. Defaulting to NATIVE", pszValue );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */

    const bool bAppend = CPLFetchBool(papszParamList, "APPEND_SUBDATASET", false);

    char szOpeningFlag[5] = {};
    strcpy(szOpeningFlag, bAppend ? "r+" : "w+");
    if( bCreateBigTIFF )
        strcat(szOpeningFlag, "8");
    if( eEndianness == ENDIANNESS_BIG )
        strcat(szOpeningFlag, "b");
    else if( eEndianness == ENDIANNESS_LITTLE )
        strcat(szOpeningFlag, "l");

    VSILFILE* l_fpL = VSIFOpenL( pszFilename, bAppend ? "r+b" : "w+b" );
    if( l_fpL == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create new tiff file `%s' failed: %s",
                  pszFilename, VSIStrerror(errno) );
        return nullptr;
    }
    TIFF *l_hTIFF = VSI_TIFFOpen( pszFilename, szOpeningFlag, l_fpL );
    if( l_hTIFF == nullptr )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attempt to create new tiff file `%s' "
                      "failed in XTIFFOpen().",
                      pszFilename );
        CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
        return nullptr;
    }

    if( bAppend )
    {
        // This is a bit of a hack to cause (*tif->tif_cleanup)(tif); to be called.
        // See https://trac.osgeo.org/gdal/ticket/2055
        TIFFSetField( l_hTIFF, TIFFTAG_COMPRESSION, COMPRESSION_NONE );
        TIFFFreeDirectory( l_hTIFF );
        TIFFCreateDirectory( l_hTIFF );
    }

/* -------------------------------------------------------------------- */
/*      How many bits per sample?  We have a special case if NBITS      */
/*      specified for GDT_Byte, GDT_UInt16, GDT_UInt32.                 */
/* -------------------------------------------------------------------- */
    int l_nBitsPerSample = GDALGetDataTypeSizeBits(eType);
    if( CSLFetchNameValue(papszParamList, "NBITS") != nullptr )
    {
        int nMinBits = 0;
        int nMaxBits = 0;
        l_nBitsPerSample = atoi(CSLFetchNameValue(papszParamList, "NBITS"));
        if( eType == GDT_Byte )
        {
            nMinBits = 1;
            nMaxBits = 8;
        }
        else if( eType == GDT_UInt16 )
        {
            nMinBits = 9;
            nMaxBits = 16;
        }
        else if( eType == GDT_UInt32 )
        {
            nMinBits = 17;
            nMaxBits = 32;
        }
        else if( eType == GDT_Float32 )
        {
            if( l_nBitsPerSample != 16 && l_nBitsPerSample != 32 )
            {
                ReportError( pszFilename, CE_Warning, CPLE_NotSupported,
                     "Only NBITS=16 is supported for data type Float32");
                l_nBitsPerSample = GDALGetDataTypeSizeBits(eType);
            }
        }
        else
        {
            ReportError( pszFilename, CE_Warning, CPLE_NotSupported,
                     "NBITS is not supported for data type %s",
                     GDALGetDataTypeName(eType));
            l_nBitsPerSample = GDALGetDataTypeSizeBits(eType);
        }

        if( nMinBits != 0 )
        {
            if( l_nBitsPerSample < nMinBits )
            {
                ReportError( pszFilename,CE_Warning, CPLE_AppDefined,
                         "NBITS=%d is invalid for data type %s. Using NBITS=%d",
                         l_nBitsPerSample, GDALGetDataTypeName(eType),
                         nMinBits);
                l_nBitsPerSample = nMinBits;
            }
            else if( l_nBitsPerSample > nMaxBits )
            {
                ReportError( pszFilename,CE_Warning, CPLE_AppDefined,
                         "NBITS=%d is invalid for data type %s. Using NBITS=%d",
                         l_nBitsPerSample, GDALGetDataTypeName(eType),
                         nMaxBits);
                l_nBitsPerSample = nMaxBits;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a custom pixel type (just used for signed byte now). */
/* -------------------------------------------------------------------- */
    const char *pszPixelType = CSLFetchNameValue( papszParamList, "PIXELTYPE" );
    if( pszPixelType == nullptr )
        pszPixelType = "";

/* -------------------------------------------------------------------- */
/*      Setup some standard flags.                                      */
/* -------------------------------------------------------------------- */
    TIFFSetField( l_hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( l_hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( l_hTIFF, TIFFTAG_BITSPERSAMPLE, l_nBitsPerSample );

    uint16_t l_nSampleFormat = 0;
    if( (eType == GDT_Byte && EQUAL(pszPixelType,"SIGNEDBYTE"))
        || eType == GDT_Int16 || eType == GDT_Int32 )
        l_nSampleFormat = SAMPLEFORMAT_INT;
    else if( eType == GDT_CInt16 || eType == GDT_CInt32 )
        l_nSampleFormat = SAMPLEFORMAT_COMPLEXINT;
    else if( eType == GDT_Float32 || eType == GDT_Float64 )
        l_nSampleFormat = SAMPLEFORMAT_IEEEFP;
    else if( eType == GDT_CFloat32 || eType == GDT_CFloat64 )
        l_nSampleFormat = SAMPLEFORMAT_COMPLEXIEEEFP;
    else
        l_nSampleFormat = SAMPLEFORMAT_UINT;

    TIFFSetField( l_hTIFF, TIFFTAG_SAMPLEFORMAT, l_nSampleFormat );
    TIFFSetField( l_hTIFF, TIFFTAG_SAMPLESPERPIXEL, l_nBands );
    TIFFSetField( l_hTIFF, TIFFTAG_PLANARCONFIG, nPlanar );

/* -------------------------------------------------------------------- */
/*      Setup Photometric Interpretation. Take this value from the user */
/*      passed option or guess correct value otherwise.                 */
/* -------------------------------------------------------------------- */
    int nSamplesAccountedFor = 1;
    bool bForceColorTable = false;

    pszValue = CSLFetchNameValue(papszParamList,"PHOTOMETRIC");
    if( pszValue != nullptr )
    {
        if( EQUAL( pszValue, "MINISBLACK" ) )
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC,
                          PHOTOMETRIC_MINISBLACK );
        else if( EQUAL( pszValue, "MINISWHITE" ) )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC,
                          PHOTOMETRIC_MINISWHITE );
        }
        else if( EQUAL( pszValue, "PALETTE" ))
        {
            if( eType == GDT_Byte || eType == GDT_UInt16 )
            {
                TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC,
                              PHOTOMETRIC_PALETTE );
                nSamplesAccountedFor = 1;
                bForceColorTable = true;
            }
            else
            {
                ReportError( pszFilename,CE_Warning, CPLE_AppDefined,
                    "PHOTOMETRIC=PALETTE only compatible with Byte or UInt16" );
            }
        }
        else if( EQUAL( pszValue, "RGB" ))
        {
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "CMYK" ))
        {
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED );
            nSamplesAccountedFor = 4;
        }
        else if( EQUAL( pszValue, "YCBCR" ))
        {
            // Because of subsampling, setting YCBCR without JPEG compression
            // leads to a crash currently. Would need to make
            // GTiffRasterBand::IWriteBlock() aware of subsampling so that it
            // doesn't overrun buffer size returned by libtiff.
            if( l_nCompression != COMPRESSION_JPEG )
            {
                ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
                         "Currently, PHOTOMETRIC=YCBCR requires COMPRESS=JPEG");
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }

            if( nPlanar == PLANARCONFIG_SEPARATE )
            {
                ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
                         "PHOTOMETRIC=YCBCR requires INTERLEAVE=PIXEL");
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }

            // YCBCR strictly requires 3 bands. Not less, not more Issue an
            // explicit error message as libtiff one is a bit cryptic:
            // TIFFVStripSize64:Invalid td_samplesperpixel value.
            if( l_nBands != 3 )
            {
                ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
                    "PHOTOMETRIC=YCBCR not supported on a %d-band raster: "
                    "only compatible of a 3-band (RGB) raster", l_nBands );
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }

            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_YCBCR );
            nSamplesAccountedFor = 3;

            // Explicitly register the subsampling so that JPEGFixupTags
            // is a no-op (helps for cloud optimized geotiffs)
            TIFFSetField( l_hTIFF, TIFFTAG_YCBCRSUBSAMPLING, 2, 2 );
        }
        else if( EQUAL( pszValue, "CIELAB" ))
        {
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CIELAB );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "ICCLAB" ))
        {
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ICCLAB );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "ITULAB" ))
        {
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ITULAB );
            nSamplesAccountedFor = 3;
        }
        else
        {
            ReportError( pszFilename,CE_Warning, CPLE_IllegalArg,
                      "PHOTOMETRIC=%s value not recognised, ignoring.  "
                      "Set the Photometric Interpretation as MINISBLACK.",
                      pszValue );
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        }

        if( l_nBands < nSamplesAccountedFor )
        {
            ReportError( pszFilename,CE_Warning, CPLE_IllegalArg,
                      "PHOTOMETRIC=%s value does not correspond to number "
                      "of bands (%d), ignoring.  "
                      "Set the Photometric Interpretation as MINISBLACK.",
                      pszValue, l_nBands );
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        }
    }
    else
    {
        // If image contains 3 or 4 bands and datatype is Byte then we will
        // assume it is RGB. In all other cases assume it is MINISBLACK.
        if( l_nBands == 3 && eType == GDT_Byte )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
            nSamplesAccountedFor = 3;
        }
        else if( l_nBands == 4 && eType == GDT_Byte )
        {
            uint16_t v[1] = {
                GTiffGetAlphaValue(CSLFetchNameValue(papszParamList, "ALPHA"),
                                   DEFAULT_ALPHA_TYPE)
            };

            TIFFSetField( l_hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
            nSamplesAccountedFor = 4;
        }
        else
        {
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            nSamplesAccountedFor = 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      If there are extra samples, we need to mark them with an        */
/*      appropriate extrasamples definition here.                       */
/* -------------------------------------------------------------------- */
    if( l_nBands > nSamplesAccountedFor )
    {
        const int nExtraSamples = l_nBands - nSamplesAccountedFor;

        uint16_t *v = static_cast<uint16_t *>(
            CPLMalloc( sizeof(uint16_t) * nExtraSamples ) );

        v[0] = GTiffGetAlphaValue( CSLFetchNameValue(papszParamList, "ALPHA"),
                                   EXTRASAMPLE_UNSPECIFIED );

        for( int i = 1; i < nExtraSamples; ++i )
            v[i] = EXTRASAMPLE_UNSPECIFIED;

        TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, nExtraSamples, v );

        CPLFree(v);
    }

    // Set the ICC color profile.
    if( eProfile != GTiffProfile::BASELINE )
    {
        SaveICCProfile(nullptr, l_hTIFF, papszParamList, l_nBitsPerSample);
    }

    // Set the compression method before asking the default strip size
    // This is useful when translating to a JPEG-In-TIFF file where
    // the default strip size is 8 or 16 depending on the photometric value.
    TIFFSetField( l_hTIFF, TIFFTAG_COMPRESSION, l_nCompression );

    if( l_nCompression == COMPRESSION_LERC )
    {
        const char* pszCompress =
            CSLFetchNameValueDef( papszParamList, "COMPRESS", "" );
        if( EQUAL(pszCompress , "LERC_DEFLATE" ) )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_LERC_ADD_COMPRESSION,
                        LERC_ADD_COMPRESSION_DEFLATE );
        }
        else if( EQUAL(pszCompress, "LERC_ZSTD" ) )
        {
            if( TIFFSetField( l_hTIFF, TIFFTAG_LERC_ADD_COMPRESSION,
                            LERC_ADD_COMPRESSION_ZSTD ) != 1 )
            {
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return nullptr;
            }
        }
    }
    // TODO later: take into account LERC version

/* -------------------------------------------------------------------- */
/*      Setup tiling/stripping flags.                                   */
/* -------------------------------------------------------------------- */
    if( bTiled )
    {
        if( !TIFFSetField( l_hTIFF, TIFFTAG_TILEWIDTH, l_nBlockXSize ) ||
            !TIFFSetField( l_hTIFF, TIFFTAG_TILELENGTH, l_nBlockYSize ) )
        {
            XTIFFClose(l_hTIFF);
            CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
            return nullptr;
        }
    }
    else
    {
        const uint32_t l_nRowsPerStrip = std::min(nYSize,
            l_nBlockYSize == 0
            ? static_cast<int>(TIFFDefaultStripSize(l_hTIFF,0))
            : l_nBlockYSize );

        TIFFSetField( l_hTIFF, TIFFTAG_ROWSPERSTRIP, l_nRowsPerStrip );
    }

/* -------------------------------------------------------------------- */
/*      Set compression related tags.                                   */
/* -------------------------------------------------------------------- */
    if( l_nCompression == COMPRESSION_LZW ||
         l_nCompression == COMPRESSION_ADOBE_DEFLATE ||
         l_nCompression == COMPRESSION_ZSTD )
        TIFFSetField( l_hTIFF, TIFFTAG_PREDICTOR, nPredictor );
    if( l_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        l_nCompression == COMPRESSION_LERC )
    {
        GTiffSetDeflateSubCodec(l_hTIFF);

        if( l_nZLevel != -1 )
            TIFFSetField( l_hTIFF, TIFFTAG_ZIPQUALITY, l_nZLevel );
    }
    if( l_nCompression == COMPRESSION_JPEG && l_nJpegQuality != -1 )
        TIFFSetField( l_hTIFF, TIFFTAG_JPEGQUALITY, l_nJpegQuality );
    if( l_nCompression == COMPRESSION_LZMA && l_nLZMAPreset != -1)
        TIFFSetField( l_hTIFF, TIFFTAG_LZMAPRESET, l_nLZMAPreset );
    if( (l_nCompression == COMPRESSION_ZSTD ||
         l_nCompression == COMPRESSION_LERC) && l_nZSTDLevel != -1)
        TIFFSetField( l_hTIFF, TIFFTAG_ZSTD_LEVEL, l_nZSTDLevel);
    if( l_nCompression == COMPRESSION_LERC )
    {
        TIFFSetField( l_hTIFF, TIFFTAG_LERC_MAXZERROR, l_dfMaxZError );
    }
    if( l_nCompression == COMPRESSION_WEBP && l_nWebPLevel != -1)
        TIFFSetField( l_hTIFF, TIFFTAG_WEBP_LEVEL, l_nWebPLevel);
    if( l_nCompression == COMPRESSION_WEBP && l_bWebPLossless)
        TIFFSetField( l_hTIFF, TIFFTAG_WEBP_LOSSLESS, 1);

    if( l_nCompression == COMPRESSION_JPEG )
        TIFFSetField( l_hTIFF, TIFFTAG_JPEGTABLESMODE, l_nJpegTablesMode );

/* -------------------------------------------------------------------- */
/*      If we forced production of a file with photometric=palette,     */
/*      we need to push out a default color table.                      */
/* -------------------------------------------------------------------- */
    if( bForceColorTable )
    {
        const int nColors = eType == GDT_Byte ? 256 : 65536;

        unsigned short *panTRed = static_cast<unsigned short *>(
            CPLMalloc(sizeof(unsigned short)*nColors) );
        unsigned short *panTGreen = static_cast<unsigned short *>(
            CPLMalloc(sizeof(unsigned short)*nColors) );
        unsigned short *panTBlue = static_cast<unsigned short *>(
            CPLMalloc(sizeof(unsigned short)*nColors) );

        for( int iColor = 0; iColor < nColors; ++iColor )
        {
            if( eType == GDT_Byte )
            {
                panTRed[iColor] = static_cast<unsigned short>(257 * iColor);
                panTGreen[iColor] = static_cast<unsigned short>(257 * iColor);
                panTBlue[iColor] = static_cast<unsigned short>(257 * iColor);
            }
            else
            {
                panTRed[iColor] = static_cast<unsigned short>(iColor);
                panTGreen[iColor] = static_cast<unsigned short>(iColor);
                panTBlue[iColor] = static_cast<unsigned short>(iColor);
            }
        }

        TIFFSetField( l_hTIFF, TIFFTAG_COLORMAP,
                      panTRed, panTGreen, panTBlue );

        CPLFree( panTRed );
        CPLFree( panTGreen );
        CPLFree( panTBlue );
    }

    // This trick
    // creates a temporary in-memory file and fetches its JPEG tables so that
    // we can directly set them, before tif_jpeg.c compute them at the first
    // strip/tile writing, which is too late, since we have already crystalized
    // the directory. This way we avoid a directory rewriting.
    if( l_nCompression == COMPRESSION_JPEG &&
        !STARTS_WITH(pszFilename, szJPEGGTiffDatasetTmpPrefix) &&
        CPLTestBool(
            CSLFetchNameValueDef(papszParamList, "WRITE_JPEGTABLE_TAG", "YES")) )
    {
        GTiffWriteJPEGTables( l_hTIFF,
                              CSLFetchNameValue(papszParamList, "PHOTOMETRIC"),
                              CSLFetchNameValue(papszParamList, "JPEG_QUALITY"),
                              CSLFetchNameValue(papszParamList,
                                                "JPEGTABLESMODE") );
    }

    *pfpL = l_fpL;

    return l_hTIFF;
}

/************************************************************************/
/*                      GTiffWriteJPEGTables()                          */
/*                                                                      */
/*      Sets the TIFFTAG_JPEGTABLES (and TIFFTAG_REFERENCEBLACKWHITE)   */
/*      tags immediately, instead of relying on the TIFF JPEG codec     */
/*      to write them when it starts compressing imagery. This avoids   */
/*      an IFD rewrite at the end of the file.                          */
/*      Must be used after having set TIFFTAG_SAMPLESPERPIXEL,          */
/*      TIFFTAG_BITSPERSAMPLE.                                          */
/************************************************************************/

void GTiffWriteJPEGTables( TIFF* hTIFF,
                           const char* pszPhotometric,
                           const char* pszJPEGQuality,
                           const char* pszJPEGTablesMode )
{
    // This trick
    // creates a temporary in-memory file and fetches its JPEG tables so that
    // we can directly set them, before tif_jpeg.c compute them at the first
    // strip/tile writing, which is too late, since we have already crystalized
    // the directory. This way we avoid a directory rewriting.
    uint16_t nBands = 0;
    if( !TIFFGetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL,
                        &nBands ) )
        nBands = 1;

    uint16_t l_nBitsPerSample = 0;
    if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE,
                        &(l_nBitsPerSample)) )
        l_nBitsPerSample = 1;

    CPLString osTmpFilenameIn;
    osTmpFilenameIn.Printf("%s%p", szJPEGGTiffDatasetTmpPrefix, hTIFF);
    VSILFILE* fpTmp = nullptr;
    CPLString osTmp;
    char** papszLocalParameters = nullptr;
    const int nInMemImageWidth = 16;
    const int nInMemImageHeight = 16;
    papszLocalParameters = CSLSetNameValue( papszLocalParameters,
                                            "COMPRESS", "JPEG" );
    papszLocalParameters = CSLSetNameValue( papszLocalParameters,
                                            "JPEG_QUALITY",
                                            pszJPEGQuality );
    papszLocalParameters = CSLSetNameValue( papszLocalParameters,
                                            "PHOTOMETRIC", pszPhotometric );
    papszLocalParameters = CSLSetNameValue( papszLocalParameters,
                                            "BLOCKYSIZE",
                                            CPLSPrintf("%u", nInMemImageHeight)
                                          );
    papszLocalParameters = CSLSetNameValue( papszLocalParameters,
                                            "NBITS",
                                            CPLSPrintf("%u", l_nBitsPerSample));
    papszLocalParameters = CSLSetNameValue( papszLocalParameters,
                                            "JPEGTABLESMODE",
                                            pszJPEGTablesMode );

    TIFF* hTIFFTmp = GTiffDataset::CreateLL(
                    osTmpFilenameIn, nInMemImageWidth, nInMemImageHeight,
                    (nBands <= 4) ? nBands : 1,
                    (l_nBitsPerSample <= 8) ? GDT_Byte : GDT_UInt16, 0.0,
                    papszLocalParameters, &fpTmp, osTmp );
    CSLDestroy(papszLocalParameters);
    if( hTIFFTmp )
    {
        uint16_t l_nPhotometric = 0;
        int nJpegTablesModeIn = 0;
        TIFFGetField( hTIFFTmp, TIFFTAG_PHOTOMETRIC, &(l_nPhotometric) );
        TIFFGetField( hTIFFTmp, TIFFTAG_JPEGTABLESMODE,
                        &nJpegTablesModeIn );
        TIFFWriteCheck( hTIFFTmp, FALSE, "CreateLL" );
        TIFFWriteDirectory( hTIFFTmp );
        TIFFSetDirectory( hTIFFTmp, 0 );
        // Now, reset quality and jpegcolormode.
        const int l_nJpegQuality = pszJPEGQuality ? atoi(pszJPEGQuality) : 0;
        if(l_nJpegQuality > 0)
            TIFFSetField(hTIFFTmp, TIFFTAG_JPEGQUALITY, l_nJpegQuality);
        if( l_nPhotometric == PHOTOMETRIC_YCBCR
            && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                                "YES") ) )
        {
            TIFFSetField( hTIFFTmp, TIFFTAG_JPEGCOLORMODE,
                            JPEGCOLORMODE_RGB);
        }
        if( nJpegTablesModeIn >= 0 )
            TIFFSetField( hTIFFTmp, TIFFTAG_JPEGTABLESMODE,
                            nJpegTablesModeIn);

        GPtrDiff_t nBlockSize = static_cast<GPtrDiff_t>(nInMemImageWidth) * nInMemImageHeight *
                                        ((nBands <= 4) ? nBands : 1);
        if( l_nBitsPerSample == 12 )
            nBlockSize = (nBlockSize * 3) / 2;
        std::vector<GByte> abyZeroData( nBlockSize, 0 );
        TIFFWriteEncodedStrip( hTIFFTmp, 0, &abyZeroData[0], nBlockSize);

        uint32_t nJPEGTableSize = 0;
        void* pJPEGTable = nullptr;
        if( TIFFGetField( hTIFFTmp, TIFFTAG_JPEGTABLES, &nJPEGTableSize,
                            &pJPEGTable) )
            TIFFSetField( hTIFF, TIFFTAG_JPEGTABLES, nJPEGTableSize,
                            pJPEGTable);

        float *ref = nullptr;
        if( TIFFGetField(hTIFFTmp, TIFFTAG_REFERENCEBLACKWHITE, &ref) )
            TIFFSetField(hTIFF, TIFFTAG_REFERENCEBLACKWHITE, ref);

        XTIFFClose(hTIFFTmp);
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTmp));
    }
    VSIUnlink(osTmpFilenameIn);
}

/************************************************************************/
/*                            GuessJPEGQuality()                        */
/*                                                                      */
/*      Guess JPEG quality from JPEGTABLES tag.                         */
/************************************************************************/

static const GByte* GTIFFFindNextTable( const GByte* paby, GByte byMarker,
                                        int nLen, int* pnLenTable )
{
    for( int i = 0; i + 1 < nLen; )
    {
        if( paby[i] != 0xFF )
            return nullptr;
        ++i;
        if( paby[i] == 0xD8 )
        {
            ++i;
            continue;
        }
        if( i + 2 >= nLen )
            return nullptr;
        int nMarkerLen = paby[i+1] * 256 + paby[i+2];
        if( i+1+nMarkerLen >= nLen )
            return nullptr;
        if( paby[i] == byMarker )
        {
            if( pnLenTable ) *pnLenTable = nMarkerLen;
            return paby + i + 1;
        }
        i += 1 + nMarkerLen;
    }
    return nullptr;
}

constexpr GByte MARKER_HUFFMAN_TABLE = 0xC4;
constexpr GByte MARKER_QUANT_TABLE = 0xDB;

// We assume that if there are several quantization tables, they are
// in the same order. Which is a reasonable assumption for updating
// a file generated by ourselves.
static bool GTIFFQuantizationTablesEqual( const GByte* paby1, int nLen1,
                                          const GByte* paby2, int nLen2 )
{
    bool bFound = false;
    while( true )
    {
        int nLenTable1 = 0;
        int nLenTable2 = 0;
        const GByte* paby1New =
            GTIFFFindNextTable(paby1, MARKER_QUANT_TABLE, nLen1, &nLenTable1);
        const GByte* paby2New =
            GTIFFFindNextTable(paby2, MARKER_QUANT_TABLE, nLen2, &nLenTable2);
        if( paby1New == nullptr && paby2New == nullptr )
            return bFound;
        if( paby1New == nullptr || paby2New == nullptr )
            return false;
        if( nLenTable1 != nLenTable2 )
            return false;
        if( memcmp(paby1New, paby2New, nLenTable1) != 0 )
            return false;
        paby1New += nLenTable1;
        paby2New += nLenTable2;
        nLen1 -= static_cast<int>(paby1New - paby1);
        nLen2 -= static_cast<int>(paby2New - paby2);
        paby1 = paby1New;
        paby2 = paby2New;
        bFound = true;
    }
}

// Guess the JPEG quality by comparing against the MD5Sum of precomputed
// quantization tables
static int GuessJPEGQualityFromMD5( const uint8_t md5JPEGQuantTable[][16],
                                    const GByte* const pabyJPEGTable,
                                    int nJPEGTableSize )
{
    int nRemainingLen = nJPEGTableSize;
    const GByte* pabyCur = pabyJPEGTable;

    struct CPLMD5Context context;
    CPLMD5Init( &context );

    while( true )
    {
        int nLenTable = 0;
        const GByte* pabyNew =
            GTIFFFindNextTable(pabyCur, MARKER_QUANT_TABLE, nRemainingLen, &nLenTable);
        if( pabyNew == nullptr )
            break;
        CPLMD5Update( &context, pabyNew, nLenTable);
        pabyNew += nLenTable;
        nRemainingLen -= static_cast<int>(pabyNew - pabyCur);
        pabyCur = pabyNew;
    }

    GByte digest[16];
    CPLMD5Final(digest, &context);

    for( int i=0;i<100;i++)
    {
        if( memcmp(md5JPEGQuantTable[i], digest, 16) == 0 )
        {
            return i+1;
        }
    }
    return -1;
}

int GTiffDataset::GuessJPEGQuality( bool& bOutHasQuantizationTable,
                                    bool& bOutHasHuffmanTable )
{
    CPLAssert( m_nCompression == COMPRESSION_JPEG );
    uint32_t nJPEGTableSize = 0;
    void* pJPEGTable = nullptr;
    if( !TIFFGetField(m_hTIFF, TIFFTAG_JPEGTABLES, &nJPEGTableSize, &pJPEGTable) )
    {
        bOutHasQuantizationTable = false;
        bOutHasHuffmanTable = false;
        return -1;
    }

    bOutHasQuantizationTable =
        GTIFFFindNextTable( static_cast<const GByte*>(pJPEGTable), MARKER_QUANT_TABLE,
                            nJPEGTableSize, nullptr) != nullptr;
    bOutHasHuffmanTable =
        GTIFFFindNextTable( static_cast<const GByte*>(pJPEGTable), MARKER_HUFFMAN_TABLE,
                            nJPEGTableSize, nullptr) != nullptr;
    if( !bOutHasQuantizationTable )
        return -1;

    if( (nBands == 1 && m_nBitsPerSample == 8) ||
        (nBands == 3 && m_nBitsPerSample == 8 && m_nPhotometric == PHOTOMETRIC_RGB) ||
        (nBands == 4 && m_nBitsPerSample == 8 && m_nPhotometric == PHOTOMETRIC_SEPARATED) )
    {
        return GuessJPEGQualityFromMD5(md5JPEGQuantTable_generic_8bit,
                                       static_cast<const GByte*>(pJPEGTable),
                                       static_cast<int>(nJPEGTableSize));
    }

    if( nBands == 3 && m_nBitsPerSample == 8 && m_nPhotometric == PHOTOMETRIC_YCBCR )
    {
        return GuessJPEGQualityFromMD5(md5JPEGQuantTable_3_YCBCR_8bit,
                                       static_cast<const GByte*>(pJPEGTable),
                                       static_cast<int>(nJPEGTableSize));
    }

    char** papszLocalParameters = nullptr;
    papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                            "COMPRESS", "JPEG");
    if( m_nPhotometric == PHOTOMETRIC_YCBCR )
        papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                            "PHOTOMETRIC", "YCBCR");
    else if( m_nPhotometric == PHOTOMETRIC_SEPARATED )
        papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                            "PHOTOMETRIC", "CMYK");
    papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                            "BLOCKYSIZE", "16");
    if( m_nBitsPerSample == 12 )
        papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                                "NBITS", "12");

    CPLString osTmpFilenameIn;
    osTmpFilenameIn.Printf( "/vsimem/gtiffdataset_guess_jpeg_quality_tmp_%p",
                            this );

    int nRet = -1;
    for( int nQuality = 0; nQuality <= 100 && nRet < 0; ++nQuality )
    {
        VSILFILE* fpTmp = nullptr;
        if( nQuality == 0 )
            papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                                   "JPEG_QUALITY", "75");
        else
            papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                "JPEG_QUALITY", CPLSPrintf("%d", nQuality));

        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLString osTmp;
        TIFF* hTIFFTmp =
            CreateLL( osTmpFilenameIn, 16, 16, (nBands <= 4) ? nBands : 1,
                      GetRasterBand(1)->GetRasterDataType(), 0.0,
                      papszLocalParameters, &fpTmp, osTmp );
        CPLPopErrorHandler();
        if( !hTIFFTmp )
        {
            break;
        }

        TIFFWriteCheck( hTIFFTmp, FALSE, "CreateLL" );
        TIFFWriteDirectory( hTIFFTmp );
        TIFFSetDirectory( hTIFFTmp, 0 );
        // Now reset jpegcolormode.
        if( m_nPhotometric == PHOTOMETRIC_YCBCR
            && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                                "YES") ) )
        {
            TIFFSetField(hTIFFTmp, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }

        GByte abyZeroData[(16*16*4*3)/2] = {};
        const int nBlockSize =
            (16 * 16 * ((nBands <= 4) ? nBands : 1) * m_nBitsPerSample) / 8;
        TIFFWriteEncodedStrip( hTIFFTmp, 0, abyZeroData, nBlockSize);

        uint32_t nJPEGTableSizeTry = 0;
        void* pJPEGTableTry = nullptr;
        if( TIFFGetField(hTIFFTmp, TIFFTAG_JPEGTABLES,
                         &nJPEGTableSizeTry, &pJPEGTableTry) )
        {
            if( GTIFFQuantizationTablesEqual(
                   static_cast<GByte *>(pJPEGTable), nJPEGTableSize,
                   static_cast<GByte *>(pJPEGTableTry), nJPEGTableSizeTry) )
            {
                nRet = (nQuality == 0 ) ? 75 : nQuality;
            }
        }

        XTIFFClose(hTIFFTmp);
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTmp));
    }

    CSLDestroy(papszLocalParameters);
    VSIUnlink(osTmpFilenameIn);

    return nRet;
}

/************************************************************************/
/*               SetJPEGQualityAndTablesModeFromFile()                  */
/************************************************************************/

void GTiffDataset::SetJPEGQualityAndTablesModeFromFile(int nQuality,
                                                       bool bHasQuantizationTable,
                                                       bool bHasHuffmanTable)
{
    if( nQuality > 0 )
    {
        CPLDebug("GTiff", "Guessed JPEG quality to be %d", nQuality);
        m_nJpegQuality = static_cast<signed char>(nQuality);
        TIFFSetField( m_hTIFF, TIFFTAG_JPEGQUALITY, nQuality );

        // This means we will use the quantization tables from the
        // JpegTables tag.
        m_nJpegTablesMode = JPEGTABLESMODE_QUANT;
    }
    else
    {
        uint32_t nJPEGTableSize = 0;
        void* pJPEGTable = nullptr;
        if( !TIFFGetField( m_hTIFF, TIFFTAG_JPEGTABLES,
                            &nJPEGTableSize, &pJPEGTable) )
        {
            toff_t *panByteCounts = nullptr;
            const int nBlockCount =
                m_nPlanarConfig == PLANARCONFIG_SEPARATE
                ? m_nBlocksPerBand * nBands
                : m_nBlocksPerBand;
            if( TIFFIsTiled( m_hTIFF ) )
                TIFFGetField( m_hTIFF, TIFFTAG_TILEBYTECOUNTS,
                                &panByteCounts );
            else
                TIFFGetField( m_hTIFF, TIFFTAG_STRIPBYTECOUNTS,
                                &panByteCounts );

            bool bFoundNonEmptyBlock = false;
            if( panByteCounts != nullptr )
            {
                for( int iBlock = 0; iBlock < nBlockCount; ++iBlock )
                {
                    if( panByteCounts[iBlock] != 0 )
                    {
                        bFoundNonEmptyBlock = true;
                        break;
                    }
                }
            }
            if( bFoundNonEmptyBlock )
            {
                CPLDebug("GTiff", "Could not guess JPEG quality. "
                            "JPEG tables are missing, so going in "
                            "TIFFTAG_JPEGTABLESMODE = 0/2 mode");
                // Write quantization tables in each strile.
                m_nJpegTablesMode = 0;
            }
        }
        else
        {
            if( bHasQuantizationTable )
            {
                // FIXME in libtiff: this is likely going to cause issues
                // since libtiff will reuse in each strile the number of
                // the global quantization table, which is invalid.
                CPLDebug(
                    "GTiff", "Could not guess JPEG quality although JPEG "
                    "quantization tables are present, so going in "
                    "TIFFTAG_JPEGTABLESMODE = 0/2 mode" );
            }
            else
            {
                CPLDebug("GTiff", "Could not guess JPEG quality since JPEG "
                        "quantization tables are not present, so going in "
                        "TIFFTAG_JPEGTABLESMODE = 0/2 mode");
            }

            // Write quantization tables in each strile.
            m_nJpegTablesMode = 0;
        }
    }
    if( bHasHuffmanTable )
    {
        // If there are Huffman tables in header use them, otherwise
        // if we use optimized tables, libtiff will currently reuse
        // the number of the Huffman tables of the header for the
        // optimized version of each strile, which is illegal.
        m_nJpegTablesMode |= JPEGTABLESMODE_HUFF;
    }
    if( m_nJpegTablesMode >= 0 )
        TIFFSetField( m_hTIFF, TIFFTAG_JPEGTABLESMODE,
                        m_nJpegTablesMode);
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new GeoTIFF or TIFF file.                              */
/************************************************************************/

GDALDataset *GTiffDataset::Create( const char * pszFilename,
                                   int nXSize, int nYSize, int l_nBands,
                                   GDALDataType eType,
                                   char **papszParamList )

{
    VSILFILE* l_fpL = nullptr;
    CPLString l_osTmpFilename;

/* -------------------------------------------------------------------- */
/*      Create the underlying TIFF file.                                */
/* -------------------------------------------------------------------- */
    TIFF *l_hTIFF = CreateLL(
        pszFilename,
        nXSize, nYSize, l_nBands,
        eType, 0, papszParamList, &l_fpL, l_osTmpFilename );
    const bool bStreaming = !l_osTmpFilename.empty();

    if( l_hTIFF == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->m_hTIFF = l_hTIFF;
    poDS->m_fpL = l_fpL;
    if( bStreaming )
    {
        poDS->m_bStreamingOut = true;
        poDS->m_pszTmpFilename = CPLStrdup(l_osTmpFilename);
        poDS->m_fpToWrite = VSIFOpenL( pszFilename, "wb" );
        if( poDS->m_fpToWrite == nullptr )
        {
            VSIUnlink(l_osTmpFilename);
            delete poDS;
            return nullptr;
        }
    }
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->m_bCrystalized = false;
    poDS->m_nSamplesPerPixel = static_cast<uint16_t>(l_nBands);
    poDS->m_pszFilename = CPLStrdup(pszFilename);

    // Don't try to load external metadata files (#6597).
    poDS->m_bIMDRPCMetadataLoaded = true;

    // Avoid premature crystalization that will cause directory re-writing if
    // GetProjectionRef() or GetGeoTransform() are called on the newly created
    // GeoTIFF.
    poDS->m_bLookedForProjection = true;

    TIFFGetField( l_hTIFF, TIFFTAG_SAMPLEFORMAT, &(poDS->m_nSampleFormat) );
    TIFFGetField( l_hTIFF, TIFFTAG_PLANARCONFIG, &(poDS->m_nPlanarConfig) );
    // Weird that we need this, but otherwise we get a Valgrind warning on
    // tiff_write_124.
    if( !TIFFGetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, &(poDS->m_nPhotometric) ) )
        poDS->m_nPhotometric = PHOTOMETRIC_MINISBLACK;
    TIFFGetField( l_hTIFF, TIFFTAG_BITSPERSAMPLE, &(poDS->m_nBitsPerSample) );
    TIFFGetField( l_hTIFF, TIFFTAG_COMPRESSION, &(poDS->m_nCompression) );

    if( TIFFIsTiled(l_hTIFF) )
    {
        TIFFGetField( l_hTIFF, TIFFTAG_TILEWIDTH, &(poDS->m_nBlockXSize) );
        TIFFGetField( l_hTIFF, TIFFTAG_TILELENGTH, &(poDS->m_nBlockYSize) );
    }
    else
    {
        if( !TIFFGetField( l_hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(poDS->m_nRowsPerStrip) ) )
            poDS->m_nRowsPerStrip = 1;  // Dummy value.

        poDS->m_nBlockXSize = nXSize;
        poDS->m_nBlockYSize =
            std::min( static_cast<int>(poDS->m_nRowsPerStrip) , nYSize );
    }

    poDS->m_nBlocksPerBand =
        DIV_ROUND_UP(nYSize, poDS->m_nBlockYSize)
        * DIV_ROUND_UP(nXSize, poDS->m_nBlockXSize);

    poDS->m_eProfile = GetProfile(
        CSLFetchNameValue( papszParamList, "PROFILE" ) );

/* -------------------------------------------------------------------- */
/*      YCbCr JPEG compressed images should be translated on the fly    */
/*      to RGB by libtiff/libjpeg unless specifically requested         */
/*      otherwise.                                                      */
/* -------------------------------------------------------------------- */
    if( poDS->m_nCompression == COMPRESSION_JPEG
        && poDS->m_nPhotometric == PHOTOMETRIC_YCBCR
        && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES") ) )
    {
        int nColorMode = 0;

        poDS->SetMetadataItem("SOURCE_COLOR_SPACE", "YCbCr", "IMAGE_STRUCTURE");
        if( !TIFFGetField( l_hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode ) ||
            nColorMode != JPEGCOLORMODE_RGB )
            TIFFSetField(l_hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
    }

    if( poDS->m_nCompression == COMPRESSION_LERC )
    {
        uint32_t nLercParamCount = 0;
        uint32_t* panLercParams = nullptr;
        if( TIFFGetField( l_hTIFF, TIFFTAG_LERC_PARAMETERS, &nLercParamCount,
                          &panLercParams ) &&
            nLercParamCount == 2 )
        {
            memcpy( poDS->m_anLercAddCompressionAndVersion, panLercParams,
                    sizeof(poDS->m_anLercAddCompressionAndVersion) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Read palette back as a color table if it has one.               */
/* -------------------------------------------------------------------- */
    unsigned short *panRed = nullptr;
    unsigned short *panGreen = nullptr;
    unsigned short *panBlue = nullptr;

    if( poDS->m_nPhotometric == PHOTOMETRIC_PALETTE
        && TIFFGetField( l_hTIFF, TIFFTAG_COLORMAP,
                         &panRed, &panGreen, &panBlue) )
    {

        poDS->m_poColorTable = new GDALColorTable();

        const int nColorCount = 1 << poDS->m_nBitsPerSample;

        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            const unsigned short divisor = 257;
            const GDALColorEntry oEntry = {
                static_cast<short>(panRed[iColor] / divisor),
                static_cast<short>(panGreen[iColor] / divisor),
                static_cast<short>(panBlue[iColor] / divisor),
                static_cast<short>(255)
            };

            poDS->m_poColorTable->SetColorEntry( iColor, &oEntry );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we want to ensure all blocks get written out on close to     */
/*      avoid sparse files?                                             */
/* -------------------------------------------------------------------- */
    if( !CPLFetchBool( papszParamList, "SPARSE_OK", false ) )
        poDS->m_bFillEmptyTilesAtClosing = true;

    poDS->m_bWriteEmptyTiles = bStreaming ||
        (poDS->m_nCompression != COMPRESSION_NONE &&
         poDS->m_bFillEmptyTilesAtClosing);
    // Only required for people writing non-compressed striped files in the
    // right order and wanting all tstrips to be written in the same order
    // so that the end result can be memory mapped without knowledge of each
    // strip offset.
    if( CPLTestBool( CSLFetchNameValueDef( papszParamList,
                              "WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE" )) ||
         CPLTestBool( CSLFetchNameValueDef( papszParamList,
                              "@WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE" )) )
    {
        poDS->m_bWriteEmptyTiles = true;
    }

/* -------------------------------------------------------------------- */
/*      Preserve creation options for consulting later (for instance    */
/*      to decide if a TFW file should be written).                     */
/* -------------------------------------------------------------------- */
    poDS->m_papszCreationOptions = CSLDuplicate( papszParamList );

    poDS->m_nZLevel = GTiffGetZLevel(papszParamList);
    poDS->m_nLZMAPreset = GTiffGetLZMAPreset(papszParamList);
    poDS->m_nZSTDLevel = GTiffGetZSTDPreset(papszParamList);
    poDS->m_nWebPLevel = GTiffGetWebPLevel(papszParamList);
    poDS->m_bWebPLossless = GTiffGetWebPLossless(papszParamList);
    poDS->m_nJpegQuality = GTiffGetJpegQuality(papszParamList);
    poDS->m_nJpegTablesMode = GTiffGetJpegTablesMode(papszParamList);
    poDS->m_dfMaxZError = GTiffGetLERCMaxZError(papszParamList);
    poDS->InitCreationOrOpenOptions(papszParamList);

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < l_nBands; ++iBand )
    {
        if( poDS->m_nBitsPerSample == 8 ||
            (poDS->m_nBitsPerSample == 16 && eType != GDT_Float32) ||
            poDS->m_nBitsPerSample == 32 ||
            poDS->m_nBitsPerSample == 64 ||
            poDS->m_nBitsPerSample == 128)
        {
            poDS->SetBand( iBand + 1, new GTiffRasterBand( poDS, iBand + 1 ) );
        }
        else
        {
            poDS->SetBand( iBand + 1, new GTiffOddBitsBand( poDS, iBand + 1 ) );
            poDS->GetRasterBand( iBand + 1 )->
                SetMetadataItem( "NBITS",
                                 CPLString().Printf("%d",poDS->m_nBitsPerSample),
                                 "IMAGE_STRUCTURE" );
        }
    }

    poDS->GetDiscardLsbOption(papszParamList);

    if( poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG && l_nBands != 1 )
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    else
        poDS->SetMetadataItem( "INTERLEAVE", "BAND", "IMAGE_STRUCTURE" );

    poDS->oOvManager.Initialize( poDS, pszFilename );

    return poDS;
}

/************************************************************************/
/*                           CopyImageryAndMask()                       */
/************************************************************************/

CPLErr GTiffDataset::CopyImageryAndMask(GTiffDataset* poDstDS,
                                        GDALDataset* poSrcDS,
                                        GDALRasterBand* poSrcMaskBand,
                                        GDALProgressFunc pfnProgress,
                                        void * pProgressData )
{
    CPLErr eErr = CE_None;

    const auto eType = poDstDS->GetRasterBand(1)->GetRasterDataType();
    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eType);
    const int l_nBands = poDstDS->GetRasterCount();
    void *pBlockBuffer = VSI_MALLOC3_VERBOSE(
        poDstDS->m_nBlockXSize, poDstDS->m_nBlockYSize, l_nBands * nDataTypeSize);
    if( pBlockBuffer == nullptr )
    {
        eErr = CE_Failure;
    }
    const int nYSize = poDstDS->nRasterYSize;
    const int nXSize = poDstDS->nRasterXSize;
    const int nYBlocks = DIV_ROUND_UP(nYSize, poDstDS->m_nBlockYSize);
    const int nXBlocks = DIV_ROUND_UP(nXSize, poDstDS->m_nBlockXSize);
    const int nBlocks = nXBlocks * nYBlocks;

    CPLAssert(l_nBands == 1 || poDstDS->m_nPlanarConfig == PLANARCONFIG_CONTIG );

    const bool bIsOddBand =
        dynamic_cast<GTiffOddBitsBand*>(poDstDS->GetRasterBand(1)) != nullptr;

    if( poDstDS->m_poMaskDS )
    {
        CPLAssert( poDstDS->m_poMaskDS->m_nBlockXSize == poDstDS->m_nBlockXSize );
        CPLAssert( poDstDS->m_poMaskDS->m_nBlockYSize == poDstDS->m_nBlockYSize );
    }

    int iBlock = 0;
    for( int iY = 0, nYBlock = 0; iY < nYSize && eErr == CE_None;
            iY = ((nYSize - iY < poDstDS->m_nBlockYSize) ? nYSize :
                iY + poDstDS->m_nBlockYSize),
            nYBlock++ )
    {
        const int nReqYSize = std::min(nYSize - iY, poDstDS->m_nBlockYSize);
        for( int iX = 0, nXBlock = 0; iX < nXSize && eErr == CE_None;
                iX = ((nXSize - iX < poDstDS->m_nBlockXSize) ? nXSize :
                    iX + poDstDS->m_nBlockXSize),
                nXBlock++ )
        {
            const int nReqXSize = std::min(nXSize - iX, poDstDS->m_nBlockXSize);
            if( nReqXSize < poDstDS->m_nBlockXSize ||
                nReqYSize < poDstDS->m_nBlockYSize )
            {
                memset(pBlockBuffer, 0, static_cast<size_t>(
                    poDstDS->m_nBlockXSize) * poDstDS->m_nBlockYSize *
                    l_nBands * nDataTypeSize);
            }

            if( !bIsOddBand )
            {
                eErr = poSrcDS->RasterIO( GF_Read,
                    iX, iY, nReqXSize, nReqYSize,
                    pBlockBuffer, nReqXSize, nReqYSize,
                    eType,
                    l_nBands, nullptr,
                    nDataTypeSize * l_nBands,
                    poDstDS->m_nBlockXSize * nDataTypeSize * l_nBands,
                    nDataTypeSize,
                    nullptr );
                if( eErr == CE_None )
                {
                    eErr = poDstDS->WriteEncodedTileOrStrip(
                        iBlock, pBlockBuffer, false);
                }
            }
            else
            {
                // In the odd bit case, this is a bit messy to ensure
                // the strile gets written synchronously.
                // We load the content of the n-1 bands in the cache,
                // and for the last band we invoke WriteBlock() directly
                // We also force FlushBlockBuf()
                std::vector<GDALRasterBlock*> apoLockedBlocks;
                for( int i = 0; eErr == CE_None && i < l_nBands - 1; i++ )
                {
                    auto poBlock = poDstDS->GetRasterBand(i+1)->GetLockedBlockRef(
                        nXBlock, nYBlock, TRUE);
                    if( poBlock )
                    {
                        eErr = poSrcDS->GetRasterBand(i+1)->RasterIO(
                            GF_Read,
                            iX, iY, nReqXSize, nReqYSize,
                            poBlock->GetDataRef(), nReqXSize, nReqYSize,
                            eType,
                            nDataTypeSize,
                            nDataTypeSize * poDstDS->m_nBlockXSize, nullptr);
                        poBlock->MarkDirty();
                        apoLockedBlocks.emplace_back(poBlock);
                    }
                    else
                    {
                        eErr = CE_Failure;
                    }
                }
                if( eErr == CE_None )
                {
                    eErr = poSrcDS->GetRasterBand(l_nBands)->RasterIO(
                            GF_Read,
                            iX, iY, nReqXSize, nReqYSize,
                            pBlockBuffer, nReqXSize, nReqYSize,
                            eType,
                            nDataTypeSize,
                            nDataTypeSize * poDstDS->m_nBlockXSize, nullptr);
                }
                if( eErr == CE_None )
                {
                    // Avoid any attempt to load from disk
                    poDstDS->m_nLoadedBlock = iBlock;
                    eErr = poDstDS->GetRasterBand(l_nBands)->WriteBlock(
                        nXBlock, nYBlock, pBlockBuffer);
                    if( eErr == CE_None )
                        eErr = poDstDS->FlushBlockBuf();
                }
                for( auto poBlock: apoLockedBlocks )
                {
                    poBlock->MarkClean();
                    poBlock->DropLock();
                }
            }

            if( eErr == CE_None && poDstDS->m_poMaskDS )
            {
                if( nReqXSize < poDstDS->m_nBlockXSize ||
                    nReqYSize < poDstDS->m_nBlockYSize )
                {
                    memset(pBlockBuffer, 0,
                            static_cast<size_t>(poDstDS->m_nBlockXSize) *
                            poDstDS->m_nBlockYSize);
                }
                eErr = poSrcMaskBand->RasterIO(
                    GF_Read,
                    iX, iY, nReqXSize, nReqYSize,
                    pBlockBuffer, nReqXSize, nReqYSize,
                    GDT_Byte,
                    1, poDstDS->m_nBlockXSize, nullptr);
                if( eErr == CE_None )
                {
                    // Avoid any attempt to load from disk
                    poDstDS->m_poMaskDS->m_nLoadedBlock = iBlock;
                    eErr = poDstDS->m_poMaskDS->GetRasterBand(1)->
                        WriteBlock(nXBlock, nYBlock, pBlockBuffer);
                    if( eErr == CE_None )
                        eErr = poDstDS->m_poMaskDS->FlushBlockBuf();
                }
            }
            if( poDstDS->m_bWriteError )
                eErr = CE_Failure;

            iBlock ++;
            if( pfnProgress && !pfnProgress(
                static_cast<double>(iBlock) / nBlocks, nullptr, pProgressData) )
            {
                eErr = CE_Failure;
            }
        }
    }
    poDstDS->FlushCache(); // mostly to wait for thread completion
    VSIFree(pBlockBuffer);

    return eErr;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
GTiffDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                          int bStrict, char ** papszOptions,
                          GDALProgressFunc pfnProgress, void * pProgressData )

{
    if( poSrcDS->GetRasterCount() == 0 )
    {
        ReportError( pszFilename, CE_Failure, CPLE_AppDefined,
                  "Unable to export GeoTIFF files with zero bands." );
        return nullptr;
    }

    GDALRasterBand * const poPBand = poSrcDS->GetRasterBand(1);
    const GDALDataType eType = poPBand->GetRasterDataType();

/* -------------------------------------------------------------------- */
/*      Check, whether all bands in input dataset has the same type.    */
/* -------------------------------------------------------------------- */
    const int l_nBands = poSrcDS->GetRasterCount();
    for( int iBand = 2; iBand <= l_nBands; ++iBand )
    {
        if( eType != poSrcDS->GetRasterBand(iBand)->GetRasterDataType() )
        {
            if( bStrict )
            {
                ReportError( pszFilename, CE_Failure, CPLE_AppDefined,
                    "Unable to export GeoTIFF file with different datatypes "
                    "per different bands. All bands should have the same "
                    "types in TIFF." );
                return nullptr;
            }
            else
            {
                ReportError( pszFilename,CE_Warning, CPLE_AppDefined,
                    "Unable to export GeoTIFF file with different datatypes "
                    "per different bands. All bands should have the same "
                    "types in TIFF." );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Capture the profile.                                            */
/* -------------------------------------------------------------------- */
    const GTiffProfile eProfile = GetProfile(
        CSLFetchNameValue(papszOptions, "PROFILE"));

    const bool bGeoTIFF = eProfile != GTiffProfile::BASELINE;

/* -------------------------------------------------------------------- */
/*      Special handling for NBITS.  Copy from band metadata if found.  */
/* -------------------------------------------------------------------- */
    char **papszCreateOptions = CSLDuplicate( papszOptions );

    if( poPBand->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" ) != nullptr
        && atoi(poPBand->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" )) > 0
        && CSLFetchNameValue( papszCreateOptions, "NBITS") == nullptr )
    {
        papszCreateOptions =
            CSLSetNameValue( papszCreateOptions, "NBITS",
                             poPBand->GetMetadataItem( "NBITS",
                                                       "IMAGE_STRUCTURE" ) );
    }

    if( CSLFetchNameValue( papszOptions, "PIXELTYPE" ) == nullptr
        && eType == GDT_Byte
        && poPBand->GetMetadataItem( "PIXELTYPE", "IMAGE_STRUCTURE" ) )
    {
        papszCreateOptions =
            CSLSetNameValue( papszCreateOptions, "PIXELTYPE",
                             poPBand->GetMetadataItem(
                                 "PIXELTYPE", "IMAGE_STRUCTURE" ) );
    }

/* -------------------------------------------------------------------- */
/*      Color profile.  Copy from band metadata if found.              */
/* -------------------------------------------------------------------- */
    if( bGeoTIFF )
    {
        const char* pszOptionsMD[] = {
            "SOURCE_ICC_PROFILE",
            "SOURCE_PRIMARIES_RED",
            "SOURCE_PRIMARIES_GREEN",
            "SOURCE_PRIMARIES_BLUE",
            "SOURCE_WHITEPOINT",
            "TIFFTAG_TRANSFERFUNCTION_RED",
            "TIFFTAG_TRANSFERFUNCTION_GREEN",
            "TIFFTAG_TRANSFERFUNCTION_BLUE",
            "TIFFTAG_TRANSFERRANGE_BLACK",
            "TIFFTAG_TRANSFERRANGE_WHITE",
            nullptr
        };

        // Copy all the tags.  Options will override tags in the source.
        int i = 0;
        while(pszOptionsMD[i] != nullptr)
        {
            char const *pszMD =
                CSLFetchNameValue(papszOptions, pszOptionsMD[i]);
            if( pszMD == nullptr )
                pszMD = poSrcDS->GetMetadataItem( pszOptionsMD[i],
                                                  "COLOR_PROFILE" );

            if( (pszMD != nullptr) && !EQUAL(pszMD, "") )
            {
                papszCreateOptions =
                    CSLSetNameValue( papszCreateOptions, pszOptionsMD[i],
                                     pszMD );

                // If an ICC profile exists, other tags are not needed.
                if( EQUAL(pszOptionsMD[i], "SOURCE_ICC_PROFILE") )
                    break;
            }

            ++i;
        }
    }

    double dfExtraSpaceForOverviews = 0;
    const bool bCopySrcOverviews = CPLFetchBool(papszCreateOptions, "COPY_SRC_OVERVIEWS", false);
    std::unique_ptr<GDALDataset> poOvrDS;
    int nSrcOverviews = 0;
    if( bCopySrcOverviews )
    {
        const char* pszOvrDS = CSLFetchNameValue(papszCreateOptions, "@OVERVIEW_DATASET");
        if( pszOvrDS )
        {
            // Empty string is used by COG driver to indicate that we want
            // to ignore source overviews.
            if( !EQUAL(pszOvrDS, "") )
            {
                poOvrDS.reset(GDALDataset::Open(pszOvrDS));
                if( !poOvrDS )
                {
                    CSLDestroy(papszCreateOptions);
                    return nullptr;
                }
                if( poOvrDS->GetRasterCount() != l_nBands )
                {
                    CSLDestroy(papszCreateOptions);
                    return nullptr;
                }
                nSrcOverviews = poOvrDS->GetRasterBand(1)->GetOverviewCount() + 1;
            }
        }
        else
        {
            nSrcOverviews = poSrcDS->GetRasterBand(1)->GetOverviewCount();
        }
        if( nSrcOverviews )
        {
            for( int j = 1; j <= l_nBands; ++j )
            {
                const int nOtherBandOverviewCount = poOvrDS ?
                    poOvrDS->GetRasterBand(j)->GetOverviewCount() + 1:
                    poSrcDS->GetRasterBand(j)->GetOverviewCount();
                if( nOtherBandOverviewCount != nSrcOverviews )
                {
                    ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
                        "COPY_SRC_OVERVIEWS cannot be used when the bands have "
                        "not the same number of overview levels." );
                    CSLDestroy(papszCreateOptions);
                    return nullptr;
                }
                for( int i = 0; i < nSrcOverviews; ++i )
                {
                    GDALRasterBand* poOvrBand = poOvrDS ?
                        (i == 0 ? poOvrDS->GetRasterBand(j) :
                                  poOvrDS->GetRasterBand(j)->GetOverview(i-1)) :
                        poSrcDS->GetRasterBand(j)->GetOverview(i);
                    if( poOvrBand == nullptr )
                    {
                        ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
                            "COPY_SRC_OVERVIEWS cannot be used when one "
                            "overview band is NULL." );
                        CSLDestroy(papszCreateOptions);
                        return nullptr;
                    }
                    GDALRasterBand* poOvrFirstBand = poOvrDS ?
                        (i == 0 ? poOvrDS->GetRasterBand(1) :
                                  poOvrDS->GetRasterBand(1)->GetOverview(i-1)) :
                        poSrcDS->GetRasterBand(1)->GetOverview(i);
                    if( poOvrBand->GetXSize() != poOvrFirstBand->GetXSize() ||
                        poOvrBand->GetYSize() != poOvrFirstBand->GetYSize() )
                    {
                        ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
                            "COPY_SRC_OVERVIEWS cannot be used when the "
                            "overview bands have not the same dimensions "
                            "among bands." );
                        CSLDestroy(papszCreateOptions);
                        return nullptr;
                    }
                }
            }

            for( int i = 0; i < nSrcOverviews; ++i )
            {
                GDALRasterBand* poOvrFirstBand = poOvrDS ?
                        (i == 0 ? poOvrDS->GetRasterBand(1) :
                                  poOvrDS->GetRasterBand(1)->GetOverview(i-1)) :
                        poSrcDS->GetRasterBand(1)->GetOverview(i);
                dfExtraSpaceForOverviews +=
                    static_cast<double>(
                      poOvrFirstBand->GetXSize()) *
                      poOvrFirstBand->GetYSize();
            }
            dfExtraSpaceForOverviews *=
                                l_nBands * GDALGetDataTypeSizeBytes(eType);
        }
        else
        {
            CPLDebug("GTiff", "No source overviews to copy");
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we use optimized way of copying from an input JPEG       */
/*      dataset?                                                        */
/* -------------------------------------------------------------------- */

// TODO(schwehr): Refactor bDirectCopyFromJPEG to be a const.
#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
    bool bDirectCopyFromJPEG = false;
#endif

    // Note: JPEG_DIRECT_COPY is not defined by default, because it is mainly
    // useful for debugging purposes.
#ifdef JPEG_DIRECT_COPY
    if( CPLFetchBool(papszCreateOptions, "JPEG_DIRECT_COPY", false) &&
        GTIFF_CanDirectCopyFromJPEG(poSrcDS, papszCreateOptions) )
    {
        CPLDebug("GTiff", "Using special direct copy mode from a JPEG dataset");

        bDirectCopyFromJPEG = true;
    }
#endif

#ifdef HAVE_LIBJPEG
    bool bCopyFromJPEG = false;

    // When CreateCopy'ing() from a JPEG dataset, and asking for COMPRESS=JPEG,
    // use DCT coefficients (unless other options are incompatible, like
    // strip/tile dimensions, specifying JPEG_QUALITY option, incompatible
    // PHOTOMETRIC with the source colorspace, etc.) to avoid the lossy steps
    // involved by decompression/recompression.
    if( !bDirectCopyFromJPEG &&
        GTIFF_CanCopyFromJPEG(poSrcDS, papszCreateOptions) )
    {
        CPLDebug( "GTiff", "Using special copy mode from a JPEG dataset" );

        bCopyFromJPEG = true;
    }
#endif

/* -------------------------------------------------------------------- */
/*      If the source is RGB, then set the PHOTOMETRIC=RGB value        */
/* -------------------------------------------------------------------- */

    const bool bForcePhotometric =
        CSLFetchNameValue(papszOptions, "PHOTOMETRIC") != nullptr;

    if( l_nBands >= 3 && !bForcePhotometric &&
#ifdef HAVE_LIBJPEG
        !bCopyFromJPEG &&
#endif
        poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_RedBand &&
        poSrcDS->GetRasterBand(2)->GetColorInterpretation() == GCI_GreenBand &&
        poSrcDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand )
    {
        papszCreateOptions =
            CSLSetNameValue( papszCreateOptions, "PHOTOMETRIC", "RGB" );
    }

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    VSILFILE* l_fpL = nullptr;
    CPLString l_osTmpFilename;

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    TIFF *l_hTIFF =
        CreateLL( pszFilename, nXSize, nYSize, l_nBands,
                  eType, dfExtraSpaceForOverviews, papszCreateOptions, &l_fpL,
                  l_osTmpFilename );
    const bool bStreaming = !l_osTmpFilename.empty();

    CSLDestroy( papszCreateOptions );
    papszCreateOptions = nullptr;

    if( l_hTIFF == nullptr )
    {
        if( bStreaming ) VSIUnlink(l_osTmpFilename);
        return nullptr;
    }

    uint16_t l_nPlanarConfig = 0;
    TIFFGetField( l_hTIFF, TIFFTAG_PLANARCONFIG, &l_nPlanarConfig );

    uint16_t l_nCompression = 0;

    if( !TIFFGetField( l_hTIFF, TIFFTAG_COMPRESSION, &(l_nCompression) ) )
        l_nCompression = COMPRESSION_NONE;

/* -------------------------------------------------------------------- */
/*      Set the alpha channel if it is the last one.                    */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetRasterBand(l_nBands)->GetColorInterpretation() ==
        GCI_AlphaBand )
    {
        uint16_t *v = nullptr;
        uint16_t count = 0;
        if( TIFFGetField( l_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            const int nBaseSamples = l_nBands - count;
            if( l_nBands > nBaseSamples && l_nBands - nBaseSamples - 1 < count )
            {
                // We need to allocate a new array as (current) libtiff
                // versions will not like that we reuse the array we got from
                // TIFFGetField().

                uint16_t* pasNewExtraSamples =
                    static_cast<uint16_t *>(
                        CPLMalloc( count * sizeof(uint16_t) ) );
                memcpy( pasNewExtraSamples, v, count * sizeof(uint16_t) );
                pasNewExtraSamples[l_nBands - nBaseSamples - 1] =
                    GTiffGetAlphaValue(
                        CPLGetConfigOption(
                            "GTIFF_ALPHA",
                            CSLFetchNameValue(papszOptions,"ALPHA") ),
                        DEFAULT_ALPHA_TYPE);

                TIFFSetField( l_hTIFF, TIFFTAG_EXTRASAMPLES, count,
                              pasNewExtraSamples);

                CPLFree(pasNewExtraSamples);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If the output is jpeg compressed, and the input is RGB make     */
/*      sure we note that.                                              */
/* -------------------------------------------------------------------- */

    if( l_nCompression == COMPRESSION_JPEG )
    {
        if( l_nBands >= 3
            && (poSrcDS->GetRasterBand(1)->GetColorInterpretation()
                == GCI_YCbCr_YBand)
            && (poSrcDS->GetRasterBand(2)->GetColorInterpretation()
                == GCI_YCbCr_CbBand)
            && (poSrcDS->GetRasterBand(3)->GetColorInterpretation()
                == GCI_YCbCr_CrBand) )
        {
            // Do nothing.
        }
        else
        {
            // Assume RGB if it is not explicitly YCbCr.
            CPLDebug( "GTiff", "Setting JPEGCOLORMODE_RGB" );
            TIFFSetField( l_hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB );
        }
    }

/* -------------------------------------------------------------------- */
/*      Does the source image consist of one band, with a palette?      */
/*      If so, copy over.                                               */
/* -------------------------------------------------------------------- */
    if( (l_nBands == 1 || l_nBands == 2) &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr
        && eType == GDT_Byte )
    {
        unsigned short anTRed[256] = { 0 };
        unsigned short anTGreen[256] = { 0 };
        unsigned short anTBlue[256] = { 0 };
        GDALColorTable *poCT = poSrcDS->GetRasterBand(1)->GetColorTable();

        for( int iColor = 0; iColor < 256; ++iColor )
        {
            if( iColor < poCT->GetColorEntryCount() )
            {
                GDALColorEntry sRGB = { 0, 0, 0, 0 };

                poCT->GetColorEntryAsRGB( iColor, &sRGB );

                anTRed[iColor] = static_cast<unsigned short>(257 * sRGB.c1);
                anTGreen[iColor] = static_cast<unsigned short>(257 * sRGB.c2);
                anTBlue[iColor] = static_cast<unsigned short>(257 * sRGB.c3);
            }
            else
            {
                anTRed[iColor] = 0;
                anTGreen[iColor] = 0;
                anTBlue[iColor] = 0;
            }
        }

        if( !bForcePhotometric )
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
        TIFFSetField( l_hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue );
    }
    else if( (l_nBands == 1 || l_nBands == 2)
             && poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr
             && eType == GDT_UInt16 )
    {
        unsigned short *panTRed = static_cast<unsigned short *>(
            CPLMalloc(65536 * sizeof(unsigned short)) );
        unsigned short *panTGreen = static_cast<unsigned short *>(
            CPLMalloc(65536 * sizeof(unsigned short)) );
        unsigned short *panTBlue = static_cast<unsigned short *>(
            CPLMalloc(65536 * sizeof(unsigned short)) );

        GDALColorTable *poCT = poSrcDS->GetRasterBand(1)->GetColorTable();

        for( int iColor = 0; iColor < 65536; ++iColor )
        {
            if( iColor < poCT->GetColorEntryCount() )
            {
                GDALColorEntry sRGB = { 0, 0, 0, 0 };

                poCT->GetColorEntryAsRGB( iColor, &sRGB );

                panTRed[iColor] = static_cast<unsigned short>(257 * sRGB.c1);
                panTGreen[iColor] = static_cast<unsigned short>(257 * sRGB.c2);
                panTBlue[iColor] = static_cast<unsigned short>(257 * sRGB.c3);
            }
            else
            {
                panTRed[iColor] = 0;
                panTGreen[iColor] = 0;
                panTBlue[iColor] = 0;
            }
        }

        if( !bForcePhotometric )
            TIFFSetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
        TIFFSetField( l_hTIFF, TIFFTAG_COLORMAP, panTRed, panTGreen, panTBlue );

        CPLFree( panTRed );
        CPLFree( panTGreen );
        CPLFree( panTBlue );
    }
    else if( poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr )
        ReportError( pszFilename, CE_Failure, CPLE_AppDefined,
            "Unable to export color table to GeoTIFF file.  Color tables "
            "can only be written to 1 band or 2 bands Byte or "
            "UInt16 GeoTIFF files." );

    if( l_nCompression == COMPRESSION_JPEG )
    {
        uint16_t l_nPhotometric = 0;
        TIFFGetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, &l_nPhotometric);
        // Check done in tif_jpeg.c later, but not with a very clear error message
        if( l_nPhotometric == PHOTOMETRIC_PALETTE )
        {
            ReportError( pszFilename, CE_Failure, CPLE_NotSupported,
                     "JPEG compression not supported with paletted image");
            XTIFFClose( l_hTIFF );
            VSIUnlink(l_osTmpFilename);
            CPL_IGNORE_RET_VAL( VSIFCloseL(l_fpL) );
            return nullptr;
        }
    }

    if( l_nBands == 2
        && poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr
        && (eType == GDT_Byte || eType == GDT_UInt16) )
    {
        uint16_t v[1] = { EXTRASAMPLE_UNASSALPHA };

        TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, 1, v );
    }

    const int nMaskFlags = poSrcDS->GetRasterBand(1)->GetMaskFlags();
    bool bCreateMask = false;
    CPLString osHiddenStructuralMD;
    if( (l_nBands == 1 || l_nPlanarConfig == PLANARCONFIG_CONTIG) &&
        bCopySrcOverviews )
    {
        osHiddenStructuralMD += "LAYOUT=IFDS_BEFORE_DATA\n";
        osHiddenStructuralMD += "BLOCK_ORDER=ROW_MAJOR\n";
        osHiddenStructuralMD += "BLOCK_LEADER=SIZE_AS_UINT4\n";
        osHiddenStructuralMD += "BLOCK_TRAILER=LAST_4_BYTES_REPEATED\n";
        osHiddenStructuralMD += "KNOWN_INCOMPATIBLE_EDITION=NO\n "; // Final space intended, so this can be replaced by YES
    }
    if( !(nMaskFlags & (GMF_ALL_VALID|GMF_ALPHA|GMF_NODATA) )
        && (nMaskFlags & GMF_PER_DATASET) && !bStreaming )
    {
        bCreateMask = true;
        if( GTiffDataset::MustCreateInternalMask() &&
            !osHiddenStructuralMD.empty() )
        {
            osHiddenStructuralMD += "MASK_INTERLEAVED_WITH_IMAGERY=YES\n";
        }
    }
    if( !osHiddenStructuralMD.empty() )
    {
        const int nHiddenMDSize = static_cast<int>(osHiddenStructuralMD.size());
        osHiddenStructuralMD = CPLOPrintf(
            "GDAL_STRUCTURAL_METADATA_SIZE=%06d bytes\n", nHiddenMDSize) + osHiddenStructuralMD;
        VSI_TIFFWrite(l_hTIFF, osHiddenStructuralMD.c_str(), osHiddenStructuralMD.size());
    }


    // FIXME? libtiff writes extended tags in the order they are specified
    // and not in increasing order.

/* -------------------------------------------------------------------- */
/*      Transfer some TIFF specific metadata, if available.             */
/*      The return value will tell us if we need to try again later with*/
/*      PAM because the profile doesn't allow to write some metadata    */
/*      as TIFF tag                                                     */
/* -------------------------------------------------------------------- */
    const bool bHasWrittenMDInGeotiffTAG =
            GTiffDataset::WriteMetadata( poSrcDS, l_hTIFF, false, eProfile,
                                         pszFilename, papszOptions );

/* -------------------------------------------------------------------- */
/*      Write NoData value, if exist.                                   */
/* -------------------------------------------------------------------- */
    if( eProfile == GTiffProfile::GDALGEOTIFF )
    {
        int bSuccess = FALSE;
        const double dfNoData =
            poSrcDS->GetRasterBand(1)->GetNoDataValue( &bSuccess );
        if( bSuccess )
            GTiffDataset::WriteNoDataValue( l_hTIFF, dfNoData );
    }

/* -------------------------------------------------------------------- */
/*      Are we addressing PixelIsPoint mode?                            */
/* -------------------------------------------------------------------- */
    bool bPixelIsPoint = false;
    bool bPointGeoIgnore = false;

    if( poSrcDS->GetMetadataItem( GDALMD_AREA_OR_POINT )
        && EQUAL(poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT),
                 GDALMD_AOP_POINT) )
    {
        bPixelIsPoint = true;
        bPointGeoIgnore =
            CPLTestBool( CPLGetConfigOption( "GTIFF_POINT_GEO_IGNORE",
                                             "FALSE") );
    }

/* -------------------------------------------------------------------- */
/*      Write affine transform if it is meaningful.                     */
/* -------------------------------------------------------------------- */
    const OGRSpatialReference* l_poSRS = nullptr;
    double l_adfGeoTransform[6] = { 0.0 };

    if( poSrcDS->GetGeoTransform( l_adfGeoTransform ) == CE_None )
    {
        if( bGeoTIFF )
        {
            l_poSRS = poSrcDS->GetSpatialRef();

            if( l_adfGeoTransform[2] == 0.0 && l_adfGeoTransform[4] == 0.0
                && l_adfGeoTransform[5] < 0.0 )
            {
                double dfOffset = 0.0;
                {
                    // In the case the SRS has a vertical component and we have
                    // a single band, encode its scale/offset in the GeoTIFF tags
                    int bHasScale = FALSE;
                    double dfScale =
                        poSrcDS->GetRasterBand(1)->GetScale(&bHasScale);
                    int bHasOffset = FALSE;
                    dfOffset =
                        poSrcDS->GetRasterBand(1)->GetOffset(&bHasOffset);
                    const bool bApplyScaleOffset =
                        l_poSRS && l_poSRS->IsVertical() &&
                        poSrcDS->GetRasterCount() == 1;
                    if( bApplyScaleOffset && !bHasScale )
                        dfScale = 1.0;
                    if( !bApplyScaleOffset || !bHasOffset )
                        dfOffset = 0.0;
                    const double adfPixelScale[3] = {
                        l_adfGeoTransform[1], fabs(l_adfGeoTransform[5]),
                        bApplyScaleOffset ? dfScale : 0.0 };

                    TIFFSetField( l_hTIFF, TIFFTAG_GEOPIXELSCALE, 3,
                                  adfPixelScale );
                }

                double adfTiePoints[6] = {
                    0.0,
                    0.0,
                    0.0,
                    l_adfGeoTransform[0],
                    l_adfGeoTransform[3],
                    dfOffset
                };

                if( bPixelIsPoint && !bPointGeoIgnore )
                {
                    adfTiePoints[3] +=
                        l_adfGeoTransform[1] * 0.5 + l_adfGeoTransform[2] * 0.5;
                    adfTiePoints[4] +=
                        l_adfGeoTransform[4] * 0.5 + l_adfGeoTransform[5] * 0.5;
                }

                TIFFSetField( l_hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
            }
            else
            {
                double adfMatrix[16] = { 0.0 };

                adfMatrix[0] = l_adfGeoTransform[1];
                adfMatrix[1] = l_adfGeoTransform[2];
                adfMatrix[3] = l_adfGeoTransform[0];
                adfMatrix[4] = l_adfGeoTransform[4];
                adfMatrix[5] = l_adfGeoTransform[5];
                adfMatrix[7] = l_adfGeoTransform[3];
                adfMatrix[15] = 1.0;

                if( bPixelIsPoint && !bPointGeoIgnore )
                {
                    adfMatrix[3] +=
                        l_adfGeoTransform[1] * 0.5 + l_adfGeoTransform[2] * 0.5;
                    adfMatrix[7] +=
                        l_adfGeoTransform[4] * 0.5 + l_adfGeoTransform[5] * 0.5;
                }

                TIFFSetField( l_hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix );
            }
        }

/* -------------------------------------------------------------------- */
/*      Do we need a TFW file?                                          */
/* -------------------------------------------------------------------- */
        if( CPLFetchBool( papszOptions, "TFW", false ) )
            GDALWriteWorldFile( pszFilename, "tfw", l_adfGeoTransform );
        else if( CPLFetchBool( papszOptions, "WORLDFILE", false ) )
            GDALWriteWorldFile( pszFilename, "wld", l_adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise write tiepoints if they are available.                */
/* -------------------------------------------------------------------- */
    else if( poSrcDS->GetGCPCount() > 0 && bGeoTIFF )
    {
        const GDAL_GCP *pasGCPs = poSrcDS->GetGCPs();
        double *padfTiePoints = static_cast<double *>(
            CPLMalloc(6 * sizeof(double) * poSrcDS->GetGCPCount()) );

        for( int iGCP = 0; iGCP < poSrcDS->GetGCPCount(); ++iGCP )
        {

            padfTiePoints[iGCP*6+0] = pasGCPs[iGCP].dfGCPPixel;
            padfTiePoints[iGCP*6+1] = pasGCPs[iGCP].dfGCPLine;
            padfTiePoints[iGCP*6+2] = 0;
            padfTiePoints[iGCP*6+3] = pasGCPs[iGCP].dfGCPX;
            padfTiePoints[iGCP*6+4] = pasGCPs[iGCP].dfGCPY;
            padfTiePoints[iGCP*6+5] = pasGCPs[iGCP].dfGCPZ;

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                padfTiePoints[iGCP*6+0] -= 0.5;
                padfTiePoints[iGCP*6+1] -= 0.5;
            }
        }

        TIFFSetField( l_hTIFF, TIFFTAG_GEOTIEPOINTS,
                      6*poSrcDS->GetGCPCount(), padfTiePoints );
        CPLFree( padfTiePoints );

        l_poSRS = poSrcDS->GetGCPSpatialRef();

        if( CPLFetchBool( papszOptions, "TFW", false )
            || CPLFetchBool( papszOptions, "WORLDFILE", false ) )
        {
            ReportError( pszFilename,CE_Warning, CPLE_AppDefined,
                "TFW=ON or WORLDFILE=ON creation options are ignored when "
                "GCPs are available" );
        }
    }
    else
    {
        l_poSRS = poSrcDS->GetSpatialRef();
    }

/* -------------------------------------------------------------------- */
/*      Copy xml:XMP data                                               */
/* -------------------------------------------------------------------- */
    char **papszXMP = poSrcDS->GetMetadata("xml:XMP");
    if( papszXMP != nullptr && *papszXMP != nullptr )
    {
        int nTagSize = static_cast<int>(strlen(*papszXMP));
        TIFFSetField( l_hTIFF, TIFFTAG_XMLPACKET, nTagSize, *papszXMP );
    }

/* -------------------------------------------------------------------- */
/*      Write the projection information, if possible.                  */
/* -------------------------------------------------------------------- */
    const bool bHasProjection = l_poSRS != nullptr;
    bool bExportSRSToPAM = false;
    if( (bHasProjection || bPixelIsPoint) && bGeoTIFF )
    {
        GTIF *psGTIF = GTiffDatasetGTIFNew( l_hTIFF );

        if( bHasProjection )
        {
            char* pszWKT = nullptr;
            OGRErr eErr;
            {
                CPLErrorStateBackuper oErrorStateBackuper;
                CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
                eErr = l_poSRS->exportToWkt(&pszWKT);
            }
            if( eErr == OGRERR_NONE && strstr(pszWKT, "custom_proj4") == nullptr )
            {
                GTIFSetFromOGISDefnEx( psGTIF, pszWKT,
                                    GetGTIFFKeysFlavor(papszOptions),
                                    GetGeoTIFFVersion(papszOptions) );
            }
            else
            {
                bExportSRSToPAM = true;
            }
            CPLFree(pszWKT);
        }

        if( bPixelIsPoint )
        {
            GTIFKeySet( psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1,
                        RasterPixelIsPoint );
        }

        GTIFWriteKeys( psGTIF );
        GTIFFree( psGTIF );
    }

    bool l_bDontReloadFirstBlock = false;

#ifdef HAVE_LIBJPEG
    if( bCopyFromJPEG )
    {
        GTIFF_CopyFromJPEG_WriteAdditionalTags(l_hTIFF,
                                               poSrcDS);
    }
#endif

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
    if( bCopySrcOverviews )
    {
        TIFFDeferStrileArrayWriting( l_hTIFF );
    }
#endif
    TIFFWriteCheck( l_hTIFF, TIFFIsTiled(l_hTIFF), "GTiffCreateCopy()" );
    TIFFWriteDirectory( l_hTIFF );
    if( bStreaming )
    {
        // We need to write twice the directory to be sure that custom
        // TIFF tags are correctly sorted and that padding bytes have been
        // added.
        TIFFSetDirectory( l_hTIFF, 0 );
        TIFFWriteDirectory( l_hTIFF );

        if( VSIFSeekL( l_fpL, 0, SEEK_END ) != 0 )
            ReportError(pszFilename, CE_Failure, CPLE_FileIO, "Cannot seek");
        const int nSize = static_cast<int>( VSIFTellL(l_fpL) );

        vsi_l_offset nDataLength = 0;
        VSIGetMemFileBuffer( l_osTmpFilename, &nDataLength, FALSE);
        TIFFSetDirectory( l_hTIFF, 0 );
        GTiffFillStreamableOffsetAndCount( l_hTIFF, nSize );
        TIFFWriteDirectory( l_hTIFF );
    }
    TIFFSetDirectory( l_hTIFF,
                    static_cast<tdir_t>(TIFFNumberOfDirectories(l_hTIFF) - 1) );
    const toff_t l_nDirOffset = TIFFCurrentDirOffset( l_hTIFF );
    TIFFFlush( l_hTIFF );
    XTIFFClose( l_hTIFF );

    VSIFSeekL(l_fpL, 0, SEEK_SET);

    // fpStreaming will assigned to the instance and not closed here.
    VSILFILE *fpStreaming = nullptr;
    if( bStreaming )
    {
        vsi_l_offset nDataLength = 0;
        void* pabyBuffer =
            VSIGetMemFileBuffer( l_osTmpFilename, &nDataLength, FALSE);
        fpStreaming = VSIFOpenL( pszFilename, "wb" );
        if( fpStreaming == nullptr )
        {
            VSIUnlink(l_osTmpFilename);
            CPL_IGNORE_RET_VAL( VSIFCloseL(l_fpL) );
            return nullptr;
        }
        if( static_cast<vsi_l_offset>(
                VSIFWriteL( pabyBuffer, 1, static_cast<int>(nDataLength),
                            fpStreaming ) ) != nDataLength )
        {
            ReportError(pszFilename, CE_Failure, CPLE_FileIO,
                        "Could not write %d bytes",
                        static_cast<int>(nDataLength) );
            CPL_IGNORE_RET_VAL(VSIFCloseL( fpStreaming ));
            VSIUnlink(l_osTmpFilename);
            CPL_IGNORE_RET_VAL( VSIFCloseL(l_fpL) );
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Re-open as a dataset and copy over missing metadata using       */
/*      PAM facilities.                                                 */
/* -------------------------------------------------------------------- */
    l_hTIFF =
        VSI_TIFFOpen( bStreaming ? l_osTmpFilename.c_str() : pszFilename,
                      "r+",
                      l_fpL );
    if( l_hTIFF == nullptr )
    {
        if( bStreaming ) VSIUnlink(l_osTmpFilename);
        CPL_IGNORE_RET_VAL( VSIFCloseL(l_fpL) );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->SetDescription( pszFilename );
    poDS->eAccess = GA_Update;
    poDS->m_pszFilename = CPLStrdup(pszFilename);
    poDS->m_fpL = l_fpL;
    poDS->m_bIMDRPCMetadataLoaded = true;

    const bool bAppend = CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false);
    if( poDS->OpenOffset( l_hTIFF,
                          bAppend ? l_nDirOffset : TIFFCurrentDirOffset(l_hTIFF),
                          GA_Update,
                          false, // bAllowRGBAInterface
                          true // bReadGeoTransform
                         ) != CE_None )
    {
        delete poDS;
        if( bStreaming ) VSIUnlink(l_osTmpFilename);
        return nullptr;
    }
    poDS->oOvManager.Initialize( poDS, pszFilename );

    if( bStreaming )
    {
        VSIUnlink(l_osTmpFilename);
        poDS->m_fpToWrite = fpStreaming;
    }
    poDS->m_eProfile = eProfile;

    int nCloneInfoFlags = GCIF_PAM_DEFAULT & ~GCIF_MASK;

    // If we explicitly asked not to tag the alpha band as such, do not
    // reintroduce this alpha color interpretation in PAM.
    if( poSrcDS->GetRasterBand(l_nBands)->GetColorInterpretation() ==
        GCI_AlphaBand &&
        GTiffGetAlphaValue(
            CPLGetConfigOption(
                "GTIFF_ALPHA",
                CSLFetchNameValue(papszOptions,"ALPHA") ),
            DEFAULT_ALPHA_TYPE) == EXTRASAMPLE_UNSPECIFIED )
    {
        nCloneInfoFlags &= ~GCIF_COLORINTERP;
    }
    // Ignore source band color interpretation if requesting PHOTOMETRIC=RGB
    else if( l_nBands >= 3 &&
        EQUAL(CSLFetchNameValueDef(papszOptions, "PHOTOMETRIC", ""), "RGB") )
    {
        for( int i = 1; i <= 3; i++)
        {
            poDS->GetRasterBand(i)->SetColorInterpretation(
                static_cast<GDALColorInterp>(GCI_RedBand + (i-1)));
        }
        nCloneInfoFlags &= ~GCIF_COLORINTERP;
        if( !(l_nBands == 4 && CSLFetchNameValue(papszOptions, "ALPHA") != nullptr) )
        {
            for( int i = 4; i <= l_nBands; i++)
            {
                poDS->GetRasterBand(i)->SetColorInterpretation(
                    poSrcDS->GetRasterBand(i)->GetColorInterpretation());
            }
        }
    }

    CPLString osOldGTIFF_REPORT_COMPD_CSVal(
        CPLGetConfigOption("GTIFF_REPORT_COMPD_CS", ""));
    CPLSetThreadLocalConfigOption("GTIFF_REPORT_COMPD_CS", "YES");
    poDS->CloneInfo( poSrcDS, nCloneInfoFlags );
    CPLSetThreadLocalConfigOption("GTIFF_REPORT_COMPD_CS",
        osOldGTIFF_REPORT_COMPD_CSVal.empty() ? nullptr :
        osOldGTIFF_REPORT_COMPD_CSVal.c_str());

    if( (!bGeoTIFF || bExportSRSToPAM) && (poDS->GetPamFlags() & GPF_DISABLED) == 0 )
    {
        // Copy georeferencing info to PAM if the profile is not GeoTIFF
        poDS->GDALPamDataset::SetSpatialRef(poDS->GetSpatialRef());
        double adfGeoTransform[6];
        if( poDS->GetGeoTransform(adfGeoTransform) == CE_None )
        {
            poDS->GDALPamDataset::SetGeoTransform(adfGeoTransform);
        }
        poDS->GDALPamDataset::SetGCPs(poDS->GetGCPCount(),
                                      poDS->GetGCPs(),
                                      poDS->GetGCPSpatialRef());
    }

    poDS->m_papszCreationOptions = CSLDuplicate( papszOptions );
    poDS->m_bDontReloadFirstBlock = l_bDontReloadFirstBlock;

/* -------------------------------------------------------------------- */
/*      CloneInfo() does not merge metadata, it just replaces it        */
/*      totally.  So we have to merge it.                               */
/* -------------------------------------------------------------------- */

    char **papszSRC_MD = poSrcDS->GetMetadata();
    char **papszDST_MD = CSLDuplicate(poDS->GetMetadata());

    papszDST_MD = CSLMerge( papszDST_MD, papszSRC_MD );

    poDS->SetMetadata( papszDST_MD );
    CSLDestroy( papszDST_MD );

    // Depending on the PHOTOMETRIC tag, the TIFF file may not have the same
    // band count as the source. Will fail later in GDALDatasetCopyWholeRaster
    // anyway.
    for( int nBand = 1;
         nBand <= std::min(poDS->GetRasterCount(), poSrcDS->GetRasterCount()) ;
         ++nBand )
    {
        GDALRasterBand* poSrcBand = poSrcDS->GetRasterBand(nBand);
        GDALRasterBand* poDstBand = poDS->GetRasterBand(nBand);
        papszSRC_MD = poSrcBand->GetMetadata();
        papszDST_MD = CSLDuplicate(poDstBand->GetMetadata());

        papszDST_MD = CSLMerge( papszDST_MD, papszSRC_MD );

        poDstBand->SetMetadata( papszDST_MD );
        CSLDestroy( papszDST_MD );

        char** papszCatNames = poSrcBand->GetCategoryNames();
        if( nullptr != papszCatNames )
            poDstBand->SetCategoryNames( papszCatNames );
    }

    l_hTIFF = static_cast<TIFF *>( poDS->GetInternalHandle(nullptr) );

/* -------------------------------------------------------------------- */
/*      Handle forcing xml:ESRI data to be written to PAM.              */
/* -------------------------------------------------------------------- */
    if( CPLTestBool(CPLGetConfigOption( "ESRI_XML_PAM", "NO" )) )
    {
        char **papszESRIMD = poSrcDS->GetMetadata("xml:ESRI");
        if( papszESRIMD )
        {
            poDS->SetMetadata( papszESRIMD, "xml:ESRI");
        }
    }

/* -------------------------------------------------------------------- */
/*      Second chance: now that we have a PAM dataset, it is possible   */
/*      to write metadata that we could not write as a TIFF tag.        */
/* -------------------------------------------------------------------- */
    if( !bHasWrittenMDInGeotiffTAG && !bStreaming )
    {
        GTiffDataset::WriteMetadata(
            poDS, l_hTIFF, true, eProfile,
            pszFilename, papszOptions,
            true /* don't write RPC and IMD file again */ );
    }

    if( !bStreaming )
        GTiffDataset::WriteRPC(
            poDS, l_hTIFF, true, eProfile,
            pszFilename, papszOptions,
            true /* write only in PAM AND if needed */ );

    // Propagate ISIS3 or VICAR metadata, but only as PAM metadata.
    for( const char* pszMDD: { "json:ISIS3", "json:VICAR" } )
    {
        char **papszMD = poSrcDS->GetMetadata(pszMDD);
        if( papszMD )
        {
            poDS->SetMetadata( papszMD, pszMDD);
            poDS->PushMetadataToPam();
        }
    }

    poDS->m_bWriteCOGLayout = bCopySrcOverviews;

    // To avoid unnecessary directory rewriting.
    poDS->m_bMetadataChanged = false;
    poDS->m_bGeoTIFFInfoChanged = false;
    poDS->m_bNoDataChanged = false;
    poDS->m_bForceUnsetGTOrGCPs = false;
    poDS->m_bForceUnsetProjection = false;
    poDS->m_bStreamingOut = bStreaming;

    // Don't try to load external metadata files (#6597).
    poDS->m_bIMDRPCMetadataLoaded = true;

    // We must re-set the compression level at this point, since it has been
    // lost a few lines above when closing the newly create TIFF file The
    // TIFFTAG_ZIPQUALITY & TIFFTAG_JPEGQUALITY are not store in the TIFF file.
    // They are just TIFF session parameters.

    poDS->m_nZLevel = GTiffGetZLevel(papszOptions);
    poDS->m_nLZMAPreset = GTiffGetLZMAPreset(papszOptions);
    poDS->m_nZSTDLevel = GTiffGetZSTDPreset(papszOptions);
    poDS->m_nWebPLevel = GTiffGetWebPLevel(papszOptions);
    poDS->m_bWebPLossless = GTiffGetWebPLossless(papszOptions);
    poDS->m_nJpegQuality = GTiffGetJpegQuality(papszOptions);
    poDS->m_nJpegTablesMode = GTiffGetJpegTablesMode(papszOptions);
    poDS->GetDiscardLsbOption(papszOptions);
    poDS->m_dfMaxZError = GTiffGetLERCMaxZError(papszOptions);
    poDS->InitCreationOrOpenOptions(papszOptions);

    if( l_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        l_nCompression == COMPRESSION_LERC )
    {
        GTiffSetDeflateSubCodec(l_hTIFF);

        if( poDS->m_nZLevel != -1 )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_ZIPQUALITY, poDS->m_nZLevel );
        }
    }
    if( l_nCompression == COMPRESSION_JPEG )
    {
        if( poDS->m_nJpegQuality != -1 )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_JPEGQUALITY, poDS->m_nJpegQuality );
        }
        TIFFSetField( l_hTIFF, TIFFTAG_JPEGTABLESMODE, poDS->m_nJpegTablesMode );
    }
    if( l_nCompression == COMPRESSION_LZMA )
    {
        if( poDS->m_nLZMAPreset != -1 )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_LZMAPRESET, poDS->m_nLZMAPreset );
        }
    }
    if( l_nCompression == COMPRESSION_ZSTD ||
        l_nCompression == COMPRESSION_LERC )
    {
        if( poDS->m_nZSTDLevel != -1 )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_ZSTD_LEVEL, poDS->m_nZSTDLevel );
        }
    }
    if( l_nCompression == COMPRESSION_LERC )
    {
        TIFFSetField( l_hTIFF, TIFFTAG_LERC_MAXZERROR, poDS->m_dfMaxZError );
    }
    if( l_nCompression == COMPRESSION_WEBP )
    {
        if( poDS->m_nWebPLevel != -1 )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_WEBP_LEVEL, poDS->m_nWebPLevel );
        }

        if( poDS->m_bWebPLossless)
        {
          TIFFSetField( l_hTIFF, TIFFTAG_WEBP_LOSSLESS, poDS->m_bWebPLossless );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we want to ensure all blocks get written out on close to     */
/*      avoid sparse files?                                             */
/* -------------------------------------------------------------------- */
    if( !CPLFetchBool( papszOptions, "SPARSE_OK", false ) )
        poDS->m_bFillEmptyTilesAtClosing = true;

    poDS->m_bWriteEmptyTiles =
        (bCopySrcOverviews && poDS->m_bFillEmptyTilesAtClosing) ||
        bStreaming ||
        (poDS->m_nCompression != COMPRESSION_NONE &&
            poDS->m_bFillEmptyTilesAtClosing);
    // Only required for people writing non-compressed striped files in the
    // rightorder and wanting all tstrips to be written in the same order
    // so that the end result can be memory mapped without knowledge of each
    // strip offset
    if( CPLTestBool( CSLFetchNameValueDef(
                            papszOptions,
                            "WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE" )) ||
        CPLTestBool( CSLFetchNameValueDef(
                            papszOptions,
                            "@WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE" )) )
    {
        poDS->m_bWriteEmptyTiles = true;
    }

    // Precreate (internal) mask, so that the IBuildOverviews() below
    // has a chance to create also the overviews of the mask.
    CPLErr eErr = CE_None;

    if( bCreateMask )
    {
        eErr = poDS->CreateMaskBand( nMaskFlags );
        if( poDS->m_poMaskDS )
        {
            poDS->m_poMaskDS->m_bFillEmptyTilesAtClosing = poDS->m_bFillEmptyTilesAtClosing;
            poDS->m_poMaskDS->m_bWriteEmptyTiles = poDS->m_bWriteEmptyTiles;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create and then copy existing overviews if requested            */
/*  We do it such that all the IFDs are at the beginning of the file,   */
/*  and that the imagery data for the smallest overview is written      */
/*  first, that way the file is more usable when embedded in a          */
/*  compressed stream.                                                  */
/* -------------------------------------------------------------------- */

    // For scaled progress due to overview copying.
    const int nBandsWidthMask = l_nBands +  (bCreateMask ? 1 : 0);
    double dfTotalPixels =
        static_cast<double>(nXSize) * nYSize * nBandsWidthMask;
    double dfCurPixels = 0;

    if( eErr == CE_None && bCopySrcOverviews )
    {
        std::unique_ptr<GDALDataset> poMaskOvrDS;
        const char* pszMaskOvrDS = CSLFetchNameValue(papszOptions, "@MASK_OVERVIEW_DATASET");
        if( pszMaskOvrDS )
        {
            poMaskOvrDS.reset(GDALDataset::Open(pszMaskOvrDS));
            if( !poMaskOvrDS )
            {
                delete poDS;
                return nullptr;
            }
            if( poMaskOvrDS->GetRasterCount() != 1 )
            {
                delete poDS;
                return nullptr;
            }
        }
        if( nSrcOverviews )
        {
            eErr = poDS->CreateOverviewsFromSrcOverviews(poSrcDS, poOvrDS.get());

            if( eErr == CE_None &&
                (poMaskOvrDS != nullptr ||
                 (poSrcDS->GetRasterBand(1)->GetOverview(0) &&
                  poSrcDS->GetRasterBand(1)->GetOverview(0)->GetMaskFlags() == GMF_PER_DATASET)) )
            {
                int nOvrBlockXSize = 0;
                int nOvrBlockYSize = 0;
                GTIFFGetOverviewBlockSize(
                    GDALRasterBand::ToHandle(poDS->GetRasterBand(1)),
                    &nOvrBlockXSize, &nOvrBlockYSize);
                eErr = poDS->CreateInternalMaskOverviews(nOvrBlockXSize, nOvrBlockYSize);
            }
        }

#ifdef SUPPORTS_GET_OFFSET_BYTECOUNT
        TIFFForceStrileArrayWriting( poDS->m_hTIFF );

        if( poDS->m_poMaskDS )
        {
            TIFFForceStrileArrayWriting( poDS->m_poMaskDS->m_hTIFF );
        }

        for( int i = 0; i < poDS->m_nOverviewCount; i++)
        {
            TIFFForceStrileArrayWriting( poDS->m_papoOverviewDS[i]->m_hTIFF );

            if( poDS->m_papoOverviewDS[i]->m_poMaskDS )
            {
                TIFFForceStrileArrayWriting(
                    poDS->m_papoOverviewDS[i]->m_poMaskDS->m_hTIFF );
            }
        }
#endif

        if( eErr == CE_None && nSrcOverviews )
        {
            if( poDS->m_nOverviewCount != nSrcOverviews )
            {
                ReportError( pszFilename, CE_Failure, CPLE_AppDefined,
                        "Did only manage to instantiate %d overview levels, "
                        "whereas source contains %d",
                        poDS->m_nOverviewCount, nSrcOverviews);
                eErr = CE_Failure;
            }

            for( int i = 0; eErr == CE_None && i < nSrcOverviews; ++i )
            {
                GDALRasterBand* poOvrBand = poOvrDS ?
                    (i == 0 ? poOvrDS->GetRasterBand(1) : poOvrDS->GetRasterBand(1)->GetOverview(i-1)) :
                    poSrcDS->GetRasterBand(1)->GetOverview(i);
                const double dfOvrPixels =
                    static_cast<double>(poOvrBand->GetXSize()) *
                                poOvrBand->GetYSize();
                dfTotalPixels += dfOvrPixels * l_nBands;
                if( poOvrBand->GetMaskFlags() == GMF_PER_DATASET ||
                    poMaskOvrDS != nullptr )
                {
                    dfTotalPixels += dfOvrPixels;
                }
                else if( i == 0 &&
                         poDS->GetRasterBand(1)->GetMaskFlags() == GMF_PER_DATASET )
                {
                    ReportError( pszFilename, CE_Warning, CPLE_AppDefined,
                             "Source dataset has a mask band on full "
                             "resolution, overviews on the regular bands, "
                             "but lacks overviews on the mask band.");
                }
            }

            char* papszCopyWholeRasterOptions[2] = { nullptr, nullptr };
            if( l_nCompression != COMPRESSION_NONE )
                papszCopyWholeRasterOptions[0] =
                    const_cast<char*>( "COMPRESSED=YES" );
            // Now copy the imagery.
            // Begin with the smallest overview.
            for( int iOvrLevel = nSrcOverviews - 1;
                    eErr == CE_None && iOvrLevel >= 0; --iOvrLevel )
            {
                auto poDstDS = poDS->m_papoOverviewDS[iOvrLevel];

                // Create a fake dataset with the source overview level so that
                // GDALDatasetCopyWholeRaster can cope with it.
                GDALDataset* poSrcOvrDS = poOvrDS ?
                    (iOvrLevel == 0 ? poOvrDS.get() :
                        GDALCreateOverviewDataset(poOvrDS.get(), iOvrLevel - 1, TRUE)) :
                    GDALCreateOverviewDataset(poSrcDS, iOvrLevel, TRUE);
                GDALRasterBand* poSrcOvrBand = poOvrDS ?
                    (iOvrLevel == 0 ? poOvrDS->GetRasterBand(1):
                        poOvrDS->GetRasterBand(1)->GetOverview(iOvrLevel - 1)) :
                    poSrcDS->GetRasterBand(1)->GetOverview(iOvrLevel);
                double dfNextCurPixels =
                    dfCurPixels +
                    static_cast<double>(poSrcOvrBand->GetXSize()) *
                    poSrcOvrBand->GetYSize() * l_nBands;

                poDstDS->m_bBlockOrderRowMajor = true;
                poDstDS->m_bLeaderSizeAsUInt4 = true;
                poDstDS->m_bTrailerRepeatedLast4BytesRepeated = true;
                poDstDS->m_bFillEmptyTilesAtClosing = poDS->m_bFillEmptyTilesAtClosing;
                poDstDS->m_bWriteEmptyTiles = poDS->m_bWriteEmptyTiles;
                GDALRasterBand* poSrcMaskBand = nullptr;
                if( poDstDS->m_poMaskDS )
                {
                    poDstDS->m_poMaskDS->m_bBlockOrderRowMajor = true;
                    poDstDS->m_poMaskDS->m_bLeaderSizeAsUInt4 = true;
                    poDstDS->m_poMaskDS->m_bTrailerRepeatedLast4BytesRepeated = true;
                    poDstDS->m_poMaskDS->m_bFillEmptyTilesAtClosing = poDS->m_bFillEmptyTilesAtClosing;
                    poDstDS->m_poMaskDS->m_bWriteEmptyTiles = poDS->m_bWriteEmptyTiles;

                    poSrcMaskBand = poMaskOvrDS ?
                        (iOvrLevel == 0 ? poMaskOvrDS->GetRasterBand(1) :
                            poMaskOvrDS->GetRasterBand(1)->GetOverview(iOvrLevel - 1)) :
                        poSrcOvrBand->GetMaskBand();
                }

                if( l_nBands == 1 || poDstDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
                {
                    if( poDstDS->m_poMaskDS )
                    {
                        dfNextCurPixels +=
                            static_cast<double>(poSrcOvrBand->GetXSize()) *
                                                poSrcOvrBand->GetYSize();
                    }
                    void* pScaledData =
                        GDALCreateScaledProgress( dfCurPixels / dfTotalPixels,
                                                  dfNextCurPixels / dfTotalPixels,
                                                  pfnProgress, pProgressData );

                    eErr = CopyImageryAndMask(poDstDS, poSrcOvrDS,
                                      poSrcMaskBand,
                                      GDALScaledProgress, pScaledData);


                    dfCurPixels = dfNextCurPixels;
                    GDALDestroyScaledProgress(pScaledData);
                }
                else
                {
                    void* pScaledData =
                        GDALCreateScaledProgress( dfCurPixels / dfTotalPixels,
                                                dfNextCurPixels / dfTotalPixels,
                                                pfnProgress, pProgressData );

                    eErr =
                        GDALDatasetCopyWholeRaster(
                            GDALDataset::ToHandle(poSrcOvrDS),
                            GDALDataset::ToHandle(poDstDS),
                            papszCopyWholeRasterOptions,
                            GDALScaledProgress, pScaledData );

                    dfCurPixels = dfNextCurPixels;
                    GDALDestroyScaledProgress(pScaledData);

                    poDstDS->FlushCache();

                    // Copy mask of the overview.
                    if( eErr == CE_None &&
                        (poMaskOvrDS ||
                         poSrcOvrBand->GetMaskFlags() == GMF_PER_DATASET) &&
                        poDstDS->m_poMaskDS != nullptr )
                    {
                        dfNextCurPixels +=
                            static_cast<double>(poSrcOvrBand->GetXSize()) *
                                                poSrcOvrBand->GetYSize();
                        pScaledData =
                            GDALCreateScaledProgress( dfCurPixels / dfTotalPixels,
                                                dfNextCurPixels / dfTotalPixels,
                                                pfnProgress, pProgressData );
                        eErr =
                            GDALRasterBandCopyWholeRaster(
                                poSrcMaskBand,
                                poDstDS->m_poMaskDS->GetRasterBand(1),
                                papszCopyWholeRasterOptions,
                                GDALScaledProgress, pScaledData );
                        dfCurPixels = dfNextCurPixels;
                        GDALDestroyScaledProgress(pScaledData);
                        poDstDS->m_poMaskDS->FlushCache();
                    }
                }

                if( poSrcOvrDS != poOvrDS.get() )
                    delete poSrcOvrDS;
                poSrcOvrDS = nullptr;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy actual imagery.                                            */
/* -------------------------------------------------------------------- */
    double dfNextCurPixels =
        dfCurPixels + static_cast<double>(nXSize) * nYSize * l_nBands;
    void* pScaledData = GDALCreateScaledProgress(
        dfCurPixels / dfTotalPixels,
        dfNextCurPixels / dfTotalPixels,
        pfnProgress, pProgressData);

#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
    bool bTryCopy = true;
#endif

#ifdef HAVE_LIBJPEG
    if( bCopyFromJPEG )
    {
        eErr = GTIFF_CopyFromJPEG( poDS, poSrcDS,
                                   pfnProgress, pProgressData,
                                   bTryCopy );

        // In case of failure in the decompression step, try normal copy.
        if( bTryCopy )
            eErr = CE_None;
    }
#endif

#ifdef JPEG_DIRECT_COPY
    if( bDirectCopyFromJPEG )
    {
        eErr = GTIFF_DirectCopyFromJPEG(poDS, poSrcDS,
                                        pfnProgress, pProgressData,
                                        bTryCopy);

        // In case of failure in the reading step, try normal copy.
        if( bTryCopy )
            eErr = CE_None;
    }
#endif

    bool bWriteMask = true;
    if(
#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
        bTryCopy &&
#endif
        (poDS->m_bTreatAsSplit || poDS->m_bTreatAsSplitBitmap) )
    {
        // For split bands, we use TIFFWriteScanline() interface.
        CPLAssert(poDS->m_nBitsPerSample == 8 || poDS->m_nBitsPerSample == 1);

        if( poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG && poDS->nBands > 1 )
        {
            GByte* pabyScanline =
                static_cast<GByte *>(
                    VSI_MALLOC_VERBOSE(TIFFScanlineSize(l_hTIFF)) );
            if( pabyScanline == nullptr )
                eErr = CE_Failure;
            for( int j = 0; j < nYSize && eErr == CE_None; ++j )
            {
                eErr =
                    poSrcDS->RasterIO(
                        GF_Read, 0, j, nXSize, 1,
                        pabyScanline, nXSize, 1,
                        GDT_Byte, l_nBands, nullptr, poDS->nBands, 0, 1,
                        nullptr );
                if( eErr == CE_None &&
                    TIFFWriteScanline( l_hTIFF, pabyScanline, j, 0) == -1 )
                {
                    ReportError( pszFilename, CE_Failure, CPLE_AppDefined,
                              "TIFFWriteScanline() failed." );
                    eErr = CE_Failure;
                }
                if( !GDALScaledProgress( (j + 1) * 1.0 / nYSize,
                                         nullptr, pScaledData ) )
                    eErr = CE_Failure;
            }
            CPLFree( pabyScanline );
        }
        else
        {
            GByte* pabyScanline = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(nXSize) );
            if( pabyScanline == nullptr )
                eErr = CE_Failure;
            else
                eErr = CE_None;
            for( int iBand = 1; iBand <= l_nBands && eErr == CE_None; ++iBand )
            {
                for( int j = 0; j < nYSize && eErr == CE_None; ++j )
                {
                    eErr = poSrcDS->GetRasterBand(iBand)->RasterIO(
                        GF_Read, 0, j, nXSize, 1,
                        pabyScanline, nXSize, 1,
                        GDT_Byte, 0, 0, nullptr );
                    if( poDS->m_bTreatAsSplitBitmap )
                    {
                        for( int i = 0; i < nXSize; ++i )
                        {
                            const GByte byVal = pabyScanline[i];
                            if( (i & 0x7) == 0 )
                                pabyScanline[i >> 3] = 0;
                            if( byVal )
                                pabyScanline[i >> 3] |= 0x80 >> (i & 0x7);
                        }
                    }
                    if( eErr == CE_None &&
                        TIFFWriteScanline(
                            l_hTIFF, pabyScanline, j,
                            static_cast<uint16_t>(iBand - 1)) == -1 )
                    {
                        ReportError( pszFilename, CE_Failure, CPLE_AppDefined,
                                  "TIFFWriteScanline() failed." );
                        eErr = CE_Failure;
                    }
                    if( !GDALScaledProgress(
                           (j + 1 + (iBand - 1) * nYSize) * 1.0 /
                           (l_nBands * nYSize),
                           nullptr, pScaledData ) )
                        eErr = CE_Failure;
                }
            }
            CPLFree(pabyScanline);
        }

        // Necessary to be able to read the file without re-opening.
        TIFFSizeProc pfnSizeProc = TIFFGetSizeProc( l_hTIFF );

        TIFFFlushData( l_hTIFF );

        toff_t nNewDirOffset = pfnSizeProc( TIFFClientdata( l_hTIFF ) );
        if( (nNewDirOffset % 2) == 1 )
            ++nNewDirOffset;

        TIFFFlush( l_hTIFF );

        if( poDS->m_nDirOffset != TIFFCurrentDirOffset( l_hTIFF ) )
        {
            poDS->m_nDirOffset = nNewDirOffset;
            CPLDebug( "GTiff", "directory moved during flush." );
        }
    }
    else if(
#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
        bTryCopy &&
#endif
        eErr == CE_None )
    {
        const char* papszCopyWholeRasterOptions[3] = { nullptr, nullptr, nullptr };
        int iNextOption = 0;
        papszCopyWholeRasterOptions[iNextOption++] =
                "SKIP_HOLES=YES" ;
        if( l_nCompression != COMPRESSION_NONE )
        {
            papszCopyWholeRasterOptions[iNextOption++] =
                "COMPRESSED=YES";
        }
        // For streaming with separate, we really want that bands are written
        // after each other, even if the source is pixel interleaved.
        else if( bStreaming && poDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE )
        {
            papszCopyWholeRasterOptions[iNextOption++] =
                "INTERLEAVE=BAND";
        }

        if( bCopySrcOverviews &&
            (l_nBands == 1 || poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG) )
        {
            poDS->m_bBlockOrderRowMajor = true;
            poDS->m_bLeaderSizeAsUInt4 = true;
            poDS->m_bTrailerRepeatedLast4BytesRepeated = true;
            if( poDS->m_poMaskDS )
            {
                poDS->m_poMaskDS->m_bBlockOrderRowMajor = true;
                poDS->m_poMaskDS->m_bLeaderSizeAsUInt4 = true;
                poDS->m_poMaskDS->m_bTrailerRepeatedLast4BytesRepeated = true;
            }

            if( poDS->m_poMaskDS )
            {
                GDALDestroyScaledProgress(pScaledData);
                pScaledData = GDALCreateScaledProgress(
                                dfCurPixels / dfTotalPixels,
                                1.0,
                                pfnProgress, pProgressData);
            }

            eErr = CopyImageryAndMask(poDS, poSrcDS,
                              poSrcDS->GetRasterBand(1)->GetMaskBand(),
                              GDALScaledProgress, pScaledData);
            if( poDS->m_poMaskDS )
            {
                bWriteMask = false;
            }
        }
        else
        {
            eErr = GDALDatasetCopyWholeRaster(
                /* (GDALDatasetH) */ poSrcDS,
                /* (GDALDatasetH) */ poDS,
                papszCopyWholeRasterOptions,
                GDALScaledProgress, pScaledData );
        }
    }

    GDALDestroyScaledProgress(pScaledData);

    if( eErr == CE_None && !bStreaming && bWriteMask )
    {
        pScaledData = GDALCreateScaledProgress(
            dfNextCurPixels / dfTotalPixels,
            1.0,
            pfnProgress, pProgressData);
        if( poDS->m_poMaskDS )
        {
            const char* l_papszOptions[2] = { "COMPRESSED=YES", nullptr };
            eErr = GDALRasterBandCopyWholeRaster(
                                    poSrcDS->GetRasterBand(1)->GetMaskBand(),
                                    poDS->GetRasterBand(1)->GetMaskBand(),
                                    const_cast<char **>(l_papszOptions),
                                    GDALScaledProgress, pScaledData );
        }
        else
        {
            eErr = GDALDriver::DefaultCopyMasks( poSrcDS, poDS, bStrict,
                                                 nullptr,
                                                 GDALScaledProgress, pScaledData );
        }
        GDALDestroyScaledProgress(pScaledData);
    }

    poDS->m_bWriteCOGLayout = false;

    if( eErr == CE_Failure )
    {
        delete poDS;
        poDS = nullptr;

        if( CPLTestBool(CPLGetConfigOption("GTIFF_DELETE_ON_ERROR", "YES")) )
        {
            if( !bStreaming )
            {
                // Should really delete more carefully.
                VSIUnlink( pszFilename );
            }
        }
    }

    return poDS;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference* GTiffDataset::GetSpatialRef() const

{
    if( m_nGCPCount == 0 )
    {
        const_cast<GTiffDataset*>(this)->LoadGeoreferencingAndPamIfNeeded();
        const_cast<GTiffDataset*>(this)->LookForProjection();

        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    return nullptr;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr GTiffDataset::SetSpatialRef( const OGRSpatialReference * poSRS )

{
    if( m_bStreamingOut && m_bCrystalized )
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify projection at that point in "
            "a streamed output file" );
        return CE_Failure;
    }

    LoadGeoreferencingAndPamIfNeeded();
    LookForProjection();

    if( poSRS == nullptr || poSRS->IsEmpty() )
    {
        if( !m_oSRS.IsEmpty() )
        {
            m_bForceUnsetProjection = true;
        }
        m_oSRS.Clear();
    }
    else
    {
        m_oSRS = *poSRS;
        m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }

    m_bGeoTIFFInfoChanged = true;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::GetGeoTransform( double * padfTransform )

{
    LoadGeoreferencingAndPamIfNeeded();

    memcpy( padfTransform, m_adfGeoTransform, sizeof(double) * 6 );

    if( !m_bGeoTransformValid )
        return CE_Failure;

    // Same logic as in the .gtx driver, for the benefit of GDALOpenVerticalShiftGrid()
    // when used with PROJ-data's US geoids.
    if( CPLFetchBool(papszOpenOptions,
                                "SHIFT_ORIGIN_IN_MINUS_180_PLUS_180", false) )
    {
        if( padfTransform[0] < -180.0 - padfTransform[1] )
            padfTransform[0] += 360.0;
        else if( padfTransform[0] > 180.0 )
            padfTransform[0] -= 360.0;
    }

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::SetGeoTransform( double * padfTransform )

{
    if( m_bStreamingOut && m_bCrystalized )
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify geotransform at that point in a "
            "streamed output file" );
        return CE_Failure;
    }

    LoadGeoreferencingAndPamIfNeeded();

    if( GetAccess() == GA_Update )
    {
        if( m_nGCPCount > 0 )
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                     "GCPs previously set are going to be cleared "
                     "due to the setting of a geotransform.");
            m_bForceUnsetGTOrGCPs = true;
            GDALDeinitGCPs( m_nGCPCount, m_pasGCPList );
            CPLFree( m_pasGCPList );
            m_nGCPCount = 0;
            m_pasGCPList = nullptr;
        }
        else if( padfTransform[0] == 0.0 &&
                 padfTransform[1] == 0.0 &&
                 padfTransform[2] == 0.0 &&
                 padfTransform[3] == 0.0 &&
                 padfTransform[4] == 0.0 &&
                 padfTransform[5] == 0.0 )
        {
            if( m_bGeoTransformValid )
            {
                m_bForceUnsetGTOrGCPs = true;
                m_bGeoTIFFInfoChanged = true;
            }
            m_bGeoTransformValid = false;
            memcpy( m_adfGeoTransform, padfTransform, sizeof(double)*6 );
            return CE_None;
        }

        memcpy( m_adfGeoTransform, padfTransform, sizeof(double)*6 );
        m_bGeoTransformValid = true;
        m_bGeoTIFFInfoChanged = true;

        return CE_None;
    }
    else
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Attempt to call SetGeoTransform() on a read-only GeoTIFF file." );
        return CE_Failure;
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GTiffDataset::GetGCPCount()

{
    LoadGeoreferencingAndPamIfNeeded();

    return m_nGCPCount;
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *GTiffDataset::GetGCPSpatialRef() const

{
    const_cast<GTiffDataset*>(this)->LoadGeoreferencingAndPamIfNeeded();

    if( m_nGCPCount > 0 )
    {
        const_cast<GTiffDataset*>(this)->LookForProjection();
    }
    if( !m_oSRS.IsEmpty() )
        return &m_oSRS;

    return nullptr;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GTiffDataset::GetGCPs()

{
    LoadGeoreferencingAndPamIfNeeded();

    return m_pasGCPList;
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr GTiffDataset::SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                              const OGRSpatialReference *poGCPSRS )
{
    LoadGeoreferencingAndPamIfNeeded();

    if( GetAccess() == GA_Update )
    {
        LookForProjection();

        if( m_nGCPCount > 0 && nGCPCountIn == 0 )
        {
            m_bForceUnsetGTOrGCPs = true;
        }
        else if( nGCPCountIn > 0 &&
                 m_bGeoTransformValid )
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                     "A geotransform previously set is going to be cleared "
                     "due to the setting of GCPs.");
            m_adfGeoTransform[0] = 0.0;
            m_adfGeoTransform[1] = 1.0;
            m_adfGeoTransform[2] = 0.0;
            m_adfGeoTransform[3] = 0.0;
            m_adfGeoTransform[4] = 0.0;
            m_adfGeoTransform[5] = 1.0;
            m_bGeoTransformValid = false;
            m_bForceUnsetGTOrGCPs = true;
        }

        if( poGCPSRS == nullptr || poGCPSRS->IsEmpty() )
        {
            if( !m_oSRS.IsEmpty() )
            {
                m_bForceUnsetProjection = true;
            }
            m_oSRS.Clear();
        }
        else
        {
            m_oSRS = *poGCPSRS;
            m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }

        if( m_nGCPCount > 0 )
        {
            GDALDeinitGCPs( m_nGCPCount, m_pasGCPList );
            CPLFree( m_pasGCPList );
        }

        m_nGCPCount = nGCPCountIn;
        m_pasGCPList = GDALDuplicateGCPs(m_nGCPCount, pasGCPListIn);

        m_bGeoTIFFInfoChanged = true;

        return CE_None;
    }
    else
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                 "SetGCPs() is only supported on newly created GeoTIFF files.");
        return CE_Failure;
    }
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GTiffDataset::GetMetadataDomainList()
{
    LoadGeoreferencingAndPamIfNeeded();

    char **papszDomainList = CSLDuplicate(m_oGTiffMDMD.GetDomainList());
    char **papszBaseList = GDALDataset::GetMetadataDomainList();

    const int nbBaseDomains = CSLCount(papszBaseList);

    for( int domainId = 0; domainId < nbBaseDomains; ++domainId )
    {
        if( CSLFindString(papszDomainList, papszBaseList[domainId]) < 0 )
        {
            papszDomainList = CSLAddString(papszDomainList,papszBaseList[domainId]);
        }
    }

    CSLDestroy(papszBaseList);

    return BuildMetadataDomainList(
        papszDomainList,
        TRUE,
        "", "ProxyOverviewRequest", MD_DOMAIN_RPC, MD_DOMAIN_IMD,
        "SUBDATASETS", "EXIF",
        "xml:XMP", "COLOR_PROFILE", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GTiffDataset::GetMetadata( const char * pszDomain )

{
    if( pszDomain == nullptr || !EQUAL(pszDomain, "IMAGE_STRUCTURE") )
    {
        LoadGeoreferencingAndPamIfNeeded();
    }

    if( pszDomain != nullptr && EQUAL(pszDomain, "ProxyOverviewRequest") )
        return GDALPamDataset::GetMetadata( pszDomain );

    if( pszDomain != nullptr && EQUAL(pszDomain, "DERIVED_SUBDATASETS"))
    {
        return GDALDataset::GetMetadata(pszDomain);
    }

    else if( pszDomain != nullptr && (EQUAL(pszDomain, MD_DOMAIN_RPC) ||
                                   EQUAL(pszDomain, MD_DOMAIN_IMD) ||
                                   EQUAL(pszDomain, MD_DOMAIN_IMAGERY)) )
        LoadMetadata();

    else if( pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS") )
        ScanDirectories();

    else if( pszDomain != nullptr && EQUAL(pszDomain, "EXIF") )
        LoadEXIFMetadata();

    else if( pszDomain != nullptr && EQUAL(pszDomain, "COLOR_PROFILE") )
        LoadICCProfile();

    else if( pszDomain == nullptr || EQUAL(pszDomain, "") )
        LoadMDAreaOrPoint();  // To set GDALMD_AREA_OR_POINT.

    return m_oGTiffMDMD.GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/
CPLErr GTiffDataset::SetMetadata( char ** papszMD, const char *pszDomain )

{
    LoadGeoreferencingAndPamIfNeeded();

    if( m_bStreamingOut && m_bCrystalized )
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify metadata at that point in a streamed output file" );
        return CE_Failure;
    }

    if( pszDomain != nullptr && EQUAL(pszDomain, MD_DOMAIN_RPC) )
    {
        // So that a subsequent GetMetadata() wouldn't override our new values
        LoadMetadata();
        m_bForceUnsetRPC = (CSLCount(papszMD) == 0);
    }

    if( (papszMD != nullptr) &&
        (pszDomain != nullptr) &&
        EQUAL(pszDomain, "COLOR_PROFILE") )
    {
        m_bColorProfileMetadataChanged = true;
    }
    else if( pszDomain == nullptr || !EQUAL(pszDomain,"_temporary_") )
    {
        m_bMetadataChanged = true;
        // Cancel any existing metadata from PAM file.
        if( eAccess == GA_Update &&
            GDALPamDataset::GetMetadata(pszDomain) != nullptr )
            GDALPamDataset::SetMetadata(nullptr, pszDomain);
    }

    if( (pszDomain == nullptr || EQUAL(pszDomain, "")) &&
        CSLFetchNameValue(papszMD, GDALMD_AREA_OR_POINT) != nullptr )
    {
        const char* pszPrevValue =
                GetMetadataItem(GDALMD_AREA_OR_POINT);
        const char* pszNewValue =
                CSLFetchNameValue(papszMD, GDALMD_AREA_OR_POINT);
        if( pszPrevValue == nullptr || pszNewValue == nullptr ||
            !EQUAL(pszPrevValue, pszNewValue) )
        {
            LookForProjection();
            m_bGeoTIFFInfoChanged = true;
        }
    }

    if( pszDomain != nullptr && EQUAL(pszDomain, "xml:XMP") )
    {
        if( papszMD != nullptr && *papszMD != nullptr )
        {
            int nTagSize = static_cast<int>(strlen(*papszMD));
            TIFFSetField( m_hTIFF, TIFFTAG_XMLPACKET, nTagSize, *papszMD );
        }
        else
        {
            TIFFUnsetField( m_hTIFF, TIFFTAG_XMLPACKET );
        }
    }

    return m_oGTiffMDMD.SetMetadata( papszMD, pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GTiffDataset::GetMetadataItem( const char *pszName,
                                           const char *pszDomain )

{
    if( pszDomain == nullptr || !EQUAL(pszDomain, "IMAGE_STRUCTURE") )
    {
        LoadGeoreferencingAndPamIfNeeded();
    }

    if( pszDomain != nullptr && EQUAL(pszDomain,"ProxyOverviewRequest") )
    {
        return GDALPamDataset::GetMetadataItem( pszName, pszDomain );
    }
    else if( pszDomain != nullptr && (EQUAL(pszDomain, MD_DOMAIN_RPC) ||
                                   EQUAL(pszDomain, MD_DOMAIN_IMD) ||
                                   EQUAL(pszDomain, MD_DOMAIN_IMAGERY)) )
    {
        LoadMetadata();
    }
    else if( pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS") )
    {
        ScanDirectories();
    }
    else if( pszDomain != nullptr && EQUAL(pszDomain, "EXIF") )
    {
        LoadEXIFMetadata();
    }
    else if( pszDomain != nullptr && EQUAL(pszDomain, "COLOR_PROFILE") )
    {
        LoadICCProfile();
    }
    else if( (pszDomain == nullptr || EQUAL(pszDomain, "")) &&
        pszName != nullptr && EQUAL(pszName, GDALMD_AREA_OR_POINT) )
    {
        LoadMDAreaOrPoint();  // To set GDALMD_AREA_OR_POINT.
    }
    else if( pszDomain != nullptr && EQUAL(pszDomain, "_DEBUG_") &&
             pszName != nullptr )
    {
#ifdef DEBUG_REACHED_VIRTUAL_MEM_IO
        if( EQUAL(pszName, "UNREACHED_VIRTUALMEMIO_CODE_PATH") )
        {
            CPLString osMissing;
            for( int i = 0; i < static_cast<int>(
                                    CPL_ARRAYSIZE(anReachedVirtualMemIO)); ++i )
            {
                if( !anReachedVirtualMemIO[i] )
                {
                    if( !osMissing.empty() ) osMissing += ",";
                    osMissing += CPLSPrintf("%d", i);
                }
            }
            return (osMissing.size()) ? CPLSPrintf("%s", osMissing.c_str()) : nullptr;
        }
        else
#endif
        if( EQUAL(pszName, "TIFFTAG_EXTRASAMPLES") )
        {
            CPLString osRet;
            uint16_t *v = nullptr;
            uint16_t count = 0;

            if( TIFFGetField( m_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
            {
                for( int i = 0; i < static_cast<int>(count); ++i )
                {
                    if( i > 0 ) osRet += ",";
                    osRet += CPLSPrintf("%d", v[i]);
                }
            }
            return (osRet.size()) ? CPLSPrintf("%s", osRet.c_str()) : nullptr;
        }
        else if( EQUAL(pszName, "TIFFTAG_PHOTOMETRIC") )
        {
            return CPLSPrintf("%d", m_nPhotometric);
        }

        else if( EQUAL( pszName, "TIFFTAG_GDAL_METADATA") )
        {
            char* pszText = nullptr;
            if( !TIFFGetField( m_hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) )
                return nullptr;

            return CPLSPrintf("%s", pszText);
        }
        else if( EQUAL( pszName, "HAS_USED_READ_ENCODED_API") )
        {
            return m_bHasUsedReadEncodedAPI ? "1" : "0";
        }
        return nullptr;
    }

    else if( pszDomain != nullptr && EQUAL(pszDomain, "TIFF") &&
             pszName != nullptr )
    {
        if( EQUAL(pszName, "GDAL_STRUCTURAL_METADATA") )
        {
            const auto nOffset = VSIFTellL(m_fpL);
            VSIFSeekL( m_fpL, 0, SEEK_SET );
            GByte abyData[1024];
            size_t nRead = VSIFReadL(abyData, 1, sizeof(abyData)-1, m_fpL);
            abyData[nRead] = 0;
            VSIFSeekL( m_fpL, nOffset, SEEK_SET );
            if( nRead > 4 )
            {
                const int nOffsetOfStructuralMetadata =
                    (abyData[2] == 0x2B || abyData[3] == 0x2B ) ? 16 : 8;
                const int nSizePatternLen = static_cast<int>(strlen("XXXXXX bytes\n"));
                if( nRead > nOffsetOfStructuralMetadata +
                            strlen("GDAL_STRUCTURAL_METADATA_SIZE=") + nSizePatternLen &&
                    memcmp(abyData + nOffsetOfStructuralMetadata,
                            "GDAL_STRUCTURAL_METADATA_SIZE=",
                            strlen("GDAL_STRUCTURAL_METADATA_SIZE=")) == 0 )
                {
                    char* pszStructuralMD = reinterpret_cast<char*>(
                        abyData + nOffsetOfStructuralMetadata);
                    const int nLenMD = atoi(pszStructuralMD +
                                    strlen("GDAL_STRUCTURAL_METADATA_SIZE="));
                    if( nOffsetOfStructuralMetadata +
                        strlen("GDAL_STRUCTURAL_METADATA_SIZE=") +
                        nSizePatternLen + nLenMD <= nRead )
                    {
                        pszStructuralMD[
                            strlen("GDAL_STRUCTURAL_METADATA_SIZE=") +
                            nSizePatternLen + nLenMD] = 0;
                        return CPLSPrintf("%s", pszStructuralMD);
                    }
                }
            }
            return nullptr;
        }
    }

    return m_oGTiffMDMD.GetMetadataItem( pszName, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GTiffDataset::SetMetadataItem( const char *pszName,
                                      const char *pszValue,
                                      const char *pszDomain )

{
    LoadGeoreferencingAndPamIfNeeded();

    if( m_bStreamingOut && m_bCrystalized )
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify metadata at that point in a streamed output file" );
        return CE_Failure;
    }

    if( (pszDomain != nullptr) && EQUAL(pszDomain, "COLOR_PROFILE") )
    {
        m_bColorProfileMetadataChanged = true;
    }
    else if( pszDomain == nullptr || !EQUAL(pszDomain,"_temporary_") )
    {
        m_bMetadataChanged = true;
        // Cancel any existing metadata from PAM file.
        if( eAccess == GA_Update &&
            GDALPamDataset::GetMetadataItem(pszName, pszDomain) != nullptr )
            GDALPamDataset::SetMetadataItem(pszName, nullptr, pszDomain);
    }

    if( (pszDomain == nullptr || EQUAL(pszDomain, "")) &&
        pszName != nullptr && EQUAL(pszName, GDALMD_AREA_OR_POINT) )
    {
        LookForProjection();
        m_bGeoTIFFInfoChanged = true;
    }

    return m_oGTiffMDMD.SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                         GetInternalHandle()                          */
/************************************************************************/

void *GTiffDataset::GetInternalHandle( const char * /* pszHandleName */ )

{
    return m_hTIFF;
}

/************************************************************************/
/*                         LoadEXIFMetadata()                           */
/************************************************************************/

void GTiffDataset::LoadEXIFMetadata()
{
    if( m_bEXIFMetadataLoaded )
        return;
    m_bEXIFMetadataLoaded = true;

    VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( m_hTIFF ));

    GByte abyHeader[2] = { 0 };
    if( VSIFSeekL(fp, 0, SEEK_SET) != 0 ||
        VSIFReadL(abyHeader, 1, 2, fp) != 2 )
        return;

    const bool bLittleEndian = abyHeader[0] == 'I' && abyHeader[1] == 'I';
    const bool bLeastSignificantBit = CPL_IS_LSB != 0;
    const bool bSwabflag = bLittleEndian != bLeastSignificantBit;  // != is XOR.

    char** papszMetadata = nullptr;
    toff_t nOffset = 0;  // TODO(b/28199387): Refactor to simplify casting.

    if( TIFFGetField(m_hTIFF, TIFFTAG_EXIFIFD, &nOffset) )
    {
        int nExifOffset = static_cast<int>(nOffset);
        int nInterOffset = 0;
        int nGPSOffset = 0;
        EXIFExtractMetadata( papszMetadata,
                             fp, static_cast<int>(nOffset),
                             bSwabflag, 0,
                             nExifOffset, nInterOffset, nGPSOffset);
    }

    if( TIFFGetField(m_hTIFF, TIFFTAG_GPSIFD, &nOffset) )
    {
        int nExifOffset = 0;  // TODO(b/28199387): Refactor to simplify casting.
        int nInterOffset = 0;
        int nGPSOffset = static_cast<int>(nOffset);
        EXIFExtractMetadata( papszMetadata,
                             fp, static_cast<int>(nOffset),
                             bSwabflag, 0,
                             nExifOffset, nInterOffset, nGPSOffset );
    }

    if( papszMetadata )
    {
        m_oGTiffMDMD.SetMetadata( papszMetadata, "EXIF" );
        CSLDestroy( papszMetadata );
    }
}

/************************************************************************/
/*                           LoadMetadata()                             */
/************************************************************************/
void GTiffDataset::LoadMetadata()
{
    if( m_bIMDRPCMetadataLoaded )
        return;
    m_bIMDRPCMetadataLoaded = true;

    GDALMDReaderManager mdreadermanager;
    GDALMDReaderBase* mdreader =
        mdreadermanager.GetReader(m_pszFilename,
                                  oOvManager.GetSiblingFiles(), MDR_ANY);

    if( nullptr != mdreader )
    {
        mdreader->FillMetadata(&m_oGTiffMDMD);

        if(mdreader->GetMetadataDomain(MD_DOMAIN_RPC) == nullptr)
        {
            char** papszRPCMD = GTiffDatasetReadRPCTag(m_hTIFF);
            if( papszRPCMD )
            {
                m_oGTiffMDMD.SetMetadata( papszRPCMD, MD_DOMAIN_RPC );
                CSLDestroy( papszRPCMD );
            }
        }

        m_papszMetadataFiles = mdreader->GetMetadataFiles();
    }
    else
    {
        char** papszRPCMD = GTiffDatasetReadRPCTag(m_hTIFF);
        if( papszRPCMD )
        {
            m_oGTiffMDMD.SetMetadata( papszRPCMD, MD_DOMAIN_RPC );
            CSLDestroy( papszRPCMD );
        }
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **GTiffDataset::GetFileList()

{
    LoadGeoreferencingAndPamIfNeeded();

    char **papszFileList = GDALPamDataset::GetFileList();

    LoadMetadata();
    if(nullptr != m_papszMetadataFiles)
    {
        for( int i = 0; m_papszMetadataFiles[i] != nullptr; ++i )
        {
            if( CSLFindString( papszFileList, m_papszMetadataFiles[i] ) < 0 )
            {
                papszFileList =
                    CSLAddString( papszFileList, m_papszMetadataFiles[i] );
            }
        }
    }

    if( m_pszGeorefFilename &&
        CSLFindString(papszFileList, m_pszGeorefFilename) == -1 )
    {
        papszFileList = CSLAddString( papszFileList, m_pszGeorefFilename );
    }

    return papszFileList;
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr GTiffDataset::CreateMaskBand(int nFlagsIn)
{
    ScanDirectories();

    if( m_poMaskDS != nullptr )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                  "This TIFF dataset has already an internal mask band" );
        return CE_Failure;
    }
    else if( MustCreateInternalMask() )
    {
        if( nFlagsIn != GMF_PER_DATASET )
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "The only flag value supported for internal mask is "
                "GMF_PER_DATASET" );
            return CE_Failure;
        }

        int l_nCompression = COMPRESSION_PACKBITS;
        if( strstr(GDALGetMetadataItem(GDALGetDriverByName( "GTiff" ),
                                       GDAL_DMD_CREATIONOPTIONLIST, nullptr ),
                   "<Value>DEFLATE</Value>") != nullptr )
            l_nCompression = COMPRESSION_ADOBE_DEFLATE;

    /* -------------------------------------------------------------------- */
    /*      If we don't have read access, then create the mask externally.  */
    /* -------------------------------------------------------------------- */
        if( GetAccess() != GA_Update )
        {
            ReportError( CE_Warning, CPLE_AppDefined,
                      "File open for read-only accessing, "
                      "creating mask externally." );

            return GDALPamDataset::CreateMaskBand(nFlagsIn);
        }

        if( m_bLayoutIFDSBeforeData && !m_bKnownIncompatibleEdition &&
            !m_bWriteKnownIncompatibleEdition )
        {
            ReportError(CE_Warning, CPLE_AppDefined,
                        "Adding a mask invalidates the "
                        "LAYOUT=IFDS_BEFORE_DATA property");
            m_bKnownIncompatibleEdition = true;
            m_bWriteKnownIncompatibleEdition = true;
        }

        bool bIsOverview = false;
        uint32_t nSubType = 0;
        if( TIFFGetField(m_hTIFF, TIFFTAG_SUBFILETYPE, &nSubType) )
        {
            bIsOverview = (nSubType & FILETYPE_REDUCEDIMAGE) != 0;

            if( (nSubType & FILETYPE_MASK) != 0 )
            {
                ReportError( CE_Failure, CPLE_AppDefined,
                          "Cannot create a mask on a TIFF mask IFD !" );
                return CE_Failure;
            }
        }

        const int bIsTiled = TIFFIsTiled(m_hTIFF);

        FlushDirectory();

        const toff_t nOffset =
            GTIFFWriteDirectory(
                m_hTIFF,
                bIsOverview ?
                FILETYPE_REDUCEDIMAGE | FILETYPE_MASK : FILETYPE_MASK,
                nRasterXSize, nRasterYSize,
                1, PLANARCONFIG_CONTIG, 1,
                m_nBlockXSize, m_nBlockYSize,
                bIsTiled, l_nCompression,
                PHOTOMETRIC_MASK, PREDICTOR_NONE,
                SAMPLEFORMAT_UINT, nullptr, nullptr, nullptr, 0, nullptr, "", nullptr, nullptr,
                nullptr, nullptr, m_bWriteCOGLayout, nullptr );

        ReloadDirectory();

        if( nOffset == 0 )
            return CE_Failure;

        m_poMaskDS = new GTiffDataset();
        m_poMaskDS->m_poBaseDS = this;
        m_poMaskDS->m_poImageryDS = this;
        m_poMaskDS->ShareLockWithParentDataset(this);
        m_poMaskDS->m_bPromoteTo8Bits =
            CPLTestBool(
                CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
        if( m_poMaskDS->OpenOffset( VSI_TIFFOpenChild(m_hTIFF), nOffset,
                                  GA_Update ) != CE_None)
        {
            delete m_poMaskDS;
            m_poMaskDS = nullptr;
            return CE_Failure;
        }

        return CE_None;
    }

    return GDALPamDataset::CreateMaskBand(nFlagsIn);
}

/************************************************************************/
/*                        MustCreateInternalMask()                      */
/************************************************************************/

bool GTiffDataset::MustCreateInternalMask()
{
    return CPLTestBool(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", "NO"));
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr GTiffRasterBand::CreateMaskBand( int nFlagsIn )
{
    m_poGDS->ScanDirectories();

    if( m_poGDS->m_poMaskDS != nullptr )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                  "This TIFF dataset has already an internal mask band" );
        return CE_Failure;
    }

    if( CPLTestBool( CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", "NO") ) )
    {
        return m_poGDS->CreateMaskBand(nFlagsIn);
    }

    return GDALPamRasterBand::CreateMaskBand(nFlagsIn);
}

/************************************************************************/
/*                        GetRawBinaryLayout()                          */
/************************************************************************/

bool GTiffDataset::GetRawBinaryLayout(GDALDataset::RawBinaryLayout& sLayout)
{
    if( eAccess == GA_Update )
    {
        FlushCache();
        Crystalize();
    }

    if( m_nCompression != COMPRESSION_NONE )
        return false;
    if( !CPLIsPowerOfTwo(m_nBitsPerSample) || m_nBitsPerSample < 8 )
        return false;
    const auto eDT = GetRasterBand(1)->GetRasterDataType();
    if( GDALDataTypeIsComplex( eDT ) )
        return false;

    toff_t *panByteCounts = nullptr;
    toff_t *panOffsets = nullptr;
    const bool bIsTiled = CPL_TO_BOOL( TIFFIsTiled(m_hTIFF) );

    if( !(( bIsTiled
            && TIFFGetField( m_hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts )
            && TIFFGetField( m_hTIFF, TIFFTAG_TILEOFFSETS, &panOffsets ) )
            || ( !bIsTiled
            && TIFFGetField( m_hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts )
            && TIFFGetField( m_hTIFF, TIFFTAG_STRIPOFFSETS, &panOffsets ) )) )
    {
        return false;
    }

    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    vsi_l_offset        nImgOffset = panOffsets[0];
    GIntBig             nPixelOffset = ( m_nPlanarConfig == PLANARCONFIG_CONTIG ) ? static_cast<GIntBig>(nDTSize) * nBands : nDTSize;
    GIntBig             nLineOffset = nPixelOffset * nRasterXSize;
    GIntBig             nBandOffset = ( m_nPlanarConfig == PLANARCONFIG_CONTIG && nBands > 1 ) ? nDTSize : 0;
    RawBinaryLayout::Interleaving eInterleaving =
        (nBands == 1) ?                             RawBinaryLayout::Interleaving::UNKNOWN :
        (m_nPlanarConfig == PLANARCONFIG_CONTIG ) ? RawBinaryLayout::Interleaving::BIP :
                                                    RawBinaryLayout::Interleaving::BSQ;
    if( bIsTiled )
    {
        // Only a single block tiled file with same dimension as the raster
        // might be acceptable
        if( m_nBlockXSize != nRasterXSize || m_nBlockYSize != nRasterYSize )
            return false;
        if( nBands > 1 && m_nPlanarConfig != PLANARCONFIG_CONTIG )
        {
            nBandOffset = static_cast<GIntBig>(panOffsets[1]) - static_cast<GIntBig>(panOffsets[0]);
            for( int i = 2; i < nBands; i++ )
            {
                if( static_cast<GIntBig>(panOffsets[i]) - static_cast<GIntBig>(panOffsets[i - 1]) != nBandOffset )
                    return false;
            }
        }
    }
    else
    {
        const int nStrips = DIV_ROUND_UP(nRasterYSize, m_nRowsPerStrip);
        if( nBands == 1 || m_nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            vsi_l_offset nLastStripEnd = panOffsets[0] + panByteCounts[0];
            for( int iStrip = 1; iStrip < nStrips; iStrip++ )
            {
                if( nLastStripEnd != panOffsets[iStrip] )
                    return false;
                nLastStripEnd = panOffsets[iStrip] + panByteCounts[iStrip];
            }
        }
        else
        {
            // Note: we could potentially have BIL order with m_nRowsPerStrip == 1
            // and if strips are ordered strip_line_1_band_1, ..., strip_line_1_band_N, strip_line2_band1, ... strip_line2_band_N, etc....
            // but that'd be faily exotic !
            // So only detect BSQ layout here
            nBandOffset = static_cast<GIntBig>(panOffsets[nStrips]) - static_cast<GIntBig>(panOffsets[0]);
            for( int i = 0; i < nBands; i++ )
            {
                uint32_t iStripOffset = nStrips * i;
                vsi_l_offset nLastStripEnd = panOffsets[iStripOffset] + panByteCounts[iStripOffset];
                for( int iStrip = 1; iStrip < nStrips; iStrip++ )
                {
                    if( nLastStripEnd != panOffsets[iStripOffset + iStrip] )
                        return false;
                    nLastStripEnd = panOffsets[iStripOffset + iStrip] + panByteCounts[iStripOffset + iStrip];
                }
                if( i >= 2 &&
                     static_cast<GIntBig>(panOffsets[iStripOffset]) -
                        static_cast<GIntBig>(panOffsets[iStripOffset - nStrips]) != nBandOffset )
                {
                    return false;
                }
            }
        }
    }

    sLayout.osRawFilename = m_pszFilename;
    sLayout.eInterleaving = eInterleaving;
    sLayout.eDataType = eDT;
#ifdef CPL_LSB
    sLayout.bLittleEndianOrder = !TIFFIsByteSwapped(m_hTIFF);
#else
    sLayout.bLittleEndianOrder = TIFFIsByteSwapped(m_hTIFF);
#endif
    sLayout.nImageOffset = nImgOffset;
    sLayout.nPixelOffset = nPixelOffset;
    sLayout.nLineOffset = nLineOffset;
    sLayout.nBandOffset = nBandOffset;

    return true;
}

/************************************************************************/
/*                       PrepareTIFFErrorFormat()                       */
/*                                                                      */
/*      sometimes the "module" has stuff in it that has special         */
/*      meaning in a printf() style format, so we try to escape it.     */
/*      For now we hope the only thing we have to escape is %'s.        */
/************************************************************************/

static char *PrepareTIFFErrorFormat( const char *module, const char *fmt )

{
    const size_t nModuleSize = strlen(module);
    const size_t nModFmtSize = nModuleSize * 2 + strlen(fmt) + 2;
    char *pszModFmt = static_cast<char *>( CPLMalloc( nModFmtSize ) );

    size_t iOut = 0;  // Used after for.

    for( size_t iIn = 0; iIn < nModuleSize; ++iIn )
    {
        if( module[iIn] == '%' )
        {
            CPLAssert(iOut < nModFmtSize - 2);
            pszModFmt[iOut++] = '%';
            pszModFmt[iOut++] = '%';
        }
        else
        {
            CPLAssert(iOut < nModFmtSize - 1);
            pszModFmt[iOut++] = module[iIn];
        }
    }
    CPLAssert(iOut < nModFmtSize);
    pszModFmt[iOut] = '\0';
    strcat( pszModFmt, ":" );
    strcat( pszModFmt, fmt );

    return pszModFmt;
}

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
static void
GTiffWarningHandler(const char* module, const char* fmt, va_list ap )
{
    if( strstr(fmt,"nknown field") != nullptr )
        return;

    char *pszModFmt = PrepareTIFFErrorFormat( module, fmt );
    if( strstr(fmt, "does not end in null byte") != nullptr )
    {
        CPLString osMsg;
        osMsg.vPrintf(pszModFmt, ap);
        CPLDebug( "GTiff", "%s", osMsg.c_str() );
    }
    else
    {
        CPLErrorV( CE_Warning, CPLE_AppDefined, pszModFmt, ap );
    }
    CPLFree( pszModFmt );
}

/************************************************************************/
/*                         GTiffErrorHandler()                          */
/************************************************************************/
static void
GTiffErrorHandler( const char* module, const char* fmt, va_list ap )
{
    if( strcmp(fmt, "Maximum TIFF file size exceeded") == 0 )
    {
        // Ideally there would be a thread-safe way of setting this flag,
        // but we cannot really use the extended error handler, since the
        // handler is for all TIFF handles, and not necessarily the ones of
        // this driver.
        if( bGlobalInExternalOvr )
            fmt =
                "Maximum TIFF file size exceeded. "
                "Use --config BIGTIFF_OVERVIEW YES configuration option.";
        else
            fmt =
                "Maximum TIFF file size exceeded. "
                "Use BIGTIFF=YES creation option.";
    }

    char* pszModFmt = PrepareTIFFErrorFormat( module, fmt );
    CPLErrorV( CE_Failure, CPLE_AppDefined, pszModFmt, ap );
    CPLFree( pszModFmt );
}

/************************************************************************/
/*                          GTiffTagExtender()                          */
/*                                                                      */
/*      Install tags specially known to GDAL.                           */
/************************************************************************/

static TIFFExtendProc _ParentExtender = nullptr;

static void GTiffTagExtender(TIFF *tif)

{
    const TIFFFieldInfo xtiffFieldInfo[] = {
        { TIFFTAG_GDAL_METADATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM,
          TRUE, FALSE, const_cast<char *>( "GDALMetadata" ) },
        { TIFFTAG_GDAL_NODATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM,
          TRUE, FALSE, const_cast<char*>( "GDALNoDataValue" ) },
        { TIFFTAG_RPCCOEFFICIENT, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM,
          TRUE, TRUE, const_cast<char *>( "RPCCoefficient" ) },
        { TIFFTAG_TIFF_RSID, -1, -1, TIFF_ASCII, FIELD_CUSTOM,
          TRUE, FALSE, const_cast<char *>( "TIFF_RSID" ) },
        { TIFFTAG_GEO_METADATA, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_BYTE, FIELD_CUSTOM,
          TRUE, TRUE, const_cast<char *>( "GEO_METADATA" ) }
    };

    if( _ParentExtender )
        (*_ParentExtender)(tif);

    TIFFMergeFieldInfo( tif, xtiffFieldInfo,
                        sizeof(xtiffFieldInfo) / sizeof(xtiffFieldInfo[0]) );
}

/************************************************************************/
/*                          GTiffOneTimeInit()                          */
/*                                                                      */
/*      This is stuff that is initialized for the TIFF library just     */
/*      once.  We deliberately defer the initialization till the        */
/*      first time we are likely to call into libtiff to avoid          */
/*      unnecessary paging in of the library for GDAL apps that         */
/*      don't use it.                                                   */
/************************************************************************/
#if defined(HAVE_DLFCN_H) && !defined(WIN32)
#include <dlfcn.h>
#endif

static std::mutex oDeleteMutex;

int GTiffOneTimeInit()

{
    std::lock_guard<std::mutex> oLock(oDeleteMutex);

    static bool bOneTimeInitDone = false;
    if( bOneTimeInitDone )
        return TRUE;

    bOneTimeInitDone = true;

    // This is a frequent configuration error that is difficult to track down
    // for people unaware of the issue : GDAL built against internal libtiff
    // (4.X), but used by an application that links with external libtiff (3.X)
    // Note: on my conf, the order that cause GDAL to crash - and that is
    // detected by the following code - is "-ltiff -lgdal". "-lgdal -ltiff"
    // works for the GTiff driver but probably breaks the application that
    // believes it uses libtiff 3.X but we cannot detect that.
#if !defined(RENAME_INTERNAL_LIBTIFF_SYMBOLS)
#if defined(HAVE_DLFCN_H) && !defined(WIN32)
    const char* (*pfnVersion)(void);
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
    pfnVersion = reinterpret_cast<const char* (*)(void)>(dlsym(RTLD_DEFAULT, "TIFFGetVersion"));
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic pop
#endif
    if( pfnVersion )
    {
        const char* pszVersion = pfnVersion();
        if( pszVersion && strstr(pszVersion, "Version 3.") != nullptr )
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "libtiff version mismatch: You're linking against libtiff 3.X, "
                "but GDAL has been compiled against libtiff >= 4.0.0" );
        }
    }
#endif  // HAVE_DLFCN_H
#endif // !defined(RENAME_INTERNAL_LIBTIFF_SYMBOLS)

    _ParentExtender = TIFFSetTagExtender(GTiffTagExtender);

    TIFFSetWarningHandler( GTiffWarningHandler );
    TIFFSetErrorHandler( GTiffErrorHandler );

    LibgeotiffOneTimeInit();

    return TRUE;
}

/************************************************************************/
/*                        GDALDeregister_GTiff()                        */
/************************************************************************/

static
void GDALDeregister_GTiff( GDALDriver * )

{
}

/************************************************************************/
/*                   GTIFFGetCompressionMethod()                        */
/************************************************************************/

int GTIFFGetCompressionMethod(const char* pszValue, const char* pszVariableName)
{
    int nCompression = COMPRESSION_NONE;
    if( EQUAL( pszValue, "NONE" ) )
        nCompression = COMPRESSION_NONE;
    else if( EQUAL( pszValue, "JPEG" ) )
        nCompression = COMPRESSION_JPEG;
    else if( EQUAL( pszValue, "LZW" ) )
        nCompression = COMPRESSION_LZW;
    else if( EQUAL( pszValue, "PACKBITS" ))
        nCompression = COMPRESSION_PACKBITS;
    else if( EQUAL( pszValue, "DEFLATE" ) || EQUAL( pszValue, "ZIP" ))
        nCompression = COMPRESSION_ADOBE_DEFLATE;
    else if( EQUAL( pszValue, "FAX3" )
             || EQUAL( pszValue, "CCITTFAX3" ))
        nCompression = COMPRESSION_CCITTFAX3;
    else if( EQUAL( pszValue, "FAX4" )
             || EQUAL( pszValue, "CCITTFAX4" ))
        nCompression = COMPRESSION_CCITTFAX4;
    else if( EQUAL( pszValue, "CCITTRLE" ) )
        nCompression = COMPRESSION_CCITTRLE;
    else if( EQUAL( pszValue, "LZMA" ) )
        nCompression = COMPRESSION_LZMA;
    else if( EQUAL( pszValue, "ZSTD" ) )
        nCompression = COMPRESSION_ZSTD;
    else if( EQUAL( pszValue, "LERC" ) ||
             EQUAL( pszValue, "LERC_DEFLATE" ) ||
             EQUAL( pszValue, "LERC_ZSTD" ) )
    {
        nCompression = COMPRESSION_LERC;
    }
    else if( EQUAL( pszValue, "WEBP" ) )
        nCompression = COMPRESSION_WEBP;
    else
        CPLError( CE_Warning, CPLE_IllegalArg,
                  "%s=%s value not recognised, ignoring.",
                  pszVariableName,pszValue );

    if( nCompression != COMPRESSION_NONE &&
        !TIFFIsCODECConfigured(static_cast<uint16_t>(nCompression)) )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Cannot create TIFF file due to missing codec for %s.", pszValue );
        return -1;
    }

    return nCompression;
}

/************************************************************************/
/*                     GTiffGetCompressValues()                         */
/************************************************************************/

CPLString GTiffGetCompressValues(bool& bHasLZW,
                                 bool& bHasDEFLATE,
                                 bool& bHasLZMA,
                                 bool& bHasZSTD,
                                 bool& bHasJPEG,
                                 bool& bHasWebP,
                                 bool& bHasLERC,
                                 bool bForCOG)
{
    bHasLZW = false;
    bHasDEFLATE = false;
    bHasLZMA = false;
    bHasZSTD = false;
    bHasJPEG = false;
    bHasWebP = false;
    bHasLERC = false;

/* -------------------------------------------------------------------- */
/*      Determine which compression codecs are available that we        */
/*      want to advertise.  If we are using an old libtiff we won't     */
/*      be able to find out so we just assume all are available.        */
/* -------------------------------------------------------------------- */
    CPLString osCompressValues = "       <Value>NONE</Value>";

    TIFFCodec *codecs = TIFFGetConfiguredCODECs();

    for( TIFFCodec *c = codecs; c->name; ++c )
    {
        if( c->scheme == COMPRESSION_PACKBITS && !bForCOG )
        {
            osCompressValues +=
                    "       <Value>PACKBITS</Value>";
        }
        else if( c->scheme == COMPRESSION_JPEG )
        {
            bHasJPEG = true;
            osCompressValues +=
                    "       <Value>JPEG</Value>";
        }
        else if( c->scheme == COMPRESSION_LZW )
        {
            bHasLZW = true;
            osCompressValues +=
                    "       <Value>LZW</Value>";
        }
        else if( c->scheme == COMPRESSION_ADOBE_DEFLATE )
        {
            bHasDEFLATE = true;
            osCompressValues +=
                    "       <Value>DEFLATE</Value>";
        }
        else if( c->scheme == COMPRESSION_CCITTRLE && !bForCOG )
        {
            osCompressValues +=
                    "       <Value>CCITTRLE</Value>";
        }
        else if( c->scheme == COMPRESSION_CCITTFAX3 && !bForCOG )
        {
            osCompressValues +=
                    "       <Value>CCITTFAX3</Value>";
        }
        else if( c->scheme == COMPRESSION_CCITTFAX4 && !bForCOG )
        {
            osCompressValues +=
                    "       <Value>CCITTFAX4</Value>";
        }
        else if( c->scheme == COMPRESSION_LZMA && !bForCOG )
        {
            bHasLZMA = true;
            osCompressValues +=
                    "       <Value>LZMA</Value>";
        }
        else if( c->scheme == COMPRESSION_ZSTD )
        {
            bHasZSTD = true;
            osCompressValues +=
                    "       <Value>ZSTD</Value>";
        }
        else if( c->scheme == COMPRESSION_WEBP )
        {
            bHasWebP = true;
            osCompressValues +=
                    "       <Value>WEBP</Value>";
        }
        else if( c->scheme == COMPRESSION_LERC )
        {
            bHasLERC = true;
        }
    }
    if( bHasLERC )
    {
        osCompressValues +=
                        "       <Value>LERC</Value>"
                        "       <Value>LERC_DEFLATE</Value>";
        if( bHasZSTD )
        {
            osCompressValues +=
                        "       <Value>LERC_ZSTD</Value>";
        }
    }
    _TIFFfree( codecs );

    return osCompressValues;
}

/************************************************************************/
/*                          GDALRegister_GTiff()                        */
/************************************************************************/

void GDALRegister_GTiff()

{
    if( GDALGetDriverByName( "GTiff" ) != nullptr )
        return;

    CPLString osOptions;

    bool bHasLZW = false;
    bool bHasDEFLATE = false;
    bool bHasLZMA = false;
    bool bHasZSTD = false;
    bool bHasJPEG = false;
    bool bHasWebP = false;
    bool bHasLERC = false;
    CPLString osCompressValues(GTiffGetCompressValues(
        bHasLZW, bHasDEFLATE, bHasLZMA, bHasZSTD, bHasJPEG, bHasWebP, bHasLERC,
        false /* bForCOG */));

    GDALDriver *poDriver = new GDALDriver();

/* -------------------------------------------------------------------- */
/*      Build full creation option list.                                */
/* -------------------------------------------------------------------- */
    osOptions = "<CreationOptionList>"
              "   <Option name='COMPRESS' type='string-select'>";
    osOptions += osCompressValues;
    osOptions += "   </Option>";
    if( bHasLZW || bHasDEFLATE || bHasZSTD )
        osOptions += ""
"   <Option name='PREDICTOR' type='int' description='Predictor Type (1=default, 2=horizontal differencing, 3=floating point prediction)'/>";
    osOptions += ""
"   <Option name='DISCARD_LSB' type='string' description='Number of least-significant bits to set to clear as a single value or comma-separated list of values for per-band values'/>";
    if( bHasJPEG )
    {
        osOptions += ""
"   <Option name='JPEG_QUALITY' type='int' description='JPEG quality 1-100' default='75'/>"
"   <Option name='JPEGTABLESMODE' type='int' description='Content of JPEGTABLES tag. 0=no JPEGTABLES tag, 1=Quantization tables only, 2=Huffman tables only, 3=Both' default='1'/>";
#ifdef JPEG_DIRECT_COPY
        osOptions += ""
"   <Option name='JPEG_DIRECT_COPY' type='boolean' description='To copy without any decompression/recompression a JPEG source file' default='NO'/>";
#endif
    }
    if( bHasDEFLATE )
    {
#ifdef LIBDEFLATE_SUPPORT
        osOptions += ""
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-12' default='6'/>";
#else
        osOptions += ""
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='6'/>";
#endif
    }
    if( bHasLZMA )
        osOptions += ""
"   <Option name='LZMA_PRESET' type='int' description='LZMA compression level 0(fast)-9(slow)' default='6'/>";
    if( bHasZSTD )
        osOptions += ""
"   <Option name='ZSTD_LEVEL' type='int' description='ZSTD compression level 1(fast)-22(slow)' default='9'/>";
    if( bHasLERC )
        osOptions += ""
"   <Option name='MAX_Z_ERROR' type='float' description='Maximum error for LERC compression' default='0'/>";
    if ( bHasWebP )
    {
      osOptions += ""
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
"   <Option name='WEBP_LOSSLESS' type='boolean' description='Whether lossless compression should be used' default='FALSE'/>"
#endif
"   <Option name='WEBP_LEVEL' type='int' description='WEBP quality level. Low values result in higher compression ratios' default='75'/>";
    }
    osOptions += ""
"   <Option name='NUM_THREADS' type='string' description='Number of worker threads for compression. Can be set to ALL_CPUS' default='1'/>"
"   <Option name='NBITS' type='int' description='BITS for sub-byte files (1-7), sub-uint16_t (9-15), sub-uint32_t (17-31), or float32 (16)'/>"
"   <Option name='INTERLEAVE' type='string-select' default='PIXEL'>"
"       <Value>BAND</Value>"
"       <Value>PIXEL</Value>"
"   </Option>"
"   <Option name='TILED' type='boolean' description='Switch to tiled format'/>"
"   <Option name='TFW' type='boolean' description='Write out world file'/>"
"   <Option name='RPB' type='boolean' description='Write out .RPB (RPC) file'/>"
"   <Option name='RPCTXT' type='boolean' description='Write out _RPC.TXT file'/>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile/Strip Height'/>"
"   <Option name='PHOTOMETRIC' type='string-select'>"
"       <Value>MINISBLACK</Value>"
"       <Value>MINISWHITE</Value>"
"       <Value>PALETTE</Value>"
"       <Value>RGB</Value>"
"       <Value>CMYK</Value>"
"       <Value>YCBCR</Value>"
"       <Value>CIELAB</Value>"
"       <Value>ICCLAB</Value>"
"       <Value>ITULAB</Value>"
"   </Option>"
"   <Option name='SPARSE_OK' type='boolean' description='Should empty blocks be omitted on disk?' default='FALSE'/>"
"   <Option name='ALPHA' type='string-select' description='Mark first extrasample as being alpha'>"
"       <Value>NON-PREMULTIPLIED</Value>"
"       <Value>PREMULTIPLIED</Value>"
"       <Value>UNSPECIFIED</Value>"
"       <Value aliasOf='NON-PREMULTIPLIED'>YES</Value>"
"       <Value aliasOf='UNSPECIFIED'>NO</Value>"
"   </Option>"
"   <Option name='PROFILE' type='string-select' default='GDALGeoTIFF'>"
"       <Value>GDALGeoTIFF</Value>"
"       <Value>GeoTIFF</Value>"
"       <Value>BASELINE</Value>"
"   </Option>"
"   <Option name='PIXELTYPE' type='string-select'>"
"       <Value>DEFAULT</Value>"
"       <Value>SIGNEDBYTE</Value>"
"   </Option>"
"   <Option name='BIGTIFF' type='string-select' description='Force creation of BigTIFF file'>"
"     <Value>YES</Value>"
"     <Value>NO</Value>"
"     <Value>IF_NEEDED</Value>"
"     <Value>IF_SAFER</Value>"
"   </Option>"
"   <Option name='ENDIANNESS' type='string-select' default='NATIVE' description='Force endianness of created file. For DEBUG purpose mostly'>"
"       <Value>NATIVE</Value>"
"       <Value>INVERTED</Value>"
"       <Value>LITTLE</Value>"
"       <Value>BIG</Value>"
"   </Option>"
"   <Option name='COPY_SRC_OVERVIEWS' type='boolean' default='NO' description='Force copy of overviews of source dataset (CreateCopy())'/>"
"   <Option name='SOURCE_ICC_PROFILE' type='string' description='ICC profile'/>"
"   <Option name='SOURCE_PRIMARIES_RED' type='string' description='x,y,1.0 (xyY) red chromaticity'/>"
"   <Option name='SOURCE_PRIMARIES_GREEN' type='string' description='x,y,1.0 (xyY) green chromaticity'/>"
"   <Option name='SOURCE_PRIMARIES_BLUE' type='string' description='x,y,1.0 (xyY) blue chromaticity'/>"
"   <Option name='SOURCE_WHITEPOINT' type='string' description='x,y,1.0 (xyY) whitepoint'/>"
"   <Option name='TIFFTAG_TRANSFERFUNCTION_RED' type='string' description='Transfer function for red'/>"
"   <Option name='TIFFTAG_TRANSFERFUNCTION_GREEN' type='string' description='Transfer function for green'/>"
"   <Option name='TIFFTAG_TRANSFERFUNCTION_BLUE' type='string' description='Transfer function for blue'/>"
"   <Option name='TIFFTAG_TRANSFERRANGE_BLACK' type='string' description='Transfer range for black'/>"
"   <Option name='TIFFTAG_TRANSFERRANGE_WHITE' type='string' description='Transfer range for white'/>"
"   <Option name='STREAMABLE_OUTPUT' type='boolean' default='NO' description='Enforce a mode compatible with a streamable file'/>"
"   <Option name='GEOTIFF_KEYS_FLAVOR' type='string-select' default='STANDARD' description='Which flavor of GeoTIFF keys must be used'>"
"       <Value>STANDARD</Value>"
"       <Value>ESRI_PE</Value>"
"   </Option>"
#if LIBGEOTIFF_VERSION >= 1600
"   <Option name='GEOTIFF_VERSION' type='string-select' default='AUTO' description='Which version of GeoTIFF must be used'>"
"       <Value>AUTO</Value>"
"       <Value>1.0</Value>"
"       <Value>1.1</Value>"
"   </Option>"
#endif
"</CreationOptionList>";

/* -------------------------------------------------------------------- */
/*      Set the driver details.                                         */
/* -------------------------------------------------------------------- */
    poDriver->SetDescription( "GTiff" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GeoTIFF" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/gtiff.html" );
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/tiff" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "tif" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "tif tiff" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 "
                               "Float64 CInt16 CInt32 CFloat32 CFloat64" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, osOptions );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='NUM_THREADS' type='string' description='Number of worker threads for compression. Can be set to ALL_CPUS' default='1'/>"
"   <Option name='GEOTIFF_KEYS_FLAVOR' type='string-select' default='STANDARD' description='Which flavor of GeoTIFF keys must be used (for writing)'>"
"       <Value>STANDARD</Value>"
"       <Value>ESRI_PE</Value>"
"   </Option>"
"   <Option name='GEOREF_SOURCES' type='string' description='Comma separated list made with values INTERNAL/TABFILE/WORLDFILE/PAM/NONE that describe the priority order for georeferencing' default='PAM,INTERNAL,TABFILE,WORLDFILE'/>"
"   <Option name='SPARSE_OK' type='boolean' description='Should empty blocks be omitted on disk?' default='FALSE'/>"
"</OpenOptionList>" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

#ifdef INTERNAL_LIBTIFF
    poDriver->SetMetadataItem( "LIBTIFF", "INTERNAL" );
#else
    poDriver->SetMetadataItem( "LIBTIFF", TIFFLIB_VERSION_STR );
#endif

#define STRINGIFY(x) #x
#define XSTRINGIFY(x) STRINGIFY(x)
    poDriver->SetMetadataItem( "LIBGEOTIFF", XSTRINGIFY(LIBGEOTIFF_VERSION) );

    poDriver->pfnOpen = GTiffDataset::Open;
    poDriver->pfnCreate = GTiffDataset::Create;
    poDriver->pfnCreateCopy = GTiffDataset::CreateCopy;
    poDriver->pfnUnloadDriver = GDALDeregister_GTiff;
    poDriver->pfnIdentify = GTiffDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
