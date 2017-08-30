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

// TODO(schwehr): Move this to cpl_port.h?
#if HAVE_CXX11 && !defined(__MINGW32__)
#define HAVE_CXX11_MUTEX 1
#endif

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
#include <memory>
#if HAVE_CXX11_MUTEX
#  include <mutex>
#endif
#include <set>
#include <string>
#include <vector>

#include "cpl_config.h"
#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
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
#include "gdal_csv.h"
#include "gdal_frmts.h"
#include "gdal_mdreader.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_priv_templates.hpp"
#include "geo_normalize.h"
#include "geotiff.h"
#include "geovalues.h"
#include "gt_jpeg_copy.h"
#include "gt_overview.h"
#include "gt_wkt_srs.h"
#include "gt_wkt_srs_priv.h"
#include "ogr_spatialref.h"
#include "tiff.h"
#include "tif_float.h"
#include "tiffio.h"
#ifdef INTERNAL_LIBTIFF
#  include "tiffiop.h"
#endif
#include "tiffvers.h"
#include "tifvsi.h"
#include "xtiffio.h"


CPL_CVSID("$Id$")

#if SIZEOF_VOIDP == 4
static bool bGlobalStripIntegerOverflow = false;
#endif
static bool bGlobalInExternalOvr = false;

const char* const szJPEGGTiffDatasetTmpPrefix = "/vsimem/gtiffdataset_jpg_tmp_";

typedef enum
{
    GTIFFTAGTYPE_STRING,
    GTIFFTAGTYPE_SHORT,
    GTIFFTAGTYPE_FLOAT
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
};

/************************************************************************/
/*                            IsPowerOfTwo()                            */
/************************************************************************/

static bool IsPowerOfTwo( unsigned int i )
{
    int nBitSet = 0;
    while(i != 0)
    {
        if( i & 1 )
            ++nBitSet;
        i >>= 1;
    }
    return nBitSet == 1;
}

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

void GTIFFGetOverviewBlockSize( int* pnBlockXSize, int* pnBlockYSize )
{
    const char* pszVal = CPLGetConfigOption("GDAL_TIFF_OVR_BLOCKSIZE", "128");
    int nOvrBlockSize = atoi(pszVal);
    if( nOvrBlockSize < 64 || nOvrBlockSize > 4096 ||
        !IsPowerOfTwo(nOvrBlockSize) )
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

typedef enum
{
    VIRTUAL_MEM_IO_NO,
    VIRTUAL_MEM_IO_YES,
    VIRTUAL_MEM_IO_IF_ENOUGH_RAM
} VirtualMemIOEnum;

#if !defined(__MINGW32__)
namespace {
#endif
typedef struct
{
    GTiffDataset *poDS;
    bool          bTIFFIsBigEndian;
    char         *pszTmpFilename;
    int           nHeight;
    uint16        nPredictor;
    GByte        *pabyBuffer;
    int           nBufferSize;
    int           nStripOrTile;

    GByte        *pabyCompressedBuffer;  // Owned by pszTmpFilename.
    int           nCompressedBufferSize;
    bool          bReady;
} GTiffCompressionJob;
#if !defined(__MINGW32__)
}
#endif

class GTiffDataset CPL_FINAL : public GDALPamDataset
{
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

    TIFF       *hTIFF;
    VSILFILE   *fpL;
#if defined(INTERNAL_LIBTIFF) && defined(DEFER_STRILE_LOAD)
    uint32      nStripArrayAlloc;
    vsi_l_offset m_nFileSize; // 0 when unknown, only valid in GA_ReadOnly mode
#endif

    bool        bStreamingIn;

    bool        bStreamingOut;
    CPLString   osTmpFilename;
    VSILFILE*   fpToWrite;
    int         nLastWrittenBlockId;

    GTiffDataset **ppoActiveDSRef;
    GTiffDataset *poActiveDS;  // Only used in actual base.

    bool        bScanDeferred;
    void        ScanDirectories();

    toff_t      nDirOffset;
    bool        bBase;
    // Useful for closing TIFF handle opened by GTIFF_DIR:
    bool        bCloseTIFFHandle;

    uint16      nPlanarConfig;
    uint16      nSamplesPerPixel;
    uint16      nBitsPerSample;
    uint32      nRowsPerStrip;
    uint16      nPhotometric;
    uint16      nSampleFormat;
    uint16      nCompression;

    int         nBlocksPerBand;

    int         nBlockXSize;
    int         nBlockYSize;

    int         nLoadedBlock;  // Or tile.
    bool        bLoadedBlockDirty;
    GByte       *pabyBlockBuf;

    CPLErr      LoadBlockBuf( int nBlockId, bool bReadFromDisk = true );
    CPLErr      FlushBlockBuf();
    bool        bWriteErrorInFlushBlockBuf;

    char        *pszProjection;
    CPLString   m_osVertUnit;
    bool        bLookedForProjection;
    bool        bLookedForMDAreaOrPoint;

    void        LoadMDAreaOrPoint();
    void        LookForProjection();
#ifdef ESRI_BUILD
    void        AdjustLinearUnit( short UOMLength );
#endif

    double      adfGeoTransform[6];
    bool        bGeoTransformValid;

    bool        bTreatAsRGBA;
    bool        bCrystalized;

    void        Crystalize();  // TODO: Spelling.

    GDALColorTable *poColorTable;

    void        WriteGeoTIFFInfo();
    bool        SetDirectory( toff_t nDirOffset = 0 );

    int         nOverviewCount;
    GTiffDataset **papoOverviewDS;

    // If > 0, the implicit JPEG overviews are visible through
    // GetOverviewCount().
    int         nJPEGOverviewVisibilityCounter;
    // Currently visible overviews. Generally == nJPEGOverviewCountOri.
    int         nJPEGOverviewCount;
    int         nJPEGOverviewCountOri;  // Size of papoJPEGOverviewDS.
    GTiffJPEGOverviewDS **papoJPEGOverviewDS;
    int         GetJPEGOverviewCount();

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    bool        IsBlockAvailable( int nBlockId,
                                  vsi_l_offset* pnOffset = NULL,
                                  vsi_l_offset* pnSize = NULL,
                                  bool *pbErrOccured = NULL );

    bool        bGeoTIFFInfoChanged;
    bool        bForceUnsetGTOrGCPs;
    bool        bForceUnsetProjection;

    bool        bNoDataChanged;
    bool        bNoDataSet;
    double      dfNoDataValue;

    bool        bMetadataChanged;
    bool        bColorProfileMetadataChanged;

    bool        bNeedsRewrite;

    void        ApplyPamInfo();
    void        PushMetadataToPam();

    GDALMultiDomainMetadata oGTiffMDMD;

    CPLString   osProfile;
    char      **papszCreationOptions;

    bool        bLoadingOtherBands;

    void*        pabyTempWriteBuffer;
    int          nTempWriteBufferSize;
    bool         WriteEncodedTile( uint32 tile, GByte* pabyData,
                                   int bPreserveDataBuffer );
    bool         WriteEncodedStrip( uint32 strip, GByte* pabyData,
                                    int bPreserveDataBuffer );
    template<class T>
    bool         HasOnlyNoDataT( const T* pBuffer, int nWidth, int nHeight,
                                int nLineStride, int nComponents );
    bool         HasOnlyNoData( const void* pBuffer, int nWidth, int nHeight,
                                int nLineStride, int nComponents );
    inline bool  IsFirstPixelEqualToNoData( const void* pBuffer );

    GTiffDataset* poMaskDS;
    GTiffDataset* poBaseDS;

    CPLString    osFilename;

    bool         bWriteEmptyTiles;
    bool         bFillEmptyTilesAtClosing;
    void         FillEmptyTiles();

    void         FlushDirectory();
    CPLErr       CleanOverviews();

    // Used for the all-in-on-strip case.
    int           nLastLineRead;
    int           nLastBandRead;
    bool          bTreatAsSplit;
    bool          bTreatAsSplitBitmap;

    bool          bClipWarn;

    bool          bIMDRPCMetadataLoaded;
    char**        papszMetadataFiles;
    void          LoadMetadata();

    bool          bEXIFMetadataLoaded;
    void          LoadEXIFMetadata();

    bool          bICCMetadataLoaded;
    void          LoadICCProfile();

    bool          bHasWarnedDisableAggressiveBandCaching;

    bool          bDontReloadFirstBlock;  // Hack for libtiff 3.X and #3633.

    int           nZLevel;
    int           nLZMAPreset;
    int           nJpegQuality;
    int           nJpegTablesMode;

    bool          bPromoteTo8Bits;

    bool          bDebugDontWriteBlocks;

    CPLErr        RegisterNewOverviewDataset( toff_t nOverviewOffset, int l_nJpegQuality );
    CPLErr        CreateOverviewsFromSrcOverviews( GDALDataset* poSrcDS );
    CPLErr        CreateInternalMaskOverviews( int nOvrBlockXSize,
                                               int nOvrBlockYSize );

    bool          bIsFinalized;
    int           Finalize();

    bool          bIgnoreReadErrors;

    CPLString     osGeorefFilename;

    bool          bDirectIO;

    VirtualMemIOEnum eVirtualMemIOUsage;
    CPLVirtualMem* psVirtualMemIOMapping;

    GTIFFKeysFlavorEnum eGeoTIFFKeysFlavor;

    CPLVirtualMem *pBaseMapping;
    int            nRefBaseMapping;

    bool           bHasDiscardedLsb;
    std::vector<int> anMaskLsb;
    std::vector<int> anOffsetLsb;
    void           DiscardLsb(GByte* pabyBuffer, int nBytes, int iBand);
    void           GetDiscardLsbOption( char** papszOptions );

    CPLWorkerThreadPool *poCompressThreadPool;
    std::vector<GTiffCompressionJob> asCompressionJobs;
    CPLMutex      *hCompressThreadPoolMutex;
    void           InitCompressionThreads( char** papszOptions );
    void           InitCreationOrOpenOptions( char** papszOptions );
    static void    ThreadCompressionFunc( void* pData );
    void           WaitCompletionForBlock( int nBlockId );
    void           WriteRawStripOrTile( int nStripOrTile,
                                        GByte* pabyCompressedBuffer,
                                        int nCompressedBufferSize );
    bool           SubmitCompressionJob( int nStripOrTile, GByte* pabyData,
                                         int cc, int nHeight) ;

    int            GuessJPEGQuality( bool& bOutHasQuantizationTable,
                                     bool& bOutHasHuffmanTable );

    void           SetJPEGQualityAndTablesModeFromFile();

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

    GByte          *m_pTempBufferForCommonDirectIO;
    size_t          m_nTempBufferForCommonDirectIOSize;
    template<class FetchBuffer> CPLErr CommonDirectIO(
        FetchBuffer& oFetcher,
        int nXOff, int nYOff, int nXSize, int nYSize,
        void * pData, int nBufXSize, int nBufYSize,
        GDALDataType eBufType,
        int nBandCount, int *panBandMap,
        GSpacing nPixelSpace, GSpacing nLineSpace,
        GSpacing nBandSpace );

    bool        m_bReadGeoTransform;
    bool        m_bLoadPam;
    void        LoadGeoreferencingAndPamIfNeeded();

    bool        m_bHasGotSiblingFiles;
    char      **GetSiblingFiles();

    void        IdentifyAuthorizedGeoreferencingSources();
    bool        m_bHasIdentifiedAuthorizedGeoreferencingSources;
    int         m_nPAMGeorefSrcIndex;
    int         m_nINTERNALGeorefSrcIndex;
    int         m_nTABFILEGeorefSrcIndex;
    int         m_nWORLDFILEGeorefSrcIndex;
    int         m_nGeoTransformGeorefSrcIndex;

    void        FlushCacheInternal( bool bFlushDirectory );

  protected:
    virtual int         CloseDependentDatasets() override;

  public:
             GTiffDataset();
    virtual ~GTiffDataset();

    virtual const char *GetProjectionRef() override;
    virtual CPLErr SetProjection( const char * ) override;
    virtual CPLErr GetGeoTransform( double * ) override;
    virtual CPLErr SetGeoTransform( double * ) override;

    virtual int    GetGCPCount() override;
    virtual const char *GetGCPProjection() override;
    virtual const GDAL_GCP *GetGCPs() override;
    CPLErr         SetGCPs( int, const GDAL_GCP *, const char * ) override;

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

    CPLErr         OpenOffset( TIFF *, GTiffDataset **ppoActiveDSRef,
                               toff_t nDirOffset, bool bBaseIn, GDALAccess,
                               bool bAllowRGBAInterface = true,
                               bool bReadGeoTransform = false );

    static GDALDataset *OpenDir( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
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

    // Only needed by createcopy and close code.
    static void     WriteRPC( GDALDataset *, TIFF *, int, const char *,
                              const char *, char **,
                              bool bWriteOnlyInPAMIfNeeded = false );
    static bool     WriteMetadata( GDALDataset *, TIFF *, bool, const char *,
                                   const char *, char **,
                                   bool bExcludeRPBandIMGFileWriting = false );
    static void     WriteNoDataValue( TIFF *, double );
    static void     UnsetNoDataValue( TIFF * );

    static TIFF *   CreateLL( const char * pszFilename,
                              int nXSize, int nYSize, int nBands,
                              GDALDataType eType,
                              double dfExtraSpaceForOverviews,
                              char **papszParmList,
                              VSILFILE** pfpL,
                              CPLString& osTmpFilename );

    CPLErr   WriteEncodedTileOrStrip( uint32 tile_or_strip, void* data,
                                      int bPreserveDataBuffer );

    static void SaveICCProfile( GTiffDataset *pDS, TIFF *hTIFF,
                                char **papszParmList, uint32 nBitsPerSample );
};

/************************************************************************/
/* ==================================================================== */
/*                        GTiffJPEGOverviewDS                           */
/* ==================================================================== */
/************************************************************************/

class GTiffJPEGOverviewDS CPL_FINAL : public GDALDataset
{
    friend class GTiffJPEGOverviewBand;
    GTiffDataset* poParentDS;
    int nOverviewLevel;

    int        nJPEGTableSize;
    GByte     *pabyJPEGTable;
    CPLString  osTmpFilenameJPEGTable;

    CPLString    osTmpFilename;
    GDALDataset* poJPEGDS;
    // Valid block id of the parent DS that match poJPEGDS.
    int          nBlockId;

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

class GTiffJPEGOverviewBand CPL_FINAL : public GDALRasterBand
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
    poParentDS(poParentDSIn),
    nOverviewLevel(nOverviewLevelIn),
    nJPEGTableSize(nJPEGTableSizeIn),
    pabyJPEGTable(NULL),
    poJPEGDS(NULL),
    nBlockId(-1)
{
    osTmpFilenameJPEGTable.Printf("/vsimem/jpegtable_%p", this);

    const GByte abyAdobeAPP14RGB[] = {
        0xFF, 0xEE, 0x00, 0x0E, 0x41, 0x64, 0x6F, 0x62, 0x65, 0x00,
        0x64, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const bool bAddAdobe =
        poParentDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
        poParentDS->nPhotometric != PHOTOMETRIC_YCBCR &&
        poParentDS->nBands == 3;
    pabyJPEGTable =
        static_cast<GByte*>( CPLMalloc(
            nJPEGTableSize + (bAddAdobe ? sizeof(abyAdobeAPP14RGB) : 0)) );
    memcpy(pabyJPEGTable, pJPEGTable, nJPEGTableSize);
    if( bAddAdobe )
    {
        memcpy( pabyJPEGTable + nJPEGTableSize, abyAdobeAPP14RGB,
                sizeof(abyAdobeAPP14RGB) );
        nJPEGTableSize += sizeof(abyAdobeAPP14RGB);
    }
    CPL_IGNORE_RET_VAL(
        VSIFCloseL(
            VSIFileFromMemBuffer(
                osTmpFilenameJPEGTable, pabyJPEGTable, nJPEGTableSize, TRUE )));

    const int nScaleFactor = 1 << nOverviewLevel;
    nRasterXSize = (poParentDS->nRasterXSize + nScaleFactor - 1) / nScaleFactor;
    nRasterYSize = (poParentDS->nRasterYSize + nScaleFactor - 1) / nScaleFactor;

    for( int i = 1; i <= poParentDS->nBands; ++i )
        SetBand(i, new GTiffJPEGOverviewBand(this, i));

    SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    if( poParentDS->nPhotometric == PHOTOMETRIC_YCBCR )
        SetMetadataItem( "COMPRESSION", "YCbCr JPEG", "IMAGE_STRUCTURE" );
    else
        SetMetadataItem( "COMPRESSION", "JPEG", "IMAGE_STRUCTURE" );
}

/************************************************************************/
/*                       ~GTiffJPEGOverviewDS()                         */
/************************************************************************/

GTiffJPEGOverviewDS::~GTiffJPEGOverviewDS()
{
    if( poJPEGDS != NULL )
        GDALClose( poJPEGDS );
    VSIUnlink(osTmpFilenameJPEGTable);
    if( !osTmpFilename.empty() )
        VSIUnlink(osTmpFilename);
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
    if( nBandCount > 1 && poParentDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
        (poParentDS->nBlockXSize < poParentDS->nRasterXSize ||
         poParentDS->nBlockYSize > 1) )
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
        poDSIn->poParentDS->GetRasterBand(nBandIn)->GetRasterDataType();
    poDSIn->poParentDS->GetRasterBand(nBandIn)->
        GetBlockSize(&nBlockXSize, &nBlockYSize);
    const int nScaleFactor = 1 << poDSIn->nOverviewLevel;
    nBlockXSize = (nBlockXSize + nScaleFactor - 1) / nScaleFactor;
    nBlockYSize = (nBlockYSize + nScaleFactor - 1) / nScaleFactor;
}

/************************************************************************/
/*                          IReadBlock()                                */
/************************************************************************/

CPLErr GTiffJPEGOverviewBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                          void *pImage )
{
    GTiffJPEGOverviewDS* poGDS = static_cast<GTiffJPEGOverviewDS *>(poDS);

    // Compute the source block ID.
    int nBlockId = 0;
    int nParentBlockXSize, nParentBlockYSize;
    poGDS->poParentDS->GetRasterBand(1)->
        GetBlockSize(&nParentBlockXSize, &nParentBlockYSize);
    const bool bIsSingleStripAsSplit = (nParentBlockYSize == 1 &&
                           poGDS->poParentDS->nBlockYSize != nParentBlockYSize);
    if( !bIsSingleStripAsSplit )
    {
        int l_nBlocksPerRow = DIV_ROUND_UP(poGDS->poParentDS->nRasterXSize,
                                               poGDS->poParentDS->nBlockXSize);
        nBlockId = nBlockYOff * l_nBlocksPerRow + nBlockXOff;
    }
    if( poGDS->poParentDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        nBlockId += (nBand-1) * poGDS->poParentDS->nBlocksPerBand;
    }

    if( !poGDS->poParentDS->SetDirectory() )
        return CE_Failure;

    // Make sure it is available.
    const int nDataTypeSize = GDALGetDataTypeSizeBytes(eDataType);
    vsi_l_offset nOffset = 0;
    vsi_l_offset nByteCount = 0;
    bool bErrOccured = false;
    if( !poGDS->poParentDS->IsBlockAvailable(nBlockId, &nOffset, &nByteCount, &bErrOccured) )
    {
        memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize );
        if( bErrOccured )
            return CE_Failure;
        return CE_None;
    }

    const int nScaleFactor = 1 << poGDS->nOverviewLevel;
    if( poGDS->poJPEGDS == NULL || nBlockId != poGDS->nBlockId )
    {
        if( nByteCount < 2 )
            return CE_Failure;
        nOffset += 2;  // Skip leading 0xFF 0xF8.
        nByteCount -= 2;

        // Special case for last strip that might be smaller than other strips
        // In which case we must invalidate the dataset.
        TIFF* hTIFF = poGDS->poParentDS->hTIFF;
        if( !TIFFIsTiled( hTIFF ) && !bIsSingleStripAsSplit &&
            (nBlockYOff + 1 ==
                 DIV_ROUND_UP( poGDS->poParentDS->nRasterYSize,
                               poGDS->poParentDS->nBlockYSize ) ||
             (poGDS->poJPEGDS != NULL &&
              poGDS->poJPEGDS->GetRasterYSize() !=
              nBlockYSize * nScaleFactor)) )
        {
            if( poGDS->poJPEGDS != NULL )
                GDALClose( poGDS->poJPEGDS );
            poGDS->poJPEGDS = NULL;
        }

        CPLString osFileToOpen;
        poGDS->osTmpFilename.Printf("/vsimem/sparse_%p", poGDS);
        VSILFILE* fp = VSIFOpenL(poGDS->osTmpFilename, "wb+");

        // If the size of the JPEG strip/tile is small enough, we will
        // read it from the TIFF file and forge a in-memory JPEG file with
        // the JPEG table followed by the JPEG data.
        const bool bInMemoryJPEGFile = nByteCount < 256 * 256;
        if( bInMemoryJPEGFile )
        {
            // If the previous file was opened as a /vsisparse/, must re-open.
            if( poGDS->poJPEGDS != NULL &&
                STARTS_WITH(poGDS->poJPEGDS->GetDescription(), "/vsisparse/") )
            {
                GDALClose( poGDS->poJPEGDS );
                poGDS->poJPEGDS = NULL;
            }
            osFileToOpen = poGDS->osTmpFilename;

            bool bError = false;
            if( VSIFSeekL(fp, poGDS->nJPEGTableSize + nByteCount - 1, SEEK_SET)
                != 0 )
                bError = true;
            char ch = 0;
            if( !bError && VSIFWriteL(&ch, 1, 1, fp) != 1 )
                bError = true;
            GByte* pabyBuffer =
                VSIGetMemFileBuffer( poGDS->osTmpFilename, NULL, FALSE);
            memcpy(pabyBuffer, poGDS->pabyJPEGTable, poGDS->nJPEGTableSize);
            VSILFILE* fpTIF = VSI_TIFFGetVSILFile(TIFFClientdata( hTIFF ));
            if( !bError && VSIFSeekL(fpTIF, nOffset, SEEK_SET) != 0 )
                bError = true;
            if( VSIFReadL( pabyBuffer + poGDS->nJPEGTableSize,
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
            GDALClose( poGDS->poJPEGDS );
            poGDS->poJPEGDS = NULL;

            osFileToOpen =
                CPLSPrintf("/vsisparse/%s", poGDS->osTmpFilename.c_str());

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
                    poGDS->osTmpFilenameJPEGTable.c_str(),
                    static_cast<int>(poGDS->nJPEGTableSize),
                    poGDS->poParentDS->GetDescription(),
                    static_cast<int>(poGDS->nJPEGTableSize),
                    nOffset,
                    nByteCount) < 0 )
            {
                CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
                return CE_Failure;
            }
        }
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));

        if( poGDS->poJPEGDS == NULL )
        {
            const char* apszDrivers[] = { "JPEG", NULL };

            CPLString osOldVal;
            if( poGDS->poParentDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
                poGDS->nBands == 4 )
            {
                osOldVal =
                    CPLGetThreadLocalConfigOption("GDAL_JPEG_TO_RGB", "");
                CPLSetThreadLocalConfigOption("GDAL_JPEG_TO_RGB", "NO");
            }

            poGDS->poJPEGDS =
                static_cast<GDALDataset *>( GDALOpenEx(
                    osFileToOpen,
                    GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    apszDrivers, NULL, NULL) );

            if( poGDS->poJPEGDS != NULL )
            {
                // Force all implicit overviews to be available, even for
                // small tiles.
                CPLSetThreadLocalConfigOption( "JPEG_FORCE_INTERNAL_OVERVIEWS",
                                               "YES");
                GDALGetOverviewCount(GDALGetRasterBand(poGDS->poJPEGDS, 1));
                CPLSetThreadLocalConfigOption( "JPEG_FORCE_INTERNAL_OVERVIEWS",
                                               NULL);

                poGDS->nBlockId = nBlockId;
            }

            if( poGDS->poParentDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
                poGDS->nBands == 4 )
            {
                CPLSetThreadLocalConfigOption(
                    "GDAL_JPEG_TO_RGB",
                    !osOldVal.empty() ? osOldVal.c_str() : NULL );
            }
        }
        else
        {
            // Trick: we invalidate the JPEG dataset to force a reload
            // of the new content.
            CPLErrorReset();
            poGDS->poJPEGDS->FlushCache();
            if( CPLGetLastErrorNo() != 0 )
            {
                GDALClose( poGDS->poJPEGDS );
                poGDS->poJPEGDS = NULL;
                return CE_Failure;
            }
            poGDS->nBlockId = nBlockId;
        }
    }

    CPLErr eErr = CE_Failure;
    if( poGDS->poJPEGDS )
    {
        GDALDataset* l_poDS = poGDS->poJPEGDS;

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
            if( nBlockXSize == poGDS->GetRasterXSize() )
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
        if( nBlockXOff == DIV_ROUND_UP(poGDS->poParentDS->nRasterXSize,
                                       poGDS->poParentDS->nBlockXSize) - 1 )
        {
            nReqXSize = poGDS->poParentDS->nRasterXSize -
                                nBlockXOff * poGDS->poParentDS->nBlockXSize;
        }
        if( nReqXOff + nReqXSize > l_poDS->GetRasterXSize() )
        {
            nReqXSize = l_poDS->GetRasterXSize() - nReqXOff;
        }
        if( !bIsSingleStripAsSplit &&
            nBlockYOff == DIV_ROUND_UP(poGDS->poParentDS->nRasterYSize,
                                       poGDS->poParentDS->nBlockYSize) - 1 )
        {
            nReqYSize = poGDS->poParentDS->nRasterYSize -
                                nBlockYOff * poGDS->poParentDS->nBlockYSize;
        }
        if( nReqYOff + nReqYSize > l_poDS->GetRasterYSize() )
        {
            nReqYSize = l_poDS->GetRasterYSize() - nReqYOff;
        }
        if( nBlockXOff * nBlockXSize > poGDS->GetRasterXSize() - nBufXSize )
        {
            memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize);
            nBufXSize = poGDS->GetRasterXSize() - nBlockXOff * nBlockXSize;
        }
        if( nBlockYOff * nBlockYSize > poGDS->GetRasterYSize() - nBufYSize )
        {
            memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize);
            nBufYSize = poGDS->GetRasterYSize() - nBlockYOff * nBlockYSize;
        }

        const int nSrcBand =
            poGDS->poParentDS->nPlanarConfig == PLANARCONFIG_SEPARATE ?
            1 : nBand;
        if( nSrcBand <= l_poDS->GetRasterCount() )
        {
            eErr = l_poDS->GetRasterBand(nSrcBand)->RasterIO(GF_Read,
                                 nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                                 pImage,
                                 nBufXSize, nBufYSize, eDataType,
                                 0, nBlockXSize * nDataTypeSize, NULL );
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
    poDS->nJpegQuality = nJpegQuality;

    poDS->ScanDirectories();

    for( int i = 0; i < poDS->nOverviewCount; ++i )
        poDS->papoOverviewDS[i]->nJpegQuality = nJpegQuality;
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
    poDS->nJpegTablesMode = nJpegTablesMode;

    poDS->ScanDirectories();

    for( int i = 0; i < poDS->nOverviewCount; ++i )
        poDS->papoOverviewDS[i]->nJpegTablesMode = nJpegTablesMode;
}

/************************************************************************/
/* ==================================================================== */
/*                            GTiffRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class GTiffRasterBand : public GDALPamRasterBand
{
    friend class GTiffDataset;

    GDALColorInterp    eBandInterp;

    bool               bHaveOffsetScale;
    double             dfOffset;
    double             dfScale;
    CPLString          osUnitType;
    CPLString          osDescription;

    int                DirectIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg );

    std::set<GTiffRasterBand **> aSetPSelf;
    static void     DropReferenceVirtualMem( void* pUserData );
    CPLVirtualMem * GetVirtualMemAutoInternal( GDALRWFlag eRWFlag,
                                               int *pnPixelSpace,
                                               GIntBig *pnLineSpace,
                                               char **papszOptions );

protected:
    GTiffDataset       *poGDS;
    GDALMultiDomainMetadata oGTiffMDMD;

    bool               bNoDataSet;
    double             dfNoDataValue;

    void NullBlock( void *pData );
    CPLErr FillCacheForOtherBands( int nBlockXOff, int nBlockYOff );

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
                              GDALRasterIOExtraArg* psExtraArg ) override CPL_FINAL;

    virtual const char *GetDescription() const override CPL_FINAL;
    virtual void        SetDescription( const char * ) override CPL_FINAL;

    virtual GDALColorInterp GetColorInterpretation() override /*CPL_FINAL*/;
    virtual GDALColorTable *GetColorTable() override /*CPL_FINAL*/;
    virtual CPLErr          SetColorTable( GDALColorTable * ) override CPL_FINAL;
    virtual double          GetNoDataValue( int * ) override CPL_FINAL;
    virtual CPLErr          SetNoDataValue( double ) override CPL_FINAL;
    virtual CPLErr DeleteNoDataValue() override CPL_FINAL;

    virtual double GetOffset( int *pbSuccess = NULL ) override CPL_FINAL;
    virtual CPLErr SetOffset( double dfNewValue ) override CPL_FINAL;
    virtual double GetScale( int *pbSuccess = NULL ) override CPL_FINAL;
    virtual CPLErr SetScale( double dfNewValue ) override CPL_FINAL;
    virtual const char* GetUnitType() override CPL_FINAL;
    virtual CPLErr SetUnitType( const char *pszNewValue ) override CPL_FINAL;
    virtual CPLErr SetColorInterpretation( GDALColorInterp ) override CPL_FINAL;

    virtual char      **GetMetadataDomainList() override CPL_FINAL;
    virtual CPLErr  SetMetadata( char **, const char * = "" ) override CPL_FINAL;
    virtual char  **GetMetadata( const char * pszDomain = "" ) override CPL_FINAL;
    virtual CPLErr  SetMetadataItem( const char*, const char*,
                                     const char* = "" ) override CPL_FINAL;
    virtual const char *GetMetadataItem(
        const char * pszName, const char * pszDomain = "" ) override CPL_FINAL;
    virtual int    GetOverviewCount()  override CPL_FINAL;
    virtual GDALRasterBand *GetOverview( int ) override CPL_FINAL;

    virtual GDALRasterBand *GetMaskBand() override CPL_FINAL;
    virtual int             GetMaskFlags() override CPL_FINAL;
    virtual CPLErr          CreateMaskBand( int nFlags )  override CPL_FINAL;

    virtual CPLVirtualMem  *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                               int *pnPixelSpace,
                                               GIntBig *pnLineSpace,
                                               char **papszOptions )  override CPL_FINAL;

    virtual CPLErr  GetHistogram(
        double dfMin, double dfMax,
        int nBuckets, GUIntBig * panHistogram,
        int bIncludeOutOfRange, int bApproxOK,
        GDALProgressFunc, void *pProgressData )  override CPL_FINAL;

    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets,
                                        GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc,
                                        void *pProgressData)  override CPL_FINAL;
};

/************************************************************************/
/*                           GTiffRasterBand()                          */
/************************************************************************/

GTiffRasterBand::GTiffRasterBand( GTiffDataset *poDSIn, int nBandIn ) :
    eBandInterp(GCI_Undefined),
    bHaveOffsetScale(false),
    dfOffset(0.0),
    dfScale(1.0),
    poGDS(poDSIn),
    bNoDataSet(false),
    dfNoDataValue(-9999.0)
{
    poDS = poDSIn;
    nBand = nBandIn;

/* -------------------------------------------------------------------- */
/*      Get the GDAL data type.                                         */
/* -------------------------------------------------------------------- */
    const uint16 nBitsPerSample = poGDS->nBitsPerSample;
    const uint16 nSampleFormat = poGDS->nSampleFormat;

    eDataType = GDT_Unknown;

    if( nBitsPerSample <= 8 )
    {
        eDataType = GDT_Byte;
        if( nSampleFormat == SAMPLEFORMAT_INT )
            oGTiffMDMD.SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE",
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

    if( poGDS->poColorTable != NULL && nBand == 1 )
    {
        eBandInterp = GCI_PaletteIndex;
    }
    else if( poGDS->nPhotometric == PHOTOMETRIC_RGB
             || (poGDS->nPhotometric == PHOTOMETRIC_YCBCR
                 && poGDS->nCompression == COMPRESSION_JPEG
                 && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                                    "YES") )) )
    {
        if( nBand == 1 )
            eBandInterp = GCI_RedBand;
        else if( nBand == 2 )
            eBandInterp = GCI_GreenBand;
        else if( nBand == 3 )
            eBandInterp = GCI_BlueBand;
        else
            bLookForExtraSamples = true;
    }
    else if( poGDS->nPhotometric == PHOTOMETRIC_YCBCR )
    {
        if( nBand == 1 )
            eBandInterp = GCI_YCbCr_YBand;
        else if( nBand == 2 )
            eBandInterp = GCI_YCbCr_CbBand;
        else if( nBand == 3 )
            eBandInterp = GCI_YCbCr_CrBand;
        else
            bLookForExtraSamples = true;
    }
    else if( poGDS->nPhotometric == PHOTOMETRIC_SEPARATED )
    {
        if( nBand == 1 )
            eBandInterp = GCI_CyanBand;
        else if( nBand == 2 )
            eBandInterp = GCI_MagentaBand;
        else if( nBand == 3 )
            eBandInterp = GCI_YellowBand;
        else if( nBand == 4 )
            eBandInterp = GCI_BlackBand;
        else
            bLookForExtraSamples = true;
    }
    else if( poGDS->nPhotometric == PHOTOMETRIC_MINISBLACK && nBand == 1 )
    {
        eBandInterp = GCI_GrayIndex;
    }
    else
    {
        bLookForExtraSamples = true;
    }

    if( bLookForExtraSamples )
    {
        uint16 *v = NULL;
        uint16 count = 0;

        if( TIFFGetField( poGDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            const int nBaseSamples = poGDS->nSamplesPerPixel - count;
            const int nExpectedBaseSamples =
                (poGDS->nPhotometric == PHOTOMETRIC_MINISBLACK) ? 1 :
                (poGDS->nPhotometric == PHOTOMETRIC_MINISWHITE) ? 1 :
                (poGDS->nPhotometric == PHOTOMETRIC_RGB) ? 3 :
                (poGDS->nPhotometric == PHOTOMETRIC_YCBCR) ? 3 :
                (poGDS->nPhotometric == PHOTOMETRIC_SEPARATED) ? 4 : 0;

            if( nExpectedBaseSamples > 0 &&
                nBand == nExpectedBaseSamples + 1 &&
                nBaseSamples != nExpectedBaseSamples )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Wrong number of ExtraSamples : %d. %d were expected",
                         count, poGDS->nSamplesPerPixel - nExpectedBaseSamples);
            }

            if( nBand > nBaseSamples
                && nBand-nBaseSamples-1 < count
                && (v[nBand-nBaseSamples-1] == EXTRASAMPLE_ASSOCALPHA
                    || v[nBand-nBaseSamples-1] == EXTRASAMPLE_UNASSALPHA) )
                eBandInterp = GCI_AlphaBand;
            else
                eBandInterp = GCI_Undefined;
        }
        else
        {
            eBandInterp = GCI_Undefined;
        }
    }

/* -------------------------------------------------------------------- */
/*      Establish block size for strip or tiles.                        */
/* -------------------------------------------------------------------- */
    nBlockXSize = poGDS->nBlockXSize;
    nBlockYSize = poGDS->nBlockYSize;
}

/************************************************************************/
/*                          ~GTiffRasterBand()                          */
/************************************************************************/

GTiffRasterBand::~GTiffRasterBand()
{
    // So that any future DropReferenceVirtualMem() will not try to access the
    // raster band object, but this would not conform to the advertised
    // contract.
    if( !aSetPSelf.empty() )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Virtual memory objects still exist at GTiffRasterBand "
                  "destruction" );
        std::set<GTiffRasterBand**>::iterator oIter = aSetPSelf.begin();
        for( ; oIter != aSetPSelf.end(); ++oIter )
            *(*oIter) = NULL;
    }
}

/************************************************************************/
/*                        FetchBufferDirectIO                           */
/************************************************************************/

class FetchBufferDirectIO CPL_FINAL
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
            return NULL;
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
          poGDS->nCompression == COMPRESSION_NONE &&
          (poGDS->nPhotometric == PHOTOMETRIC_MINISBLACK ||
           poGDS->nPhotometric == PHOTOMETRIC_RGB ||
           poGDS->nPhotometric == PHOTOMETRIC_PALETTE) &&
          poGDS->nBitsPerSample == nDTSizeBits &&
          poGDS->SetDirectory() /* very important to make hTIFF uptodate! */) )
    {
        return -1;
    }

    // Only know how to deal with nearest neighbour in this optimized routine.
    if( (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg != NULL &&
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
    if( poGDS->GetAccess() == GA_Update )
    {
        poGDS->FlushCache();
        VSI_TIFFFlushBufferedWrite( TIFFClientdata( poGDS->hTIFF ) );
    }

    if( TIFFIsTiled( poGDS->hTIFF ) )
    {
        if( poGDS->m_pTempBufferForCommonDirectIO == NULL )
        {
            const int nDTSize = nDTSizeBits / 8;
            poGDS->m_nTempBufferForCommonDirectIOSize =
                static_cast<size_t>(
                    nBlockXSize * nBlockYSize * nDTSize *
                    (poGDS->nPlanarConfig == PLANARCONFIG_CONTIG ?
                     poGDS->nBands : 1) );

            poGDS->m_pTempBufferForCommonDirectIO =
                static_cast<GByte *>( VSI_MALLOC_VERBOSE(
                    poGDS->m_nTempBufferForCommonDirectIOSize ) );
            if( poGDS->m_pTempBufferForCommonDirectIO == NULL )
                return CE_Failure;
        }

        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( poGDS->hTIFF ));
        FetchBufferDirectIO oFetcher(fp, poGDS->m_pTempBufferForCommonDirectIO,
                                     poGDS->m_nTempBufferForCommonDirectIOSize);

        return poGDS->CommonDirectIO(
            oFetcher,
            nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize,
            eBufType,
            1, &nBand,
            nPixelSpace, nLineSpace,
            0 );
    }

    // Get strip offsets.
    toff_t *panTIFFOffsets = NULL;
    if( !TIFFGetField( poGDS->hTIFF, TIFFTAG_STRIPOFFSETS, &panTIFFOffsets ) ||
        panTIFFOffsets == NULL )
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
    void* pTmpBuffer = NULL;
    int eErr = CE_None;
    int nContigBands =
        poGDS->nPlanarConfig == PLANARCONFIG_CONTIG ? poGDS->nBands : 1;
    int nSrcPixelSize = nDTSize * nContigBands;

    if( ppData == NULL || panOffsets == NULL || panSizes == NULL )
        eErr = CE_Failure;
    else if( nXSize != nBufXSize || nYSize != nBufYSize ||
             eBufType != eDataType ||
             nPixelSpace != GDALGetDataTypeSizeBytes(eBufType) ||
             nContigBands > 1 )
    {
        // We need a temporary buffer for over-sampling/sub-sampling
        // and/or data type conversion.
        pTmpBuffer = VSI_MALLOC_VERBOSE(nReqXSize * nReqYSize * nSrcPixelSize);
        if( pTmpBuffer == NULL )
            eErr = CE_Failure;
    }

    // Prepare data extraction.
    const double dfSrcYInc = nYSize / static_cast<double>( nBufYSize );

    for( int iLine = 0; eErr == CE_None && iLine < nReqYSize; ++iLine )
    {
        if( pTmpBuffer == NULL )
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
        if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        {
            nBlockId += (nBand-1) * poGDS->nBlocksPerBand;
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
        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( poGDS->hTIFF ));
        const int nRet =
            VSIFReadMultiRangeL( nReqYSize, ppData, panOffsets, panSizes, fp );
        if( nRet != 0 )
            eErr = CE_Failure;
    }

    // Byte-swap if necessary.
    if( eErr == CE_None && TIFFIsByteSwapped(poGDS->hTIFF) )
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
    if( eErr == CE_None && pTmpBuffer != NULL )
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
    if( psRet != NULL )
    {
        CPLDebug("GTiff", "GetVirtualMemAuto(): Using memory file mapping");
        return psRet;
    }

    if( EQUAL(pszImpl, "NO") || EQUAL(pszImpl, "OFF") ||
        EQUAL(pszImpl, "0") || EQUAL(pszImpl, "FALSE") )
    {
        return NULL;
    }

    CPLDebug("GTiff", "GetVirtualMemAuto(): Defaulting to base implementation");
    return GDALRasterBand::GetVirtualMemAuto( eRWFlag, pnPixelSpace,
                                              pnLineSpace, papszOptions );
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
    poGDS->LoadGeoreferencingAndPamIfNeeded();
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
    poGDS->LoadGeoreferencingAndPamIfNeeded();
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

    if( poSelf != NULL )
    {
        if( --(poSelf->poGDS->nRefBaseMapping) == 0 )
        {
            poSelf->poGDS->pBaseMapping = NULL;
        }
        poSelf->aSetPSelf.erase(ppoSelf);
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
    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        nLineSize *= poGDS->nBands;

    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        // In case of a pixel interleaved file, we save virtual memory space
        // by reusing a base mapping that embraces the whole imagery.
        if( poGDS->pBaseMapping != NULL )
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
                poGDS->pBaseMapping,
                nOffset,
                CPLVirtualMemGetSize(poGDS->pBaseMapping) - nOffset,
                GTiffRasterBand::DropReferenceVirtualMem,
                ppoSelf);
            if( pVMem == NULL )
            {
                CPLFree(ppoSelf);
                return NULL;
            }

            // Mechanism used so that the memory mapping object can be
            // destroyed after the raster band.
            aSetPSelf.insert(ppoSelf);
            ++poGDS->nRefBaseMapping;
            *pnPixelSpace = GDALGetDataTypeSizeBytes(eDataType);
            if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
                *pnPixelSpace *= poGDS->nBands;
            *pnLineSpace = nLineSize;
            return pVMem;
        }
    }

    if( !poGDS->SetDirectory() )  // Very important to make hTIFF up-to-date.
        return NULL;
    VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( poGDS->hTIFF ));

    vsi_l_offset nLength = static_cast<vsi_l_offset>(nRasterYSize) * nLineSize;

    if( !(CPLIsVirtualMemFileMapAvailable() &&
          VSIFGetNativeFileDescriptorL(fp) != NULL &&
#if SIZEOF_VOIDP == 4
          nLength == static_cast<size_t>(nLength) &&
#endif
          poGDS->nCompression == COMPRESSION_NONE &&
          (poGDS->nPhotometric == PHOTOMETRIC_MINISBLACK ||
           poGDS->nPhotometric == PHOTOMETRIC_RGB ||
           poGDS->nPhotometric == PHOTOMETRIC_PALETTE) &&
          poGDS->nBitsPerSample == GDALGetDataTypeSizeBits(eDataType) &&
          !TIFFIsTiled( poGDS->hTIFF ) && !TIFFIsByteSwapped(poGDS->hTIFF)) )
    {
        return NULL;
    }

    // Make sure that TIFFTAG_STRIPOFFSETS is up-to-date.
    if( poGDS->GetAccess() == GA_Update )
    {
        poGDS->FlushCache();
        VSI_TIFFFlushBufferedWrite( TIFFClientdata( poGDS->hTIFF ) );
    }

    // Get strip offsets.
    toff_t *panTIFFOffsets = NULL;
    if( !TIFFGetField( poGDS->hTIFF, TIFFTAG_STRIPOFFSETS, &panTIFFOffsets ) ||
        panTIFFOffsets == NULL )
    {
        return NULL;
    }

    int nBlockSize =
        nBlockXSize * nBlockYSize * GDALGetDataTypeSizeBytes(eDataType);
    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        nBlockSize *= poGDS->nBands;

    int nBlocks = poGDS->nBlocksPerBand;
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlocks *= poGDS->nBands;
    int i = 0;  // Used after for.
    for( ; i < nBlocks; ++i )
    {
        if( panTIFFOffsets[i] != 0 )
            break;
    }
    if( i == nBlocks )
    {
        // All zeroes.
        if( poGDS->eAccess == GA_Update )
        {
            // Initialize the file with empty blocks so that the file has
            // the appropriate size.

            toff_t* panByteCounts = NULL;
            if( !TIFFGetField( poGDS->hTIFF, TIFFTAG_STRIPBYTECOUNTS,
                               &panByteCounts ) ||
                panByteCounts == NULL )
            {
                return NULL;
            }
            if( VSIFSeekL(fp, 0, SEEK_END) != 0 )
                return NULL;
            vsi_l_offset nBaseOffset = VSIFTellL(fp);

            // Just write one tile with libtiff to put it in appropriate state.
            GByte* pabyData =
                static_cast<GByte*>(VSI_CALLOC_VERBOSE(1, nBlockSize));
            if( pabyData == NULL )
            {
                return NULL;
            }
            int ret =
                static_cast<int>(
                    TIFFWriteEncodedStrip( poGDS->hTIFF, 0, pabyData,
                                           nBlockSize ) );
            VSI_TIFFFlushBufferedWrite( TIFFClientdata( poGDS->hTIFF ) );
            VSIFree(pabyData);
            if( ret != nBlockSize )
            {
                return NULL;
            }
            CPLAssert(panTIFFOffsets[0] == nBaseOffset);
            CPLAssert(panByteCounts[0] == static_cast<toff_t>(nBlockSize));

            // Now simulate the writing of other blocks.
            const vsi_l_offset nDataSize =
                static_cast<vsi_l_offset>(nBlockSize) * nBlocks;
            if( VSIFTruncateL(fp, nBaseOffset + nDataSize) != 0 )
                return NULL;

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
            return NULL;
        }
    }

    GIntBig nBlockSpacing = 0;
    bool bCompatibleSpacing = true;
    toff_t nPrevOffset = 0;
    for( i = 0; i < poGDS->nBlocksPerBand; ++i )
    {
        toff_t nCurOffset = 0;
        if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
            nCurOffset =
                panTIFFOffsets[poGDS->nBlocksPerBand * (nBand - 1) + i];
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
        return NULL;
    }

    vsi_l_offset nOffset = 0;
    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        CPLAssert( poGDS->pBaseMapping == NULL );
        nOffset = panTIFFOffsets[0];
    }
    else
    {
        nOffset = panTIFFOffsets[poGDS->nBlocksPerBand * (nBand - 1)];
    }
    CPLVirtualMem* pVMem = CPLVirtualMemFileMapNew(
        fp, nOffset, nLength,
        eRWFlag == GF_Write ? VIRTUALMEM_READWRITE : VIRTUALMEM_READONLY,
        NULL, NULL);
    if( pVMem == NULL )
    {
        return NULL;
    }

    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        // TODO(schwehr): Revisit this block.
        poGDS->pBaseMapping = pVMem;
        pVMem = GetVirtualMemAutoInternal( eRWFlag,
                                           pnPixelSpace,
                                           pnLineSpace,
                                           papszOptions );
        // Drop ref on base mapping.
        CPLVirtualMemFree(poGDS->pBaseMapping);
        if( pVMem == NULL )
            poGDS->pBaseMapping = NULL;
    }
    else
    {
        *pnPixelSpace = GDALGetDataTypeSizeBytes(eDataType);
        if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
            *pnPixelSpace *= poGDS->nBands;
        *pnLineSpace = nLineSize;
    }
    return pVMem;
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
        ++nJPEGOverviewVisibilityCounter;
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
        --nJPEGOverviewVisibilityCounter;
        if( bTried )
            return eErr;
    }

    if( eVirtualMemIOUsage != VIRTUAL_MEM_IO_NO )
    {
        const int nErr = VirtualMemIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg );
        if( nErr >= 0 )
            return static_cast<CPLErr>(nErr);
    }
    if( bDirectIO )
    {
        const int nErr = DirectIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg );
        if( nErr >= 0 )
            return static_cast<CPLErr>(nErr);
    }

    ++nJPEGOverviewVisibilityCounter;
    const CPLErr eErr =
        GDALPamDataset::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg);
    nJPEGOverviewVisibilityCounter--;
    return eErr;
}

/************************************************************************/
/*                        FetchBufferVirtualMemIO                       */
/************************************************************************/

class FetchBufferVirtualMemIO CPL_FINAL
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
            return NULL;
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
    if( eAccess == GA_Update || eRWFlag == GF_Write || bStreamingIn )
        return -1;

    // Only know how to deal with nearest neighbour in this optimized routine.
    if( (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg != NULL &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
    {
        return -1;
    }

    if( !SetDirectory() )
        return CE_Failure;

    const GDALDataType eDataType = GetRasterBand(1)->GetRasterDataType();
    const int nDTSizeBits = GDALGetDataTypeSizeBits(eDataType);
    if( !(nCompression == COMPRESSION_NONE &&
        (nPhotometric == PHOTOMETRIC_MINISBLACK ||
        nPhotometric == PHOTOMETRIC_RGB ||
        nPhotometric == PHOTOMETRIC_PALETTE) &&
        nBitsPerSample == nDTSizeBits) )
    {
        eVirtualMemIOUsage = VIRTUAL_MEM_IO_NO;
        return -1;
    }

    size_t nMappingSize = 0;
    GByte* pabySrcData = NULL;
    if( STARTS_WITH(GetDescription(), "/vsimem/") )
    {
        vsi_l_offset nDataLength = 0;
        pabySrcData =
            VSIGetMemFileBuffer(GetDescription(), &nDataLength, FALSE);
        nMappingSize = static_cast<size_t>(nDataLength);
        if( pabySrcData == NULL )
            return -1;
    }
    else if( psVirtualMemIOMapping == NULL )
    {
        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( hTIFF ));
        if( !CPLIsVirtualMemFileMapAvailable() ||
            VSIFGetNativeFileDescriptorL(fp) == NULL )
        {
            eVirtualMemIOUsage = VIRTUAL_MEM_IO_NO;
            return -1;
        }
        if( VSIFSeekL(fp, 0, SEEK_END) != 0 )
        {
            eVirtualMemIOUsage = VIRTUAL_MEM_IO_NO;
            return -1;
        }
        const vsi_l_offset nLength = VSIFTellL(fp);
        if( static_cast<size_t>(nLength) != nLength )
        {
            eVirtualMemIOUsage = VIRTUAL_MEM_IO_NO;
            return -1;
        }
        if( eVirtualMemIOUsage == VIRTUAL_MEM_IO_IF_ENOUGH_RAM )
        {
            GIntBig nRAM = CPLGetUsablePhysicalRAM();
            if( static_cast<GIntBig>(nLength) > nRAM )
            {
                CPLDebug( "GTiff",
                          "Not enough RAM to map whole file into memory." );
                eVirtualMemIOUsage = VIRTUAL_MEM_IO_NO;
                return -1;
            }
        }
        psVirtualMemIOMapping = CPLVirtualMemFileMapNew(
            fp, 0, nLength, VIRTUALMEM_READONLY, NULL, NULL);
        if( psVirtualMemIOMapping == NULL )
        {
            eVirtualMemIOUsage = VIRTUAL_MEM_IO_NO;
            return -1;
        }
        eVirtualMemIOUsage = VIRTUAL_MEM_IO_YES;
    }

    if( psVirtualMemIOMapping )
    {
#ifdef DEBUG
        CPLDebug("GTiff", "Using VirtualMemIO");
#endif
        nMappingSize = CPLVirtualMemGetSize(psVirtualMemIOMapping);
        pabySrcData = static_cast<GByte *>(
            CPLVirtualMemGetAddr(psVirtualMemIOMapping) );
    }

    if( TIFFIsByteSwapped(hTIFF) && m_pTempBufferForCommonDirectIO == NULL )
    {
        const int nDTSize = nDTSizeBits / 8;
        m_nTempBufferForCommonDirectIOSize =
            static_cast<size_t>(nBlockXSize * nDTSize *
                (nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1));
        if( TIFFIsTiled(hTIFF) )
            m_nTempBufferForCommonDirectIOSize *= nBlockYSize;

        m_pTempBufferForCommonDirectIO =
            static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(m_nTempBufferForCommonDirectIOSize) );
        if( m_pTempBufferForCommonDirectIO == NULL )
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
    toff_t *panOffsets = NULL;
    if( !TIFFGetField( hTIFF, (TIFFIsTiled( hTIFF )) ?
                       TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS,
                       &panOffsets ) ||
        panOffsets == NULL )
    {
        return CE_Failure;
    }

    bool bUseContigImplementation =
        nPlanarConfig == PLANARCONFIG_CONTIG &&
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
        nPlanarConfig == PLANARCONFIG_SEPARATE ? 1 : nBands;
    const int nBandsPerBlockDTSize = nBandsPerBlock * nDTSize;
    const int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    const bool bNoTypeChange = (eDataType == eBufType);
    const bool bNoXResampling = (nXSize == nBufXSize );
    const bool bNoXResamplingNoTypeChange = (bNoTypeChange && bNoXResampling);
    const bool bByteOnly = (bNoTypeChange && nDTSize == 1 );
    const bool bByteNoXResampling = ( bByteOnly && bNoXResamplingNoTypeChange );
    const bool bIsByteSwapped = CPL_TO_BOOL(TIFFIsByteSwapped(hTIFF));
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
             TIFFIsTiled( hTIFF ) && bNoXResampling && (nYSize == nBufYSize ) &&
             nPlanarConfig == PLANARCONFIG_CONTIG && nBandCount > 1 )
    {
        GByte* pabyData = static_cast<GByte *>(pData);
        for( int y = 0; y < nBufYSize; )
        {
            const int nSrcLine = nYOff + y;
            const int nBlockYOff = nSrcLine / nBlockYSize;
            const int nYOffsetInBlock = nSrcLine % nBlockYSize;
            const int nUsedBlockHeight =
                std::min( nBufYSize - y,
                          nBlockYSize - nYOffsetInBlock );

            int nBlockXOff = nXOff / nBlockXSize;
            int nXOffsetInBlock = nXOff % nBlockXSize;
            int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

            int x = 0;
            while( x < nBufXSize )
            {
                const toff_t nCurOffset = panOffsets[nBlockId];
                const int nUsedBlockWidth =
                    std::min( nBlockXSize - nXOffsetInBlock,
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
                        nYOffsetInBlock * nBlockXSize * nBandsPerBlockDTSize;
                    const GByte* pabyLocalSrcDataK0 = oFetcher.FetchBytes(
                            nCurOffset + nByteOffsetInBlock,
                            nBlockXSize *
                            nUsedBlockHeight * nBandsPerBlock,
                            nDTSize, bIsByteSwapped, bIsComplex, nBlockId);
                    if( pabyLocalSrcDataK0 == NULL )
                        return CE_Failure;

                    for( int k = 0; k < nUsedBlockHeight; ++k )
                    {
                        GByte* pabyLocalData =
                            pabyData + (y + k) * nLineSpace + x * nPixelSpace;
                        const GByte* pabyLocalSrcData =
                            pabyLocalSrcDataK0 +
                            (k * nBlockXSize + nXOffsetInBlock) *
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
             TIFFIsTiled( hTIFF ) && bNoXResampling &&
             (nYSize == nBufYSize ) )
             // && (nPlanarConfig == PLANARCONFIG_SEPARATE || nBandCount == 1) )
    {
        for( int iBand = 0; iBand < nBandCount; ++iBand )
        {
            GByte* pabyData = static_cast<GByte *>(pData) + iBand * nBandSpace;
            const int nBand = panBandMap[iBand];
            for( int y = 0; y < nBufYSize; )
            {
                const int nSrcLine = nYOff + y;
                const int nBlockYOff = nSrcLine / nBlockYSize;
                const int nYOffsetInBlock = nSrcLine % nBlockYSize;
                const int nUsedBlockHeight =
                    std::min( nBufYSize - y,
                              nBlockYSize - nYOffsetInBlock);

                int nBlockXOff = nXOff / nBlockXSize;
                int nXOffsetInBlock = nXOff % nBlockXSize;
                int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
                if( nPlanarConfig == PLANARCONFIG_SEPARATE )
                {
                    REACHED(33);
                    nBlockId += nBlocksPerBand * (nBand - 1);
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
                            nBlockXSize - nXOffsetInBlock,
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
                        const int nByteOffsetInBlock =
                            nYOffsetInBlock * nBlockXSize *
                            nBandsPerBlockDTSize;
                        const GByte* pabyLocalSrcDataK0 =
                            oFetcher.FetchBytes(
                                nCurOffset + nByteOffsetInBlock,
                                nBlockXSize *
                                nUsedBlockHeight * nBandsPerBlock,
                                nDTSize, bIsByteSwapped, bIsComplex, nBlockId);
                        if( pabyLocalSrcDataK0 == NULL )
                            return CE_Failure;

                        if( nPlanarConfig == PLANARCONFIG_CONTIG )
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
                                (k * nBlockXSize + nXOffsetInBlock) *
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
             TIFFIsTiled( hTIFF ) &&
             nPlanarConfig == PLANARCONFIG_CONTIG && nBandCount > 1 )
    {
        GByte* pabyData = static_cast<GByte *>(pData);
        int anSrcYOffset[256] = { 0 };
        for( int y = 0; y < nBufYSize; )
        {
            const double dfYOffStart = nYOff + (y + 0.5) * dfSrcYInc;
            const int nSrcLine = static_cast<int>(dfYOffStart);
            const int nYOffsetInBlock = nSrcLine % nBlockYSize;
            const int nBlockYOff = nSrcLine / nBlockYSize;
            const int nBaseByteOffsetInBlock =
                nYOffsetInBlock * nBlockXSize * nBandsPerBlockDTSize;
            int ychunk = 1;
            int nLastSrcLineK = nSrcLine;
            anSrcYOffset[0] = 0;
            for( int k = 1; k < nBufYSize - y; ++k )
            {
                int nSrcLineK =
                    nYOff + static_cast<int>((y + k + 0.5) * dfSrcYInc);
                const int nBlockYOffK = nSrcLineK / nBlockYSize;
                if( k < 256)
                    anSrcYOffset[k] =
                        ((nSrcLineK % nBlockYSize) - nYOffsetInBlock) *
                        nBlockXSize * nBandsPerBlockDTSize;
                if( nBlockYOffK != nBlockYOff )
                {
                    break;
                }
                ++ychunk;
                nLastSrcLineK = nSrcLineK;
            }
            const int nUsedBlockHeight = nLastSrcLineK - nSrcLine + 1;
            // CPLAssert(nUsedBlockHeight <= nBlockYSize);

            double dfSrcX = nXOff + 0.5 * dfSrcXInc;
            int nCurBlockXOff = 0;
            int nNextBlockXOff = 0;
            toff_t nCurOffset = 0;
            const GByte* pabyLocalSrcDataStartLine = NULL;
            for( int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc)
            {
                const int nSrcPixel = static_cast<int>(dfSrcX);
                if( nSrcPixel >= nNextBlockXOff )
                {
                    const int nBlockXOff = nSrcPixel / nBlockXSize;
                    nCurBlockXOff = nBlockXOff * nBlockXSize;
                    nNextBlockXOff = nCurBlockXOff + nBlockXSize;
                    const int nBlockId =
                        nBlockXOff + nBlockYOff * nBlocksPerRow;
                    nCurOffset = panOffsets[nBlockId];
                    if( nCurOffset != 0 )
                    {
                        pabyLocalSrcDataStartLine =
                            oFetcher.FetchBytes(
                                nCurOffset + nBaseByteOffsetInBlock,
                                nBlockXSize *
                                nBandsPerBlock * nUsedBlockHeight,
                                nDTSize,
                                bIsByteSwapped, bIsComplex, nBlockId);
                        if( pabyLocalSrcDataStartLine == NULL )
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
                        const GByte* pabyLocalSrcData = NULL;
                        if( ychunk <= 256 )
                        {
                            REACHED(39);
                            pabyLocalSrcData =
                                pabyLocalSrcDataK0 + anSrcYOffset[k];
                        }
                        else
                        {
                            REACHED(40);
                            const int nYOffsetInBlockK =
                                static_cast<int>(dfYOff) % nBlockYSize;
                            // CPLAssert(
                            //     nYOffsetInBlockK - nYOffsetInBlock <=
                            //     nUsedBlockHeight);
                            pabyLocalSrcData =
                                pabyLocalSrcDataK0 +
                                (nYOffsetInBlockK - nYOffsetInBlock) *
                                nBlockXSize * nBandsPerBlockDTSize;
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
             TIFFIsTiled( hTIFF ) )
             // && (nPlanarConfig == PLANARCONFIG_SEPARATE || nBandCount == 1) )
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
                const int nYOffsetInBlock = nSrcLine % nBlockYSize;
                const int nBlockYOff = nSrcLine / nBlockYSize;
                const int nBaseByteOffsetInBlock =
                    nYOffsetInBlock * nBlockXSize * nBandsPerBlockDTSize;
                int ychunk = 1;
                int nLastSrcLineK = nSrcLine;
                anSrcYOffset[0] = 0;
                for( int k = 1; k < nBufYSize - y; ++k )
                {
                    const int nSrcLineK =
                        nYOff + static_cast<int>((y + k + 0.5) * dfSrcYInc);
                    const int nBlockYOffK = nSrcLineK / nBlockYSize;
                    if( k < 256)
                        anSrcYOffset[k] =
                            ((nSrcLineK % nBlockYSize) - nYOffsetInBlock) *
                            nBlockXSize * nBandsPerBlockDTSize;
                    if( nBlockYOffK != nBlockYOff )
                    {
                        break;
                    }
                    ++ychunk;
                    nLastSrcLineK = nSrcLineK;
                }
                const int nUsedBlockHeight = nLastSrcLineK - nSrcLine + 1;
                // CPLAssert(nUsedBlockHeight <= nBlockYSize);

                double dfSrcX = nXOff + 0.5 * dfSrcXInc;
                int nCurBlockXOff = 0;
                int nNextBlockXOff = 0;
                toff_t nCurOffset = 0;
                const GByte* pabyLocalSrcDataStartLine = NULL;
                for( int x = 0; x < nBufXSize; ++x, dfSrcX += dfSrcXInc )
                {
                    int nSrcPixel = static_cast<int>(dfSrcX);
                    if( nSrcPixel >= nNextBlockXOff )
                    {
                        const int nBlockXOff = nSrcPixel / nBlockXSize;
                        nCurBlockXOff = nBlockXOff * nBlockXSize;
                        nNextBlockXOff = nCurBlockXOff + nBlockXSize;
                        int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
                        if( nPlanarConfig == PLANARCONFIG_SEPARATE )
                        {
                            REACHED(43);
                            nBlockId += nBlocksPerBand * (nBand - 1);
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
                                    nCurOffset + nBaseByteOffsetInBlock,
                                    nBlockXSize *
                                    nBandsPerBlock * nUsedBlockHeight,
                                    nDTSize,
                                    bIsByteSwapped, bIsComplex, nBlockId);
                            if( pabyLocalSrcDataStartLine == NULL )
                                return CE_Failure;

                            if( nPlanarConfig == PLANARCONFIG_CONTIG )
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
                            const GByte* pabyLocalSrcData = NULL;
                            if( ychunk <= 256 )
                            {
                                REACHED(48);
                                pabyLocalSrcData =
                                    pabyLocalSrcDataK0 + anSrcYOffset[k];
                            }
                            else
                            {
                                REACHED(49);
                                const int nYOffsetInBlockK =
                                    static_cast<int>(dfYOff) % nBlockYSize;
                                // CPLAssert(
                                //     nYOffsetInBlockK - nYOffsetInBlock <=
                                //     nUsedBlockHeight);
                                pabyLocalSrcData = pabyLocalSrcDataK0 +
                                    (nYOffsetInBlockK - nYOffsetInBlock) *
                                    nBlockXSize * nBandsPerBlockDTSize;
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
        if( !FetchBuffer::bMinimizeIO && TIFFIsTiled( hTIFF ) )
        {
            GByte* pabyData = static_cast<GByte *>(pData);
            for( int y = 0; y < nBufYSize; ++y )
            {
                const int nSrcLine =
                    nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                const int nBlockYOff = nSrcLine / nBlockYSize;
                const int nYOffsetInBlock = nSrcLine % nBlockYSize;
                const int nBaseByteOffsetInBlock =
                    nYOffsetInBlock * nBlockXSize * nBandsPerBlockDTSize;

                if( bNoXResampling )
                {
                    GByte* pabyLocalData = pabyData + y * nLineSpace;
                    int nBlockXOff = nXOff / nBlockXSize;
                    int nXOffsetInBlock = nXOff % nBlockXSize;
                    int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

                    int x = 0;
                    while( x < nBufXSize )
                    {
                        const int nByteOffsetInBlock = nBaseByteOffsetInBlock +
                                        nXOffsetInBlock * nBandsPerBlockDTSize;
                        const toff_t nCurOffset = panOffsets[nBlockId];
                        const int nUsedBlockWidth =
                            std::min(
                                nBlockXSize - nXOffsetInBlock,
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
                                        nCurOffset + nByteOffsetInBlock,
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
                                        nCurOffset + nByteOffsetInBlock,
                                        nIters * nBandsPerBlock, nDTSize,
                                        bIsByteSwapped, bIsComplex, nBlockId);
                                if( pabyLocalSrcData == NULL )
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
                    const GByte* pabyLocalSrcDataStartLine = NULL;
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
                            const int nBlockXOff = nSrcPixel / nBlockXSize;
                            nCurBlockXOff = nBlockXOff * nBlockXSize;
                            nNextBlockXOff = nCurBlockXOff + nBlockXSize;
                            const int nBlockId =
                                nBlockXOff + nBlockYOff * nBlocksPerRow;
                            nCurOffset = panOffsets[nBlockId];
                            if( nCurOffset != 0 )
                            {
                                pabyLocalSrcDataStartLine =
                                    oFetcher.FetchBytes(
                                        nCurOffset + nBaseByteOffsetInBlock,
                                        nBlockXSize *
                                        nBandsPerBlock,
                                        nDTSize,
                                        bIsByteSwapped, bIsComplex, nBlockId);
                                if( pabyLocalSrcDataStartLine == NULL )
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
        else  // Contig, stripped organized.
        {
            GByte* pabyData = static_cast<GByte*>(pData);
            for( int y = 0; y < nBufYSize; ++y )
            {
                const int nSrcLine =
                    nYOff + static_cast<int>((y + 0.5) * dfSrcYInc);
                const int nBlockYOff = nSrcLine / nBlockYSize;
                const int nYOffsetInBlock = nSrcLine % nBlockYSize;
                const int nBlockId = nBlockYOff;
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
                    const int nBaseByteOffsetInBlock =
                        (nYOffsetInBlock * nBlockXSize + nXOff) *
                        nBandsPerBlockDTSize;

                    if( bNoXResamplingNoTypeChange && nBands == nBandCount &&
                        nPixelSpace == nBandsPerBlockDTSize )
                    {
                        REACHED(8);
                        if( !oFetcher.FetchBytes(
                               pabyLocalData,
                               nCurOffset + nBaseByteOffsetInBlock,
                               nXSize * nBandsPerBlock, nDTSize, bIsByteSwapped,
                               bIsComplex, nBlockId) )
                        {
                            return CE_Failure;
                        }
                    }
                    else
                    {
                        const GByte* pabyLocalSrcData = oFetcher.FetchBytes(
                            nCurOffset + nBaseByteOffsetInBlock,
                            nXSize * nBandsPerBlock, nDTSize, bIsByteSwapped,
                            bIsComplex, nBlockId);
                        if( pabyLocalSrcData == NULL )
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
        if( !FetchBuffer::bMinimizeIO && TIFFIsTiled( hTIFF ) )
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
                    const int nBlockYOff = nSrcLine / nBlockYSize;
                    const int nYOffsetInBlock = nSrcLine % nBlockYSize;

                    int nBaseByteOffsetInBlock =
                        nYOffsetInBlock * nBlockXSize * nBandsPerBlockDTSize;
                    if( nPlanarConfig == PLANARCONFIG_CONTIG )
                    {
                        REACHED(12);
                        nBaseByteOffsetInBlock += (nBand - 1) * nDTSize;
                    }
                    else
                    {
                        REACHED(13);
                    }

                    if( bNoXResampling )
                    {
                        GByte* pabyLocalData = pabyData + y * nLineSpace;
                        int nBlockXOff = nXOff / nBlockXSize;
                        int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
                        if( nPlanarConfig == PLANARCONFIG_SEPARATE )
                        {
                            REACHED(14);
                            nBlockId += nBlocksPerBand * (nBand - 1);
                        }
                        else
                        {
                            REACHED(15);
                        }
                        int nXOffsetInBlock = nXOff % nBlockXSize;

                        int x = 0;
                        while( x < nBufXSize )
                        {
                            const int nByteOffsetInBlock =
                                nBaseByteOffsetInBlock +
                                nXOffsetInBlock * nBandsPerBlockDTSize;
                            const toff_t nCurOffset = panOffsets[nBlockId];
                            const int nUsedBlockWidth =
                                std::min(
                                    nBlockXSize - nXOffsetInBlock,
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
                                           nCurOffset + nByteOffsetInBlock,
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
                                            nCurOffset + nByteOffsetInBlock,
                                            (nIters - 1) * nBandsPerBlock + 1,
                                            nDTSize,
                                            bIsByteSwapped,
                                            bIsComplex,
                                            nBlockId );
                                    if( pabyLocalSrcData == NULL )
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

                        const GByte* pabyLocalSrcDataStartLine = NULL;
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
                                const int nBlockXOff = nSrcPixel / nBlockXSize;
                                nCurBlockXOff = nBlockXOff * nBlockXSize;
                                nNextBlockXOff = nCurBlockXOff + nBlockXSize;
                                int nBlockId =
                                    nBlockXOff + nBlockYOff * nBlocksPerRow;
                                if( nPlanarConfig == PLANARCONFIG_SEPARATE )
                                {
                                    REACHED(19);
                                    nBlockId += nBlocksPerBand * (nBand - 1);
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
                                            nCurOffset + nBaseByteOffsetInBlock,
                                            nBlockXSize * nBandsPerBlock,
                                            nDTSize,
                                            bIsByteSwapped,
                                            bIsComplex,
                                            nBlockId);
                                    if( pabyLocalSrcDataStartLine == NULL )
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
        else  // Non-contig reading, stripped.
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
                    const int nBlockYOff = nSrcLine / nBlockYSize;
                    const int nYOffsetInBlock = nSrcLine % nBlockYSize;
                    int nBlockId = nBlockYOff;
                    if( nPlanarConfig == PLANARCONFIG_SEPARATE )
                    {
                        REACHED(23);
                        nBlockId += nBlocksPerBand * (nBand - 1);
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
                        int nBaseByteOffsetInBlock =
                            (nYOffsetInBlock * nBlockXSize + nXOff) *
                            nBandsPerBlockDTSize;
                        if( nPlanarConfig == PLANARCONFIG_CONTIG )
                            nBaseByteOffsetInBlock += (nBand - 1) * nDTSize;

                        GByte* pabyLocalData = pabyData + y * nLineSpace;
                        if( bNoXResamplingNoTypeChange &&
                            nPixelSpace == nBandsPerBlockDTSize )
                        {
                            REACHED(26);
                            if( !oFetcher.FetchBytes(
                                pabyLocalData,
                                nCurOffset + nBaseByteOffsetInBlock,
                                (nXSize - 1) * nBandsPerBlock + 1, nDTSize,
                                bIsByteSwapped, bIsComplex, nBlockId) )
                            {
                                return CE_Failure;
                            }
                        }
                        else
                        {
                            const GByte* pabyLocalSrcData = oFetcher.FetchBytes(
                                nCurOffset + nBaseByteOffsetInBlock,
                                (nXSize - 1) * nBandsPerBlock + 1, nDTSize,
                                bIsByteSwapped, bIsComplex, nBlockId);
                            if( pabyLocalSrcData == NULL )
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
          nCompression == COMPRESSION_NONE &&
          (nPhotometric == PHOTOMETRIC_MINISBLACK ||
           nPhotometric == PHOTOMETRIC_RGB ||
           nPhotometric == PHOTOMETRIC_PALETTE) &&
          nBitsPerSample == nDTSizeBits &&
          SetDirectory() /* Very important to make hTIFF uptodate! */ ) )
    {
        return -1;
    }

    // Only know how to deal with nearest neighbour in this optimized routine.
    if( (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg != NULL &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
    {
        return -1;
    }

    // If the file is band interleave or only one band is requested, then
    // fallback to band DirectIO.
    bool bUseBandRasterIO = false;
    if( nPlanarConfig == PLANARCONFIG_SEPARATE || nBandCount == 1 )
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
        VSI_TIFFFlushBufferedWrite( TIFFClientdata( hTIFF ) );
    }

    if( TIFFIsTiled( hTIFF ) )
    {
        if( m_pTempBufferForCommonDirectIO == NULL )
        {
            const int nDTSize = nDTSizeBits / 8;
            m_nTempBufferForCommonDirectIOSize =
                static_cast<size_t>(nBlockXSize * nBlockYSize * nDTSize *
                ((nPlanarConfig == PLANARCONFIG_CONTIG) ? nBands : 1));

            m_pTempBufferForCommonDirectIO =
                static_cast<GByte *>(
                    VSI_MALLOC_VERBOSE(m_nTempBufferForCommonDirectIOSize) );
            if( m_pTempBufferForCommonDirectIO == NULL )
                return CE_Failure;
        }

        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( hTIFF ));
        FetchBufferDirectIO oFetcher(fp, m_pTempBufferForCommonDirectIO,
                                     m_nTempBufferForCommonDirectIOSize);

        return CommonDirectIO( oFetcher,
                               nXOff, nYOff, nXSize, nYSize,
                               pData, nBufXSize, nBufYSize,
                               eBufType,
                               nBandCount, panBandMap,
                               nPixelSpace, nLineSpace,
                              nBandSpace );
    }

    // Get strip offsets.
    toff_t *panTIFFOffsets = NULL;
    if( !TIFFGetField( hTIFF, TIFFTAG_STRIPOFFSETS, &panTIFFOffsets ) ||
        panTIFFOffsets == NULL )
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
    void* pTmpBuffer = NULL;
    int eErr = CE_None;
    int nContigBands = nBands;
    const int nSrcPixelSize = nDTSize * nContigBands;

    if( ppData == NULL || panOffsets == NULL || panSizes == NULL )
    {
        eErr = CE_Failure;
    }
    // For now we always allocate a temp buffer as it's easier.
    else
        // if( nXSize != nBufXSize || nYSize != nBufYSize ||
        //   eBufType != eDataType ||
        //   nPixelSpace != GDALGetDataTypeSizeBytes(eBufType) ||
        //   check if the user buffer is large enough )
    {
        // We need a temporary buffer for over-sampling/sub-sampling
        // and/or data type conversion.
        pTmpBuffer = VSI_MALLOC_VERBOSE(nReqXSize * nReqYSize * nSrcPixelSize);
        if( pTmpBuffer == NULL )
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
        const int nBlockYOff = nSrcLine / nBlockYSize;
        const int nYOffsetInBlock = nSrcLine % nBlockYSize;
        const int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
        const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

        panOffsets[iLine] = panTIFFOffsets[nBlockId];
        if( panOffsets[iLine] == 0)  // We don't support sparse files.
            eErr = -1;

        panOffsets[iLine] +=
            (nXOff + nYOffsetInBlock * nBlockXSize) * nSrcPixelSize;
        panSizes[iLine] = nReqXSize * nSrcPixelSize;
    }

    // Extract data from the file.
    if( eErr == CE_None )
    {
        VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( hTIFF ));
        const int nRet =
            VSIFReadMultiRangeL(nReqYSize, ppData, panOffsets, panSizes, fp);
        if( nRet != 0 )
            eErr = CE_Failure;
    }

    // Byte-swap if necessary.
    if( eErr == CE_None && TIFFIsByteSwapped(hTIFF) )
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
    if( eErr == CE_None && pTmpBuffer != NULL )
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
        ++poGDS->nJPEGOverviewVisibilityCounter;
        const CPLErr eErr =
            TryOverviewRasterIO( eRWFlag,
                                 nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize,
                                 eBufType,
                                 nPixelSpace, nLineSpace,
                                 psExtraArg,
                                 &bTried );
        --poGDS->nJPEGOverviewVisibilityCounter;
        if( bTried )
            return eErr;
    }

    if( poGDS->eVirtualMemIOUsage != VIRTUAL_MEM_IO_NO )
    {
        const int nErr = poGDS->VirtualMemIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            1, &nBand, nPixelSpace, nLineSpace, 0, psExtraArg);
        if( nErr >= 0 )
            return static_cast<CPLErr>(nErr);
    }
    if( poGDS->bDirectIO )
    {
        int nErr = DirectIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                            pData, nBufXSize, nBufYSize, eBufType,
                            nPixelSpace, nLineSpace, psExtraArg);
        if( nErr >= 0 )
            return static_cast<CPLErr>(nErr);
    }

    if( poGDS->nBands != 1 &&
        poGDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
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
            static_cast<GIntBig>(poGDS->nBands) * nXBlocks * nYBlocks *
            nBlockXSize * nBlockYSize *
            GDALGetDataTypeSizeBytes(eDataType);
        if( nRequiredMem > GDALGetCacheMax64() )
        {
            if( !poGDS->bHasWarnedDisableAggressiveBandCaching )
            {
                CPLDebug( "GTiff",
                          "Disable aggressive band caching. "
                          "Cache not big enough. "
                          "At least " CPL_FRMT_GIB " bytes necessary",
                          nRequiredMem );
                poGDS->bHasWarnedDisableAggressiveBandCaching = true;
            }
            poGDS->bLoadingOtherBands = true;
        }
    }

    ++poGDS->nJPEGOverviewVisibilityCounter;
    const CPLErr eErr =
        GDALPamRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize, eBufType,
                                      nPixelSpace, nLineSpace, psExtraArg );
    --poGDS->nJPEGOverviewVisibilityCounter;

    poGDS->bLoadingOtherBands = false;

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
        poGDS->FlushCache();

    if( !poGDS->SetDirectory() )
    {
        return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
               GDAL_DATA_COVERAGE_STATUS_DATA;
    }

    const int iXBlockStart = nXOff / nBlockXSize;
    const int iXBlockEnd = (nXOff + nXSize - 1) / nBlockXSize;
    const int iYBlockStart = nYOff / nBlockYSize;
    const int iYBlockEnd = (nYOff + nYSize - 1) / nBlockYSize;
    int nStatus = 0;
    VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( poGDS->hTIFF ));
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
            if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
                nBlockId = nBlockIdBand0 + (nBand - 1) * poGDS->nBlocksPerBand;
            vsi_l_offset nOffset = 0;
            vsi_l_offset nLength = 0;
            bool bHasData = false;
            if( !poGDS->IsBlockAvailable(nBlockId,&nOffset,&nLength) )
            {
                nStatus |= GDAL_DATA_COVERAGE_STATUS_EMPTY;
            }
            else
            {
                if( poGDS->nCompression == COMPRESSION_NONE &&
                    poGDS->eAccess == GA_ReadOnly &&
                    (!bNoDataSet || dfNoDataValue == 0.0) )
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
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    if( !poGDS->SetDirectory() )
        return CE_Failure;

    int nBlockBufSize = 0;
    if( TIFFIsTiled(poGDS->hTIFF) )
    {
        nBlockBufSize = static_cast<int>(TIFFTileSize( poGDS->hTIFF ));
    }
    else
    {
        CPLAssert( nBlockXOff == 0 );
        nBlockBufSize = static_cast<int>(TIFFStripSize( poGDS->hTIFF ));
    }

    CPLAssert(nBlocksPerRow != 0);
    const int nBlockIdBand0 =
        nBlockXOff + nBlockYOff * nBlocksPerRow;
    int nBlockId = nBlockIdBand0;
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId = nBlockIdBand0 + (nBand - 1) * poGDS->nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*      The bottom most partial tiles and strips are sometimes only     */
/*      partially encoded.  This code reduces the requested data so     */
/*      an error won't be reported in this case. (#1179)                */
/* -------------------------------------------------------------------- */
    int nBlockReqSize = nBlockBufSize;

    if( nBlockYOff * nBlockYSize > nRasterYSize - nBlockYSize )
    {
        nBlockReqSize = (nBlockBufSize / nBlockYSize)
            * (nBlockYSize - static_cast<int>(
                (static_cast<GIntBig>(nBlockYOff + 1) * nBlockYSize)
                    % nRasterYSize));
    }

    poGDS->WaitCompletionForBlock(nBlockId);

/* -------------------------------------------------------------------- */
/*      Handle the case of a strip or tile that doesn't exist yet.      */
/*      Just set to zeros and return.                                   */
/* -------------------------------------------------------------------- */
    vsi_l_offset nOffset = 0;
    bool bErrOccured = false;
    if( nBlockId != poGDS->nLoadedBlock &&
        !poGDS->IsBlockAvailable(nBlockId, &nOffset, NULL, &bErrOccured) )
    {
        NullBlock( pImage );
        if( bErrOccured )
            return CE_Failure;
        return CE_None;
    }

    if( poGDS->bStreamingIn &&
        !(poGDS->nBands > 1 &&
          poGDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
          nBlockId == poGDS->nLoadedBlock) )
    {
        if( nOffset < VSIFTellL(poGDS->fpL) )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Trying to load block %d at offset " CPL_FRMT_GUIB
                      " whereas current pos is " CPL_FRMT_GUIB
                      " (backward read not supported)",
                      nBlockId, static_cast<GUIntBig>(nOffset),
                      static_cast<GUIntBig>(VSIFTellL(poGDS->fpL)) );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle simple case (separate, onesampleperpixel)                */
/* -------------------------------------------------------------------- */
    if( poGDS->nBands == 1
        || poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        if( nBlockReqSize < nBlockBufSize )
            memset( pImage, 0, nBlockBufSize );

        CPLErr eErr = CE_None;
        if( TIFFIsTiled( poGDS->hTIFF ) )
        {
            if( TIFFReadEncodedTile( poGDS->hTIFF, nBlockId, pImage,
                                     nBlockReqSize ) == -1
                && !poGDS->bIgnoreReadErrors )
            {
                memset( pImage, 0, nBlockBufSize );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedTile() failed." );

                eErr = CE_Failure;
            }
        }
        else
        {
            if( TIFFReadEncodedStrip( poGDS->hTIFF, nBlockId, pImage,
                                      nBlockReqSize ) == -1
                && !poGDS->bIgnoreReadErrors )
            {
                memset( pImage, 0, nBlockBufSize );
                CPLError( CE_Failure, CPLE_AppDefined,
                        "TIFFReadEncodedStrip() failed." );

                eErr = CE_Failure;
            }
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Load desired block                                              */
/* -------------------------------------------------------------------- */
    {
        const CPLErr eErr = poGDS->LoadBlockBuf( nBlockId );
        if( eErr != CE_None )
        {
            memset( pImage, 0,
                    nBlockXSize * nBlockYSize
                    * GDALGetDataTypeSizeBytes(eDataType) );
            return eErr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for YCbCr subsampled data.                         */
/* -------------------------------------------------------------------- */

    // Removed "Special case for YCbCr" added in r9432; disabled in r9470

    const int nWordBytes = poGDS->nBitsPerSample / 8;
    GByte* pabyImage = poGDS->pabyBlockBuf + (nBand - 1) * nWordBytes;

    GDALCopyWords(pabyImage, eDataType, poGDS->nBands * nWordBytes,
                  pImage, eDataType, nWordBytes,
                  nBlockXSize * nBlockYSize);

    const CPLErr eErr = FillCacheForOtherBands(nBlockXOff, nBlockYOff);

    return eErr;
}

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
    if( poGDS->nBands != 1 && !poGDS->bLoadingOtherBands &&
        nBlockXSize * nBlockYSize * GDALGetDataTypeSizeBytes(eDataType) <
        GDALGetCacheMax64() / poGDS->nBands )
    {
        poGDS->bLoadingOtherBands = true;

        for( int iOtherBand = 1; iOtherBand <= poGDS->nBands; ++iOtherBand )
        {
            if( iOtherBand == nBand )
                continue;

            GDALRasterBlock *poBlock = poGDS->GetRasterBand(iOtherBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff);
            if( poBlock == NULL )
            {
                eErr = CE_Failure;
                break;
            }
            poBlock->DropLock();
        }

        poGDS->bLoadingOtherBands = false;
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    if( poGDS->bDebugDontWriteBlocks )
        return CE_None;

    if( poGDS->bWriteErrorInFlushBlockBuf )
    {
        // Report as an error if a previously loaded block couldn't be written
        // correctly.
        poGDS->bWriteErrorInFlushBlockBuf = false;
        return CE_Failure;
    }

    if( !poGDS->SetDirectory() )
        return CE_Failure;

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );
    CPLAssert(nBlocksPerRow != 0);

/* -------------------------------------------------------------------- */
/*      Handle case of "separate" images                                */
/* -------------------------------------------------------------------- */
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE
        || poGDS->nBands == 1 )
    {
        const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
            + (nBand - 1) * poGDS->nBlocksPerBand;

        const CPLErr eErr =
            poGDS->WriteEncodedTileOrStrip(nBlockId, pImage, true);

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Handle case of pixel interleaved (PLANARCONFIG_CONTIG) images.  */
/* -------------------------------------------------------------------- */
    const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
     // Why 10 ? Somewhat arbitrary
    static const int MAX_BANDS_FOR_DIRTY_CHECK = 10;
    GDALRasterBlock* apoBlocks[MAX_BANDS_FOR_DIRTY_CHECK] = {};
    const int nBands = poGDS->nBands;
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
                    reinterpret_cast<GTiffRasterBand *>(
                        poGDS->GetRasterBand( iBand + 1 ))
                            ->TryGetLockedBlockRef( nBlockXOff, nBlockYOff );

                if( apoBlocks[iBand] == NULL )
                {
                    bAllBlocksDirty = false;
                }
                else if( !apoBlocks[iBand]->GetDirty() )
                {
                    apoBlocks[iBand]->DropLock();
                    apoBlocks[iBand] = NULL;
                    bAllBlocksDirty = false;
                }
            }
            else
                apoBlocks[iBand] = NULL;
        }
#if DEBUG_VERBOSE
        if( bAllBlocksDirty )
            CPLDebug("GTIFF", "Saved reloading block %d", nBlockId);
        else
            CPLDebug("GTIFF", "Must reload block %d", nBlockId);
#endif
    }

    {
        const CPLErr eErr = poGDS->LoadBlockBuf( nBlockId, !bAllBlocksDirty );
        if( eErr != CE_None )
        {
            if( nBands <= MAX_BANDS_FOR_DIRTY_CHECK )
            {
                for( int iBand = 0; iBand < nBands; ++iBand )
                {
                    if( apoBlocks[iBand] != NULL )
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
    const int nWordBytes = poGDS->nBitsPerSample / 8;

    for( int iBand = 0; iBand < nBands; ++iBand )
    {
        const GByte *pabyThisImage = NULL;
        GDALRasterBlock *poBlock = NULL;

        if( iBand + 1 == nBand )
        {
            pabyThisImage = static_cast<GByte *>( pImage );
        }
        else
        {
            if( nBands <= MAX_BANDS_FOR_DIRTY_CHECK )
                poBlock = apoBlocks[iBand];
            else
                poBlock = reinterpret_cast<GTiffRasterBand *>(
                    poGDS->GetRasterBand( iBand + 1 ))
                        ->TryGetLockedBlockRef( nBlockXOff, nBlockYOff );

            if( poBlock == NULL )
                continue;

            if( !poBlock->GetDirty() )
            {
                poBlock->DropLock();
                continue;
            }

            pabyThisImage = static_cast<GByte *>( poBlock->GetDataRef() );
        }

        GByte *pabyOut = poGDS->pabyBlockBuf + iBand*nWordBytes;

        GDALCopyWords(pabyThisImage, eDataType, nWordBytes,
                      pabyOut, eDataType, nWordBytes * nBands,
                      nBlockXSize * nBlockYSize);

        if( poBlock != NULL )
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    if( bAllBlocksDirty )
    {
        // We can synchronously write the block now.
        const CPLErr eErr =
            poGDS->WriteEncodedTileOrStrip(nBlockId, poGDS->pabyBlockBuf, true);
        poGDS->bLoadedBlockDirty = false;
        return eErr;
    }

    poGDS->bLoadedBlockDirty = true;

    return CE_None;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void GTiffRasterBand::SetDescription( const char *pszDescription )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( pszDescription == NULL )
        pszDescription = "";

    if( osDescription != pszDescription )
        poGDS->bMetadataChanged = true;

    osDescription = pszDescription;
}

/************************************************************************/
/*                           GetDescription()                           */
/************************************************************************/

const char *GTiffRasterBand::GetDescription() const
{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    return osDescription;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GTiffRasterBand::GetOffset( int *pbSuccess )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( pbSuccess )
        *pbSuccess = bHaveOffsetScale;
    return dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetOffset( double dfNewValue )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( !bHaveOffsetScale || dfNewValue != dfOffset )
        poGDS->bMetadataChanged = true;

    bHaveOffsetScale = true;
    dfOffset = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GTiffRasterBand::GetScale( int *pbSuccess )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( pbSuccess )
        *pbSuccess = bHaveOffsetScale;
    return dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetScale( double dfNewValue )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( !bHaveOffsetScale || dfNewValue != dfScale )
        poGDS->bMetadataChanged = true;

    bHaveOffsetScale = true;
    dfScale = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char* GTiffRasterBand::GetUnitType()

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();
    if( osUnitType.empty() )
    {
        poGDS->LookForProjection();
        return poGDS->m_osVertUnit.c_str();
    }

    return osUnitType.c_str();
}

/************************************************************************/
/*                           SetUnitType()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetUnitType( const char* pszNewValue )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    CPLString osNewValue(pszNewValue ? pszNewValue : "");
    if( osNewValue.compare(osUnitType) != 0 )
        poGDS->bMetadataChanged = true;

    osUnitType = osNewValue;
    return CE_None;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GTiffRasterBand::GetMetadataDomainList()
{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    return CSLDuplicate(oGTiffMDMD.GetDomainList());
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GTiffRasterBand::GetMetadata( const char * pszDomain )

{
    if( pszDomain == NULL || !EQUAL(pszDomain, "IMAGE_STRUCTURE") )
    {
        poGDS->LoadGeoreferencingAndPamIfNeeded();
    }

    return oGTiffMDMD.GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GTiffRasterBand::SetMetadata( char ** papszMD, const char *pszDomain )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( poGDS->bStreamingOut && poGDS->bCrystalized )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Cannot modify metadata at that point in a streamed "
                  "output file" );
        return CE_Failure;
    }

    if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
    {
        if( papszMD != NULL || GetMetadata(pszDomain) != NULL )
        {
            poGDS->bMetadataChanged = true;
            // Cancel any existing metadata from PAM file.
            if( eAccess == GA_Update &&
                GDALPamRasterBand::GetMetadata(pszDomain) != NULL )
                GDALPamRasterBand::SetMetadata(NULL, pszDomain);
        }
    }

    return oGTiffMDMD.SetMetadata( papszMD, pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GTiffRasterBand::GetMetadataItem( const char * pszName,
                                              const char * pszDomain )

{
    if( pszDomain == NULL || !EQUAL(pszDomain, "IMAGE_STRUCTURE") )
    {
        poGDS->LoadGeoreferencingAndPamIfNeeded();
    }

    if( pszName != NULL && pszDomain != NULL && EQUAL(pszDomain, "TIFF") )
    {
        int nBlockXOff = 0;
        int nBlockYOff = 0;

        if( EQUAL(pszName, "JPEGTABLES") )
        {
            if( !poGDS->SetDirectory() )
                return NULL;

            uint32 nJPEGTableSize = 0;
            void* pJPEGTable = NULL;
            if( TIFFGetField( poGDS->hTIFF, TIFFTAG_JPEGTABLES,
                              &nJPEGTableSize, &pJPEGTable ) != 1 ||
                pJPEGTable == NULL || nJPEGTableSize > INT_MAX )
            {
                return NULL;
            }
            char* const pszHex =
                CPLBinaryToHex( nJPEGTableSize, (const GByte*)pJPEGTable );
            const char* pszReturn = CPLSPrintf("%s", pszHex);
            CPLFree(pszHex);

            return pszReturn;
        }

        if( EQUAL(pszName, "IFD_OFFSET") )
        {
            if( !poGDS->SetDirectory() )
                return NULL;

            return CPLSPrintf( CPL_FRMT_GUIB,
                               static_cast<GUIntBig>(poGDS->nDirOffset) );
        }

        if( sscanf( pszName, "BLOCK_OFFSET_%d_%d",
                         &nBlockXOff, &nBlockYOff ) == 2 )
        {
            if( !poGDS->SetDirectory() )
                return NULL;

            nBlocksPerRow =
                DIV_ROUND_UP(poGDS->nRasterXSize, poGDS->nBlockXSize);
            nBlocksPerColumn =
                DIV_ROUND_UP(poGDS->nRasterYSize, poGDS->nBlockYSize);
            if( nBlockXOff < 0 || nBlockXOff >= nBlocksPerRow ||
                nBlockYOff < 0 || nBlockYOff >= nBlocksPerColumn )
                return NULL;

            int nBlockId = nBlockYOff * nBlocksPerRow + nBlockXOff;
            if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
            {
                nBlockId += (nBand - 1) * poGDS->nBlocksPerBand;
            }

            vsi_l_offset nOffset = 0;
            if( !poGDS->IsBlockAvailable(nBlockId, &nOffset) )
            {
                return NULL;
            }

            return CPLSPrintf( CPL_FRMT_GUIB, static_cast<GUIntBig>(nOffset) );
        }

        if( sscanf( pszName, "BLOCK_SIZE_%d_%d",
                    &nBlockXOff, &nBlockYOff ) == 2 )
        {
            if( !poGDS->SetDirectory() )
                return NULL;

            nBlocksPerRow =
                DIV_ROUND_UP(poGDS->nRasterXSize, poGDS->nBlockXSize);
            nBlocksPerColumn =
                DIV_ROUND_UP(poGDS->nRasterYSize, poGDS->nBlockYSize);
            if( nBlockXOff < 0 || nBlockXOff >= nBlocksPerRow ||
                nBlockYOff < 0 || nBlockYOff >= nBlocksPerColumn )
                return NULL;

            int nBlockId = nBlockYOff * nBlocksPerRow + nBlockXOff;
            if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
            {
                nBlockId += (nBand - 1) * poGDS->nBlocksPerBand;
            }

            vsi_l_offset nByteCount = 0;
            if( !poGDS->IsBlockAvailable(nBlockId, NULL, &nByteCount) )
            {
                return NULL;
            }

            return CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nByteCount));
        }
    }
    return oGTiffMDMD.GetMetadataItem( pszName, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GTiffRasterBand::SetMetadataItem( const char *pszName,
                                         const char *pszValue,
                                         const char *pszDomain )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( poGDS->bStreamingOut && poGDS->bCrystalized )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Cannot modify metadata at that point in a streamed "
                  "output file" );
        return CE_Failure;
    }

    if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
    {
        poGDS->bMetadataChanged = true;
        // Cancel any existing metadata from PAM file.
        if( eAccess == GA_Update &&
            GDALPamRasterBand::GetMetadataItem(pszName, pszDomain) != NULL )
            GDALPamRasterBand::SetMetadataItem(pszName, NULL, pszDomain);
    }

    return oGTiffMDMD.SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRasterBand::GetColorInterpretation()

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    return eBandInterp;
}

/************************************************************************/
/*                         GTiffGetAlphaValue()                         */
/************************************************************************/

// Note: Was EXTRASAMPLE_ASSOCALPHA in GDAL < 1.10.
static const uint16 DEFAULT_ALPHA_TYPE = EXTRASAMPLE_UNASSALPHA;

static uint16 GTiffGetAlphaValue(const char* pszValue, uint16 nDefault)
{
    if( pszValue == NULL )
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
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( eInterp == eBandInterp )
        return CE_None;

    eBandInterp = eInterp;

    if( poGDS->bCrystalized )
    {
        CPLDebug( "GTIFF", "ColorInterpretation %s for band %d goes to PAM "
                  "instead of TIFF tag",
                  GDALGetColorInterpretationName(eInterp), nBand );
        return GDALPamRasterBand::SetColorInterpretation( eInterp );
    }

    // Greyscale + alpha.
    if( eInterp == GCI_AlphaBand
        && nBand == 2
        && poGDS->nSamplesPerPixel == 2
        && poGDS->nPhotometric == PHOTOMETRIC_MINISBLACK )
    {
        const uint16 v[1] = {
            GTiffGetAlphaValue(CPLGetConfigOption("GTIFF_ALPHA", NULL),
                               DEFAULT_ALPHA_TYPE) };

        TIFFSetField(poGDS->hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
        return CE_None;
    }

    // Try to autoset TIFFTAG_PHOTOMETRIC = PHOTOMETRIC_RGB if possible.
    if( poGDS->nBands >= 3 &&
        poGDS->nCompression != COMPRESSION_JPEG &&
        poGDS->nPhotometric != PHOTOMETRIC_RGB &&
        CSLFetchNameValue( poGDS->papszCreationOptions,
                           "PHOTOMETRIC" ) == NULL &&
        ((nBand == 1 && eInterp == GCI_RedBand) ||
         (nBand == 2 && eInterp == GCI_GreenBand) ||
         (nBand == 3 && eInterp == GCI_BlueBand)) )
    {
        if( poGDS->GetRasterBand(1)->GetColorInterpretation() == GCI_RedBand &&
            poGDS->GetRasterBand(2)->GetColorInterpretation() == GCI_GreenBand &&
            poGDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand )
        {
            poGDS->nPhotometric = PHOTOMETRIC_RGB;
            TIFFSetField( poGDS->hTIFF, TIFFTAG_PHOTOMETRIC,
                          poGDS->nPhotometric );

            // We need to update the number of extra samples.
            uint16 *v = NULL;
            uint16 count = 0;
            const uint16 nNewExtraSamplesCount =
                static_cast<uint16>(poGDS->nBands - 3);
            if( poGDS->nBands >= 4 &&
                TIFFGetField( poGDS->hTIFF, TIFFTAG_EXTRASAMPLES,
                              &count, &v ) &&
                count > nNewExtraSamplesCount )
            {
                uint16 * const pasNewExtraSamples =
                    static_cast<uint16 *>( CPLMalloc(
                        nNewExtraSamplesCount * sizeof(uint16) ) );
                memcpy( pasNewExtraSamples, v + count - nNewExtraSamplesCount,
                        nNewExtraSamplesCount * sizeof(uint16) );

                TIFFSetField( poGDS->hTIFF, TIFFTAG_EXTRASAMPLES,
                              nNewExtraSamplesCount, pasNewExtraSamples );

                CPLFree(pasNewExtraSamples);
            }
        }
        return CE_None;
    }

    // On the contrary, cancel the above if needed
    if( poGDS->nCompression != COMPRESSION_JPEG &&
        poGDS->nPhotometric == PHOTOMETRIC_RGB &&
        CSLFetchNameValue( poGDS->papszCreationOptions,
                           "PHOTOMETRIC") == NULL &&
        ((nBand == 1 && eInterp != GCI_RedBand) ||
         (nBand == 2 && eInterp != GCI_GreenBand) ||
         (nBand == 3 && eInterp != GCI_BlueBand)) )
    {
        poGDS->nPhotometric = PHOTOMETRIC_MINISBLACK;
        TIFFSetField(poGDS->hTIFF, TIFFTAG_PHOTOMETRIC, poGDS->nPhotometric);

        // We need to update the number of extra samples.
        uint16 *v = NULL;
        uint16 count = 0;
        const uint16 nNewExtraSamplesCount =
            static_cast<uint16>(poGDS->nBands - 1);
        if( poGDS->nBands >= 2 )
        {
            TIFFGetField( poGDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v );
            if( nNewExtraSamplesCount > count )
            {
                uint16 * const pasNewExtraSamples =
                    static_cast<uint16 *>( CPLMalloc(
                        nNewExtraSamplesCount * sizeof(uint16) ) );
                for( int i = 0;
                     i < static_cast<int>(nNewExtraSamplesCount - count);
                     ++i )
                    pasNewExtraSamples[i] = EXTRASAMPLE_UNSPECIFIED;
                if( count > 0 )
                {
                    memcpy( pasNewExtraSamples + nNewExtraSamplesCount - count,
                            v,
                            count * sizeof(uint16) );
                }

                TIFFSetField( poGDS->hTIFF, TIFFTAG_EXTRASAMPLES,
                              nNewExtraSamplesCount, pasNewExtraSamples );

                CPLFree(pasNewExtraSamples);
            }
        }
    }

    // Mark alpha band in extrasamples.
    if( eInterp == GCI_AlphaBand )
    {
        uint16 *v = NULL;
        uint16 count = 0;
        if( TIFFGetField( poGDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            const int nBaseSamples = poGDS->nSamplesPerPixel - count;

            for( int i = 1; i <= poGDS->nBands; ++i )
            {
                if( i != nBand &&
                    poGDS->GetRasterBand(i)->GetColorInterpretation() ==
                    GCI_AlphaBand )
                {
                    if( i == nBaseSamples + 1 &&
                        CSLFetchNameValue( poGDS->papszCreationOptions,
                                           "ALPHA" ) != NULL )
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Band %d was already identified as alpha band, "
                            "and band %d is now marked as alpha too. "
                            "Presumably ALPHA creation option is not needed",
                            i, nBand );
                    }
                    else
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Band %d was already identified as alpha band, "
                            "and band %d is now marked as alpha too",
                            i, nBand );
                    }
                }
            }

            if( nBand > nBaseSamples && nBand - nBaseSamples - 1 < count )
            {
                // We need to allocate a new array as (current) libtiff
                // versions will not like that we reuse the array we got from
                // TIFFGetField().

                uint16* pasNewExtraSamples =
                    static_cast<uint16 *>(
                        CPLMalloc( count * sizeof(uint16) ) );
                memcpy( pasNewExtraSamples, v, count * sizeof(uint16) );
                pasNewExtraSamples[nBand - nBaseSamples - 1] =
                    GTiffGetAlphaValue(CPLGetConfigOption("GTIFF_ALPHA", NULL),
                                            DEFAULT_ALPHA_TYPE);

                TIFFSetField( poGDS->hTIFF, TIFFTAG_EXTRASAMPLES,
                              count, pasNewExtraSamples);

                CPLFree(pasNewExtraSamples);

                return CE_None;
            }
        }
    }

    if( poGDS->nPhotometric != PHOTOMETRIC_MINISBLACK &&
        CSLFetchNameValue( poGDS->papszCreationOptions, "PHOTOMETRIC") == NULL )
    {
        poGDS->nPhotometric = PHOTOMETRIC_MINISBLACK;
        TIFFSetField(poGDS->hTIFF, TIFFTAG_PHOTOMETRIC, poGDS->nPhotometric);
    }

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffRasterBand::GetColorTable()

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( nBand == 1 )
        return poGDS->poColorTable;

    return NULL;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr GTiffRasterBand::SetColorTable( GDALColorTable * poCT )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

/* -------------------------------------------------------------------- */
/*      Check if this is even a candidate for applying a PCT.           */
/* -------------------------------------------------------------------- */
    if( nBand != 1)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetColorTable() can only be called on band 1." );
        return CE_Failure;
    }

    if( poGDS->nSamplesPerPixel != 1 && poGDS->nSamplesPerPixel != 2)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetColorTable() not supported for multi-sample TIFF "
                  "files." );
        return CE_Failure;
    }

    if( eDataType != GDT_Byte && eDataType != GDT_UInt16 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SetColorTable() only supported for Byte or UInt16 bands "
                  "in TIFF format." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      We are careful about calling SetDirectory() to avoid            */
/*      prematurely crystallizing the directory.  (#2820)               */
/* -------------------------------------------------------------------- */
    if( poGDS->bCrystalized )
    {
        if( !poGDS->SetDirectory() )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Is this really a request to clear the color table?              */
/* -------------------------------------------------------------------- */
    if( poCT == NULL || poCT->GetColorEntryCount() == 0 )
    {
        TIFFSetField( poGDS->hTIFF, TIFFTAG_PHOTOMETRIC,
                      PHOTOMETRIC_MINISBLACK );

#ifdef HAVE_UNSETFIELD
        TIFFUnsetField( poGDS->hTIFF, TIFFTAG_COLORMAP );
#else
        CPLDebug(
            "GTiff",
            "TIFFUnsetField() not supported, colormap may not be cleared." );
#endif

        if( poGDS->poColorTable )
        {
            delete poGDS->poColorTable;
            poGDS->poColorTable = NULL;
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

    TIFFSetField( poGDS->hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
    TIFFSetField( poGDS->hTIFF, TIFFTAG_COLORMAP,
                  panTRed, panTGreen, panTBlue );

    CPLFree( panTRed );
    CPLFree( panTGreen );
    CPLFree( panTBlue );

    if( poGDS->poColorTable )
        delete poGDS->poColorTable;

    // libtiff 3.X needs setting this in all cases (creation or update)
    // whereas libtiff 4.X would just need it if there
    // was no color table before.
    poGDS->bNeedsRewrite = true;

    poGDS->poColorTable = poCT->Clone();
    eBandInterp = GCI_PaletteIndex;

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GTiffRasterBand::GetNoDataValue( int * pbSuccess )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return dfNoDataValue;
    }

    if( poGDS->bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return poGDS->dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr GTiffRasterBand::SetNoDataValue( double dfNoData )

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( poGDS->bNoDataSet && poGDS->dfNoDataValue == dfNoData )
        return CE_None;

    if( poGDS->bStreamingOut && poGDS->bCrystalized )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify nodata at that point in a streamed output file" );
        return CE_Failure;
    }

    poGDS->bNoDataSet = true;
    poGDS->dfNoDataValue = dfNoData;

    poGDS->bNoDataChanged = true;

    bNoDataSet = true;
    dfNoDataValue = dfNoData;
    return CE_None;
}

/************************************************************************/
/*                        DeleteNoDataValue()                           */
/************************************************************************/

CPLErr GTiffRasterBand::DeleteNoDataValue()

{
    poGDS->LoadGeoreferencingAndPamIfNeeded();

    if( !poGDS->bNoDataSet )
        return CE_None;

    if( poGDS->bStreamingOut && poGDS->bCrystalized )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify nodata at that point in a streamed output file" );
        return CE_Failure;
    }

    poGDS->bNoDataSet = false;
    poGDS->dfNoDataValue = -9999.0;

    poGDS->bNoDataChanged = true;

    bNoDataSet = false;
    dfNoDataValue = -9999.0;
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
    const int nWords = nBlockXSize * nBlockYSize;
    const int nChunkSize = std::max(1, GDALGetDataTypeSizeBytes(eDataType));

    int bNoDataSetIn = FALSE;
    const double dfNoData = GetNoDataValue( &bNoDataSetIn );
    if( !bNoDataSetIn )
    {
#ifdef ESRI_BUILD
        if( poGDS->nBitsPerSample >= 2 )
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
        GDALCopyWords( &dfNoData, GDT_Float64, 0,
                       pData, eDataType, nChunkSize, nWords);
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GTiffRasterBand::GetOverviewCount()

{
    poGDS->ScanDirectories();

    if( poGDS->nOverviewCount > 0 )
    {
        return poGDS->nOverviewCount;
    }

    const int nOverviewCount = GDALRasterBand::GetOverviewCount();
    if( nOverviewCount > 0 )
        return nOverviewCount;

    // Implicit JPEG overviews are normally hidden, except when doing
    // IRasterIO() operations.
    if( poGDS->nJPEGOverviewVisibilityCounter )
        return poGDS->GetJPEGOverviewCount();

    return 0;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetOverview( int i )

{
    poGDS->ScanDirectories();

    if( poGDS->nOverviewCount > 0 )
    {
        // Do we have internal overviews?
        if( i < 0 || i >= poGDS->nOverviewCount )
            return NULL;

        return poGDS->papoOverviewDS[i]->GetRasterBand(nBand);
    }

    GDALRasterBand* const poOvrBand = GDALRasterBand::GetOverview( i );
    if( poOvrBand != NULL )
        return poOvrBand;

    // For consistency with GetOverviewCount(), we should also test
    // nJPEGOverviewVisibilityCounter, but it is also convenient to be able
    // to query them for testing purposes.
    if( i >= 0 && i < poGDS->GetJPEGOverviewCount() )
        return poGDS->papoJPEGOverviewDS[i]->GetRasterBand(nBand);

    return NULL;
}

/************************************************************************/
/*                           GetMaskFlags()                             */
/************************************************************************/

int GTiffRasterBand::GetMaskFlags()
{
    poGDS->ScanDirectories();

    if( poGDS->poMaskDS != NULL )
    {
        if( poGDS->poMaskDS->GetRasterCount() == 1)
        {
            return GMF_PER_DATASET;
        }

        return 0;
    }

    return GDALPamRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetMaskBand()
{
    poGDS->ScanDirectories();

    if( poGDS->poMaskDS != NULL )
    {
        if( poGDS->poMaskDS->GetRasterCount() == 1 )
            return poGDS->poMaskDS->GetRasterBand(1);

        return poGDS->poMaskDS->GetRasterBand(nBand);
    }

    return GDALPamRasterBand::GetMaskBand();
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffSplitBand                           */
/* ==================================================================== */
/************************************************************************/

class GTiffSplitBand CPL_FINAL : public GTiffRasterBand
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
    // Optimization when reading the same line in a contig multi-band TIFF.
    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG && poGDS->nBands > 1 &&
        poGDS->nLastLineRead == nBlockYOff )
    {
        goto extract_band_data;
    }

    if( !poGDS->SetDirectory() )
        return CE_Failure;

    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
        poGDS->nBands > 1 )
    {
        if( poGDS->pabyBlockBuf == NULL )
        {
            poGDS->pabyBlockBuf =
                static_cast<GByte *>(
                    VSI_MALLOC_VERBOSE(TIFFScanlineSize(poGDS->hTIFF)) );
            if( poGDS->pabyBlockBuf == NULL )
            {
                return CE_Failure;
            }
        }
    }
    else
    {
        CPLAssert(TIFFScanlineSize(poGDS->hTIFF) == nBlockXSize);
    }

/* -------------------------------------------------------------------- */
/*      Read through to target scanline.                                */
/* -------------------------------------------------------------------- */
    if( poGDS->nLastLineRead >= nBlockYOff )
        poGDS->nLastLineRead = -1;

    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE && poGDS->nBands > 1 )
    {
        // If we change of band, we must start reading the
        // new strip from its beginning.
        if( poGDS->nLastBandRead != nBand )
            poGDS->nLastLineRead = -1;
        poGDS->nLastBandRead = nBand;
    }

    while( poGDS->nLastLineRead < nBlockYOff )
    {
        ++poGDS->nLastLineRead;
        if( TIFFReadScanline(
                poGDS->hTIFF,
                poGDS->pabyBlockBuf ? poGDS->pabyBlockBuf : pImage,
                poGDS->nLastLineRead,
                (poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE) ?
                 static_cast<uint16>(nBand - 1) : 0 ) == -1
            && !poGDS->bIgnoreReadErrors )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadScanline() failed." );
            poGDS->nLastLineRead = -1;
            return CE_Failure;
        }
    }

extract_band_data:
/* -------------------------------------------------------------------- */
/*      Extract band data from contig buffer.                           */
/* -------------------------------------------------------------------- */
    if( poGDS->pabyBlockBuf != NULL )
    {
        for( int iPixel = 0, iSrcOffset= nBand - 1, iDstOffset = 0;
             iPixel < nBlockXSize;
             ++iPixel, iSrcOffset += poGDS->nBands, ++iDstOffset )
        {
            static_cast<GByte *>(pImage)[iDstOffset] =
                poGDS->pabyBlockBuf[iSrcOffset];
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
    CPLError( CE_Failure, CPLE_AppDefined,
              "Split bands are read-only." );
    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffRGBABand                            */
/* ==================================================================== */
/************************************************************************/

class GTiffRGBABand CPL_FINAL : public GTiffRasterBand
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
    CPLError( CE_Failure, CPLE_AppDefined,
              "RGBA interpreted raster bands are read-only." );
    return CE_Failure;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRGBABand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    if( !poGDS->SetDirectory() )
        return CE_Failure;

    CPLAssert( nBlocksPerRow != 0 );
    const int nBlockBufSize = 4 * nBlockXSize * nBlockYSize;
    const int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( poGDS->pabyBlockBuf == NULL )
    {
        poGDS->pabyBlockBuf =
            static_cast<GByte *>(
                VSI_MALLOC3_VERBOSE( 4, nBlockXSize, nBlockYSize ) );
        if( poGDS->pabyBlockBuf == NULL )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the strip                                                  */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( poGDS->nLoadedBlock != nBlockId )
    {
        if( TIFFIsTiled( poGDS->hTIFF ) )
        {
#if defined(INTERNAL_LIBTIFF) || TIFFLIB_VERSION > 20161119
            if( TIFFReadRGBATileExt(
                   poGDS->hTIFF,
                   nBlockXOff * nBlockXSize,
                   nBlockYOff * nBlockYSize,
                   reinterpret_cast<uint32 *>(poGDS->pabyBlockBuf),
                   !poGDS->bIgnoreReadErrors) == 0
                && !poGDS->bIgnoreReadErrors )
#else
            if( TIFFReadRGBATile(
                   poGDS->hTIFF,
                   nBlockXOff * nBlockXSize,
                   nBlockYOff * nBlockYSize,
                   reinterpret_cast<uint32 *>(poGDS->pabyBlockBuf)) == 0
                && !poGDS->bIgnoreReadErrors )
#endif
            {
                // Once TIFFError() is properly hooked, this can go away.
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadRGBATile() failed." );

                memset( poGDS->pabyBlockBuf, 0, nBlockBufSize );

                eErr = CE_Failure;
            }
        }
        else
        {
#if defined(INTERNAL_LIBTIFF) || TIFFLIB_VERSION > 20161119
            if( TIFFReadRGBAStripExt(
                   poGDS->hTIFF,
                   nBlockId * nBlockYSize,
                   reinterpret_cast<uint32 *>(poGDS->pabyBlockBuf),
                   !poGDS->bIgnoreReadErrors) == 0
                && !poGDS->bIgnoreReadErrors )
#else
            if( TIFFReadRGBAStrip(
                   poGDS->hTIFF,
                   nBlockId * nBlockYSize,
                   reinterpret_cast<uint32 *>(poGDS->pabyBlockBuf)) == 0
                && !poGDS->bIgnoreReadErrors )
#endif
            {
                // Once TIFFError() is properly hooked, this can go away.
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadRGBAStrip() failed." );

                memset( poGDS->pabyBlockBuf, 0, nBlockBufSize );

                eErr = CE_Failure;
            }
        }
    }

    poGDS->nLoadedBlock = nBlockId;

/* -------------------------------------------------------------------- */
/*      Handle simple case of eight bit data, and pixel interleaving.   */
/* -------------------------------------------------------------------- */
    int nThisBlockYSize = nBlockYSize;

    if( nBlockYOff * nBlockYSize > GetYSize() - nBlockYSize
        && !TIFFIsTiled( poGDS->hTIFF ) )
        nThisBlockYSize = GetYSize() - nBlockYOff * nBlockYSize;

#ifdef CPL_LSB
    const int nBO = nBand - 1;
#else
    const int nBO = 4 - nBand;
#endif

    for( int iDestLine = 0; iDestLine < nThisBlockYSize; ++iDestLine )
    {
        const int nSrcOffset =
            (nThisBlockYSize - iDestLine - 1) * nBlockXSize * 4;

        GDALCopyWords(
            poGDS->pabyBlockBuf + nBO + nSrcOffset, GDT_Byte, 4,
            static_cast<GByte *>(pImage)+iDestLine*nBlockXSize, GDT_Byte, 1,
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

class GTiffOddBitsBand : public GTiffRasterBand
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

GTiffOddBitsBand::GTiffOddBitsBand( GTiffDataset *poGDSIn, int nBandIn )
        : GTiffRasterBand( poGDSIn, nBandIn )

{
    eDataType = GDT_Unknown;
    if( (poGDS->nBitsPerSample == 16 || poGDS->nBitsPerSample == 24) &&
        poGDS->nSampleFormat == SAMPLEFORMAT_IEEEFP )
        eDataType = GDT_Float32;
    // FIXME ? in autotest we currently open gcore/data/int24.tif
    // which is declared as signed, but we consider it as unsigned
    else if( (poGDS->nSampleFormat == SAMPLEFORMAT_UINT ||
              poGDS->nSampleFormat == SAMPLEFORMAT_INT) &&
             poGDS->nBitsPerSample < 8 )
        eDataType = GDT_Byte;
    else if( (poGDS->nSampleFormat == SAMPLEFORMAT_UINT ||
              poGDS->nSampleFormat == SAMPLEFORMAT_INT) &&
             poGDS->nBitsPerSample > 8 && poGDS->nBitsPerSample < 16 )
        eDataType = GDT_UInt16;
    else if( (poGDS->nSampleFormat == SAMPLEFORMAT_UINT ||
              poGDS->nSampleFormat == SAMPLEFORMAT_INT) &&
             poGDS->nBitsPerSample > 16 && poGDS->nBitsPerSample < 32 )
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
    if( poGDS->bWriteErrorInFlushBlockBuf )
    {
        // Report as an error if a previously loaded block couldn't be written
        // correctly.
        poGDS->bWriteErrorInFlushBlockBuf = false;
        return CE_Failure;
    }

    if( !poGDS->SetDirectory() )
        return CE_Failure;

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    if( eDataType == GDT_Float32 && poGDS->nBitsPerSample != 16 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Writing float data with nBitsPerSample = %d is unsupported",
                 poGDS->nBitsPerSample);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    CPLAssert(nBlocksPerRow != 0);
    int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId += (nBand - 1) * poGDS->nBlocksPerBand;

    // Only read content from disk in the CONTIG case.
    {
        const CPLErr eErr =
            poGDS->LoadBlockBuf( nBlockId,
                                 poGDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
                                 poGDS->nBands > 1 );
        if( eErr != CE_None )
            return eErr;
    }

    const GUInt32 nMaxVal = (1U << poGDS->nBitsPerSample) - 1;

/* -------------------------------------------------------------------- */
/*      Handle case of "separate" images or single band images where    */
/*      no interleaving with other data is required.                    */
/* -------------------------------------------------------------------- */
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE
        || poGDS->nBands == 1 )
    {
        // TODO(schwehr): Create a CplNumBits8Aligned.
        // Bits per line rounds up to next byte boundary.
        int nBitsPerLine = nBlockXSize * poGDS->nBitsPerSample;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        int iPixel = 0;

        // Small optimization in 1 bit case.
        if( poGDS->nBitsPerSample == 1 )
        {
            for( int iY = 0; iY < nBlockYSize; ++iY, iPixel += nBlockXSize )
            {
                int iBitOffset = iY * nBitsPerLine;

                const GByte* pabySrc =
                    static_cast<const GByte*>(pImage) + iPixel;
                int iByteOffset = iBitOffset / 8;
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
                    poGDS->pabyBlockBuf[iByteOffset] = static_cast<GByte>(nRes);
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
                    poGDS->pabyBlockBuf[iBitOffset>>3] =
                        static_cast<GByte>(nRes);
                }
            }

            poGDS->bLoadedBlockDirty = true;

            return CE_None;
        }

        if( eDataType == GDT_Float32 && poGDS->nBitsPerSample == 16 )
        {
            for( ; iPixel < nBlockYSize * nBlockXSize; iPixel++ )
            {
                GUInt32 nInWord = static_cast<GUInt32 *>(pImage)[iPixel];
                GUInt16 nHalf = FloatToHalf(nInWord, poGDS->bClipWarn);
                reinterpret_cast<GUInt16*>(poGDS->pabyBlockBuf)[iPixel] = nHalf;
            }

            poGDS->bLoadedBlockDirty = true;

            return CE_None;
        }

        // Initialize to zero as we set the buffer with binary or operations.
        if( poGDS->nBitsPerSample != 24 )
            memset(poGDS->pabyBlockBuf, 0, (nBitsPerLine / 8) * nBlockYSize);

        for( int iY = 0; iY < nBlockYSize; ++iY )
        {
            int iBitOffset = iY * nBitsPerLine;

            if( poGDS->nBitsPerSample == 12 )
            {
                for( int iX = 0; iX < nBlockXSize; ++iX )
                {
                    GUInt32 nInWord = static_cast<GUInt16 *>(pImage)[iPixel++];
                    if( nInWord > nMaxVal )
                    {
                        nInWord = nMaxVal;
                        if( !poGDS->bClipWarn )
                        {
                            poGDS->bClipWarn = true;
                            CPLError(
                                CE_Warning, CPLE_AppDefined,
                                "One or more pixels clipped to fit %d bit "
                                "domain.", poGDS->nBitsPerSample );
                        }
                    }

                    if( (iBitOffset % 8) == 0 )
                    {
                        poGDS->pabyBlockBuf[iBitOffset>>3] =
                            static_cast<GByte>(nInWord >> 4);
                        // Let 4 lower bits to zero as they're going to be
                        // overridden by the next word.
                        poGDS->pabyBlockBuf[(iBitOffset>>3)+1] =
                            static_cast<GByte>((nInWord & 0xf) << 4);
                    }
                    else
                    {
                        // Must or to preserve the 4 upper bits written
                        // for the previous word.
                        poGDS->pabyBlockBuf[iBitOffset>>3] |=
                            static_cast<GByte>(nInWord >> 8);
                        poGDS->pabyBlockBuf[(iBitOffset>>3)+1] =
                            static_cast<GByte>(nInWord & 0xff);
                    }

                    iBitOffset += poGDS->nBitsPerSample;
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
                    if( !poGDS->bClipWarn )
                    {
                        poGDS->bClipWarn = true;
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "One or more pixels clipped to fit %d bit domain.",
                            poGDS->nBitsPerSample );
                    }
                }

                if( poGDS->nBitsPerSample == 24 )
                {
/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugh (#2361).              */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 0] =
                        static_cast<GByte>( nInWord );
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 1] =
                        static_cast<GByte>( nInWord >> 8 );
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 2] =
                        static_cast<GByte>( nInWord >> 16 );
#else
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 0] =
                        static_cast<GByte>( nInWord >> 16 );
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 1] =
                        static_cast<GByte>( nInWord >> 8 );
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 2] =
                        static_cast<GByte>( nInWord );
#endif
                    iBitOffset += 24;
                }
                else
                {
                    for( int iBit = 0; iBit < poGDS->nBitsPerSample; ++iBit )
                    {
                        if( nInWord &
                            (1 << (poGDS->nBitsPerSample - 1 - iBit)) )
                            poGDS->pabyBlockBuf[iBitOffset>>3] |=
                                ( 0x80 >> (iBitOffset & 7) );
                        ++iBitOffset;
                    }
                }
            }
        }

        poGDS->bLoadedBlockDirty = true;

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
    for( int iBand = 0; iBand < poGDS->nBands; ++iBand )
    {
        const GByte *pabyThisImage = NULL;
        GDALRasterBlock *poBlock = NULL;

        if( iBand + 1 == nBand )
        {
            pabyThisImage = static_cast<GByte *>( pImage );
        }
        else
        {
            poBlock =
                reinterpret_cast<GTiffOddBitsBand *>(
                    poGDS->GetRasterBand( iBand + 1 ))
                        ->TryGetLockedBlockRef( nBlockXOff, nBlockYOff );

            if( poBlock == NULL )
                continue;

            if( !poBlock->GetDirty() )
            {
                poBlock->DropLock();
                continue;
            }

            pabyThisImage = static_cast<GByte *>(poBlock->GetDataRef());
        }

        const int iPixelBitSkip = poGDS->nBitsPerSample * poGDS->nBands;
        const int iBandBitOffset = iBand * poGDS->nBitsPerSample;

        // Bits per line rounds up to next byte boundary.
        int nBitsPerLine = nBlockXSize * iPixelBitSkip;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        int iPixel = 0;

        if( eDataType == GDT_Float32 && poGDS->nBitsPerSample == 16 )
        {
            for( ; iPixel < nBlockYSize * nBlockXSize; iPixel++ )
            {
                GUInt32 nInWord = reinterpret_cast<const GUInt32 *>(
                                                        pabyThisImage)[iPixel];
                GUInt16 nHalf = FloatToHalf(nInWord, poGDS->bClipWarn);
                reinterpret_cast<GUInt16*>(poGDS->pabyBlockBuf)[
                                    iPixel * poGDS->nBands + iBand] = nHalf;
            }

            if( poBlock != NULL )
            {
                poBlock->MarkClean();
                poBlock->DropLock();
            }
            continue;
        }

        for( int iY = 0; iY < nBlockYSize; ++iY )
        {
            int iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            if( poGDS->nBitsPerSample == 12 )
            {
                for( int iX = 0; iX < nBlockXSize; ++iX )
                {
                    GUInt32 nInWord =
                        reinterpret_cast<const GUInt16 *>(
                            pabyThisImage)[iPixel++];
                    if( nInWord > nMaxVal )
                    {
                        nInWord = nMaxVal;
                        if( !poGDS->bClipWarn )
                        {
                            poGDS->bClipWarn = true;
                            CPLError(
                                CE_Warning, CPLE_AppDefined,
                                "One or more pixels clipped to fit %d bit "
                                "domain.", poGDS->nBitsPerSample );
                        }
                    }

                    if( (iBitOffset % 8) == 0 )
                    {
                        poGDS->pabyBlockBuf[iBitOffset>>3] =
                            static_cast<GByte>( nInWord >> 4 );
                        poGDS->pabyBlockBuf[(iBitOffset>>3)+1] =
                            static_cast<GByte>(
                                ((nInWord & 0xf) << 4) |
                                (poGDS->pabyBlockBuf[(iBitOffset>>3)+1] &
                                 0xf) );
                    }
                    else
                    {
                        poGDS->pabyBlockBuf[iBitOffset>>3] =
                            static_cast<GByte>(
                                (poGDS->pabyBlockBuf[iBitOffset>>3] &
                                 0xf0) |
                                (nInWord >> 8));
                        poGDS->pabyBlockBuf[(iBitOffset>>3)+1] =
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
                    if( !poGDS->bClipWarn )
                    {
                        poGDS->bClipWarn = true;
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "One or more pixels clipped to fit %d bit domain.",
                            poGDS->nBitsPerSample );
                    }
                }

                if( poGDS->nBitsPerSample == 24 )
                {
/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugh (#2361).              */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 0] =
                        static_cast<GByte>(nInWord);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 1] =
                        static_cast<GByte>(nInWord >> 8);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 2] =
                        static_cast<GByte>(nInWord >> 16);
#else
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 0] =
                        static_cast<GByte>(nInWord >> 16);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 1] =
                        static_cast<GByte>(nInWord >> 8);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 2] =
                        static_cast<GByte>(nInWord);
#endif
                    iBitOffset += 24;
                }
                else
                {
                    for( int iBit = 0; iBit < poGDS->nBitsPerSample; ++iBit )
                    {
                        // TODO(schwehr): Revisit this block.
                        if( nInWord &
                            (1 << (poGDS->nBitsPerSample - 1 - iBit)) )
                        {
                            poGDS->pabyBlockBuf[iBitOffset>>3] |=
                                ( 0x80 >> (iBitOffset & 7) );
                        }
                        else
                        {
                            // We must explicitly unset the bit as we
                            // may update an existing block.
                            poGDS->pabyBlockBuf[iBitOffset>>3] &=
                                ~(0x80 >>(iBitOffset & 7));
                        }

                        ++iBitOffset;
                    }
                }

                iBitOffset = iBitOffset + iPixelBitSkip - poGDS->nBitsPerSample;
            }
        }

        if( poBlock != NULL )
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    poGDS->bLoadedBlockDirty = true;

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

static void ExpandPacked8ToByte1( const GByte * const CPL_RESTRICT pabySrc,
                                  GByte* const CPL_RESTRICT pabyDest,
                                  int nBytes )
{
    for( int i = 0, j = 0; i < nBytes; i++, j+= 8 )
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
                                    int nBytes )
{
    for( int i = 0, j = 0; i < nBytes; i++, j += 8 )
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
    if( !poGDS->SetDirectory() )
        return CE_Failure;

    CPLAssert(nBlocksPerRow != 0);
    int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId += (nBand - 1) * poGDS->nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*      Handle the case of a strip in a writable file that doesn't      */
/*      exist yet, but that we want to read.  Just set to zeros and     */
/*      return.                                                         */
/* -------------------------------------------------------------------- */
    if( nBlockId != poGDS->nLoadedBlock )
    {
        bool bErrOccured = false;
        if( !poGDS->IsBlockAvailable(nBlockId, NULL, NULL, &bErrOccured) )
        {
            NullBlock( pImage );
            if( bErrOccured )
                return CE_Failure;
            return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    {
        const CPLErr eErr = poGDS->LoadBlockBuf( nBlockId );
        if( eErr != CE_None )
            return eErr;
    }

    if( poGDS->nBitsPerSample == 1 &&
        (poGDS->nBands == 1 || poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE ) )
    {
/* -------------------------------------------------------------------- */
/*      Translate 1bit data to eight bit.                               */
/* -------------------------------------------------------------------- */
        int iDstOffset = 0;
        const GByte * const CPL_RESTRICT pabyBlockBuf = poGDS->pabyBlockBuf;
        GByte* CPL_RESTRICT pabyDest = static_cast<GByte *>(pImage);

        for( int iLine = 0; iLine < nBlockYSize; ++iLine )
        {
            int iSrcOffsetByte = ((nBlockXSize + 7) >> 3) * iLine;

            if( !poGDS->bPromoteTo8Bits )
            {
                ExpandPacked8ToByte1( pabyBlockBuf + iSrcOffsetByte,
                                      pabyDest + iDstOffset,
                                      nBlockXSize / 8 );
            }
            else
            {
                ExpandPacked8ToByte255( pabyBlockBuf + iSrcOffsetByte,
                                        pabyDest + iDstOffset,
                                        nBlockXSize / 8 );
            }
            int iSrcOffsetBit = (iSrcOffsetByte + nBlockXSize / 8) * 8;
            iDstOffset += nBlockXSize & ~0x7;
            const GByte bSetVal = poGDS->bPromoteTo8Bits ? 255 : 1;
            for( int iPixel = nBlockXSize & ~0x7 ;
                 iPixel < nBlockXSize;
                 ++iPixel, ++iSrcOffsetBit )
            {
                if( pabyBlockBuf[iSrcOffsetBit >>3] &
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
        const int nWordBytes = poGDS->nBitsPerSample / 8;
        const GByte *pabyImage = poGDS->pabyBlockBuf +
            ( ( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE ) ? 0 :
              (nBand - 1) * nWordBytes );
        const int iSkipBytes =
            ( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE ) ?
            nWordBytes : poGDS->nBands * nWordBytes;

        const int nBlockPixels = nBlockXSize * nBlockYSize;
        if( poGDS->nBitsPerSample == 16 )
        {
            for( int i = 0; i < nBlockPixels; ++i )
            {
                static_cast<GUInt32 *>(pImage)[i] =
                    HalfToFloat( *reinterpret_cast<const GUInt16 *>(pabyImage) );
                pabyImage += iSkipBytes;
            }
        }
        else if( poGDS->nBitsPerSample == 24 )
        {
            for( int i = 0; i < nBlockPixels; ++i )
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
    else if( poGDS->nBitsPerSample == 12 )
    {
        int iPixelBitSkip = 0;
        int iBandBitOffset = 0;

        if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelBitSkip = poGDS->nBands * poGDS->nBitsPerSample;
            iBandBitOffset = (nBand - 1) * poGDS->nBitsPerSample;
        }
        else
        {
            iPixelBitSkip = poGDS->nBitsPerSample;
        }

        // Bits per line rounds up to next byte boundary.
        int nBitsPerLine = nBlockXSize * iPixelBitSkip;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        int iPixel = 0;
        for( int iY = 0; iY < nBlockYSize; ++iY )
        {
            int iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for( int iX = 0; iX < nBlockXSize; ++iX )
            {
                const int iByte = iBitOffset >> 3;

                if( (iBitOffset & 0x7) == 0 )
                {
                    // Starting on byte boundary.

                    static_cast<GUInt16 *>(pImage)[iPixel++] =
                        (poGDS->pabyBlockBuf[iByte] << 4)
                        | (poGDS->pabyBlockBuf[iByte+1] >> 4);
                }
                else
                {
                    // Starting off byte boundary.

                    static_cast<GUInt16 *>(pImage)[iPixel++] =
                        ((poGDS->pabyBlockBuf[iByte] & 0xf) << 8)
                        | (poGDS->pabyBlockBuf[iByte+1]);
                }
                iBitOffset += iPixelBitSkip;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugh (#2361).              */
/* -------------------------------------------------------------------- */
    else if( poGDS->nBitsPerSample == 24 )
    {
        int iPixelByteSkip = 0;
        int iBandByteOffset = 0;

        if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelByteSkip = (poGDS->nBands * poGDS->nBitsPerSample) / 8;
            iBandByteOffset = ((nBand - 1) * poGDS->nBitsPerSample) / 8;
        }
        else
        {
            iPixelByteSkip = poGDS->nBitsPerSample / 8;
        }

        const int nBytesPerLine = nBlockXSize * iPixelByteSkip;

        int iPixel = 0;
        for( int iY = 0; iY < nBlockYSize; ++iY )
        {
            GByte *pabyImage =
                poGDS->pabyBlockBuf + iBandByteOffset + iY * nBytesPerLine;

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
        int iPixelBitSkip = 0;
        int iBandBitOffset = 0;

        if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelBitSkip = poGDS->nBands * poGDS->nBitsPerSample;
            iBandBitOffset = (nBand - 1) * poGDS->nBitsPerSample;
        }
        else
        {
            iPixelBitSkip = poGDS->nBitsPerSample;
        }

        // Bits per line rounds up to next byte boundary.
        GIntBig nBitsPerLine = static_cast<GIntBig>(nBlockXSize) * iPixelBitSkip;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        const GByte * const pabyBlockBuf = poGDS->pabyBlockBuf;
        const int nBitsPerSample = poGDS->nBitsPerSample;
        int iPixel = 0;

        for( int iY = 0; iY < nBlockYSize; ++iY )
        {
            GIntBig iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for( int iX = 0; iX < nBlockXSize; ++iX )
            {
                int nOutWord = 0;

                for( int iBit = 0; iBit < nBitsPerSample; ++iBit )
                {
                    if( pabyBlockBuf[iBitOffset>>3]
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

    GDALColorTable *poColorTable;

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

    if( poDSIn->poColorTable != NULL )
    {
        poColorTable = poDSIn->poColorTable->Clone();
    }
    else
    {
#ifdef ESRI_BUILD
        poColorTable = NULL;
#else
        const GDALColorEntry oWhite = { 255, 255, 255, 255 };
        const GDALColorEntry oBlack = { 0, 0, 0, 255 };

        poColorTable = new GDALColorTable();

        if( poDSIn->nPhotometric == PHOTOMETRIC_MINISWHITE )
        {
            poColorTable->SetColorEntry( 0, &oWhite );
            poColorTable->SetColorEntry( 1, &oBlack );
        }
        else
        {
            poColorTable->SetColorEntry( 0, &oBlack );
            poColorTable->SetColorEntry( 1, &oWhite );
        }
#endif  // not defined ESRI_BUILD.
    }
}

/************************************************************************/
/*                          ~GTiffBitmapBand()                          */
/************************************************************************/

GTiffBitmapBand::~GTiffBitmapBand()

{
    delete poColorTable;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffBitmapBand::GetColorInterpretation()

{
    if( poGDS->bPromoteTo8Bits )
        return GCI_Undefined;

    return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffBitmapBand::GetColorTable()

{
    if( poGDS->bPromoteTo8Bits )
        return NULL;

    return poColorTable;
}

/************************************************************************/
/* ==================================================================== */
/*                          GTiffSplitBitmapBand                        */
/* ==================================================================== */
/************************************************************************/

class GTiffSplitBitmapBand CPL_FINAL : public GTiffBitmapBand
{
    friend class GTiffDataset;
    int nLastLineValid;

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
        , nLastLineValid( -1 )

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
/*                            GTIFFErrorHandler()                       */
/************************************************************************/

namespace {
class GTIFFErrorStruct CPL_FINAL
{
  public:
    CPLErr type;
    CPLErrorNum no;
    CPLString msg;

    GTIFFErrorStruct() : type(CE_None), no(CPLE_None) {}
    GTIFFErrorStruct(CPLErr eErrIn, CPLErrorNum noIn, const char* msgIn) :
        type(eErrIn), no(noIn), msg(msgIn) {}
};
}

static void CPL_STDCALL GTIFFErrorHandler( CPLErr eErr, CPLErrorNum no,
                                           const char* msg )
{
    std::vector<GTIFFErrorStruct>* paoErrors =
        static_cast<std::vector<GTIFFErrorStruct> *>(
            CPLGetErrorHandlerUserData());
    paoErrors->push_back(GTIFFErrorStruct(eErr, no, msg));
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBitmapBand::IReadBlock( int /* nBlockXOff */, int nBlockYOff,
                                         void * pImage )

{
    if( nLastLineValid >= 0 && nBlockYOff > nLastLineValid )
        return CE_Failure;

    if( !poGDS->SetDirectory() )
        return CE_Failure;

    if( poGDS->pabyBlockBuf == NULL )
    {
        poGDS->pabyBlockBuf =
            static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(TIFFScanlineSize(poGDS->hTIFF)) );
        if( poGDS->pabyBlockBuf == NULL )
        {
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Read through to target scanline.                                */
/* -------------------------------------------------------------------- */
    if( poGDS->nLastLineRead >= nBlockYOff )
        poGDS->nLastLineRead = -1;

    while( poGDS->nLastLineRead < nBlockYOff )
    {
        ++poGDS->nLastLineRead;

        std::vector<GTIFFErrorStruct> aoErrors;
        CPLPushErrorHandlerEx(GTIFFErrorHandler, &aoErrors);
        int nRet = TIFFReadScanline( poGDS->hTIFF, poGDS->pabyBlockBuf,
                                     poGDS->nLastLineRead, 0 );
        CPLPopErrorHandler();

        for( size_t iError = 0; iError < aoErrors.size(); ++iError )
        {
            CPLError( aoErrors[iError].type,
                      aoErrors[iError].no,
                      "%s",
                      aoErrors[iError].msg.c_str() );
            // FAX decoding only handles EOF condition as a warning, so
            // catch it so as to turn on error when attempting to read
            // following lines, to avoid performance issues.
            if(  !poGDS->bIgnoreReadErrors &&
                    aoErrors[iError].msg.find("Premature EOF") !=
                                                    std::string::npos )
            {
                nLastLineValid = nBlockYOff;
                nRet = -1;
            }
        }

        if( nRet == -1
            && !poGDS->bIgnoreReadErrors )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadScanline() failed." );
            poGDS->nLastLineRead = -1;
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
        if( poGDS->pabyBlockBuf[iSrcOffset >>3] & (0x80 >> (iSrcOffset & 0x7)) )
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
    CPLError( CE_Failure, CPLE_AppDefined,
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

GTiffDataset::GTiffDataset() :
    hTIFF(NULL),
    fpL(NULL),
#if defined(INTERNAL_LIBTIFF) && defined(DEFER_STRILE_LOAD)
    nStripArrayAlloc(0),
    m_nFileSize(0),
#endif
    bStreamingIn(false),
    bStreamingOut(false),
    fpToWrite(NULL),
    nLastWrittenBlockId(-1),
    ppoActiveDSRef(NULL),
    poActiveDS(NULL),
    bScanDeferred(true),
    nDirOffset(0),
    bBase(true),
    bCloseTIFFHandle(false),
    nPlanarConfig(0),
    nSamplesPerPixel(0),
    nBitsPerSample(0),
    nRowsPerStrip(0),
    nPhotometric(0),
    nSampleFormat(0),
    nCompression(0),
    nBlocksPerBand(0),
    nBlockXSize(0),
    nBlockYSize(0),
    nLoadedBlock(-1),
    bLoadedBlockDirty(false),
    pabyBlockBuf(NULL),
    bWriteErrorInFlushBlockBuf(false),
    pszProjection(CPLStrdup("")),
    bLookedForProjection(false),
    bLookedForMDAreaOrPoint(false),
    bGeoTransformValid(false),
    bTreatAsRGBA(false),
    bCrystalized(true),
    poColorTable(NULL),
    nOverviewCount(0),
    papoOverviewDS(NULL),
    nJPEGOverviewVisibilityCounter(0),
    nJPEGOverviewCount(-1),
    nJPEGOverviewCountOri(0),
    papoJPEGOverviewDS(NULL),
    nGCPCount(0),
    pasGCPList(NULL),
    bGeoTIFFInfoChanged(false),
    bForceUnsetGTOrGCPs(false),
    bForceUnsetProjection(false),
    bNoDataChanged(false),
    bNoDataSet(false),
    dfNoDataValue(-9999.0),
    bMetadataChanged(false),
    bColorProfileMetadataChanged(false),
    bNeedsRewrite(false),
    osProfile("GDALGeoTIFF"),
    papszCreationOptions(NULL),
    bLoadingOtherBands(false),
    pabyTempWriteBuffer(NULL),
    nTempWriteBufferSize(0),
    poMaskDS(NULL),
    poBaseDS(NULL),
    bWriteEmptyTiles(true),
    bFillEmptyTilesAtClosing(false),
    nLastLineRead(-1),
    nLastBandRead(-1),
    bTreatAsSplit(false),
    bTreatAsSplitBitmap(false),
    bClipWarn(false),
    bIMDRPCMetadataLoaded(false),
    papszMetadataFiles(NULL),
    bEXIFMetadataLoaded(false),
    bICCMetadataLoaded(false),
    bHasWarnedDisableAggressiveBandCaching(false),
    bDontReloadFirstBlock(false),
    nZLevel(-1),
    nLZMAPreset(-1),
    nJpegQuality(-1),
    nJpegTablesMode(-1),
    bPromoteTo8Bits(false),
    bDebugDontWriteBlocks(false),
    bIsFinalized(false),
    bIgnoreReadErrors(false),
    bDirectIO(false),
    eVirtualMemIOUsage(VIRTUAL_MEM_IO_NO),
    psVirtualMemIOMapping(NULL),
    eGeoTIFFKeysFlavor(GEOTIFF_KEYS_STANDARD),
    pBaseMapping(NULL),
    nRefBaseMapping(0),
    bHasDiscardedLsb(false),
    poCompressThreadPool(NULL),
    hCompressThreadPoolMutex(NULL),
    m_pTempBufferForCommonDirectIO(NULL),
    m_nTempBufferForCommonDirectIOSize(0),
    m_bReadGeoTransform(false),
    m_bLoadPam(false),
    m_bHasGotSiblingFiles(false),
    m_bHasIdentifiedAuthorizedGeoreferencingSources(false),
    m_nPAMGeorefSrcIndex(-1),
    m_nINTERNALGeorefSrcIndex(-1),
    m_nTABFILEGeorefSrcIndex(-1),
    m_nWORLDFILEGeorefSrcIndex(-1),
    m_nGeoTransformGeorefSrcIndex(-1)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    bDebugDontWriteBlocks =
        CPLTestBool(CPLGetConfigOption("GTIFF_DONT_WRITE_BLOCKS", "NO"));

    bIgnoreReadErrors =
        CPLTestBool(CPLGetConfigOption("GTIFF_IGNORE_READ_ERRORS", "NO"));

    bDirectIO = CPLTestBool(CPLGetConfigOption("GTIFF_DIRECT_IO", "NO"));

    const char* pszVirtualMemIO =
        CPLGetConfigOption("GTIFF_VIRTUAL_MEM_IO", "NO");
    if( EQUAL(pszVirtualMemIO, "IF_ENOUGH_RAM") )
        eVirtualMemIOUsage = VIRTUAL_MEM_IO_IF_ENOUGH_RAM;
    else if( CPLTestBool(pszVirtualMemIO) )
        eVirtualMemIOUsage = VIRTUAL_MEM_IO_YES;
}

/************************************************************************/
/*                           ~GTiffDataset()                            */
/************************************************************************/

GTiffDataset::~GTiffDataset()

{
    Finalize();
}

/************************************************************************/
/*                             Finalize()                               */
/************************************************************************/

int GTiffDataset::Finalize()
{
    if( bIsFinalized )
        return FALSE;

    bool bHasDroppedRef = false;

    Crystalize();

    if( bColorProfileMetadataChanged )
    {
        SaveICCProfile(this, NULL, NULL, 0);
        bColorProfileMetadataChanged = false;
    }

/* -------------------------------------------------------------------- */
/*      Handle forcing xml:ESRI data to be written to PAM.              */
/* -------------------------------------------------------------------- */
    if( CPLTestBool(CPLGetConfigOption( "ESRI_XML_PAM", "NO" )) )
    {
        char **papszESRIMD = GetMetadata("xml:ESRI");
        if( papszESRIMD )
        {
            GDALPamDataset::SetMetadata( papszESRIMD, "xml:ESRI");
        }
    }

    if( psVirtualMemIOMapping )
        CPLVirtualMemFree( psVirtualMemIOMapping );
    psVirtualMemIOMapping = NULL;

/* -------------------------------------------------------------------- */
/*      Fill in missing blocks with empty data.                         */
/* -------------------------------------------------------------------- */
    if( bFillEmptyTilesAtClosing )
    {
/* -------------------------------------------------------------------- */
/*  Ensure any blocks write cached by GDAL gets pushed through libtiff. */
/* -------------------------------------------------------------------- */
        FlushCacheInternal( false /* do not call FlushDirectory */ );

        FillEmptyTiles();
        bFillEmptyTilesAtClosing = false;
    }

/* -------------------------------------------------------------------- */
/*      Force a complete flush, including either rewriting(moving)      */
/*      of writing in place the current directory.                      */
/* -------------------------------------------------------------------- */
    FlushCacheInternal( true );

    // Destroy compression pool.
    if( poCompressThreadPool )
    {
        delete poCompressThreadPool;

        for( int i = 0; i < static_cast<int>(asCompressionJobs.size()); ++i )
        {
            CPLFree(asCompressionJobs[i].pabyBuffer);
            if( asCompressionJobs[i].pszTmpFilename )
            {
                VSIUnlink(asCompressionJobs[i].pszTmpFilename);
                CPLFree(asCompressionJobs[i].pszTmpFilename);
            }
        }
        CPLDestroyMutex(hCompressThreadPoolMutex);
    }

/* -------------------------------------------------------------------- */
/*      If there is still changed metadata, then presumably we want     */
/*      to push it into PAM.                                            */
/* -------------------------------------------------------------------- */
    if( bMetadataChanged )
    {
        PushMetadataToPam();
        bMetadataChanged = false;
        GDALPamDataset::FlushCache();
    }

/* -------------------------------------------------------------------- */
/*      Cleanup overviews.                                              */
/* -------------------------------------------------------------------- */
    if( bBase )
    {
        for( int i = 0; i < nOverviewCount; ++i )
        {
            delete papoOverviewDS[i];
            bHasDroppedRef = true;
        }
        nOverviewCount = 0;

        for( int i = 0; i < nJPEGOverviewCountOri; ++i )
        {
            delete papoJPEGOverviewDS[i];
            bHasDroppedRef = true;
        }
        nJPEGOverviewCount = 0;
        nJPEGOverviewCountOri = 0;
        CPLFree( papoJPEGOverviewDS );
        papoJPEGOverviewDS = NULL;
    }

    // If we are a mask dataset, we can have overviews, but we don't
    // own them. We can only free the array, not the overviews themselves.
    CPLFree( papoOverviewDS );
    papoOverviewDS = NULL;

    // poMaskDS is owned by the main image and the overviews
    // so because of the latter case, we can delete it even if
    // we are not the base image.
    if( poMaskDS )
    {
        delete poMaskDS;
        poMaskDS = NULL;
        bHasDroppedRef = true;
    }

    if( poColorTable != NULL )
        delete poColorTable;
    poColorTable = NULL;

    if( bBase || bCloseTIFFHandle )
    {
        XTIFFClose( hTIFF );
        hTIFF = NULL;
        if( fpL != NULL )
        {
            if( VSIFCloseL( fpL ) != 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            }
            fpL = NULL;
        }
    }

    if( fpToWrite != NULL )
    {
        if( VSIFCloseL( fpToWrite ) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
        fpToWrite = NULL;
    }

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
        pasGCPList = NULL;
        nGCPCount = 0;
    }

    CPLFree( pszProjection );
    pszProjection = NULL;

    CSLDestroy( papszCreationOptions );
    papszCreationOptions = NULL;

    CPLFree(pabyTempWriteBuffer);
    pabyTempWriteBuffer = NULL;

    if( ppoActiveDSRef != NULL && *ppoActiveDSRef == this )
        *ppoActiveDSRef = NULL;
    ppoActiveDSRef = NULL;

    bIMDRPCMetadataLoaded = false;
    CSLDestroy(papszMetadataFiles);
    papszMetadataFiles = NULL;

    VSIFree(m_pTempBufferForCommonDirectIO);
    m_pTempBufferForCommonDirectIO = NULL;

    bIsFinalized = true;

    return bHasDroppedRef;
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int GTiffDataset::CloseDependentDatasets()
{
    if( !bBase )
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
    if( nJPEGOverviewCount >= 0 )
        return nJPEGOverviewCount;

    nJPEGOverviewCount = 0;
    if( !bBase || eAccess != GA_ReadOnly || nCompression != COMPRESSION_JPEG ||
        (nRasterXSize < 256 && nRasterYSize < 256) ||
        !CPLTestBool(CPLGetConfigOption("GTIFF_IMPLICIT_JPEG_OVR", "YES")) ||
        GDALGetDriverByName("JPEG") == NULL )
    {
        return 0;
    }
    const char* pszSourceColorSpace =
        oGTiffMDMD.GetMetadataItem( "SOURCE_COLOR_SPACE", "IMAGE_STRUCTURE" );
    if( pszSourceColorSpace != NULL && EQUAL(pszSourceColorSpace, "CMYK") )
    {
        // We cannot handle implicit overviews on JPEG CMYK datasets converted
        // to RGBA This would imply doing the conversion in
        // GTiffJPEGOverviewBand.
        return 0;
    }

    // libjpeg-6b only supports 2, 4 and 8 scale denominators.
    // TODO: Later versions support more.
    for( int i = 2; i >= 0; i-- )
    {
        if( nRasterXSize >= (256 << i) || nRasterYSize >= (256 << i) )
        {
            nJPEGOverviewCount = i + 1;
            break;
        }
    }
    if( nJPEGOverviewCount == 0 )
        return 0;

    if( !SetDirectory() )
    {
        nJPEGOverviewCount = 0;
        return 0;
    }

    // Get JPEG tables.
    uint32 nJPEGTableSize = 0;
    void* pJPEGTable = NULL;
    GByte abyFFD8[] = { 0xFF, 0xD8 };
    if( TIFFGetField(hTIFF, TIFFTAG_JPEGTABLES, &nJPEGTableSize, &pJPEGTable) )
    {
        if( pJPEGTable == NULL ||
            nJPEGTableSize > INT_MAX ||
            static_cast<GByte*>(pJPEGTable)[nJPEGTableSize-1] != 0xD9 )
        {
            nJPEGOverviewCount = 0;
            return 0;
        }
        nJPEGTableSize--;  // Remove final 0xD9.
    }
    else
    {
        pJPEGTable = abyFFD8;
        nJPEGTableSize = 2;
    }

    papoJPEGOverviewDS =
        static_cast<GTiffJPEGOverviewDS **>(
            CPLMalloc( sizeof(GTiffJPEGOverviewDS*) * nJPEGOverviewCount ) );
    for( int i = 0; i < nJPEGOverviewCount; ++i )
    {
        papoJPEGOverviewDS[i] =
            new GTiffJPEGOverviewDS(
                this, i + 1,
                pJPEGTable, static_cast<int>(nJPEGTableSize) );
    }

    nJPEGOverviewCountOri = nJPEGOverviewCount;

    return nJPEGOverviewCount;
}

/************************************************************************/
/*                           FillEmptyTiles()                           */
/************************************************************************/

void GTiffDataset::FillEmptyTiles()

{
    if( !SetDirectory() )
        return;

/* -------------------------------------------------------------------- */
/*      How many blocks are there in this file?                         */
/* -------------------------------------------------------------------- */
    const int nBlockCount =
        nPlanarConfig == PLANARCONFIG_SEPARATE ?
        nBlocksPerBand * nBands :
        nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*      Fetch block maps.                                               */
/* -------------------------------------------------------------------- */
    toff_t *panByteCounts = NULL;

    if( TIFFIsTiled( hTIFF ) )
        TIFFGetField( hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts );
    else
        TIFFGetField( hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts );

    if( panByteCounts == NULL )
    {
        // Got here with libtiff 3.9.3 and tiff_write_8 test.
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FillEmptyTiles() failed because panByteCounts == NULL" );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Prepare a blank data buffer to write for uninitialized blocks.  */
/* -------------------------------------------------------------------- */
    const int nBlockBytes =
        TIFFIsTiled( hTIFF ) ?
        static_cast<int>(TIFFTileSize(hTIFF)) :
        static_cast<int>(TIFFStripSize(hTIFF));

    GByte *pabyData =
        static_cast<GByte *>( VSI_CALLOC_VERBOSE(nBlockBytes, 1) );
    if( pabyData == NULL )
    {
        return;
    }

    // Force tiles completely filled with the nodata value to be written.
    bWriteEmptyTiles = true;

/* -------------------------------------------------------------------- */
/*      If set, fill data buffer with no data value.                    */
/* -------------------------------------------------------------------- */
    if( bNoDataSet && dfNoDataValue != 0.0 )
    {
        const GDALDataType eDataType = GetRasterBand( 1 )->GetRasterDataType();
        const int nDataTypeSize = GDALGetDataTypeSizeBytes( eDataType );
        if( nDataTypeSize &&
            nDataTypeSize * 8 == static_cast<int>(nBitsPerSample) )
        {
            GDALCopyWords( &dfNoDataValue, GDT_Float64, 0,
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
                VSI_MALLOC3_VERBOSE(nBlockXSize, nBlockYSize, nDataTypeSize) );
            if( pabyData == NULL )
                return;
            GDALCopyWords( &dfNoDataValue, GDT_Float64, 0,
                           pabyData, eDataType,
                           nDataTypeSize,
                           nBlockXSize * nBlockYSize );
            const int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
            for( int iBlock = 0; iBlock < nBlockCount; ++iBlock )
            {
                if( panByteCounts[iBlock] == 0 )
                {
                    if( nPlanarConfig == PLANARCONFIG_SEPARATE || nBands == 1 )
                    {
                        CPL_IGNORE_RET_VAL( GetRasterBand(
                            1 + iBlock / nBlocksPerBand )->WriteBlock(
                                (iBlock % nBlocksPerBand) % nBlocksPerRow,
                                (iBlock % nBlocksPerBand) / nBlocksPerRow,
                                pabyData ) );
                    }
                    else
                    {
                        // In contig case, don't directly call WriteBlock(), as
                        // it could cause useless decompression-recompression.
                        const int nXOff =
                            (iBlock % nBlocksPerRow) * nBlockXSize;
                        const int nYOff =
                            (iBlock / nBlocksPerRow) * nBlockYSize;
                        const int nXSize =
                            (nXOff + nBlockXSize <= nRasterXSize) ?
                            nBlockXSize : nRasterXSize - nXOff;
                        const int nYSize =
                            (nYOff + nBlockYSize <= nRasterYSize) ?
                            nBlockYSize : nRasterYSize - nYOff;
                        for( int iBand = 1; iBand <= nBands; ++iBand )
                        {
                            CPL_IGNORE_RET_VAL( GetRasterBand( iBand )->
                                RasterIO(
                                    GF_Write, nXOff, nYOff, nXSize, nYSize,
                                    pabyData, nXSize, nYSize,
                                    eDataType, 0, 0, NULL ) );
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
    else if( nCompression == COMPRESSION_NONE && (nBitsPerSample % 8) == 0 )
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
                    const bool bWriteEmptyTilesBak = bWriteEmptyTiles;
                    bWriteEmptyTiles = true;
                    const bool bOK =
                        WriteEncodedTileOrStrip( iBlock, pabyData,
                                                 FALSE ) == CE_None;
                    bWriteEmptyTiles = bWriteEmptyTilesBak;
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
            toff_t *panByteOffsets = NULL;

            if( TIFFIsTiled( hTIFF ) )
                TIFFGetField( hTIFF, TIFFTAG_TILEOFFSETS, &panByteOffsets );
            else
                TIFFGetField( hTIFF, TIFFTAG_STRIPOFFSETS, &panByteOffsets );

            if( panByteOffsets == NULL )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "FillEmptyTiles() failed because panByteOffsets == NULL");
                return;
            }

            VSILFILE* fpTIF = VSI_TIFFGetVSILFile(TIFFClientdata( hTIFF ));
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
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot initialize empty blocks");
            }
        }

        return;
    }

/* -------------------------------------------------------------------- */
/*      Check all blocks, writing out data for uninitialized blocks.    */
/* -------------------------------------------------------------------- */

    GByte* pabyRaw = NULL;
    vsi_l_offset nRawSize = 0;
    for( int iBlock = 0; iBlock < nBlockCount; ++iBlock )
    {
        if( panByteCounts[iBlock] == 0 )
        {
            if( pabyRaw == NULL )
            {
                if( WriteEncodedTileOrStrip( iBlock, pabyData, FALSE
                                                                ) != CE_None )
                    break;
                vsi_l_offset nOffset = 0;
                bool b = IsBlockAvailable( iBlock, &nOffset, &nRawSize);
#ifdef DEBUG
                CPLAssert(b);
#else
                CPL_IGNORE_RET_VAL(b);
#endif
                // When using compression, get back the compressed block
                // so we can use the raw API to write it faster.
                if( nCompression != COMPRESSION_NONE )
                {
                    pabyRaw = static_cast<GByte*>(
                            VSI_MALLOC_VERBOSE(static_cast<int>(nRawSize)));
                    if( pabyRaw )
                    {
                        VSILFILE* fp = VSI_TIFFGetVSILFile(
                                                    TIFFClientdata( hTIFF ));
                        const vsi_l_offset nCurOffset = VSIFTellL(fp);
                        VSIFSeekL(fp, nOffset, SEEK_SET);
                        VSIFReadL(pabyRaw, 1, static_cast<int>(nRawSize), fp);
                        VSIFSeekL(fp, nCurOffset, SEEK_SET);
                    }
                }
            }
            else
            {
                WriteRawStripOrTile( iBlock, pabyRaw,
                                     static_cast<int>(nRawSize) );
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
                                   int nLineStride, int nComponents )
{
    const T noDataValue = static_cast<T>((bNoDataSet) ? dfNoDataValue : 0.0);
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
    if( (!bNoDataSet || dfNoDataValue == 0.0) && nWidth == nLineStride
#ifdef CPL_CPU_REQUIRES_ALIGNED_ACCESS
        && CPL_IS_ALIGNED(pBuffer, sizeof(WordType))
#endif
        )
    {
        const GByte* pabyBuffer = reinterpret_cast<const GByte*>(pBuffer);
        const size_t nSize = static_cast<size_t>(nWidth) * nHeight *
                             nComponents * GDALGetDataTypeSizeBytes(eDT);
        size_t i = 0;
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

    if( nBitsPerSample == 8 )
    {
        if( nSampleFormat == SAMPLEFORMAT_INT )
        {
            return HasOnlyNoDataT(reinterpret_cast<const signed char*>(pBuffer),
                                  nWidth, nHeight, nLineStride, nComponents);
        }
        return HasOnlyNoDataT(reinterpret_cast<const GByte*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( nBitsPerSample == 16 && eDT == GDT_UInt16 )
    {
        return HasOnlyNoDataT(reinterpret_cast<const GUInt16*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( nBitsPerSample == 16 && eDT== GDT_Int16 )
    {
        return HasOnlyNoDataT(reinterpret_cast<const GInt16*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( nBitsPerSample == 32 && eDT == GDT_UInt32 )
    {
        return HasOnlyNoDataT(reinterpret_cast<const GUInt32*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( nBitsPerSample == 32 && eDT == GDT_Int32 )
    {
        return HasOnlyNoDataT(reinterpret_cast<const GInt32*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( nBitsPerSample == 32 && eDT == GDT_Float32 )
    {
        return HasOnlyNoDataT(reinterpret_cast<const float*>(pBuffer),
                              nWidth, nHeight, nLineStride, nComponents);
    }
    if( nBitsPerSample == 64 && eDT == GDT_Float64 )
    {
        return HasOnlyNoDataT(reinterpret_cast<const double*>(pBuffer),
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
    const double dfEffectiveNoData = (bNoDataSet) ? dfNoDataValue : 0.0;
    if( nBitsPerSample == 8 )
    {
        if( nSampleFormat == SAMPLEFORMAT_INT )
        {
            return GDALIsValueInRange<signed char>(dfEffectiveNoData) &&
                   *(reinterpret_cast<const signed char*>(pBuffer)) ==
                        static_cast<signed char>(dfEffectiveNoData);
        }
        return GDALIsValueInRange<GByte>(dfEffectiveNoData) &&
               *(reinterpret_cast<const GByte*>(pBuffer)) ==
                        static_cast<GByte>(dfEffectiveNoData);
    }
    if( nBitsPerSample == 16 && eDT == GDT_UInt16 )
    {
        return GDALIsValueInRange<GUInt16>(dfEffectiveNoData) &&
               *(reinterpret_cast<const GUInt16*>(pBuffer)) ==
                        static_cast<GUInt16>(dfEffectiveNoData);
    }
    if( nBitsPerSample == 16 && eDT == GDT_Int16 )
    {
        return GDALIsValueInRange<GInt16>(dfEffectiveNoData) &&
               *(reinterpret_cast<const GInt16*>(pBuffer)) ==
                        static_cast<GInt16>(dfEffectiveNoData);
    }
    if( nBitsPerSample == 32 && eDT == GDT_UInt32 )
    {
        return GDALIsValueInRange<GUInt32>(dfEffectiveNoData) &&
               *(reinterpret_cast<const GUInt32*>(pBuffer)) ==
                        static_cast<GUInt32>(dfEffectiveNoData);
    }
    if( nBitsPerSample == 32 && eDT == GDT_Int32 )
    {
        return GDALIsValueInRange<GInt32>(dfEffectiveNoData) &&
               *(reinterpret_cast<const GInt32*>(pBuffer)) ==
                        static_cast<GInt32>(dfEffectiveNoData);
    }
    if( nBitsPerSample == 32 && eDT == GDT_Float32 )
    {
        if( CPLIsNan(dfNoDataValue) )
            return CPL_TO_BOOL(
                CPLIsNan(*(reinterpret_cast<const float*>(pBuffer))));
        return GDALIsValueInRange<float>(dfEffectiveNoData) &&
               *(reinterpret_cast<const float*>(pBuffer)) ==
                        static_cast<float>(dfEffectiveNoData);
    }
    if( nBitsPerSample == 64 && eDT == GDT_Float64 )
    {
        if( CPLIsNan(dfEffectiveNoData) )
            return CPL_TO_BOOL(
                CPLIsNan(*(reinterpret_cast<const double*>(pBuffer))));
        return *(reinterpret_cast<const double*>(pBuffer)) == dfEffectiveNoData;
    }
    return false;
}

/************************************************************************/
/*                        WriteEncodedTile()                            */
/************************************************************************/

bool GTiffDataset::WriteEncodedTile( uint32 tile, GByte *pabyData,
                                     int bPreserveDataBuffer )
{
    int iRow = 0;
    int iColumn = 0;
    int nBlocksPerRow = 1;
    int nBlocksPerColumn = 1;

/* -------------------------------------------------------------------- */
/*      Don't write empty blocks in some cases.                         */
/* -------------------------------------------------------------------- */
    if( !bWriteEmptyTiles && IsFirstPixelEqualToNoData(pabyData) )
    {
        if( !IsBlockAvailable(tile) )
        {
            const int nComponents =
                nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;
            nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
            nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);

            iColumn = (tile % nBlocksPerBand) % nBlocksPerRow;
            iRow = (tile % nBlocksPerBand) / nBlocksPerRow;

            const int nActualBlockWidth =
                ( iColumn == nBlocksPerRow - 1 ) ?
                nRasterXSize - iColumn * nBlockXSize : nBlockXSize;
            const int nActualBlockHeight =
                ( iRow == nBlocksPerColumn - 1 ) ?
                nRasterYSize - iRow * nBlockYSize : nBlockYSize;

            if( HasOnlyNoData(pabyData,
                              nActualBlockWidth, nActualBlockHeight,
                              nBlockXSize, nComponents ) )
            {
                return true;
            }
        }
    }

    // Do we need to spread edge values right or down for a partial
    // JPEG encoded tile?  We do this to avoid edge artifacts.
    bool bNeedTileFill = false;
    if( nCompression == COMPRESSION_JPEG )
    {
        nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
        nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);

        iColumn = (tile % nBlocksPerBand) % nBlocksPerRow;
        iRow = (tile % nBlocksPerBand) / nBlocksPerRow;

        // Is this a partial right edge tile?
        if( iRow == nBlocksPerRow - 1
            && nRasterXSize % nBlockXSize != 0 )
            bNeedTileFill = true;

        // Is this a partial bottom edge tile?
        if( iColumn == nBlocksPerColumn - 1
            && nRasterYSize % nBlockYSize != 0 )
            bNeedTileFill = true;
    }

    // If we need to fill out the tile, or if we want to prevent
    // TIFFWriteEncodedTile from altering the buffer as part of
    // byte swapping the data on write then we will need a temporary
    // working buffer.  If not, we can just do a direct write.
    const int cc = static_cast<int>(TIFFTileSize( hTIFF ));

    if( bPreserveDataBuffer
        && (TIFFIsByteSwapped(hTIFF) || bNeedTileFill || bHasDiscardedLsb) )
    {
        if( cc != nTempWriteBufferSize )
        {
            pabyTempWriteBuffer = CPLRealloc(pabyTempWriteBuffer, cc);
            nTempWriteBufferSize = cc;
        }
        memcpy(pabyTempWriteBuffer, pabyData, cc);

        pabyData = static_cast<GByte *>( pabyTempWriteBuffer );
    }

    // Perform tile fill if needed.
    // TODO: we should also handle the case of nBitsPerSample == 12
    // but this is more involved.
    if( bNeedTileFill && nBitsPerSample == 8 )
    {
        const int nComponents =
            nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;

        CPLDebug( "GTiff", "Filling out jpeg edge tile on write." );

        const int nRightPixelsToFill =
            iColumn == nBlocksPerRow - 1 ?
            nBlockXSize * (iColumn + 1) - nRasterXSize :
            0;
        const int nBottomPixelsToFill =
            iRow == nBlocksPerColumn - 1 ?
            nBlockYSize * (iRow + 1) - nRasterYSize :
            0;

        // Fill out to the right.
        const int iSrcX = nBlockXSize - nRightPixelsToFill - 1;

        for( int iX = iSrcX + 1; iX < nBlockXSize; ++iX )
        {
            for( int iY = 0; iY < nBlockYSize; ++iY )
            {
                memcpy( pabyData + (nBlockXSize * iY + iX) * nComponents,
                        pabyData + (nBlockXSize * iY + iSrcX) * nComponents,
                        nComponents );
            }
        }

        // Now fill out the bottom.
        const int iSrcY = nBlockYSize - nBottomPixelsToFill - 1;
        for( int iY = iSrcY + 1; iY < nBlockYSize; ++iY )
        {
            memcpy( pabyData + nBlockXSize * nComponents * iY,
                    pabyData + nBlockXSize * nComponents * iSrcY,
                    nBlockXSize * nComponents );
        }
    }

    if( bHasDiscardedLsb )
    {
        const int iBand =
            nPlanarConfig == PLANARCONFIG_SEPARATE ?
            static_cast<int>(tile) / nBlocksPerBand : -1;
        DiscardLsb(pabyData, cc, iBand);
    }

    if( bStreamingOut )
    {
        if( tile != static_cast<uint32>(nLastWrittenBlockId + 1) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Attempt to write block %d whereas %d was expected",
                     tile, nLastWrittenBlockId + 1);
            return false;
        }
        if( static_cast<int>( VSIFWriteL(pabyData, 1, cc, fpToWrite) ) != cc )
        {
            CPLError( CE_Failure, CPLE_FileIO, "Could not write %d bytes",
                      cc );
            return false;
        }
        nLastWrittenBlockId = tile;
        return true;
    }

/* -------------------------------------------------------------------- */
/*      Should we do compression in a worker thread ?                   */
/* -------------------------------------------------------------------- */
    if( SubmitCompressionJob(tile, pabyData, cc, nBlockYSize) )
        return true;

    // libtiff 4.0.6 or older do not always properly report write errors.
#if !defined(INTERNAL_LIBTIFF) && (!defined(TIFFLIB_VERSION) || (TIFFLIB_VERSION <= 20150912))
    const CPLErr eBefore = CPLGetLastErrorType();
#endif
    const bool bRet =
        static_cast<int>(TIFFWriteEncodedTile(hTIFF, tile, pabyData, cc)) == cc;
#if !defined(INTERNAL_LIBTIFF) && (!defined(TIFFLIB_VERSION) || (TIFFLIB_VERSION <= 20150912))
    if( eBefore == CE_None && CPLGetLastErrorType() == CE_Failure )
        return false;
#endif
    return bRet;
}

/************************************************************************/
/*                        WriteEncodedStrip()                           */
/************************************************************************/

bool GTiffDataset::WriteEncodedStrip( uint32 strip, GByte* pabyData,
                                      int bPreserveDataBuffer )
{
    int cc = static_cast<int>(TIFFStripSize( hTIFF ));

/* -------------------------------------------------------------------- */
/*      If this is the last strip in the image, and is partial, then    */
/*      we need to trim the number of scanlines written to the          */
/*      amount of valid data we have. (#2748)                           */
/* -------------------------------------------------------------------- */
    const int nStripWithinBand = strip % nBlocksPerBand;
    int nStripHeight = nRowsPerStrip;

    if( nStripWithinBand * nStripHeight > GetRasterYSize() - nStripHeight )
    {
        nStripHeight = GetRasterYSize() - nStripWithinBand * nRowsPerStrip;
        cc = (cc / nRowsPerStrip) * nStripHeight;
        CPLDebug( "GTiff", "Adjusted bytes to write from %d to %d.",
                  static_cast<int>(TIFFStripSize(hTIFF)), cc );
    }

/* -------------------------------------------------------------------- */
/*      Don't write empty blocks in some cases.                         */
/* -------------------------------------------------------------------- */
    if( !bWriteEmptyTiles && IsFirstPixelEqualToNoData(pabyData) )
    {
        if( !IsBlockAvailable(strip) )
        {
            const int nComponents =
                nPlanarConfig == PLANARCONFIG_CONTIG ? nBands : 1;

            if( HasOnlyNoData(pabyData,
                              nBlockXSize, nStripHeight,
                              nBlockXSize, nComponents ) )
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
    if( bPreserveDataBuffer && (TIFFIsByteSwapped(hTIFF) || bHasDiscardedLsb) )
    {
        if( cc != nTempWriteBufferSize )
        {
            pabyTempWriteBuffer = CPLRealloc(pabyTempWriteBuffer, cc);
            nTempWriteBufferSize = cc;
        }
        memcpy(pabyTempWriteBuffer, pabyData, cc);
        pabyData = static_cast<GByte *>( pabyTempWriteBuffer );
    }

    if( bHasDiscardedLsb )
    {
        int iBand =
            nPlanarConfig == PLANARCONFIG_SEPARATE ?
            static_cast<int>(strip) / nBlocksPerBand : -1;
        DiscardLsb(pabyData, cc, iBand);
    }

    if( bStreamingOut )
    {
        if( strip != static_cast<uint32>(nLastWrittenBlockId + 1) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Attempt to write block %d whereas %d was expected",
                     strip, nLastWrittenBlockId + 1);
            return false;
        }
        if( static_cast<int>( VSIFWriteL(pabyData, 1, cc, fpToWrite) ) != cc )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Could not write %d bytes",
                     cc);
            return false;
        }
        nLastWrittenBlockId = strip;
        return true;
    }

/* -------------------------------------------------------------------- */
/*      Should we do compression in a worker thread ?                   */
/* -------------------------------------------------------------------- */
    if( SubmitCompressionJob(strip, pabyData, cc, nStripHeight) )
        return true;

    // libtiff 4.0.6 or older do not always properly report write errors.
#if !defined(INTERNAL_LIBTIFF) && (!defined(TIFFLIB_VERSION) || (TIFFLIB_VERSION <= 20150912))
    CPLErr eBefore = CPLGetLastErrorType();
#endif
    bool bRet =
        static_cast<int>(TIFFWriteEncodedStrip( hTIFF, strip,
                                                pabyData, cc)) == cc;
#if !defined(INTERNAL_LIBTIFF) && (!defined(TIFFLIB_VERSION) || (TIFFLIB_VERSION <= 20150912))
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
    const char* pszValue = CSLFetchNameValue( papszOptions, "NUM_THREADS" );
    if( pszValue == NULL )
        pszValue = CPLGetConfigOption("GDAL_NUM_THREADS", NULL);
    if( pszValue )
    {
        const int nThreads =
            EQUAL(pszValue, "ALL_CPUS") ? CPLGetNumCPUs() : atoi(pszValue);
        if( nThreads > 1 )
        {
            if( nCompression == COMPRESSION_NONE ||
                nCompression == COMPRESSION_JPEG )
            {
                CPLDebug( "GTiff",
                          "NUM_THREADS ignored with uncompressed or JPEG" );
            }
            else
            {
                CPLDebug("GTiff", "Using %d threads for compression", nThreads);
                poCompressThreadPool = new CPLWorkerThreadPool();
                if( !poCompressThreadPool->Setup(nThreads, NULL, NULL) )
                {
                    delete poCompressThreadPool;
                    poCompressThreadPool = NULL;
                }
                else
                {
                    // Add a margin of an extra job w.r.t thread number
                    // so as to optimize compression time (enables the main
                    // thread to do boring I/O while all CPUs are working).
                    asCompressionJobs.resize(nThreads + 1);
                    memset(&asCompressionJobs[0], 0,
                           asCompressionJobs.size() *
                           sizeof(GTiffCompressionJob));
                    for( int i = 0;
                         i < static_cast<int>(asCompressionJobs.size());
                         ++i )
                    {
                        asCompressionJobs[i].pszTmpFilename =
                            CPLStrdup(CPLSPrintf("/vsimem/gtiff/thread/job/%p",
                                                 &asCompressionJobs[i]));
                        asCompressionJobs[i].nStripOrTile = -1;
                    }
                    hCompressThreadPoolMutex = CPLCreateMutex();
                    CPLReleaseMutex(hCompressThreadPoolMutex);

                    // This is kind of a hack, but basically using
                    // TIFFWriteRawStrip/Tile and then TIFFReadEncodedStrip/Tile
                    // does not work on a newly created file, because
                    // TIFF_MYBUFFER is not set in tif_flags
                    // (if using TIFFWriteEncodedStrip/Tile first,
                    // TIFFWriteBufferSetup() is automatically called).
                    // This should likely rather fixed in libtiff itself.
                    TIFFWriteBufferSetup(hTIFF, NULL, -1);
                }
            }
        }
        else if( nThreads < 0 ||
                 (!EQUAL(pszValue, "0") &&
                  !EQUAL(pszValue, "1") &&
                  !EQUAL(pszValue, "ALL_CPUS")) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
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
/*                      InitCreationOrOpenOptions()                     */
/************************************************************************/

void GTiffDataset::InitCreationOrOpenOptions( char** papszOptions )
{
    InitCompressionThreads(papszOptions);

    eGeoTIFFKeysFlavor = GetGTIFFKeysFlavor(papszOptions);
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
    CPLAssert( hTIFFTmp != NULL );
    TIFFSetField(hTIFFTmp, TIFFTAG_IMAGEWIDTH, poDS->nBlockXSize);
    TIFFSetField(hTIFFTmp, TIFFTAG_IMAGELENGTH, psJob->nHeight);
    TIFFSetField(hTIFFTmp, TIFFTAG_BITSPERSAMPLE, poDS->nBitsPerSample);
    TIFFSetField(hTIFFTmp, TIFFTAG_COMPRESSION, poDS->nCompression);
    if( psJob->nPredictor != PREDICTOR_NONE )
        TIFFSetField(hTIFFTmp, TIFFTAG_PREDICTOR, psJob->nPredictor);
    if( poDS->nZLevel >= 0 )
        TIFFSetField(hTIFFTmp, TIFFTAG_ZIPQUALITY, poDS->nZLevel);
    if( poDS->nLZMAPreset > 0 && poDS->nCompression == COMPRESSION_LZMA)
        TIFFSetField(hTIFFTmp, TIFFTAG_LZMAPRESET, poDS->nLZMAPreset);
    TIFFSetField(hTIFFTmp, TIFFTAG_PHOTOMETRIC, poDS->nPhotometric);
    TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLEFORMAT, poDS->nSampleFormat);
    TIFFSetField(hTIFFTmp, TIFFTAG_SAMPLESPERPIXEL, poDS->nSamplesPerPixel);
    TIFFSetField(hTIFFTmp, TIFFTAG_ROWSPERSTRIP, poDS->nBlockYSize);
    TIFFSetField(hTIFFTmp, TIFFTAG_PLANARCONFIG, poDS->nPlanarConfig);

    bool bOK =
        TIFFWriteEncodedStrip(hTIFFTmp, 0, psJob->pabyBuffer,
                              psJob->nBufferSize) == psJob->nBufferSize;

    int nOffset = 0;
    if( bOK )
    {
        toff_t* panOffsets = NULL;
        toff_t* panByteCounts = NULL;
        TIFFGetField(hTIFFTmp, TIFFTAG_STRIPOFFSETS, &panOffsets);
        TIFFGetField(hTIFFTmp, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts);

        nOffset = static_cast<int>( panOffsets[0]);
        psJob->nCompressedBufferSize = static_cast<int>( panByteCounts[0] );
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
        CPLAssert( nOffset + psJob->nCompressedBufferSize <=
                   static_cast<int>(nFileSize) );
        psJob->pabyCompressedBuffer = pabyCompressedBuffer + nOffset;
    }
    else
    {
        psJob->pabyCompressedBuffer = NULL;
        psJob->nCompressedBufferSize = 0;
    }

    CPLAcquireMutex(poDS->hCompressThreadPoolMutex, 1000.0);
    psJob->bReady = true;
    CPLReleaseMutex(poDS->hCompressThreadPoolMutex);
}

/************************************************************************/
/*                        WriteRawStripOrTile()                         */
/************************************************************************/

void GTiffDataset::WriteRawStripOrTile( int nStripOrTile,
                                        GByte* pabyCompressedBuffer,
                                        int nCompressedBufferSize )
{
#ifdef DEBUG_VERBOSE
    CPLDebug("GTIFF", "Writing raw strip/tile %d, size %d",
             nStripOrTile, nCompressedBufferSize);
#endif
    toff_t *panOffsets = NULL;
    if( TIFFGetField(
            hTIFF,
            TIFFIsTiled( hTIFF ) ?
            TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS, &panOffsets ) &&
            panOffsets[nStripOrTile] != 0 )
    {
        // Make sure that if the tile/strip already exists,
        // we write at end of file.
        TIFFSetWriteOffset(hTIFF, 0);
    }
    if( TIFFIsTiled( hTIFF ) )
        TIFFWriteRawTile( hTIFF, nStripOrTile, pabyCompressedBuffer,
                          nCompressedBufferSize );
    else
        TIFFWriteRawStrip( hTIFF, nStripOrTile, pabyCompressedBuffer,
                           nCompressedBufferSize );
}

/************************************************************************/
/*                        WaitCompletionForBlock()                      */
/************************************************************************/

void GTiffDataset::WaitCompletionForBlock(int nBlockId)
{
    if( poCompressThreadPool != NULL )
    {
        for( int i = 0; i < static_cast<int>(asCompressionJobs.size()); ++i )
        {
            if( asCompressionJobs[i].nStripOrTile == nBlockId )
            {
                CPLDebug("GTIFF",
                         "Waiting for worker job to finish handling block %d",
                         nBlockId);

                CPLAcquireMutex(hCompressThreadPoolMutex, 1000.0);
                const bool bReady = asCompressionJobs[i].bReady;
                CPLReleaseMutex(hCompressThreadPoolMutex);
                if( !bReady )
                {
                    poCompressThreadPool->WaitCompletion(0);
                    CPLAssert( asCompressionJobs[i].bReady );
                }

                if( asCompressionJobs[i].nCompressedBufferSize )
                {
                    WriteRawStripOrTile(asCompressionJobs[i].nStripOrTile,
                                  asCompressionJobs[i].pabyCompressedBuffer,
                                  asCompressionJobs[i].nCompressedBufferSize);
                }
                asCompressionJobs[i].pabyCompressedBuffer = NULL;
                asCompressionJobs[i].nBufferSize = 0;
                asCompressionJobs[i].bReady = false;
                asCompressionJobs[i].nStripOrTile = -1;
                return;
            }
        }
    }
}

/************************************************************************/
/*                      SubmitCompressionJob()                          */
/************************************************************************/

bool GTiffDataset::SubmitCompressionJob( int nStripOrTile, GByte* pabyData,
                                         int cc, int nHeight )
{
/* -------------------------------------------------------------------- */
/*      Should we do compression in a worker thread ?                   */
/* -------------------------------------------------------------------- */
    if( !( poCompressThreadPool != NULL &&
           (nCompression == COMPRESSION_ADOBE_DEFLATE ||
            nCompression == COMPRESSION_LZW ||
            nCompression == COMPRESSION_PACKBITS ||
            nCompression == COMPRESSION_LZMA) ) )
        return false;

    int nNextCompressionJobAvail = -1;
    // Wait that at least one job is finished.
    poCompressThreadPool->WaitCompletion(
        static_cast<int>(asCompressionJobs.size() - 1) );
    for( int i = 0; i < static_cast<int>(asCompressionJobs.size()); ++i )
    {
        CPLAcquireMutex(hCompressThreadPoolMutex, 1000.0);
        const bool bReady = asCompressionJobs[i].bReady;
        CPLReleaseMutex(hCompressThreadPoolMutex);
        if( bReady )
        {
            if( asCompressionJobs[i].nCompressedBufferSize )
            {
                WriteRawStripOrTile( asCompressionJobs[i].nStripOrTile,
                                asCompressionJobs[i].pabyCompressedBuffer,
                                asCompressionJobs[i].nCompressedBufferSize );
            }
            asCompressionJobs[i].pabyCompressedBuffer = NULL;
            asCompressionJobs[i].nBufferSize = 0;
            asCompressionJobs[i].bReady = false;
            asCompressionJobs[i].nStripOrTile = -1;
        }
        if( asCompressionJobs[i].nBufferSize == 0 )
        {
            if( nNextCompressionJobAvail < 0 )
                nNextCompressionJobAvail = i;
        }
    }
    CPLAssert(nNextCompressionJobAvail >= 0);

    GTiffCompressionJob* psJob = &asCompressionJobs[nNextCompressionJobAvail];
    psJob->poDS = this;
    psJob->bTIFFIsBigEndian = CPL_TO_BOOL( TIFFIsBigEndian(hTIFF) );
    psJob->pabyBuffer =
        static_cast<GByte*>( CPLRealloc(psJob->pabyBuffer, cc) );
    memcpy(psJob->pabyBuffer, pabyData, cc);
    psJob->nBufferSize = cc;
    psJob->nHeight = nHeight;
    psJob->nStripOrTile = nStripOrTile;
    psJob->nPredictor = PREDICTOR_NONE;
    if( nCompression == COMPRESSION_LZW ||
        nCompression == COMPRESSION_ADOBE_DEFLATE )
    {
        TIFFGetField( hTIFF, TIFFTAG_PREDICTOR, &psJob->nPredictor );
    }

    poCompressThreadPool->SubmitJob(ThreadCompressionFunc, psJob);
    return true;
}

/************************************************************************/
/*                          DiscardLsb()                                */
/************************************************************************/

void GTiffDataset::DiscardLsb( GByte* pabyBuffer, int nBytes, int iBand )
{
    if( nBitsPerSample == 8 )
    {
        if( nPlanarConfig == PLANARCONFIG_SEPARATE )
        {
            const int nMask = anMaskLsb[iBand];
            const int nOffset = anOffsetLsb[iBand];
            for( int i = 0; i < nBytes; ++i )
            {
                // Keep 255 in case it is alpha.
                if( pabyBuffer[i] != 255 )
                    pabyBuffer[i] =
                        static_cast<GByte>((pabyBuffer[i] & nMask) | nOffset);
            }
        }
        else
        {
            for( int i = 0; i < nBytes; i += nBands )
            {
                for( int j = 0; j < nBands; ++j )
                {
                    // Keep 255 in case it is alpha.
                    if( pabyBuffer[i + j] != 255 )
                        pabyBuffer[i + j] =
                            static_cast<GByte>((pabyBuffer[i + j] &
                                                anMaskLsb[j]) | anOffsetLsb[j]);
                }
            }
        }
    }
    else if( nBitsPerSample == 16 )
    {
        if( nPlanarConfig == PLANARCONFIG_SEPARATE )
        {
            const int nMask = anMaskLsb[iBand];
            const int nOffset = anOffsetLsb[iBand];
            for( int i = 0; i < nBytes/2; ++i )
            {
                reinterpret_cast<GUInt16*>(pabyBuffer)[i] =
                    static_cast<GUInt16>(
                        (reinterpret_cast<GUInt16 *>(pabyBuffer)[i] & nMask) |
                        nOffset);
            }
        }
        else
        {
            for( int i = 0; i < nBytes/2; i += nBands )
            {
                for( int j = 0; j < nBands; ++j )
                {
                    reinterpret_cast<GUInt16*>(pabyBuffer)[i + j] =
                        static_cast<GUInt16>(
                            (reinterpret_cast<GUInt16*>(pabyBuffer)[i + j] &
                             anMaskLsb[j]) |
                            anOffsetLsb[j]);
                }
            }
        }
    }
    else if( nBitsPerSample == 32 )
    {
        if( nPlanarConfig == PLANARCONFIG_SEPARATE )
        {
            const int nMask = anMaskLsb[iBand];
            const int nOffset = anOffsetLsb[iBand];
            for( int i = 0; i < nBytes/4; ++i )
            {
                reinterpret_cast<GUInt32 *>(pabyBuffer)[i] =
                    (reinterpret_cast<GUInt32*>(pabyBuffer)[i] & nMask) |
                    nOffset;
            }
        }
        else
        {
            for( int i = 0; i < nBytes/4; i += nBands )
            {
                for( int j = 0; j < nBands; ++j )
                {
                    reinterpret_cast<GUInt32 *>(pabyBuffer)[i + j] =
                        (reinterpret_cast<GUInt32 *>(pabyBuffer)[i + j] &
                         anMaskLsb[j]) |
                        anOffsetLsb[j];
                }
            }
        }
    }
}

/************************************************************************/
/*                  WriteEncodedTileOrStrip()                           */
/************************************************************************/

CPLErr GTiffDataset::WriteEncodedTileOrStrip( uint32 tile_or_strip, void* data,
                                              int bPreserveDataBuffer )
{
    CPLErr eErr = CE_None;

    if( TIFFIsTiled( hTIFF ) )
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
    if( nLoadedBlock < 0 || !bLoadedBlockDirty )
        return CE_None;

    bLoadedBlockDirty = false;

    if( !SetDirectory() )
        return CE_Failure;

    const CPLErr eErr =
        WriteEncodedTileOrStrip(nLoadedBlock, pabyBlockBuf, true);
    if( eErr != CE_None )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "WriteEncodedTile/Strip() failed." );
        bWriteErrorInFlushBlockBuf = true;
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
    if( nLoadedBlock == nBlockId )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      If we have a dirty loaded block, flush it out first.            */
/* -------------------------------------------------------------------- */
    if( nLoadedBlock != -1 && bLoadedBlockDirty )
    {
        const CPLErr eErr = FlushBlockBuf();
        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Get block size.                                                 */
/* -------------------------------------------------------------------- */
    const int nBlockBufSize =
        static_cast<int>(
            TIFFIsTiled(hTIFF) ? TIFFTileSize(hTIFF) : TIFFStripSize(hTIFF));
    if( !nBlockBufSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Bogus block size; unable to allocate a buffer." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( pabyBlockBuf == NULL )
    {
        pabyBlockBuf =
            static_cast<GByte *>( VSI_CALLOC_VERBOSE( 1, nBlockBufSize ) );
        if( pabyBlockBuf == NULL )
        {
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*  When called from ::IWriteBlock in separate cases (or in single band */
/*  geotiffs), the ::IWriteBlock will override the content of the buffer*/
/*  with pImage, so we don't need to read data from disk                */
/* -------------------------------------------------------------------- */
    if( !bReadFromDisk || bStreamingOut )
    {
        nLoadedBlock = nBlockId;
        return CE_None;
    }

    // libtiff 3.X doesn't like mixing read&write of JPEG compressed blocks
    // The below hack is necessary due to another hack that consist in
    // writing zero block to force creation of JPEG tables.
    if( nBlockId == 0 && bDontReloadFirstBlock )
    {
        bDontReloadFirstBlock = false;
        memset( pabyBlockBuf, 0, nBlockBufSize );
        nLoadedBlock = nBlockId;
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      The bottom most partial tiles and strips are sometimes only     */
/*      partially encoded.  This code reduces the requested data so     */
/*      an error won't be reported in this case. (#1179)                */
/* -------------------------------------------------------------------- */
    int nBlockReqSize = nBlockBufSize;
    const int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    const int nBlockYOff = (nBlockId % nBlocksPerBand) / nBlocksPerRow;

    if( nBlockYOff * nBlockYSize > nRasterYSize - nBlockYSize )
    {
        nBlockReqSize = (nBlockBufSize / nBlockYSize)
            * (nBlockYSize - static_cast<int>(
                (static_cast<GIntBig>(nBlockYOff + 1) * nBlockYSize) %
                    nRasterYSize));
        memset( pabyBlockBuf, 0, nBlockBufSize );
    }

    WaitCompletionForBlock(nBlockId);

/* -------------------------------------------------------------------- */
/*      If we don't have this block already loaded, and we know it      */
/*      doesn't yet exist on disk, just zero the memory buffer and      */
/*      pretend we loaded it.                                           */
/* -------------------------------------------------------------------- */
    bool bErrOccured = false;
    if( !IsBlockAvailable( nBlockId, NULL, NULL, &bErrOccured ) )
    {
        memset( pabyBlockBuf, 0, nBlockBufSize );
        nLoadedBlock = nBlockId;
        if( bErrOccured )
            return CE_Failure;
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Load the block, if it isn't our current block.                  */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    if( TIFFIsTiled( hTIFF ) )
    {
        if( TIFFReadEncodedTile(hTIFF, nBlockId, pabyBlockBuf,
                                nBlockReqSize) == -1
            && !bIgnoreReadErrors )
        {
            // Once TIFFError() is properly hooked, this can go away.
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadEncodedTile() failed." );

            memset( pabyBlockBuf, 0, nBlockBufSize );

            eErr = CE_Failure;
        }
    }
    else
    {
        if( TIFFReadEncodedStrip(hTIFF, nBlockId, pabyBlockBuf,
                                 nBlockReqSize) == -1
            && !bIgnoreReadErrors )
        {
            // Once TIFFError() is properly hooked, this can go away.
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadEncodedStrip() failed." );

            memset( pabyBlockBuf, 0, nBlockBufSize );

            eErr = CE_Failure;
        }
    }

    if( eErr == CE_None )
    {
        nLoadedBlock = nBlockId;
    }
    else
    {
        nLoadedBlock = -1;
    }
    bLoadedBlockDirty = false;

    return eErr;
}

/************************************************************************/
/*                   GTiffFillStreamableOffsetAndCount()                */
/************************************************************************/

static void GTiffFillStreamableOffsetAndCount( TIFF* hTIFF, int nSize )
{
    uint32 nXSize = 0;
    uint32 nYSize = 0;
    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
    const bool bIsTiled = CPL_TO_BOOL( TIFFIsTiled(hTIFF) );
    const int nBlockCount =
        bIsTiled ? TIFFNumberOfTiles(hTIFF) : TIFFNumberOfStrips(hTIFF);

    toff_t *panOffset = NULL;
    TIFFGetField( hTIFF, bIsTiled ? TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS,
                  &panOffset );
    toff_t *panSize = NULL;
    TIFFGetField( hTIFF,
                  bIsTiled ? TIFFTAG_TILEBYTECOUNTS : TIFFTAG_STRIPBYTECOUNTS,
                  &panSize );
    toff_t nOffset = nSize;
    // Trick to avoid clang static analyzer raising false positive about
    // divide by zero later.
    int nBlocksPerBand = 1;
    uint32 nRowsPerStrip = 0;
    if( !bIsTiled )
    {
        TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP, &nRowsPerStrip);
        if( nRowsPerStrip > static_cast<uint32>(nYSize) )
            nRowsPerStrip = nYSize;
        nBlocksPerBand = DIV_ROUND_UP(nYSize, nRowsPerStrip);
    }
    for( int i = 0; i < nBlockCount; ++i )
    {
        int cc = bIsTiled ? static_cast<int>(TIFFTileSize(hTIFF)) :
                            static_cast<int>(TIFFStripSize(hTIFF));
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
    if( bCrystalized )
        return;

    // TODO: libtiff writes extended tags in the order they are specified
    // and not in increasing order.
    WriteMetadata( this, hTIFF, true, osProfile, osFilename,
                   papszCreationOptions );
    WriteGeoTIFFInfo();
    if( bNoDataSet )
        WriteNoDataValue( hTIFF, dfNoDataValue );

    bMetadataChanged = false;
    bGeoTIFFInfoChanged = false;
    bNoDataChanged = false;
    bNeedsRewrite = false;

    bCrystalized = true;

    TIFFWriteCheck( hTIFF, TIFFIsTiled(hTIFF), "GTiffDataset::Crystalize");

    // Keep zip and tiff quality, and jpegcolormode which get reset when
    // we call TIFFWriteDirectory.
    int jquality = -1;
    TIFFGetField(hTIFF, TIFFTAG_JPEGQUALITY, &jquality);
    int zquality = -1;
    TIFFGetField(hTIFF, TIFFTAG_ZIPQUALITY, &zquality);
    int nColorMode = -1;
    TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode );
    int nJpegTablesModeIn = -1;
    TIFFGetField( hTIFF, TIFFTAG_JPEGTABLESMODE, &nJpegTablesModeIn );

    TIFFWriteDirectory( hTIFF );
    if( bStreamingOut )
    {
        // We need to write twice the directory to be sure that custom
        // TIFF tags are correctly sorted and that padding bytes have been
        // added.
        TIFFSetDirectory( hTIFF, 0 );
        TIFFWriteDirectory( hTIFF );

        if( VSIFSeekL( fpL, 0, SEEK_END ) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Could not seek");
        }
        const int nSize = static_cast<int>( VSIFTellL(fpL) );

        TIFFSetDirectory( hTIFF, 0 );
        GTiffFillStreamableOffsetAndCount( hTIFF, nSize );
        TIFFWriteDirectory( hTIFF );

        vsi_l_offset nDataLength = 0;
        void* pabyBuffer =
            VSIGetMemFileBuffer( osTmpFilename, &nDataLength, FALSE);
        if( static_cast<int>(
                VSIFWriteL( pabyBuffer, 1,
                            static_cast<int>(nDataLength), fpToWrite ) ) !=
            static_cast<int>(nDataLength) )
        {
            CPLError( CE_Failure, CPLE_FileIO, "Could not write %d bytes",
                      static_cast<int>(nDataLength) );
        }
        // In case of single strip file, there's a libtiff check that would
        // issue a warning since the file hasn't the required size.
        CPLPushErrorHandler(CPLQuietErrorHandler);
        TIFFSetDirectory( hTIFF, 0 );
        CPLPopErrorHandler();
    }
    else
    {
        TIFFSetDirectory( hTIFF, 0 );
    }

    // Now, reset zip and tiff quality and jpegcolormode.
    if( jquality > 0 )
        TIFFSetField(hTIFF, TIFFTAG_JPEGQUALITY, jquality);
    if( zquality > 0 )
        TIFFSetField(hTIFF, TIFFTAG_ZIPQUALITY, zquality);
    if( nColorMode >= 0 )
        TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, nColorMode);
    if( nJpegTablesModeIn >= 0 )
        TIFFSetField(hTIFF, TIFFTAG_JPEGTABLESMODE, nJpegTablesModeIn);

    nDirOffset = TIFFCurrentDirOffset( hTIFF );
}

#if defined(INTERNAL_LIBTIFF) && defined(DEFER_STRILE_LOAD)

static
bool GTiffCacheOffsetOrCount( VSILFILE* fp,
                              bool bSwab,
                              vsi_l_offset nBaseOffset,
                              int nBlockId,
                              uint32 nstrips,
                              uint64* panVals,
                              size_t sizeofval )
{
    static const vsi_l_offset IO_CACHE_PAGE_SIZE = 4096;

    const int sizeofvalint = static_cast<int>(sizeofval);
    const vsi_l_offset nOffset = nBaseOffset + sizeofval * nBlockId;
    const vsi_l_offset nOffsetStartPage =
        (nOffset / IO_CACHE_PAGE_SIZE) * IO_CACHE_PAGE_SIZE;
    vsi_l_offset nOffsetEndPage = nOffsetStartPage + IO_CACHE_PAGE_SIZE;

    if( nOffset + sizeofval > nOffsetEndPage )
        nOffsetEndPage += IO_CACHE_PAGE_SIZE;
    vsi_l_offset nLastStripOffset = nBaseOffset + nstrips * sizeofval;
    if( nLastStripOffset < nOffsetEndPage )
        nOffsetEndPage = nLastStripOffset;
    if( nOffsetStartPage >= nOffsetEndPage )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read offset/size for strile %d", nBlockId);
        panVals[nBlockId] = 0;
        return false;
    }
    if( VSIFSeekL(fp, nOffsetStartPage, SEEK_SET) != 0 )
    {
        panVals[nBlockId] = 0;
        return false;
    }

    const size_t nToRead =
        static_cast<size_t>(nOffsetEndPage - nOffsetStartPage);
    GByte buffer[2 * IO_CACHE_PAGE_SIZE] = {};  // TODO(schwehr): Off the stack.
    const size_t nRead = VSIFReadL(buffer, 1, nToRead, fp);
    if( nRead < nToRead )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read offset/size for strile around ~%d", nBlockId);
        return false;
    }
    int iStartBefore =
        - static_cast<int>((nOffset - nOffsetStartPage) / sizeofval);
    if( nBlockId + iStartBefore < 0 )
        iStartBefore = -nBlockId;
    for( int i = iStartBefore;
         static_cast<uint32>(nBlockId + i) < nstrips &&
         static_cast<GIntBig>(nOffset) + (i + 1) * sizeofvalint <=
         static_cast<GIntBig>(nOffsetEndPage);
         ++i )
    {
        if( sizeofval == 2 )
        {
            uint16 val;
            memcpy(&val,
                   buffer + (nOffset - nOffsetStartPage) + i * sizeofvalint,
                   sizeof(val));
            if( bSwab )
                CPL_SWAP16PTR(&val);
            panVals[nBlockId + i] = val;
        }
        else if( sizeofval == 4 )
        {
            uint32 val;
            memcpy(&val,
                   buffer + (nOffset - nOffsetStartPage) + i * sizeofvalint,
                   sizeof(val));
            if( bSwab )
                CPL_SWAP32PTR(&val);
            panVals[nBlockId + i] = val;
        }
        else
        {
            uint64 val;
            memcpy(&val,
                   buffer + (nOffset - nOffsetStartPage) + i * sizeofvalint,
                   sizeof(val));
            if( bSwab )
                CPL_SWAP64PTR(&val);
            panVals[nBlockId + i] = val;
        }
    }
    return true;
}

static bool ReadStripArray( VSILFILE* fp,
                            TIFF* hTIFF,
                            const TIFFDirEntry* psEntry,
                            int nBlockId,
                            uint32 nStripArrayAlloc,
                            uint64* panOffsetOrCountArray )
{
    const bool bSwab = (hTIFF->tif_flags & TIFF_SWAB) != 0;
    if( (hTIFF->tif_flags&TIFF_BIGTIFF) &&
        psEntry->tdir_type == TIFF_SHORT &&
        psEntry->tdir_count <= 4 )
    {
        uint16 offset;
        const GByte* src = reinterpret_cast<const GByte*>(
                                    &(psEntry->tdir_offset.toff_long8));
        for( size_t i = 0; i < 4 && i < nStripArrayAlloc; i++ )
        {
            memcpy(&offset, src + sizeof(offset) * i, sizeof(offset));
            if( bSwab )
                CPL_SWAP16PTR(&offset);
            panOffsetOrCountArray[i] = offset;
        }
        return true;
    }
    else if( (hTIFF->tif_flags&TIFF_BIGTIFF) &&
        psEntry->tdir_type == TIFF_LONG &&
        psEntry->tdir_count <= 2 )
    {
        uint32 offset;
        const GByte* src = reinterpret_cast<const GByte*>(
                                    &(psEntry->tdir_offset.toff_long8));
        for( size_t i = 0; i < 2 && i < nStripArrayAlloc; i++ )
        {
            memcpy(&offset, src + sizeof(offset) * i, sizeof(offset));
            if( bSwab )
                CPL_SWAP32PTR(&offset);
            panOffsetOrCountArray[i] = offset;
        }
        return true;
    }
    else if( (hTIFF->tif_flags&TIFF_BIGTIFF) &&
        psEntry->tdir_type == TIFF_LONG8 &&
        psEntry->tdir_count <= 1 )
    {
        uint64 offset = psEntry->tdir_offset.toff_long8;
        if( bSwab )
            CPL_SWAP64PTR(&offset);
        panOffsetOrCountArray[0] = offset;
        return true;
    }
    else if( !(hTIFF->tif_flags&TIFF_BIGTIFF) &&
        psEntry->tdir_type == TIFF_SHORT &&
        psEntry->tdir_count <= 2 )
    {
        uint16 offset;
        const GByte* src = reinterpret_cast<const GByte*>(
                                    &(psEntry->tdir_offset.toff_long));

        for( size_t i = 0; i < 2 && i < nStripArrayAlloc; i++ )
        {
            memcpy(&offset, src + sizeof(offset) * i, sizeof(offset));
            if( bSwab )
                CPL_SWAP16PTR(&offset);
            panOffsetOrCountArray[i] = offset;
        }
        return true;
    }
    else if( !(hTIFF->tif_flags&TIFF_BIGTIFF) &&
        psEntry->tdir_type == TIFF_LONG &&
        psEntry->tdir_count <= 1 )
    {
        uint32 offset = psEntry->tdir_offset.toff_long;
        if( bSwab )
            CPL_SWAP32PTR(&offset);
        panOffsetOrCountArray[0] = offset;
        return true;
    }
    else
    {
        vsi_l_offset l_nDirOffset = 0;
        if( hTIFF->tif_flags&TIFF_BIGTIFF )
        {
            uint64 offset = psEntry->tdir_offset.toff_long8;
            if( bSwab )
                CPL_SWAP64PTR(&offset);
            l_nDirOffset = offset;
        }
        else
        {
            uint32 offset = psEntry->tdir_offset.toff_long;
            if( bSwab )
                CPL_SWAP32PTR(&offset);
            l_nDirOffset = offset;
        }

        if( psEntry->tdir_type == TIFF_SHORT )
        {
            return GTiffCacheOffsetOrCount(fp,
                                    bSwab,
                                    l_nDirOffset,
                                    nBlockId,
                                    nStripArrayAlloc,
                                    panOffsetOrCountArray,
                                    sizeof(uint16));
        }
        else if( psEntry->tdir_type == TIFF_LONG )
        {
            return GTiffCacheOffsetOrCount(fp,
                                    bSwab,
                                    l_nDirOffset,
                                    nBlockId,
                                    nStripArrayAlloc,
                                    panOffsetOrCountArray,
                                    sizeof(uint32));
        }
        else
        {
            return GTiffCacheOffsetOrCount(fp,
                                    bSwab,
                                    l_nDirOffset,
                                    nBlockId,
                                    nStripArrayAlloc,
                                    panOffsetOrCountArray,
                                    sizeof(uint64));
        }
    }
}

#endif  // #if defined(INTERNAL_LIBTIFF) && defined(DEFER_STRILE_LOAD)

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
                                     bool *pbErrOccured )

{
    if( pbErrOccured )
        *pbErrOccured = false;

#if defined(INTERNAL_LIBTIFF) && defined(DEFER_STRILE_LOAD)
    // Optimization to avoid fetching the whole Strip/TileCounts and
    // Strip/TileOffsets arrays.

    // Note: if strip choping is in effect, _TIFFFillStrilesInternal()
    // will have 0-memset td_stripoffset_entry/td_stripbytecount_entry, so
    // we won't enter the below block

    if( eAccess == GA_ReadOnly &&
        hTIFF->tif_dir.td_stripoffset_entry.tdir_tag != 0 &&
        hTIFF->tif_dir.td_stripbytecount_entry.tdir_tag != 0 &&
        !bStreamingIn )
    {
        if( !((hTIFF->tif_dir.td_stripoffset_entry.tdir_type == TIFF_SHORT ||
               hTIFF->tif_dir.td_stripoffset_entry.tdir_type == TIFF_LONG ||
               hTIFF->tif_dir.td_stripoffset_entry.tdir_type == TIFF_LONG8) &&
              (hTIFF->tif_dir.td_stripbytecount_entry.tdir_type == TIFF_SHORT ||
               hTIFF->tif_dir.td_stripbytecount_entry.tdir_type == TIFF_LONG ||
               hTIFF->tif_dir.td_stripbytecount_entry.tdir_type == TIFF_LONG8)) )
        {
            if( nStripArrayAlloc == 0 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unhandled type for StripOffset/StripByteCount");
                nStripArrayAlloc = ~nStripArrayAlloc;
            }
            if( pnOffset )
                *pnOffset = 0;
            if( pnSize )
                *pnSize = 0;
            if( pbErrOccured )
                *pbErrOccured = true;
            return false;
        }

        // The size of tags can be actually lesser than the number of strips
        // (libtiff accepts such files)
        if( static_cast<uint32>(nBlockId) >=
                hTIFF->tif_dir.td_stripoffset_entry.tdir_count ||
            static_cast<uint32>(nBlockId) >=
                hTIFF->tif_dir.td_stripbytecount_entry.tdir_count )
        {
            // In case the tags aren't large enough.
            if( pnOffset )
                *pnOffset = 0;
            if( pnSize )
                *pnSize = 0;
            if( pbErrOccured )
                *pbErrOccured = true;
            return false;
        }

        if( hTIFF->tif_dir.td_stripoffset == NULL )
        {
            nStripArrayAlloc = 0;
        }
        if( static_cast<uint32>(nBlockId) >= nStripArrayAlloc )
        {
            if( nBlockId > 1000000 )
            {
                // Avoid excessive memory allocation attempt
                if( m_nFileSize == 0 )
                {
                    VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( hTIFF ));
                    const vsi_l_offset nCurOffset = VSIFTellL(fp);
                    CPL_IGNORE_RET_VAL( VSIFSeekL(fp, 0, SEEK_END) );
                    m_nFileSize = VSIFTellL(fp);
                    CPL_IGNORE_RET_VAL( VSIFSeekL(fp, nCurOffset, SEEK_SET) );
                }
                // For such a big blockid we need at least a TIFF_LONG
                if( static_cast<vsi_l_offset>(nBlockId) >
                                        m_nFileSize / (2 * sizeof(GUInt32)) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "File too short");
                    if( pnOffset )
                        *pnOffset = 0;
                    if( pnSize )
                        *pnSize = 0;
                    if( pbErrOccured )
                        *pbErrOccured = true;
                    return false;
                }
            }

            uint32 nStripArrayAllocBefore = nStripArrayAlloc;
            uint32 nStripArrayAllocNew;
            if( nStripArrayAlloc == 0 &&
                hTIFF->tif_dir.td_nstrips < 1024 * 1024 )
            {
                nStripArrayAllocNew = hTIFF->tif_dir.td_nstrips;
            }
            else
            {
                nStripArrayAllocNew = std::max(
                    static_cast<uint32>(nBlockId) + 1, 1024U * 512U );
                if( nStripArrayAllocNew < UINT_MAX / 2  )
                    nStripArrayAllocNew *= 2;
                nStripArrayAllocNew = std::min(
                    nStripArrayAllocNew, hTIFF->tif_dir.td_nstrips);
            }
            CPLAssert( static_cast<uint32>(nBlockId) < nStripArrayAllocNew );
            const uint64 nArraySize64 =
                static_cast<uint64>(sizeof(uint64)) * nStripArrayAllocNew;
            const size_t nArraySize = static_cast<size_t>(nArraySize64);
#if SIZEOF_VOIDP == 4
            if( nArraySize != nArraySize64 )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate strip offset and bytecount arrays");
                if( pbErrOccured )
                    *pbErrOccured = true;
                return false;
            }
#endif
            uint64* offsetArray = static_cast<uint64 *>(
                _TIFFrealloc( hTIFF->tif_dir.td_stripoffset, nArraySize ) );
            uint64* bytecountArray = static_cast<uint64 *>(
                _TIFFrealloc( hTIFF->tif_dir.td_stripbytecount, nArraySize ) );
            if( offsetArray )
                hTIFF->tif_dir.td_stripoffset = offsetArray;
            if( bytecountArray )
                hTIFF->tif_dir.td_stripbytecount = bytecountArray;
            if( offsetArray && bytecountArray )
            {
                nStripArrayAlloc = nStripArrayAllocNew;
                memset(hTIFF->tif_dir.td_stripoffset + nStripArrayAllocBefore,
                    0xFF,
                    (nStripArrayAlloc - nStripArrayAllocBefore) * sizeof(uint64) );
                memset(hTIFF->tif_dir.td_stripbytecount + nStripArrayAllocBefore,
                    0xFF,
                    (nStripArrayAlloc - nStripArrayAllocBefore) * sizeof(uint64) );
            }
            else
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate strip offset and bytecount arrays");
                _TIFFfree(hTIFF->tif_dir.td_stripoffset);
                hTIFF->tif_dir.td_stripoffset = NULL;
                _TIFFfree(hTIFF->tif_dir.td_stripbytecount);
                hTIFF->tif_dir.td_stripbytecount = NULL;
                nStripArrayAlloc = 0;
            }
        }
        if( hTIFF->tif_dir.td_stripbytecount == NULL )
        {
            if( pbErrOccured )
                *pbErrOccured = true;
            return false;
        }
        if( ~(hTIFF->tif_dir.td_stripoffset[nBlockId]) == 0 ||
            ~(hTIFF->tif_dir.td_stripbytecount[nBlockId]) == 0 )
        {
            VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( hTIFF ));
            const vsi_l_offset nCurOffset = VSIFTellL(fp);
            if( ~(hTIFF->tif_dir.td_stripoffset[nBlockId]) == 0 )
            {
                if( !ReadStripArray( fp,
                                hTIFF,
                                &hTIFF->tif_dir.td_stripoffset_entry,
                                nBlockId,
                                nStripArrayAlloc,
                                hTIFF->tif_dir.td_stripoffset ) )
                {
                    if( pbErrOccured )
                        *pbErrOccured = true;
                    return false;
                }
            }

            if( ~(hTIFF->tif_dir.td_stripbytecount[nBlockId]) == 0 )
            {
                if( !ReadStripArray( fp,
                                hTIFF,
                                &hTIFF->tif_dir.td_stripbytecount_entry,
                                nBlockId,
                                nStripArrayAlloc,
                                hTIFF->tif_dir.td_stripbytecount ) )
                {
                    if( pbErrOccured )
                        *pbErrOccured = true;
                    return false;
                }
            }
            if( VSIFSeekL(fp, nCurOffset, SEEK_SET) != 0 )
            {
                // For some reason Coverity reports:
                // Value of non-local "this->hTIFF->tif_dir.td_stripoffset"
                // that was verified to be "NULL" is not restored as it was
                // along other paths.
                // coverity[end_of_path]
                if( pbErrOccured )
                    *pbErrOccured = true;
                return false;
            }
        }
        if( pnOffset )
            *pnOffset = hTIFF->tif_dir.td_stripoffset[nBlockId];
        if( pnSize )
            *pnSize = hTIFF->tif_dir.td_stripbytecount[nBlockId];
        return hTIFF->tif_dir.td_stripbytecount[nBlockId] != 0;
    }
#endif  // defined(INTERNAL_LIBTIFF) && defined(DEFER_STRILE_LOAD)
    toff_t *panByteCounts = NULL;
    toff_t *panOffsets = NULL;
    const bool bIsTiled = CPL_TO_BOOL( TIFFIsTiled(hTIFF) );

    if( ( bIsTiled
          && TIFFGetField( hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts )
          && (pnOffset == NULL ||
              TIFFGetField( hTIFF, TIFFTAG_TILEOFFSETS, &panOffsets )) )
        || ( !bIsTiled
          && TIFFGetField( hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts )
          && (pnOffset == NULL ||
              TIFFGetField( hTIFF, TIFFTAG_STRIPOFFSETS, &panOffsets )) ) )
    {
        if( panByteCounts == NULL || (pnOffset != NULL && panOffsets == NULL) )
        {
            if( pbErrOccured )
                *pbErrOccured = true;
            return false;
        }
        const int nBlockCount =
            bIsTiled ? TIFFNumberOfTiles(hTIFF) : TIFFNumberOfStrips(hTIFF);
        if( nBlockId >= nBlockCount )
        {
            if( pbErrOccured )
                *pbErrOccured = true;
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
        if( pbErrOccured )
            *pbErrOccured = true;
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
    if( bIsFinalized || ppoActiveDSRef == NULL )
        return;

    GDALPamDataset::FlushCache();

    if( bLoadedBlockDirty && nLoadedBlock != -1 )
        FlushBlockBuf();

    CPLFree( pabyBlockBuf );
    pabyBlockBuf = NULL;
    nLoadedBlock = -1;
    bLoadedBlockDirty = false;

    // Finish compression
    if( poCompressThreadPool )
    {
        poCompressThreadPool->WaitCompletion();

        // Flush remaining data
        for( int i = 0; i < static_cast<int>(asCompressionJobs.size()); ++i )
        {
            if( asCompressionJobs[i].bReady )
            {
                if( asCompressionJobs[i].nCompressedBufferSize )
                {
                    WriteRawStripOrTile( asCompressionJobs[i].nStripOrTile,
                                   asCompressionJobs[i].pabyCompressedBuffer,
                                   asCompressionJobs[i].nCompressedBufferSize );
                }
                asCompressionJobs[i].pabyCompressedBuffer = NULL;
                asCompressionJobs[i].nBufferSize = 0;
                asCompressionJobs[i].bReady = false;
                asCompressionJobs[i].nStripOrTile = -1;
            }
        }
    }

    if( bFlushDirectory )
    {
        if( !SetDirectory() )
            return;
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
        if( bMetadataChanged )
        {
            if( !SetDirectory() )
                return;
            bNeedsRewrite =
                    WriteMetadata( this, hTIFF, true, osProfile, osFilename,
                                   papszCreationOptions );
            bMetadataChanged = false;
        }

        if( bGeoTIFFInfoChanged )
        {
            if( !SetDirectory() )
                return;
            WriteGeoTIFFInfo();
        }

        if( bNoDataChanged )
        {
            if( !SetDirectory() )
                return;
            if( bNoDataSet )
            {
                WriteNoDataValue( hTIFF, dfNoDataValue );
            }
            else
            {
                UnsetNoDataValue( hTIFF );
            }
            bNeedsRewrite = true;
            bNoDataChanged = false;
        }

        if( bNeedsRewrite )
        {
#if defined(TIFFLIB_VERSION)
#if defined(HAVE_TIFFGETSIZEPROC)
            if( !SetDirectory() )
                return;

            const TIFFSizeProc pfnSizeProc = TIFFGetSizeProc( hTIFF );

            nDirOffset = pfnSizeProc( TIFFClientdata( hTIFF ) );
            if( (nDirOffset % 2) == 1 )
                ++nDirOffset;

            TIFFRewriteDirectory( hTIFF );

            TIFFSetSubDirectory( hTIFF, nDirOffset );
#elif TIFFLIB_VERSION > 20010925 && TIFFLIB_VERSION != 20011807
            if( !SetDirectory() )
                return;

            TIFFRewriteDirectory( hTIFF );
#endif
#endif
            bNeedsRewrite = false;
        }
    }

    // There are some circumstances in which we can reach this point
    // without having made this our directory (SetDirectory()) in which
    // case we should not risk a flush.
    if( GetAccess() == GA_Update && TIFFCurrentDirOffset(hTIFF) == nDirOffset )
    {
#if defined(BIGTIFF_SUPPORT)
        const TIFFSizeProc pfnSizeProc = TIFFGetSizeProc( hTIFF );

        toff_t nNewDirOffset = pfnSizeProc( TIFFClientdata( hTIFF ) );
        if( (nNewDirOffset % 2) == 1 )
            ++nNewDirOffset;

        TIFFFlush( hTIFF );

        if( nDirOffset != TIFFCurrentDirOffset( hTIFF ) )
        {
            nDirOffset = nNewDirOffset;
            CPLDebug( "GTiff",
                      "directory moved during flush in FlushDirectory()" );
        }
#else
        // For libtiff 3.X, the above causes regressions and crashes in
        // tiff_write.py and tiff_ovr.py.
        TIFFFlush( hTIFF );
#endif
    }
}

/************************************************************************/
/*                           CleanOverviews()                           */
/************************************************************************/

CPLErr GTiffDataset::CleanOverviews()

{
    CPLAssert( bBase );

    ScanDirectories();

    FlushDirectory();
    *ppoActiveDSRef = NULL;

/* -------------------------------------------------------------------- */
/*      Cleanup overviews objects, and get offsets to all overview      */
/*      directories.                                                    */
/* -------------------------------------------------------------------- */
    std::vector<toff_t> anOvDirOffsets;

    for( int i = 0; i < nOverviewCount; ++i )
    {
        anOvDirOffsets.push_back( papoOverviewDS[i]->nDirOffset );
        delete papoOverviewDS[i];
    }

/* -------------------------------------------------------------------- */
/*      Loop through all the directories, translating the offsets       */
/*      into indexes we can use with TIFFUnlinkDirectory().             */
/* -------------------------------------------------------------------- */
    std::vector<uint16> anOvDirIndexes;
    int iThisOffset = 1;

    TIFFSetDirectory( hTIFF, 0 );

    while( true )
    {
        for( int i = 0; i < nOverviewCount; ++i )
        {
            if( anOvDirOffsets[i] == TIFFCurrentDirOffset( hTIFF ) )
            {
                CPLDebug( "GTiff", "%d -> %d",
                          static_cast<int>(anOvDirOffsets[i]), iThisOffset );
                anOvDirIndexes.push_back( static_cast<uint16>(iThisOffset) );
            }
        }

        if( TIFFLastDirectory( hTIFF ) )
            break;

        TIFFReadDirectory( hTIFF );
        ++iThisOffset;
    }

/* -------------------------------------------------------------------- */
/*      Actually unlink the target directories.  Note that we do        */
/*      this from last to first so as to avoid renumbering any of       */
/*      the earlier directories we need to remove.                      */
/* -------------------------------------------------------------------- */
    while( !anOvDirIndexes.empty() )
    {
        TIFFUnlinkDirectory( hTIFF, anOvDirIndexes.back() );
        anOvDirIndexes.pop_back();
    }

    CPLFree( papoOverviewDS );

    nOverviewCount = 0;
    papoOverviewDS = NULL;

    if( !SetDirectory() )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                   RegisterNewOverviewDataset()                       */
/************************************************************************/

CPLErr GTiffDataset::RegisterNewOverviewDataset(toff_t nOverviewOffset,
                                                int l_nJpegQuality)
{
    GTiffDataset* poODS = new GTiffDataset();
    poODS->nJpegQuality = l_nJpegQuality;
    poODS->nZLevel = nZLevel;
    poODS->nLZMAPreset = nLZMAPreset;
    poODS->nJpegTablesMode = nJpegTablesMode;

    if( poODS->OpenOffset( hTIFF, ppoActiveDSRef, nOverviewOffset, false,
                            GA_Update ) != CE_None )
    {
        delete poODS;
        return CE_Failure;
    }

    ++nOverviewCount;
    papoOverviewDS = static_cast<GTiffDataset **>(
        CPLRealloc( papoOverviewDS,
                    nOverviewCount * (sizeof(void*))) );
    papoOverviewDS[nOverviewCount-1] = poODS;
    poODS->poBaseDS = this;
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

            anTRed[iColor] = static_cast<unsigned short>(256 * sRGB.c1);
            anTGreen[iColor] = static_cast<unsigned short>(256 * sRGB.c2);
            anTBlue[iColor] = static_cast<unsigned short>(256 * sRGB.c3);
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

CPLErr GTiffDataset::CreateOverviewsFromSrcOverviews(GDALDataset* poSrcDS)
{
    CPLAssert(poSrcDS->GetRasterCount() != 0);
    CPLAssert(nOverviewCount == 0);

    ScanDirectories();

/* -------------------------------------------------------------------- */
/*      Move to the directory for this dataset.                         */
/* -------------------------------------------------------------------- */
    if( !SetDirectory() )
        return CE_Failure;
    FlushDirectory();

    int nOvBitsPerSample = nBitsPerSample;

/* -------------------------------------------------------------------- */
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    std::vector<unsigned short> anTRed;
    std::vector<unsigned short> anTGreen;
    std::vector<unsigned short> anTBlue;
    unsigned short *panRed = NULL;
    unsigned short *panGreen = NULL;
    unsigned short *panBlue = NULL;

    if( nPhotometric == PHOTOMETRIC_PALETTE && poColorTable != NULL )
    {
        CreateTIFFColorTable(poColorTable, nOvBitsPerSample,
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
    uint16 *panExtraSampleValues = NULL;
    uint16 nExtraSamples = 0;

    if( TIFFGetField( hTIFF, TIFFTAG_EXTRASAMPLES, &nExtraSamples,
                      &panExtraSampleValues) )
    {
        uint16* panExtraSampleValuesNew =
            static_cast<uint16*>(
                CPLMalloc(nExtraSamples * sizeof(uint16)) );
        memcpy( panExtraSampleValuesNew, panExtraSampleValues,
                nExtraSamples * sizeof(uint16));
        panExtraSampleValues = panExtraSampleValuesNew;
    }
    else
    {
        panExtraSampleValues = NULL;
        nExtraSamples = 0;
    }

/* -------------------------------------------------------------------- */
/*      Fetch predictor tag                                             */
/* -------------------------------------------------------------------- */
    uint16 nPredictor = PREDICTOR_NONE;
    if( nCompression == COMPRESSION_LZW ||
        nCompression == COMPRESSION_ADOBE_DEFLATE )
        TIFFGetField( hTIFF, TIFFTAG_PREDICTOR, &nPredictor );
    int nOvrBlockXSize = 0;
    int nOvrBlockYSize = 0;
    GTIFFGetOverviewBlockSize(&nOvrBlockXSize, &nOvrBlockYSize);

    int nSrcOverviews = poSrcDS->GetRasterBand(1)->GetOverviewCount();
    CPLErr eErr = CE_None;

    for( int i = 0; i < nSrcOverviews && eErr == CE_None; ++i )
    {
        GDALRasterBand* poOvrBand = poSrcDS->GetRasterBand(1)->GetOverview(i);

        int nOXSize = poOvrBand->GetXSize();
        int nOYSize = poOvrBand->GetYSize();

        int nOvrJpegQuality = nJpegQuality;
        if( nCompression == COMPRESSION_JPEG &&
            CPLGetConfigOption( "JPEG_QUALITY_OVERVIEW", NULL ) != NULL )
        {
            nOvrJpegQuality =
                atoi(CPLGetConfigOption("JPEG_QUALITY_OVERVIEW","75"));
        }

        CPLString osNoData; // don't move this in inner scope
        const char* pszNoData = NULL;
        if( bNoDataSet )
        {
            osNoData = GTiffFormatGDALNoDataTagValue(dfNoDataValue);
            pszNoData = osNoData.c_str();
        }

        toff_t nOverviewOffset =
                GTIFFWriteDirectory(hTIFF, FILETYPE_REDUCEDIMAGE,
                                    nOXSize, nOYSize,
                                    nOvBitsPerSample, nPlanarConfig,
                                    nSamplesPerPixel,
                                    nOvrBlockXSize,
                                    nOvrBlockYSize,
                                    TRUE,
                                    nCompression, nPhotometric, nSampleFormat,
                                    nPredictor,
                                    panRed, panGreen, panBlue,
                                    nExtraSamples, panExtraSampleValues,
                                    osMetadata,
                                    nOvrJpegQuality >= 0 ?
                                        CPLSPrintf("%d", nOvrJpegQuality) : NULL,
                                    CPLSPrintf("%d", nJpegTablesMode),
                                    pszNoData
                                   );

        if( nOverviewOffset == 0 )
            eErr = CE_Failure;
        else
            eErr = RegisterNewOverviewDataset(nOverviewOffset, nOvrJpegQuality);
    }

    CPLFree(panExtraSampleValues);
    panExtraSampleValues = NULL;

/* -------------------------------------------------------------------- */
/*      Create overviews for the mask.                                  */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
        eErr = CreateInternalMaskOverviews(nOvrBlockXSize, nOvrBlockYSize);

    return eErr;
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
        CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", NULL);
    if( poMaskDS != NULL &&
        poMaskDS->GetRasterCount() == 1 &&
        (pszInternalMask == NULL || CPLTestBool(pszInternalMask)) )
    {
        int nMaskOvrCompression;
        if( strstr(GDALGetMetadataItem(GDALGetDriverByName( "GTiff" ),
                                       GDAL_DMD_CREATIONOPTIONLIST, NULL ),
                   "<Value>DEFLATE</Value>") != NULL )
            nMaskOvrCompression = COMPRESSION_ADOBE_DEFLATE;
        else
            nMaskOvrCompression = COMPRESSION_PACKBITS;

        for( int i = 0; i < nOverviewCount; ++i )
        {
            if( papoOverviewDS[i]->poMaskDS == NULL )
            {
                const toff_t nOverviewOffset =
                    GTIFFWriteDirectory(
                        hTIFF, FILETYPE_REDUCEDIMAGE | FILETYPE_MASK,
                        papoOverviewDS[i]->nRasterXSize,
                        papoOverviewDS[i]->nRasterYSize,
                        1, PLANARCONFIG_CONTIG,
                        1, nOvrBlockXSize, nOvrBlockYSize, TRUE,
                        nMaskOvrCompression, PHOTOMETRIC_MASK,
                        SAMPLEFORMAT_UINT, PREDICTOR_NONE,
                        NULL, NULL, NULL, 0, NULL,
                        "",
                        NULL, NULL, NULL );

                if( nOverviewOffset == 0 )
                {
                    eErr = CE_Failure;
                    continue;
                }

                GTiffDataset *poODS = new GTiffDataset();
                if( poODS->OpenOffset( hTIFF, ppoActiveDSRef,
                                       nOverviewOffset, false,
                                       GA_Update ) != CE_None )
                {
                    delete poODS;
                    eErr = CE_Failure;
                }
                else
                {
                    poODS->bPromoteTo8Bits =
                        CPLTestBool(
                            CPLGetConfigOption(
                                "GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES" ) );
                    poODS->poBaseDS = this;
                    papoOverviewDS[i]->poMaskDS = poODS;
                    ++poMaskDS->nOverviewCount;
                    poMaskDS->papoOverviewDS = static_cast<GTiffDataset **>(
                        CPLRealloc(
                            poMaskDS->papoOverviewDS,
                            poMaskDS->nOverviewCount * (sizeof(void*))) );
                    poMaskDS->papoOverviewDS[poMaskDS->nOverviewCount-1] =
                        poODS;
                }
            }
        }
    }

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
    // has the right to do that.  Behaviour maybe undefined in GDAL API.
    nJPEGOverviewCount = 0;

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
        if( nOverviewCount != 0 )
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Cannot add external overviews when there are already "
                "internal overviews" );
            return CE_Failure;
        }

        return GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList,
            nBandsIn, panBandList, pfnProgress, pProgressData );
    }

/* -------------------------------------------------------------------- */
/*      Our TIFF overview support currently only works safely if all    */
/*      bands are handled at the same time.                             */
/* -------------------------------------------------------------------- */
    if( nBandsIn != GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
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
        if( nOverviewCount == 0 )
            return GDALDataset::IBuildOverviews(
                pszResampling, nOverviews, panOverviewList,
                nBandsIn, panBandList, pfnProgress, pProgressData );

        return CleanOverviews();
    }

/* -------------------------------------------------------------------- */
/*      libtiff 3.X has issues when generating interleaved overviews.   */
/*      so generate them one after another one.                         */
/* -------------------------------------------------------------------- */

    CPLErr eErr = CE_None;

#ifndef BIGTIFF_SUPPORT
    if( nOverviews > 1 )
    {
        double* padfOvrRasterFactor =
            static_cast<double*>( CPLMalloc(sizeof(double) * nOverviews) );
        double dfTotal = 0;
        for( int i = 0; i < nOverviews; ++i )
        {
            if( panOverviewList[i] <= 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid overview factor : %d", panOverviewList[i]);
                eErr = CE_Failure;
                break;
            }
            padfOvrRasterFactor[i] =
                1.0 / (panOverviewList[i] * panOverviewList[i]);
            dfTotal += padfOvrRasterFactor[i];
        }

        double dfAcc = 0.0;
        for( int i = 0; i < nOverviews && eErr == CE_None; ++i )
        {
            void *pScaledProgressData =
                GDALCreateScaledProgress(
                    dfAcc / dfTotal,
                    (dfAcc + padfOvrRasterFactor[i]) / dfTotal,
                    pfnProgress, pProgressData );
            dfAcc += padfOvrRasterFactor[i];

            eErr = IBuildOverviews(
                pszResampling, 1, &panOverviewList[i],
                nBandsIn, panBandList, GDALScaledProgress,
                pScaledProgressData );

            GDALDestroyScaledProgress(pScaledProgressData);
        }

        CPLFree(padfOvrRasterFactor);

        return eErr;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Move to the directory for this dataset.                         */
/* -------------------------------------------------------------------- */
    if( !SetDirectory() )
        return CE_Failure;
    FlushDirectory();

/* -------------------------------------------------------------------- */
/*      If we are averaging bit data to grayscale we need to create     */
/*      8bit overviews.                                                 */
/* -------------------------------------------------------------------- */
    int nOvBitsPerSample = nBitsPerSample;

    if( STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2") )
        nOvBitsPerSample = 8;

/* -------------------------------------------------------------------- */
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    std::vector<unsigned short> anTRed;
    std::vector<unsigned short> anTGreen;
    std::vector<unsigned short> anTBlue;
    unsigned short *panRed = NULL;
    unsigned short *panGreen = NULL;
    unsigned short *panBlue = NULL;

    if( nPhotometric == PHOTOMETRIC_PALETTE && poColorTable != NULL )
    {
        CreateTIFFColorTable(poColorTable, nOvBitsPerSample,
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
    uint16 *panExtraSampleValues = NULL;
    uint16 nExtraSamples = 0;

    if( TIFFGetField( hTIFF, TIFFTAG_EXTRASAMPLES, &nExtraSamples,
                      &panExtraSampleValues) )
    {
        uint16* panExtraSampleValuesNew =
            static_cast<uint16*>( CPLMalloc(nExtraSamples * sizeof(uint16)) );
        memcpy( panExtraSampleValuesNew, panExtraSampleValues,
                nExtraSamples * sizeof(uint16) );
        panExtraSampleValues = panExtraSampleValuesNew;
    }
    else
    {
        panExtraSampleValues = NULL;
        nExtraSamples = 0;
    }

/* -------------------------------------------------------------------- */
/*      Fetch predictor tag                                             */
/* -------------------------------------------------------------------- */
    uint16 nPredictor = PREDICTOR_NONE;
    if( nCompression == COMPRESSION_LZW ||
        nCompression == COMPRESSION_ADOBE_DEFLATE )
        TIFFGetField( hTIFF, TIFFTAG_PREDICTOR, &nPredictor );

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    int nOvrBlockXSize = 0;
    int nOvrBlockYSize = 0;
    GTIFFGetOverviewBlockSize(&nOvrBlockXSize, &nOvrBlockYSize);
    for( int i = 0; i < nOverviews && eErr == CE_None; ++i )
    {
        for( int j = 0; j < nOverviewCount && eErr == CE_None; ++j )
        {
            GTiffDataset *poODS = papoOverviewDS[j];

            const int nOvFactor =
                GDALComputeOvFactor(poODS->GetRasterXSize(),
                                    GetRasterXSize(),
                                    poODS->GetRasterYSize(),
                                    GetRasterYSize());

            if( nOvFactor == panOverviewList[i]
                || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                                    GetRasterXSize(),
                                                    GetRasterYSize() ) )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
        {
            const int nOXSize =
                (GetRasterXSize() + panOverviewList[i] - 1)
                / panOverviewList[i];
            const int nOYSize =
                (GetRasterYSize() + panOverviewList[i] - 1)
                / panOverviewList[i];

            int nOvrJpegQuality = nJpegQuality;
            if( nCompression == COMPRESSION_JPEG &&
                CPLGetConfigOption( "JPEG_QUALITY_OVERVIEW", NULL ) != NULL )
            {
                nOvrJpegQuality =
                    atoi(CPLGetConfigOption("JPEG_QUALITY_OVERVIEW","75"));
            }

            CPLString osNoData; // don't move this in inner scope
            const char* pszNoData = NULL;
            if( bNoDataSet )
            {
                osNoData = GTiffFormatGDALNoDataTagValue(dfNoDataValue);
                pszNoData = osNoData.c_str();
            }

            const toff_t nOverviewOffset =
                GTIFFWriteDirectory(
                    hTIFF, FILETYPE_REDUCEDIMAGE,
                    nOXSize, nOYSize,
                    nOvBitsPerSample, nPlanarConfig,
                    nSamplesPerPixel, nOvrBlockXSize, nOvrBlockYSize, TRUE,
                    nCompression, nPhotometric, nSampleFormat,
                    nPredictor,
                    panRed, panGreen, panBlue,
                    nExtraSamples, panExtraSampleValues,
                    osMetadata,
                    nOvrJpegQuality >= 0 ?
                                CPLSPrintf("%d", nOvrJpegQuality) : NULL,
                    CPLSPrintf("%d", nJpegTablesMode),
                    pszNoData );

            if( nOverviewOffset == 0 )
                eErr = CE_Failure;
            else
                eErr = RegisterNewOverviewDataset(nOverviewOffset,
                                                  nOvrJpegQuality);
        }
        else
        {
            panOverviewList[i] *= -1;
        }
    }

    CPLFree(panExtraSampleValues);
    panExtraSampleValues = NULL;

/* -------------------------------------------------------------------- */
/*      Create overviews for the mask.                                  */
/* -------------------------------------------------------------------- */
    if( eErr != CE_None )
        return eErr;

    eErr = CreateInternalMaskOverviews(nOvrBlockXSize, nOvrBlockYSize);

/* -------------------------------------------------------------------- */
/*      Refresh overviews for the mask                                  */
/* -------------------------------------------------------------------- */
    if( poMaskDS != NULL &&
        poMaskDS->GetRasterCount() == 1 )
    {
        int nMaskOverviews = 0;

        GDALRasterBand **papoOverviewBands = static_cast<GDALRasterBand **>(
            CPLCalloc(sizeof(void*),nOverviewCount) );
        for( int i = 0; i < nOverviewCount; ++i )
        {
            if( papoOverviewDS[i]->poMaskDS != NULL )
            {
                papoOverviewBands[nMaskOverviews++] =
                        papoOverviewDS[i]->poMaskDS->GetRasterBand(1);
            }
        }
        eErr = GDALRegenerateOverviews(
            poMaskDS->GetRasterBand(1),
            nMaskOverviews,
            reinterpret_cast<GDALRasterBandH *>( papoOverviewBands ),
            pszResampling, GDALDummyProgress, NULL );
        CPLFree(papoOverviewBands);
    }

/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
    if( nPlanarConfig == PLANARCONFIG_CONTIG &&
        GDALDataTypeIsComplex(GetRasterBand( panBandList[0] )->
                              GetRasterDataType()) == FALSE &&
        GetRasterBand( panBandList[0] )->GetColorTable() == NULL &&
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
            for( int i = 0; i < nOverviews; ++i )
            {
                for( int j = 0; j < poBand->GetOverviewCount(); ++j )
                {
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

            int nNewOverviews = 0;
            for( int i = 0; i < nOverviews && poBand != NULL; ++i )
            {
                for( int j = 0; j < poBand->GetOverviewCount(); ++j )
                {
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

    pfnProgress( 1.0, NULL, pProgressData );

    return eErr;
}

/************************************************************************/
/*                      GTiffWriteDummyGeokeyDirectory()                */
/************************************************************************/

static void GTiffWriteDummyGeokeyDirectory( TIFF* hTIFF )
{
    // If we have existing geokeys, try to wipe them
    // by writing a dummy geokey directory. (#2546)
    uint16 *panVI = NULL;
    uint16 nKeyCount = 0;

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

#if LIBGEOTIFF_VERSION >= 1430

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
    return GTIFNewEx(hTIFF, GTiffDatasetLibGeotiffErrorCallback, NULL);
}
#else

/************************************************************************/
/*                           GTiffDatasetGTIFNew()                      */
/************************************************************************/

static GTIF* GTiffDatasetGTIFNew( TIFF* hTIFF )
{
    return GTIFNew(hTIFF);
}
#endif


/************************************************************************/
/*                          WriteGeoTIFFInfo()                          */
/************************************************************************/

void GTiffDataset::WriteGeoTIFFInfo()

{
    bool bPixelIsPoint = false;
    bool bPointGeoIgnore = false;

    if( GetMetadataItem( GDALMD_AREA_OR_POINT )
        && EQUAL(GetMetadataItem(GDALMD_AREA_OR_POINT),
                 GDALMD_AOP_POINT) )
    {
        bPixelIsPoint = true;
        bPointGeoIgnore =
            CPLTestBool( CPLGetConfigOption( "GTIFF_POINT_GEO_IGNORE",
                                             "FALSE") );
    }

    if( bForceUnsetGTOrGCPs )
    {
        bNeedsRewrite = true;
        bForceUnsetGTOrGCPs = false;

#ifdef HAVE_UNSETFIELD
        TIFFUnsetField( hTIFF, TIFFTAG_GEOPIXELSCALE );
        TIFFUnsetField( hTIFF, TIFFTAG_GEOTIEPOINTS );
        TIFFUnsetField( hTIFF, TIFFTAG_GEOTRANSMATRIX );
#endif
    }

    if( bForceUnsetProjection )
    {
        bNeedsRewrite = true;
        bForceUnsetProjection = false;

#ifdef HAVE_UNSETFIELD
        TIFFUnsetField( hTIFF, TIFFTAG_GEOKEYDIRECTORY );
        TIFFUnsetField( hTIFF, TIFFTAG_GEODOUBLEPARAMS );
        TIFFUnsetField( hTIFF, TIFFTAG_GEOASCIIPARAMS );
#else
        GTiffWriteDummyGeokeyDirectory(hTIFF);
#endif
    }

/* -------------------------------------------------------------------- */
/*      If the geotransform is the default, don't bother writing it.    */
/* -------------------------------------------------------------------- */
    if( adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
        || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
        || adfGeoTransform[4] != 0.0 || std::abs(adfGeoTransform[5]) != 1.0 )
    {
        bNeedsRewrite = true;

/* -------------------------------------------------------------------- */
/*      Clear old tags to ensure we don't end up with conflicting       */
/*      information. (#2625)                                            */
/* -------------------------------------------------------------------- */
#ifdef HAVE_UNSETFIELD
        TIFFUnsetField( hTIFF, TIFFTAG_GEOPIXELSCALE );
        TIFFUnsetField( hTIFF, TIFFTAG_GEOTIEPOINTS );
        TIFFUnsetField( hTIFF, TIFFTAG_GEOTRANSMATRIX );
#endif

/* -------------------------------------------------------------------- */
/*      Write the transform.  If we have a normal north-up image we     */
/*      use the tiepoint plus pixelscale otherwise we use a matrix.     */
/* -------------------------------------------------------------------- */
        if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0
                && adfGeoTransform[5] < 0.0 )
        {
            if( !EQUAL(osProfile,"BASELINE") )
            {
                const double adfPixelScale[3] = {
                    adfGeoTransform[1], fabs(adfGeoTransform[5]), 0.0 };
                TIFFSetField( hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );
            }

            double adfTiePoints[6] = {
                0.0, 0.0, 0.0, adfGeoTransform[0], adfGeoTransform[3], 0.0 };

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                adfTiePoints[3] +=
                    adfGeoTransform[1] * 0.5 + adfGeoTransform[2] * 0.5;
                adfTiePoints[4] +=
                    adfGeoTransform[4] * 0.5 + adfGeoTransform[5] * 0.5;
            }

            if( !EQUAL(osProfile,"BASELINE") )
                TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
        }
        else
        {
            double adfMatrix[16] = {};

            adfMatrix[0] = adfGeoTransform[1];
            adfMatrix[1] = adfGeoTransform[2];
            adfMatrix[3] = adfGeoTransform[0];
            adfMatrix[4] = adfGeoTransform[4];
            adfMatrix[5] = adfGeoTransform[5];
            adfMatrix[7] = adfGeoTransform[3];
            adfMatrix[15] = 1.0;

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                adfMatrix[3] +=
                    adfGeoTransform[1] * 0.5 + adfGeoTransform[2] * 0.5;
                adfMatrix[7] +=
                    adfGeoTransform[4] * 0.5 + adfGeoTransform[5] * 0.5;
            }

            if( !EQUAL(osProfile,"BASELINE") )
                TIFFSetField( hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix );
        }

        // Do we need a world file?
        if( CPLFetchBool( papszCreationOptions, "TFW", false ) )
            GDALWriteWorldFile( osFilename, "tfw", adfGeoTransform );
        else if( CPLFetchBool( papszCreationOptions, "WORLDFILE", false ) )
            GDALWriteWorldFile( osFilename, "wld", adfGeoTransform );
    }
    else if( GetGCPCount() > 0 )
    {
        bNeedsRewrite = true;

        double *padfTiePoints = static_cast<double *>(
            CPLMalloc( 6 * sizeof(double) * GetGCPCount() ) );

        for( int iGCP = 0; iGCP < GetGCPCount(); ++iGCP )
        {

            padfTiePoints[iGCP*6+0] = pasGCPList[iGCP].dfGCPPixel;
            padfTiePoints[iGCP*6+1] = pasGCPList[iGCP].dfGCPLine;
            padfTiePoints[iGCP*6+2] = 0;
            padfTiePoints[iGCP*6+3] = pasGCPList[iGCP].dfGCPX;
            padfTiePoints[iGCP*6+4] = pasGCPList[iGCP].dfGCPY;
            padfTiePoints[iGCP*6+5] = pasGCPList[iGCP].dfGCPZ;

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                padfTiePoints[iGCP*6+0] += 0.5;
                padfTiePoints[iGCP*6+1] += 0.5;
            }
        }

        if( !EQUAL(osProfile,"BASELINE") )
            TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS,
                          6 * GetGCPCount(), padfTiePoints );
        CPLFree( padfTiePoints );
    }

/* -------------------------------------------------------------------- */
/*      Write out projection definition.                                */
/* -------------------------------------------------------------------- */
    const bool bHasProjection =
        pszProjection != NULL && strlen(pszProjection) > 0;
    if( (bHasProjection || bPixelIsPoint)
        && !EQUAL(osProfile,"BASELINE") )
    {
        bNeedsRewrite = true;

        // If we have existing geokeys, try to wipe them
        // by writing a dummy geokey directory. (#2546)
        GTiffWriteDummyGeokeyDirectory(hTIFF);

        GTIF *psGTIF = GTiffDatasetGTIFNew( hTIFF );

        // Set according to coordinate system.
        if( bHasProjection )
        {
            GTIFSetFromOGISDefnEx( psGTIF, pszProjection, eGeoTIFFKeysFlavor );
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
    CPLXMLNode *psItem = CPLCreateXMLNode( NULL, CXT_Element, "Item" );
    CPLCreateXMLNode( CPLCreateXMLNode( psItem, CXT_Attribute, "name"),
                      CXT_Text, pszKey );

    if( nBand > 0 )
    {
        char szBandId[32] = {};
        snprintf( szBandId, sizeof(szBandId), "%d", nBand - 1 );
        CPLCreateXMLNode( CPLCreateXMLNode( psItem,CXT_Attribute,"sample"),
                          CXT_Text, szBandId );
    }

    if( pszRole != NULL )
        CPLCreateXMLNode( CPLCreateXMLNode( psItem,CXT_Attribute,"role"),
                          CXT_Text, pszRole );

    if( pszDomain != NULL && strlen(pszDomain) > 0 )
        CPLCreateXMLNode( CPLCreateXMLNode( psItem,CXT_Attribute,"domain"),
                          CXT_Text, pszDomain );

    char *pszEscapedItemValue = CPLEscapeString(pszValue,-1,CPLES_XML);
    CPLCreateXMLNode( psItem, CXT_Text, pszEscapedItemValue );
    CPLFree( pszEscapedItemValue );

/* -------------------------------------------------------------------- */
/*      Create root, if missing.                                        */
/* -------------------------------------------------------------------- */
    if( *ppsRoot == NULL )
        *ppsRoot = CPLCreateXMLNode( NULL, CXT_Element, "GDALMetadata" );

/* -------------------------------------------------------------------- */
/*      Append item to tail.  We keep track of the tail to avoid        */
/*      O(nsquared) time as the list gets longer.                       */
/* -------------------------------------------------------------------- */
    if( *ppsTail == NULL )
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
                             int nBand, const char * /* pszProfile */ )

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

        if( STARTS_WITH_CI(papszDomainList[iDomain], "xml:") )
            bIsXML = true;

/* -------------------------------------------------------------------- */
/*      Process each item in this domain.                               */
/* -------------------------------------------------------------------- */
        for( int iItem = 0; papszMD && papszMD[iItem]; ++iItem )
        {
            const char *pszItemValue = NULL;
            char *pszItemName = NULL;

            if( bIsXML )
            {
                pszItemName = CPLStrdup("doc");
                pszItemValue = papszMD[iItem];
            }
            else
            {
                pszItemValue = CPLParseNameValue( papszMD[iItem], &pszItemName);
                if( pszItemName == NULL )
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
                && nBand == 0 && STARTS_WITH_CI(pszItemName, "TIFFTAG_") )
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
                                    nBand, NULL, papszDomainList[iDomain] );
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
                char* pszText = NULL;
                int16 nVal = 0;
                float fVal = 0.0f;
                const char* pszVal =
                    CSLFetchNameValue(papszMD, asTIFFTags[iTag].pszTagName);
                if( pszVal == NULL &&
                    ((asTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING &&
                      TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal,
                                    &pszText )) ||
                     (asTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT &&
                      TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &nVal )) ||
                     (asTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT &&
                      TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &fVal ))) )
                {
#ifdef HAVE_UNSETFIELD
                    TIFFUnsetField( hTIFF, asTIFFTags[iTag].nTagVal );
#else
                    if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING )
                    {
                        TIFFSetField( hTIFF, asTIFFTags[iTag].nTagVal, "" );
                    }
#endif
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
                             const char *pszProfile,
                             const char *pszTIFFFilename,
                             char **l_papszCreationOptions,
                             bool bWriteOnlyInPAMIfNeeded )
{
/* -------------------------------------------------------------------- */
/*      Handle RPC data written to an RPB file.                         */
/* -------------------------------------------------------------------- */
    char **papszRPCMD = poSrcDS->GetMetadata(MD_DOMAIN_RPC);
    if( papszRPCMD != NULL )
    {
        bool bRPCSerializedOtherWay = false;

        if( EQUAL(pszProfile,"GDALGeoTIFF") )
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
        if( (!EQUAL(pszProfile,"GDALGeoTIFF") &&
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
            reinterpret_cast<GTiffDataset*>(poSrcDS)->
                GDALPamDataset::SetMetadata( papszRPCMD, MD_DOMAIN_RPC );
    }
}

/************************************************************************/
/*                  IsStandardColorInterpretation()                     */
/************************************************************************/

static bool IsStandardColorInterpretation(GDALDataset* poSrcDS,
                                          uint16 nPhotometric)
{
    bool bStardardColorInterp = true;
    if( nPhotometric == PHOTOMETRIC_MINISBLACK )
    {
        for( int i = 0; i < poSrcDS->GetRasterCount(); ++i )
        {
            const GDALColorInterp eInterp =
                poSrcDS->GetRasterBand(i + 1)->GetColorInterpretation();
            if( !(eInterp == GCI_GrayIndex || eInterp == GCI_Undefined ||
                    (i > 0 && eInterp == GCI_AlphaBand)) )
            {
                bStardardColorInterp = false;
                break;
            }
        }
    }
    else if( nPhotometric == PHOTOMETRIC_RGB )
    {
        for( int i = 0; i < poSrcDS->GetRasterCount(); ++i )
        {
            const GDALColorInterp eInterp =
                poSrcDS->GetRasterBand(i+1)->GetColorInterpretation();
            if( !((i == 0 && eInterp == GCI_RedBand) ||
                    (i == 1 && eInterp == GCI_GreenBand) ||
                    (i == 2 && eInterp == GCI_BlueBand) ||
                    (i >= 3 && (eInterp == GCI_Undefined ||
                                eInterp == GCI_AlphaBand))) )
            {
                bStardardColorInterp = false;
                break;
            }
        }
    }
    else
    {
        bStardardColorInterp = false;
    }
    return bStardardColorInterp;
}

/************************************************************************/
/*                           WriteMetadata()                            */
/************************************************************************/

bool GTiffDataset::WriteMetadata( GDALDataset *poSrcDS, TIFF *l_hTIFF,
                                  bool bSrcIsGeoTIFF,
                                  const char *pszProfile,
                                  const char *pszTIFFFilename,
                                  char **l_papszCreationOptions,
                                  bool bExcludeRPBandIMGFileWriting)

{
/* -------------------------------------------------------------------- */
/*      Convert all the remaining metadata into a simple XML            */
/*      format.                                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot = NULL;
    CPLXMLNode *psTail = NULL;

    if( bSrcIsGeoTIFF )
    {
        WriteMDMetadata(
            &reinterpret_cast<GTiffDataset *>(poSrcDS)->oGTiffMDMD,
            l_hTIFF, &psRoot, &psTail, 0, pszProfile );
    }
    else
    {
        char **papszMD = poSrcDS->GetMetadata();

        if( CSLCount(papszMD) > 0 )
        {
            GDALMultiDomainMetadata l_oMDMD;
            l_oMDMD.SetMetadata( papszMD );

            WriteMDMetadata( &l_oMDMD, l_hTIFF, &psRoot, &psTail,
                             0, pszProfile );
        }
    }

    if( !bExcludeRPBandIMGFileWriting )
    {
        WriteRPC(poSrcDS, l_hTIFF, bSrcIsGeoTIFF,
                 pszProfile, pszTIFFFilename,
                 l_papszCreationOptions);

/* -------------------------------------------------------------------- */
/*      Handle metadata data written to an IMD file.                    */
/* -------------------------------------------------------------------- */
        char **papszIMDMD = poSrcDS->GetMetadata(MD_DOMAIN_IMD);
        if( papszIMDMD != NULL )
        {
            GDALWriteIMDFile( pszTIFFFilename, papszIMDMD );
        }
    }

    uint16 nPhotometric = 0;
    if( !TIFFGetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, &(nPhotometric) ) )
        nPhotometric = PHOTOMETRIC_MINISBLACK;

    const bool bStardardColorInterp =
        IsStandardColorInterpretation(poSrcDS, nPhotometric);

/* -------------------------------------------------------------------- */
/*      We also need to address band specific metadata, and special     */
/*      "role" metadata.                                                */
/* -------------------------------------------------------------------- */
    for( int nBand = 1; nBand <= poSrcDS->GetRasterCount(); ++nBand )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( nBand );

        if( bSrcIsGeoTIFF )
        {
            WriteMDMetadata(
                &reinterpret_cast<GTiffRasterBand *>(poBand)->oGTiffMDMD,
                l_hTIFF, &psRoot, &psTail, nBand, pszProfile );
        }
        else
        {
            char **papszMD = poBand->GetMetadata();

            if( CSLCount(papszMD) > 0 )
            {
                GDALMultiDomainMetadata l_oMDMD;
                l_oMDMD.SetMetadata( papszMD );

                WriteMDMetadata( &l_oMDMD, l_hTIFF, &psRoot, &psTail, nBand,
                                 pszProfile );
            }
        }

        const double dfOffset = poBand->GetOffset();
        const double dfScale = poBand->GetScale();

        if( dfOffset != 0.0 || dfScale != 1.0 )
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
        if( pszUnitType != NULL && pszUnitType[0] != '\0' )
            AppendMetadataItem( &psRoot, &psTail, "UNITTYPE",
                                pszUnitType, nBand,
                                "unittype", "" );

        if( strlen(poBand->GetDescription()) > 0 )
        {
            AppendMetadataItem( &psRoot, &psTail, "DESCRIPTION",
                                poBand->GetDescription(), nBand,
                                "description", "" );
        }

        if( !bStardardColorInterp )
        {
            AppendMetadataItem( &psRoot, &psTail, "COLORINTERP",
                                GDALGetColorInterpretationName(
                                    poBand->GetColorInterpretation()),
                                nBand,
                                "colorinterp", "" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write out the generic XML metadata if there is any.             */
/* -------------------------------------------------------------------- */
    if( psRoot != NULL )
    {
        bool bRet = true;

        if( EQUAL(pszProfile,"GDALGeoTIFF") )
        {
            char *pszXML_MD = CPLSerializeXMLTree( psRoot );
            if( strlen(pszXML_MD) > 32000 )
            {
                if( bSrcIsGeoTIFF )
                {
                    if( reinterpret_cast<GTiffDataset *>(
                           poSrcDS)->GetPamFlags() & GPF_DISABLED )
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Metadata exceeding 32000 bytes cannot be written "
                            "into GeoTIFF." );
                    }
                    else
                    {
                        reinterpret_cast<GTiffDataset *>(poSrcDS)->
                            PushMetadataToPam();
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
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
                reinterpret_cast<GTiffDataset *>(poSrcDS)->PushMetadataToPam();
            else
                bRet = false;
        }

        CPLDestroyXMLNode( psRoot );

        return bRet;
    }

    // If we have no more metadata but it existed before,
    // remove the GDAL_METADATA tag.
    if( EQUAL(pszProfile,"GDALGeoTIFF") )
    {
        char* pszText = NULL;
        if( TIFFGetField( l_hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) )
        {
#ifdef HAVE_UNSETFIELD
            TIFFUnsetField( l_hTIFF, TIFFTAG_GDAL_METADATA );
#else
            TIFFSetField( l_hTIFF, TIFFTAG_GDAL_METADATA, "" );
#endif
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
    const bool bStardardColorInterp =
        IsStandardColorInterpretation(this, nPhotometric);

    for( int nBand = 0; nBand <= GetRasterCount(); ++nBand )
    {
        GDALMultiDomainMetadata *poSrcMDMD = NULL;
        GTiffRasterBand *poBand = NULL;

        if( nBand == 0 )
        {
            poSrcMDMD = &(this->oGTiffMDMD);
        }
        else
        {
            poBand = reinterpret_cast<GTiffRasterBand *>(GetRasterBand(nBand));
            poSrcMDMD = &(poBand->oGTiffMDMD);
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
                    papszMD = CSLRemoveStrings( papszMD, i, 1, NULL );
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
        if( poBand != NULL )
        {
            poBand->GDALPamRasterBand::SetOffset( poBand->GetOffset() );
            poBand->GDALPamRasterBand::SetScale( poBand->GetScale() );
            poBand->GDALPamRasterBand::SetUnitType( poBand->GetUnitType() );
            poBand->
                GDALPamRasterBand::SetDescription( poBand->GetDescription() );
            if( !bStardardColorInterp )
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
    GDALRPCInfo sRPC;

    if( !GDALExtractRPCInfo( papszRPCMD, &sRPC ) )
        return;

    double adfRPCTag[92] = {};
    adfRPCTag[0] = -1.0;  // Error Bias
    adfRPCTag[1] = -1.0;  // Error Random

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
    double *padfRPCTag = NULL;
    uint16 nCount;

    if( !TIFFGetField( hTIFF, TIFFTAG_RPCCOEFFICIENT, &nCount, &padfRPCTag )
        || nCount != 92 )
        return NULL;

    CPLStringList asMD;
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
#ifdef HAVE_UNSETFIELD
    TIFFUnsetField( l_hTIFF, TIFFTAG_GDAL_NODATA );
#else
    TIFFSetField( l_hTIFF, TIFFTAG_GDAL_NODATA, "" );
#endif
}

/************************************************************************/
/*                            SetDirectory()                            */
/************************************************************************/

bool GTiffDataset::SetDirectory( toff_t nNewOffset )

{
    Crystalize();

    if( nNewOffset == 0 )
        nNewOffset = nDirOffset;

    if( TIFFCurrentDirOffset(hTIFF) == nNewOffset )
    {
        CPLAssert( *ppoActiveDSRef == this || *ppoActiveDSRef == NULL );
        *ppoActiveDSRef = this;
        return true;
    }

    if( GetAccess() == GA_Update )
    {
        if( *ppoActiveDSRef != NULL )
            (*ppoActiveDSRef)->FlushDirectory();
    }

    if( nNewOffset == 0)
        return true;

    (*ppoActiveDSRef) = this;

    const int nSetDirResult = TIFFSetSubDirectory( hTIFF, nNewOffset );
    if( !nSetDirResult )
        return false;

/* -------------------------------------------------------------------- */
/*      YCbCr JPEG compressed images should be translated on the fly    */
/*      to RGB by libtiff/libjpeg unless specifically requested         */
/*      otherwise.                                                      */
/* -------------------------------------------------------------------- */
    if( !TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &(nCompression) ) )
        nCompression = COMPRESSION_NONE;

    if( !TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &(nPhotometric) ) )
        nPhotometric = PHOTOMETRIC_MINISBLACK;

    if( nCompression == COMPRESSION_JPEG
        && nPhotometric == PHOTOMETRIC_YCBCR
        && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                              "YES") ) )
    {
        int nColorMode = JPEGCOLORMODE_RAW;  // Initialize to 0;

        TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode );
        if( nColorMode != JPEGCOLORMODE_RGB )
            TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
    }

/* -------------------------------------------------------------------- */
/*      Propagate any quality settings.                                 */
/* -------------------------------------------------------------------- */
    if( GetAccess() == GA_Update )
    {
        // Now, reset zip and jpeg quality.
        if(nJpegQuality > 0 && nCompression == COMPRESSION_JPEG)
        {
#ifdef DEBUG_VERBOSE
            CPLDebug( "GTiff", "Propagate JPEG_QUALITY(%d) in SetDirectory()",
                      nJpegQuality );
#endif
            TIFFSetField(hTIFF, TIFFTAG_JPEGQUALITY, nJpegQuality);
        }
        if(nJpegTablesMode >= 0 && nCompression == COMPRESSION_JPEG)
            TIFFSetField(hTIFF, TIFFTAG_JPEGTABLESMODE, nJpegTablesMode);
        if(nZLevel > 0 && nCompression == COMPRESSION_ADOBE_DEFLATE)
            TIFFSetField(hTIFF, TIFFTAG_ZIPQUALITY, nZLevel);
        if(nLZMAPreset > 0 && nCompression == COMPRESSION_LZMA)
            TIFFSetField(hTIFF, TIFFTAG_LZMAPRESET, nLZMAPreset);
    }

    return true;
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
    if( poOpenInfo->fpL == NULL || poOpenInfo->nHeaderBytes < 2 )
        return FALSE;

    if( (poOpenInfo->pabyHeader[0] != 'I' || poOpenInfo->pabyHeader[1] != 'I')
        && (poOpenInfo->pabyHeader[0] != 'M'
        || poOpenInfo->pabyHeader[1] != 'M'))
        return FALSE;

#ifndef BIGTIFF_SUPPORT
    if( (poOpenInfo->pabyHeader[2] == 0x2B && poOpenInfo->pabyHeader[3] == 0) ||
        (poOpenInfo->pabyHeader[2] == 0 && poOpenInfo->pabyHeader[3] == 0x2B) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "This is a BigTIFF file.  BigTIFF is not supported by this "
                  "version of GDAL and libtiff." );
        return FALSE;
    }
#endif

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
    if( fpTemp == NULL )
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
#ifndef CPL_HAS_GINT64
        CPLError(CE_Failure, CPLE_NotSupported, "BigTIFF not supported");
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTemp));
        VSIUnlink(osTmpFilename);
        return false;
#else
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
                TIFFDataWidth((TIFFDataType)nDataType) * nCount;
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
#endif
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
/*                  GTiffCheckCurrentDirIsCompatOfStripChop()           */
/************************************************************************/

static bool GTiffCheckCurrentDirIsCompatOfStripChop( TIFF* l_hTIFF,
                                                     bool& bCandidateForStripChopReopening )
{
    uint32 nXSize = 0;
    TIFFGetField( l_hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );

    uint32 nYSize = 0;
    TIFFGetField( l_hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );

    if( nXSize > INT_MAX || nYSize > INT_MAX )
    {
        return false;
    }

    uint16 l_nPlanarConfig = 0;
    if( !TIFFGetField( l_hTIFF, TIFFTAG_PLANARCONFIG, &(l_nPlanarConfig) ) )
        l_nPlanarConfig = PLANARCONFIG_CONTIG;

    uint16 l_nCompression = 0;
    if( !TIFFGetField( l_hTIFF, TIFFTAG_COMPRESSION, &(l_nCompression) ) )
        l_nCompression = COMPRESSION_NONE;

    uint32 l_nRowsPerStrip = 0;
    if( !TIFFGetField( l_hTIFF, TIFFTAG_ROWSPERSTRIP, &(l_nRowsPerStrip) ) )
        l_nRowsPerStrip = nYSize;

    bool bCanReopenWithStripChop = true;
    if( !TIFFIsTiled( l_hTIFF ) &&
        l_nCompression == COMPRESSION_NONE &&
        l_nRowsPerStrip >= nYSize &&
        l_nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        bCandidateForStripChopReopening = true;
        if( nYSize > 10 * 1024 * 1024 )
        {
            uint16 l_nSamplesPerPixel = 0;
            if( !TIFFGetField( l_hTIFF, TIFFTAG_SAMPLESPERPIXEL,
                               &l_nSamplesPerPixel ) )
                l_nSamplesPerPixel = 1;

            uint16 l_nBitsPerSample = 0;
            if( !TIFFGetField(l_hTIFF, TIFFTAG_BITSPERSAMPLE,
                              &(l_nBitsPerSample)) )
                l_nBitsPerSample = 1;

            const vsi_l_offset nLineSize =
                (l_nSamplesPerPixel * static_cast<vsi_l_offset>(nXSize) *
                 l_nBitsPerSample + 7) / 8;
            int nDefaultStripHeight = static_cast<int>(8192 / nLineSize);
            if( nDefaultStripHeight == 0 ) nDefaultStripHeight = 1;
            const vsi_l_offset nStrips = nYSize / nDefaultStripHeight;

            // There is a risk of DoS due to huge amount of memory allocated in
            // ChopUpSingleUncompressedStrip() in libtiff.
            if( nStrips > 10 * 1024 * 1024 &&
                !CPLTestBool(
                    CPLGetConfigOption("GTIFF_FORCE_STRIP_CHOP", "NO")) )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Potential denial of service detected. Avoid using strip "
                    "chop. Set the GTIFF_FORCE_STRIP_CHOP configuration open "
                    "to go over this test." );
                bCanReopenWithStripChop = false;
            }
        }
    }
    return bCanReopenWithStripChop;
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
        return NULL;

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
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    // Disable strip chop for now.
    bool bStreaming = false;
    const char* pszReadStreaming =
        CPLGetConfigOption("TIFF_READ_STREAMING", NULL);
    if( poOpenInfo->fpL == NULL )
    {
        poOpenInfo->fpL =
            VSIFOpenL( pszFilename,
                       poOpenInfo->eAccess == GA_ReadOnly ? "rb" : "r+b" );
        if( poOpenInfo->fpL == NULL )
            return NULL;
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
            return NULL;
    }

    // Store errors/warnings and emit them later.
    std::vector<GTIFFErrorStruct> aoErrors;
    CPLPushErrorHandlerEx(GTIFFErrorHandler, &aoErrors);
    CPLSetCurrentErrorHandlerCatchDebug( FALSE );
    // Open and disable "strip chopping" (c option)
    TIFF *l_hTIFF =
        VSI_TIFFOpen( pszFilename,
                      poOpenInfo->eAccess == GA_ReadOnly ? "rc" : "r+c",
                      poOpenInfo->fpL );
    CPLPopErrorHandler();
#if SIZEOF_VOIDP == 4
    if( l_hTIFF == NULL )
    {
        // Case of one-strip file where the strip size is > 2GB (#5403).
        if( bGlobalStripIntegerOverflow )
        {
            l_hTIFF =
                VSI_TIFFOpen( pszFilename,
                              poOpenInfo->eAccess == GA_ReadOnly ? "r" : "r+",
                              poOpenInfo->fpL );
            bGlobalStripIntegerOverflow = false;
        }
    }
    else
    {
        bGlobalStripIntegerOverflow = false;
    }
#endif

    // Now emit errors and change their criticality if needed
    // We only emit failures if we didn't manage to open the file.
    // Otherwise it makes Python bindings unhappy (#5616).
    for( size_t iError = 0; iError < aoErrors.size(); ++iError )
    {
        CPLError( (l_hTIFF == NULL && aoErrors[iError].type == CE_Failure) ?
                  CE_Failure : CE_Warning,
                  aoErrors[iError].no,
                  "%s",
                  aoErrors[iError].msg.c_str() );
    }
    aoErrors.resize(0);

    if( l_hTIFF == NULL )
        return NULL;

    uint32 nXSize = 0;
    TIFFGetField( l_hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    uint32 nYSize = 0;
    TIFFGetField( l_hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );

    if( nXSize > INT_MAX || nYSize > INT_MAX )
    {
        // GDAL only supports signed 32bit dimensions.
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Too large image size: %u x %u",
                 nXSize, nYSize);
        XTIFFClose( l_hTIFF );
        return NULL;
    }

    uint16 l_nCompression = 0;
    if( !TIFFGetField( l_hTIFF, TIFFTAG_COMPRESSION, &(l_nCompression) ) )
        l_nCompression = COMPRESSION_NONE;

    bool bCandidateForStripChopReopening = false;
    if( GTiffCheckCurrentDirIsCompatOfStripChop(l_hTIFF,
                                    bCandidateForStripChopReopening ) &&
        bCandidateForStripChopReopening )
    {
        bool bReopenWithStripChop = true;
        int iDirIndex = 1;
        // Inspect all directories to decide if we can safely re-open in
        // strip chop mode

        toff_t nCurOffset = TIFFCurrentDirOffset(l_hTIFF);
        bool bHasSeveralDirecotries = false;

        while( !TIFFLastDirectory( l_hTIFF ) )
        {
            bHasSeveralDirecotries = true;
            const CPLErr eLastErrorType = CPLGetLastErrorType();
            const CPLErrorNum eLastErrorNo = CPLGetLastErrorNo();
            const CPLString osLastErrorMsg(CPLGetLastErrorMsg());
            CPLPushErrorHandler(CPLQuietErrorHandler);
            bool bOk = TIFFReadDirectory( l_hTIFF ) != 0;
            CPLPopErrorHandler();
            CPLErrorSetState(eLastErrorType, eLastErrorNo, osLastErrorMsg);
            if( !bOk )
                break;

            // Only libtiff 4.0.4 can handle between 32768 and 65535 directories.
#if !defined(INTERNAL_LIBTIFF) && (!defined(TIFFLIB_VERSION) || (TIFFLIB_VERSION < 20120922))
            if( iDirIndex == 32768 )
                break;
#endif
            if( !GTiffCheckCurrentDirIsCompatOfStripChop(l_hTIFF,
                                    bCandidateForStripChopReopening) )
            {
                bReopenWithStripChop = false;
                break;
            }
            iDirIndex ++;
        }

        if( bReopenWithStripChop )
        {
            CPLDebug("GTiff", "Reopen with strip chop enabled");
            XTIFFClose(l_hTIFF);
            l_hTIFF =
                VSI_TIFFOpen( pszFilename,
                              poOpenInfo->eAccess == GA_ReadOnly ? "r" : "r+",
                              poOpenInfo->fpL );
            if( l_hTIFF == NULL )
                return NULL;
        }
        else if( bHasSeveralDirecotries )
        {
            TIFFSetSubDirectory( l_hTIFF, nCurOffset );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->SetDescription( pszFilename );
    poDS->osFilename = pszFilename;
    poDS->poActiveDS = poDS;
    poDS->fpL = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;
    poDS->bStreamingIn = bStreaming;
    poDS->nCompression = l_nCompression;

    // In the case of GDAL_DISABLE_READDIR_ON_OPEN = NO / EMPTY_DIR
    if( poOpenInfo->AreSiblingFilesLoaded() &&
        CSLCount( poOpenInfo->GetSiblingFiles() ) <= 1 )
    {
        poDS->oOvManager.TransferSiblingFiles( CSLDuplicate(
                                            poOpenInfo->GetSiblingFiles() ) );
        poDS->m_bHasGotSiblingFiles = true;
    }

    if( poDS->OpenOffset( l_hTIFF, &(poDS->poActiveDS),
                          TIFFCurrentDirOffset(l_hTIFF), true,
                          poOpenInfo->eAccess,
                          bAllowRGBAInterface, true) != CE_None )
    {
        delete poDS;
        return NULL;
    }

    // Do we want blocks that are set to zero and that haven't yet being
    // allocated as tile/strip to remain implicit?
    if( CPLFetchBool( poOpenInfo->papszOpenOptions, "SPARSE_OK", false ) )
        poDS->bWriteEmptyTiles = false;

    if( poOpenInfo->eAccess == GA_Update )
    {
        poDS->InitCreationOrOpenOptions(poOpenInfo->papszOpenOptions);
    }

    poDS->m_bLoadPam = true;
    poDS->bColorProfileMetadataChanged = false;
    poDS->bMetadataChanged = false;
    poDS->bGeoTIFFInfoChanged = false;
    poDS->bNoDataChanged = false;
    poDS->bForceUnsetGTOrGCPs = false;
    poDS->bForceUnsetProjection = false;

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
    if( CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE", NULL) != NULL )
    {
        poDS->LoadGeoreferencingAndPamIfNeeded();
    }

    return poDS;
}

/************************************************************************/
/*                      GTiffDatasetSetAreaOrPointMD()                  */
/************************************************************************/

static void GTiffDatasetSetAreaOrPointMD( GTIF* hGTIF,
                                          GDALMultiDomainMetadata& oGTiffMDMD )
{
    // Is this a pixel-is-point dataset?
    short nRasterType = 0;

    if( GDALGTIFKeyGetSHORT(hGTIF, GTRasterTypeGeoKey, &nRasterType,
                    0, 1 ) == 1 )
    {
        if( nRasterType == static_cast<short>(RasterPixelIsPoint) )
            oGTiffMDMD.SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);
        else
            oGTiffMDMD.SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_AREA);
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
    if( bLookedForProjection || bLookedForMDAreaOrPoint ||
        oGTiffMDMD.GetMetadataItem( GDALMD_AREA_OR_POINT ) != NULL )
        return;

    bLookedForMDAreaOrPoint = true;

    if( !SetDirectory() )
        return;

    GTIF* hGTIF = GTiffDatasetGTIFNew(hTIFF);

    if( !hGTIF )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "GeoTIFF tags apparently corrupt, they are being ignored." );
    }
    else
    {
        GTiffDatasetSetAreaOrPointMD( hGTIF, oGTiffMDMD );

        GTIFFree( hGTIF );
    }
}

/************************************************************************/
/*                         LookForProjection()                          */
/************************************************************************/

void GTiffDataset::LookForProjection()

{
    if( bLookedForProjection )
        return;

    bLookedForProjection = true;

    IdentifyAuthorizedGeoreferencingSources();
    if( m_nINTERNALGeorefSrcIndex < 0 )
        return;

    if( !SetDirectory() )
        return;

/* -------------------------------------------------------------------- */
/*      Capture the GeoTIFF projection, if available.                   */
/* -------------------------------------------------------------------- */
    CPLFree( pszProjection );
    pszProjection = NULL;

    GTIF *hGTIF = GTiffDatasetGTIFNew(hTIFF);

    if( !hGTIF )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "GeoTIFF tags apparently corrupt, they are being ignored." );
    }
    else
    {
#if LIBGEOTIFF_VERSION >= 1410
        GTIFDefn *psGTIFDefn = GTIFAllocDefn();
#else
        GTIFDefn *psGTIFDefn =
            static_cast<GTIFDefn *>(CPLCalloc(1, sizeof(GTIFDefn)));
#endif

        if( GTIFGetDefn( hGTIF, psGTIFDefn ) )
        {
            pszProjection = GTIFGetOGISDefn( hGTIF, psGTIFDefn );

            if( STARTS_WITH_CI(pszProjection, "COMPD_CS") )
            {
                OGRSpatialReference oSRS;

                char *pszWKT = pszProjection;
                oSRS.importFromWkt( &pszWKT );

                char* pszVertUnit = NULL;
                oSRS.GetTargetLinearUnits("COMPD_CS|VERT_CS", &pszVertUnit);
                if( pszVertUnit && !EQUAL(pszVertUnit, "unknown") )
                {
                    m_osVertUnit = pszVertUnit;
                }

                // Should we simplify away vertical CS stuff?
                if( !CPLTestBool( CPLGetConfigOption("GTIFF_REPORT_COMPD_CS",
                                                    "NO") ) )
                {
                    CPLDebug( "GTiff", "Got COMPD_CS, but stripping it." );

                    oSRS.StripVertical();
                    CPLFree( pszProjection );
                    oSRS.exportToWkt( &pszProjection );
                }
            }
        }

        // Check the tif linear unit and the CS linear unit.
#ifdef ESRI_BUILD
        AdjustLinearUnit(psGTIFDefn.UOMLength);
#endif

#if LIBGEOTIFF_VERSION >= 1410
        GTIFFreeDefn(psGTIFDefn);
#else
        CPLFree(psGTIFDefn);
#endif

        GTiffDatasetSetAreaOrPointMD( hGTIF, oGTiffMDMD );

        GTIFFree( hGTIF );
    }

    if( pszProjection == NULL )
    {
        pszProjection = CPLStrdup( "" );
    }
    // else if( !EQUAL(pszProjection, "") )
    // {
    //     m_nProjectionGeorefSrcIndex = m_nINTERNALGeorefSrcIndex;
    // }

    bGeoTIFFInfoChanged = false;
    bForceUnsetGTOrGCPs = false;
    bForceUnsetProjection = false;
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
        ((bGeoTransformValid &&
          m_nPAMGeorefSrcIndex < m_nGeoTransformGeorefSrcIndex) ||
          m_nGeoTransformGeorefSrcIndex < 0 || !bGeoTransformValid) )
    {
        double adfPamGeoTransform[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        if( GDALPamDataset::GetGeoTransform( adfPamGeoTransform ) == CE_None
            && (adfPamGeoTransform[0] != 0.0 || adfPamGeoTransform[1] != 1.0
                || adfPamGeoTransform[2] != 0.0 || adfPamGeoTransform[3] != 0.0
                || adfPamGeoTransform[4] != 0.0
                || adfPamGeoTransform[5] != 1.0 ))
        {
            if( m_nGeoTransformGeorefSrcIndex == m_nWORLDFILEGeorefSrcIndex )
                osGeorefFilename.clear();
            memcpy(adfGeoTransform, adfPamGeoTransform, sizeof(double) * 6);
            bGeoTransformValid = true;
        }
    }

    if( m_nPAMGeorefSrcIndex >= 0 )
    {
        if( (m_nTABFILEGeorefSrcIndex < 0 ||
             m_nPAMGeorefSrcIndex < m_nTABFILEGeorefSrcIndex) &&
            (m_nINTERNALGeorefSrcIndex < 0 ||
             m_nPAMGeorefSrcIndex < m_nINTERNALGeorefSrcIndex) )
        {
            const char *pszPamSRS = GDALPamDataset::GetProjectionRef();
            if( pszPamSRS != NULL && strlen(pszPamSRS) > 0 )
            {
                CPLFree( pszProjection );
                pszProjection = CPLStrdup( pszPamSRS );
                bLookedForProjection = true;
                // m_nProjectionGeorefSrcIndex = m_nPAMGeorefSrcIndex;
            }
        }
        else
        {
            if( m_nINTERNALGeorefSrcIndex >= 0 )
                LookForProjection();
            if( pszProjection == NULL || strlen(pszProjection) == 0 )
            {
                const char *pszPamSRS = GDALPamDataset::GetProjectionRef();
                if( pszPamSRS != NULL && strlen(pszPamSRS) > 0 )
                {
                    CPLFree( pszProjection );
                    pszProjection = CPLStrdup( pszPamSRS );
                    bLookedForProjection = true;
                    // m_nProjectionGeorefSrcIndex = m_nPAMGeorefSrcIndex;
                }
            }
        }
    }

    int nPamGCPCount;
    if( m_nPAMGeorefSrcIndex >= 0 &&
        (nPamGCPCount = GDALPamDataset::GetGCPCount()) > 0 &&
        ( (nGCPCount > 0 &&
           m_nPAMGeorefSrcIndex < m_nGeoTransformGeorefSrcIndex) ||
          m_nGeoTransformGeorefSrcIndex < 0 || nGCPCount == 0 ) )
    {
        if( nGCPCount > 0 )
        {
            GDALDeinitGCPs( nGCPCount, pasGCPList );
            CPLFree( pasGCPList );
            pasGCPList = NULL;
        }

        nGCPCount = nPamGCPCount;
        pasGCPList = GDALDuplicateGCPs(nGCPCount, GDALPamDataset::GetGCPs());

        CPLFree( pszProjection );
        pszProjection = NULL;
        // m_nProjectionGeorefSrcIndex = m_nPAMGeorefSrcIndex;

        const char *pszPamGCPProjection = GDALPamDataset::GetGCPProjection();
        if( pszPamGCPProjection != NULL && strlen(pszPamGCPProjection) > 0 )
            pszProjection = CPLStrdup(pszPamGCPProjection);

        bLookedForProjection = true;
    }

    if( m_nPAMGeorefSrcIndex >= 0 && nGCPCount == 0 )
    {
        CPLXMLNode *psValueAsXML = NULL;
        CPLXMLNode *psGeodataXform = NULL;
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
                                    psIter != NULL;
                                    psIter = psIter->psNext )
                {
                    if( psIter->eType == CXT_Element &&
                        EQUAL(psIter->pszValue, "Double") )
                    {
                        adfSourceGCPs.push_back(
                            CPLAtof( CPLGetXMLValue(psIter, NULL, "") ) );
                    }
                }
                for( CPLXMLNode* psIter = psTargetGCPs->psChild;
                                    psIter != NULL;
                                    psIter = psIter->psNext )
                {
                    if( psIter->eType == CXT_Element &&
                        EQUAL(psIter->pszValue, "Double") )
                    {
                        adfTargetGCPs.push_back(
                            CPLAtof( CPLGetXMLValue(psIter, NULL, "") ) );
                    }
                }
                if( adfSourceGCPs.size() == adfTargetGCPs.size() &&
                    (adfSourceGCPs.size() % 2) == 0 )
                {
                    nGCPCount = static_cast<int>(
                                            adfSourceGCPs.size() / 2);
                    pasGCPList = static_cast<GDAL_GCP *>(
                            CPLCalloc(sizeof(GDAL_GCP), nGCPCount) );
                    for( int i = 0; i < nGCPCount; ++i )
                    {
                        pasGCPList[i].pszId = CPLStrdup("");
                        pasGCPList[i].pszInfo = CPLStrdup("");
                        // The origin used is the bottom left corner,
                        // and raw values are in inches!
                        pasGCPList[i].dfGCPPixel = adfSourceGCPs[2*i] *
                                                        CPLAtof(pszTIFFTagXRes);
                        pasGCPList[i].dfGCPLine = nRasterYSize -
                                adfSourceGCPs[2*i+1] * CPLAtof(pszTIFFTagYRes);
                        pasGCPList[i].dfGCPX = adfTargetGCPs[2*i];
                        pasGCPList[i].dfGCPY = adfTargetGCPs[2*i+1];
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
         papszPamDomains && papszPamDomains[iDomain] != NULL;
         ++iDomain )
    {
        const char *pszDomain = papszPamDomains[iDomain];
        char **papszGT_MD = CSLDuplicate(oGTiffMDMD.GetMetadata( pszDomain ));
        char **papszPAM_MD = oMDMD.GetMetadata( pszDomain );

        papszGT_MD = CSLMerge( papszGT_MD, papszPAM_MD );

        oGTiffMDMD.SetMetadata( papszGT_MD, pszDomain );
        CSLDestroy( papszGT_MD );
    }

    for( int i = 1; i <= GetRasterCount(); ++i )
    {
        GTiffRasterBand* poBand =
            reinterpret_cast<GTiffRasterBand *>(GetRasterBand(i));
        papszPamDomains = poBand->oMDMD.GetDomainList();

        for( int iDomain = 0;
             papszPamDomains && papszPamDomains[iDomain] != NULL;
             ++iDomain )
        {
            const char *pszDomain = papszPamDomains[iDomain];
            char **papszGT_MD =
                CSLDuplicate(poBand->oGTiffMDMD.GetMetadata( pszDomain ));
            char **papszPAM_MD = poBand->oMDMD.GetMetadata( pszDomain );

            papszGT_MD = CSLMerge( papszGT_MD, papszPAM_MD );

            poBand->oGTiffMDMD.SetMetadata( papszGT_MD, pszDomain );
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
        return NULL;
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
        CPLError(
            CE_Failure, CPLE_OpenFailed,
            "Unable to extract offset or filename, should take the form:\n"
            "GTIFF_DIR:<dir>:filename or GTIFF_DIR:off:<dir_offset>:filename" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    if( !GTiffOneTimeInit() )
        return NULL;

    VSILFILE* l_fpL = VSIFOpenL(pszFilename, "r");
    if( l_fpL == NULL )
        return NULL;
    TIFF *l_hTIFF = VSI_TIFFOpen( pszFilename, "r", l_fpL );
    if( l_hTIFF == NULL )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
        return NULL;
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
                CPLError(
                    CE_Failure, CPLE_OpenFailed,
                    "Requested directory %lu not found.",
                    static_cast<long unsigned int>(nOffsetRequested));
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return NULL;
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
    poDS->osFilename = poOpenInfo->pszFilename;
    poDS->poActiveDS = poDS;
    poDS->fpL = l_fpL;
    poDS->hTIFF = l_hTIFF;
    poDS->bCloseTIFFHandle = true;

    if( !EQUAL(pszFilename,poOpenInfo->pszFilename)
        && !STARTS_WITH_CI(poOpenInfo->pszFilename, "GTIFF_RAW:") )
    {
        poDS->SetPhysicalFilename( pszFilename );
        poDS->SetSubdatasetName( poOpenInfo->pszFilename );
        poDS->osFilename = pszFilename;
    }

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "Opening a specific TIFF directory is not supported in "
            "update mode. Switching to read-only" );
    }

    if( poOpenInfo->AreSiblingFilesLoaded() )
        poDS->oOvManager.TransferSiblingFiles(
            poOpenInfo->StealSiblingFiles() );

    if( poDS->OpenOffset( l_hTIFF, &(poDS->poActiveDS),
                          nOffset, false, GA_ReadOnly,
                          bAllowRGBAInterface, true ) != CE_None )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                   ConvertTransferFunctionToString()                  */
/*                                                                      */
/*      Convert a transfer function table into a string.                */
/*      Used by LoadICCProfile().                                       */
/************************************************************************/
static CPLString ConvertTransferFunctionToString( const uint16 *pTable,
                                                  uint32 nTableEntries )
{
    CPLString sValue;

    for( uint32 i = 0; i < nTableEntries; ++i )
    {
        if( i == 0 )
            sValue = sValue.Printf("%d", (uint32)pTable[i]);
        else
            sValue = sValue.Printf( "%s, %d",
                                    (const char*)sValue,
                                    (uint32)pTable[i]);
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
    if( bICCMetadataLoaded )
        return;
    bICCMetadataLoaded = true;

    if( !SetDirectory() )
        return;

    uint32 nEmbedLen = 0;
    uint8* pEmbedBuffer = NULL;


    if( TIFFGetField(hTIFF, TIFFTAG_ICCPROFILE, &nEmbedLen, &pEmbedBuffer) )
    {
        char *pszBase64Profile =
            CPLBase64Encode(nEmbedLen, (const GByte*)pEmbedBuffer);

        oGTiffMDMD.SetMetadataItem( "SOURCE_ICC_PROFILE", pszBase64Profile,
                                    "COLOR_PROFILE" );

        CPLFree(pszBase64Profile);

        return;
    }

    // Check for colorimetric tiff.
    float* pCHR = NULL;
    float* pWP = NULL;
    uint16 *pTFR = NULL;
    uint16 *pTFG = NULL;
    uint16 *pTFB = NULL;
    uint16 *pTransferRange = NULL;

    if( TIFFGetField(hTIFF, TIFFTAG_PRIMARYCHROMATICITIES, &pCHR) )
    {
        if( TIFFGetField(hTIFF, TIFFTAG_WHITEPOINT, &pWP) )
        {
            if( !TIFFGetFieldDefaulted( hTIFF, TIFFTAG_TRANSFERFUNCTION, &pTFR,
                                        &pTFG, &pTFB) )
                return;

            const int TIFFTAG_TRANSFERRANGE = 0x0156;
            TIFFGetFieldDefaulted( hTIFF, TIFFTAG_TRANSFERRANGE,
                                   &pTransferRange);

            // Set all the colorimetric metadata.
            oGTiffMDMD.SetMetadataItem(
                "SOURCE_PRIMARIES_RED",
                CPLString().Printf( "%.9f, %.9f, 1.0",
                                    static_cast<double>(pCHR[0]),
                                    static_cast<double>(pCHR[1]) ),
                "COLOR_PROFILE" );
            oGTiffMDMD.SetMetadataItem(
                "SOURCE_PRIMARIES_GREEN",
                CPLString().Printf( "%.9f, %.9f, 1.0",
                                    static_cast<double>(pCHR[2]),
                                    static_cast<double>(pCHR[3]) ),
                "COLOR_PROFILE" );
            oGTiffMDMD.SetMetadataItem(
                "SOURCE_PRIMARIES_BLUE",
                CPLString().Printf( "%.9f, %.9f, 1.0",
                                    static_cast<double>(pCHR[4]),
                                    static_cast<double>(pCHR[5]) ),
                "COLOR_PROFILE" );

            oGTiffMDMD.SetMetadataItem(
                "SOURCE_WHITEPOINT",
                CPLString().Printf( "%.9f, %.9f, 1.0",
                                    static_cast<double>(pWP[0]),
                                    static_cast<double>(pWP[1]) ),
                "COLOR_PROFILE" );

            // Set transfer function metadata.

            // Get length of table.
            const uint32 nTransferFunctionLength = 1 << nBitsPerSample;

            oGTiffMDMD.SetMetadataItem(
                "TIFFTAG_TRANSFERFUNCTION_RED",
                ConvertTransferFunctionToString( pTFR, nTransferFunctionLength),
                "COLOR_PROFILE" );

            oGTiffMDMD.SetMetadataItem(
                "TIFFTAG_TRANSFERFUNCTION_GREEN",
                ConvertTransferFunctionToString( pTFG, nTransferFunctionLength),
                "COLOR_PROFILE" );

            oGTiffMDMD.SetMetadataItem(
                "TIFFTAG_TRANSFERFUNCTION_BLUE",
                ConvertTransferFunctionToString( pTFB, nTransferFunctionLength),
                "COLOR_PROFILE" );

            // Set transfer range.
            if( pTransferRange )
            {
                oGTiffMDMD.SetMetadataItem(
                    "TIFFTAG_TRANSFERRANGE_BLACK",
                    CPLString().Printf( "%d, %d, %d",
                                        static_cast<int>(pTransferRange[0]),
                                        static_cast<int>(pTransferRange[2]),
                                        static_cast<int>(pTransferRange[4])),
                    "COLOR_PROFILE" );
                oGTiffMDMD.SetMetadataItem(
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
/*      pDS->hTIFF is NULL.                                             */
/* papszParmList:                                                       */
/*      Options containing the ICC profile or colorimetric metadata.    */
/*      Ignored if pDS is not NULL.                                     */
/* nBitsPerSample:                                                      */
/*      Bits per sample. Ignored if pDS is not NULL.                    */
/************************************************************************/

void GTiffDataset::SaveICCProfile( GTiffDataset *pDS, TIFF *l_hTIFF,
                                   char **papszParmList,
                                   uint32 l_nBitsPerSample )
{
    if( (pDS != NULL) && (pDS->eAccess != GA_Update) )
        return;

    if( l_hTIFF == NULL )
    {
        if( pDS == NULL )
            return;

        l_hTIFF = pDS->hTIFF;
        if( l_hTIFF == NULL )
            return;
    }

    if( (papszParmList == NULL) && (pDS == NULL) )
        return;

    const char *pszValue = NULL;
    if( pDS != NULL )
        pszValue = pDS->GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE");
    else
        pszValue = CSLFetchNameValue(papszParmList, "SOURCE_ICC_PROFILE");
    if( pszValue != NULL )
    {
        char *pEmbedBuffer = CPLStrdup(pszValue);
        int32 nEmbedLen =
            CPLBase64DecodeInPlace(reinterpret_cast<GByte *>(pEmbedBuffer));

        TIFFSetField(l_hTIFF, TIFFTAG_ICCPROFILE, nEmbedLen, pEmbedBuffer);

        CPLFree(pEmbedBuffer);
    }
    else
    {
        // Output colorimetric data.
        float pCHR[6] = {};  // Primaries.
        uint16 pTXR[6] = {};  // Transfer range.
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
            if( pDS != NULL )
                pszValue =
                    pDS->GetMetadataItem(pszCHRNames[i], "COLOR_PROFILE");
            else
                pszValue = CSLFetchNameValue(papszParmList, pszCHRNames[i]);
            if( pszValue == NULL )
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
        if( pDS != NULL )
            pszValue =
                pDS->GetMetadataItem("SOURCE_WHITEPOINT", "COLOR_PROFILE");
        else
            pszValue = CSLFetchNameValue(papszParmList, "SOURCE_WHITEPOINT");
        if( pszValue != NULL )
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
        char const *pszTFRed = NULL;
        if( pDS != NULL )
            pszTFRed =
                pDS->GetMetadataItem( "TIFFTAG_TRANSFERFUNCTION_RED",
                                      "COLOR_PROFILE" );
        else
            pszTFRed =
                CSLFetchNameValue( papszParmList,
                                   "TIFFTAG_TRANSFERFUNCTION_RED" );

        char const *pszTFGreen = NULL;
        if( pDS != NULL )
            pszTFGreen =
                pDS->GetMetadataItem( "TIFFTAG_TRANSFERFUNCTION_GREEN",
                                      "COLOR_PROFILE" );
        else
            pszTFGreen =
                CSLFetchNameValue( papszParmList,
                                   "TIFFTAG_TRANSFERFUNCTION_GREEN" );

        char const *pszTFBlue = NULL;
        if( pDS != NULL )
            pszTFBlue =
                pDS->GetMetadataItem( "TIFFTAG_TRANSFERFUNCTION_BLUE",
                                      "COLOR_PROFILE" );
        else
            pszTFBlue =
                CSLFetchNameValue( papszParmList,
                                   "TIFFTAG_TRANSFERFUNCTION_BLUE" );

        if( (pszTFRed != NULL) && (pszTFGreen != NULL) && (pszTFBlue != NULL) )
        {
            // Get length of table.
            const int nTransferFunctionLength =
                1 << ((pDS!=NULL)?pDS->nBitsPerSample:l_nBitsPerSample);

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
                uint16 *pTransferFuncRed =
                    static_cast<uint16*>( CPLMalloc(
                        sizeof(uint16) * nTransferFunctionLength ) );
                uint16 *pTransferFuncGreen =
                    static_cast<uint16*>( CPLMalloc(
                        sizeof(uint16) * nTransferFunctionLength ) );
                uint16 *pTransferFuncBlue =
                    static_cast<uint16*>( CPLMalloc(
                        sizeof(uint16) * nTransferFunctionLength ) );

                // Convert our table in string format into int16 format.
                for( int i = 0; i < nTransferFunctionLength; ++i )
                {
                    pTransferFuncRed[i] =
                        static_cast<uint16>(atoi(papszTokensRed[i]));
                    pTransferFuncGreen[i] =
                        static_cast<uint16>(atoi(papszTokensGreen[i]));
                    pTransferFuncBlue[i] =
                        static_cast<uint16>(atoi(papszTokensBlue[i]));
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
            if( pDS != NULL )
                pszValue = pDS->GetMetadataItem( pszTXRNames[i],
                                                 "COLOR_PROFILE" );
            else
                pszValue = CSLFetchNameValue(papszParmList, pszTXRNames[i]);
            if( pszValue == NULL )
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
                pTXR[i + j * 2] = static_cast<uint16>(atoi(papszTokens[j]));
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
                                 GTiffDataset **ppoActiveDSRefIn,
                                 toff_t nDirOffsetIn,
                                 bool bBaseIn, GDALAccess eAccessIn,
                                 bool bAllowRGBAInterface,
                                 bool bReadGeoTransform )

{
    eAccess = eAccessIn;

    hTIFF = hTIFFIn;
    ppoActiveDSRef = ppoActiveDSRefIn;

    nDirOffset = nDirOffsetIn;

    if( !SetDirectory( nDirOffsetIn ) )
        return CE_Failure;

    bBase = bBaseIn;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    uint32 nXSize = 0;
    uint32 nYSize = 0;
    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
    if( nXSize > INT_MAX || nYSize > INT_MAX )
    {
        // GDAL only supports signed 32bit dimensions.
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Too large image size: %u x %u",
                 nXSize, nYSize);
        return CE_Failure;
    }
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    if( !TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSamplesPerPixel ) )
        nBands = 1;
    else
        nBands = nSamplesPerPixel;

    if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(nBitsPerSample)) )
        nBitsPerSample = 1;

    if( !TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &(nPlanarConfig) ) )
        nPlanarConfig = PLANARCONFIG_CONTIG;

    if( !TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &(nPhotometric) ) )
        nPhotometric = PHOTOMETRIC_MINISBLACK;

    if( !TIFFGetField( hTIFF, TIFFTAG_SAMPLEFORMAT, &(nSampleFormat) ) )
        nSampleFormat = SAMPLEFORMAT_UINT;

    if( !TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &(nCompression) ) )
        nCompression = COMPRESSION_NONE;

#if defined(TIFFLIB_VERSION) && TIFFLIB_VERSION > 20031007 // 3.6.0
    if( nCompression != COMPRESSION_NONE &&
        !TIFFIsCODECConfigured(nCompression) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot open TIFF file due to missing codec." );
        return CE_Failure;
    }
#endif

/* -------------------------------------------------------------------- */
/*      YCbCr JPEG compressed images should be translated on the fly    */
/*      to RGB by libtiff/libjpeg unless specifically requested         */
/*      otherwise.                                                      */
/* -------------------------------------------------------------------- */
    if( nCompression == COMPRESSION_JPEG
        && nPhotometric == PHOTOMETRIC_YCBCR
        && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES") ) )
    {
        oGTiffMDMD.SetMetadataItem( "SOURCE_COLOR_SPACE", "YCbCr",
                                    "IMAGE_STRUCTURE" );
        int nColorMode = 0;
        if( !TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode ) ||
            nColorMode != JPEGCOLORMODE_RGB )
            TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
    }

/* -------------------------------------------------------------------- */
/*      Get strip/tile layout.                                          */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled(hTIFF) )
    {
        uint32 l_nBlockXSize = 0;
        uint32 l_nBlockYSize = 0;
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(l_nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(l_nBlockYSize) );
        if( l_nBlockXSize > INT_MAX || l_nBlockYSize > INT_MAX )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too large block size: %u x %u",
                     l_nBlockXSize, l_nBlockYSize);
            return CE_Failure;
        }
        nBlockXSize = static_cast<int>(l_nBlockXSize);
        nBlockYSize = static_cast<int>(l_nBlockYSize);
    }
    else
    {
        if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(nRowsPerStrip) ) )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "RowsPerStrip not defined ... assuming all one strip." );
            nRowsPerStrip = nYSize;  // Dummy value.
        }

        // If the rows per strip is larger than the file we will get
        // confused.  libtiff internally will treat the rowsperstrip as
        // the image height and it is best if we do too. (#4468)
        if( nRowsPerStrip > static_cast<uint32>(nRasterYSize) )
            nRowsPerStrip = nRasterYSize;

        nBlockXSize = nRasterXSize;
        nBlockYSize = nRowsPerStrip;
    }

    const int l_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);
    const int l_nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    if( l_nBlocksPerColumn > INT_MAX / l_nBlocksPerRow )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Too many blocks: %d x %d",
                  l_nBlocksPerRow, l_nBlocksPerColumn );
        return CE_Failure;
    }

    // Note: we could potentially go up to UINT_MAX blocks, but currently
    // we use a int nBlockId
    nBlocksPerBand = l_nBlocksPerColumn * l_nBlocksPerRow;
    if( nPlanarConfig == PLANARCONFIG_SEPARATE &&
        nBlocksPerBand > INT_MAX / nBands )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Too many blocks: %d x %d x %d bands",
                  l_nBlocksPerRow, l_nBlocksPerColumn, nBands );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Should we handle this using the GTiffBitmapBand?                */
/* -------------------------------------------------------------------- */
    bool bTreatAsBitmap = false;

    if( nBitsPerSample == 1 && nBands == 1 )
    {
        bTreatAsBitmap = true;

        // Lets treat large "one row" bitmaps using the scanline api.
        if( !TIFFIsTiled(hTIFF)
            && nBlockYSize == nRasterYSize
            && nRasterYSize > 2000
            // libtiff does not support reading JBIG files with
            // TIFFReadScanline().
            && nCompression != COMPRESSION_JBIG )
        {
            bTreatAsSplitBitmap = true;
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we treat this via the RGBA interface?                    */
/* -------------------------------------------------------------------- */
    if(
#ifdef DEBUG
        CPLTestBool(CPLGetConfigOption("GTIFF_FORCE_RGBA", "NO")) ||
#endif
        (bAllowRGBAInterface &&
        !bTreatAsBitmap && !(nBitsPerSample > 8)
        && (nPhotometric == PHOTOMETRIC_CIELAB ||
            nPhotometric == PHOTOMETRIC_LOGL ||
            nPhotometric == PHOTOMETRIC_LOGLUV ||
            nPhotometric == PHOTOMETRIC_SEPARATED ||
            ( nPhotometric == PHOTOMETRIC_YCBCR
              && nCompression != COMPRESSION_JPEG ))) )
    {
        char szMessage[1024] = {};

        if( TIFFRGBAImageOK( hTIFF, szMessage ) == 1 )
        {
            const char* pszSourceColorSpace = NULL;
            nBands = 4;
            switch( nPhotometric )
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
                oGTiffMDMD.SetMetadataItem( "SOURCE_COLOR_SPACE",
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
    if( nCompression == COMPRESSION_OJPEG &&
        !bTreatAsRGBA )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Old-JPEG compression only supported through RGBA interface, "
                 "which cannot be used probably because the file is corrupted");
        return CE_Failure;
    }

    // If photometric is YCbCr, scanline/strip/tile interfaces assumes that
    // we are ready with downsampled data. And we are not.
    if( nCompression != COMPRESSION_JPEG &&
        nCompression != COMPRESSION_OJPEG &&
        nPhotometric == PHOTOMETRIC_YCBCR &&
        nPlanarConfig == PLANARCONFIG_CONTIG &&
        !bTreatAsRGBA )
    {
        uint16 nF1, nF2;
        TIFFGetFieldDefaulted(hTIFF,TIFFTAG_YCBCRSUBSAMPLING,&nF1,&nF2);
        if( nF1 != 1 || nF2 != 1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot open TIFF file with YCbCr, subsampling and "
                      "BitsPerSample > 8 that is not JPEG compressed" );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we treat this via the split interface?                   */
/* -------------------------------------------------------------------- */
    if( !TIFFIsTiled(hTIFF)
        && nBitsPerSample == 8
        && nBlockYSize == nRasterYSize
        && nRasterYSize > 2000
        && !bTreatAsRGBA
        && CPLTestBool(CPLGetConfigOption("GDAL_ENABLE_TIFF_SPLIT", "YES")) )
    {
        // libtiff 3.9.2 (20091104) and older, libtiff 4.0.0beta5 (also
        // 20091104) and older will crash when trying to open a
        // all-in-one-strip YCbCr JPEG compressed TIFF (see #3259).
#if (TIFFLIB_VERSION <= 20091104 && !defined(BIGTIFF_SUPPORT)) || \
    (TIFFLIB_VERSION <= 20091104 && defined(BIGTIFF_SUPPORT))
        if( nPhotometric == PHOTOMETRIC_YCBCR &&
            nCompression == COMPRESSION_JPEG )
        {
            CPLDebug(
                "GTiff",
                "Avoid using split band to open all-in-one-strip "
                "YCbCr JPEG compressed TIFF because of older libtiff" );
        }
        else
#endif
        {
            bTreatAsSplit = true;
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we treat this via the odd bits interface?                */
/* -------------------------------------------------------------------- */
    bool bTreatAsOdd = false;
    if( nSampleFormat == SAMPLEFORMAT_IEEEFP )
    {
        if( nBitsPerSample == 16 || nBitsPerSample == 24 )
            bTreatAsOdd = true;
    }
    else if( !bTreatAsRGBA && !bTreatAsBitmap
             && nBitsPerSample != 8
             && nBitsPerSample != 16
             && nBitsPerSample != 32
             && nBitsPerSample != 64
             && nBitsPerSample != 128 )
    {
        bTreatAsOdd = true;
    }

/* -------------------------------------------------------------------- */
/*      We don't support 'chunks' bigger than 2GB although libtiff v4   */
/*      can.                                                            */
/* -------------------------------------------------------------------- */
#if defined(BIGTIFF_SUPPORT)
    uint64 nChunkSize = 0;
    if( bTreatAsSplit || bTreatAsSplitBitmap )
    {
        nChunkSize = TIFFScanlineSize64( hTIFF );
    }
    else
    {
        if( TIFFIsTiled(hTIFF) )
            nChunkSize = TIFFTileSize64( hTIFF );
        else
            nChunkSize = TIFFStripSize64( hTIFF );
    }
    if( bTreatAsRGBA )
    {
        nChunkSize = std::max(nChunkSize,
                        4 * static_cast<uint64>(nBlockXSize) * nBlockYSize);
    }
    if( nChunkSize > static_cast<uint64>(INT_MAX) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Scanline/tile/strip size bigger than 2GB." );
        return CE_Failure;
    }
#endif

    const bool bMinIsWhite = nPhotometric == PHOTOMETRIC_MINISWHITE;

/* -------------------------------------------------------------------- */
/*      Check for NODATA                                                */
/* -------------------------------------------------------------------- */
    char *pszText = NULL;
    if( TIFFGetField( hTIFF, TIFFTAG_GDAL_NODATA, &pszText ) &&
        !EQUAL(pszText, "") )
    {
        bNoDataSet = true;
        dfNoDataValue = CPLAtofM( pszText );
    }

/* -------------------------------------------------------------------- */
/*      Capture the color table if there is one.                        */
/* -------------------------------------------------------------------- */
    unsigned short *panRed = NULL;
    unsigned short *panGreen = NULL;
    unsigned short *panBlue = NULL;

    if( bTreatAsRGBA || nBitsPerSample > 16
        || TIFFGetField( hTIFF, TIFFTAG_COLORMAP,
                         &panRed, &panGreen, &panBlue) == 0 )
    {
        // Build inverted palette if we have inverted photometric.
        // Pixel values remains unchanged.  Avoid doing this for *deep*
        // data types (per #1882)
        if( nBitsPerSample <= 16 && nPhotometric == PHOTOMETRIC_MINISWHITE )
        {
            poColorTable = new GDALColorTable();
            const int nColorCount = 1 << nBitsPerSample;

            for( int iColor = 0; iColor < nColorCount; ++iColor )
            {
                const short nValue =
                    static_cast<short>(((255 * (nColorCount - 1 - iColor)) /
                                        (nColorCount - 1)));
                const GDALColorEntry oEntry =
                    { nValue, nValue, nValue, static_cast<short>(255) };
                poColorTable->SetColorEntry( iColor, &oEntry );
            }

            nPhotometric = PHOTOMETRIC_PALETTE;
        }
        else
        {
            poColorTable = NULL;
        }
    }
    else
    {
        unsigned short nMaxColor = 0;

        poColorTable = new GDALColorTable();

        const int nColorCount = 1 << nBitsPerSample;

        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            // TODO(schwehr): Ensure the color entries are never negative?
            const unsigned short divisor = 256;
            const GDALColorEntry oEntry = {
                static_cast<short>(panRed[iColor] / divisor),
                static_cast<short>(panGreen[iColor] / divisor),
                static_cast<short>(panBlue[iColor] / divisor),
                static_cast<short>(
                    bNoDataSet && static_cast<int>(dfNoDataValue) == iColor
                    ? 0
                    : 255)
            };

            poColorTable->SetColorEntry( iColor, &oEntry );

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
                    bNoDataSet &&
                    static_cast<int>(dfNoDataValue) == iColor
                    ? static_cast<short>(0)
                    : static_cast<short>(255)
                };

                poColorTable->SetColorEntry( iColor, &oEntry );
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
        else if( bTreatAsSplitBitmap )
            SetBand( iBand + 1, new GTiffSplitBitmapBand( this, iBand + 1 ) );
        else if( bTreatAsSplit )
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
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported TIFF configuration: BitsPerSample(=%d) and "
                 "SampleType(=%d)",
                 nBitsPerSample,
                 nSampleFormat);
        return CE_Failure;
    }

    m_bReadGeoTransform = bReadGeoTransform;

/* -------------------------------------------------------------------- */
/*      Capture some other potentially interesting information.         */
/* -------------------------------------------------------------------- */
    char szWorkMDI[200] = {};
    uint16 nShort = 0;

    for( size_t iTag = 0;
         iTag < sizeof(asTIFFTags) / sizeof(asTIFFTags[0]);
         ++iTag )
    {
        if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING )
        {
            if( TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &pszText ) )
                oGTiffMDMD.SetMetadataItem( asTIFFTags[iTag].pszTagName,
                                            pszText );
        }
        else if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT )
        {
            float fVal = 0.0;
            if( TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &fVal ) )
            {
                CPLsnprintf( szWorkMDI, sizeof(szWorkMDI), "%.8g", fVal );
                oGTiffMDMD.SetMetadataItem( asTIFFTags[iTag].pszTagName,
                                            szWorkMDI );
            }
        }
        else if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT &&
                 asTIFFTags[iTag].nTagVal != TIFFTAG_RESOLUTIONUNIT )
        {
            if( TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &nShort ) )
            {
                snprintf( szWorkMDI, sizeof(szWorkMDI), "%d", nShort );
                oGTiffMDMD.SetMetadataItem( asTIFFTags[iTag].pszTagName,
                                            szWorkMDI );
            }
        }
    }

    if( TIFFGetField( hTIFF, TIFFTAG_RESOLUTIONUNIT, &nShort ) )
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
        oGTiffMDMD.SetMetadataItem( "TIFFTAG_RESOLUTIONUNIT", szWorkMDI );
    }

    int nTagSize = 0;
    void* pData = NULL;
    if( TIFFGetField( hTIFF, TIFFTAG_XMLPACKET, &nTagSize, &pData ) )
    {
        char* pszXMP =
            static_cast<char *>( VSI_MALLOC_VERBOSE(nTagSize + 1) );
        if( pszXMP )
        {
            memcpy(pszXMP, pData, nTagSize);
            pszXMP[nTagSize] = '\0';

            char *apszMDList[2] = { pszXMP, NULL };
            oGTiffMDMD.SetMetadata(apszMDList, "xml:XMP");

            CPLFree(pszXMP);
        }
    }

    if( nCompression == COMPRESSION_NONE )
        /* no compression tag */;
    else if( nCompression == COMPRESSION_CCITTRLE )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "CCITTRLE",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_CCITTFAX3 )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "CCITTFAX3",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_CCITTFAX4 )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "CCITTFAX4",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_LZW )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "LZW", "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_OJPEG )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "OJPEG", "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_JPEG )
    {
        if( nPhotometric == PHOTOMETRIC_YCBCR )
            oGTiffMDMD.SetMetadataItem( "COMPRESSION", "YCbCr JPEG",
                                        "IMAGE_STRUCTURE" );
        else
            oGTiffMDMD.SetMetadataItem( "COMPRESSION", "JPEG",
                                        "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_NEXT )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "NEXT", "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_CCITTRLEW )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "CCITTRLEW",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_PACKBITS )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "PACKBITS",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_THUNDERSCAN )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "THUNDERSCAN",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_PIXARFILM )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "PIXARFILM",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_PIXARLOG )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "PIXARLOG",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_DEFLATE )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "DEFLATE",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_ADOBE_DEFLATE )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "DEFLATE",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_DCS )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "DCS", "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_JBIG )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "JBIG", "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_SGILOG )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "SGILOG",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_SGILOG24 )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "SGILOG24",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_JP2000 )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "JP2000",
                                    "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_LZMA )
    {
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", "LZMA", "IMAGE_STRUCTURE" );
    }
    else
    {
        CPLString oComp;
        oComp.Printf( "%d", nCompression);
        oGTiffMDMD.SetMetadataItem( "COMPRESSION", oComp.c_str());
    }

    if( nPlanarConfig == PLANARCONFIG_CONTIG && nBands != 1 )
        oGTiffMDMD.SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    else
        oGTiffMDMD.SetMetadataItem( "INTERLEAVE", "BAND", "IMAGE_STRUCTURE" );

    if( (GetRasterBand(1)->GetRasterDataType() == GDT_Byte &&
         nBitsPerSample != 8 ) ||
        (GetRasterBand(1)->GetRasterDataType() == GDT_UInt16 &&
         nBitsPerSample != 16) ||
        ((GetRasterBand(1)->GetRasterDataType() == GDT_UInt32 ||
          GetRasterBand(1)->GetRasterDataType() == GDT_Float32) &&
         nBitsPerSample != 32) )
    {
        for( int i = 0; i < nBands; ++i )
            static_cast<GTiffRasterBand*>(GetRasterBand(i + 1))->
                oGTiffMDMD.SetMetadataItem(
                    "NBITS",
                    CPLString().Printf(
                        "%d", static_cast<int>(nBitsPerSample) ),
                    "IMAGE_STRUCTURE" );
    }

    if( bMinIsWhite )
        oGTiffMDMD.SetMetadataItem( "MINISWHITE", "YES", "IMAGE_STRUCTURE" );

    if( TIFFGetField( hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) )
    {
        CPLXMLNode *psRoot = CPLParseXMLString( pszText );
        CPLXMLNode *psItem = NULL;

        if( psRoot != NULL && psRoot->eType == CXT_Element
            && EQUAL(psRoot->pszValue,"GDALMetadata") )
            psItem = psRoot->psChild;

        for( ; psItem != NULL; psItem = psItem->psNext )
        {

            if( psItem->eType != CXT_Element
                || !EQUAL(psItem->pszValue,"Item") )
                continue;

            const char *pszKey = CPLGetXMLValue( psItem, "name", NULL );
            const char *pszValue = CPLGetXMLValue( psItem, NULL, NULL );
            int nBand =
                atoi(CPLGetXMLValue( psItem, "sample", "-1" ));
            if( nBand < -1 || nBand > 65535 )
                continue;
            nBand ++;
            const char *pszRole = CPLGetXMLValue( psItem, "role", "" );
            const char *pszDomain = CPLGetXMLValue( psItem, "domain", "" );

            if( pszKey == NULL || pszValue == NULL )
                continue;
            if( EQUAL(pszDomain, "IMAGE_STRUCTURE") )
                continue;

            bool bIsXML = false;

            if( STARTS_WITH_CI(pszDomain, "xml:") )
                bIsXML = TRUE;

            char *pszUnescapedValue =
                CPLUnescapeString( pszValue, NULL, CPLES_XML );
            if( nBand == 0 )
            {
                if( bIsXML )
                {
                    char *apszMD[2] = { pszUnescapedValue, NULL };
                    oGTiffMDMD.SetMetadata( apszMD, pszDomain );
                }
                else
                {
                    oGTiffMDMD.SetMetadataItem( pszKey, pszUnescapedValue,
                                                pszDomain );
                }
            }
            else
            {
                GTiffRasterBand *poBand =
                    static_cast<GTiffRasterBand*>(GetRasterBand(nBand));
                if( poBand != NULL )
                {
                    if( EQUAL(pszRole,"scale") )
                    {
                        poBand->bHaveOffsetScale = true;
                        poBand->dfScale = CPLAtofM(pszUnescapedValue);
                    }
                    else if( EQUAL(pszRole,"offset") )
                    {
                        poBand->bHaveOffsetScale = true;
                        poBand->dfOffset = CPLAtofM(pszUnescapedValue);
                    }
                    else if( EQUAL(pszRole,"unittype") )
                    {
                        poBand->osUnitType = pszUnescapedValue;
                    }
                    else if( EQUAL(pszRole,"description") )
                    {
                        poBand->osDescription = pszUnescapedValue;
                    }
                    else if( EQUAL(pszRole, "colorinterp") )
                    {
                        poBand->eBandInterp =
                            GDALGetColorInterpretationByName(pszUnescapedValue);
                    }
                    else
                    {
                        if( bIsXML )
                        {
                            char *apszMD[2] = { pszUnescapedValue, NULL };
                            poBand->oGTiffMDMD.SetMetadata( apszMD, pszDomain );
                        }
                        else
                        {
                            poBand->oGTiffMDMD.SetMetadataItem(
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

    if( bStreamingIn )
    {
        toff_t* panOffsets = NULL;
        TIFFGetField( hTIFF,
                      TIFFIsTiled( hTIFF ) ?
                      TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS,
                      &panOffsets );
        if( panOffsets )
        {
            int nBlockCount =
                TIFFIsTiled(hTIFF) ?
                TIFFNumberOfTiles(hTIFF) : TIFFNumberOfStrips(hTIFF);
            for( int i = 1; i < nBlockCount; ++i )
            {
                if( panOffsets[i] < panOffsets[i-1] )
                {
                    oGTiffMDMD.SetMetadataItem( "UNORDERED_BLOCKS", "YES",
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

    if( nCompression == COMPRESSION_JPEG && eAccess == GA_Update )
    {
        SetJPEGQualityAndTablesModeFromFile();
    }

    CPLAssert(m_bReadGeoTransform == bReadGeoTransform);
    CPLAssert(!bMetadataChanged);
    bMetadataChanged = false;

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
        VSIReadDirEx(CPLGetDirname(osFilename), nMaxFiles);
    if( nMaxFiles > 0 && CSLCount(papszSiblingFiles) > nMaxFiles )
    {
        CPLDebug("GTiff", "GDAL_READDIR_LIMIT_ON_OPEN reached on %s",
                 CPLGetDirname(osFilename));
        CSLDestroy(papszSiblingFiles);
        papszSiblingFiles = NULL;
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
    m_nPAMGeorefSrcIndex = CSLFindString(papszTokens, "PAM");
    m_nINTERNALGeorefSrcIndex = CSLFindString(papszTokens, "INTERNAL");
    m_nTABFILEGeorefSrcIndex = CSLFindString(papszTokens, "TABFILE");
    m_nWORLDFILEGeorefSrcIndex = CSLFindString(papszTokens, "WORLDFILE");
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

        if( !SetDirectory() )
            return;

        char *pszTabWKT = NULL;
        double *padfTiePoints = NULL;
        double *padfScale = NULL;
        double *padfMatrix = NULL;
        uint16 nCount = 0;
        bool bPixelIsPoint = false;
        short nRasterType = 0;
        bool bPointGeoIgnore = false;

        std::set<int> aoSetPriorities;
        if( m_nINTERNALGeorefSrcIndex >= 0 )
            aoSetPriorities.insert(m_nINTERNALGeorefSrcIndex);
        if( m_nTABFILEGeorefSrcIndex >= 0 )
            aoSetPriorities.insert(m_nTABFILEGeorefSrcIndex);
        if( m_nWORLDFILEGeorefSrcIndex >= 0 )
            aoSetPriorities.insert(m_nWORLDFILEGeorefSrcIndex);
        std::set<int>::iterator oIter = aoSetPriorities.begin();
        for( ; oIter != aoSetPriorities.end(); ++oIter )
        {
            int nIndex = *oIter;
            if( m_nINTERNALGeorefSrcIndex == nIndex )
            {
                GTIF *psGTIF = GTiffDatasetGTIFNew( hTIFF );  // How expensive this is?

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

                adfGeoTransform[0] = 0.0;
                adfGeoTransform[1] = 1.0;
                adfGeoTransform[2] = 0.0;
                adfGeoTransform[3] = 0.0;
                adfGeoTransform[4] = 0.0;
                adfGeoTransform[5] = 1.0;

                if( TIFFGetField(hTIFF, TIFFTAG_GEOPIXELSCALE,
                                 &nCount, &padfScale )
                    && nCount >= 2
                    && padfScale[0] != 0.0 && padfScale[1] != 0.0 )
                {
                    adfGeoTransform[1] = padfScale[0];
                    adfGeoTransform[5] = -std::abs(padfScale[1]);

                    if( TIFFGetField(hTIFF, TIFFTAG_GEOTIEPOINTS,
                                     &nCount, &padfTiePoints )
                        && nCount >= 6 )
                    {
                        adfGeoTransform[0] =
                            padfTiePoints[3] -
                            padfTiePoints[0] * adfGeoTransform[1];
                        adfGeoTransform[3] =
                            padfTiePoints[4] -
                            padfTiePoints[1] * adfGeoTransform[5];

                        if( bPixelIsPoint && !bPointGeoIgnore )
                        {
                            adfGeoTransform[0] -=
                                (adfGeoTransform[1] * 0.5 +
                                 adfGeoTransform[2] * 0.5);
                            adfGeoTransform[3] -=
                                (adfGeoTransform[4] * 0.5 +
                                 adfGeoTransform[5] * 0.5);
                        }

                        bGeoTransformValid = true;
                        m_nGeoTransformGeorefSrcIndex = nIndex;
                    }
                }

                else if( TIFFGetField(hTIFF, TIFFTAG_GEOTRANSMATRIX,
                                      &nCount, &padfMatrix )
                        && nCount == 16 )
                {
                    adfGeoTransform[0] = padfMatrix[3];
                    adfGeoTransform[1] = padfMatrix[0];
                    adfGeoTransform[2] = padfMatrix[1];
                    adfGeoTransform[3] = padfMatrix[7];
                    adfGeoTransform[4] = padfMatrix[4];
                    adfGeoTransform[5] = padfMatrix[5];

                    if( bPixelIsPoint && !bPointGeoIgnore )
                    {
                        adfGeoTransform[0] -=
                            adfGeoTransform[1] * 0.5 + adfGeoTransform[2] * 0.5;
                        adfGeoTransform[3] -=
                            adfGeoTransform[4] * 0.5 + adfGeoTransform[5] * 0.5;
                    }

                    bGeoTransformValid = true;
                    m_nGeoTransformGeorefSrcIndex = nIndex;
                }
                if( bGeoTransformValid )
                    break;
            }

/* -------------------------------------------------------------------- */
/*      Otherwise try looking for a .tab, .tfw, .tifw or .wld file.     */
/* -------------------------------------------------------------------- */
            if( m_nTABFILEGeorefSrcIndex == nIndex )
            {
                char* pszGeorefFilename = NULL;

                char** papszSiblingFiles = GetSiblingFiles();

                // Begin with .tab since it can also have projection info.
                const int bTabFileOK =
                    GDALReadTabFile2( osFilename, adfGeoTransform,
                                        &pszTabWKT, &nGCPCount, &pasGCPList,
                                        papszSiblingFiles, &pszGeorefFilename );

                if( bTabFileOK )
                {
                    m_nGeoTransformGeorefSrcIndex = nIndex;
                    // if( pszTabWKT )
                    // {
                    //     m_nProjectionGeorefSrcIndex = nIndex;
                    // }
                    if( nGCPCount == 0 )
                    {
                        bGeoTransformValid = true;
                    }
                }

                if( pszGeorefFilename )
                {
                    osGeorefFilename = pszGeorefFilename;
                    CPLFree(pszGeorefFilename);
                }
                if( bGeoTransformValid )
                    break;
            }

            if( m_nWORLDFILEGeorefSrcIndex == nIndex )
            {
                char* pszGeorefFilename = NULL;

                char** papszSiblingFiles = GetSiblingFiles();

                bGeoTransformValid = CPL_TO_BOOL( GDALReadWorldFile2(
                                osFilename, NULL, adfGeoTransform,
                                papszSiblingFiles, &pszGeorefFilename) );

                if( !bGeoTransformValid )
                {
                    bGeoTransformValid =
                        CPL_TO_BOOL( GDALReadWorldFile2(
                            osFilename, "wld", adfGeoTransform,
                            papszSiblingFiles, &pszGeorefFilename ) );
                }
                if( bGeoTransformValid )
                    m_nGeoTransformGeorefSrcIndex = nIndex;

                if( pszGeorefFilename )
                {
                    osGeorefFilename = pszGeorefFilename;
                    CPLFree(pszGeorefFilename);
                }
                if( bGeoTransformValid )
                    break;
            }
        }

/* -------------------------------------------------------------------- */
/*      Check for GCPs.                                                 */
/* -------------------------------------------------------------------- */
        if( m_nINTERNALGeorefSrcIndex >= 0 &&
            TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
            && !bGeoTransformValid )
        {
            if( nGCPCount > 0 )
            {
                GDALDeinitGCPs( nGCPCount, pasGCPList );
                CPLFree( pasGCPList );
            }
            nGCPCount = nCount / 6;
            pasGCPList =
                static_cast<GDAL_GCP *>(CPLCalloc(sizeof(GDAL_GCP), nGCPCount));

            for( int iGCP = 0; iGCP < nGCPCount; ++iGCP )
            {
                char szID[32] = {};

                snprintf( szID, sizeof(szID), "%d", iGCP + 1 );
                pasGCPList[iGCP].pszId = CPLStrdup( szID );
                pasGCPList[iGCP].pszInfo = CPLStrdup("");
                pasGCPList[iGCP].dfGCPPixel = padfTiePoints[iGCP*6+0];
                pasGCPList[iGCP].dfGCPLine = padfTiePoints[iGCP*6+1];
                pasGCPList[iGCP].dfGCPX = padfTiePoints[iGCP*6+3];
                pasGCPList[iGCP].dfGCPY = padfTiePoints[iGCP*6+4];
                pasGCPList[iGCP].dfGCPZ = padfTiePoints[iGCP*6+5];

                if( bPixelIsPoint && !bPointGeoIgnore )
                {
                    pasGCPList[iGCP].dfGCPPixel -= 0.5;
                    pasGCPList[iGCP].dfGCPLine -= 0.5;
                }
            }
            m_nGeoTransformGeorefSrcIndex = m_nINTERNALGeorefSrcIndex;
        }

/* -------------------------------------------------------------------- */
/*      Did we find a tab file?  If so we will use its coordinate       */
/*      system and give it precedence.                                  */
/* -------------------------------------------------------------------- */
        if( pszTabWKT != NULL
            && (pszProjection == NULL || pszProjection[0] == '\0') )
        {
            CPLFree( pszProjection );
            pszProjection = pszTabWKT;
            pszTabWKT = NULL;
            bLookedForProjection = true;
        }

        CPLFree( pszTabWKT );
    }

    if( m_bLoadPam && m_nPAMGeorefSrcIndex >= 0 )
    {
/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
        CPLAssert(!bColorProfileMetadataChanged);
        CPLAssert(!bMetadataChanged);
        CPLAssert(!bGeoTIFFInfoChanged);
        CPLAssert(!bNoDataChanged);

        // We must absolutely unset m_bLoadPam now, otherwise calling
        // GetFileList() on a .tif with a .aux will result in an (almost)
        // endless sequence of calls.
        m_bLoadPam = false;

        TryLoadXML( GetSiblingFiles() );
        ApplyPamInfo();

        bColorProfileMetadataChanged = false;
        bMetadataChanged = false;
        bGeoTIFFInfoChanged = false;
        bNoDataChanged = false;

        for( int i = 1; i <= nBands; ++i )
        {
            GTiffRasterBand* poBand =
                reinterpret_cast<GTiffRasterBand *>(GetRasterBand(i));

            /* Load scale, offset and unittype from PAM if available */
            if( !poBand->bHaveOffsetScale )
            {
                int nHaveOffsetScale = FALSE;
                poBand->dfScale =
                    poBand->GDALPamRasterBand::GetScale( &nHaveOffsetScale );
                poBand->bHaveOffsetScale = CPL_TO_BOOL(nHaveOffsetScale);
                poBand->dfOffset = poBand->GDALPamRasterBand::GetOffset();
            }
            if( poBand->osUnitType.empty() )
            {
                const char* pszUnitType =
                    poBand->GDALPamRasterBand::GetUnitType();
                if( pszUnitType )
                    poBand->osUnitType = pszUnitType;
            }
            if( poBand->osDescription.empty() )
                poBand->osDescription =
                    poBand->GDALPamRasterBand::GetDescription();

            GDALColorInterp ePAMColorInterp =
                poBand->GDALPamRasterBand::GetColorInterpretation();
            if( ePAMColorInterp != GCI_Undefined )
                poBand->eBandInterp = ePAMColorInterp;
        }
    }
    m_bLoadPam = false;
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
    if( !bScanDeferred )
        return;

    bScanDeferred = false;

    if( !bBase )
        return;

    if( TIFFLastDirectory( hTIFF ) )
        return;

    CPLDebug( "GTiff", "ScanDirectories()" );

/* ==================================================================== */
/*      Scan all directories.                                           */
/* ==================================================================== */
    CPLStringList aosSubdatasets;
    int iDirIndex = 0;

    FlushDirectory();
    while( !TIFFLastDirectory( hTIFF )
           && (iDirIndex == 0 || TIFFReadDirectory( hTIFF ) != 0) )
    {
        // Only libtiff 4.0.4 can handle between 32768 and 65535 directories.
#if !defined(INTERNAL_LIBTIFF) && (!defined(TIFFLIB_VERSION) || (TIFFLIB_VERSION < 20120922))
        if( iDirIndex == 32768 )
            break;
#endif
        toff_t nThisDir = TIFFCurrentDirOffset(hTIFF);
        uint32 nSubType = 0;

        *ppoActiveDSRef = NULL; // Our directory no longer matches this ds.

        ++iDirIndex;

        if( !TIFFGetField(hTIFF, TIFFTAG_SUBFILETYPE, &nSubType) )
            nSubType = 0;

        /* Embedded overview of the main image */
        if( (nSubType & FILETYPE_REDUCEDIMAGE) != 0 &&
            (nSubType & FILETYPE_MASK) == 0 &&
            iDirIndex != 1 &&
            nOverviewCount < 30 /* to avoid DoS */ )
        {
            GTiffDataset *poODS = new GTiffDataset();
            if( poODS->OpenOffset( hTIFF, ppoActiveDSRef, nThisDir, false,
                                   eAccess ) != CE_None
                || poODS->GetRasterCount() != GetRasterCount() )
            {
                delete poODS;
            }
            else
            {
                CPLDebug( "GTiff", "Opened %dx%d overview.",
                          poODS->GetRasterXSize(), poODS->GetRasterYSize());
                ++nOverviewCount;
                papoOverviewDS = static_cast<GTiffDataset **>(
                    CPLRealloc(papoOverviewDS,
                               nOverviewCount * (sizeof(void*))) );
                papoOverviewDS[nOverviewCount-1] = poODS;
                poODS->poBaseDS = this;
            }
        }
        // Embedded mask of the main image.
        else if( (nSubType & FILETYPE_MASK) != 0 &&
                 (nSubType & FILETYPE_REDUCEDIMAGE) == 0 &&
                 iDirIndex != 1 &&
                 poMaskDS == NULL )
        {
            poMaskDS = new GTiffDataset();

            // The TIFF6 specification - page 37 - only allows 1
            // SamplesPerPixel and 1 BitsPerSample Here we support either 1 or
            // 8 bit per sample and we support either 1 sample per pixel or as
            // many samples as in the main image We don't check the value of
            // the PhotometricInterpretation tag, which should be set to
            // "Transparency mask" (4) according to the specification (page
            // 36).  However, the TIFF6 specification allows image masks to
            // have a higher resolution than the main image, what we don't
            // support here.

            if( poMaskDS->OpenOffset( hTIFF, ppoActiveDSRef, nThisDir,
                                      false, eAccess ) != CE_None
                || poMaskDS->GetRasterCount() == 0
                || !(poMaskDS->GetRasterCount() == 1
                     || poMaskDS->GetRasterCount() == GetRasterCount())
                || poMaskDS->GetRasterXSize() != GetRasterXSize()
                || poMaskDS->GetRasterYSize() != GetRasterYSize()
                || poMaskDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
            {
                delete poMaskDS;
                poMaskDS = NULL;
            }
            else
            {
                CPLDebug( "GTiff", "Opened band mask.");
                poMaskDS->poBaseDS = this;

                poMaskDS->bPromoteTo8Bits =
                    CPLTestBool(
                        CPLGetConfigOption( "GDAL_TIFF_INTERNAL_MASK_TO_8BIT",
                                            "YES" ) );
            }
        }

        // Embedded mask of an overview.  The TIFF6 specification allows the
        // combination of the FILETYPE_xxxx masks.
        else if( (nSubType & FILETYPE_REDUCEDIMAGE) != 0 &&
                 (nSubType & FILETYPE_MASK) != 0 &&
                 iDirIndex != 1 )
        {
            GTiffDataset* poDS = new GTiffDataset();
            if( poDS->OpenOffset( hTIFF, ppoActiveDSRef, nThisDir, FALSE,
                                  eAccess ) != CE_None
                || poDS->GetRasterCount() == 0
                || poDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
            {
                delete poDS;
            }
            else
            {
                int i = 0;  // Used after for.
                for( ; i < nOverviewCount; ++i )
                {
                    if( reinterpret_cast<GTiffDataset *>(
                           papoOverviewDS[i])->poMaskDS == NULL &&
                        poDS->GetRasterXSize() ==
                        papoOverviewDS[i]->GetRasterXSize() &&
                        poDS->GetRasterYSize() ==
                        papoOverviewDS[i]->GetRasterYSize() &&
                        (poDS->GetRasterCount() == 1 ||
                         poDS->GetRasterCount() == GetRasterCount()))
                    {
                        CPLDebug(
                            "GTiff", "Opened band mask for %dx%d overview.",
                            poDS->GetRasterXSize(), poDS->GetRasterYSize());
                        reinterpret_cast<GTiffDataset*>(papoOverviewDS[i])->
                            poMaskDS = poDS;
                        poDS->bPromoteTo8Bits =
                            CPLTestBool(
                                CPLGetConfigOption(
                                    "GDAL_TIFF_INTERNAL_MASK_TO_8BIT",
                                    "YES" ) );
                        poDS->poBaseDS = this;
                        break;
                    }
                }
                if( i == nOverviewCount )
                {
                    delete poDS;
                }
            }
        }
        else if( nSubType == 0 || nSubType == FILETYPE_PAGE )
        {
            uint32 nXSize = 0;
            uint32 nYSize = 0;

            TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
            TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );

            if( nXSize > INT_MAX || nYSize > INT_MAX )
            {
                CPLDebug("GTiff",
                         "Skipping directory with too large image: %u x %u",
                         nXSize, nYSize);
            }
            else
            {
                uint16 nSPP = 0;
                if( !TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSPP ) )
                    nSPP = 1;

                CPLString osName, osDesc;
                osName.Printf( "SUBDATASET_%d_NAME=GTIFF_DIR:%d:%s",
                            iDirIndex, iDirIndex, osFilename.c_str() );
                osDesc.Printf( "SUBDATASET_%d_DESC=Page %d (%dP x %dL x %dB)",
                            iDirIndex, iDirIndex,
                            static_cast<int>(nXSize),
                            static_cast<int>(nYSize),
                            nSPP );

                aosSubdatasets.AddString(osName);
                aosSubdatasets.AddString(osDesc);
            }
        }

        // Make sure we are stepping from the expected directory regardless
        // of churn done processing the above.
        if( TIFFCurrentDirOffset(hTIFF) != nThisDir )
            TIFFSetSubDirectory( hTIFF, nThisDir );
        *ppoActiveDSRef = NULL;
    }

    // Nasty hack. Probably something that should be fixed in libtiff
    // In case the last directory cycles to the first directory, we have
    // TIFFCurrentDirOffset(hTIFF) == nDirOffset, but the TIFFReadDirectory()
    // hasn't done its job, so SetDirectory() would be confused and think it
    // has nothing to do. To avoid that reset to a fake offset before calling
    // SetDirectory()
    if( TIFFCurrentDirOffset(hTIFF) == nDirOffset )
    {
        TIFFSetSubDirectory( hTIFF, 0 );
        *ppoActiveDSRef = NULL;
        CPL_IGNORE_RET_VAL( SetDirectory() );
    }

    // If we have a mask for the main image, loop over the overviews, and if
    // they have a mask, let's set this mask as an overview of the main mask.
    if( poMaskDS != NULL )
    {
        for( int i = 0; i < nOverviewCount; ++i )
        {
            if( reinterpret_cast<GTiffDataset *>(
                   papoOverviewDS[i])->poMaskDS != NULL)
            {
                ++poMaskDS->nOverviewCount;
                poMaskDS->papoOverviewDS = static_cast<GTiffDataset **>(
                    CPLRealloc(poMaskDS->papoOverviewDS,
                               poMaskDS->nOverviewCount * (sizeof(void*))) );
                poMaskDS->papoOverviewDS[poMaskDS->nOverviewCount-1] =
                    reinterpret_cast<GTiffDataset*>(
                        papoOverviewDS[i])->poMaskDS;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Only keep track of subdatasets if we have more than one         */
/*      subdataset (pair).                                              */
/* -------------------------------------------------------------------- */
    if( aosSubdatasets.size() > 2 )
    {
        oGTiffMDMD.SetMetadata( aosSubdatasets.List(), "SUBDATASETS" );
    }
}

static int GTiffGetLZMAPreset(char** papszOptions)
{
    int nLZMAPreset = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "LZMA_PRESET" );
    if( pszValue != NULL )
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
    return nLZMAPreset;
}

static int GTiffGetZLevel(char** papszOptions)
{
    int nZLevel = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "ZLEVEL" );
    if( pszValue != NULL )
    {
        nZLevel = atoi( pszValue );
        if( nZLevel < 1 || nZLevel > 9 )
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "ZLEVEL=%s value not recognised, ignoring.",
                      pszValue );
            nZLevel = -1;
        }
    }
    return nZLevel;
}

static int GTiffGetJpegQuality(char** papszOptions)
{
    int nJpegQuality = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "JPEG_QUALITY" );
    if( pszValue != NULL )
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
    return nJpegQuality;
}

static int GTiffGetJpegTablesMode(char** papszOptions)
{
    return atoi(CSLFetchNameValueDef( papszOptions, "JPEGTABLESMODE",
                                      CPLSPrintf("%d",
                                                knGTIFFJpegTablesModeDefault)));
}

/************************************************************************/
/*                        GetDiscardLsbOption()                         */
/************************************************************************/

void GTiffDataset::GetDiscardLsbOption(char** papszOptions)
{
    const char* pszBits = CSLFetchNameValue( papszOptions, "DISCARD_LSB" );
    if( pszBits == NULL)
        return;

    if( nPhotometric == PHOTOMETRIC_PALETTE )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DISCARD_LSB ignored on a paletted image");
        return;
    }
    if( !(nBitsPerSample == 8 || nBitsPerSample == 16 || nBitsPerSample == 32) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DISCARD_LSB ignored on non 8, 16 or 32 bits integer images");
        return;
    }

    char** papszTokens = CSLTokenizeString2( pszBits, ",", 0 );
    if( CSLCount(papszTokens) == 1 )
    {
        bHasDiscardedLsb = true;
        for( int i = 0; i < nBands; ++i )
        {
            int nBits = atoi(papszTokens[0]);
            anMaskLsb.push_back(~((1 << nBits)-1));
            if( nBits > 1 )
                anOffsetLsb.push_back(1 << (nBits - 1));
            else
                anOffsetLsb.push_back(0);
        }
    }
    else if( CSLCount(papszTokens) == nBands )
    {
        bHasDiscardedLsb = true;
        for( int i = 0; i < nBands; ++i )
        {
            int nBits = atoi(papszTokens[i]);
            anMaskLsb.push_back(~((1 << nBits)-1));
            if( nBits > 1 )
                anOffsetLsb.push_back(1 << (nBits - 1));
            else
                anOffsetLsb.push_back(0);
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DISCARD_LSB ignored: wrong number of components");
    }
    CSLDestroy(papszTokens);
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
                              char **papszParmList,
                              VSILFILE** pfpL,
                              CPLString& l_osTmpFilename )

{
    if( !GTiffOneTimeInit() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Blow on a few errors.                                           */
/* -------------------------------------------------------------------- */
    if( nXSize < 1 || nYSize < 1 || l_nBands < 1 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Attempt to create %dx%dx%d TIFF file, but width, height and bands"
            "must be positive.",
            nXSize, nYSize, l_nBands );

        return NULL;
    }

    if( l_nBands > 65535 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create %dx%dx%d TIFF file, but bands "
                  "must be lesser or equal to 65535.",
                  nXSize, nYSize, l_nBands );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup values based on options.                                  */
/* -------------------------------------------------------------------- */
    const char *pszProfile = CSLFetchNameValue(papszParmList, "PROFILE");
    if( pszProfile == NULL )
        pszProfile = "GDALGeoTIFF";

    const bool bTiled = CPLFetchBool( papszParmList, "TILED", false );

    int l_nBlockXSize = 0;
    const char *pszValue = CSLFetchNameValue(papszParmList, "BLOCKXSIZE");
    if( pszValue != NULL )
        l_nBlockXSize = atoi( pszValue );

    int l_nBlockYSize = 0;
    pszValue = CSLFetchNameValue(papszParmList, "BLOCKYSIZE");
    if( pszValue != NULL )
        l_nBlockYSize = atoi( pszValue );

    int nPlanar = 0;
    pszValue = CSLFetchNameValue(papszParmList, "INTERLEAVE");
    if( pszValue != NULL )
    {
        if( EQUAL( pszValue, "PIXEL" ) )
            nPlanar = PLANARCONFIG_CONTIG;
        else if( EQUAL( pszValue, "BAND" ) )
        {
            nPlanar = PLANARCONFIG_SEPARATE;
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "INTERLEAVE=%s unsupported, value must be PIXEL or BAND.",
                      pszValue );
            return NULL;
        }
    }
    else
    {
        nPlanar = PLANARCONFIG_CONTIG;
    }

    int l_nCompression = COMPRESSION_NONE;
    pszValue = CSLFetchNameValue( papszParmList, "COMPRESS" );
    if( pszValue != NULL )
    {
        l_nCompression = GTIFFGetCompressionMethod(pszValue, "COMPRESS");
        if( l_nCompression < 0 )
            return NULL;
    }

    int nPredictor = PREDICTOR_NONE;
    pszValue = CSLFetchNameValue( papszParmList, "PREDICTOR" );
    if( pszValue != NULL )
        nPredictor = atoi( pszValue );

    const int l_nZLevel = GTiffGetZLevel(papszParmList);
    const int l_nLZMAPreset = GTiffGetLZMAPreset(papszParmList);
    const int l_nJpegQuality = GTiffGetJpegQuality(papszParmList);
    const int l_nJpegTablesMode = GTiffGetJpegTablesMode(papszParmList);

/* -------------------------------------------------------------------- */
/*      Streaming related code                                          */
/* -------------------------------------------------------------------- */
    const CPLString osOriFilename(pszFilename);
    bool bStreaming =
        strcmp(pszFilename, "/vsistdout/") == 0 ||
        CPLFetchBool(papszParmList, "STREAMABLE_OUTPUT", false);
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
                CSLFetchNameValueDef(papszParmList, "COMPRESS", "NONE")) )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Streaming only supported to uncompressed TIFF" );
        return NULL;
    }
    if( bStreaming && CPLFetchBool(papszParmList, "SPARSE_OK", false) )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Streaming not supported with SPARSE_OK" );
        return NULL;
    }
    if( bStreaming && CPLFetchBool(papszParmList, "COPY_SRC_OVERVIEWS", false) )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Streaming not supported with COPY_SRC_OVERVIEWS" );
        return NULL;
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

    if( l_nCompression == COMPRESSION_NONE
        && dfUncompressedImageSize > 4200000000.0 )
    {
#ifndef BIGTIFF_SUPPORT
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "A %d pixels x %d lines x %d bands %s image would be larger than "
            "4GB but this is the largest size a TIFF can be, and BigTIFF "
            "is unavailable.  Creation failed.",
            nXSize, nYSize, l_nBands, GDALGetDataTypeName(eType) );
        return NULL;
#endif
    }

/* -------------------------------------------------------------------- */
/*      Check free space (only for big, non sparse, uncompressed)       */
/* -------------------------------------------------------------------- */
    if( l_nCompression == COMPRESSION_NONE &&
        dfUncompressedImageSize >= 1e9 &&
        !CPLFetchBool(papszParmList, "SPARSE_OK", false) &&
        osOriFilename != "/vsistdout/" &&
        osOriFilename != "/vsistdout_redirect/" &&
        CPLTestBool(CPLGetConfigOption("CHECK_DISK_FREE_SPACE", "TRUE")) )
    {
        GIntBig nFreeDiskSpace =
            VSIGetDiskFreeSpace(CPLGetDirname(pszFilename));
        if( nFreeDiskSpace >= 0 &&
            nFreeDiskSpace < dfUncompressedImageSize )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Free disk space available is " CPL_FRMT_GIB " bytes, "
                      "whereas " CPL_FRMT_GIB " are at least necessary. "
                      "You can disable this check by defining the "
                      "CHECK_DISK_FREE_SPACE configuration option to FALSE.",
                      nFreeDiskSpace,
                      static_cast<GIntBig>(dfUncompressedImageSize) );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Should the file be created as a bigtiff file?                   */
/* -------------------------------------------------------------------- */
    const char *pszBIGTIFF = CSLFetchNameValue(papszParmList, "BIGTIFF");

    if( pszBIGTIFF == NULL )
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
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "The TIFF file will be larger than 4GB, so BigTIFF is "
                "necessary.  Creation failed.");
            return NULL;
        }
    }

#ifndef BIGTIFF_SUPPORT
    if( bCreateBigTIFF )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "BigTIFF requested, but GDAL built without BigTIFF "
                  "enabled libtiff, request ignored." );
        bCreateBigTIFF = false;
    }
#endif

    if( bCreateBigTIFF )
        CPLDebug( "GTiff", "File being created as a BigTIFF." );

/* -------------------------------------------------------------------- */
/*      Check if the user wishes a particular endianness                */
/* -------------------------------------------------------------------- */

    int eEndianness = ENDIANNESS_NATIVE;
    pszValue = CSLFetchNameValue(papszParmList, "ENDIANNESS");
    if( pszValue == NULL )
        pszValue = CPLGetConfigOption( "GDAL_TIFF_ENDIANNESS", NULL );
    if( pszValue != NULL )
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
            CPLError(
                CE_Warning, CPLE_NotSupported,
                "ENDIANNESS=%s not supported. Defaulting to NATIVE", pszValue );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */

    char szOpeningFlag[5] = {};
    strcpy(szOpeningFlag, "w+");
    if( bCreateBigTIFF )
        strcat(szOpeningFlag, "8");
    if( eEndianness == ENDIANNESS_BIG )
        strcat(szOpeningFlag, "b");
    else if( eEndianness == ENDIANNESS_LITTLE )
        strcat(szOpeningFlag, "l");

    VSILFILE* l_fpL = VSIFOpenL( pszFilename, "w+b" );
    if( l_fpL == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create new tiff file `%s' failed: %s",
                  pszFilename, VSIStrerror(errno) );
        return NULL;
    }
    TIFF *l_hTIFF = VSI_TIFFOpen( pszFilename, szOpeningFlag, l_fpL );
    if( l_hTIFF == NULL )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attempt to create new tiff file `%s' "
                      "failed in XTIFFOpen().",
                      pszFilename );
        CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      How many bits per sample?  We have a special case if NBITS      */
/*      specified for GDT_Byte, GDT_UInt16, GDT_UInt32.                 */
/* -------------------------------------------------------------------- */
    int l_nBitsPerSample = GDALGetDataTypeSizeBits(eType);
    if( CSLFetchNameValue(papszParmList, "NBITS") != NULL )
    {
        int nMinBits = 0;
        int nMaxBits = 0;
        l_nBitsPerSample = atoi(CSLFetchNameValue(papszParmList, "NBITS"));
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
                CPLError(CE_Warning, CPLE_NotSupported,
                     "NBITS is not supported for data type %s",
                     GDALGetDataTypeName(eType));
                l_nBitsPerSample = GDALGetDataTypeSizeBits(eType);
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "NBITS is not supported for data type %s",
                     GDALGetDataTypeName(eType));
            l_nBitsPerSample = GDALGetDataTypeSizeBits(eType);
        }

        if( nMinBits != 0 )
        {
            if( l_nBitsPerSample < nMinBits )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "NBITS=%d is invalid for data type %s. Using NBITS=%d",
                         l_nBitsPerSample, GDALGetDataTypeName(eType),
                         nMinBits);
                l_nBitsPerSample = nMinBits;
            }
            else if( l_nBitsPerSample > nMaxBits )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
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
    const char *pszPixelType = CSLFetchNameValue( papszParmList, "PIXELTYPE" );
    if( pszPixelType == NULL )
        pszPixelType = "";

/* -------------------------------------------------------------------- */
/*      Setup some standard flags.                                      */
/* -------------------------------------------------------------------- */
    TIFFSetField( l_hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( l_hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( l_hTIFF, TIFFTAG_BITSPERSAMPLE, l_nBitsPerSample );

    uint16 l_nSampleFormat = 0;
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

    pszValue = CSLFetchNameValue(papszParmList,"PHOTOMETRIC");
    if( pszValue != NULL )
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
                CPLError(
                    CE_Warning, CPLE_AppDefined,
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
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Currently, PHOTOMETRIC=YCBCR requires COMPRESS=JPEG");
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return NULL;
            }

            if( nPlanar == PLANARCONFIG_SEPARATE )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "PHOTOMETRIC=YCBCR requires INTERLEAVE=PIXEL");
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return NULL;
            }

            // YCBCR strictly requires 3 bands. Not less, not more Issue an
            // explicit error message as libtiff one is a bit cryptic:
            // TIFFVStripSize64:Invalid td_samplesperpixel value.
            if( l_nBands != 3 )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "PHOTOMETRIC=YCBCR requires a source raster with "
                    "only 3 bands (RGB)" );
                XTIFFClose(l_hTIFF);
                CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
                return NULL;
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
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "PHOTOMETRIC=%s value not recognised, ignoring.  "
                      "Set the Photometric Interpretation as MINISBLACK.",
                      pszValue );
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        }

        if( l_nBands < nSamplesAccountedFor )
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
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
            uint16 v[1] = {
                GTiffGetAlphaValue(CSLFetchNameValue(papszParmList, "ALPHA"),
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

        uint16 *v = static_cast<uint16 *>(
            CPLMalloc( sizeof(uint16) * nExtraSamples ) );

        v[0] = GTiffGetAlphaValue( CSLFetchNameValue(papszParmList, "ALPHA"),
                                   EXTRASAMPLE_UNSPECIFIED );

        for( int i = 1; i < nExtraSamples; ++i )
            v[i] = EXTRASAMPLE_UNSPECIFIED;

        TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, nExtraSamples, v );

        CPLFree(v);
    }

    // Set the ICC color profile.
    if( !EQUAL(pszProfile,"BASELINE") )
    {
        SaveICCProfile(NULL, l_hTIFF, papszParmList, l_nBitsPerSample);
    }

    // Set the compression method before asking the default strip size
    // This is useful when translating to a JPEG-In-TIFF file where
    // the default strip size is 8 or 16 depending on the photometric value.
    TIFFSetField( l_hTIFF, TIFFTAG_COMPRESSION, l_nCompression );

/* -------------------------------------------------------------------- */
/*      Setup tiling/stripping flags.                                   */
/* -------------------------------------------------------------------- */
    if( bTiled )
    {
        if( l_nBlockXSize == 0 )
            l_nBlockXSize = 256;

        if( l_nBlockYSize == 0 )
            l_nBlockYSize = 256;

        if( !TIFFSetField( l_hTIFF, TIFFTAG_TILEWIDTH, l_nBlockXSize ) ||
            !TIFFSetField( l_hTIFF, TIFFTAG_TILELENGTH, l_nBlockYSize ) )
        {
            XTIFFClose(l_hTIFF);
            CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
            return NULL;
        }
    }
    else
    {
        const uint32 l_nRowsPerStrip = std::min(nYSize,
            l_nBlockYSize == 0
            ? static_cast<int>(TIFFDefaultStripSize(l_hTIFF,0))
            : l_nBlockYSize );

        TIFFSetField( l_hTIFF, TIFFTAG_ROWSPERSTRIP, l_nRowsPerStrip );
    }

/* -------------------------------------------------------------------- */
/*      Set compression related tags.                                   */
/* -------------------------------------------------------------------- */
    if( l_nCompression == COMPRESSION_LZW ||
         l_nCompression == COMPRESSION_ADOBE_DEFLATE )
        TIFFSetField( l_hTIFF, TIFFTAG_PREDICTOR, nPredictor );
    if( l_nCompression == COMPRESSION_ADOBE_DEFLATE && l_nZLevel != -1 )
        TIFFSetField( l_hTIFF, TIFFTAG_ZIPQUALITY, l_nZLevel );
    else if( l_nCompression == COMPRESSION_JPEG && l_nJpegQuality != -1 )
        TIFFSetField( l_hTIFF, TIFFTAG_JPEGQUALITY, l_nJpegQuality );
    else if( l_nCompression == COMPRESSION_LZMA && l_nLZMAPreset != -1)
        TIFFSetField( l_hTIFF, TIFFTAG_LZMAPRESET, l_nLZMAPreset );

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

    // Would perhaps works with libtiff 3.X but didn't bother trying This trick
    // creates a temporary in-memory file and fetches its JPEG tables so that
    // we can directly set them, before tif_jpeg.c compute them at the first
    // strip/tile writing, which is too late, since we have already crystalized
    // the directory. This way we avoid a directory rewriting.
#if defined(BIGTIFF_SUPPORT)
    if( l_nCompression == COMPRESSION_JPEG &&
        !STARTS_WITH(pszFilename, szJPEGGTiffDatasetTmpPrefix) &&
        CPLTestBool(
            CSLFetchNameValueDef(papszParmList, "WRITE_JPEGTABLE_TAG", "YES")) )
    {
        GTiffWriteJPEGTables( l_hTIFF,
                              CSLFetchNameValue(papszParmList, "PHOTOMETRIC"),
                              CSLFetchNameValue(papszParmList, "JPEG_QUALITY"),
                              CSLFetchNameValue(papszParmList,
                                                "JPEGTABLESMODE") );
    }
#endif

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
    // Would perhaps works with libtiff 3.X but didn't bother trying This trick
    // creates a temporary in-memory file and fetches its JPEG tables so that
    // we can directly set them, before tif_jpeg.c compute them at the first
    // strip/tile writing, which is too late, since we have already crystalized
    // the directory. This way we avoid a directory rewriting.
#if defined(BIGTIFF_SUPPORT)
    uint16 nBands = 0;
    if( !TIFFGetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL,
                        &nBands ) )
        nBands = 1;

    uint16 l_nBitsPerSample = 0;
    if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE,
                        &(l_nBitsPerSample)) )
        l_nBitsPerSample = 1;

    CPLString osTmpFilenameIn;
    osTmpFilenameIn.Printf("%s%p", szJPEGGTiffDatasetTmpPrefix, hTIFF);
    VSILFILE* fpTmp = NULL;
    CPLString osTmp;
    char** papszLocalParameters = NULL;
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
        uint16 l_nPhotometric = 0;
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

        int nBlockSize = nInMemImageWidth * nInMemImageHeight *
                                        ((nBands <= 4) ? nBands : 1);
        if( l_nBitsPerSample == 12 )
            nBlockSize = (nBlockSize * 3) / 2;
        std::vector<GByte> abyZeroData( nBlockSize, 0 );
        TIFFWriteEncodedStrip( hTIFFTmp, 0, &abyZeroData[0], nBlockSize);

        uint32 nJPEGTableSize = 0;
        void* pJPEGTable = NULL;
        if( TIFFGetField( hTIFFTmp, TIFFTAG_JPEGTABLES, &nJPEGTableSize,
                            &pJPEGTable) )
            TIFFSetField( hTIFF, TIFFTAG_JPEGTABLES, nJPEGTableSize,
                            pJPEGTable);

        float *ref = NULL;
        if( TIFFGetField(hTIFFTmp, TIFFTAG_REFERENCEBLACKWHITE, &ref) )
            TIFFSetField(hTIFF, TIFFTAG_REFERENCEBLACKWHITE, ref);

        XTIFFClose(hTIFFTmp);
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTmp));
    }
    VSIUnlink(osTmpFilenameIn);
#endif
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
            return NULL;
        ++i;
        if( paby[i] == 0xD8 )
        {
            ++i;
            continue;
        }
        if( i + 2 >= nLen )
            return NULL;
        int nMarkerLen = paby[i+1] * 256 + paby[i+2];
        if( i+1+nMarkerLen >= nLen )
            return NULL;
        if( paby[i] == byMarker )
        {
            if( pnLenTable ) *pnLenTable = nMarkerLen;
            return paby + i + 1;
        }
        i += 1 + nMarkerLen;
    }
    return NULL;
}

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
            GTIFFFindNextTable(paby1, 0xDB, nLen1, &nLenTable1);
        const GByte* paby2New =
            GTIFFFindNextTable(paby2, 0xDB, nLen2, &nLenTable2);
        if( paby1New == NULL && paby2New == NULL )
            return bFound;
        if( paby1New == NULL && paby2New != NULL )
            return false;
        if( paby1New != NULL && paby2New == NULL )
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

int GTiffDataset::GuessJPEGQuality( bool& bOutHasQuantizationTable,
                                    bool& bOutHasHuffmanTable )
{
    CPLAssert( nCompression == COMPRESSION_JPEG );
    uint32 nJPEGTableSize = 0;
    void* pJPEGTable = NULL;
    if( !TIFFGetField(hTIFF, TIFFTAG_JPEGTABLES, &nJPEGTableSize, &pJPEGTable) )
    {
        bOutHasQuantizationTable = false;
        bOutHasHuffmanTable = false;
        return -1;
    }

    bOutHasQuantizationTable =
        GTIFFFindNextTable( (const GByte*)pJPEGTable, 0xDB,
                            nJPEGTableSize, NULL) != NULL;
    bOutHasHuffmanTable =
        GTIFFFindNextTable( (const GByte*)pJPEGTable, 0xC4,
                            nJPEGTableSize, NULL) != NULL;
    if( !bOutHasQuantizationTable )
        return -1;

    char** papszLocalParameters = NULL;
    papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                            "COMPRESS", "JPEG");
    if( nPhotometric == PHOTOMETRIC_YCBCR )
        papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                            "PHOTOMETRIC", "YCBCR");
    else if( nPhotometric == PHOTOMETRIC_SEPARATED )
        papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                            "PHOTOMETRIC", "CMYK");
    papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                            "BLOCKYSIZE", "16");
    if( nBitsPerSample == 12 )
        papszLocalParameters = CSLSetNameValue(papszLocalParameters,
                                                "NBITS", "12");

    CPLString osTmpFilenameIn;
    osTmpFilenameIn.Printf( "/vsimem/gtiffdataset_guess_jpeg_quality_tmp_%p",
                            this );

    int nRet = -1;
    for( int nQuality = 0; nQuality <= 100 && nRet < 0; ++nQuality )
    {
        VSILFILE* fpTmp = NULL;
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
        if( nPhotometric == PHOTOMETRIC_YCBCR
            && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                                "YES") ) )
        {
            TIFFSetField(hTIFFTmp, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }

        GByte abyZeroData[(16*16*4*3)/2] = {};
        const int nBlockSize =
            (16 * 16 * ((nBands <= 4) ? nBands : 1) * nBitsPerSample) / 8;
        TIFFWriteEncodedStrip( hTIFFTmp, 0, abyZeroData, nBlockSize);

        uint32 nJPEGTableSizeTry = 0;
        void* pJPEGTableTry = NULL;
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

void GTiffDataset::SetJPEGQualityAndTablesModeFromFile()
{
    bool bHasQuantizationTable = false;
    bool bHasHuffmanTable = false;
    int nQuality = GuessJPEGQuality( bHasQuantizationTable,
                                     bHasHuffmanTable );
    if( nQuality > 0 )
    {
        CPLDebug("GTiff", "Guessed JPEG quality to be %d", nQuality);
        nJpegQuality = nQuality;
        TIFFSetField( hTIFF, TIFFTAG_JPEGQUALITY, nQuality );

        // This means we will use the quantization tables from the
        // JpegTables tag.
        nJpegTablesMode = JPEGTABLESMODE_QUANT;
    }
    else
    {
        uint32 nJPEGTableSize = 0;
        void* pJPEGTable = NULL;
        if( !TIFFGetField( hTIFF, TIFFTAG_JPEGTABLES,
                            &nJPEGTableSize, &pJPEGTable) )
        {
            toff_t *panByteCounts = NULL;
            const int nBlockCount =
                nPlanarConfig == PLANARCONFIG_SEPARATE
                ? nBlocksPerBand * nBands
                : nBlocksPerBand;
            if( TIFFIsTiled( hTIFF ) )
                TIFFGetField( hTIFF, TIFFTAG_TILEBYTECOUNTS,
                                &panByteCounts );
            else
                TIFFGetField( hTIFF, TIFFTAG_STRIPBYTECOUNTS,
                                &panByteCounts );

            bool bFoundNonEmptyBlock = false;
            if( panByteCounts != NULL )
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
                nJpegTablesMode = 0;
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
            nJpegTablesMode = 0;
        }
    }
    if( bHasHuffmanTable )
    {
        // If there are Huffman tables in header use them, otherwise
        // if we use optimized tables, libtiff will currently reuse
        // the number of the Huffman tables of the header for the
        // optimized version of each strile, which is illegal.
        nJpegTablesMode |= JPEGTABLESMODE_HUFF;
    }
    if( nJpegTablesMode >= 0 )
        TIFFSetField( hTIFF, TIFFTAG_JPEGTABLESMODE,
                        nJpegTablesMode);
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new GeoTIFF or TIFF file.                              */
/************************************************************************/

GDALDataset *GTiffDataset::Create( const char * pszFilename,
                                   int nXSize, int nYSize, int l_nBands,
                                   GDALDataType eType,
                                   char **papszParmList )

{
    VSILFILE* l_fpL = NULL;
    CPLString l_osTmpFilename;

/* -------------------------------------------------------------------- */
/*      Create the underlying TIFF file.                                */
/* -------------------------------------------------------------------- */
    TIFF *l_hTIFF = CreateLL(
        pszFilename,
        nXSize, nYSize, l_nBands,
        eType, 0, papszParmList, &l_fpL, l_osTmpFilename );
    const bool bStreaming = !l_osTmpFilename.empty();

    if( l_hTIFF == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    poDS->hTIFF = l_hTIFF;
    poDS->fpL = l_fpL;
    if( bStreaming )
    {
        poDS->bStreamingOut = true;
        poDS->osTmpFilename = l_osTmpFilename;
        poDS->fpToWrite = VSIFOpenL( pszFilename, "wb" );
        if( poDS->fpToWrite == NULL )
        {
            VSIUnlink(l_osTmpFilename);
            delete poDS;
            return NULL;
        }
    }
    poDS->poActiveDS = poDS;
    poDS->ppoActiveDSRef = &(poDS->poActiveDS);

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->bCrystalized = false;
    poDS->nSamplesPerPixel = (uint16) l_nBands;
    poDS->osFilename = pszFilename;

    // Don't try to load external metadata files (#6597).
    poDS->bIMDRPCMetadataLoaded = true;

    // Avoid premature crystalization that will cause directory re-writing if
    // GetProjectionRef() or GetGeoTransform() are called on the newly created
    // GeoTIFF.
    poDS->bLookedForProjection = true;

    TIFFGetField( l_hTIFF, TIFFTAG_SAMPLEFORMAT, &(poDS->nSampleFormat) );
    TIFFGetField( l_hTIFF, TIFFTAG_PLANARCONFIG, &(poDS->nPlanarConfig) );
    // Weird that we need this, but otherwise we get a Valgrind warning on
    // tiff_write_124.
    if( !TIFFGetField( l_hTIFF, TIFFTAG_PHOTOMETRIC, &(poDS->nPhotometric) ) )
        poDS->nPhotometric = PHOTOMETRIC_MINISBLACK;
    TIFFGetField( l_hTIFF, TIFFTAG_BITSPERSAMPLE, &(poDS->nBitsPerSample) );
    TIFFGetField( l_hTIFF, TIFFTAG_COMPRESSION, &(poDS->nCompression) );

    if( TIFFIsTiled(l_hTIFF) )
    {
        TIFFGetField( l_hTIFF, TIFFTAG_TILEWIDTH, &(poDS->nBlockXSize) );
        TIFFGetField( l_hTIFF, TIFFTAG_TILELENGTH, &(poDS->nBlockYSize) );
    }
    else
    {
        if( !TIFFGetField( l_hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(poDS->nRowsPerStrip) ) )
            poDS->nRowsPerStrip = 1;  // Dummy value.

        poDS->nBlockXSize = nXSize;
        poDS->nBlockYSize =
            std::min( static_cast<int>(poDS->nRowsPerStrip) , nYSize );
    }

    poDS->nBlocksPerBand =
        DIV_ROUND_UP(nYSize, poDS->nBlockYSize)
        * DIV_ROUND_UP(nXSize, poDS->nBlockXSize);

    if( CSLFetchNameValue( papszParmList, "PROFILE" ) != NULL )
        poDS->osProfile = CSLFetchNameValue( papszParmList, "PROFILE" );

/* -------------------------------------------------------------------- */
/*      YCbCr JPEG compressed images should be translated on the fly    */
/*      to RGB by libtiff/libjpeg unless specifically requested         */
/*      otherwise.                                                      */
/* -------------------------------------------------------------------- */
    if( poDS->nCompression == COMPRESSION_JPEG
        && poDS->nPhotometric == PHOTOMETRIC_YCBCR
        && CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB", "YES") ) )
    {
        int nColorMode = 0;

        poDS->SetMetadataItem("SOURCE_COLOR_SPACE", "YCbCr", "IMAGE_STRUCTURE");
        if( !TIFFGetField( l_hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode ) ||
            nColorMode != JPEGCOLORMODE_RGB )
            TIFFSetField(l_hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
    }

/* -------------------------------------------------------------------- */
/*      Read palette back as a color table if it has one.               */
/* -------------------------------------------------------------------- */
    unsigned short *panRed = NULL;
    unsigned short *panGreen = NULL;
    unsigned short *panBlue = NULL;

    if( poDS->nPhotometric == PHOTOMETRIC_PALETTE
        && TIFFGetField( l_hTIFF, TIFFTAG_COLORMAP,
                         &panRed, &panGreen, &panBlue) )
    {

        poDS->poColorTable = new GDALColorTable();

        const int nColorCount = 1 << poDS->nBitsPerSample;

        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            const unsigned short divisor = 256;
            const GDALColorEntry oEntry = {
                static_cast<short>(panRed[iColor] / divisor),
                static_cast<short>(panGreen[iColor] / divisor),
                static_cast<short>(panBlue[iColor] / divisor),
                static_cast<short>(255)
            };

            poDS->poColorTable->SetColorEntry( iColor, &oEntry );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we want to ensure all blocks get written out on close to     */
/*      avoid sparse files?                                             */
/* -------------------------------------------------------------------- */
    if( !CPLFetchBool( papszParmList, "SPARSE_OK", false ) )
        poDS->bFillEmptyTilesAtClosing = true;

    poDS->bWriteEmptyTiles = bStreaming ||
        (poDS->nCompression != COMPRESSION_NONE &&
         poDS->bFillEmptyTilesAtClosing);
    // Only required for people writing non-compressed stripped files in the
    // right order and wanting all tstrips to be written in the same order
    // so that the end result can be memory mapped without knowledge of each
    // strip offset.
    if( CPLTestBool( CSLFetchNameValueDef( papszParmList,
                              "WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE" )) ||
         CPLTestBool( CSLFetchNameValueDef( papszParmList,
                              "@WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE" )) )
    {
        poDS->bWriteEmptyTiles = true;
    }

/* -------------------------------------------------------------------- */
/*      Preserve creation options for consulting later (for instance    */
/*      to decide if a TFW file should be written).                     */
/* -------------------------------------------------------------------- */
    poDS->papszCreationOptions = CSLDuplicate( papszParmList );

    poDS->nZLevel = GTiffGetZLevel(papszParmList);
    poDS->nLZMAPreset = GTiffGetLZMAPreset(papszParmList);
    poDS->nJpegQuality = GTiffGetJpegQuality(papszParmList);
    poDS->nJpegTablesMode = GTiffGetJpegTablesMode(papszParmList);
    poDS->InitCreationOrOpenOptions(papszParmList);

#if !defined(BIGTIFF_SUPPORT)
/* -------------------------------------------------------------------- */
/*      If we are writing jpeg compression we need to write some        */
/*      imagery to force the jpegtables to get created.  This is,       */
/*      likely only needed with libtiff >= 3.9.3 (#3633)                */
/* -------------------------------------------------------------------- */
    if( poDS->nCompression == COMPRESSION_JPEG
        && strstr(TIFFLIB_VERSION_STR, "Version 3.9") != NULL )
    {
        CPLDebug( "GDAL",
                  "Writing zero block to force creation of JPEG tables." );
        if( TIFFIsTiled( l_hTIFF ) )
        {
            const int cc = TIFFTileSize( l_hTIFF );
            unsigned char *pabyZeros =
                static_cast<unsigned char *>(CPLCalloc(cc, 1));
            TIFFWriteEncodedTile(l_hTIFF, 0, pabyZeros, cc);
            CPLFree( pabyZeros );
        }
        else
        {
            const int cc = TIFFStripSize( l_hTIFF );
            unsigned char *pabyZeros =
                static_cast<unsigned char *>(CPLCalloc(cc, 1));
            TIFFWriteEncodedStrip(l_hTIFF, 0, pabyZeros, cc);
            CPLFree( pabyZeros );
        }
        poDS->bDontReloadFirstBlock = true;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < l_nBands; ++iBand )
    {
        if( poDS->nBitsPerSample == 8 ||
            (poDS->nBitsPerSample == 16 && eType != GDT_Float32) ||
            poDS->nBitsPerSample == 32 ||
            poDS->nBitsPerSample == 64 ||
            poDS->nBitsPerSample == 128)
        {
            poDS->SetBand( iBand + 1, new GTiffRasterBand( poDS, iBand + 1 ) );
        }
        else
        {
            poDS->SetBand( iBand + 1, new GTiffOddBitsBand( poDS, iBand + 1 ) );
            poDS->GetRasterBand( iBand + 1 )->
                SetMetadataItem( "NBITS",
                                 CPLString().Printf("%d",poDS->nBitsPerSample),
                                 "IMAGE_STRUCTURE" );
        }
    }

    poDS->GetDiscardLsbOption(papszParmList);

    if( poDS->nPlanarConfig == PLANARCONFIG_CONTIG && l_nBands != 1 )
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    else
        poDS->SetMetadataItem( "INTERLEAVE", "BAND", "IMAGE_STRUCTURE" );

    poDS->oOvManager.Initialize( poDS, pszFilename );

    return poDS;
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
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to export GeoTIFF files with zero bands." );
        return NULL;
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
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Unable to export GeoTIFF file with different datatypes "
                    "per different bands. All bands should have the same "
                    "types in TIFF." );
                return NULL;
            }
            else
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Unable to export GeoTIFF file with different datatypes "
                    "per different bands. All bands should have the same "
                    "types in TIFF." );
            }
        }
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Capture the profile.                                            */
/* -------------------------------------------------------------------- */
    const char *pszProfile = CSLFetchNameValue(papszOptions, "PROFILE");
    if( pszProfile == NULL )
        pszProfile = "GDALGeoTIFF";

    if( !EQUAL(pszProfile, "BASELINE")
        && !EQUAL(pszProfile, "GeoTIFF")
        && !EQUAL(pszProfile, "GDALGeoTIFF") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PROFILE=%s not supported in GTIFF driver.",
                  pszProfile );
        return NULL;
    }

    const bool bGeoTIFF = !EQUAL(pszProfile, "BASELINE");

/* -------------------------------------------------------------------- */
/*      Special handling for NBITS.  Copy from band metadata if found.  */
/* -------------------------------------------------------------------- */
    char **papszCreateOptions = CSLDuplicate( papszOptions );

    if( poPBand->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" ) != NULL
        && atoi(poPBand->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" )) > 0
        && CSLFetchNameValue( papszCreateOptions, "NBITS") == NULL )
    {
        papszCreateOptions =
            CSLSetNameValue( papszCreateOptions, "NBITS",
                             poPBand->GetMetadataItem( "NBITS",
                                                       "IMAGE_STRUCTURE" ) );
    }

    if( CSLFetchNameValue( papszOptions, "PIXELTYPE" ) == NULL
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
            NULL
        };

        // Copy all the tags.  Options will override tags in the source.
        int i = 0;
        while(pszOptionsMD[i] != NULL)
        {
            char const *pszMD =
                CSLFetchNameValue(papszOptions, pszOptionsMD[i]);
            if( pszMD == NULL )
                pszMD = poSrcDS->GetMetadataItem( pszOptionsMD[i],
                                                  "COLOR_PROFILE" );

            if( (pszMD != NULL) && !EQUAL(pszMD, "") )
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
    if( CPLFetchBool(papszOptions, "COPY_SRC_OVERVIEWS", false) )
    {
        const int nSrcOverviews = poSrcDS->GetRasterBand(1)->GetOverviewCount();
        if( nSrcOverviews )
        {
            for( int j = 1; j <= l_nBands; ++j )
            {
                if( poSrcDS->GetRasterBand(j)->GetOverviewCount() !=
                                                        nSrcOverviews )
                {
                    CPLError(
                        CE_Failure, CPLE_NotSupported,
                        "COPY_SRC_OVERVIEWS cannot be used when the bands have "
                        "not the same number of overview levels." );
                    CSLDestroy(papszCreateOptions);
                    return NULL;
                }
                for( int i = 0; i < nSrcOverviews; ++i )
                {
                    GDALRasterBand* poOvrBand =
                        poSrcDS->GetRasterBand(j)->GetOverview(i);
                    if( poOvrBand == NULL )
                    {
                        CPLError(
                            CE_Failure, CPLE_NotSupported,
                            "COPY_SRC_OVERVIEWS cannot be used when one "
                            "overview band is NULL." );
                        CSLDestroy(papszCreateOptions);
                        return NULL;
                    }
                    GDALRasterBand* poOvrFirstBand =
                        poSrcDS->GetRasterBand(1)->GetOverview(i);
                    if( poOvrBand->GetXSize() != poOvrFirstBand->GetXSize() ||
                        poOvrBand->GetYSize() != poOvrFirstBand->GetYSize() )
                    {
                        CPLError(
                            CE_Failure, CPLE_NotSupported,
                            "COPY_SRC_OVERVIEWS cannot be used when the "
                            "overview bands have not the same dimensions "
                            "among bands." );
                        CSLDestroy(papszCreateOptions);
                        return NULL;
                    }
                }
            }

            for( int i = 0; i < nSrcOverviews; ++i )
            {
                dfExtraSpaceForOverviews +=
                    static_cast<double>(
                      poSrcDS->GetRasterBand(1)->GetOverview(i)->GetXSize() ) *
                      poSrcDS->GetRasterBand(1)->GetOverview(i)->GetYSize();
            }
            dfExtraSpaceForOverviews *=
                                l_nBands * GDALGetDataTypeSizeBytes(eType);
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
        CSLFetchNameValue(papszOptions, "PHOTOMETRIC") != NULL;

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
    VSILFILE* l_fpL = NULL;
    CPLString l_osTmpFilename;

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    TIFF *l_hTIFF =
        CreateLL( pszFilename, nXSize, nYSize, l_nBands,
                  eType, dfExtraSpaceForOverviews, papszCreateOptions, &l_fpL,
                  l_osTmpFilename );
    const bool bStreaming = !l_osTmpFilename.empty();

    CSLDestroy( papszCreateOptions );
    papszCreateOptions = NULL;

    if( l_hTIFF == NULL )
    {
        if( bStreaming ) VSIUnlink(l_osTmpFilename);
        return NULL;
    }

    uint16 l_nPlanarConfig = 0;
    TIFFGetField( l_hTIFF, TIFFTAG_PLANARCONFIG, &l_nPlanarConfig );

    uint16 l_nBitsPerSample = 0;
    TIFFGetField(l_hTIFF, TIFFTAG_BITSPERSAMPLE, &l_nBitsPerSample );

    uint16 l_nCompression = 0;

    if( !TIFFGetField( l_hTIFF, TIFFTAG_COMPRESSION, &(l_nCompression) ) )
        l_nCompression = COMPRESSION_NONE;

/* -------------------------------------------------------------------- */
/*      Set the alpha channel if it is the last one.                    */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetRasterBand(l_nBands)->GetColorInterpretation() ==
        GCI_AlphaBand )
    {
        uint16 *v = NULL;
        uint16 count = 0;
        if( TIFFGetField( l_hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            const int nBaseSamples = l_nBands - count;
            if( l_nBands > nBaseSamples && l_nBands - nBaseSamples - 1 < count )
            {
                // We need to allocate a new array as (current) libtiff
                // versions will not like that we reuse the array we got from
                // TIFFGetField().

                uint16* pasNewExtraSamples =
                    static_cast<uint16 *>(
                        CPLMalloc( count * sizeof(uint16) ) );
                memcpy( pasNewExtraSamples, v, count * sizeof(uint16) );
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
        poSrcDS->GetRasterBand(1)->GetColorTable() != NULL
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
             && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL
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

                panTRed[iColor] = static_cast<unsigned short>(256 * sRGB.c1);
                panTGreen[iColor] = static_cast<unsigned short>(256 * sRGB.c2);
                panTBlue[iColor] = static_cast<unsigned short>(256 * sRGB.c3);
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
    else if( poSrcDS->GetRasterBand(1)->GetColorTable() != NULL )
        CPLError(
            CE_Warning, CPLE_AppDefined,
            "Unable to export color table to GeoTIFF file.  Color tables "
            "can only be written to 1 band or 2 bands Byte or "
            "UInt16 GeoTIFF files." );

    if( l_nBands == 2
        && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL
        && (eType == GDT_Byte || eType == GDT_UInt16) )
    {
        uint16 v[1] = { EXTRASAMPLE_UNASSALPHA };

        TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, 1, v );
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
            GTiffDataset::WriteMetadata( poSrcDS, l_hTIFF, false, pszProfile,
                                         pszFilename, papszOptions );

/* -------------------------------------------------------------------- */
/*      Write NoData value, if exist.                                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszProfile,"GDALGeoTIFF") )
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
    const char *l_pszProjection = NULL;
    double l_adfGeoTransform[6] = { 0.0 };

    if( poSrcDS->GetGeoTransform( l_adfGeoTransform ) == CE_None
        && (l_adfGeoTransform[0] != 0.0 || l_adfGeoTransform[1] != 1.0
            || l_adfGeoTransform[2] != 0.0 || l_adfGeoTransform[3] != 0.0
            || l_adfGeoTransform[4] != 0.0 || l_adfGeoTransform[5] != 1.0 ))
    {
        if( bGeoTIFF )
        {
            if( l_adfGeoTransform[2] == 0.0 && l_adfGeoTransform[4] == 0.0
                && l_adfGeoTransform[5] < 0.0 )
            {
                {
                    const double adfPixelScale[3] = {
                        l_adfGeoTransform[1], fabs(l_adfGeoTransform[5]), 0.0 };

                    TIFFSetField( l_hTIFF, TIFFTAG_GEOPIXELSCALE, 3,
                                  adfPixelScale );
                }

                double adfTiePoints[6] = {
                    0.0,
                    0.0,
                    0.0,
                    l_adfGeoTransform[0],
                    l_adfGeoTransform[3],
                    0.0
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

            l_pszProjection = poSrcDS->GetProjectionRef();
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
                padfTiePoints[iGCP*6+0] += 0.5;
                padfTiePoints[iGCP*6+1] += 0.5;
            }
        }

        TIFFSetField( l_hTIFF, TIFFTAG_GEOTIEPOINTS,
                      6*poSrcDS->GetGCPCount(), padfTiePoints );
        CPLFree( padfTiePoints );

        l_pszProjection = poSrcDS->GetGCPProjection();

        if( CPLFetchBool( papszOptions, "TFW", false )
            || CPLFetchBool( papszOptions, "WORLDFILE", false ) )
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "TFW=ON or WORLDFILE=ON creation options are ignored when "
                "GCPs are available" );
        }
    }
    else
    {
        l_pszProjection = poSrcDS->GetProjectionRef();
    }

/* -------------------------------------------------------------------- */
/*      Write the projection information, if possible.                  */
/* -------------------------------------------------------------------- */
    const bool bHasProjection =
        l_pszProjection != NULL && strlen(l_pszProjection) > 0;
    if( (bHasProjection || bPixelIsPoint) && bGeoTIFF )
    {
        GTIF *psGTIF = GTiffDatasetGTIFNew( l_hTIFF );

        if( bHasProjection )
        {
            GTIFSetFromOGISDefnEx( psGTIF, l_pszProjection,
                                   GetGTIFFKeysFlavor(papszOptions) );
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

#if !defined(BIGTIFF_SUPPORT)
    /* -------------------------------------------------------------------- */
    /*      If we are writing jpeg compression we need to write some        */
    /*      imagery to force the jpegtables to get created.  This is,       */
    /*      likely only needed with libtiff >= 3.9.3 (#3633)                */
    /* -------------------------------------------------------------------- */
    else if( l_nCompression == COMPRESSION_JPEG
            && strstr(TIFFLIB_VERSION_STR, "Version 3.9") != NULL )
    {
        CPLDebug( "GDAL",
                  "Writing zero block to force creation of JPEG tables." );
        if( TIFFIsTiled( l_hTIFF ) )
        {
            const int cc = TIFFTileSize( l_hTIFF );
            unsigned char *pabyZeros =
                static_cast<unsigned char *>( CPLCalloc(cc, 1) );
            TIFFWriteEncodedTile( l_hTIFF, 0, pabyZeros, cc );
            CPLFree( pabyZeros );
        }
        else
        {
            int cc = TIFFStripSize( l_hTIFF );
            unsigned char *pabyZeros =
                static_cast<unsigned char *>( CPLCalloc(cc,1) );
            TIFFWriteEncodedStrip( l_hTIFF, 0, pabyZeros, cc );
            CPLFree( pabyZeros );
        }
        l_bDontReloadFirstBlock = true;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */

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
            CPLError(CE_Failure, CPLE_FileIO, "Cannot seek");
        const int nSize = static_cast<int>( VSIFTellL(l_fpL) );

        vsi_l_offset nDataLength = 0;
        VSIGetMemFileBuffer( l_osTmpFilename, &nDataLength, FALSE);
        TIFFSetDirectory( l_hTIFF, 0 );
        GTiffFillStreamableOffsetAndCount( l_hTIFF, nSize );
        TIFFWriteDirectory( l_hTIFF );
    }
    TIFFFlush( l_hTIFF );
    XTIFFClose( l_hTIFF );
    l_hTIFF = NULL;

    {
        const CPLErr eErr = VSIFCloseL(l_fpL) == 0 ? CE_None : CE_Failure;
        l_fpL = NULL;

        if( eErr != CE_None )
        {
            VSIUnlink( bStreaming ? l_osTmpFilename.c_str() : pszFilename );
            return NULL;
        }
    }

    // fpStreaming will assigned to the instance and not closed here.
    VSILFILE *fpStreaming = NULL;
    if( bStreaming )
    {
        vsi_l_offset nDataLength = 0;
        void* pabyBuffer =
            VSIGetMemFileBuffer( l_osTmpFilename, &nDataLength, FALSE);
        fpStreaming = VSIFOpenL( pszFilename, "wb" );
        if( fpStreaming == NULL )
        {
            VSIUnlink(l_osTmpFilename);
            return NULL;
        }
        if( static_cast<vsi_l_offset>(
                VSIFWriteL( pabyBuffer, 1, static_cast<int>(nDataLength),
                            fpStreaming ) ) != nDataLength )
        {
            CPLError( CE_Failure, CPLE_FileIO, "Could not write %d bytes",
                      static_cast<int>(nDataLength) );
            CPL_IGNORE_RET_VAL(VSIFCloseL( fpStreaming ));
            VSIUnlink(l_osTmpFilename);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Re-open as a dataset and copy over missing metadata using       */
/*      PAM facilities.                                                 */
/* -------------------------------------------------------------------- */
    CPLString osFileName("GTIFF_RAW:");

    osFileName += bStreaming ? l_osTmpFilename.c_str() : pszFilename;

    GDALOpenInfo oOpenInfo( osFileName, GA_Update );
    if( bStreaming )
    {
        // In case of single strip file, there's a libtiff check that would
        // issue a warning since the file hasn't the required size.
        CPLPushErrorHandler(CPLQuietErrorHandler);
    }
    GTiffDataset *poDS = static_cast<GTiffDataset *>( Open(&oOpenInfo) );
    if( bStreaming )
        CPLPopErrorHandler();
    if( poDS == NULL )
    {
        oOpenInfo.eAccess = GA_ReadOnly;
        poDS = static_cast<GTiffDataset *>( Open(&oOpenInfo) );
    }

    if( poDS == NULL )
    {
        VSIUnlink( bStreaming ? l_osTmpFilename.c_str() : pszFilename );
        return NULL;
    }

    if( bStreaming )
    {
        VSIUnlink(l_osTmpFilename);
        poDS->fpToWrite = fpStreaming;
    }
    poDS->osProfile = pszProfile;

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

    poDS->CloneInfo( poSrcDS, nCloneInfoFlags );
    poDS->papszCreationOptions = CSLDuplicate( papszOptions );
    poDS->bDontReloadFirstBlock = l_bDontReloadFirstBlock;

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
        if( NULL != papszCatNames )
            poDstBand->SetCategoryNames( papszCatNames );
    }

    l_hTIFF = static_cast<TIFF *>( poDS->GetInternalHandle(NULL) );

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
        GTiffDataset::WriteMetadata(
            poDS, l_hTIFF, true, pszProfile,
            pszFilename, papszOptions,
            true /* don't write RPC and IMD file again */ );

    if( !bStreaming )
        GTiffDataset::WriteRPC(
            poDS, l_hTIFF, true, pszProfile,
            pszFilename, papszOptions,
            true /* write only in PAM AND if needed */ );

    // To avoid unnecessary directory rewriting.
    poDS->bMetadataChanged = false;
    poDS->bGeoTIFFInfoChanged = false;
    poDS->bNoDataChanged = false;
    poDS->bForceUnsetGTOrGCPs = false;
    poDS->bForceUnsetProjection = false;
    poDS->bStreamingOut = bStreaming;

    // Don't try to load external metadata files (#6597).
    poDS->bIMDRPCMetadataLoaded = true;

    // We must re-set the compression level at this point, since it has been
    // lost a few lines above when closing the newly create TIFF file The
    // TIFFTAG_ZIPQUALITY & TIFFTAG_JPEGQUALITY are not store in the TIFF file.
    // They are just TIFF session parameters.

    poDS->nZLevel = GTiffGetZLevel(papszOptions);
    poDS->nLZMAPreset = GTiffGetLZMAPreset(papszOptions);
    poDS->nJpegQuality = GTiffGetJpegQuality(papszOptions);
    poDS->nJpegTablesMode = GTiffGetJpegTablesMode(papszOptions);
    poDS->GetDiscardLsbOption(papszOptions);
    poDS->InitCreationOrOpenOptions(papszOptions);

    if( l_nCompression == COMPRESSION_ADOBE_DEFLATE )
    {
        if( poDS->nZLevel != -1 )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_ZIPQUALITY, poDS->nZLevel );
        }
    }
    else if( l_nCompression == COMPRESSION_JPEG )
    {
        if( poDS->nJpegQuality != -1 )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_JPEGQUALITY, poDS->nJpegQuality );
        }
        TIFFSetField( l_hTIFF, TIFFTAG_JPEGTABLESMODE, poDS->nJpegTablesMode );
    }
    else if( l_nCompression == COMPRESSION_LZMA )
    {
        if( poDS->nLZMAPreset != -1 )
        {
            TIFFSetField( l_hTIFF, TIFFTAG_LZMAPRESET, poDS->nLZMAPreset );
        }
    }

    // Precreate (internal) mask, so that the IBuildOverviews() below
    // has a chance to create also the overviews of the mask.
    CPLErr eErr = CE_None;

    const int nMaskFlags = poSrcDS->GetRasterBand(1)->GetMaskFlags();
    if( !(nMaskFlags & (GMF_ALL_VALID|GMF_ALPHA|GMF_NODATA) )
        && (nMaskFlags & GMF_PER_DATASET) )
    {
        eErr = poDS->CreateMaskBand( nMaskFlags );
    }

/* -------------------------------------------------------------------- */
/*      Create and then copy existing overviews if requested            */
/*  We do it such that all the IFDs are at the beginning of the file,   */
/*  and that the imagery data for the smallest overview is written      */
/*  first, that way the file is more usable when embedded in a          */
/*  compressed stream.                                                  */
/* -------------------------------------------------------------------- */

    // For scaled progress due to overview copying.
    double dfTotalPixels = static_cast<double>(nXSize) * nYSize;
    double dfCurPixels = 0;

    if( eErr == CE_None &&
        CPLFetchBool(papszOptions, "COPY_SRC_OVERVIEWS", false) )
    {
        const int nSrcOverviews = poSrcDS->GetRasterBand(1)->GetOverviewCount();
        if( nSrcOverviews )
        {
            eErr = poDS->CreateOverviewsFromSrcOverviews(poSrcDS);

            if( poDS->nOverviewCount != nSrcOverviews )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Did only manage to instantiate %d overview levels, "
                        "whereas source contains %d",
                        poDS->nOverviewCount, nSrcOverviews);
                eErr = CE_Failure;
            }

            for( int i = 0; i < nSrcOverviews; ++i )
            {
                GDALRasterBand* poOvrBand =
                    poSrcDS->GetRasterBand(1)->GetOverview(i);
                dfTotalPixels += static_cast<double>(poOvrBand->GetXSize()) *
                                poOvrBand->GetYSize();
            }

            char* papszCopyWholeRasterOptions[2] = { NULL, NULL };
            if( l_nCompression != COMPRESSION_NONE )
                papszCopyWholeRasterOptions[0] =
                    const_cast<char*>( "COMPRESSED=YES" );
            // Now copy the imagery.
            for( int i = 0; eErr == CE_None && i < nSrcOverviews; ++i )
            {
                // Begin with the smallest overview.
                const int iOvrLevel = nSrcOverviews - 1 - i;

                // Create a fake dataset with the source overview level so that
                // GDALDatasetCopyWholeRaster can cope with it.
                GDALDataset* poSrcOvrDS =
                    GDALCreateOverviewDataset(poSrcDS, iOvrLevel, TRUE);

                GDALRasterBand* poOvrBand =
                        poSrcDS->GetRasterBand(1)->GetOverview(iOvrLevel);
                double dfNextCurPixels =
                    dfCurPixels +
                    static_cast<double>(poOvrBand->GetXSize()) *
                    poOvrBand->GetYSize();

                void* pScaledData =
                    GDALCreateScaledProgress( dfCurPixels / dfTotalPixels,
                                            dfNextCurPixels / dfTotalPixels,
                                            pfnProgress, pProgressData );

                eErr =
                    GDALDatasetCopyWholeRaster(
                        (GDALDatasetH) poSrcOvrDS,
                        (GDALDatasetH) poDS->papoOverviewDS[iOvrLevel],
                        papszCopyWholeRasterOptions,
                        GDALScaledProgress, pScaledData );

                dfCurPixels = dfNextCurPixels;
                GDALDestroyScaledProgress(pScaledData);

                delete poSrcOvrDS;
                poSrcOvrDS = NULL;
                poDS->papoOverviewDS[iOvrLevel]->FlushCache();

                // Copy mask of the overview.
                if( eErr == CE_None && poDS->poMaskDS != NULL )
                {
                    eErr =
                        GDALRasterBandCopyWholeRaster(
                            poOvrBand->GetMaskBand(),
                            poDS->papoOverviewDS[iOvrLevel]->
                            poMaskDS->GetRasterBand(1),
                            papszCopyWholeRasterOptions,
                            GDALDummyProgress, NULL);
                    poDS->papoOverviewDS[iOvrLevel]->poMaskDS->FlushCache();
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy actual imagery.                                            */
/* -------------------------------------------------------------------- */
    void* pScaledData = GDALCreateScaledProgress(dfCurPixels / dfTotalPixels,
                                                 1.0,
                                                 pfnProgress, pProgressData);

    int bTryCopy = TRUE;  // TODO(schwehr): Make this a bool.

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

    if( bTryCopy && (poDS->bTreatAsSplit || poDS->bTreatAsSplitBitmap) )
    {
        // For split bands, we use TIFFWriteScanline() interface.
        CPLAssert(poDS->nBitsPerSample == 8 || poDS->nBitsPerSample == 1);

        if( poDS->nPlanarConfig == PLANARCONFIG_CONTIG && poDS->nBands > 1 )
        {
            GByte* pabyScanline =
                static_cast<GByte *>(
                    VSI_MALLOC_VERBOSE(TIFFScanlineSize(l_hTIFF)) );
            if( pabyScanline == NULL )
                eErr = CE_Failure;
            for( int j = 0; j < nYSize && eErr == CE_None; ++j )
            {
                eErr =
                    poSrcDS->RasterIO(
                        GF_Read, 0, j, nXSize, 1,
                        pabyScanline, nXSize, 1,
                        GDT_Byte, l_nBands, NULL, poDS->nBands, 0, 1,
                        NULL );
                if( eErr == CE_None &&
                    TIFFWriteScanline( l_hTIFF, pabyScanline, j, 0) == -1 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "TIFFWriteScanline() failed." );
                    eErr = CE_Failure;
                }
                if( !GDALScaledProgress( (j + 1) * 1.0 / nYSize,
                                         NULL, pScaledData ) )
                    eErr = CE_Failure;
            }
            CPLFree( pabyScanline );
        }
        else
        {
            GByte* pabyScanline = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(nXSize) );
            if( pabyScanline == NULL )
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
                        GDT_Byte, 0, 0, NULL );
                    if( poDS->bTreatAsSplitBitmap )
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
                            static_cast<uint16>(iBand - 1)) == -1 )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "TIFFWriteScanline() failed." );
                        eErr = CE_Failure;
                    }
                    if( !GDALScaledProgress(
                           (j + 1 + (iBand - 1) * nYSize) * 1.0 /
                           (l_nBands * nYSize),
                           NULL, pScaledData ) )
                        eErr = CE_Failure;
                }
            }
            CPLFree(pabyScanline);
        }

        // Necessary to be able to read the file without re-opening.
#if defined(HAVE_TIFFGETSIZEPROC)
        TIFFSizeProc pfnSizeProc = TIFFGetSizeProc( l_hTIFF );

        TIFFFlushData( l_hTIFF );

        toff_t nNewDirOffset = pfnSizeProc( TIFFClientdata( l_hTIFF ) );
        if( (nNewDirOffset % 2) == 1 )
            ++nNewDirOffset;
#endif

        TIFFFlush( l_hTIFF );

#if defined(HAVE_TIFFGETSIZEPROC)
        if( poDS->nDirOffset != TIFFCurrentDirOffset( l_hTIFF ) )
        {
            poDS->nDirOffset = nNewDirOffset;
            CPLDebug( "GTiff", "directory moved during flush." );
        }
#endif
    }
    else if( bTryCopy && eErr == CE_None )
    {
        char* papszCopyWholeRasterOptions[3] = { NULL, NULL, NULL };
        int iNextOption = 0;
        papszCopyWholeRasterOptions[iNextOption++] =
                const_cast<char *>( "SKIP_HOLES=YES" );
        if( l_nCompression != COMPRESSION_NONE )
        {
            papszCopyWholeRasterOptions[iNextOption++] =
                const_cast<char *>( "COMPRESSED=YES" );
        }
        // For streaming with separate, we really want that bands are written
        // after each other, even if the source is pixel interleaved.
        else if( bStreaming && poDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        {
            papszCopyWholeRasterOptions[iNextOption++] =
                const_cast<char *>("INTERLEAVE=BAND");
        }

    /* -------------------------------------------------------------------- */
    /*      Do we want to ensure all blocks get written out on close to     */
    /*      avoid sparse files?                                             */
    /* -------------------------------------------------------------------- */
        if( !CPLFetchBool( papszOptions, "SPARSE_OK", false ) )
            poDS->bFillEmptyTilesAtClosing = true;

        poDS->bWriteEmptyTiles =
            bStreaming ||
            (poDS->nCompression != COMPRESSION_NONE &&
             poDS->bFillEmptyTilesAtClosing);
        // Only required for people writing non-compressed stripped files in the
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
            poDS->bWriteEmptyTiles = true;
        }

        eErr = GDALDatasetCopyWholeRaster(
            /* (GDALDatasetH) */ poSrcDS,
            /* (GDALDatasetH) */ poDS,
            papszCopyWholeRasterOptions,
            GDALScaledProgress, pScaledData );
    }

    GDALDestroyScaledProgress(pScaledData);

    if( eErr == CE_None && !bStreaming )
    {
        if( poDS->poMaskDS )
        {
            const char* l_papszOptions[2] = { "COMPRESSED=YES", NULL };
            eErr = GDALRasterBandCopyWholeRaster(
                                    poSrcDS->GetRasterBand(1)->GetMaskBand(),
                                    poDS->GetRasterBand(1)->GetMaskBand(),
                                    const_cast<char **>(l_papszOptions),
                                    GDALDummyProgress, NULL);
        }
        else
        {
            eErr = GDALDriver::DefaultCopyMasks( poSrcDS, poDS, bStrict );
        }
    }

    if( eErr == CE_Failure )
    {
        delete poDS;
        poDS = NULL;

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
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GTiffDataset::GetProjectionRef()

{
    if( nGCPCount == 0 )
    {
        LoadGeoreferencingAndPamIfNeeded();
        LookForProjection();

        return pszProjection;
    }

    return "";
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr GTiffDataset::SetProjection( const char * pszNewProjection )

{
    if( bStreamingOut && bCrystalized )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify projection at that point in "
            "a streamed output file" );
        return CE_Failure;
    }

    LoadGeoreferencingAndPamIfNeeded();
    LookForProjection();

    if( !STARTS_WITH_CI(pszNewProjection, "GEOGCS")
        && !STARTS_WITH_CI(pszNewProjection, "PROJCS")
        && !STARTS_WITH_CI(pszNewProjection, "LOCAL_CS")
        && !STARTS_WITH_CI(pszNewProjection, "COMPD_CS")
        && !STARTS_WITH_CI(pszNewProjection, "GEOCCS")
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Only OGC WKT Projections supported for writing to GeoTIFF.  "
                "%s not supported.",
                  pszNewProjection );

        return CE_Failure;
    }

    if( EQUAL(pszNewProjection, "") &&
        pszProjection != NULL &&
        !EQUAL(pszProjection, "") )
    {
        bForceUnsetProjection = true;
    }

    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    bGeoTIFFInfoChanged = true;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::GetGeoTransform( double * padfTransform )

{
    LoadGeoreferencingAndPamIfNeeded();

    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    if( !bGeoTransformValid )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::SetGeoTransform( double * padfTransform )

{
    if( bStreamingOut && bCrystalized )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify geotransform at that point in a "
            "streamed output file" );
        return CE_Failure;
    }

    LoadGeoreferencingAndPamIfNeeded();

    if( GetAccess() == GA_Update )
    {
        if( nGCPCount > 0 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "GCPs previously set are going to be cleared "
                     "due to the setting of a geotransform.");
            bForceUnsetGTOrGCPs = true;
            GDALDeinitGCPs( nGCPCount, pasGCPList );
            CPLFree( pasGCPList );
            nGCPCount = 0;
            pasGCPList = NULL;
        }
        else if( padfTransform[0] == 0.0 &&
            padfTransform[1] == 1.0 &&
            padfTransform[2] == 0.0 &&
            padfTransform[3] == 0.0 &&
            padfTransform[4] == 0.0 &&
            padfTransform[5] == 1.0 &&
          !(adfGeoTransform[0] == 0.0 &&
            adfGeoTransform[1] == 1.0 &&
            adfGeoTransform[2] == 0.0 &&
            adfGeoTransform[3] == 0.0 &&
            adfGeoTransform[4] == 0.0 &&
            adfGeoTransform[5] == 1.0) )
        {
            bForceUnsetGTOrGCPs = true;
        }

        memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
        bGeoTransformValid = true;
        bGeoTIFFInfoChanged = true;

        return CE_None;
    }
    else
    {
        CPLError(
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

    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *GTiffDataset::GetGCPProjection()

{
    LoadGeoreferencingAndPamIfNeeded();

    if( nGCPCount > 0 )
    {
        LookForProjection();
    }
    if( pszProjection != NULL )
        return pszProjection;

    return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GTiffDataset::GetGCPs()

{
    LoadGeoreferencingAndPamIfNeeded();

    return pasGCPList;
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr GTiffDataset::SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                              const char *pszGCPProjection )
{
    LoadGeoreferencingAndPamIfNeeded();

    if( GetAccess() == GA_Update )
    {
        LookForProjection();

        if( nGCPCount > 0 && nGCPCountIn == 0 )
        {
            bForceUnsetGTOrGCPs = true;
        }
        else if( nGCPCountIn > 0 &&
                 !(adfGeoTransform[0] == 0.0 &&
                   adfGeoTransform[1] == 1.0 &&
                   adfGeoTransform[2] == 0.0 &&
                   adfGeoTransform[3] == 0.0 &&
                   adfGeoTransform[4] == 0.0 &&
                   adfGeoTransform[5] == 1.0) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "A geotransform previously set is going to be cleared "
                     "due to the setting of GCPs.");
            adfGeoTransform[0] = 0.0;
            adfGeoTransform[1] = 1.0;
            adfGeoTransform[2] = 0.0;
            adfGeoTransform[3] = 0.0;
            adfGeoTransform[4] = 0.0;
            adfGeoTransform[5] = 1.0;
            bGeoTransformValid = false;
            bForceUnsetGTOrGCPs = true;
        }

        if( !EQUAL(pszProjection, "") &&
                   (pszGCPProjection == NULL ||
                   pszGCPProjection[0] == '\0') )
            bForceUnsetProjection = true;

        if( nGCPCount > 0 )
        {
            GDALDeinitGCPs( nGCPCount, pasGCPList );
            CPLFree( pasGCPList );
        }

        nGCPCount = nGCPCountIn;
        pasGCPList = GDALDuplicateGCPs(nGCPCount, pasGCPListIn);

        CPLFree( pszProjection );
        pszProjection = CPLStrdup( pszGCPProjection );
        bGeoTIFFInfoChanged = true;

        return CE_None;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
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

    char **papszDomainList = CSLDuplicate(oGTiffMDMD.GetDomainList());
    char **papszBaseList = GDALDataset::GetMetadataDomainList();

    const int nbBaseDomains = CSLCount(papszBaseList);

    for( int domainId = 0; domainId < nbBaseDomains; ++domainId )
        papszDomainList = CSLAddString(papszDomainList,papszBaseList[domainId]);

    CSLDestroy(papszBaseList);

    return BuildMetadataDomainList(
        papszDomainList,
        TRUE,
        "", "ProxyOverviewRequest", MD_DOMAIN_RPC, MD_DOMAIN_IMD,
        "SUBDATASETS", "EXIF",
        "xml:XMP", "COLOR_PROFILE", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GTiffDataset::GetMetadata( const char * pszDomain )

{
    if( pszDomain == NULL || !EQUAL(pszDomain, "IMAGE_STRUCTURE") )
    {
        LoadGeoreferencingAndPamIfNeeded();
    }

    if( pszDomain != NULL && EQUAL(pszDomain, "ProxyOverviewRequest") )
        return GDALPamDataset::GetMetadata( pszDomain );

    if( pszDomain != NULL && EQUAL(pszDomain, "DERIVED_SUBDATASETS"))
    {
        return GDALDataset::GetMetadata(pszDomain);
    }

    else if( pszDomain != NULL && (EQUAL(pszDomain, MD_DOMAIN_RPC) ||
                                   EQUAL(pszDomain, MD_DOMAIN_IMD) ||
                                   EQUAL(pszDomain, MD_DOMAIN_IMAGERY)) )
        LoadMetadata();

    else if( pszDomain != NULL && EQUAL(pszDomain, "SUBDATASETS") )
        ScanDirectories();

    else if( pszDomain != NULL && EQUAL(pszDomain, "EXIF") )
        LoadEXIFMetadata();

    else if( pszDomain != NULL && EQUAL(pszDomain, "COLOR_PROFILE") )
        LoadICCProfile();

    else if( pszDomain == NULL || EQUAL(pszDomain, "") )
        LoadMDAreaOrPoint();  // To set GDALMD_AREA_OR_POINT.

    return oGTiffMDMD.GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/
CPLErr GTiffDataset::SetMetadata( char ** papszMD, const char *pszDomain )

{
    LoadGeoreferencingAndPamIfNeeded();

    if( bStreamingOut && bCrystalized )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify metadata at that point in a streamed output file" );
        return CE_Failure;
    }

    if( (papszMD != NULL) &&
        (pszDomain != NULL) &&
        EQUAL(pszDomain, "COLOR_PROFILE") )
    {
        bColorProfileMetadataChanged = true;
    }
    else if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
    {
        bMetadataChanged = true;
        // Cancel any existing metadata from PAM file.
        if( eAccess == GA_Update &&
            GDALPamDataset::GetMetadata(pszDomain) != NULL )
            GDALPamDataset::SetMetadata(NULL, pszDomain);
    }

    if( (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        CSLFetchNameValue(papszMD, GDALMD_AREA_OR_POINT) != NULL )
    {
        const char* pszPrevValue =
                GetMetadataItem(GDALMD_AREA_OR_POINT);
        const char* pszNewValue =
                CSLFetchNameValue(papszMD, GDALMD_AREA_OR_POINT);
        if( pszPrevValue == NULL || pszNewValue == NULL ||
            !EQUAL(pszPrevValue, pszNewValue) )
        {
            LookForProjection();
            bGeoTIFFInfoChanged = true;
        }
    }

    return oGTiffMDMD.SetMetadata( papszMD, pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GTiffDataset::GetMetadataItem( const char *pszName,
                                           const char *pszDomain )

{
    if( pszDomain == NULL || !EQUAL(pszDomain, "IMAGE_STRUCTURE") )
    {
        LoadGeoreferencingAndPamIfNeeded();
    }

    if( pszDomain != NULL && EQUAL(pszDomain,"ProxyOverviewRequest") )
    {
        return GDALPamDataset::GetMetadataItem( pszName, pszDomain );
    }
    else if( pszDomain != NULL && (EQUAL(pszDomain, MD_DOMAIN_RPC) ||
                                   EQUAL(pszDomain, MD_DOMAIN_IMD) ||
                                   EQUAL(pszDomain, MD_DOMAIN_IMAGERY)) )
    {
        LoadMetadata();
    }
    else if( pszDomain != NULL && EQUAL(pszDomain, "SUBDATASETS") )
    {
        ScanDirectories();
    }
    else if( pszDomain != NULL && EQUAL(pszDomain, "EXIF") )
    {
        LoadEXIFMetadata();
    }
    else if( pszDomain != NULL && EQUAL(pszDomain, "COLOR_PROFILE") )
    {
        LoadICCProfile();
    }
    else if( (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        pszName != NULL && EQUAL(pszName, GDALMD_AREA_OR_POINT) )
    {
        LoadMDAreaOrPoint();  // To set GDALMD_AREA_OR_POINT.
    }

#ifdef DEBUG_REACHED_VIRTUAL_MEM_IO
    else if( pszDomain != NULL && EQUAL(pszDomain, "_DEBUG_") &&
             pszName != NULL &&
             EQUAL(pszName, "UNREACHED_VIRTUALMEMIO_CODE_PATH") )
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
        return (osMissing.size()) ? CPLSPrintf("%s", osMissing.c_str()) : NULL;
    }
#endif
    else if( pszDomain != NULL && EQUAL(pszDomain, "_DEBUG_") &&
             pszName != NULL && EQUAL(pszName, "TIFFTAG_EXTRASAMPLES") )
    {
        CPLString osRet;
        uint16 *v = NULL;
        uint16 count = 0;

        if( TIFFGetField( hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            for( int i = 0; i < static_cast<int>(count); ++i )
            {
                if( i > 0 ) osRet += ",";
                osRet += CPLSPrintf("%d", v[i]);
            }
        }
        return (osRet.size()) ? CPLSPrintf("%s", osRet.c_str()) : NULL;
    }
    else if( pszDomain != NULL && EQUAL(pszDomain, "_DEBUG_") &&
             pszName != NULL && EQUAL(pszName, "TIFFTAG_PHOTOMETRIC") )
    {
        return CPLSPrintf("%d", nPhotometric);
    }

    else if( pszDomain != NULL && EQUAL(pszDomain, "_DEBUG_") &&
             pszName != NULL && EQUAL( pszName, "TIFFTAG_GDAL_METADATA") )
    {
        char* pszText = NULL;
        if( !TIFFGetField( hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) )
            return NULL;

        return CPLSPrintf("%s", pszText);
    }

    return oGTiffMDMD.GetMetadataItem( pszName, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GTiffDataset::SetMetadataItem( const char *pszName,
                                      const char *pszValue,
                                      const char *pszDomain )

{
    LoadGeoreferencingAndPamIfNeeded();

    if( bStreamingOut && bCrystalized )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Cannot modify metadata at that point in a streamed output file" );
        return CE_Failure;
    }

    if( (pszDomain != NULL) && EQUAL(pszDomain, "COLOR_PROFILE") )
    {
        bColorProfileMetadataChanged = true;
    }
    else if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
    {
        bMetadataChanged = true;
        // Cancel any existing metadata from PAM file.
        if( eAccess == GA_Update &&
            GDALPamDataset::GetMetadataItem(pszName, pszDomain) != NULL )
            GDALPamDataset::SetMetadataItem(pszName, NULL, pszDomain);
    }

    if( (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        pszName != NULL && EQUAL(pszName, GDALMD_AREA_OR_POINT) )
    {
        LookForProjection();
        bGeoTIFFInfoChanged = true;
    }

    return oGTiffMDMD.SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                         GetInternalHandle()                          */
/************************************************************************/

void *GTiffDataset::GetInternalHandle( const char * /* pszHandleName */ )

{
    return hTIFF;
}

/************************************************************************/
/*                         LoadEXIFMetadata()                           */
/************************************************************************/

void GTiffDataset::LoadEXIFMetadata()
{
    if( bEXIFMetadataLoaded )
        return;
    bEXIFMetadataLoaded = true;

    if( !SetDirectory() )
        return;

    VSILFILE* fp = VSI_TIFFGetVSILFile(TIFFClientdata( hTIFF ));

    GByte abyHeader[2] = { 0 };
    if( VSIFSeekL(fp, 0, SEEK_SET) != 0 ||
        VSIFReadL(abyHeader, 1, 2, fp) != 2 )
        return;

    const bool bLittleEndian = abyHeader[0] == 'I' && abyHeader[1] == 'I';
    const bool bLeastSignificantBit = CPL_IS_LSB != 0;
    const bool bSwabflag = bLittleEndian != bLeastSignificantBit;  // != is XOR.

    char** papszMetadata = NULL;
    toff_t nOffset = 0;  // TODO(b/28199387): Refactor to simplify casting.

    if( TIFFGetField(hTIFF, TIFFTAG_EXIFIFD, &nOffset) )
    {
        int nExifOffset = static_cast<int>(nOffset);
        int nInterOffset = 0;
        int nGPSOffset = 0;
        EXIFExtractMetadata( papszMetadata,
                             fp, static_cast<int>(nOffset),
                             bSwabflag, 0,
                             nExifOffset, nInterOffset, nGPSOffset);
    }

    if( TIFFGetField(hTIFF, TIFFTAG_GPSIFD, &nOffset) )
    {
        int nExifOffset = 0;  // TODO(b/28199387): Refactor to simplify casting.
        int nInterOffset = 0;
        int nGPSOffset = static_cast<int>(nOffset);
        EXIFExtractMetadata( papszMetadata,
                             fp, static_cast<int>(nOffset),
                             bSwabflag, 0,
                             nExifOffset, nInterOffset, nGPSOffset );
    }

    oGTiffMDMD.SetMetadata( papszMetadata, "EXIF" );
    CSLDestroy( papszMetadata );
}

/************************************************************************/
/*                           LoadMetadata()                             */
/************************************************************************/
void GTiffDataset::LoadMetadata()
{
    if( bIMDRPCMetadataLoaded )
        return;
    bIMDRPCMetadataLoaded = true;

    GDALMDReaderManager mdreadermanager;
    GDALMDReaderBase* mdreader =
        mdreadermanager.GetReader(osFilename,
                                  oOvManager.GetSiblingFiles(), MDR_ANY);

    if( NULL != mdreader )
    {
        mdreader->FillMetadata(&oGTiffMDMD);

        if(mdreader->GetMetadataDomain(MD_DOMAIN_RPC) == NULL)
        {
            char** papszRPCMD = GTiffDatasetReadRPCTag(hTIFF);
            if( papszRPCMD )
            {
                oGTiffMDMD.SetMetadata( papszRPCMD, MD_DOMAIN_RPC );
                CSLDestroy( papszRPCMD );
            }
        }

        papszMetadataFiles = mdreader->GetMetadataFiles();
    }
    else
    {
        char** papszRPCMD = GTiffDatasetReadRPCTag(hTIFF);
        if( papszRPCMD )
        {
            oGTiffMDMD.SetMetadata( papszRPCMD, MD_DOMAIN_RPC );
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
    if(NULL != papszMetadataFiles)
    {
        for( int i = 0; papszMetadataFiles[i] != NULL; ++i )
        {
            if( CSLFindString( papszFileList, papszMetadataFiles[i] ) < 0 )
            {
                papszFileList =
                    CSLAddString( papszFileList, papszMetadataFiles[i] );
            }
        }
    }

    if( !osGeorefFilename.empty() &&
        CSLFindString(papszFileList, osGeorefFilename) == -1 )
    {
        papszFileList = CSLAddString( papszFileList, osGeorefFilename );
    }

    return papszFileList;
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr GTiffDataset::CreateMaskBand(int nFlagsIn)
{
    ScanDirectories();

    if( poMaskDS != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "This TIFF dataset has already an internal mask band" );
        return CE_Failure;
    }
    else if( CPLTestBool(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", "NO")) )
    {
        if( nFlagsIn != GMF_PER_DATASET )
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "The only flag value supported for internal mask is "
                "GMF_PER_DATASET" );
            return CE_Failure;
        }

        int l_nCompression = COMPRESSION_PACKBITS;
        if( strstr(GDALGetMetadataItem(GDALGetDriverByName( "GTiff" ),
                                       GDAL_DMD_CREATIONOPTIONLIST, NULL ),
                   "<Value>DEFLATE</Value>") != NULL )
            l_nCompression = COMPRESSION_ADOBE_DEFLATE;

    /* -------------------------------------------------------------------- */
    /*      If we don't have read access, then create the mask externally.  */
    /* -------------------------------------------------------------------- */
        if( GetAccess() != GA_Update )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "File open for read-only accessing, "
                      "creating mask externally." );

            return GDALPamDataset::CreateMaskBand(nFlagsIn);
        }

        if( poBaseDS && !poBaseDS->SetDirectory() )
            return CE_Failure;

        if( !SetDirectory() )
            return CE_Failure;

        bool bIsOverview = false;
        uint32 nSubType = 0;
        if( TIFFGetField(hTIFF, TIFFTAG_SUBFILETYPE, &nSubType) )
        {
            bIsOverview = (nSubType & FILETYPE_REDUCEDIMAGE) != 0;

            if( (nSubType & FILETYPE_MASK) != 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Cannot create a mask on a TIFF mask IFD !" );
                return CE_Failure;
            }
        }

        const int bIsTiled = TIFFIsTiled(hTIFF);

        FlushDirectory();

        const toff_t nOffset =
            GTIFFWriteDirectory(
                hTIFF,
                bIsOverview ?
                FILETYPE_REDUCEDIMAGE | FILETYPE_MASK : FILETYPE_MASK,
                nRasterXSize, nRasterYSize,
                1, PLANARCONFIG_CONTIG, 1,
                nBlockXSize, nBlockYSize,
                bIsTiled, l_nCompression,
                PHOTOMETRIC_MASK, PREDICTOR_NONE,
                SAMPLEFORMAT_UINT, NULL, NULL, NULL, 0, NULL, "", NULL, NULL,
                NULL );
        if( nOffset == 0 )
            return CE_Failure;

        poMaskDS = new GTiffDataset();
        poMaskDS->bPromoteTo8Bits =
            CPLTestBool(
                CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
        if( poMaskDS->OpenOffset( hTIFF, ppoActiveDSRef, nOffset,
                                  false, GA_Update ) != CE_None)
        {
            delete poMaskDS;
            poMaskDS = NULL;
            return CE_Failure;
        }

        return CE_None;
    }

    return GDALPamDataset::CreateMaskBand(nFlagsIn);
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr GTiffRasterBand::CreateMaskBand( int nFlagsIn )
{
    poGDS->ScanDirectories();

    if( poGDS->poMaskDS != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "This TIFF dataset has already an internal mask band" );
        return CE_Failure;
    }

    if( CPLTestBool( CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", "NO") ) )
    {
        return poGDS->CreateMaskBand(nFlagsIn);
    }

    return GDALPamRasterBand::CreateMaskBand(nFlagsIn);
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
    if( strstr(fmt,"nknown field") != NULL )
        return;

    char *pszModFmt = PrepareTIFFErrorFormat( module, fmt );
    if( strstr(fmt, "does not end in null byte") != NULL )
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
    char *pszModFmt = NULL;

#if SIZEOF_VOIDP == 4
    // Case of one-strip file where the strip size is > 2GB (#5403).
    if( strcmp(module, "TIFFStripSize") == 0 &&
        strstr(fmt, "Integer overflow") != NULL )
    {
        bGlobalStripIntegerOverflow = true;
        return;
    }
    if( bGlobalStripIntegerOverflow &&
        strstr(fmt, "Cannot handle zero strip size") != NULL )
    {
        return;
    }
#endif

#ifdef BIGTIFF_SUPPORT
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
#endif

    pszModFmt = PrepareTIFFErrorFormat( module, fmt );
    CPLErrorV( CE_Failure, CPLE_AppDefined, pszModFmt, ap );
    CPLFree( pszModFmt );
}

/************************************************************************/
/*                          GTiffTagExtender()                          */
/*                                                                      */
/*      Install tags specially known to GDAL.                           */
/************************************************************************/

static TIFFExtendProc _ParentExtender = NULL;

static void GTiffTagExtender(TIFF *tif)

{
    const TIFFFieldInfo xtiffFieldInfo[] = {
        { TIFFTAG_GDAL_METADATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM,
          TRUE, FALSE, const_cast<char *>( "GDALMetadata" ) },
        { TIFFTAG_GDAL_NODATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM,
          TRUE, FALSE, const_cast<char*>( "GDALNoDataValue" ) },
        { TIFFTAG_RPCCOEFFICIENT, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM,
          TRUE, TRUE, const_cast<char *>( "RPCCoefficient" ) }
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

#if HAVE_CXX11_MUTEX
static std::mutex oDeleteMutex;
#else
static CPLMutex* hGTiffOneTimeInitMutex = NULL;
#endif  // HAVE_CXX11_MUTEX

int GTiffOneTimeInit()

{
#if HAVE_CXX11_MUTEX
    std::lock_guard<std::mutex> oLock(oDeleteMutex);
#else
    CPLMutexHolder oHolder( &hGTiffOneTimeInitMutex);
#endif

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
#if defined(BIGTIFF_SUPPORT) && !defined(RENAME_INTERNAL_LIBTIFF_SYMBOLS)
#if defined(HAVE_DLFCN_H) && !defined(WIN32)
    const char* (*pfnVersion)(void);
    pfnVersion = (const char* (*)(void)) dlsym(RTLD_DEFAULT, "TIFFGetVersion");
    if( pfnVersion )
    {
        const char* pszVersion = pfnVersion();
        if( pszVersion && strstr(pszVersion, "Version 3.") != NULL )
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "libtiff version mismatch: You're linking against libtiff 3.X, "
                "but GDAL has been compiled against libtiff >= 4.0.0" );
        }
    }
#endif  // HAVE_DLFCN_H
#endif  // BIGTIFF_SUPPORT

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
    CSVDeaccess( NULL );

#if defined(LIBGEOTIFF_VERSION) && LIBGEOTIFF_VERSION > 1150
    GTIFDeaccessCSV();
#endif

#if !HAVE_CXX11
    if( hGTiffOneTimeInitMutex != NULL )
    {
        CPLDestroyMutex(hGTiffOneTimeInitMutex);
        hGTiffOneTimeInitMutex = NULL;
    }
#endif  // !HAVE_CXX11

    LibgeotiffOneTimeCleanupMutex();
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
    else
        CPLError( CE_Warning, CPLE_IllegalArg,
                  "%s=%s value not recognised, ignoring.",
                  pszVariableName,pszValue );

#if defined(TIFFLIB_VERSION) && TIFFLIB_VERSION > 20031007  // 3.6.0
    if( nCompression != COMPRESSION_NONE &&
        !TIFFIsCODECConfigured((uint16) nCompression) )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Cannot create TIFF file due to missing codec for %s.", pszValue );
        return -1;
    }
#endif

    return nCompression;
}

/************************************************************************/
/*                          GDALRegister_GTiff()                        */
/************************************************************************/

void GDALRegister_GTiff()

{
    if( GDALGetDriverByName( "GTiff" ) != NULL )
        return;

    char szCreateOptions[5000] = { '\0' };
    char szOptionalCompressItems[500] = { '\0' };
    bool bHasJPEG = false;
    bool bHasLZW = false;
    bool bHasDEFLATE = false;
    bool bHasLZMA = false;

    GDALDriver *poDriver = new GDALDriver();

/* -------------------------------------------------------------------- */
/*      Determine which compression codecs are available that we        */
/*      want to advertise.  If we are using an old libtiff we won't     */
/*      be able to find out so we just assume all are available.        */
/* -------------------------------------------------------------------- */
    strcpy( szOptionalCompressItems, "       <Value>NONE</Value>" );

#if TIFFLIB_VERSION <= 20040919
    strcat( szOptionalCompressItems,
            "       <Value>PACKBITS</Value>"
            "       <Value>JPEG</Value>"
            "       <Value>LZW</Value>"
            "       <Value>DEFLATE</Value>" );
    bHasLZW = true;
    bHasDEFLATE = true;
#else
    TIFFCodec *codecs = TIFFGetConfiguredCODECs();

    for( TIFFCodec *c = codecs; c->name; ++c )
    {
        if( c->scheme == COMPRESSION_PACKBITS )
        {
            strcat( szOptionalCompressItems,
                    "       <Value>PACKBITS</Value>" );
        }
        else if( c->scheme == COMPRESSION_JPEG )
        {
            bHasJPEG = true;
            strcat( szOptionalCompressItems,
                    "       <Value>JPEG</Value>" );
        }
        else if( c->scheme == COMPRESSION_LZW )
        {
            bHasLZW = true;
            strcat( szOptionalCompressItems,
                    "       <Value>LZW</Value>" );
        }
        else if( c->scheme == COMPRESSION_ADOBE_DEFLATE )
        {
            bHasDEFLATE = true;
            strcat( szOptionalCompressItems,
                    "       <Value>DEFLATE</Value>" );
        }
        else if( c->scheme == COMPRESSION_CCITTRLE )
        {
            strcat( szOptionalCompressItems,
                    "       <Value>CCITTRLE</Value>" );
        }
        else if( c->scheme == COMPRESSION_CCITTFAX3 )
        {
            strcat( szOptionalCompressItems,
                    "       <Value>CCITTFAX3</Value>" );
        }
        else if( c->scheme == COMPRESSION_CCITTFAX4 )
        {
            strcat( szOptionalCompressItems,
                    "       <Value>CCITTFAX4</Value>" );
        }
        else if( c->scheme == COMPRESSION_LZMA )
        {
            bHasLZMA = true;
            strcat( szOptionalCompressItems,
                    "       <Value>LZMA</Value>" );
        }
    }
    _TIFFfree( codecs );
#endif

/* -------------------------------------------------------------------- */
/*      Build full creation option list.                                */
/* -------------------------------------------------------------------- */
    snprintf( szCreateOptions, sizeof(szCreateOptions), "%s%s%s",
              "<CreationOptionList>"
              "   <Option name='COMPRESS' type='string-select'>",
              szOptionalCompressItems,
              "   </Option>");
    if( bHasLZW || bHasDEFLATE )
        strcat( szCreateOptions, ""
"   <Option name='PREDICTOR' type='int' description='Predictor Type (1=default, 2=horizontal differencing, 3=floating point prediction)'/>");
    strcat( szCreateOptions, ""
"   <Option name='DISCARD_LSB' type='string' description='Number of least-significant bits to set to clear as a single value or comma-separated list of values for per-band values'/>" );
    if( bHasJPEG )
    {
        strcat( szCreateOptions, ""
"   <Option name='JPEG_QUALITY' type='int' description='JPEG quality 1-100' default='75'/>"
"   <Option name='JPEGTABLESMODE' type='int' description='Content of JPEGTABLES tag. 0=no JPEGTABLES tag, 1=Quantization tables only, 2=Huffman tables only, 3=Both' default='1'/>" );
#ifdef JPEG_DIRECT_COPY
        strcat( szCreateOptions, ""
"   <Option name='JPEG_DIRECT_COPY' type='boolean' description='To copy without any decompression/recompression a JPEG source file' default='NO'/>");
#endif
    }
    if( bHasDEFLATE )
        strcat( szCreateOptions, ""
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='6'/>");
    if( bHasLZMA )
        strcat( szCreateOptions, ""
"   <Option name='LZMA_PRESET' type='int' description='LZMA compression level 0(fast)-9(slow)' default='6'/>");
    strcat( szCreateOptions, ""
"   <Option name='NUM_THREADS' type='string' description='Number of worker threads for compression. Can be set to ALL_CPUS' default='1'/>"
"   <Option name='NBITS' type='int' description='BITS for sub-byte files (1-7), sub-uint16 (9-15), sub-uint32 (17-31), or float32 (16)'/>"
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
#ifdef BIGTIFF_SUPPORT
"   <Option name='BIGTIFF' type='string-select' description='Force creation of BigTIFF file'>"
"     <Value>YES</Value>"
"     <Value>NO</Value>"
"     <Value>IF_NEEDED</Value>"
"     <Value>IF_SAFER</Value>"
"   </Option>"
#endif
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
"</CreationOptionList>" );

/* -------------------------------------------------------------------- */
/*      Set the driver details.                                         */
/* -------------------------------------------------------------------- */
    poDriver->SetDescription( "GTiff" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GeoTIFF" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_gtiff.html" );
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/tiff" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "tif" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "tif tiff" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 "
                               "Float64 CInt16 CInt32 CFloat32 CFloat64" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, szCreateOptions );
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

    poDriver->pfnOpen = GTiffDataset::Open;
    poDriver->pfnCreate = GTiffDataset::Create;
    poDriver->pfnCreateCopy = GTiffDataset::CreateCopy;
    poDriver->pfnUnloadDriver = GDALDeregister_GTiff;
    poDriver->pfnIdentify = GTiffDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
