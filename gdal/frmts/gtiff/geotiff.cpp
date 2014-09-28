/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

/* If we use sunpro compiler on linux. Weird idea indeed ! */
#if defined(__SUNPRO_CC) && defined(__linux__)
#define _GNU_SOURCE
#elif defined(__GNUC__) && !defined(_GNU_SOURCE)
/* Required to use RTLD_DEFAULT of dlfcn.h */
#define _GNU_SOURCE
#endif

#include "gdal_pam.h"
#define CPL_SERV_H_INCLUDED

#include "xtiffio.h"
#include "geovalues.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_minixml.h"
#include "gt_overview.h"
#include "ogr_spatialref.h"
#include "tif_float.h"
#include "gtiff.h"
#include "gdal_csv.h"
#include "gt_wkt_srs.h"
#include "tifvsi.h"
#include "cpl_multiproc.h"
#include "cplkeywordparser.h"
#include "gt_jpeg_copy.h"
#include <set>

#ifdef INTERNAL_LIBTIFF
#include "tiffiop.h"
#endif

CPL_CVSID("$Id$");

#if SIZEOF_VOIDP == 4
static int bGlobalStripIntegerOverflow = FALSE;
#endif

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
    { "TIFFTAG_IMAGEDESCRIPTION", TIFFTAG_IMAGEDESCRIPTION, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_SOFTWARE", TIFFTAG_SOFTWARE, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_DATETIME", TIFFTAG_DATETIME, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_ARTIST", TIFFTAG_ARTIST, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_HOSTCOMPUTER", TIFFTAG_HOSTCOMPUTER, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_COPYRIGHT", TIFFTAG_COPYRIGHT, GTIFFTAGTYPE_STRING },
    { "TIFFTAG_XRESOLUTION", TIFFTAG_XRESOLUTION, GTIFFTAGTYPE_FLOAT },
    { "TIFFTAG_YRESOLUTION", TIFFTAG_YRESOLUTION, GTIFFTAGTYPE_FLOAT },
    { "TIFFTAG_RESOLUTIONUNIT", TIFFTAG_RESOLUTIONUNIT, GTIFFTAGTYPE_SHORT }, /* dealt as special case */
    { "TIFFTAG_MINSAMPLEVALUE", TIFFTAG_MINSAMPLEVALUE, GTIFFTAGTYPE_SHORT },
    { "TIFFTAG_MAXSAMPLEVALUE", TIFFTAG_MAXSAMPLEVALUE, GTIFFTAGTYPE_SHORT },
};

/************************************************************************/
/* ==================================================================== */
/*                                GDALOverviewDS                        */
/* ==================================================================== */
/************************************************************************/

/* GDALOverviewDS is not specific to GTiff and could probably be moved */
/* in gcore. It is currently used to generate a fake */
/* dataset from the overview levels of the source dataset that is taken */
/* by CreateCopy() */

#include "gdal_proxy.h"

class GDALOverviewBand;

class GDALOverviewDS : public GDALDataset
{
    private:

        friend class GDALOverviewBand;

        GDALDataset* poDS;
        GDALDataset* poOvrDS;
        int          nOvrLevel;

    public:
                        GDALOverviewDS(GDALDataset* poDS, int nOvrLevel);
        virtual        ~GDALOverviewDS();

        virtual char  **GetMetadata( const char * pszDomain = "" );
        virtual const char *GetMetadataItem( const char * pszName,
                                             const char * pszDomain = "" );
};

class GDALOverviewBand : public GDALProxyRasterBand
{
    protected:
        GDALRasterBand*         poUnderlyingBand;
        virtual GDALRasterBand* RefUnderlyingRasterBand();

    public:
                    GDALOverviewBand(GDALOverviewDS* poDS, int nBand);
        virtual    ~GDALOverviewBand();
};

GDALOverviewDS::GDALOverviewDS(GDALDataset* poDS, int nOvrLevel)
{
    this->poDS = poDS;
    this->nOvrLevel = nOvrLevel;
    eAccess = poDS->GetAccess();
    nRasterXSize = poDS->GetRasterBand(1)->GetOverview(nOvrLevel)->GetXSize();
    nRasterYSize = poDS->GetRasterBand(1)->GetOverview(nOvrLevel)->GetYSize();
    poOvrDS = poDS->GetRasterBand(1)->GetOverview(nOvrLevel)->GetDataset();
    nBands = poDS->GetRasterCount();
    int i;
    for(i=0;i<nBands;i++)
        SetBand(i+1, new GDALOverviewBand(this, i+1));
}

GDALOverviewDS::~GDALOverviewDS()
{
    FlushCache();
}

char  **GDALOverviewDS::GetMetadata( const char * pszDomain )
{
    if (poOvrDS != NULL)
        return poOvrDS->GetMetadata(pszDomain);

    return poDS->GetMetadata(pszDomain);
}

const char *GDALOverviewDS::GetMetadataItem( const char * pszName, const char * pszDomain )
{
    if (poOvrDS != NULL)
        return poOvrDS->GetMetadataItem(pszName, pszDomain);

    return poDS->GetMetadataItem(pszName, pszDomain);
}

GDALOverviewBand::GDALOverviewBand(GDALOverviewDS* poDS, int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    poUnderlyingBand = poDS->poDS->GetRasterBand(nBand)->GetOverview(poDS->nOvrLevel);
    nRasterXSize = poDS->nRasterXSize;
    nRasterYSize = poDS->nRasterYSize;
    eDataType = poUnderlyingBand->GetRasterDataType();
    poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

GDALOverviewBand::~GDALOverviewBand()
{
    FlushCache();
}

GDALRasterBand* GDALOverviewBand::RefUnderlyingRasterBand()
{
    return poUnderlyingBand;
}

/************************************************************************/
/*                            IsPowerOfTwo()                            */
/************************************************************************/

static int IsPowerOfTwo(unsigned int i)
{
    int nBitSet = 0;
    while(i != 0)
    {
        if ((i & 1))
            nBitSet ++;
        i >>= 1;
    }
    return nBitSet == 1;
}

/************************************************************************/
/*                     GTIFFGetOverviewBlockSize()                      */
/************************************************************************/

void GTIFFGetOverviewBlockSize(int* pnBlockXSize, int* pnBlockYSize)
{
    static int bHasWarned = FALSE;
    const char* pszVal = CPLGetConfigOption("GDAL_TIFF_OVR_BLOCKSIZE", "128");
    int nOvrBlockSize = atoi(pszVal);
    if (nOvrBlockSize < 64 || nOvrBlockSize > 4096 ||
        !IsPowerOfTwo(nOvrBlockSize))
    {
        if (!bHasWarned)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Wrong value for GDAL_TIFF_OVR_BLOCKSIZE : %s. "
                    "Should be a power of 2 between 64 and 4096. Defaulting to 128",
                    pszVal);
            bHasWarned = TRUE;
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
/*				GTiffDataset				*/
/* ==================================================================== */
/************************************************************************/

class GTiffRasterBand;
class GTiffRGBABand;
class GTiffBitmapBand;
class GTiffJPEGOverviewDS;
class GTiffJPEGOverviewBand;

class GTiffDataset : public GDALPamDataset
{
    friend class GTiffRasterBand;
    friend class GTiffSplitBand;
    friend class GTiffRGBABand;
    friend class GTiffBitmapBand;
    friend class GTiffSplitBitmapBand;
    friend class GTiffOddBitsBand;
    friend class GTiffJPEGOverviewDS;
    friend class GTiffJPEGOverviewBand;

    friend void    GTIFFSetJpegQuality(GDALDatasetH hGTIFFDS, int nJpegQuality);
    
    TIFF	*hTIFF;
    VSILFILE *fpL;
    GTiffDataset **ppoActiveDSRef;
    GTiffDataset *poActiveDS; /* only used in actual base */

    int         bScanDeferred;
    void        ScanDirectories();

    toff_t      nDirOffset;
    int		bBase;
    int         bCloseTIFFHandle; /* usefull for closing TIFF handle opened by GTIFF_DIR: */

    uint16	nPlanarConfig;
    uint16	nSamplesPerPixel;
    uint16	nBitsPerSample;
    uint32	nRowsPerStrip;
    uint16	nPhotometric;
    uint16      nSampleFormat;
    uint16      nCompression;
    
    int		nBlocksPerBand;

    uint32      nBlockXSize;
    uint32	nBlockYSize;

    int		nLoadedBlock;		/* or tile */
    int         bLoadedBlockDirty;  
    GByte	*pabyBlockBuf;

    CPLErr      LoadBlockBuf( int nBlockId, int bReadFromDisk = TRUE );
    CPLErr      FlushBlockBuf();
    int         bWriteErrorInFlushBlockBuf;

    char	*pszProjection;
    int         bLookedForProjection;
    int         bLookedForMDAreaOrPoint;

    void        LoadMDAreaOrPoint();
    void        LookForProjection();
#ifdef ESRI_BUILD
    void        AdjustLinearUnit( short UOMLength );
#endif

    double	adfGeoTransform[6];
    int		bGeoTransformValid;

    int         bTreatAsRGBA;
    int         bCrystalized;

    void	Crystalize();

    GDALColorTable *poColorTable;

    void	WriteGeoTIFFInfo();
    int		SetDirectory( toff_t nDirOffset = 0 );

    int		nOverviewCount;
    GTiffDataset **papoOverviewDS;

    int         nJPEGOverviewVisibilityFlag; /* if > 0, the implicit JPEG overviews are visible through GetOverviewCount() */
    int         nJPEGOverviewCount; /* currently visible overviews. Generally == nJPEGOverviewCountOri */
    int         nJPEGOverviewCountOri; /* size of papoJPEGOverviewDS */
    GTiffJPEGOverviewDS **papoJPEGOverviewDS;
    int         GetJPEGOverviewCount();

    int		nGCPCount;
    GDAL_GCP	*pasGCPList;

    int         IsBlockAvailable( int nBlockId );

    int         bGeoTIFFInfoChanged;
    int         bForceUnsetGT;
    int         bForceUnsetProjection;
    int         bNoDataSet;
    double      dfNoDataValue;

    int	        bMetadataChanged;
    int         bColorProfileMetadataChanged;

    int         bNeedsRewrite;

    void        ApplyPamInfo();
    void        PushMetadataToPam();

    GDALMultiDomainMetadata oGTiffMDMD;

    CPLString   osProfile;
    char      **papszCreationOptions;

    int         bLoadingOtherBands;

    static void WriteRPCTag( TIFF *, char ** );
    void        ReadRPCTag();

    void*        pabyTempWriteBuffer;
    int          nTempWriteBufferSize;
    int          WriteEncodedTile(uint32 tile, GByte* pabyData, int bPreserveDataBuffer);
    int          WriteEncodedStrip(uint32 strip, GByte* pabyData, int bPreserveDataBuffer);

    GTiffDataset* poMaskDS;
    GTiffDataset* poBaseDS;

    CPLString    osFilename;

    int          bFillEmptyTiles;
    void         FillEmptyTiles(void);

    void         FlushDirectory();
    CPLErr       CleanOverviews();

    /* Used for the all-in-on-strip case */
    int           nLastLineRead;
    int           nLastBandRead;
    int           bTreatAsSplit;
    int           bTreatAsSplitBitmap;

    int           bClipWarn;

    CPLString     osRPBFile;
    int           FindRPBFile();
    CPLString     osRPCFile;
    int           FindRPCFile();
    CPLString     osIMDFile;
    int           FindIMDFile();
    CPLString     osPVLFile;
    int           FindPVLFile();
    int           bHasSearchedRPC;
    void          LoadRPCRPB();
    int           bHasSearchedIMD;
    int           bHasSearchedPVL;
    void          LoadIMDPVL();

    int           bEXIFMetadataLoaded;
    void          LoadEXIFMetadata();

    int           bICCMetadataLoaded;
    void          LoadICCProfile();

    int           bHasWarnedDisableAggressiveBandCaching;

    int           bDontReloadFirstBlock; /* Hack for libtiff 3.X and #3633 */

    int           nZLevel;
    int           nLZMAPreset;
    int           nJpegQuality;
    
    int           bPromoteTo8Bits;

    int           bDebugDontWriteBlocks;

    CPLErr        RegisterNewOverviewDataset(toff_t nOverviewOffset);
    CPLErr        CreateOverviewsFromSrcOverviews(GDALDataset* poSrcDS);
    CPLErr        CreateInternalMaskOverviews(int nOvrBlockXSize,
                                              int nOvrBlockYSize);

    int           bIsFinalized;
    int           Finalize();

    int           bIgnoreReadErrors;

    CPLString     osGeorefFilename;

    int           bDirectIO;
    
    int           nSetPhotometricFromBandColorInterp;

    CPLVirtualMem *pBaseMapping;
    int            nRefBaseMapping;

  protected:
    virtual int         CloseDependentDatasets();

  public:
                 GTiffDataset();
                 ~GTiffDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    CPLErr         SetGCPs( int, const GDAL_GCP *, const char * );

    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace);
    virtual char **GetFileList(void);

    virtual CPLErr IBuildOverviews( const char *, int, int *, int, int *, 
                                    GDALProgressFunc, void * );

    CPLErr	   OpenOffset( TIFF *, GTiffDataset **ppoActiveDSRef, 
                               toff_t nDirOffset, int bBaseIn, GDALAccess, 
                               int bAllowRGBAInterface = TRUE, int bReadGeoTransform = FALSE,
                               char** papszSiblingFiles = NULL);

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
    virtual void    FlushCache( void );

    virtual char      **GetMetadataDomainList();
    virtual CPLErr  SetMetadata( char **, const char * = "" );
    virtual char  **GetMetadata( const char * pszDomain = "" );
    virtual CPLErr  SetMetadataItem( const char*, const char*, 
                                     const char* = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual void   *GetInternalHandle( const char * );

    virtual CPLErr          CreateMaskBand( int nFlags );

    // only needed by createcopy and close code.
    static int	    WriteMetadata( GDALDataset *, TIFF *, int, const char *,
                                   const char *, char **, int bExcludeRPBandIMGFileWriting = FALSE );
    static void	    WriteNoDataValue( TIFF *, double );

    static TIFF *   CreateLL( const char * pszFilename,
                              int nXSize, int nYSize, int nBands,
                              GDALDataType eType,
                              double dfExtraSpaceForOverviews,
                              char **papszParmList,
                              VSILFILE** pfpL);

    CPLErr   WriteEncodedTileOrStrip(uint32 tile_or_strip, void* data, int bPreserveDataBuffer);

    static void SaveICCProfile(GTiffDataset *pDS, TIFF *hTIFF, char **papszParmList, uint32 nBitsPerSample);
};


/************************************************************************/
/* ==================================================================== */
/*                        GTiffJPEGOverviewDS                           */
/* ==================================================================== */
/************************************************************************/

class GTiffJPEGOverviewDS : public GDALDataset
{
        friend class GTiffJPEGOverviewBand;
        GTiffDataset* poParentDS;
        int nOverviewLevel;

        int        nJPEGTableSize;
        GByte     *pabyJPEGTable;
        CPLString  osTmpFilenameJPEGTable;

        CPLString    osTmpFilename;
        GDALDataset* poJPEGDS;
        int          nBlockId; /* valid block id of the parent DS that match poJPEGDS */

    public:
        GTiffJPEGOverviewDS(GTiffDataset* poParentDS, int nOverviewLevel,
                            const void* pJPEGTable, int nJPEGTableSize);
       ~GTiffJPEGOverviewDS();

       virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace);
};

class GTiffJPEGOverviewBand : public GDALRasterBand
{
    public:
        GTiffJPEGOverviewBand(GTiffJPEGOverviewDS* poDS, int nBand);

        virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/*                        GTiffJPEGOverviewDS()                         */
/************************************************************************/

GTiffJPEGOverviewDS::GTiffJPEGOverviewDS(GTiffDataset* poParentDS, int nOverviewLevel,
                                         const void* pJPEGTable, int nJPEGTableSizeIn)
{
    this->poParentDS = poParentDS;
    this->nOverviewLevel = nOverviewLevel;
    poJPEGDS = NULL;
    nBlockId = -1;

    osTmpFilenameJPEGTable.Printf("/vsimem/jpegtable_%p", this);
    nJPEGTableSize = nJPEGTableSizeIn;

    const GByte abyAdobeAPP14RGB[] = {
        0xFF, 0xEE, 0x00, 0x0E, 0x41, 0x64, 0x6F, 0x62, 0x65, 0x00,
        0x64, 0x00, 0x00, 0x00, 0x00, 0x00 };
    int bAddAdobe = ( poParentDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
                      poParentDS->nPhotometric != PHOTOMETRIC_YCBCR && poParentDS->nBands == 3 );
    pabyJPEGTable = (GByte*) CPLMalloc(nJPEGTableSize + ((bAddAdobe) ? sizeof(abyAdobeAPP14RGB) : 0));
    memcpy(pabyJPEGTable, pJPEGTable, nJPEGTableSize);
    if( bAddAdobe )
    {
        memcpy(pabyJPEGTable + nJPEGTableSize, abyAdobeAPP14RGB, sizeof(abyAdobeAPP14RGB));
        nJPEGTableSize += sizeof(abyAdobeAPP14RGB);
    }
    VSIFCloseL(VSIFileFromMemBuffer( osTmpFilenameJPEGTable, pabyJPEGTable, nJPEGTableSize, TRUE ));

    int nScaleFactor = 1 << nOverviewLevel;
    nRasterXSize = (poParentDS->nRasterXSize + nScaleFactor - 1) / nScaleFactor; 
    nRasterYSize = (poParentDS->nRasterYSize + nScaleFactor - 1) / nScaleFactor; 

    int i;
    for(i=1;i<=poParentDS->nBands;i++)
        SetBand(i, new GTiffJPEGOverviewBand(this, i));

    SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    if ( poParentDS->nPhotometric == PHOTOMETRIC_YCBCR )
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
        GDALClose( (GDALDatasetH) poJPEGDS );
    VSIUnlink(osTmpFilenameJPEGTable);
    if( osTmpFilename.size() )
        VSIUnlink(osTmpFilename);
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GTiffJPEGOverviewDS::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace)

{
    /* For non-single strip JPEG-IN-TIFF, the block based strategy will */
    /* be the most efficient one, to avoid decompressing the JPEG content */
    /* for each requested band */
    if( nBandCount > 1 && poParentDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
        ((int)poParentDS->nBlockXSize < poParentDS->nRasterXSize ||
        poParentDS->nBlockYSize > 1) )
    {
        return BlockBasedRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                                   pData, nBufXSize, nBufYSize,
                                   eBufType, nBandCount, panBandMap,
                                   nPixelSpace, nLineSpace, nBandSpace );
    }
    else
    {
        return GDALDataset::IRasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType,
                nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace);
    }

}

/************************************************************************/
/*                        GTiffJPEGOverviewBand()                       */
/************************************************************************/

GTiffJPEGOverviewBand::GTiffJPEGOverviewBand(GTiffJPEGOverviewDS* poDS, int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = poDS->poParentDS->GetRasterBand(nBand)->GetRasterDataType();
    poDS->poParentDS->GetRasterBand(nBand)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    int nScaleFactor = 1 << poDS->nOverviewLevel;
    nBlockXSize = (nBlockXSize + nScaleFactor - 1) / nScaleFactor; 
    if( nBlockYSize == 1 )
        nBlockYSize = 1;
    else
        nBlockYSize = (nBlockYSize + nScaleFactor - 1) / nScaleFactor; 
}

/************************************************************************/
/*                          IReadBlock()                                */
/************************************************************************/

CPLErr GTiffJPEGOverviewBand::IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage )
{
    GTiffJPEGOverviewDS* poGDS = (GTiffJPEGOverviewDS*)poDS;

    /* Compute the source block ID */
    int nBlockId;
    if( nBlockYSize == 1 )
    {
        nBlockId = 0;
    }
    else
    {
        int nBlocksPerRow = DIV_ROUND_UP(poGDS->poParentDS->nRasterXSize, poGDS->poParentDS->nBlockXSize);
        nBlockId = nBlockYOff * nBlocksPerRow + nBlockXOff;
    }
    if( poGDS->poParentDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        nBlockId += (nBand-1) * poGDS->poParentDS->nBlocksPerBand;
    }

    if( !poGDS->poParentDS->SetDirectory() )
        return CE_Failure;

    /* Make sure it is available */
    int nDataTypeSize = GDALGetDataTypeSize(eDataType)/8;
    if( !poGDS->poParentDS->IsBlockAvailable(nBlockId) )
    {
        memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize );
        return CE_None;
    }

    int nScaleFactor = 1 << poGDS->nOverviewLevel;
    if( poGDS->poJPEGDS == NULL || nBlockId != poGDS->nBlockId )
    {
        toff_t *panByteCounts = NULL;
        toff_t *panOffsets = NULL;
        vsi_l_offset nOffset = 0;
        vsi_l_offset nByteCount = 0;

        /* Find offset and size of the JPEG tile/strip */
        TIFF* hTIFF = poGDS->poParentDS->hTIFF;
        if( (( TIFFIsTiled( hTIFF ) 
            && TIFFGetField( hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts )
            && TIFFGetField( hTIFF, TIFFTAG_TILEOFFSETS, &panOffsets ) )
            || ( !TIFFIsTiled( hTIFF ) 
            && TIFFGetField( hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts )
            && TIFFGetField( hTIFF, TIFFTAG_STRIPOFFSETS, &panOffsets ) )) &&
            panByteCounts != NULL && panOffsets != NULL )
        {
            if( panByteCounts[nBlockId] < 2 )
                return CE_Failure;
            nOffset = panOffsets[nBlockId] + 2; /* skip leading 0xFF 0xF8 */
            nByteCount = panByteCounts[nBlockId] - 2;
        }
        else
        {
            return CE_Failure;
        }

        /* Special case for last strip that might be smaller than other strips */
        /* In which case we must invalidate the dataset */
        if( !TIFFIsTiled( hTIFF ) && poGDS->poParentDS->nBlockYSize > 1 &&
            (nBlockYOff + 1 == (int)DIV_ROUND_UP(poGDS->poParentDS->nRasterYSize, poGDS->poParentDS->nBlockYSize) ||
             (poGDS->poJPEGDS != NULL && poGDS->poJPEGDS->GetRasterYSize() != nBlockYSize * nScaleFactor)) )
        {
            if( poGDS->poJPEGDS != NULL )
                GDALClose( (GDALDatasetH) poGDS->poJPEGDS );
            poGDS->poJPEGDS = NULL;
        }

        CPLString osFileToOpen;
        poGDS->osTmpFilename.Printf("/vsimem/sparse_%p", poGDS);
        VSILFILE* fp = VSIFOpenL(poGDS->osTmpFilename, "wb+");

        /* If the size of the JPEG strip/tile is small enough, we will */
        /* read it from the TIFF file and forge a in-memory JPEG file with */
        /* the JPEG table followed by the JPEG data. */
        int bInMemoryJPEGFile = ( nByteCount < 256 * 256 );
        if( bInMemoryJPEGFile )
        {
            /* If the previous file was opened as a /vsisparse/, we have to re-open */
            if( poGDS->poJPEGDS != NULL &&
                strncmp(poGDS->poJPEGDS->GetDescription(), "/vsisparse/", strlen("/vsisparse/")) == 0 )
            {
                GDALClose( (GDALDatasetH) poGDS->poJPEGDS );
                poGDS->poJPEGDS = NULL;
            }
            osFileToOpen = poGDS->osTmpFilename;

            VSIFSeekL(fp, poGDS->nJPEGTableSize + nByteCount - 1, SEEK_SET);
            char ch = 0;
            VSIFWriteL(&ch, 1, 1, fp);
            GByte* pabyBuffer = VSIGetMemFileBuffer( poGDS->osTmpFilename, NULL, FALSE);
            memcpy(pabyBuffer, poGDS->pabyJPEGTable, poGDS->nJPEGTableSize);
            VSILFILE* fpTIF = (VSILFILE*) TIFFClientdata( hTIFF );
            VSIFSeekL(fpTIF, nOffset, SEEK_SET);
            VSIFReadL(pabyBuffer + poGDS->nJPEGTableSize, 1, (size_t)nByteCount, fpTIF);
        }
        else
        {
            /* If the JPEG strip/tile is too big (e.g. a single-strip JPEG-in-TIFF) */
            /* we will use /vsisparse mechanism to make a fake JPEG file */

            /* If the previous file was NOT opened as a /vsisparse/, we have to re-open */
            if( poGDS->poJPEGDS != NULL &&
                strncmp(GDALGetDescription(poGDS->poJPEGDS), "/vsisparse/", strlen("/vsisparse/")) != 0  )
            {
                GDALClose( (GDALDatasetH) poGDS->poJPEGDS );
                poGDS->poJPEGDS = NULL;
            }
            osFileToOpen = CPLSPrintf("/vsisparse/%s", poGDS->osTmpFilename.c_str());

            VSIFPrintfL(fp, "<VSISparseFile><SubfileRegion><Filename relative='0'>%s</Filename>"
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
                        (int)poGDS->nJPEGTableSize,
                        poGDS->poParentDS->GetDescription(),
                        (int)poGDS->nJPEGTableSize,
                        nOffset,
                        nByteCount);
        }
        VSIFCloseL(fp);

        if( poGDS->poJPEGDS == NULL )
        {
            const char* apszDrivers[] = { "JPEG", NULL };
            poGDS->poJPEGDS = (GDALDataset*) GDALOpenEx(osFileToOpen,
                                                                GDAL_OF_RASTER,
                                                                apszDrivers,
                                                                NULL, NULL);
            if( poGDS->poJPEGDS != NULL )
            {
                /* Force all implicit overviews to be available, even for small tiles */
                CPLSetThreadLocalConfigOption("JPEG_FORCE_INTERNAL_OVERVIEWS", "YES");
                GDALGetOverviewCount(GDALGetRasterBand(poGDS->poJPEGDS, 1));
                CPLSetThreadLocalConfigOption("JPEG_FORCE_INTERNAL_OVERVIEWS", NULL);

                poGDS->nBlockId = nBlockId;
            }
        }
        else
        {
            /* Trick: we invalidate the JPEG dataset to force a reload */
            /* of the new content */
            CPLErrorReset();
            poGDS->poJPEGDS->FlushCache();
            if( CPLGetLastErrorNo() != 0 )
            {
                GDALClose( (GDALDatasetH) poGDS->poJPEGDS );
                poGDS->poJPEGDS = NULL;
                return CE_Failure;
            }
            poGDS->nBlockId = nBlockId;
        }
    }

    CPLErr eErr = CE_Failure;
    if( poGDS->poJPEGDS )
    {
        GDALDataset* poDS = poGDS->poJPEGDS;

        int nReqXOff = 0, nReqYOff, nReqXSize, nReqYSize;
        if( nBlockYSize == 1 )
        {
            nReqYOff = nBlockYOff * nScaleFactor;
            nReqXSize = poDS->GetRasterXSize();
            nReqYSize = nScaleFactor;
        }
        else
        {
            nReqYOff = 0;
            nReqXSize = nBlockXSize * nScaleFactor;
            nReqYSize = nBlockYSize * nScaleFactor;
        }
        int nBufXSize = nBlockXSize;
        int nBufYSize = nBlockYSize;
        if( nReqXOff + nReqXSize > poDS->GetRasterXSize() )
        {
            nReqXSize = poDS->GetRasterXSize() - nReqXOff;
            nBufXSize = nReqXSize / nScaleFactor;
            if( nBufXSize == 0 ) nBufXSize = 1;
        }
        if( nReqYOff + nReqYSize > poDS->GetRasterYSize() )
        {
            nReqYSize = poDS->GetRasterYSize() - nReqYOff;
            nBufYSize = nReqYSize / nScaleFactor;
            if( nBufYSize == 0 ) nBufYSize = 1;
        }

        int nSrcBand = ( poGDS->poParentDS->nPlanarConfig == PLANARCONFIG_SEPARATE ) ? 1 : nBand;
        if( nSrcBand <= poDS->GetRasterCount() )
        {
            eErr = poDS->GetRasterBand(nSrcBand)->RasterIO(GF_Read,
                                 nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                                 pImage,
                                 nBufXSize, nBufYSize, eDataType,
                                 0, nBlockXSize * nDataTypeSize );
        }
    }

    return eErr;
}

/************************************************************************/
/*                        GTIFFSetJpegQuality()                         */
/* Called by GTIFFBuildOverviews() to set the jpeg quality on the IFD   */
/* of the .ovr file                                                     */
/************************************************************************/

void    GTIFFSetJpegQuality(GDALDatasetH hGTIFFDS, int nJpegQuality)
{
    CPLAssert(EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(hGTIFFDS)), "GTIFF"));

    GTiffDataset* poDS = (GTiffDataset*)hGTIFFDS;
    poDS->nJpegQuality = nJpegQuality;

    poDS->ScanDirectories();

    int i;
    for(i=0;i<poDS->nOverviewCount;i++)
        poDS->papoOverviewDS[i]->nJpegQuality = nJpegQuality;
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

    int                bHaveOffsetScale;
    double             dfOffset;
    double             dfScale;
    CPLString          osUnitType;
    CPLString          osDescription;

    CPLErr DirectIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace );

    std::set<GTiffRasterBand **> aSetPSelf;
    static void     DropReferenceVirtualMem(void* pUserData);
    CPLVirtualMem * GetVirtualMemAutoInternal( GDALRWFlag eRWFlag,
                                               int *pnPixelSpace,
                                               GIntBig *pnLineSpace,
                                               char **papszOptions );
protected:
    GTiffDataset       *poGDS;
    GDALMultiDomainMetadata oGTiffMDMD;

    int                bNoDataSet;
    double             dfNoDataValue;

    void NullBlock( void *pData );
    CPLErr FillCacheForOtherBands( int nBlockXOff, int nBlockYOff );

public:
                   GTiffRasterBand( GTiffDataset *, int );
                  ~GTiffRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace );

    virtual const char *GetDescription() const;
    virtual void        SetDescription( const char * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr          SetColorTable( GDALColorTable * );
    virtual double	    GetNoDataValue( int * );
    virtual CPLErr	    SetNoDataValue( double );

    virtual double GetOffset( int *pbSuccess = NULL );
    virtual CPLErr SetOffset( double dfNewValue );
    virtual double GetScale( int *pbSuccess = NULL );
    virtual CPLErr SetScale( double dfNewValue );
    virtual const char* GetUnitType();
    virtual CPLErr SetUnitType( const char *pszNewValue );
    virtual CPLErr SetColorInterpretation( GDALColorInterp );

    virtual char      **GetMetadataDomainList();
    virtual CPLErr  SetMetadata( char **, const char * = "" );
    virtual char  **GetMetadata( const char * pszDomain = "" );
    virtual CPLErr  SetMetadataItem( const char*, const char*, 
                                     const char* = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );

    virtual GDALRasterBand *GetMaskBand();
    virtual int             GetMaskFlags();
    virtual CPLErr          CreateMaskBand( int nFlags );

    virtual CPLVirtualMem  *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                               int *pnPixelSpace,
                                               GIntBig *pnLineSpace,
                                               char **papszOptions );
};

/************************************************************************/
/*                           GTiffRasterBand()                          */
/************************************************************************/

GTiffRasterBand::GTiffRasterBand( GTiffDataset *poDS, int nBand )

{
    poGDS = poDS;

    this->poDS = poDS;
    this->nBand = nBand;

    bHaveOffsetScale = FALSE;
    dfOffset = 0.0;
    dfScale = 1.0;

/* -------------------------------------------------------------------- */
/*      Get the GDAL data type.                                         */
/* -------------------------------------------------------------------- */
    uint16		nSampleFormat = poDS->nSampleFormat;

    eDataType = GDT_Unknown;

    if( poDS->nBitsPerSample <= 8 )
    {
        eDataType = GDT_Byte;
        if( nSampleFormat == SAMPLEFORMAT_INT )
            SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );
            
    }
    else if( poDS->nBitsPerSample <= 16 )
    {
        if( nSampleFormat == SAMPLEFORMAT_INT )
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;
    }
    else if( poDS->nBitsPerSample == 32 )
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
    else if( poDS->nBitsPerSample == 64 )
    {
        if( nSampleFormat == SAMPLEFORMAT_IEEEFP )
            eDataType = GDT_Float64;
        else if( nSampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP )
            eDataType = GDT_CFloat32;
        else if( nSampleFormat == SAMPLEFORMAT_COMPLEXINT )
            eDataType = GDT_CInt32;
    }
    else if( poDS->nBitsPerSample == 128 )
    {
        if( nSampleFormat == SAMPLEFORMAT_COMPLEXIEEEFP )
            eDataType = GDT_CFloat64;
    }

/* -------------------------------------------------------------------- */
/*      Try to work out band color interpretation.                      */
/* -------------------------------------------------------------------- */
    int bLookForExtraSamples = FALSE;

    if( poDS->poColorTable != NULL && nBand == 1 ) 
        eBandInterp = GCI_PaletteIndex;
    else if( poDS->nPhotometric == PHOTOMETRIC_RGB 
             || (poDS->nPhotometric == PHOTOMETRIC_YCBCR 
                 && poDS->nCompression == COMPRESSION_JPEG 
                 && CSLTestBoolean( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                                       "YES") )) )
    {
        if( nBand == 1 )
            eBandInterp = GCI_RedBand;
        else if( nBand == 2 )
            eBandInterp = GCI_GreenBand;
        else if( nBand == 3 )
            eBandInterp = GCI_BlueBand;
        else
            bLookForExtraSamples = TRUE;
    }
    else if( poDS->nPhotometric == PHOTOMETRIC_YCBCR )
    {
        if( nBand == 1 )
            eBandInterp = GCI_YCbCr_YBand;
        else if( nBand == 2 )
            eBandInterp = GCI_YCbCr_CbBand;
        else if( nBand == 3 )
            eBandInterp = GCI_YCbCr_CrBand;
        else
            bLookForExtraSamples = TRUE;
    }
    else if( poDS->nPhotometric == PHOTOMETRIC_SEPARATED )
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
            bLookForExtraSamples = TRUE;
    }
    else if( poDS->nPhotometric == PHOTOMETRIC_MINISBLACK && nBand == 1 )
        eBandInterp = GCI_GrayIndex;
    else
        bLookForExtraSamples = TRUE;
        
    if( bLookForExtraSamples )
    {
        uint16 *v;
        uint16 count = 0;

        if( TIFFGetField( poDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            int nBaseSamples;
            nBaseSamples = poDS->nSamplesPerPixel - count;

            if( nBand > nBaseSamples 
                && (v[nBand-nBaseSamples-1] == EXTRASAMPLE_ASSOCALPHA
                    || v[nBand-nBaseSamples-1] == EXTRASAMPLE_UNASSALPHA) )
                eBandInterp = GCI_AlphaBand;
            else
                eBandInterp = GCI_Undefined;
        }
        else
            eBandInterp = GCI_Undefined;
    }

/* -------------------------------------------------------------------- */
/*	Establish block size for strip or tiles.			*/
/* -------------------------------------------------------------------- */
    nBlockXSize = poDS->nBlockXSize;
    nBlockYSize = poDS->nBlockYSize;

    bNoDataSet = FALSE;
    dfNoDataValue = -9999.0;
}

/************************************************************************/
/*                          ~GTiffRasterBand()                          */
/************************************************************************/

GTiffRasterBand::~GTiffRasterBand()
{
    /* So that any future DropReferenceVirtualMem() will not try to access the */
    /* raster band object, but this wouldn't conform the advertized contract */
    if( aSetPSelf.size() != 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Virtual memory objects still exist at GTiffRasterBand destruction");
        std::set<GTiffRasterBand**>::iterator oIter = aSetPSelf.begin();
        for(; oIter != aSetPSelf.end(); ++oIter )
            *(*oIter) = NULL;
    }
}

/************************************************************************/
/*                           DirectIO()                                 */
/************************************************************************/

/* Reads directly bytes from the file using ReadMultiRange(), and by-pass */
/* block reading. Restricted to simple TIFF configurations (un-tiled, */
/* uncompressed data, standard data types). Particularly usefull to extract */
/* sub-windows of data on a large /vsicurl dataset). */

CPLErr GTiffRasterBand::DirectIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace )
{
    if( !(eRWFlag == GF_Read &&
          poGDS->nCompression == COMPRESSION_NONE &&
          (poGDS->nPhotometric == PHOTOMETRIC_MINISBLACK ||
           poGDS->nPhotometric == PHOTOMETRIC_RGB ||
           poGDS->nPhotometric == PHOTOMETRIC_PALETTE) &&
          (poGDS->nBitsPerSample == 8 || (poGDS->nBitsPerSample == 16) ||
           poGDS->nBitsPerSample == 32 || poGDS->nBitsPerSample == 64) &&
          poGDS->nBitsPerSample == GDALGetDataTypeSize(eDataType) &&
          !TIFFIsTiled( poGDS->hTIFF )) )
    {
        return CE_Failure;
    }

    /*CPLDebug("GTiff", "DirectIO(%d,%d,%d,%d -> %dx%d)",
             nXOff, nYOff, nXSize, nYSize,
             nBufXSize, nBufYSize);*/

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 && eRWFlag == GF_Read )
    {
        int         nOverview;

        nOverview =
            GDALBandGetBestOverviewLevel(this, nXOff, nYOff, nXSize, nYSize,
                                        nBufXSize, nBufYSize);
        if (nOverview >= 0)
        {
            GDALRasterBand* poOverviewBand = GetOverview(nOverview);
            if (poOverviewBand == NULL)
                return CE_Failure;

            return poOverviewBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                            pData, nBufXSize, nBufYSize, eBufType,
                                            nPixelSpace, nLineSpace );
        }
    }

    /* Make sure that TIFFTAG_STRIPOFFSETS is up-to-date */
    if (poGDS->GetAccess() == GA_Update)
        poGDS->FlushCache();

    /* Get strip offsets */
    toff_t *panTIFFOffsets = NULL;
    if ( !TIFFGetField( poGDS->hTIFF, TIFFTAG_STRIPOFFSETS, &panTIFFOffsets ) ||
         panTIFFOffsets == NULL )
    {
        return CE_Failure;
    }

    int iLine;
    int nReqXSize = nXSize; /* sub-sampling or over-sampling can only be done at last stage */
    int nReqYSize = MIN(nBufYSize, nYSize); /* we can do sub-sampling at the extraction stage */
    void** ppData = (void**) VSIMalloc(nReqYSize * sizeof(void*));
    vsi_l_offset* panOffsets = (vsi_l_offset*)
                            VSIMalloc(nReqYSize * sizeof(vsi_l_offset));
    size_t* panSizes = (size_t*) VSIMalloc(nReqYSize * sizeof(size_t));
    int eDTSize = GDALGetDataTypeSize(eDataType) / 8;
    void* pTmpBuffer = NULL;
    CPLErr eErr = CE_None;
    int nContigBands = ((poGDS->nPlanarConfig == PLANARCONFIG_CONTIG) ? poGDS->nBands : 1);
    int ePixelSize = eDTSize * nContigBands;

    if (ppData == NULL || panOffsets == NULL || panSizes == NULL)
        eErr = CE_Failure;
    else if (nXSize != nBufXSize || nYSize != nBufYSize ||
             eBufType != eDataType ||
             nPixelSpace != GDALGetDataTypeSize(eBufType) / 8 ||
             nContigBands > 1)
    {
        /* We need a temporary buffer for over-sampling/sub-sampling */
        /* and/or data type conversion */
        pTmpBuffer = VSIMalloc(nReqXSize * nReqYSize * ePixelSize);
        if (pTmpBuffer == NULL)
            eErr = CE_Failure;
    }

    /* Prepare data extraction */
    for(iLine=0;eErr == CE_None && iLine<nReqYSize;iLine++)
    {
        if (pTmpBuffer == NULL)
            ppData[iLine] = ((GByte*)pData) + iLine * nLineSpace;
        else
            ppData[iLine] = ((GByte*)pTmpBuffer) + iLine * nReqXSize * ePixelSize;
        int nSrcLine;
        if (nBufYSize < nYSize) /* Sub-sampling in y */
            nSrcLine = nYOff + (int)((iLine + 0.5) * nYSize / nBufYSize);
        else
            nSrcLine = nYOff + iLine;

        int nBlockXOff = 0;
        int nBlockYOff = nSrcLine / nBlockYSize;
        int nYOffsetInBlock = nSrcLine % nBlockYSize;
        int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
        int nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
        if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        {
            nBlockId += (nBand-1) * poGDS->nBlocksPerBand;
        }

        panOffsets[iLine] = panTIFFOffsets[nBlockId];
        if (panOffsets[iLine] == 0) /* We don't support sparse files */
            eErr = CE_Failure;

        panOffsets[iLine] += (nXOff + nYOffsetInBlock * nBlockXSize) * ePixelSize;
        panSizes[iLine] = nReqXSize * ePixelSize;
    }

    /* Extract data from the file */
    if (eErr == CE_None)
    {
        VSILFILE* fp = (VSILFILE*) TIFFClientdata( poGDS->hTIFF );
        int nRet = VSIFReadMultiRangeL(nReqYSize, ppData, panOffsets, panSizes, fp);
        if (nRet != 0)
            eErr = CE_Failure;
    }

    /* Byte-swap if necessary */
    if (eErr == CE_None && TIFFIsByteSwapped(poGDS->hTIFF))
    {
        for(iLine=0;iLine<nReqYSize;iLine++)
        {
            GDALSwapWords( ppData[iLine], eDTSize, nReqXSize * nContigBands, eDTSize);
        }
    }

    /* Over-sampling/sub-sampling and/or data type conversion */
    if (eErr == CE_None && pTmpBuffer != NULL)
    {
        for(int iY=0;iY<nBufYSize;iY++)
        {
            int iSrcY = (nBufYSize <= nYSize) ? iY :
                            (int)((iY + 0.5) * nYSize / nBufYSize);
            if (nBufXSize == nXSize && nContigBands == 1)
            {
                GDALCopyWords( ppData[iSrcY], eDataType, eDTSize,
                                ((GByte*)pData) + iY * nLineSpace,
                                eBufType, nPixelSpace,
                                nReqXSize);
            }
            else
            {
                GByte* pabySrcData = ((GByte*)ppData[iSrcY]) +
                            ((nContigBands > 1) ? (nBand-1) : 0) * eDTSize;
                GByte* pabyDstData = ((GByte*)pData) + iY * nLineSpace;
                for(int iX=0;iX<nBufXSize;iX++)
                {
                    int iSrcX = (nBufXSize == nXSize) ? iX :
                                    (int)((iX+0.5) * nXSize / nBufXSize);
                    GDALCopyWords( pabySrcData + iSrcX * ePixelSize,
                                   eDataType, 0,
                                   pabyDstData + iX * nPixelSpace,
                                   eBufType, 0, 1);
                }
            }
        }
    }

    /* Cleanup */
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
    CPLVirtualMem* psRet;

    if( !CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "USE_DEFAULT_IMPLEMENTATION", "NO")) )
    {
        psRet = GetVirtualMemAutoInternal(eRWFlag, pnPixelSpace, pnLineSpace,
                                        papszOptions);
        if( psRet != NULL )
        {
            CPLDebug("GTiff", "GetVirtualMemAuto(): Using memory file mapping");
            return psRet;
        }
    }

    CPLDebug("GTiff", "GetVirtualMemAuto(): Defaulting to base implementation");
    return GDALRasterBand::GetVirtualMemAuto(eRWFlag, pnPixelSpace,
                                             pnLineSpace, papszOptions);
}

/************************************************************************/
/*                     DropReferenceVirtualMem()                        */
/************************************************************************/

void GTiffRasterBand::DropReferenceVirtualMem(void* pUserData)
{
    /* This function may also be called when the dataset and rasterband */
    /* objects have been destroyed */
    /* If they are still alive, it updates the reference counter of the */
    /* base mapping to invalidate the pointer to it if needed */

    GTiffRasterBand** ppoSelf = (GTiffRasterBand**) pUserData;
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
    int nLineSize = nBlockXSize * (GDALGetDataTypeSize(eDataType) / 8);
    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        nLineSize *= poGDS->nBands;

    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
    {
        /* In case of a pixel interleaved file, we save virtual memory space */
        /* by reusing a base mapping that embraces the whole imagery */
        if( poGDS->pBaseMapping != NULL )
        {
            /* Offset between the base mapping and the requested mapping */
            vsi_l_offset nOffset = (vsi_l_offset)(nBand - 1) * GDALGetDataTypeSize(eDataType) / 8;

            GTiffRasterBand** ppoSelf = (GTiffRasterBand** )CPLCalloc(1, sizeof(GTiffRasterBand*));
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

            /* Mechanism used so that the memory mapping object can be */
            /* destroyed after the raster band */
            aSetPSelf.insert(ppoSelf);
            poGDS->nRefBaseMapping ++;
            *pnPixelSpace = GDALGetDataTypeSize(eDataType) / 8;
            if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
                *pnPixelSpace *= poGDS->nBands;
            *pnLineSpace = nLineSize;
            return pVMem;
        }
    }

    VSILFILE* fp = (VSILFILE*) TIFFClientdata( poGDS->hTIFF );

    vsi_l_offset nLength = (vsi_l_offset)nRasterYSize * nLineSize;

    if( !(CPLIsVirtualMemFileMapAvailable() &&
          VSIFGetNativeFileDescriptorL(fp) != NULL &&
          nLength == (size_t)nLength &&
          poGDS->nCompression == COMPRESSION_NONE &&
          (poGDS->nBitsPerSample == 8 || poGDS->nBitsPerSample == 16 ||
           poGDS->nBitsPerSample == 32 || poGDS->nBitsPerSample == 64) &&
          poGDS->nBitsPerSample == GDALGetDataTypeSize(eDataType) &&
          !TIFFIsTiled( poGDS->hTIFF ) && !TIFFIsByteSwapped(poGDS->hTIFF)) )
    {
        return NULL;
    }

    if (!poGDS->SetDirectory())
        return NULL;

    /* Make sure that TIFFTAG_STRIPOFFSETS is up-to-date */
    if (poGDS->GetAccess() == GA_Update)
        poGDS->FlushCache();

    /* Get strip offsets */
    toff_t *panTIFFOffsets = NULL;
    if ( !TIFFGetField( poGDS->hTIFF, TIFFTAG_STRIPOFFSETS, &panTIFFOffsets ) ||
         panTIFFOffsets == NULL )
    {
        return NULL;
    }

    int nBlockSize =
        nBlockXSize * nBlockYSize * GDALGetDataTypeSize(eDataType) / 8;
    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        nBlockSize *= poGDS->nBands;

    int nBlocks = poGDS->nBlocksPerBand;
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlocks *= poGDS->nBands;
    int i;
    for(i = 0; i < nBlocks; i ++)
    {
        if( panTIFFOffsets[i] != 0 )
            break;
    }
    if( i == nBlocks )
    {
        /* All zeroes */
        if( poGDS->eAccess == GA_Update )
        {
            /* Initialize the file with empty blocks so that the file has */
            /* the appropriate size */

            toff_t* panByteCounts = NULL;
            if( !TIFFGetField( poGDS->hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts ) ||
                panByteCounts == NULL )
            {
                return NULL;
            }
            VSIFSeekL(fp, 0, SEEK_END);
            vsi_l_offset nBaseOffset = VSIFTellL(fp);

            /* Just write one tile with libtiff to put it in appropriate state */
            GByte* pabyData = (GByte*)VSICalloc(1, nBlockSize);
            if( pabyData == NULL )
            {
                return NULL;
            }
            int ret = TIFFWriteEncodedStrip(poGDS->hTIFF, 0, pabyData, nBlockSize);
            VSIFree(pabyData);
            if( ret != nBlockSize )
            {
                return NULL;
            }
            CPLAssert(panTIFFOffsets[0] == nBaseOffset);
            CPLAssert(panByteCounts[0] == (toff_t)nBlockSize);

            /* Now simulate the writing of other blocks */
            vsi_l_offset nDataSize = (vsi_l_offset)nBlockSize * nBlocks;
            VSIFSeekL(fp, nBaseOffset + nDataSize - 1, SEEK_SET);
            char ch = 0;
            if( VSIFWriteL(&ch, 1, 1, fp) != 1 )
            {
                return NULL;
            }

            for(i = 1; i < nBlocks; i ++)
            {
                panTIFFOffsets[i] = nBaseOffset + i * (toff_t)nBlockSize;
                panByteCounts[i] = nBlockSize;
            }
        }
        else
        {
            CPLDebug("GTiff", "Sparse files not supported in file mapping");
            return NULL;
        }
    }

    GIntBig nBlockSpacing = 0;
    int bCompatibleSpacing = TRUE;
    toff_t nPrevOffset = 0;
    for(i = 0; i < poGDS->nBlocksPerBand; i ++)
    {
        toff_t nCurOffset;
        if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
            nCurOffset = panTIFFOffsets[poGDS->nBlocksPerBand * (nBand - 1) + i];
        else
            nCurOffset = panTIFFOffsets[i];
        if( nCurOffset == 0 )
        {
            bCompatibleSpacing = FALSE;
            break;
        }
        if( i > 0 )
        {
            GIntBig nCurSpacing = nCurOffset - nPrevOffset;
            if( i == 1 )
            {
                if( nCurSpacing != (GIntBig)nBlockYSize * nLineSize )
                {
                    bCompatibleSpacing = FALSE;
                    break;
                }
                nBlockSpacing = nCurSpacing;
            }
            else if( nBlockSpacing != nCurSpacing )
            {
                bCompatibleSpacing = FALSE;
                break;
            }
        }
        nPrevOffset = nCurOffset;
    }

    if( !bCompatibleSpacing )
    {
        return NULL;
    }
    else
    {
        vsi_l_offset nOffset;
        if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            CPLAssert( poGDS->pBaseMapping == NULL );
            nOffset = panTIFFOffsets[0];
        }
        else
            nOffset = panTIFFOffsets[poGDS->nBlocksPerBand * (nBand - 1)];
        CPLVirtualMem* pVMem = CPLVirtualMemFileMapNew(
            fp, nOffset, nLength,
            (eRWFlag == GF_Write) ? VIRTUALMEM_READWRITE : VIRTUALMEM_READONLY,
            NULL, NULL);
        if( pVMem == NULL )
        {
            return NULL;
        }
        else
        {
            if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
            {
                poGDS->pBaseMapping = pVMem;
                pVMem = GetVirtualMemAutoInternal( eRWFlag,
                                                   pnPixelSpace,
                                                   pnLineSpace,
                                                   papszOptions );
                /* drop ref on base mapping */
                CPLVirtualMemFree(poGDS->pBaseMapping);
                if( pVMem == NULL )
                    poGDS->pBaseMapping = NULL;
            }
            else
            {
                *pnPixelSpace = GDALGetDataTypeSize(eDataType) / 8;
                if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
                    *pnPixelSpace *= poGDS->nBands;
                *pnLineSpace = nLineSize;
            }
            return pVMem;
        }
    }
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GTiffDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace)

{
    CPLErr eErr;
    /* Try to pass the request to the most appropriate overview dataset */
    if( nBufXSize < nXSize && nBufYSize < nYSize )
    {
        int nXOffMod = nXOff, nYOffMod = nYOff, nXSizeMod = nXSize, nYSizeMod = nYSize;
        nJPEGOverviewVisibilityFlag ++;
        int iOvrLevel = GDALBandGetBestOverviewLevel(papoBands[0],
                                                     nXOffMod, nYOffMod,
                                                     nXSizeMod, nYSizeMod,
                                                     nBufXSize, nBufYSize);
        nJPEGOverviewVisibilityFlag --;

        if( iOvrLevel >= 0 && papoBands[0]->GetOverview(iOvrLevel) != NULL &&
            papoBands[0]->GetOverview(iOvrLevel)->GetDataset() != NULL )
        {
            nJPEGOverviewVisibilityFlag ++;
            eErr = papoBands[0]->GetOverview(iOvrLevel)->GetDataset()->RasterIO(
                eRWFlag, nXOffMod, nYOffMod, nXSizeMod, nYSizeMod,
                pData, nBufXSize, nBufYSize, eBufType,
                nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace);
            nJPEGOverviewVisibilityFlag --;
            return eErr;
        }
    }

    nJPEGOverviewVisibilityFlag ++;
    eErr =  GDALPamDataset::IRasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                pData, nBufXSize, nBufYSize, eBufType,
                nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace);
    nJPEGOverviewVisibilityFlag --;
    return eErr;
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr GTiffRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace )
{
    CPLErr eErr;

    //CPLDebug("GTiff", "RasterIO(%d, %d, %d, %d, %d, %d)",
    //         nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize);

    if (poGDS->bDirectIO)
    {
        poGDS->nJPEGOverviewVisibilityFlag ++;
        eErr = DirectIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                        pData, nBufXSize, nBufYSize, eBufType,
                        nPixelSpace, nLineSpace);
        poGDS->nJPEGOverviewVisibilityFlag --;
        if (eErr == CE_None)
            return eErr;
    }

    if (poGDS->nBands != 1 &&
        poGDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
        eRWFlag == GF_Read &&
        nXSize == nBufXSize && nYSize == nBufYSize)
    {
        int nBlockX1 = nXOff / nBlockXSize;
        int nBlockY1 = nYOff / nBlockYSize;
        int nBlockX2 = (nXOff + nXSize - 1) / nBlockXSize;
        int nBlockY2 = (nYOff + nYSize - 1) / nBlockYSize;
        int nXBlocks = nBlockX2 - nBlockX1 + 1;
        int nYBlocks = nBlockY2 - nBlockY1 + 1;
        GIntBig nRequiredMem = (GIntBig)poGDS->nBands * nXBlocks * nYBlocks *
                                nBlockXSize * nBlockYSize *
                               (GDALGetDataTypeSize(eDataType) / 8);
        if (nRequiredMem > GDALGetCacheMax64())
        {
            if (!poGDS->bHasWarnedDisableAggressiveBandCaching)
            {
                CPLDebug("GTiff", "Disable aggressive band caching. Cache not big enough. "
                         "At least " CPL_FRMT_GIB " bytes necessary", nRequiredMem);
                poGDS->bHasWarnedDisableAggressiveBandCaching = TRUE;
            }
            poGDS->bLoadingOtherBands = TRUE;
        }
    }

    poGDS->nJPEGOverviewVisibilityFlag ++;
    eErr = GDALPamRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nPixelSpace, nLineSpace);
    poGDS->nJPEGOverviewVisibilityFlag --;

    poGDS->bLoadingOtherBands = FALSE;

    return eErr;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    int			nBlockBufSize, nBlockId, nBlockIdBand0;
    CPLErr		eErr = CE_None;

    if (!poGDS->SetDirectory())
        return CE_Failure;

    if( TIFFIsTiled(poGDS->hTIFF) )
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
    else
    {
        CPLAssert( nBlockXOff == 0 );
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
    }

    CPLAssert(nBlocksPerRow != 0);
    nBlockIdBand0 = nBlockXOff + nBlockYOff * nBlocksPerRow;
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId = nBlockIdBand0 + (nBand-1) * poGDS->nBlocksPerBand;
    else
        nBlockId = nBlockIdBand0;
        
/* -------------------------------------------------------------------- */
/*      The bottom most partial tiles and strips are sometimes only     */
/*      partially encoded.  This code reduces the requested data so     */
/*      an error won't be reported in this case. (#1179)                */
/* -------------------------------------------------------------------- */
    int nBlockReqSize = nBlockBufSize;

    if( (nBlockYOff+1) * nBlockYSize > nRasterYSize )
    {
        nBlockReqSize = (nBlockBufSize / nBlockYSize) 
            * (nBlockYSize - (((nBlockYOff+1) * nBlockYSize) % nRasterYSize));
    }

/* -------------------------------------------------------------------- */
/*      Handle the case of a strip or tile that doesn't exist yet.      */
/*      Just set to zeros and return.                                   */
/* -------------------------------------------------------------------- */
    if( !poGDS->IsBlockAvailable(nBlockId) )
    {
        NullBlock( pImage );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Handle simple case (separate, onesampleperpixel)		*/
/* -------------------------------------------------------------------- */
    if( poGDS->nBands == 1
        || poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
    {
        if( nBlockReqSize < nBlockBufSize )
            memset( pImage, 0, nBlockBufSize );

        if( TIFFIsTiled( poGDS->hTIFF ) )
        {
            if( TIFFReadEncodedTile( poGDS->hTIFF, nBlockId, pImage,
                                     nBlockReqSize ) == -1
                && !poGDS->bIgnoreReadErrors )
            {
                memset( pImage, 0, nBlockBufSize );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedTile() failed.\n" );
                
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
                        "TIFFReadEncodedStrip() failed.\n" );

                eErr = CE_Failure;
            }
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Load desired block                                              */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadBlockBuf( nBlockId );
    if( eErr != CE_None )
    {
        memset( pImage, 0,
                nBlockXSize * nBlockYSize
                * (GDALGetDataTypeSize(eDataType) / 8) );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Special case for YCbCr subsampled data.                         */
/* -------------------------------------------------------------------- */
#ifdef notdef
    if( (eBandInterp == GCI_YCbCr_YBand 
         || eBandInterp == GCI_YCbCr_CbBand
         ||  eBandInterp == GCI_YCbCr_CrBand)
        && poGDS->nBitsPerSample == 8 )
    {
	uint16 hs, vs;
        int iX, iY;

	TIFFGetFieldDefaulted( poGDS->hTIFF, TIFFTAG_YCBCRSUBSAMPLING, 
                               &hs, &vs);
        
        for( iY = 0; iY < nBlockYSize; iY++ )
        {
            for( iX = 0; iX < nBlockXSize; iX++ )
            {
                int iBlock = (iY / vs) * (nBlockXSize/hs) + (iX / hs);
                GByte *pabySrcBlock = poGDS->pabyBlockBuf + 
                    (vs * hs + 2) * iBlock;
                
                if( eBandInterp == GCI_YCbCr_YBand )
                    ((GByte *)pImage)[iY*nBlockXSize + iX] = 
                        pabySrcBlock[(iX % hs) + (iY % vs) * hs];
                else if( eBandInterp == GCI_YCbCr_CbBand )
                    ((GByte *)pImage)[iY*nBlockXSize + iX] = 
                        pabySrcBlock[vs * hs + 0];
                else if( eBandInterp == GCI_YCbCr_CrBand )
                    ((GByte *)pImage)[iY*nBlockXSize + iX] = 
                        pabySrcBlock[vs * hs + 1];
            }
        }

        return CE_None;
    }
#endif
        
/* -------------------------------------------------------------------- */
/*      Handle simple case of eight bit data, and pixel interleaving.   */
/* -------------------------------------------------------------------- */
    if( poGDS->nBitsPerSample == 8 )
    {
        int	i, nBlockPixels;
        GByte	*pabyImage;
        GByte   *pabyImageDest = (GByte*)pImage;
        int      nBands = poGDS->nBands;

        pabyImage = poGDS->pabyBlockBuf + nBand - 1;

        nBlockPixels = nBlockXSize * nBlockYSize;

/* ==================================================================== */
/*     Optimization for high number of words to transfer and some       */
/*     typical band numbers : we unroll the loop.                       */
/* ==================================================================== */
#define COPY_TO_DST_BUFFER(nBands) \
        if (nBlockPixels > 100) \
        { \
            for ( i = nBlockPixels / 16; i != 0; i -- ) \
            { \
                pabyImageDest[0] = pabyImage[0*nBands]; \
                pabyImageDest[1] = pabyImage[1*nBands]; \
                pabyImageDest[2] = pabyImage[2*nBands]; \
                pabyImageDest[3] = pabyImage[3*nBands]; \
                pabyImageDest[4] = pabyImage[4*nBands]; \
                pabyImageDest[5] = pabyImage[5*nBands]; \
                pabyImageDest[6] = pabyImage[6*nBands]; \
                pabyImageDest[7] = pabyImage[7*nBands]; \
                pabyImageDest[8] = pabyImage[8*nBands]; \
                pabyImageDest[9] = pabyImage[9*nBands]; \
                pabyImageDest[10] = pabyImage[10*nBands]; \
                pabyImageDest[11] = pabyImage[11*nBands]; \
                pabyImageDest[12] = pabyImage[12*nBands]; \
                pabyImageDest[13] = pabyImage[13*nBands]; \
                pabyImageDest[14] = pabyImage[14*nBands]; \
                pabyImageDest[15] = pabyImage[15*nBands]; \
                pabyImageDest += 16; \
                pabyImage += 16*nBands; \
            } \
            nBlockPixels = nBlockPixels % 16; \
        } \
        for( i = 0; i < nBlockPixels; i++ ) \
        { \
            pabyImageDest[i] = *pabyImage; \
            pabyImage += nBands; \
        }

        switch (nBands)
        {
            case 3:  COPY_TO_DST_BUFFER(3); break;
            case 4:  COPY_TO_DST_BUFFER(4); break;
            default:
            {
                for( i = 0; i < nBlockPixels; i++ )
                {
                    pabyImageDest[i] = *pabyImage;
                    pabyImage += nBands;
                }
            }
        }
#undef COPY_TO_DST_BUFFER
    }

    else
    {
        int	i, nBlockPixels, nWordBytes;
        GByte	*pabyImage;

        nWordBytes = poGDS->nBitsPerSample / 8;
        pabyImage = poGDS->pabyBlockBuf + (nBand - 1) * nWordBytes;

        nBlockPixels = nBlockXSize * nBlockYSize;
        for( i = 0; i < nBlockPixels; i++ )
        {
            for( int j = 0; j < nWordBytes; j++ )
            {
                ((GByte *) pImage)[i*nWordBytes + j] = pabyImage[j];
            }
            pabyImage += poGDS->nBands * nWordBytes;
        }
    }

    if (eErr == CE_None)
        eErr = FillCacheForOtherBands(nBlockXOff, nBlockYOff);

    return eErr;
}


/************************************************************************/
/*                       FillCacheForOtherBands()                       */
/************************************************************************/

CPLErr GTiffRasterBand::FillCacheForOtherBands( int nBlockXOff, int nBlockYOff )

{
    CPLErr eErr = CE_None;
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
/*      enough to accomodate the size of all the blocks, don't enter    */
/* -------------------------------------------------------------------- */
    if( poGDS->nBands != 1 && !poGDS->bLoadingOtherBands &&
        nBlockXSize * nBlockYSize * (GDALGetDataTypeSize(eDataType) / 8) < GDALGetCacheMax64() / poGDS->nBands)
    {
        int iOtherBand;

        poGDS->bLoadingOtherBands = TRUE;

        for( iOtherBand = 1; iOtherBand <= poGDS->nBands; iOtherBand++ )
        {
            if( iOtherBand == nBand )
                continue;

            GDALRasterBlock *poBlock;

            poBlock = poGDS->GetRasterBand(iOtherBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff);
            if (poBlock == NULL)
            {
                eErr = CE_Failure;
                break;
            }
            poBlock->DropLock();
        }

        poGDS->bLoadingOtherBands = FALSE;
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    int		nBlockId;
    CPLErr      eErr = CE_None;

    if (poGDS->bDebugDontWriteBlocks)
        return CE_None;

    if (poGDS->bWriteErrorInFlushBlockBuf)
    {
        /* Report as an error if a previously loaded block couldn't be */
        /* written correctly */
        poGDS->bWriteErrorInFlushBlockBuf = FALSE;
        return CE_Failure;
    }

    if (!poGDS->SetDirectory())
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
        nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
            + (nBand-1) * poGDS->nBlocksPerBand;

        eErr = poGDS->WriteEncodedTileOrStrip(nBlockId, pImage, TRUE);

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Handle case of pixel interleaved (PLANARCONFIG_CONTIG) images.  */
/* -------------------------------------------------------------------- */
    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
        
    eErr = poGDS->LoadBlockBuf( nBlockId );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      On write of pixel interleaved data, we might as well flush      */
/*      out any other bands that are dirty in our cache.  This is       */
/*      especially helpful when writing compressed blocks.              */
/* -------------------------------------------------------------------- */
    int iBand; 
    int nWordBytes = poGDS->nBitsPerSample / 8;
    int nBands = poGDS->nBands;

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        const GByte *pabyThisImage = NULL;
        GDALRasterBlock *poBlock = NULL;

        if( iBand+1 == nBand )
            pabyThisImage = (GByte *) pImage;
        else
        {
            poBlock = ((GTiffRasterBand *)poGDS->GetRasterBand( iBand+1 ))
                ->TryGetLockedBlockRef( nBlockXOff, nBlockYOff );

            if( poBlock == NULL )
                continue;

            if( !poBlock->GetDirty() )
            {
                poBlock->DropLock();
                continue;
            }

            pabyThisImage = (GByte *) poBlock->GetDataRef();
        }

        int i, nBlockPixels = nBlockXSize * nBlockYSize;
        GByte *pabyOut = poGDS->pabyBlockBuf + iBand*nWordBytes;

        if (nWordBytes == 1)
        {

/* ==================================================================== */
/*     Optimization for high number of words to transfer and some       */
/*     typical band numbers : we unroll the loop.                       */
/* ==================================================================== */
#define COPY_TO_DST_BUFFER(nBands) \
            if (nBlockPixels > 100) \
            { \
                for ( i = nBlockPixels / 16; i != 0; i -- ) \
                { \
                    pabyOut[0*nBands] = pabyThisImage[0]; \
                    pabyOut[1*nBands] = pabyThisImage[1]; \
                    pabyOut[2*nBands] = pabyThisImage[2]; \
                    pabyOut[3*nBands] = pabyThisImage[3]; \
                    pabyOut[4*nBands] = pabyThisImage[4]; \
                    pabyOut[5*nBands] = pabyThisImage[5]; \
                    pabyOut[6*nBands] = pabyThisImage[6]; \
                    pabyOut[7*nBands] = pabyThisImage[7]; \
                    pabyOut[8*nBands] = pabyThisImage[8]; \
                    pabyOut[9*nBands] = pabyThisImage[9]; \
                    pabyOut[10*nBands] = pabyThisImage[10]; \
                    pabyOut[11*nBands] = pabyThisImage[11]; \
                    pabyOut[12*nBands] = pabyThisImage[12]; \
                    pabyOut[13*nBands] = pabyThisImage[13]; \
                    pabyOut[14*nBands] = pabyThisImage[14]; \
                    pabyOut[15*nBands] = pabyThisImage[15]; \
                    pabyThisImage += 16; \
                    pabyOut += 16*nBands; \
                } \
                nBlockPixels = nBlockPixels % 16; \
            } \
            for( i = 0; i < nBlockPixels; i++ ) \
            { \
                *pabyOut = pabyThisImage[i]; \
                pabyOut += nBands; \
            }

            switch (nBands)
            {
                case 3:  COPY_TO_DST_BUFFER(3); break;
                case 4:  COPY_TO_DST_BUFFER(4); break;
                default:
                {
                    for( i = 0; i < nBlockPixels; i++ )
                    {
                        *pabyOut = pabyThisImage[i];
                        pabyOut += nBands;
                    }
                }
            }
#undef COPY_TO_DST_BUFFER
        }
        else
        {
            for( i = 0; i < nBlockPixels; i++ )
            {
                memcpy( pabyOut, pabyThisImage, nWordBytes );

                pabyOut += nWordBytes * nBands;
                pabyThisImage += nWordBytes;
            }
        }
        
        if( poBlock != NULL )
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    poGDS->bLoadedBlockDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

void GTiffRasterBand::SetDescription( const char *pszDescription )

{
    if( pszDescription == NULL )
        pszDescription = "";

    osDescription = pszDescription;
}

/************************************************************************/
/*                           GetDescription()                           */
/************************************************************************/

const char *GTiffRasterBand::GetDescription() const
{
    return osDescription;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GTiffRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bHaveOffsetScale;
    return dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetOffset( double dfNewValue )

{
    if( !bHaveOffsetScale || dfNewValue != dfOffset )
        poGDS->bMetadataChanged = TRUE;

    bHaveOffsetScale = TRUE;
    dfOffset = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GTiffRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bHaveOffsetScale;
    return dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetScale( double dfNewValue )

{
    if( !bHaveOffsetScale || dfNewValue != dfScale )
        poGDS->bMetadataChanged = TRUE;

    bHaveOffsetScale = TRUE;
    dfScale = dfNewValue;
    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char* GTiffRasterBand::GetUnitType()

{
    return osUnitType.c_str();
}

/************************************************************************/
/*                           SetUnitType()                              */
/************************************************************************/

CPLErr GTiffRasterBand::SetUnitType( const char* pszNewValue )

{
    CPLString osNewValue(pszNewValue ? pszNewValue : "");
    if( osNewValue.compare(osUnitType) != 0 )
        poGDS->bMetadataChanged = TRUE;

    osUnitType = osNewValue;
    return CE_None;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GTiffRasterBand::GetMetadataDomainList()
{
    return CSLDuplicate(oGTiffMDMD.GetDomainList());
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GTiffRasterBand::GetMetadata( const char * pszDomain )

{
    return oGTiffMDMD.GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GTiffRasterBand::SetMetadata( char ** papszMD, const char *pszDomain )

{
    if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
    {
        if( papszMD != NULL || GetMetadata(pszDomain) != NULL )
            poGDS->bMetadataChanged = TRUE;
    }

    return oGTiffMDMD.SetMetadata( papszMD, pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GTiffRasterBand::GetMetadataItem( const char * pszName, 
                                              const char * pszDomain )

{
    if( pszName != NULL && pszDomain != NULL && EQUAL(pszDomain, "TIFF") )
    {
        int nBlockXOff, nBlockYOff;

        if( EQUAL(pszName, "JPEGTABLES") )
        {
            if( !poGDS->SetDirectory() )
                return NULL;

            uint32 nJPEGTableSize = 0;
            void* pJPEGTable = NULL;
            if( TIFFGetField(poGDS->hTIFF, TIFFTAG_JPEGTABLES, &nJPEGTableSize, &pJPEGTable) != 1 ||
                pJPEGTable == NULL || (int)nJPEGTableSize <= 0 )
            {
                return NULL;
            }
            char* pszHex = CPLBinaryToHex( nJPEGTableSize, (const GByte*)pJPEGTable );
            const char* pszReturn = CPLSPrintf("%s", pszHex);
            CPLFree(pszHex);
            return pszReturn;
        }
        else if( sscanf(pszName, "BLOCK_OFFSET_%d_%d", &nBlockXOff, &nBlockYOff) == 2 )
        {
            if( !poGDS->SetDirectory() )
                return NULL;

            int nBlocksPerRow = DIV_ROUND_UP(poGDS->nRasterXSize, poGDS->nBlockXSize);
            int nBlocksPerColumn = DIV_ROUND_UP(poGDS->nRasterYSize, poGDS->nBlockYSize);
            if( nBlockXOff < 0 || nBlockXOff >= nBlocksPerRow ||
                nBlockYOff < 0 || nBlockYOff >= nBlocksPerColumn )
                return NULL;

            int nBlockId = nBlockYOff * nBlocksPerRow + nBlockXOff;
            if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
            {
                nBlockId += (nBand-1) * poGDS->nBlocksPerBand;
            }

            if( !poGDS->IsBlockAvailable(nBlockId) )
            {
                return NULL;
            }

            toff_t *panOffsets = NULL;
            TIFF* hTIFF = poGDS->hTIFF;
            if( (( TIFFIsTiled( hTIFF ) 
                && TIFFGetField( hTIFF, TIFFTAG_TILEOFFSETS, &panOffsets ) )
                || ( !TIFFIsTiled( hTIFF ) 
                && TIFFGetField( hTIFF, TIFFTAG_STRIPOFFSETS, &panOffsets ) )) &&
                panOffsets != NULL )
            {
                return CPLSPrintf(CPL_FRMT_GUIB, (GUIntBig)panOffsets[nBlockId]);
            }
            else
            {
                return NULL;
            }
        }
        else if( sscanf(pszName, "BLOCK_SIZE_%d_%d", &nBlockXOff, &nBlockYOff) == 2 )
        {
            if( !poGDS->SetDirectory() )
                return NULL;

            int nBlocksPerRow = DIV_ROUND_UP(poGDS->nRasterXSize, poGDS->nBlockXSize);
            int nBlocksPerColumn = DIV_ROUND_UP(poGDS->nRasterYSize, poGDS->nBlockYSize);
            if( nBlockXOff < 0 || nBlockXOff >= nBlocksPerRow ||
                nBlockYOff < 0 || nBlockYOff >= nBlocksPerColumn )
                return NULL;

            int nBlockId = nBlockYOff * nBlocksPerRow + nBlockXOff;
            if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
            {
                nBlockId += (nBand-1) * poGDS->nBlocksPerBand;
            }

            if( !poGDS->IsBlockAvailable(nBlockId) )
            {
                return NULL;
            }

            toff_t *panByteCounts = NULL;
            TIFF* hTIFF = poGDS->hTIFF;
            if( (( TIFFIsTiled( hTIFF ) 
                && TIFFGetField( hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts ) )
                || ( !TIFFIsTiled( hTIFF ) 
                && TIFFGetField( hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts ) )) &&
                panByteCounts != NULL )
            {
                return CPLSPrintf(CPL_FRMT_GUIB, (GUIntBig)panByteCounts[nBlockId]);
            }
            else
            {
                return NULL;
            }
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
    if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
        poGDS->bMetadataChanged = TRUE;

    return oGTiffMDMD.SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRasterBand::GetColorInterpretation()

{
    return eBandInterp;
}

/************************************************************************/
/*                         GTiffGetAlphaValue()                         */
/************************************************************************/

 /* Note: was EXTRASAMPLE_ASSOCALPHA in GDAL < 1.10 */
#define DEFAULT_ALPHA_TYPE              EXTRASAMPLE_UNASSALPHA

static uint16 GTiffGetAlphaValue(const char* pszValue, uint16 nDefault)
{
    if (pszValue == NULL)
        return nDefault;
    else if (EQUAL(pszValue, "YES"))
        return DEFAULT_ALPHA_TYPE;
    else if (EQUAL(pszValue, "PREMULTIPLIED"))
        return EXTRASAMPLE_ASSOCALPHA;
    else if (EQUAL(pszValue, "NON-PREMULTIPLIED"))
        return EXTRASAMPLE_UNASSALPHA;
    else if (EQUAL(pszValue, "NO") ||
             EQUAL(pszValue, "UNSPECIFIED"))
        return EXTRASAMPLE_UNSPECIFIED;
    else
        return nDefault;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr GTiffRasterBand::SetColorInterpretation( GDALColorInterp eInterp )

{
    if( eInterp == eBandInterp )
        return CE_None;

    eBandInterp = eInterp;

    if( poGDS->bCrystalized )
    {
        CPLDebug("GTIFF", "ColorInterpretation %s for band %d goes to PAM instead of TIFF tag",
                 GDALGetColorInterpretationName(eInterp), nBand);
        return GDALPamRasterBand::SetColorInterpretation( eInterp );
    }

    /* greyscale + alpha */
    else if( eInterp == GCI_AlphaBand 
        && nBand == 2 
        && poGDS->nSamplesPerPixel == 2 
        && poGDS->nPhotometric == PHOTOMETRIC_MINISBLACK )
    {
        uint16 v[1];
        v[0] = GTiffGetAlphaValue(CPLGetConfigOption("GTIFF_ALPHA", NULL),
                                  DEFAULT_ALPHA_TYPE);

        TIFFSetField(poGDS->hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
        return CE_None;
    }

    /* RGB + alpha */
    else if( eInterp == GCI_AlphaBand 
             && nBand == 4 
             && poGDS->nSamplesPerPixel == 4
             && poGDS->nPhotometric == PHOTOMETRIC_RGB )
    {
        uint16 v[1];
        v[0] = GTiffGetAlphaValue(CPLGetConfigOption("GTIFF_ALPHA", NULL),
                                  DEFAULT_ALPHA_TYPE);

        TIFFSetField(poGDS->hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
        return CE_None;
    }
    
    else
    {
        /* Try to autoset TIFFTAG_PHOTOMETRIC = PHOTOMETRIC_RGB if possible */
        if( poGDS->nCompression != COMPRESSION_JPEG &&
            poGDS->nSetPhotometricFromBandColorInterp >= 0 &&
            CSLFetchNameValue( poGDS->papszCreationOptions, "PHOTOMETRIC") == NULL &&
            (poGDS->nBands == 3 || poGDS->nBands == 4) &&
            ((nBand == 1 && eInterp == GCI_RedBand) ||
             (nBand == 2 && eInterp == GCI_GreenBand) ||
             (nBand == 3 && eInterp == GCI_BlueBand) ||
             (nBand == 4 && eInterp == GCI_AlphaBand)) )
        {
            poGDS->nSetPhotometricFromBandColorInterp ++;
            if( poGDS->nSetPhotometricFromBandColorInterp == poGDS->nBands )
            {
                poGDS->nPhotometric = PHOTOMETRIC_RGB;
                TIFFSetField(poGDS->hTIFF, TIFFTAG_PHOTOMETRIC, poGDS->nPhotometric);
                if( poGDS->nSetPhotometricFromBandColorInterp == 4 )
                {
                    uint16 v[1];
                    v[0] = GTiffGetAlphaValue(CPLGetConfigOption("GTIFF_ALPHA", NULL),
                                            DEFAULT_ALPHA_TYPE);

                    TIFFSetField(poGDS->hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
                }
            }
            return CE_None;
        }
        else
        {
            if( poGDS->nPhotometric != PHOTOMETRIC_MINISBLACK &&
                CSLFetchNameValue( poGDS->papszCreationOptions, "PHOTOMETRIC") == NULL )
            {
                poGDS->nPhotometric = PHOTOMETRIC_MINISBLACK;
                TIFFSetField(poGDS->hTIFF, TIFFTAG_PHOTOMETRIC, poGDS->nPhotometric);
            }
            if( poGDS->nSetPhotometricFromBandColorInterp > 0 )
            {
                for(int i=1;i<=poGDS->nBands;i++)
                {
                    if( i != nBand )
                    {
                        ((GDALPamRasterBand*)poGDS->GetRasterBand(i))->GDALPamRasterBand::SetColorInterpretation(
                            poGDS->GetRasterBand(i)->GetColorInterpretation() );
                        CPLDebug("GTIFF", "ColorInterpretation %s for band %d goes to PAM instead of TIFF tag",
                                 GDALGetColorInterpretationName(poGDS->GetRasterBand(i)->GetColorInterpretation()), i);
                    }
                }
            }
            poGDS->nSetPhotometricFromBandColorInterp = -1;
            CPLDebug("GTIFF", "ColorInterpretation %s for band %d goes to PAM instead of TIFF tag",
                     GDALGetColorInterpretationName(eInterp), nBand);
            return GDALPamRasterBand::SetColorInterpretation( eInterp );
        }
    }
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffRasterBand::GetColorTable()

{
    if( nBand == 1 )
        return poGDS->poColorTable;
    else
        return NULL;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr GTiffRasterBand::SetColorTable( GDALColorTable * poCT )

{
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
                  "SetColorTable() not supported for multi-sample TIFF files." );
        return CE_Failure;
    }
        
    if( eDataType != GDT_Byte && eDataType != GDT_UInt16 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "SetColorTable() only supported for Byte or UInt16 bands in TIFF format." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      We are careful about calling SetDirectory() to avoid            */
/*      prematurely crystalizing the directory.  (#2820)                */
/* -------------------------------------------------------------------- */
    if( poGDS->bCrystalized )
    {
        if (!poGDS->SetDirectory())
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
        CPLDebug( "GTiff", 
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
    int nColors;

    if( eDataType == GDT_Byte )
        nColors = 256;
    else
        nColors = 65536;

    unsigned short *panTRed, *panTGreen, *panTBlue;

    panTRed = (unsigned short *) CPLMalloc(sizeof(unsigned short)*nColors);
    panTGreen = (unsigned short *) CPLMalloc(sizeof(unsigned short)*nColors);
    panTBlue = (unsigned short *) CPLMalloc(sizeof(unsigned short)*nColors);

    for( int iColor = 0; iColor < nColors; iColor++ )
    {
        if( iColor < poCT->GetColorEntryCount() )
        {
            GDALColorEntry  sRGB;
            
            poCT->GetColorEntryAsRGB( iColor, &sRGB );
            
            panTRed[iColor] = (unsigned short) (257 * sRGB.c1);
            panTGreen[iColor] = (unsigned short) (257 * sRGB.c2);
            panTBlue[iColor] = (unsigned short) (257 * sRGB.c3);
        }
        else
        {
            panTRed[iColor] = panTGreen[iColor] = panTBlue[iColor] = 0;
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

    /* libtiff 3.X needs setting this in all cases (creation or update) */
    /* whereas libtiff 4.X would just need it if there */
    /* was no color table before */
#if 0
    else
#endif
        poGDS->bNeedsRewrite = TRUE;

    poGDS->poColorTable = poCT->Clone();
    eBandInterp = GCI_PaletteIndex;

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GTiffRasterBand::GetNoDataValue( int * pbSuccess )

{
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
    if( poGDS->bNoDataSet && poGDS->dfNoDataValue == dfNoData )
        return CE_None;

    if (!poGDS->SetDirectory())  // needed to call TIFFSetField().
        return CE_Failure;

    poGDS->bNoDataSet = TRUE;
    poGDS->dfNoDataValue = dfNoData;

    poGDS->WriteNoDataValue( poGDS->hTIFF, dfNoData );
    poGDS->bNeedsRewrite = TRUE;

    bNoDataSet = TRUE;
    dfNoDataValue = dfNoData;
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
    int nWords = nBlockXSize * nBlockYSize;
    int nChunkSize = MAX(1,GDALGetDataTypeSize(eDataType)/8);

    int bNoDataSet;
    double dfNoData = GetNoDataValue( &bNoDataSet );
    if( !bNoDataSet )
    {
#ifdef ESRI_BUILD
        if ( poGDS->nBitsPerSample >= 2 )
            memset( pData, 0, nWords*nChunkSize );
        else
            memset( pData, 1, nWords*nChunkSize );
#else
        memset( pData, 0, nWords*nChunkSize );
#endif
    }
    else
    {
        /* Will convert nodata value to the right type and copy efficiently */
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
    else
    {
        int nOverviewCount = GDALRasterBand::GetOverviewCount();
        if( nOverviewCount > 0 )
            return nOverviewCount;

        /* Implict JPEG overviews are normally hidden, except when doing */
        /* IRasterIO() operations */
        if( poGDS->nJPEGOverviewVisibilityFlag )
            return poGDS->GetJPEGOverviewCount();
        else
            return 0;
    }
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetOverview( int i )

{
    poGDS->ScanDirectories();

    if( poGDS->nOverviewCount > 0 )
    {
        /* Do we have internal overviews ? */
        if( i < 0 || i >= poGDS->nOverviewCount )
            return NULL;
        else
            return poGDS->papoOverviewDS[i]->GetRasterBand(nBand);
    }
    else
    {
        GDALRasterBand* poOvrBand = GDALRasterBand::GetOverview( i );
        if( poOvrBand != NULL )
            return poOvrBand;

        /* For consistency with GetOverviewCount(), we should also test */
        /* nJPEGOverviewVisibilityFlag, but it is also convenient to be able */
        /* to query them for testing purposes. */
        if( i >= 0 && i < poGDS->GetJPEGOverviewCount() )
            return poGDS->papoJPEGOverviewDS[i]->GetRasterBand(nBand);
        else
            return NULL;
    }
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
        else
        {
            return 0;
        }
    }
    else
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
        if( poGDS->poMaskDS->GetRasterCount() == 1)
            return poGDS->poMaskDS->GetRasterBand(1);
        else
            return poGDS->poMaskDS->GetRasterBand(nBand);
    }
    else
        return GDALPamRasterBand::GetMaskBand();
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffSplitBand                            */
/* ==================================================================== */
/************************************************************************/

class GTiffSplitBand : public GTiffRasterBand
{
    friend class GTiffDataset;

  public:

                   GTiffSplitBand( GTiffDataset *, int );
    virtual       ~GTiffSplitBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
};

/************************************************************************/
/*                           GTiffSplitBand()                           */
/************************************************************************/

GTiffSplitBand::GTiffSplitBand( GTiffDataset *poDS, int nBand )
        : GTiffRasterBand( poDS, nBand )

{
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                          ~GTiffSplitBand()                          */
/************************************************************************/

GTiffSplitBand::~GTiffSplitBand()
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    (void) nBlockXOff;
    
    /* Optimization when reading the same line in a contig multi-band TIFF */
    if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG && poGDS->nBands > 1 &&
        poGDS->nLastLineRead == nBlockYOff )
    {
        goto extract_band_data;
    }

    if (!poGDS->SetDirectory())
        return CE_Failure;
        
    if (poGDS->nPlanarConfig == PLANARCONFIG_CONTIG &&
        poGDS->nBands > 1)
    {
        if (poGDS->pabyBlockBuf == NULL)
        {
            poGDS->pabyBlockBuf = (GByte *) VSIMalloc(TIFFScanlineSize(poGDS->hTIFF));
            if( poGDS->pabyBlockBuf == NULL )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate " CPL_FRMT_GUIB " bytes.",
                         (GUIntBig)TIFFScanlineSize(poGDS->hTIFF));
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
        /* If we change of band, we must start reading the */
        /* new strip from its beginning */
        if ( poGDS->nLastBandRead != nBand )
            poGDS->nLastLineRead = -1;
        poGDS->nLastBandRead = nBand;
    }
    
    while( poGDS->nLastLineRead < nBlockYOff )
    {
        if( TIFFReadScanline( poGDS->hTIFF,
                              poGDS->pabyBlockBuf ? poGDS->pabyBlockBuf : pImage,
                              ++poGDS->nLastLineRead,
                              (poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE) ? (uint16) (nBand-1) : 0 ) == -1
            && !poGDS->bIgnoreReadErrors )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadScanline() failed." );
            return CE_Failure;
        }
    }
    
extract_band_data:
/* -------------------------------------------------------------------- */
/*      Extract band data from contig buffer.                           */
/* -------------------------------------------------------------------- */
    if ( poGDS->pabyBlockBuf != NULL )
    {
        int	  iPixel, iSrcOffset= nBand - 1, iDstOffset=0;

        for( iPixel = 0; iPixel < nBlockXSize; iPixel++, iSrcOffset+=poGDS->nBands, iDstOffset++ )
        {
            ((GByte *) pImage)[iDstOffset] = poGDS->pabyBlockBuf[iSrcOffset];
        }
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    (void) nBlockXOff;
    (void) nBlockYOff;
    (void) pImage;

    CPLError( CE_Failure, CPLE_AppDefined, 
              "Split bands are read-only." );
    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffRGBABand                            */
/* ==================================================================== */
/************************************************************************/

class GTiffRGBABand : public GTiffRasterBand
{
    friend class GTiffDataset;

  public:

                   GTiffRGBABand( GTiffDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                           GTiffRGBABand()                            */
/************************************************************************/

GTiffRGBABand::GTiffRGBABand( GTiffDataset *poDS, int nBand )
        : GTiffRasterBand( poDS, nBand )

{
    eDataType = GDT_Byte;
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
    int			nBlockBufSize, nBlockId;
    CPLErr		eErr = CE_None;

    if (!poGDS->SetDirectory())
        return CE_Failure;

    CPLAssert(nBlocksPerRow != 0);
    nBlockBufSize = 4 * nBlockXSize * nBlockYSize;
    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( poGDS->pabyBlockBuf == NULL )
    {
        poGDS->pabyBlockBuf = (GByte *) VSIMalloc3( 4, nBlockXSize, nBlockYSize );
        if( poGDS->pabyBlockBuf == NULL )
            return( CE_Failure );
    }
    
/* -------------------------------------------------------------------- */
/*      Read the strip                                                  */
/* -------------------------------------------------------------------- */
    if( poGDS->nLoadedBlock != nBlockId )
    {
        if( TIFFIsTiled( poGDS->hTIFF ) )
        {
            if( TIFFReadRGBATile(poGDS->hTIFF, 
                                 nBlockXOff * nBlockXSize, 
                                 nBlockYOff * nBlockYSize,
                                 (uint32 *) poGDS->pabyBlockBuf) == -1
                && !poGDS->bIgnoreReadErrors )
            {
                /* Once TIFFError() is properly hooked, this can go away */
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadRGBATile() failed." );
                
                memset( poGDS->pabyBlockBuf, 0, nBlockBufSize );
                
                eErr = CE_Failure;
            }
        }
        else
        {
            if( TIFFReadRGBAStrip(poGDS->hTIFF, 
                                  nBlockId * nBlockYSize,
                                  (uint32 *) poGDS->pabyBlockBuf) == -1
                && !poGDS->bIgnoreReadErrors )
            {
                /* Once TIFFError() is properly hooked, this can go away */
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
    int   iDestLine, nBO;
    int   nThisBlockYSize;

    if( (nBlockYOff+1) * nBlockYSize > GetYSize()
        && !TIFFIsTiled( poGDS->hTIFF ) )
        nThisBlockYSize = GetYSize() - nBlockYOff * nBlockYSize;
    else
        nThisBlockYSize = nBlockYSize;

#ifdef CPL_LSB
    nBO = nBand - 1;
#else
    nBO = 4 - nBand;
#endif

    for( iDestLine = 0; iDestLine < nThisBlockYSize; iDestLine++ )
    {
        int	nSrcOffset;

        nSrcOffset = (nThisBlockYSize - iDestLine - 1) * nBlockXSize * 4;

        GDALCopyWords( poGDS->pabyBlockBuf + nBO + nSrcOffset, GDT_Byte, 4,
                       ((GByte *) pImage)+iDestLine*nBlockXSize, GDT_Byte, 1, 
                       nBlockXSize );
    }

    if (eErr == CE_None)
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
    else if( nBand == 2 )
        return GCI_GreenBand;
    else if( nBand == 3 )
        return GCI_BlueBand;
    else
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
    virtual       ~GTiffOddBitsBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
};


/************************************************************************/
/*                           GTiffOddBitsBand()                         */
/************************************************************************/

GTiffOddBitsBand::GTiffOddBitsBand( GTiffDataset *poGDS, int nBand )
        : GTiffRasterBand( poGDS, nBand )

{
    eDataType = GDT_Byte;
    if( poGDS->nSampleFormat == SAMPLEFORMAT_IEEEFP )
        eDataType = GDT_Float32;
    else if( poGDS->nBitsPerSample > 8 && poGDS->nBitsPerSample < 16 )
        eDataType = GDT_UInt16;
    else if( poGDS->nBitsPerSample > 16 )
        eDataType = GDT_UInt32;
}

/************************************************************************/
/*                          ~GTiffOddBitsBand()                          */
/************************************************************************/

GTiffOddBitsBand::~GTiffOddBitsBand()

{
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffOddBitsBand::IWriteBlock( int nBlockXOff, int nBlockYOff, 
                                      void *pImage )

{
    int		nBlockId;
    CPLErr      eErr = CE_None;

    if (poGDS->bWriteErrorInFlushBlockBuf)
    {
        /* Report as an error if a previously loaded block couldn't be */
        /* written correctly */
        poGDS->bWriteErrorInFlushBlockBuf = FALSE;
        return CE_Failure;
    }

    if (!poGDS->SetDirectory())
        return CE_Failure;

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    if( eDataType == GDT_Float32 && poGDS->nBitsPerSample < 32 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Writing float data with nBitsPerSample < 32 is unsupported");
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    CPLAssert(nBlocksPerRow != 0);
    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId += (nBand-1) * poGDS->nBlocksPerBand;

    /* Only read content from disk in the CONTIG case */
    eErr = poGDS->LoadBlockBuf( nBlockId, 
                                poGDS->nPlanarConfig == PLANARCONFIG_CONTIG && poGDS->nBands > 1 );
    if( eErr != CE_None )
        return eErr;

    GUInt32 nMaxVal = (1 << poGDS->nBitsPerSample) - 1;

/* -------------------------------------------------------------------- */
/*      Handle case of "separate" images or single band images where    */
/*      no interleaving with other data is required.                    */
/* -------------------------------------------------------------------- */
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE
        || poGDS->nBands == 1 )
    {
        int	iBit, iPixel, iBitOffset = 0;
        int     iX, iY, nBitsPerLine;

        // bits per line rounds up to next byte boundary.
        nBitsPerLine = nBlockXSize * poGDS->nBitsPerSample;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        /* Initialize to zero as we set the buffer with binary or operations */
        if (poGDS->nBitsPerSample != 24)
            memset(poGDS->pabyBlockBuf, 0, (nBitsPerLine / 8) * nBlockYSize);

        iPixel = 0;
        for( iY = 0; iY < nBlockYSize; iY++ )
        {
            iBitOffset = iY * nBitsPerLine;

            /* Small optimization in 1 bit case */
            if (poGDS->nBitsPerSample == 1)
            {
                for( iX = 0; iX < nBlockXSize; iX++ )
                {
                    if (((GByte *) pImage)[iPixel++])
                        poGDS->pabyBlockBuf[iBitOffset>>3] |= (0x80 >>(iBitOffset & 7));
                    iBitOffset++;
                }

                continue;
            }

            for( iX = 0; iX < nBlockXSize; iX++ )
            {
                GUInt32  nInWord = 0;
                if( eDataType == GDT_Byte )
                    nInWord = ((GByte *) pImage)[iPixel++];
                else if( eDataType == GDT_UInt16 )
                    nInWord = ((GUInt16 *) pImage)[iPixel++];
                else if( eDataType == GDT_UInt32 )
                    nInWord = ((GUInt32 *) pImage)[iPixel++];
                else {
                    CPLAssert(0);
                }

                if (nInWord > nMaxVal)
                {
                    nInWord = nMaxVal;
                    if( !poGDS->bClipWarn )
                    {
                        poGDS->bClipWarn = TRUE;
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "One or more pixels clipped to fit %d bit domain.", poGDS->nBitsPerSample );
                    }
                }

                if (poGDS->nBitsPerSample == 24)
                {
/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugg (#2361).              */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 0] = 
                        (GByte) nInWord;
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 1] = 
                        (GByte) (nInWord >> 8);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 2] = 
                        (GByte) (nInWord >> 16);
#else
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 0] = 
                        (GByte) (nInWord >> 16);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 1] = 
                        (GByte) (nInWord >> 8);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 2] = 
                        (GByte) nInWord;
#endif
                    iBitOffset += 24;
                }
                else
                {
                    for( iBit = 0; iBit < poGDS->nBitsPerSample; iBit++ )
                    {
                        if (nInWord & (1 << (poGDS->nBitsPerSample - 1 - iBit)))
                            poGDS->pabyBlockBuf[iBitOffset>>3] |= (0x80 >>(iBitOffset & 7));
                        iBitOffset++;
                    }
                } 
            }
        }

        poGDS->bLoadedBlockDirty = TRUE;

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Handle case of pixel interleaved (PLANARCONFIG_CONTIG) images.  */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      On write of pixel interleaved data, we might as well flush      */
/*      out any other bands that are dirty in our cache.  This is       */
/*      especially helpful when writing compressed blocks.              */
/* -------------------------------------------------------------------- */
    int iBand; 

    for( iBand = 0; iBand < poGDS->nBands; iBand++ )
    {
        const GByte *pabyThisImage = NULL;
        GDALRasterBlock *poBlock = NULL;
        int	iBit, iPixel, iBitOffset = 0;
        int     iPixelBitSkip, iBandBitOffset, iX, iY, nBitsPerLine;

        if( iBand+1 == nBand )
            pabyThisImage = (GByte *) pImage;
        else
        {
            poBlock = ((GTiffOddBitsBand *)poGDS->GetRasterBand( iBand+1 ))
                ->TryGetLockedBlockRef( nBlockXOff, nBlockYOff );

            if( poBlock == NULL )
                continue;

            if( !poBlock->GetDirty() )
            {
                poBlock->DropLock();
                continue;
            }

            pabyThisImage = (GByte *) poBlock->GetDataRef();
        }

        iPixelBitSkip = poGDS->nBitsPerSample * poGDS->nBands;
        iBandBitOffset = iBand * poGDS->nBitsPerSample;

        // bits per line rounds up to next byte boundary.
        nBitsPerLine = nBlockXSize * iPixelBitSkip;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        iPixel = 0;
        for( iY = 0; iY < nBlockYSize; iY++ )
        {
            iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for( iX = 0; iX < nBlockXSize; iX++ )
            {
                GUInt32  nInWord = 0;
                if( eDataType == GDT_Byte )
                    nInWord = ((GByte *) pabyThisImage)[iPixel++];
                else if( eDataType == GDT_UInt16 )
                    nInWord = ((GUInt16 *) pabyThisImage)[iPixel++];
                else if( eDataType == GDT_UInt32 )
                    nInWord = ((GUInt32 *) pabyThisImage)[iPixel++];
                else {
                    CPLAssert(0);
                }

                if (nInWord > nMaxVal)
                {
                    nInWord = nMaxVal;
                    if( !poGDS->bClipWarn )
                    {
                        poGDS->bClipWarn = TRUE;
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "One or more pixels clipped to fit %d bit domain.", poGDS->nBitsPerSample );
                    }
                }

                if (poGDS->nBitsPerSample == 24)
                {
/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugg (#2361).              */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 0] = 
                        (GByte) nInWord;
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 1] = 
                        (GByte) (nInWord >> 8);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 2] = 
                        (GByte) (nInWord >> 16);
#else
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 0] = 
                        (GByte) (nInWord >> 16);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 1] = 
                        (GByte) (nInWord >> 8);
                    poGDS->pabyBlockBuf[(iBitOffset>>3) + 2] = 
                        (GByte) nInWord;
#endif
                    iBitOffset += 24;
                }
                else
                {
                    for( iBit = 0; iBit < poGDS->nBitsPerSample; iBit++ )
                    {
                        if (nInWord & (1 << (poGDS->nBitsPerSample - 1 - iBit)))
                            poGDS->pabyBlockBuf[iBitOffset>>3] |= (0x80 >>(iBitOffset & 7));
                        else
                        {
                            /* We must explictly unset the bit as we may update an existing block */
                            poGDS->pabyBlockBuf[iBitOffset>>3] &= ~(0x80 >>(iBitOffset & 7));
                        }

                        iBitOffset++;
                    }
                } 

                iBitOffset= iBitOffset + iPixelBitSkip - poGDS->nBitsPerSample;
            }
        }

        if( poBlock != NULL )
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    poGDS->bLoadedBlockDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffOddBitsBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    int			nBlockId;
    CPLErr		eErr = CE_None;

    if (!poGDS->SetDirectory())
        return CE_Failure;

    CPLAssert(nBlocksPerRow != 0);
    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId += (nBand-1) * poGDS->nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*	Handle the case of a strip in a writable file that doesn't	*/
/*	exist yet, but that we want to read.  Just set to zeros and	*/
/*	return.								*/
/* -------------------------------------------------------------------- */
    if( !poGDS->IsBlockAvailable(nBlockId) )
    {
        NullBlock( pImage );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadBlockBuf( nBlockId );
    if( eErr != CE_None )
        return eErr;

    if (  poGDS->nBitsPerSample == 1 && (poGDS->nBands == 1 || poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE ) )
    {
/* -------------------------------------------------------------------- */
/*      Translate 1bit data to eight bit.                               */
/* -------------------------------------------------------------------- */
        int	  iDstOffset=0, iLine;
        register GByte *pabyBlockBuf = poGDS->pabyBlockBuf;

        for( iLine = 0; iLine < nBlockYSize; iLine++ )
        {
            int iSrcOffset, iPixel;

            iSrcOffset = ((nBlockXSize+7) >> 3) * 8 * iLine;
            
            GByte bSetVal = (poGDS->bPromoteTo8Bits) ? 255 : 1;

            for( iPixel = 0; iPixel < nBlockXSize; iPixel++, iSrcOffset++ )
            {
                if( pabyBlockBuf[iSrcOffset >>3] & (0x80 >> (iSrcOffset & 0x7)) )
                    ((GByte *) pImage)[iDstOffset++] = bSetVal;
                else
                    ((GByte *) pImage)[iDstOffset++] = 0;
            }
        }
    }
/* -------------------------------------------------------------------- */
/*      Handle the case of 16- and 24-bit floating point data as per    */
/*      TIFF Technical Note 3.                                          */
/* -------------------------------------------------------------------- */
    else if( eDataType == GDT_Float32 && poGDS->nBitsPerSample < 32 )
    {
        int	i, nBlockPixels, nWordBytes, iSkipBytes;
        GByte	*pabyImage;

        nWordBytes = poGDS->nBitsPerSample / 8;
        pabyImage = poGDS->pabyBlockBuf + (nBand - 1) * nWordBytes;
        iSkipBytes = ( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE ) ?
            nWordBytes : poGDS->nBands * nWordBytes;

        nBlockPixels = nBlockXSize * nBlockYSize;
        if ( poGDS->nBitsPerSample == 16 )
        {
            for( i = 0; i < nBlockPixels; i++ )
            {
                ((GUInt32 *) pImage)[i] =
                    HalfToFloat( *((GUInt16 *)pabyImage) );
                pabyImage += iSkipBytes;
            }
        }
        else if ( poGDS->nBitsPerSample == 24 )
        {
            for( i = 0; i < nBlockPixels; i++ )
            {
#ifdef CPL_MSB
                ((GUInt32 *) pImage)[i] =
                    TripleToFloat( ((GUInt32)*(pabyImage + 0) << 16)
                                   | ((GUInt32)*(pabyImage + 1) << 8)
                                   | (GUInt32)*(pabyImage + 2) );
#else
                ((GUInt32 *) pImage)[i] =
                    TripleToFloat( ((GUInt32)*(pabyImage + 2) << 16)
                                   | ((GUInt32)*(pabyImage + 1) << 8)
                                   | (GUInt32)*pabyImage );
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
        int	iPixel, iBitOffset = 0;
        int     iPixelBitSkip, iBandBitOffset, iX, iY, nBitsPerLine;

        if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelBitSkip = poGDS->nBands * poGDS->nBitsPerSample;
            iBandBitOffset = (nBand-1) * poGDS->nBitsPerSample;
        }
        else
        {
            iPixelBitSkip = poGDS->nBitsPerSample;
            iBandBitOffset = 0;
        }

        // bits per line rounds up to next byte boundary.
        nBitsPerLine = nBlockXSize * iPixelBitSkip;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        iPixel = 0;
        for( iY = 0; iY < nBlockYSize; iY++ )
        {
            iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for( iX = 0; iX < nBlockXSize; iX++ )
            {
                int iByte = iBitOffset>>3;

                if( (iBitOffset & 0x7) == 0 )
                {
                    /* starting on byte boundary */
                    
                    ((GUInt16 *) pImage)[iPixel++] = 
                        (poGDS->pabyBlockBuf[iByte] << 4)
                        | (poGDS->pabyBlockBuf[iByte+1] >> 4);
                }
                else
                {
                    /* starting off byte boundary */
                    
                    ((GUInt16 *) pImage)[iPixel++] = 
                        ((poGDS->pabyBlockBuf[iByte] & 0xf) << 8)
                        | (poGDS->pabyBlockBuf[iByte+1]);
                }
                iBitOffset += iPixelBitSkip;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugg (#2361).              */
/* -------------------------------------------------------------------- */
    else if( poGDS->nBitsPerSample == 24 )
    {
        int	iPixel;
        int     iPixelByteSkip, iBandByteOffset, iX, iY, nBytesPerLine;

        if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelByteSkip = (poGDS->nBands * poGDS->nBitsPerSample) / 8;
            iBandByteOffset = ((nBand-1) * poGDS->nBitsPerSample) / 8;
        }
        else
        {
            iPixelByteSkip = poGDS->nBitsPerSample / 8;
            iBandByteOffset = 0;
        }

        nBytesPerLine = nBlockXSize * iPixelByteSkip;

        iPixel = 0;
        for( iY = 0; iY < nBlockYSize; iY++ )
        {
            GByte *pabyImage = 
                poGDS->pabyBlockBuf + iBandByteOffset + iY * nBytesPerLine;

            for( iX = 0; iX < nBlockXSize; iX++ )
            {
#ifdef CPL_MSB
                ((GUInt32 *) pImage)[iPixel++] = 
                    ((GUInt32)*(pabyImage + 2) << 16)
                    | ((GUInt32)*(pabyImage + 1) << 8)
                    | (GUInt32)*(pabyImage + 0);
#else
                ((GUInt32 *) pImage)[iPixel++] = 
                    ((GUInt32)*(pabyImage + 0) << 16)
                    | ((GUInt32)*(pabyImage + 1) << 8)
                    | (GUInt32)*(pabyImage + 2);
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
        int	iBit, iPixel, iBitOffset = 0;
        int     iPixelBitSkip, iBandBitOffset, iX, iY, nBitsPerLine;

        if( poGDS->nPlanarConfig == PLANARCONFIG_CONTIG )
        {
            iPixelBitSkip = poGDS->nBands * poGDS->nBitsPerSample;
            iBandBitOffset = (nBand-1) * poGDS->nBitsPerSample;
        }
        else
        {
            iPixelBitSkip = poGDS->nBitsPerSample;
            iBandBitOffset = 0;
        }

        // bits per line rounds up to next byte boundary.
        nBitsPerLine = nBlockXSize * iPixelBitSkip;
        if( (nBitsPerLine & 7) != 0 )
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        register GByte *pabyBlockBuf = poGDS->pabyBlockBuf;
        iPixel = 0;

        for( iY = 0; iY < nBlockYSize; iY++ )
        {
            iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for( iX = 0; iX < nBlockXSize; iX++ )
            {
                int  nOutWord = 0;

                for( iBit = 0; iBit < poGDS->nBitsPerSample; iBit++ )
                {
                    if( pabyBlockBuf[iBitOffset>>3]
                        & (0x80 >>(iBitOffset & 7)) )
                        nOutWord |= (1 << (poGDS->nBitsPerSample - 1 - iBit));
                    iBitOffset++;
                }

                iBitOffset= iBitOffset + iPixelBitSkip - poGDS->nBitsPerSample;

                if( eDataType == GDT_Byte )
                    ((GByte *) pImage)[iPixel++] = (GByte) nOutWord;
                else if( eDataType == GDT_UInt16 )
                    ((GUInt16 *) pImage)[iPixel++] = (GUInt16) nOutWord;
                else if( eDataType == GDT_UInt32 )
                    ((GUInt32 *) pImage)[iPixel++] = nOutWord;
                else {
                    CPLAssert(0);
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

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};


/************************************************************************/
/*                           GTiffBitmapBand()                          */
/************************************************************************/

GTiffBitmapBand::GTiffBitmapBand( GTiffDataset *poDS, int nBand )
        : GTiffOddBitsBand( poDS, nBand )

{
    eDataType = GDT_Byte;

    if( poDS->poColorTable != NULL )
        poColorTable = poDS->poColorTable->Clone();
    else
    {
#ifdef ESRI_BUILD
        poColorTable = NULL;
#else
        GDALColorEntry	oWhite, oBlack;

        oWhite.c1 = 255;
        oWhite.c2 = 255;
        oWhite.c3 = 255;
        oWhite.c4 = 255;

        oBlack.c1 = 0;
        oBlack.c2 = 0;
        oBlack.c3 = 0;
        oBlack.c4 = 255;

        poColorTable = new GDALColorTable();
        
        if( poDS->nPhotometric == PHOTOMETRIC_MINISWHITE )
        {
            poColorTable->SetColorEntry( 0, &oWhite );
            poColorTable->SetColorEntry( 1, &oBlack );
        }
        else
        {
            poColorTable->SetColorEntry( 0, &oBlack );
            poColorTable->SetColorEntry( 1, &oWhite );
        }
#endif /* not defined ESRI_BUILD */
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
    if (poGDS->bPromoteTo8Bits)
        return GCI_Undefined;
    else
        return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffBitmapBand::GetColorTable()

{
    if (poGDS->bPromoteTo8Bits)
        return NULL;
    else
        return poColorTable;
}

/************************************************************************/
/* ==================================================================== */
/*                          GTiffSplitBitmapBand                        */
/* ==================================================================== */
/************************************************************************/

class GTiffSplitBitmapBand : public GTiffBitmapBand
{
    friend class GTiffDataset;

  public:

                   GTiffSplitBitmapBand( GTiffDataset *, int );
    virtual       ~GTiffSplitBitmapBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
};


/************************************************************************/
/*                       GTiffSplitBitmapBand()                         */
/************************************************************************/

GTiffSplitBitmapBand::GTiffSplitBitmapBand( GTiffDataset *poDS, int nBand )
        : GTiffBitmapBand( poDS, nBand )

{
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                      ~GTiffSplitBitmapBand()                         */
/************************************************************************/

GTiffSplitBitmapBand::~GTiffSplitBitmapBand()

{
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBitmapBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                         void * pImage )

{
    (void) nBlockXOff;

    if (!poGDS->SetDirectory())
        return CE_Failure;
        
    if (poGDS->pabyBlockBuf == NULL)
    {
        poGDS->pabyBlockBuf = (GByte *) VSIMalloc(TIFFScanlineSize(poGDS->hTIFF));
        if( poGDS->pabyBlockBuf == NULL )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate " CPL_FRMT_GUIB " bytes.",
                         (GUIntBig)TIFFScanlineSize(poGDS->hTIFF));
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
        if( TIFFReadScanline( poGDS->hTIFF, poGDS->pabyBlockBuf, ++poGDS->nLastLineRead, 0 ) == -1
            && !poGDS->bIgnoreReadErrors )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadScanline() failed." );
            return CE_Failure;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Translate 1bit data to eight bit.                               */
/* -------------------------------------------------------------------- */
    int	  iPixel, iSrcOffset=0, iDstOffset=0;

    for( iPixel = 0; iPixel < nBlockXSize; iPixel++, iSrcOffset++ )
    {
        if( poGDS->pabyBlockBuf[iSrcOffset >>3] & (0x80 >> (iSrcOffset & 0x7)) )
            ((GByte *) pImage)[iDstOffset++] = 1;
        else
            ((GByte *) pImage)[iDstOffset++] = 0;
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBitmapBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                          void * pImage )

{
    (void) nBlockXOff;
    (void) nBlockYOff;
    (void) pImage;

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

GTiffDataset::GTiffDataset()

{
    nLoadedBlock = -1;
    bLoadedBlockDirty = FALSE;
    pabyBlockBuf = NULL;
    bWriteErrorInFlushBlockBuf = FALSE;
    hTIFF = NULL;
    fpL = NULL;
    bNeedsRewrite = FALSE;
    bMetadataChanged = FALSE;
    bColorProfileMetadataChanged = FALSE;
    bGeoTIFFInfoChanged = FALSE;
    bForceUnsetGT = FALSE;
    bForceUnsetProjection = FALSE;
    bCrystalized = TRUE;
    poColorTable = NULL;
    bNoDataSet = FALSE;
    dfNoDataValue = -9999.0;
    pszProjection = CPLStrdup("");
    bLookedForProjection = FALSE;
    bLookedForMDAreaOrPoint = FALSE;
    bBase = TRUE;
    bCloseTIFFHandle = FALSE;
    bTreatAsRGBA = FALSE;
    nOverviewCount = 0;
    papoOverviewDS = NULL;

    nJPEGOverviewVisibilityFlag = FALSE;
    nJPEGOverviewCount = -1;
    nJPEGOverviewCountOri = 0;
    papoJPEGOverviewDS = NULL;

    nDirOffset = 0;
    poActiveDS = NULL;
    ppoActiveDSRef = NULL;

    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;

    osProfile = "GDALGeoTIFF";

    papszCreationOptions = NULL;

    nTempWriteBufferSize = 0;
    pabyTempWriteBuffer = NULL;

    poMaskDS = NULL;
    poBaseDS = NULL;

    bFillEmptyTiles = FALSE;
    bLoadingOtherBands = FALSE;
    nLastLineRead = -1;
    nLastBandRead = -1;
    bTreatAsSplit = FALSE;
    bTreatAsSplitBitmap = FALSE;
    bClipWarn = FALSE;
    bHasWarnedDisableAggressiveBandCaching = FALSE;
    bDontReloadFirstBlock = FALSE;

    nZLevel = -1;
    nLZMAPreset = -1;
    nJpegQuality = -1;
    
    bPromoteTo8Bits = FALSE;

    bDebugDontWriteBlocks = CSLTestBoolean(CPLGetConfigOption("GTIFF_DONT_WRITE_BLOCKS", "NO"));

    bIsFinalized = FALSE;
    bIgnoreReadErrors = CSLTestBoolean(CPLGetConfigOption("GTIFF_IGNORE_READ_ERRORS", "NO"));

    bHasSearchedRPC = FALSE;
    bHasSearchedIMD = FALSE;
    bHasSearchedPVL = FALSE;
    bEXIFMetadataLoaded = FALSE;
    bICCMetadataLoaded = FALSE;

    bScanDeferred = TRUE;

    bDirectIO = CSLTestBoolean(CPLGetConfigOption("GTIFF_DIRECT_IO", "NO"));
    nSetPhotometricFromBandColorInterp = 0;

    pBaseMapping = NULL;
    nRefBaseMapping = 0;
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
    if (bIsFinalized)
        return FALSE;

    int bHasDroppedRef = FALSE;

    Crystalize();

    if ( bColorProfileMetadataChanged )
    {
        SaveICCProfile(this, NULL, NULL, 0);
        bColorProfileMetadataChanged = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Handle forcing xml:ESRI data to be written to PAM.              */
/* -------------------------------------------------------------------- */
    if( CSLTestBoolean(CPLGetConfigOption( "ESRI_XML_PAM", "NO" )) )
    {
        char **papszESRIMD = GetMetadata("xml:ESRI");
        if( papszESRIMD )
        {
            GDALPamDataset::SetMetadata( papszESRIMD, "xml:ESRI");
        }
    }

/* -------------------------------------------------------------------- */
/*      Ensure any blocks write cached by GDAL gets pushed through libtiff.*/
/* -------------------------------------------------------------------- */
    GDALPamDataset::FlushCache();

/* -------------------------------------------------------------------- */
/*      Fill in missing blocks with empty data.                         */
/* -------------------------------------------------------------------- */
    if( bFillEmptyTiles )
    {
        FillEmptyTiles();
        bFillEmptyTiles = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Force a complete flush, including either rewriting(moving)      */
/*      of writing in place the current directory.                      */
/* -------------------------------------------------------------------- */
    FlushCache();

/* -------------------------------------------------------------------- */
/*      If there is still changed metadata, then presumably we want     */
/*      to push it into PAM.                                            */
/* -------------------------------------------------------------------- */
    if( bMetadataChanged )
    {
        PushMetadataToPam();
        bMetadataChanged = FALSE;
        GDALPamDataset::FlushCache();
    }

/* -------------------------------------------------------------------- */
/*      Cleanup overviews.                                              */
/* -------------------------------------------------------------------- */
    if( bBase )
    {
        for( int i = 0; i < nOverviewCount; i++ )
        {
            delete papoOverviewDS[i];
            bHasDroppedRef = TRUE;
        }
        nOverviewCount = 0;

        for( int i = 0; i < nJPEGOverviewCountOri; i++ )
        {
            delete papoJPEGOverviewDS[i];
            bHasDroppedRef = TRUE;
        }
        nJPEGOverviewCount = 0;
        nJPEGOverviewCountOri = 0;
        CPLFree( papoJPEGOverviewDS );
        papoJPEGOverviewDS = NULL;
    }

    /* If we are a mask dataset, we can have overviews, but we don't */
    /* own them. We can only free the array, not the overviews themselves */
    CPLFree( papoOverviewDS );
    papoOverviewDS = NULL;

    /* poMaskDS is owned by the main image and the overviews */
    /* so because of the latter case, we can delete it even if */
    /* we are not the base image */
    if (poMaskDS)
    {
        delete poMaskDS;
        poMaskDS = NULL;
        bHasDroppedRef = TRUE;
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
            VSIFCloseL( fpL );
            fpL = NULL;
        }
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

    if( *ppoActiveDSRef == this )
        *ppoActiveDSRef = NULL;
    ppoActiveDSRef = NULL;

    bIsFinalized = TRUE;

    return bHasDroppedRef;
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int GTiffDataset::CloseDependentDatasets()
{
    if (!bBase)
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
    if( eAccess != GA_ReadOnly || nCompression != COMPRESSION_JPEG ||
        (nRasterXSize < 256 && nRasterYSize < 256) ||
        !CSLTestBoolean(CPLGetConfigOption("GTIFF_IMPLICIT_JPEG_OVR", "YES")) ||
        GDALGetDriverByName("JPEG") == NULL )
    {
        return 0;
    }

    /* libjpeg-6b only suppports 2, 4 and 8 scale denominators */
    /* TODO: Later versions support more */
    int i;
    for(i = 2; i >= 0; i--)
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
        return 0;

    /* Get JPEG tables */
    uint32 nJPEGTableSize = 0;
    void* pJPEGTable = NULL;
    if( TIFFGetField(hTIFF, TIFFTAG_JPEGTABLES, &nJPEGTableSize, &pJPEGTable) != 1 ||
        pJPEGTable == NULL || (int)nJPEGTableSize <= 0 ||
        ((GByte*)pJPEGTable)[nJPEGTableSize-1] != 0xD9 )
    {
        return 0;
    }
    nJPEGTableSize --; /* remove final 0xD9 */

    papoJPEGOverviewDS = (GTiffJPEGOverviewDS**) CPLMalloc(
                        sizeof(GTiffJPEGOverviewDS*) * nJPEGOverviewCount );
    for(i = 0; i < nJPEGOverviewCount; i++)
    {
        papoJPEGOverviewDS[i] = new GTiffJPEGOverviewDS(this, i+1,
                                            pJPEGTable, (int)nJPEGTableSize);
    }

    nJPEGOverviewCountOri = nJPEGOverviewCount;

    return nJPEGOverviewCount;
}

/************************************************************************/
/*                           FillEmptyTiles()                           */
/************************************************************************/

void GTiffDataset::FillEmptyTiles()

{
    toff_t *panByteCounts = NULL;
    int    nBlockCount, iBlock;

    if (!SetDirectory())
        return;

/* -------------------------------------------------------------------- */
/*      How many blocks are there in this file?                         */
/* -------------------------------------------------------------------- */
    if( nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockCount = nBlocksPerBand * nBands;
    else
        nBlockCount = nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*      Fetch block maps.                                               */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled( hTIFF ) )
        TIFFGetField( hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts );
    else
        TIFFGetField( hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts );

    if (panByteCounts == NULL)
    {
        /* Got here with libtiff 3.9.3 and tiff_write_8 test */
        CPLError(CE_Failure, CPLE_AppDefined, "FillEmptyTiles() failed because panByteCounts == NULL");
        return;
    }

/* -------------------------------------------------------------------- */
/*      Prepare a blank data buffer to write for uninitialized blocks.  */
/* -------------------------------------------------------------------- */
    int nBlockBytes;

    if( TIFFIsTiled( hTIFF ) )
        nBlockBytes = TIFFTileSize(hTIFF);
    else
        nBlockBytes = TIFFStripSize(hTIFF);

    GByte *pabyData = (GByte *) VSICalloc(nBlockBytes,1);
    if (pabyData == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate %d bytes", nBlockBytes);
        return;
    }

/* -------------------------------------------------------------------- */
/*      Check all blocks, writing out data for uninitialized blocks.    */
/* -------------------------------------------------------------------- */
    for( iBlock = 0; iBlock < nBlockCount; iBlock++ )
    {
        if( panByteCounts[iBlock] == 0 )
        {
            if( WriteEncodedTileOrStrip( iBlock, pabyData, FALSE ) != CE_None )
                break;
        }
    }

    CPLFree( pabyData );
}

/************************************************************************/
/*                        WriteEncodedTile()                            */
/************************************************************************/

int GTiffDataset::WriteEncodedTile(uint32 tile, GByte *pabyData,
                                   int bPreserveDataBuffer)
{
    int cc = TIFFTileSize( hTIFF );
    int bNeedTileFill = FALSE;
    int iRow=0, iColumn=0;
    int nBlocksPerRow=1, nBlocksPerColumn=1;

    /* 
    ** Do we need to spread edge values right or down for a partial 
    ** JPEG encoded tile?  We do this to avoid edge artifacts. 
    */
    if( nCompression == COMPRESSION_JPEG )
    {
        nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
        nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);

        iColumn = (tile % nBlocksPerBand) % nBlocksPerRow;
        iRow = (tile % nBlocksPerBand) / nBlocksPerRow;

        // Is this a partial right edge tile?
        if( iRow == nBlocksPerRow - 1
            && nRasterXSize % nBlockXSize != 0 )
            bNeedTileFill = TRUE;

        // Is this a partial bottom edge tile?
        if( iColumn == nBlocksPerColumn - 1
            && nRasterYSize % nBlockYSize != 0 )
            bNeedTileFill = TRUE;
    }

    /* 
    ** If we need to fill out the tile, or if we want to prevent
    ** TIFFWriteEncodedTile from altering the buffer as part of
    ** byte swapping the data on write then we will need a temporary
    ** working buffer.  If not, we can just do a direct write. 
    */
    if (bPreserveDataBuffer 
        && (TIFFIsByteSwapped(hTIFF) || bNeedTileFill) )
    {
        if (cc != nTempWriteBufferSize)
        {
            pabyTempWriteBuffer = CPLRealloc(pabyTempWriteBuffer, cc);
            nTempWriteBufferSize = cc;
        }
        memcpy(pabyTempWriteBuffer, pabyData, cc);

        pabyData = (GByte *) pabyTempWriteBuffer;
    }

    /*
    ** Perform tile fill if needed.
    */
    if( bNeedTileFill )
    {
        int nRightPixelsToFill = 0;
        int nBottomPixelsToFill = 0;
        int nPixelSize = cc / (nBlockXSize * nBlockYSize);
        unsigned int iX, iY, iSrcX, iSrcY;
        
        CPLDebug( "GTiff", "Filling out jpeg edge tile on write." );

        if( iColumn == nBlocksPerRow - 1 )
            nRightPixelsToFill = nBlockXSize * (iColumn+1) - nRasterXSize;
        if( iRow == nBlocksPerColumn - 1 )
            nBottomPixelsToFill = nBlockYSize * (iRow+1) - nRasterYSize;

        // Fill out to the right. 
        iSrcX = nBlockXSize - nRightPixelsToFill - 1;

        for( iX = iSrcX+1; iX < nBlockXSize; iX++ )
        {
            for( iY = 0; iY < nBlockYSize; iY++ )
            {
                memcpy( pabyData + (nBlockXSize * iY + iX) * nPixelSize, 
                        pabyData + (nBlockXSize * iY + iSrcX) * nPixelSize, 
                        nPixelSize );
            }
        }

        // now fill out the bottom.
        iSrcY = nBlockYSize - nBottomPixelsToFill - 1;
        for( iY = iSrcY+1; iY < nBlockYSize; iY++ )
        {
            memcpy( pabyData + nBlockXSize * nPixelSize * iY, 
                    pabyData + nBlockXSize * nPixelSize * iSrcY, 
                    nPixelSize * nBlockXSize );
        }
    }

    return TIFFWriteEncodedTile(hTIFF, tile, pabyData, cc);
}

/************************************************************************/
/*                        WriteEncodedStrip()                           */
/************************************************************************/

int  GTiffDataset::WriteEncodedStrip(uint32 strip, GByte* pabyData,
                                     int bPreserveDataBuffer)
{
    int cc = TIFFStripSize( hTIFF );
    
/* -------------------------------------------------------------------- */
/*      If this is the last strip in the image, and is partial, then    */
/*      we need to trim the number of scanlines written to the          */
/*      amount of valid data we have. (#2748)                           */
/* -------------------------------------------------------------------- */
    int nStripWithinBand = strip % nBlocksPerBand;

    if( (int) ((nStripWithinBand+1) * nRowsPerStrip) > GetRasterYSize() )
    {
        cc = (cc / nRowsPerStrip)
            * (GetRasterYSize() - nStripWithinBand * nRowsPerStrip);
        CPLDebug( "GTiff", "Adjusted bytes to write from %d to %d.", 
                  (int) TIFFStripSize(hTIFF), cc );
    }

/* -------------------------------------------------------------------- */
/*      TIFFWriteEncodedStrip can alter the passed buffer if            */
/*      byte-swapping is necessary so we use a temporary buffer         */
/*      before calling it.                                              */
/* -------------------------------------------------------------------- */
    if (bPreserveDataBuffer && TIFFIsByteSwapped(hTIFF))
    {
        if (cc != nTempWriteBufferSize)
        {
            pabyTempWriteBuffer = CPLRealloc(pabyTempWriteBuffer, cc);
            nTempWriteBufferSize = cc;
        }
        memcpy(pabyTempWriteBuffer, pabyData, cc);
        return TIFFWriteEncodedStrip(hTIFF, strip, pabyTempWriteBuffer, cc);
    }
    else
        return TIFFWriteEncodedStrip(hTIFF, strip, pabyData, cc);
}

/************************************************************************/
/*                  WriteEncodedTileOrStrip()                           */
/************************************************************************/

CPLErr  GTiffDataset::WriteEncodedTileOrStrip(uint32 tile_or_strip, void* data,
                                              int bPreserveDataBuffer)
{
    CPLErr eErr = CE_None;

    if( TIFFIsTiled( hTIFF ) )
    {
        if( WriteEncodedTile(tile_or_strip, (GByte*) data, 
                             bPreserveDataBuffer) == -1 )
        {
            eErr = CE_Failure;
        }
    }
    else
    {
        if( WriteEncodedStrip(tile_or_strip, (GByte *) data, 
                              bPreserveDataBuffer) == -1 )
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
    CPLErr      eErr = CE_None;

    if( nLoadedBlock < 0 || !bLoadedBlockDirty )
        return CE_None;

    bLoadedBlockDirty = FALSE;

    if (!SetDirectory())
        return CE_Failure;

    eErr = WriteEncodedTileOrStrip(nLoadedBlock, pabyBlockBuf, TRUE);
    if (eErr != CE_None)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "WriteEncodedTile/Strip() failed." );
        bWriteErrorInFlushBlockBuf = TRUE;
    }

    return eErr;
}

/************************************************************************/
/*                            LoadBlockBuf()                            */
/*                                                                      */
/*      Load working block buffer with request block (tile/strip).      */
/************************************************************************/

CPLErr GTiffDataset::LoadBlockBuf( int nBlockId, int bReadFromDisk )

{
    int	nBlockBufSize;
    CPLErr	eErr = CE_None;

    if( nLoadedBlock == nBlockId )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      If we have a dirty loaded block, flush it out first.            */
/* -------------------------------------------------------------------- */
    if( nLoadedBlock != -1 && bLoadedBlockDirty )
    {
        eErr = FlushBlockBuf();
        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Get block size.                                                 */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled(hTIFF) )
        nBlockBufSize = TIFFTileSize( hTIFF );
    else
        nBlockBufSize = TIFFStripSize( hTIFF );

    if ( !nBlockBufSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Bogus block size; unable to allocate a buffer.");
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( pabyBlockBuf == NULL )
    {
        pabyBlockBuf = (GByte *) VSICalloc( 1, nBlockBufSize );
        if( pabyBlockBuf == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Unable to allocate %d bytes for a temporary strip "
                      "buffer in GTIFF driver.",
                      nBlockBufSize );
            
            return( CE_Failure );
        }
    }

/* -------------------------------------------------------------------- */
/*  When called from ::IWriteBlock in separate cases (or in single band */
/*  geotiffs), the ::IWriteBlock will override the content of the buffer*/
/*  with pImage, so we don't need to read data from disk                */
/* -------------------------------------------------------------------- */
    if( !bReadFromDisk )
    {
        nLoadedBlock = nBlockId;
        return CE_None;
    }

    /* libtiff 3.X doesn't like mixing read&write of JPEG compressed blocks */
    /* The below hack is necessary due to another hack that consist in */
    /* writing zero block to force creation of JPEG tables */
    if( nBlockId == 0 && bDontReloadFirstBlock )
    {
        bDontReloadFirstBlock = FALSE;
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
    int nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    int nBlockYOff = (nBlockId % nBlocksPerBand) / nBlocksPerRow;

    if( (int)((nBlockYOff+1) * nBlockYSize) > nRasterYSize )
    {
        nBlockReqSize = (nBlockBufSize / nBlockYSize) 
            * (nBlockYSize - (((nBlockYOff+1) * nBlockYSize) % nRasterYSize));
        memset( pabyBlockBuf, 0, nBlockBufSize );
    }

/* -------------------------------------------------------------------- */
/*      If we don't have this block already loaded, and we know it      */
/*      doesn't yet exist on disk, just zero the memory buffer and      */
/*      pretend we loaded it.                                           */
/* -------------------------------------------------------------------- */
    if( !IsBlockAvailable( nBlockId ) )
    {
        memset( pabyBlockBuf, 0, nBlockBufSize );
        nLoadedBlock = nBlockId;
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Load the block, if it isn't our current block.                  */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled( hTIFF ) )
    {
        if( TIFFReadEncodedTile(hTIFF, nBlockId, pabyBlockBuf,
                                nBlockReqSize) == -1
            && !bIgnoreReadErrors )
        {
            /* Once TIFFError() is properly hooked, this can go away */
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
            /* Once TIFFError() is properly hooked, this can go away */
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFReadEncodedStrip() failed." );
                
            memset( pabyBlockBuf, 0, nBlockBufSize );
                
            eErr = CE_Failure;
        }
    }

    nLoadedBlock = nBlockId;
    bLoadedBlockDirty = FALSE;

    return eErr;
}


/************************************************************************/
/*                             Crystalize()                             */
/*                                                                      */
/*      Make sure that the directory information is written out for     */
/*      a new file, require before writing any imagery data.            */
/************************************************************************/

void GTiffDataset::Crystalize()

{
    if( !bCrystalized )
    {
        WriteMetadata( this, hTIFF, TRUE, osProfile, osFilename,
                       papszCreationOptions );
        WriteGeoTIFFInfo();

        bMetadataChanged = FALSE;
        bGeoTIFFInfoChanged = FALSE;
        bNeedsRewrite = FALSE;

        bCrystalized = TRUE;

        TIFFWriteCheck( hTIFF, TIFFIsTiled(hTIFF), "GTiffDataset::Crystalize");

        // Keep zip and tiff quality, and jpegcolormode which get reset when we call 
        // TIFFWriteDirectory 
        int jquality = -1, zquality = -1, nColorMode = -1; 
        TIFFGetField(hTIFF, TIFFTAG_JPEGQUALITY, &jquality); 
        TIFFGetField(hTIFF, TIFFTAG_ZIPQUALITY, &zquality); 
        TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode );

        TIFFWriteDirectory( hTIFF );
        TIFFSetDirectory( hTIFF, 0 );


        // Now, reset zip and tiff quality and jpegcolormode. 
        if(jquality > 0) 
            TIFFSetField(hTIFF, TIFFTAG_JPEGQUALITY, jquality); 
        if(zquality > 0) 
            TIFFSetField(hTIFF, TIFFTAG_ZIPQUALITY, zquality);
        if (nColorMode >= 0)
            TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, nColorMode);

        nDirOffset = TIFFCurrentDirOffset( hTIFF );
    }
}

#ifdef INTERNAL_LIBTIFF

#define IO_CACHE_PAGE_SIZE      4096

static
void GTiffCacheOffsetOrCount4(VSILFILE* fp,
                              vsi_l_offset nBaseOffset,
                              int nBlockId,
                              uint32 nstrips,
                              uint64* panVals)
{
    uint32 val;
    int i, iStartBefore;
    vsi_l_offset nOffset, nOffsetStartPage, nOffsetEndPage;
    GByte buffer[2 * IO_CACHE_PAGE_SIZE];

    nOffset = nBaseOffset + sizeof(val) * nBlockId;
    nOffsetStartPage = (nOffset / IO_CACHE_PAGE_SIZE) * IO_CACHE_PAGE_SIZE;
    nOffsetEndPage = nOffsetStartPage + IO_CACHE_PAGE_SIZE;

    if( nOffset + sizeof(val) > nOffsetEndPage )
        nOffsetEndPage += IO_CACHE_PAGE_SIZE;
    VSIFSeekL(fp, nOffsetStartPage, SEEK_SET);
    VSIFReadL(buffer, 1, (size_t)(nOffsetEndPage - nOffsetStartPage), fp);
    iStartBefore = - (int)((nOffset - nOffsetStartPage) / sizeof(val));
    if( nBlockId + iStartBefore < 0 )
        iStartBefore = -nBlockId;
    for(i=iStartBefore; (uint32)(nBlockId + i) < nstrips &&
        (GIntBig)nOffset + (i+1) * (int)sizeof(val) <= (GIntBig)nOffsetEndPage; i++)
    {
        memcpy(&val,
            buffer + (nOffset - nOffsetStartPage) + i * sizeof(val),
            sizeof(val));
        panVals[nBlockId + i] = val;
    }
}

/* Same code as GTiffCacheOffsetOrCount4 except the 'val' declaration */
static
void GTiffCacheOffsetOrCount8(VSILFILE* fp,
                              vsi_l_offset nBaseOffset,
                              int nBlockId,
                              uint32 nstrips,
                              uint64* panVals)
{
    uint64 val;
    int i, iStartBefore;
    vsi_l_offset nOffset, nOffsetStartPage, nOffsetEndPage;
    GByte buffer[2 * IO_CACHE_PAGE_SIZE];

    nOffset = nBaseOffset + sizeof(val) * nBlockId;
    nOffsetStartPage = (nOffset / IO_CACHE_PAGE_SIZE) * IO_CACHE_PAGE_SIZE;
    nOffsetEndPage = nOffsetStartPage + IO_CACHE_PAGE_SIZE;

    if( nOffset + sizeof(val) > nOffsetEndPage )
        nOffsetEndPage += IO_CACHE_PAGE_SIZE;
    VSIFSeekL(fp, nOffsetStartPage, SEEK_SET);
    VSIFReadL(buffer, 1, (size_t)(nOffsetEndPage - nOffsetStartPage), fp);
    iStartBefore = - (int)((nOffset - nOffsetStartPage) / sizeof(val));
    if( nBlockId + iStartBefore < 0 )
        iStartBefore = -nBlockId;
    for(i=iStartBefore; (uint32)(nBlockId + i) < nstrips &&
        (GIntBig)nOffset + (i+1) * (int)sizeof(val) <= (GIntBig)nOffsetEndPage; i++)
    {
        memcpy(&val,
            buffer + (nOffset - nOffsetStartPage) + i * sizeof(val),
            sizeof(val));
        panVals[nBlockId + i] = val;
    }
}

#endif

/************************************************************************/
/*                          IsBlockAvailable()                          */
/*                                                                      */
/*      Return TRUE if the indicated strip/tile is available.  We       */
/*      establish this by testing if the stripbytecount is zero.  If    */
/*      zero then the block has never been committed to disk.           */
/************************************************************************/

int GTiffDataset::IsBlockAvailable( int nBlockId )

{
#ifdef INTERNAL_LIBTIFF

    /* Optimization to avoid fetching the whole Strip/TileCounts and Strip/TileOffsets arrays */
    if( eAccess == GA_ReadOnly &&
        !(hTIFF->tif_flags & TIFF_SWAB) &&
        hTIFF->tif_dir.td_nstrips > 2 &&
        (hTIFF->tif_dir.td_stripoffset_entry.tdir_type == TIFF_LONG ||
         hTIFF->tif_dir.td_stripoffset_entry.tdir_type == TIFF_LONG8) &&
        (hTIFF->tif_dir.td_stripbytecount_entry.tdir_type == TIFF_LONG ||
         hTIFF->tif_dir.td_stripbytecount_entry.tdir_type == TIFF_LONG8) &&
        strcmp(GetDescription(), "/vsistdin/") != 0 )
    {
        if( hTIFF->tif_dir.td_stripoffset == NULL )
        {
            hTIFF->tif_dir.td_stripoffset =
                (uint64*) _TIFFmalloc( sizeof(uint64) * hTIFF->tif_dir.td_nstrips );
            hTIFF->tif_dir.td_stripbytecount =
                (uint64*) _TIFFmalloc( sizeof(uint64) * hTIFF->tif_dir.td_nstrips );
            if( hTIFF->tif_dir.td_stripoffset && hTIFF->tif_dir.td_stripbytecount )
            {
                memset(hTIFF->tif_dir.td_stripoffset, 0xFF,
                       sizeof(uint64) * hTIFF->tif_dir.td_nstrips );
                memset(hTIFF->tif_dir.td_stripbytecount, 0xFF,
                       sizeof(uint64) * hTIFF->tif_dir.td_nstrips );
            }
            else
            {
                _TIFFfree(hTIFF->tif_dir.td_stripoffset);
                hTIFF->tif_dir.td_stripoffset = NULL;
                _TIFFfree(hTIFF->tif_dir.td_stripbytecount);
                hTIFF->tif_dir.td_stripbytecount = NULL;
            }
        }
        if( hTIFF->tif_dir.td_stripbytecount == NULL )
            return FALSE;
        if( ~(hTIFF->tif_dir.td_stripoffset[nBlockId]) == 0 ||
            ~(hTIFF->tif_dir.td_stripbytecount[nBlockId]) == 0 )
        {
            VSILFILE* fp = (VSILFILE*)hTIFF->tif_clientdata;
            vsi_l_offset nCurOffset = VSIFTellL(fp);
            if( ~(hTIFF->tif_dir.td_stripoffset[nBlockId]) == 0 )
            {
                if( hTIFF->tif_dir.td_stripoffset_entry.tdir_type == TIFF_LONG )
                {
                    GTiffCacheOffsetOrCount4(fp,
                                             hTIFF->tif_dir.td_stripoffset_entry.tdir_offset.toff_long,
                                             nBlockId,
                                             hTIFF->tif_dir.td_nstrips,
                                             hTIFF->tif_dir.td_stripoffset);
                }
                else
                {
                    GTiffCacheOffsetOrCount8(fp,
                                             hTIFF->tif_dir.td_stripoffset_entry.tdir_offset.toff_long8,
                                             nBlockId,
                                             hTIFF->tif_dir.td_nstrips,
                                             hTIFF->tif_dir.td_stripoffset);
                }
            }

            if( ~(hTIFF->tif_dir.td_stripbytecount[nBlockId]) == 0 )
            {
                if( hTIFF->tif_dir.td_stripbytecount_entry.tdir_type == TIFF_LONG )
                {
                    GTiffCacheOffsetOrCount4(fp,
                                             hTIFF->tif_dir.td_stripbytecount_entry.tdir_offset.toff_long,
                                             nBlockId,
                                             hTIFF->tif_dir.td_nstrips,
                                             hTIFF->tif_dir.td_stripbytecount);
                }
                else
                {
                    GTiffCacheOffsetOrCount8(fp,
                                             hTIFF->tif_dir.td_stripbytecount_entry.tdir_offset.toff_long8,
                                             nBlockId,
                                             hTIFF->tif_dir.td_nstrips,
                                             hTIFF->tif_dir.td_stripbytecount);
                }
            }
            VSIFSeekL(fp, nCurOffset, SEEK_SET);
        }
        return hTIFF->tif_dir.td_stripbytecount[nBlockId] != 0;
    }
#endif
    toff_t *panByteCounts = NULL;

    if( ( TIFFIsTiled( hTIFF ) 
          && TIFFGetField( hTIFF, TIFFTAG_TILEBYTECOUNTS, &panByteCounts ) )
        || ( !TIFFIsTiled( hTIFF ) 
          && TIFFGetField( hTIFF, TIFFTAG_STRIPBYTECOUNTS, &panByteCounts ) ) )
    {
        if( panByteCounts == NULL )
            return FALSE;
        else
            return panByteCounts[nBlockId] != 0;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we can also flush out local tiff strip      */
/*      cache if need be.                                               */
/************************************************************************/

void GTiffDataset::FlushCache()

{
    if (bIsFinalized)
        return;

    GDALPamDataset::FlushCache();

    if( bLoadedBlockDirty && nLoadedBlock != -1 )
        FlushBlockBuf();

    CPLFree( pabyBlockBuf );
    pabyBlockBuf = NULL;
    nLoadedBlock = -1;
    bLoadedBlockDirty = FALSE;

    if (!SetDirectory())
        return;
    FlushDirectory();
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
            if (!SetDirectory())
                return;
            bNeedsRewrite = 
                WriteMetadata( this, hTIFF, TRUE, osProfile, osFilename,
                               papszCreationOptions );
            bMetadataChanged = FALSE;
        }
        
        if( bGeoTIFFInfoChanged )
        {
            if (!SetDirectory())
                return;
            WriteGeoTIFFInfo();
        }

        if( bNeedsRewrite )
        {
#if defined(TIFFLIB_VERSION)
#if defined(HAVE_TIFFGETSIZEPROC)
            if (!SetDirectory())
                return;

            TIFFSizeProc pfnSizeProc = TIFFGetSizeProc( hTIFF );

            nDirOffset = pfnSizeProc( TIFFClientdata( hTIFF ) );
            if( (nDirOffset % 2) == 1 )
                nDirOffset++;

            TIFFRewriteDirectory( hTIFF );

            TIFFSetSubDirectory( hTIFF, nDirOffset );
#elif  TIFFLIB_VERSION > 20010925 && TIFFLIB_VERSION != 20011807
            if (!SetDirectory())
                return;

            TIFFRewriteDirectory( hTIFF );
#endif
#endif
            bNeedsRewrite = FALSE;
        }
    }

    // there are some circumstances in which we can reach this point
    // without having made this our directory (SetDirectory()) in which
    // case we should not risk a flush. 
    if( GetAccess() == GA_Update && TIFFCurrentDirOffset(hTIFF) == nDirOffset )
    {
#if defined(BIGTIFF_SUPPORT)
        TIFFSizeProc pfnSizeProc = TIFFGetSizeProc( hTIFF );

        toff_t nNewDirOffset = pfnSizeProc( TIFFClientdata( hTIFF ) );
        if( (nNewDirOffset % 2) == 1 )
            nNewDirOffset++;

        TIFFFlush( hTIFF );

        if( nDirOffset != TIFFCurrentDirOffset( hTIFF ) )
        {
            nDirOffset = nNewDirOffset;
            CPLDebug( "GTiff", 
                      "directory moved during flush in FlushDirectory()" );
        }
#else
        /* For libtiff 3.X, the above causes regressions and crashes in */
        /* tiff_write.py and tiff_ovr.py */
        TIFFFlush( hTIFF );
#endif
    }
}

/************************************************************************/
/*                         TIFF_OvLevelAdjust()                         */
/*                                                                      */
/*      Some overview levels cannot be achieved closely enough to be    */
/*      recognised as the desired overview level.  This function        */
/*      will adjust an overview level to one that is achievable on      */
/*      the given raster size.                                          */
/*                                                                      */
/*      For instance a 1200x1200 image on which a 256 level overview    */
/*      is request will end up generating a 5x5 overview.  However,     */
/*      this will appear to the system be a level 240 overview.         */
/*      This function will adjust 256 to 240 based on knowledge of      */
/*      the image size.                                                 */
/*                                                                      */
/*      This is a copy of the GDALOvLevelAdjust() function in           */
/*      gdaldefaultoverviews.cpp.                                       */
/************************************************************************/

static int TIFF_OvLevelAdjust( int nOvLevel, int nXSize )

{
    int	nOXSize = (nXSize + nOvLevel - 1) / nOvLevel;
    
    return (int) (0.5 + nXSize / (double) nOXSize);
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
    std::vector<toff_t>  anOvDirOffsets;
    int i;

    for( i = 0; i < nOverviewCount; i++ )
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
    
    for( ; TRUE; ) 
    {
        for( i = 0; i < nOverviewCount; i++ )
        {
            if( anOvDirOffsets[i] == TIFFCurrentDirOffset( hTIFF ) )
            {
                CPLDebug( "GTiff", "%d -> %d", 
                          (int) anOvDirOffsets[i], iThisOffset );
                anOvDirIndexes.push_back( (uint16) iThisOffset );
            }
        }
        
        if( TIFFLastDirectory( hTIFF ) )
            break;

        TIFFReadDirectory( hTIFF );
        iThisOffset++;
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

    if (!SetDirectory())
        return CE_Failure;

    return CE_None;
}


/************************************************************************/
/*                   RegisterNewOverviewDataset()                       */
/************************************************************************/

CPLErr GTiffDataset::RegisterNewOverviewDataset(toff_t nOverviewOffset)
{
    GTiffDataset* poODS = new GTiffDataset();
    poODS->nJpegQuality = nJpegQuality;
    poODS->nZLevel = nZLevel;
    poODS->nLZMAPreset = nLZMAPreset;

    if( nCompression == COMPRESSION_JPEG )
    {
        if ( CPLGetConfigOption( "JPEG_QUALITY_OVERVIEW", NULL ) != NULL )
        {
            poODS->nJpegQuality =  atoi(CPLGetConfigOption("JPEG_QUALITY_OVERVIEW","75"));
        }
        TIFFSetField( hTIFF, TIFFTAG_JPEGQUALITY,
                        poODS->nJpegQuality );
    }

    if( poODS->OpenOffset( hTIFF, ppoActiveDSRef, nOverviewOffset, FALSE,
                            GA_Update ) != CE_None )
    {
        delete poODS;
        return CE_Failure;
    }
    else
    {
        nOverviewCount++;
        papoOverviewDS = (GTiffDataset **)
            CPLRealloc(papoOverviewDS,
                        nOverviewCount * (sizeof(void*)));
        papoOverviewDS[nOverviewCount-1] = poODS;
        poODS->poBaseDS = this;
        return CE_None;
    }
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
    if (!SetDirectory())
        return CE_Failure;
    FlushDirectory();

    int nOvBitsPerSample = nBitsPerSample;

/* -------------------------------------------------------------------- */
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    std::vector<unsigned short> anTRed, anTGreen, anTBlue;
    unsigned short      *panRed=NULL, *panGreen=NULL, *panBlue=NULL;

    if( nPhotometric == PHOTOMETRIC_PALETTE && poColorTable != NULL )
    {
        int nColors;

        if( nOvBitsPerSample == 8 )
            nColors = 256;
        else if( nOvBitsPerSample < 8 )
            nColors = 1 << nOvBitsPerSample;
        else
            nColors = 65536;

        anTRed.resize(nColors,0);
        anTGreen.resize(nColors,0);
        anTBlue.resize(nColors,0);

        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            if( iColor < poColorTable->GetColorEntryCount() )
            {
                GDALColorEntry  sRGB;

                poColorTable->GetColorEntryAsRGB( iColor, &sRGB );

                anTRed[iColor] = (unsigned short) (256 * sRGB.c1);
                anTGreen[iColor] = (unsigned short) (256 * sRGB.c2);
                anTBlue[iColor] = (unsigned short) (256 * sRGB.c3);
            }
            else
            {
                anTRed[iColor] = anTGreen[iColor] = anTBlue[iColor] = 0;
            }
        }

        panRed = &(anTRed[0]);
        panGreen = &(anTGreen[0]);
        panBlue = &(anTBlue[0]);
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

    if( TIFFGetField( hTIFF, TIFFTAG_EXTRASAMPLES, &nExtraSamples, &panExtraSampleValues) )
    {
        uint16* panExtraSampleValuesNew = (uint16*) CPLMalloc(nExtraSamples * sizeof(uint16));
        memcpy(panExtraSampleValuesNew, panExtraSampleValues, nExtraSamples * sizeof(uint16));
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
    if ( nCompression == COMPRESSION_LZW ||
         nCompression == COMPRESSION_ADOBE_DEFLATE )
        TIFFGetField( hTIFF, TIFFTAG_PREDICTOR, &nPredictor );
    int nOvrBlockXSize, nOvrBlockYSize;
    GTIFFGetOverviewBlockSize(&nOvrBlockXSize, &nOvrBlockYSize);

    int nSrcOverviews = poSrcDS->GetRasterBand(1)->GetOverviewCount();
    int i;
    CPLErr eErr = CE_None;

    for(i=0;i<nSrcOverviews && eErr == CE_None;i++)
    {
        GDALRasterBand* poOvrBand = poSrcDS->GetRasterBand(1)->GetOverview(i);

        int         nOXSize = poOvrBand->GetXSize(), nOYSize = poOvrBand->GetYSize();

        toff_t nOverviewOffset =
                GTIFFWriteDirectory(hTIFF, FILETYPE_REDUCEDIMAGE,
                                    nOXSize, nOYSize,
                                    nOvBitsPerSample, nPlanarConfig,
                                    nSamplesPerPixel, nOvrBlockXSize, nOvrBlockYSize, TRUE,
                                    nCompression, nPhotometric, nSampleFormat,
                                    nPredictor,
                                    panRed, panGreen, panBlue,
                                    nExtraSamples, panExtraSampleValues,
                                    osMetadata );

        if( nOverviewOffset == 0 )
            eErr = CE_Failure;
        else
            eErr = RegisterNewOverviewDataset(nOverviewOffset);
    }

    CPLFree(panExtraSampleValues);
    panExtraSampleValues = NULL;

/* -------------------------------------------------------------------- */
/*      Create overviews for the mask.                                  */
/* -------------------------------------------------------------------- */
    if (eErr == CE_None)
        eErr = CreateInternalMaskOverviews(nOvrBlockXSize, nOvrBlockYSize);

    return eErr;
}


/************************************************************************/
/*                       CreateInternalMaskOverviews()                  */
/************************************************************************/

CPLErr GTiffDataset::CreateInternalMaskOverviews(int nOvrBlockXSize,
                                                 int nOvrBlockYSize)
{
    GTiffDataset *poODS;

    ScanDirectories();

/* -------------------------------------------------------------------- */
/*      Create overviews for the mask.                                  */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    const char* pszInternalMask = CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", NULL);
    if (poMaskDS != NULL &&
        poMaskDS->GetRasterCount() == 1 &&
        (pszInternalMask == NULL || CSLTestBoolean(pszInternalMask)))
    {
        int nMaskOvrCompression;
        if( strstr(GDALGetMetadataItem(GDALGetDriverByName( "GTiff" ),
                                       GDAL_DMD_CREATIONOPTIONLIST, NULL ),
                   "<Value>DEFLATE</Value>") != NULL )
            nMaskOvrCompression = COMPRESSION_ADOBE_DEFLATE;
        else
            nMaskOvrCompression = COMPRESSION_PACKBITS;

        int i;
        for( i = 0; i < nOverviewCount; i++ )
        {
            if (papoOverviewDS[i]->poMaskDS == NULL)
            {
                toff_t  nOverviewOffset;

                nOverviewOffset =
                    GTIFFWriteDirectory(hTIFF, FILETYPE_REDUCEDIMAGE | FILETYPE_MASK,
                                        papoOverviewDS[i]->nRasterXSize, papoOverviewDS[i]->nRasterYSize,
                                        1, PLANARCONFIG_CONTIG,
                                        1, nOvrBlockXSize, nOvrBlockYSize, TRUE,
                                        nMaskOvrCompression, PHOTOMETRIC_MASK, SAMPLEFORMAT_UINT, PREDICTOR_NONE,
                                        NULL, NULL, NULL, 0, NULL,
                                        "" );

                if( nOverviewOffset == 0 )
                {
                    eErr = CE_Failure;
                    continue;
                }

                poODS = new GTiffDataset();
                if( poODS->OpenOffset( hTIFF, ppoActiveDSRef,
                                       nOverviewOffset, FALSE,
                                       GA_Update ) != CE_None )
                {
                    delete poODS;
                    eErr = CE_Failure;
                }
                else
                {
                    poODS->bPromoteTo8Bits = CSLTestBoolean(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
                    poODS->poBaseDS = this;
                    papoOverviewDS[i]->poMaskDS = poODS;
                    poMaskDS->nOverviewCount++;
                    poMaskDS->papoOverviewDS = (GTiffDataset **)
                    CPLRealloc(poMaskDS->papoOverviewDS,
                               poMaskDS->nOverviewCount * (sizeof(void*)));
                    poMaskDS->papoOverviewDS[poMaskDS->nOverviewCount-1] = poODS;
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
    int nBands, int * panBandList,
    GDALProgressFunc pfnProgress, void * pProgressData )

{
    CPLErr       eErr = CE_None;
    int          i;
    GTiffDataset *poODS;
    int          bUseGenericHandling = FALSE;

    ScanDirectories();

    /* Make implicit JPEG overviews invisible, but do not destroy */
    /* them in case they are already used (not sure that the client */
    /* has the right to do that. behaviour undefined in GDAL API I think) */
    nJPEGOverviewCount = 0;

/* -------------------------------------------------------------------- */
/*      If RRD or external OVR overviews requested, then invoke         */
/*      generic handling.                                               */
/* -------------------------------------------------------------------- */
    if( CSLTestBoolean(CPLGetConfigOption( "USE_RRD", "NO" )) 
        || CSLTestBoolean(CPLGetConfigOption( "TIFF_USE_OVR", "NO" )) )
    {
        bUseGenericHandling = TRUE;
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

        bUseGenericHandling = TRUE;
    }

    if( bUseGenericHandling )
    {
        if (nOverviewCount != 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Cannot add external overviews when there are already internal overviews");
            return CE_Failure;
        }

        return GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList,
            nBands, panBandList, pfnProgress, pProgressData );
    }

/* -------------------------------------------------------------------- */
/*      Our TIFF overview support currently only works safely if all    */
/*      bands are handled at the same time.                             */
/* -------------------------------------------------------------------- */
    if( nBands != GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in TIFF currently only"
                  " supported when operating on all bands.\n" 
                  "Operation failed.\n" );
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
                nBands, panBandList, pfnProgress, pProgressData );
        else
            return CleanOverviews();
    }

/* -------------------------------------------------------------------- */
/*      libtiff 3.X has issues when generating interleaved overviews.   */
/*      so generate them one after another one.                         */
/* -------------------------------------------------------------------- */
#ifndef BIGTIFF_SUPPORT
    if( nOverviews > 1 )
    {
        double* padfOvrRasterFactor = (double*) CPLMalloc(sizeof(double) * nOverviews);
        double dfTotal = 0;
        for( i = 0; i < nOverviews; i++ )
        {
            if( panOverviewList[i] <= 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid overview factor : %d", panOverviewList[i]);
                eErr = CE_Failure;
                break;
            }
            padfOvrRasterFactor[i] = 1.0 / (panOverviewList[i] * panOverviewList[i]);
            dfTotal += padfOvrRasterFactor[i];
        }

        double dfAcc = 0.0;
        for( i = 0; i < nOverviews && eErr == CE_None; i++ )
        {
            void *pScaledProgressData;
            pScaledProgressData = 
                GDALCreateScaledProgress( dfAcc / dfTotal, 
                                          (dfAcc + padfOvrRasterFactor[i]) / dfTotal,
                                         pfnProgress, pProgressData );
            dfAcc += padfOvrRasterFactor[i];

            eErr = IBuildOverviews( 
                    pszResampling, 1, &panOverviewList[i], 
                    nBands, panBandList, GDALScaledProgress, pScaledProgressData );

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
    if (!SetDirectory())
        return CE_Failure;
    FlushDirectory();

/* -------------------------------------------------------------------- */
/*      If we are averaging bit data to grayscale we need to create     */
/*      8bit overviews.                                                 */
/* -------------------------------------------------------------------- */
    int nOvBitsPerSample = nBitsPerSample;

    if( EQUALN(pszResampling,"AVERAGE_BIT2",12) )
        nOvBitsPerSample = 8;

/* -------------------------------------------------------------------- */
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    std::vector<unsigned short> anTRed, anTGreen, anTBlue;
    unsigned short      *panRed=NULL, *panGreen=NULL, *panBlue=NULL;

    if( nPhotometric == PHOTOMETRIC_PALETTE && poColorTable != NULL )
    {
        int nColors;

        if( nOvBitsPerSample == 8 )
            nColors = 256;
        else if( nOvBitsPerSample < 8 )
            nColors = 1 << nOvBitsPerSample;
        else
            nColors = 65536;
        
        anTRed.resize(nColors,0);
        anTGreen.resize(nColors,0);
        anTBlue.resize(nColors,0);
        
        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            if( iColor < poColorTable->GetColorEntryCount() )
            {
                GDALColorEntry  sRGB;

                poColorTable->GetColorEntryAsRGB( iColor, &sRGB );

                anTRed[iColor] = (unsigned short) (256 * sRGB.c1);
                anTGreen[iColor] = (unsigned short) (256 * sRGB.c2);
                anTBlue[iColor] = (unsigned short) (256 * sRGB.c3);
            }
            else
            {
                anTRed[iColor] = anTGreen[iColor] = anTBlue[iColor] = 0;
            }
        }

        panRed = &(anTRed[0]);
        panGreen = &(anTGreen[0]);
        panBlue = &(anTBlue[0]);
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

    if( TIFFGetField( hTIFF, TIFFTAG_EXTRASAMPLES, &nExtraSamples, &panExtraSampleValues) )
    {
        uint16* panExtraSampleValuesNew = (uint16*) CPLMalloc(nExtraSamples * sizeof(uint16));
        memcpy(panExtraSampleValuesNew, panExtraSampleValues, nExtraSamples * sizeof(uint16));
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
    if ( nCompression == COMPRESSION_LZW ||
         nCompression == COMPRESSION_ADOBE_DEFLATE )
        TIFFGetField( hTIFF, TIFFTAG_PREDICTOR, &nPredictor );

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    int nOvrBlockXSize, nOvrBlockYSize;
    GTIFFGetOverviewBlockSize(&nOvrBlockXSize, &nOvrBlockYSize);
    for( i = 0; i < nOverviews && eErr == CE_None; i++ )
    {
        int   j;

        for( j = 0; j < nOverviewCount && eErr == CE_None; j++ )
        {
            int    nOvFactor;

            poODS = papoOverviewDS[j];

            nOvFactor = (int) 
                (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());

            if( nOvFactor == panOverviewList[i] 
                || nOvFactor == TIFF_OvLevelAdjust( panOverviewList[i],
                                                    GetRasterXSize() ) )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
        {
            toff_t	nOverviewOffset;
            int         nOXSize, nOYSize;

            nOXSize = (GetRasterXSize() + panOverviewList[i] - 1) 
                / panOverviewList[i];
            nOYSize = (GetRasterYSize() + panOverviewList[i] - 1)
                / panOverviewList[i];

            nOverviewOffset = 
                GTIFFWriteDirectory(hTIFF, FILETYPE_REDUCEDIMAGE,
                                    nOXSize, nOYSize, 
                                    nOvBitsPerSample, nPlanarConfig,
                                    nSamplesPerPixel, nOvrBlockXSize, nOvrBlockYSize, TRUE,
                                    nCompression, nPhotometric, nSampleFormat, 
                                    nPredictor,
                                    panRed, panGreen, panBlue,
                                    nExtraSamples, panExtraSampleValues,
                                    osMetadata );


            if( nOverviewOffset == 0 )
                eErr = CE_Failure;
            else
                eErr = RegisterNewOverviewDataset(nOverviewOffset);
        }
        else
            panOverviewList[i] *= -1;
    }

    CPLFree(panExtraSampleValues);
    panExtraSampleValues = NULL;

/* -------------------------------------------------------------------- */
/*      Create overviews for the mask.                                  */
/* -------------------------------------------------------------------- */
    if (eErr == CE_None)
        eErr = CreateInternalMaskOverviews(nOvrBlockXSize, nOvrBlockYSize);
    else
        return eErr;

/* -------------------------------------------------------------------- */
/*      Refresh overviews for the mask                                  */
/* -------------------------------------------------------------------- */
    if (poMaskDS != NULL &&
        poMaskDS->GetRasterCount() == 1)
    {
        GDALRasterBand **papoOverviewBands;
        int nMaskOverviews = 0;

        papoOverviewBands = (GDALRasterBand **) CPLCalloc(sizeof(void*),nOverviewCount);
        for( i = 0; i < nOverviewCount; i++ )
        {
            if (papoOverviewDS[i]->poMaskDS != NULL)
            {
                papoOverviewBands[nMaskOverviews ++] =
                        papoOverviewDS[i]->poMaskDS->GetRasterBand(1);
            }
        }
        eErr = GDALRegenerateOverviews( (GDALRasterBandH) 
                                        poMaskDS->GetRasterBand(1),
                                        nMaskOverviews, 
                                        (GDALRasterBandH *) papoOverviewBands,
                                        pszResampling, GDALDummyProgress, NULL);
        CPLFree(papoOverviewBands);
    }


/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
    if (nCompression != COMPRESSION_NONE &&
        nPlanarConfig == PLANARCONFIG_CONTIG &&
        GDALDataTypeIsComplex(GetRasterBand( panBandList[0] )->GetRasterDataType()) == FALSE &&
        GetRasterBand( panBandList[0] )->GetColorTable() == NULL &&
        (EQUALN(pszResampling, "NEAR", 4) || EQUAL(pszResampling, "AVERAGE") || EQUAL(pszResampling, "GAUSS")))
    {
        /* In the case of pixel interleaved compressed overviews, we want to generate */
        /* the overviews for all the bands block by block, and not band after band, */
        /* in order to write the block once and not loose space in the TIFF file */

        GDALRasterBand ***papapoOverviewBands;
        GDALRasterBand  **papoBandList;

        int nNewOverviews = 0;
        int iBand;

        papapoOverviewBands = (GDALRasterBand ***) CPLCalloc(sizeof(void*),nBands);
        papoBandList = (GDALRasterBand **) CPLCalloc(sizeof(void*),nBands);
        for( iBand = 0; iBand < nBands; iBand++ )
        {
            GDALRasterBand* poBand = GetRasterBand( panBandList[iBand] );

            papoBandList[iBand] = poBand;
            papapoOverviewBands[iBand] = (GDALRasterBand **) CPLCalloc(sizeof(void*), poBand->GetOverviewCount());

            int iCurOverview = 0;
            for( i = 0; i < nOverviews; i++ )
            {
                int   j;

                for( j = 0; j < poBand->GetOverviewCount(); j++ )
                {
                    int    nOvFactor;
                    GDALRasterBand * poOverview = poBand->GetOverview( j );

                    nOvFactor = (int) 
                    (0.5 + poBand->GetXSize() / (double) poOverview->GetXSize());

                    int bHasNoData;
                    double noDataValue = poBand->GetNoDataValue(&bHasNoData);

                    if (bHasNoData)
                        poOverview->SetNoDataValue(noDataValue);

                    if( nOvFactor == panOverviewList[i] 
                        || nOvFactor == TIFF_OvLevelAdjust( panOverviewList[i],
                                                            poBand->GetXSize() ) )
                    {
                        papapoOverviewBands[iBand][iCurOverview] = poOverview;
                        iCurOverview++ ;
                        break;
                    }
                }
            }

            if (nNewOverviews == 0)
                nNewOverviews = iCurOverview;
            else if (nNewOverviews != iCurOverview)
            {
                CPLAssert(0);
                return CE_Failure;
            }
        }

        GDALRegenerateOverviewsMultiBand(nBands, papoBandList,
                                         nNewOverviews, papapoOverviewBands,
                                         pszResampling, pfnProgress, pProgressData );

        for( iBand = 0; iBand < nBands; iBand++ )
        {
            CPLFree(papapoOverviewBands[iBand]);
        }
        CPLFree(papapoOverviewBands);
        CPLFree(papoBandList);
    }
    else
    {
        GDALRasterBand **papoOverviewBands;

        papoOverviewBands = (GDALRasterBand **) 
            CPLCalloc(sizeof(void*),nOverviews);

        for( int iBand = 0; iBand < nBands && eErr == CE_None; iBand++ )
        {
            GDALRasterBand *poBand;
            int            nNewOverviews;

            poBand = GetRasterBand( panBandList[iBand] );

            nNewOverviews = 0;
            for( i = 0; i < nOverviews && poBand != NULL; i++ )
            {
                int   j;

                for( j = 0; j < poBand->GetOverviewCount(); j++ )
                {
                    int    nOvFactor;
                    GDALRasterBand * poOverview = poBand->GetOverview( j );

                    int bHasNoData;
                    double noDataValue = poBand->GetNoDataValue(&bHasNoData);

                    if (bHasNoData)
                        poOverview->SetNoDataValue(noDataValue);

                    nOvFactor = (int) 
                    (0.5 + poBand->GetXSize() / (double) poOverview->GetXSize());

                    if( nOvFactor == panOverviewList[i] 
                        || nOvFactor == TIFF_OvLevelAdjust( panOverviewList[i],
                                                            poBand->GetXSize() ) )
                    {
                        papoOverviewBands[nNewOverviews++] = poOverview;
                        break;
                    }
                }
            }

            void         *pScaledProgressData;

            pScaledProgressData = 
                GDALCreateScaledProgress( iBand / (double) nBands, 
                                        (iBand+1) / (double) nBands,
                                        pfnProgress, pProgressData );

            eErr = GDALRegenerateOverviews( (GDALRasterBandH) poBand,
                                            nNewOverviews, 
                                            (GDALRasterBandH *) papoOverviewBands,
                                            pszResampling, 
                                            GDALScaledProgress, 
                                            pScaledProgressData);

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

static void GTiffWriteDummyGeokeyDirectory(TIFF* hTIFF)
{
    // If we have existing geokeys, try to wipe them
    // by writing a dummy geokey directory. (#2546)
    uint16 *panVI = NULL;
    uint16 nKeyCount;

    if( TIFFGetField( hTIFF, TIFFTAG_GEOKEYDIRECTORY, 
                        &nKeyCount, &panVI ) )
    {
        GUInt16 anGKVersionInfo[4] = { 1, 1, 0, 0 };
        double  adfDummyDoubleParams[1] = { 0.0 };
        TIFFSetField( hTIFF, TIFFTAG_GEOKEYDIRECTORY, 
                        4, anGKVersionInfo );
        TIFFSetField( hTIFF, TIFFTAG_GEODOUBLEPARAMS, 
                        1, adfDummyDoubleParams );
        TIFFSetField( hTIFF, TIFFTAG_GEOASCIIPARAMS, "" );
    }
}

/************************************************************************/
/*                          WriteGeoTIFFInfo()                          */
/************************************************************************/

void GTiffDataset::WriteGeoTIFFInfo()

{
    bool bPixelIsPoint = false;
    int  bPointGeoIgnore = FALSE;

    if( GetMetadataItem( GDALMD_AREA_OR_POINT ) 
        && EQUAL(GetMetadataItem(GDALMD_AREA_OR_POINT),
                 GDALMD_AOP_POINT) )
    {
        bPixelIsPoint = true;
        bPointGeoIgnore = 
            CSLTestBoolean( CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE",
                                               "FALSE") );
    }

    if( bForceUnsetGT )
    {
        bNeedsRewrite = TRUE;
        bForceUnsetGT = FALSE;

#ifdef HAVE_UNSETFIELD
        TIFFUnsetField( hTIFF, TIFFTAG_GEOPIXELSCALE );
        TIFFUnsetField( hTIFF, TIFFTAG_GEOTIEPOINTS );
        TIFFUnsetField( hTIFF, TIFFTAG_GEOTRANSMATRIX );
#endif
    }

    if( bForceUnsetProjection )
    {
        bNeedsRewrite = TRUE;
        bForceUnsetProjection = FALSE;

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
        || adfGeoTransform[4] != 0.0 || ABS(adfGeoTransform[5]) != 1.0 )
    {
        bNeedsRewrite = TRUE;

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
            double	adfPixelScale[3], adfTiePoints[6];

            adfPixelScale[0] = adfGeoTransform[1];
            adfPixelScale[1] = fabs(adfGeoTransform[5]);
            adfPixelScale[2] = 0.0;

            if( !EQUAL(osProfile,"BASELINE") )
                TIFFSetField( hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );

            adfTiePoints[0] = 0.0;
            adfTiePoints[1] = 0.0;
            adfTiePoints[2] = 0.0;
            adfTiePoints[3] = adfGeoTransform[0];
            adfTiePoints[4] = adfGeoTransform[3];
            adfTiePoints[5] = 0.0;

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                adfTiePoints[3] += adfGeoTransform[1] * 0.5 + adfGeoTransform[2] * 0.5;
                adfTiePoints[4] += adfGeoTransform[4] * 0.5 + adfGeoTransform[5] * 0.5;
            }

            if( !EQUAL(osProfile,"BASELINE") )
                TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
        }
        else
        {
            double	adfMatrix[16];

            memset(adfMatrix,0,sizeof(double) * 16);

            adfMatrix[0] = adfGeoTransform[1];
            adfMatrix[1] = adfGeoTransform[2];
            adfMatrix[3] = adfGeoTransform[0];
            adfMatrix[4] = adfGeoTransform[4];
            adfMatrix[5] = adfGeoTransform[5];
            adfMatrix[7] = adfGeoTransform[3];
            adfMatrix[15] = 1.0;

            if( bPixelIsPoint && !bPointGeoIgnore )
            {
                adfMatrix[3] += adfGeoTransform[1] * 0.5 + adfGeoTransform[2] * 0.5;
                adfMatrix[7] += adfGeoTransform[4] * 0.5 + adfGeoTransform[5] * 0.5;
            }

            if( !EQUAL(osProfile,"BASELINE") )
                TIFFSetField( hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix );
        }

        // Do we need a world file?
        if( CSLFetchBoolean( papszCreationOptions, "TFW", FALSE ) )
            GDALWriteWorldFile( osFilename, "tfw", adfGeoTransform );
        else if( CSLFetchBoolean( papszCreationOptions, "WORLDFILE", FALSE ) )
            GDALWriteWorldFile( osFilename, "wld", adfGeoTransform );
    }
    else if( GetGCPCount() > 0 )
    {
        double	*padfTiePoints;
        int		iGCP;

        bNeedsRewrite = TRUE;

        padfTiePoints = (double *)
            CPLMalloc( 6 * sizeof(double) * GetGCPCount() );

        for( iGCP = 0; iGCP < GetGCPCount(); iGCP++ )
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
/*	Write out projection definition.				*/
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL && !EQUAL( pszProjection, "" )
        && !EQUAL(osProfile,"BASELINE") )
    {
        GTIF	*psGTIF;

        bNeedsRewrite = TRUE;

        // If we have existing geokeys, try to wipe them
        // by writing a dummy geokey directory. (#2546)
        GTiffWriteDummyGeokeyDirectory(hTIFF);

        psGTIF = GTIFNew( hTIFF );  

        // set according to coordinate system.
        GTIFSetFromOGISDefn( psGTIF, pszProjection );

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
    char szBandId[32];
    CPLXMLNode *psItem;

/* -------------------------------------------------------------------- */
/*      Create the Item element, and subcomponents.                     */
/* -------------------------------------------------------------------- */
    psItem = CPLCreateXMLNode( NULL, CXT_Element, "Item" );
    CPLCreateXMLNode( CPLCreateXMLNode( psItem, CXT_Attribute, "name"),
                      CXT_Text, pszKey );

    if( nBand > 0 )
    {
        sprintf( szBandId, "%d", nBand - 1 );
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
/*                         WriteMDMetadata()                          */
/************************************************************************/

static void WriteMDMetadata( GDALMultiDomainMetadata *poMDMD, TIFF *hTIFF,
                             CPLXMLNode **ppsRoot, CPLXMLNode **ppsTail, 
                             int nBand, const char *pszProfile )

{
    int iDomain;
    char **papszDomainList;

    (void) pszProfile;

/* ==================================================================== */
/*      Process each domain.                                            */
/* ==================================================================== */
    papszDomainList = poMDMD->GetDomainList();
    for( iDomain = 0; papszDomainList && papszDomainList[iDomain]; iDomain++ )
    {
        char **papszMD = poMDMD->GetMetadata( papszDomainList[iDomain] );
        int iItem;
        int bIsXML = FALSE;

        if( EQUAL(papszDomainList[iDomain], "IMAGE_STRUCTURE") )
            continue; // ignored
        if( EQUAL(papszDomainList[iDomain], "COLOR_PROFILE") )
            continue; // ignored
        if( EQUAL(papszDomainList[iDomain], "RPC") )
            continue; // handled elsewhere
        if( EQUAL(papszDomainList[iDomain], "xml:ESRI") 
            && CSLTestBoolean(CPLGetConfigOption( "ESRI_XML_PAM", "NO" )) )
            continue; // handled elsewhere

        if( EQUALN(papszDomainList[iDomain], "xml:",4 ) )
            bIsXML = TRUE;

/* -------------------------------------------------------------------- */
/*      Process each item in this domain.                               */
/* -------------------------------------------------------------------- */
        for( iItem = 0; papszMD && papszMD[iItem]; iItem++ )
        {
            const char *pszItemValue;
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
                    CPLDebug("GTiff", "Invalid metadata item : %s", papszMD[iItem]);
                    continue;
                }
            }
            
/* -------------------------------------------------------------------- */
/*      Convert into XML item or handle as a special TIFF tag.          */
/* -------------------------------------------------------------------- */
            if( strlen(papszDomainList[iDomain]) == 0
                && nBand == 0 && EQUALN(pszItemName,"TIFFTAG_",8) )
            {
                if( EQUAL(pszItemName,"TIFFTAG_RESOLUTIONUNIT") ) {
                    /* ResolutionUnit can't be 0, which is the default if atoi() fails.
                       Set to 1=Unknown */
                    int v = atoi(pszItemValue);
                    if (!v) v = RESUNIT_NONE;
                    TIFFSetField( hTIFF, TIFFTAG_RESOLUTIONUNIT, v);
                }
                else
                {
                    int bFoundTag = FALSE;
                    size_t iTag;
                    for(iTag=0;iTag<sizeof(asTIFFTags)/sizeof(asTIFFTags[0]);iTag++)
                    {
                        if( EQUAL(pszItemName, asTIFFTags[iTag].pszTagName) )
                        {
                            bFoundTag = TRUE;
                            break;
                        }
                    }

                    if( bFoundTag && asTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING )
                        TIFFSetField( hTIFF, asTIFFTags[iTag].nTagVal, pszItemValue );
                    else if( bFoundTag && asTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT )
                        TIFFSetField( hTIFF, asTIFFTags[iTag].nTagVal, CPLAtof(pszItemValue) );
                    else if( bFoundTag && asTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT )
                        TIFFSetField( hTIFF, asTIFFTags[iTag].nTagVal, atoi(pszItemValue) );
                    else
                        CPLError(CE_Warning, CPLE_NotSupported,
                                "%s metadata item is unhandled and will not be written",
                                pszItemName);
                }
            }
            else if( nBand == 0 && EQUAL(pszItemName,GDALMD_AREA_OR_POINT) )
                /* do nothing, handled elsewhere */;
            else
                AppendMetadataItem( ppsRoot, ppsTail, 
                                    pszItemName, pszItemValue, 
                                    nBand, NULL, papszDomainList[iDomain] );

            CPLFree( pszItemName );
        }

/* -------------------------------------------------------------------- */
/*      Remove TIFFTAG_xxxxxx that are already set but no longer in     */
/*      the metadata list (#5619)                                       */
/* -------------------------------------------------------------------- */
        if( strlen(papszDomainList[iDomain]) == 0 && nBand == 0 )
        {
            size_t iTag;
            for(iTag=0;iTag<sizeof(asTIFFTags)/sizeof(asTIFFTags[0]);iTag++)
            {
                char* pszText = NULL;
                int16 nVal = 0;
                float fVal = 0.0f;
                const char* pszVal = CSLFetchNameValue(papszMD, asTIFFTags[iTag].pszTagName);
                if( pszVal == NULL &&
                    ((asTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING && TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &pszText )) ||
                     (asTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT && TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &nVal )) ||
                     (asTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT && TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &fVal ))) )
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
/*                           WriteMetadata()                            */
/************************************************************************/

int  GTiffDataset::WriteMetadata( GDALDataset *poSrcDS, TIFF *hTIFF,
                                  int bSrcIsGeoTIFF,
                                  const char *pszProfile,
                                  const char *pszTIFFFilename,
                                  char **papszCreationOptions,
                                  int bExcludeRPBandIMGFileWriting)

{
/* -------------------------------------------------------------------- */
/*      Convert all the remaining metadata into a simple XML            */
/*      format.                                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot = NULL, *psTail = NULL;

    if( bSrcIsGeoTIFF )
    {
        WriteMDMetadata( &(((GTiffDataset *)poSrcDS)->oGTiffMDMD), 
                         hTIFF, &psRoot, &psTail, 0, pszProfile );
    }
    else
    {
        char **papszMD = poSrcDS->GetMetadata();

        if( CSLCount(papszMD) > 0 )
        {
            GDALMultiDomainMetadata oMDMD;
            oMDMD.SetMetadata( papszMD );

            WriteMDMetadata( &oMDMD, hTIFF, &psRoot, &psTail, 0, pszProfile );
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle RPC data written to an RPB file.                         */
/* -------------------------------------------------------------------- */
    char **papszRPCMD = poSrcDS->GetMetadata("RPC");
    if( papszRPCMD != NULL && !bExcludeRPBandIMGFileWriting )
    {
        if( EQUAL(pszProfile,"GDALGeoTIFF") )
            WriteRPCTag( hTIFF, papszRPCMD );

        if( !EQUAL(pszProfile,"GDALGeoTIFF") 
            || CSLFetchBoolean( papszCreationOptions, "RPB", FALSE ) )
        {
            GDALWriteRPBFile( pszTIFFFilename, papszRPCMD );
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle metadata data written to an IMD file.                    */
/* -------------------------------------------------------------------- */
    char **papszIMDMD = poSrcDS->GetMetadata("IMD");
    if( papszIMDMD != NULL && !bExcludeRPBandIMGFileWriting)
    {
        GDALWriteIMDFile( pszTIFFFilename, papszIMDMD );
    }

/* -------------------------------------------------------------------- */
/*      We also need to address band specific metadata, and special     */
/*      "role" metadata.                                                */
/* -------------------------------------------------------------------- */
    int nBand;
    for( nBand = 1; nBand <= poSrcDS->GetRasterCount(); nBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( nBand );

        if( bSrcIsGeoTIFF )
        {
            WriteMDMetadata( &(((GTiffRasterBand *)poBand)->oGTiffMDMD), 
                             hTIFF, &psRoot, &psTail, nBand, pszProfile );
        }
        else
        {
            char **papszMD = poBand->GetMetadata();
            
            if( CSLCount(papszMD) > 0 )
            {
                GDALMultiDomainMetadata oMDMD;
                oMDMD.SetMetadata( papszMD );
                
                WriteMDMetadata( &oMDMD, hTIFF, &psRoot, &psTail, nBand,
                                 pszProfile );
            }
        }

        double dfOffset = poBand->GetOffset();
        double dfScale = poBand->GetScale();

        if( dfOffset != 0.0 || dfScale != 1.0 )
        {
            char szValue[128];

            sprintf( szValue, "%.18g", dfOffset );
            AppendMetadataItem( &psRoot, &psTail, "OFFSET", szValue, nBand, 
                                "offset", "" );
            sprintf( szValue, "%.18g", dfScale );
            AppendMetadataItem( &psRoot, &psTail, "SCALE", szValue, nBand, 
                                "scale", "" );
        }

        const char* pszUnitType = poBand->GetUnitType();
        if (pszUnitType != NULL && pszUnitType[0] != '\0')
            AppendMetadataItem( &psRoot, &psTail, "UNITTYPE", pszUnitType, nBand, 
                                "unittype", "" );


        if (strlen(poBand->GetDescription()) > 0) 
        {
            AppendMetadataItem( &psRoot, &psTail, "DESCRIPTION", 
                                poBand->GetDescription(), nBand, 
                                "description", "" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write out the generic XML metadata if there is any.             */
/* -------------------------------------------------------------------- */
    if( psRoot != NULL )
    {
        int bRet = TRUE;

        if( EQUAL(pszProfile,"GDALGeoTIFF") )
        {
            char *pszXML_MD = CPLSerializeXMLTree( psRoot );
            if( strlen(pszXML_MD) > 32000 )
            {
                if( bSrcIsGeoTIFF )
                    ((GTiffDataset *) poSrcDS)->PushMetadataToPam();
                else
                    bRet = FALSE;
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Lost metadata writing to GeoTIFF ... too large to fit in tag." );
            }
            else
            {
                TIFFSetField( hTIFF, TIFFTAG_GDAL_METADATA, pszXML_MD );
            }
            CPLFree( pszXML_MD );
        }
        else
        {
            if( bSrcIsGeoTIFF )
                ((GTiffDataset *) poSrcDS)->PushMetadataToPam();
            else
                bRet = FALSE;
        }

        CPLDestroyXMLNode( psRoot );

        return bRet;
    }
    else
    {
        /* If we have no more metadata but it existed before, remove the GDAL_METADATA tag */
        if( EQUAL(pszProfile,"GDALGeoTIFF") )
        {
            char* pszText = NULL;
            if( TIFFGetField( hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) )
            {
#ifdef HAVE_UNSETFIELD
                TIFFUnsetField( hTIFF, TIFFTAG_GDAL_METADATA );
#else
                TIFFSetField( hTIFF, TIFFTAG_GDAL_METADATA, "" );
#endif
            }
        }
    }

    return TRUE;
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
    int nBand;
    for( nBand = 0; nBand <= GetRasterCount(); nBand++ )
    {
        GDALMultiDomainMetadata *poSrcMDMD;
        GTiffRasterBand *poBand = NULL;

        if( nBand == 0 )
            poSrcMDMD = &(this->oGTiffMDMD);
        else
        {
            poBand = (GTiffRasterBand *) GetRasterBand(nBand);
            poSrcMDMD = &(poBand->oGTiffMDMD);
        }

/* -------------------------------------------------------------------- */
/*      Loop over the available domains.                                */
/* -------------------------------------------------------------------- */
        int iDomain, i;
        char **papszDomainList;

        papszDomainList = poSrcMDMD->GetDomainList();
        for( iDomain = 0; 
             papszDomainList && papszDomainList[iDomain]; 
             iDomain++ )
        {
            char **papszMD = poSrcMDMD->GetMetadata( papszDomainList[iDomain] );

            if( EQUAL(papszDomainList[iDomain],"RPC")
                || EQUAL(papszDomainList[iDomain],"IMD") 
                || EQUAL(papszDomainList[iDomain],"_temporary_")
                || EQUAL(papszDomainList[iDomain],"IMAGE_STRUCTURE")
                || EQUAL(papszDomainList[iDomain],"COLOR_PROFILE") )
                continue;

            papszMD = CSLDuplicate(papszMD);

            for( i = CSLCount(papszMD)-1; i >= 0; i-- )
            {
                if( EQUALN(papszMD[i],"TIFFTAG_",8)
                    || EQUALN(papszMD[i],GDALMD_AREA_OR_POINT,
                              strlen(GDALMD_AREA_OR_POINT)) )
                    papszMD = CSLRemoveStrings( papszMD, i, 1, NULL );
            }

            if( nBand == 0 )
                GDALPamDataset::SetMetadata( papszMD, papszDomainList[iDomain]);
            else
                poBand->GDALPamRasterBand::SetMetadata( papszMD, papszDomainList[iDomain]);

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
            poBand->GDALPamRasterBand::SetDescription( poBand->GetDescription() );
        }
    }
}

/************************************************************************/
/*                            WriteRPCTag()                             */
/*                                                                      */
/*      Format a TAG according to:                                      */
/*                                                                      */
/*      http://geotiff.maptools.org/rpc_prop.html                       */
/************************************************************************/

/* static */
void GTiffDataset::WriteRPCTag( TIFF *hTIFF, char **papszRPCMD )

{
    double adfRPCTag[92];
    GDALRPCInfo sRPC;

    if( !GDALExtractRPCInfo( papszRPCMD, &sRPC ) )
        return;

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

void GTiffDataset::ReadRPCTag()

{
    double *padfRPCTag;
    CPLString osField;
    CPLString osMultiField;
    CPLStringList asMD;
    int i;
    uint16 nCount;

    if( !TIFFGetField( hTIFF, TIFFTAG_RPCCOEFFICIENT, &nCount, &padfRPCTag ) 
        || nCount != 92 )
        return;

    asMD.SetNameValue("LINE_OFF", CPLOPrintf("%.15g", padfRPCTag[2]));
    asMD.SetNameValue("SAMP_OFF", CPLOPrintf("%.15g", padfRPCTag[3]));
    asMD.SetNameValue("LAT_OFF", CPLOPrintf("%.15g", padfRPCTag[4]));
    asMD.SetNameValue("LONG_OFF", CPLOPrintf("%.15g", padfRPCTag[5]));
    asMD.SetNameValue("HEIGHT_OFF", CPLOPrintf("%.15g", padfRPCTag[6]));
    asMD.SetNameValue("LINE_SCALE", CPLOPrintf("%.15g", padfRPCTag[7]));
    asMD.SetNameValue("SAMP_SCALE", CPLOPrintf("%.15g", padfRPCTag[8]));
    asMD.SetNameValue("LAT_SCALE", CPLOPrintf("%.15g", padfRPCTag[9]));
    asMD.SetNameValue("LONG_SCALE", CPLOPrintf("%.15g", padfRPCTag[10]));
    asMD.SetNameValue("HEIGHT_SCALE", CPLOPrintf("%.15g", padfRPCTag[11]));

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", padfRPCTag[12+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue("LINE_NUM_COEFF", osMultiField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", padfRPCTag[32+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue( "LINE_DEN_COEFF", osMultiField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", padfRPCTag[52+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue( "SAMP_NUM_COEFF", osMultiField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", padfRPCTag[72+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue( "SAMP_DEN_COEFF", osMultiField );

    oGTiffMDMD.SetMetadata( asMD, "RPC" );
}

/************************************************************************/
/*                         WriteNoDataValue()                           */
/************************************************************************/

void GTiffDataset::WriteNoDataValue( TIFF *hTIFF, double dfNoData )

{
    char szVal[400];
    if (CPLIsNan(dfNoData))
        strcpy(szVal, "nan");
	else
        snprintf(szVal, sizeof(szVal), "%.18g", dfNoData);
    TIFFSetField( hTIFF, TIFFTAG_GDAL_NODATA, szVal );
}

/************************************************************************/
/*                            SetDirectory()                            */
/************************************************************************/

int GTiffDataset::SetDirectory( toff_t nNewOffset )

{
    Crystalize();

    FlushBlockBuf();

    if( nNewOffset == 0 )
        nNewOffset = nDirOffset;

    if( TIFFCurrentDirOffset(hTIFF) == nNewOffset )
    {
        CPLAssert( *ppoActiveDSRef == this || *ppoActiveDSRef == NULL );
        *ppoActiveDSRef = this;
        return TRUE;
    }

    if( GetAccess() == GA_Update )
    {
        if( *ppoActiveDSRef != NULL )
            (*ppoActiveDSRef)->FlushDirectory();
    }
    
    if( nNewOffset == 0)
        return TRUE;

    (*ppoActiveDSRef) = this;

    int nSetDirResult = TIFFSetSubDirectory( hTIFF, nNewOffset );
    if (!nSetDirResult)
        return nSetDirResult;

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
        && CSLTestBoolean( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                              "YES") ) )
    {
        int nColorMode;

        TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode );
        if( nColorMode != JPEGCOLORMODE_RGB )
            TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
    }

/* -------------------------------------------------------------------- */
/*      Propogate any quality settings.                                 */
/* -------------------------------------------------------------------- */
    if( GetAccess() == GA_Update )
    {
        // Now, reset zip and jpeg quality.
        if(nJpegQuality > 0 && nCompression == COMPRESSION_JPEG)
        {
            CPLDebug( "GTiff", "Propgate JPEG_QUALITY(%d) in SetDirectory()",
                      nJpegQuality );
            TIFFSetField(hTIFF, TIFFTAG_JPEGQUALITY, nJpegQuality); 
        }
        if(nZLevel > 0 && nCompression == COMPRESSION_ADOBE_DEFLATE)
            TIFFSetField(hTIFF, TIFFTAG_ZIPQUALITY, nZLevel);
        if(nLZMAPreset > 0 && nCompression == COMPRESSION_LZMA)
            TIFFSetField(hTIFF, TIFFTAG_LZMAPRESET, nLZMAPreset);
    }

    return nSetDirResult;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int GTiffDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    const char  *pszFilename = poOpenInfo->pszFilename;
    if( EQUALN(pszFilename,"GTIFF_RAW:", strlen("GTIFF_RAW:")) )
    {
        pszFilename += strlen("GTIFF_RAW:");
        GDALOpenInfo oOpenInfo( pszFilename, poOpenInfo->eAccess );
        return Identify(&oOpenInfo);
    }

/* -------------------------------------------------------------------- */
/*      We have a special hook for handling opening a specific          */
/*      directory of a TIFF file.                                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszFilename,"GTIFF_DIR:",strlen("GTIFF_DIR:")) )
        return TRUE;

/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fpL == NULL || poOpenInfo->nHeaderBytes < 2 )
        return FALSE;

    if( (poOpenInfo->pabyHeader[0] != 'I' || poOpenInfo->pabyHeader[1] != 'I')
        && (poOpenInfo->pabyHeader[0] != 'M' || poOpenInfo->pabyHeader[1] != 'M'))
        return FALSE;

#ifndef BIGTIFF_SUPPORT
    if( (poOpenInfo->pabyHeader[2] == 0x2B && poOpenInfo->pabyHeader[3] == 0) ||
        (poOpenInfo->pabyHeader[2] == 0 && poOpenInfo->pabyHeader[3] == 0x2B) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "This is a BigTIFF file.  BigTIFF is not supported by this\n"
                  "version of GDAL and libtiff." );
        return FALSE;
    }
#endif

    if( (poOpenInfo->pabyHeader[2] != 0x2A || poOpenInfo->pabyHeader[3] != 0)
        && (poOpenInfo->pabyHeader[3] != 0x2A || poOpenInfo->pabyHeader[2] != 0)
        && (poOpenInfo->pabyHeader[2] != 0x2B || poOpenInfo->pabyHeader[3] != 0)
        && (poOpenInfo->pabyHeader[3] != 0x2B || poOpenInfo->pabyHeader[2] != 0))
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                            GTIFFErrorHandler()                       */
/************************************************************************/

class GTIFFErrorStruct
{
public:
    CPLErr type;
    int    no;
    CPLString msg;
    
        GTIFFErrorStruct() {}
        GTIFFErrorStruct(CPLErr eErr, int no, const char* msg) :
            type(eErr), no(no), msg(msg) {}
};

static void CPL_STDCALL GTIFFErrorHandler(CPLErr eErr, int no, const char* msg)
{
    std::vector<GTIFFErrorStruct>* paoErrors =
        (std::vector<GTIFFErrorStruct>*) CPLGetErrorHandlerUserData();
    paoErrors->push_back(GTIFFErrorStruct(eErr, no, msg));
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTiffDataset::Open( GDALOpenInfo * poOpenInfo )

{
    TIFF	*hTIFF;
    int          bAllowRGBAInterface = TRUE;
    const char  *pszFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Check if it looks like a TIFF file.                             */
/* -------------------------------------------------------------------- */
    if (!Identify(poOpenInfo))
        return NULL;

    if( EQUALN(pszFilename,"GTIFF_RAW:", strlen("GTIFF_RAW:")) )
    {
        bAllowRGBAInterface = FALSE;
        pszFilename +=  strlen("GTIFF_RAW:");
    }

/* -------------------------------------------------------------------- */
/*      We have a special hook for handling opening a specific          */
/*      directory of a TIFF file.                                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszFilename,"GTIFF_DIR:",strlen("GTIFF_DIR:")) )
        return OpenDir( poOpenInfo );

    if (!GTiffOneTimeInit())
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */

    /* Disable strip chop for now */
    if( poOpenInfo->fpL == NULL )
    {
        poOpenInfo->fpL = VSIFOpenL( pszFilename, ( poOpenInfo->eAccess == GA_ReadOnly ) ? "rb" : "r+b" );
        if( poOpenInfo->fpL == NULL )
            return NULL;
    }
    
    /* Store errors/warnings and emit them later */
    std::vector<GTIFFErrorStruct> aoErrors;
    CPLPushErrorHandlerEx(GTIFFErrorHandler, &aoErrors);
    hTIFF = VSI_TIFFOpen( pszFilename, ( poOpenInfo->eAccess == GA_ReadOnly ) ? "rc" : "r+c",
                          poOpenInfo->fpL );
    CPLPopErrorHandler();
#if SIZEOF_VOIDP == 4
    if( hTIFF == NULL )
    {
        /* Case of one-strip file where the strip size is > 2GB (#5403) */
        if( bGlobalStripIntegerOverflow )
        {
            hTIFF = VSI_TIFFOpen( pszFilename, ( poOpenInfo->eAccess == GA_ReadOnly ) ? "r" : "r+",
                                  poOpenInfo->fpL );
            bGlobalStripIntegerOverflow = FALSE;
        }
    }
    else
    {
        bGlobalStripIntegerOverflow = FALSE;
    }
#endif

    /* Now emit errors and change their criticality if needed */
    /* We only emit failures if we didn't manage to open the file */
    /* Otherwise it make Python bindings unhappy (#5616) */
    for(size_t iError=0;iError<aoErrors.size();iError++)
    {
        CPLError( (hTIFF == NULL && aoErrors[iError].type == CE_Failure) ? CE_Failure : CE_Warning,
                  aoErrors[iError].no,
                  "%s",
                  aoErrors[iError].msg.c_str() );
    }
    aoErrors.resize(0);

    if( hTIFF == NULL )
        return( NULL );

    uint32  nXSize, nYSize;
    uint16  nPlanarConfig;
    uint32  nRowsPerStrip;
    uint16  nCompression;

    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );

    if( nXSize > INT_MAX || nYSize > INT_MAX )
    {
        /* GDAL only supports signed 32bit dimensions */
        XTIFFClose( hTIFF );
        return( NULL );
    }

    if( !TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &(nPlanarConfig) ) )
        nPlanarConfig = PLANARCONFIG_CONTIG;

    if( !TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &(nCompression) ) )
        nCompression = COMPRESSION_NONE;

    if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP, &(nRowsPerStrip) ) )
        nRowsPerStrip = nYSize;

    if (!TIFFIsTiled( hTIFF ) &&
        nCompression == COMPRESSION_NONE &&
        nRowsPerStrip >= nYSize &&
        nPlanarConfig == PLANARCONFIG_CONTIG)
    {
        int bReopenWithStripChop = TRUE;
        if ( nYSize > 128 * 1024 * 1024 )
        {
            uint16  nSamplesPerPixel;
            uint16  nBitsPerSample;

            if( !TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSamplesPerPixel ) )
                nSamplesPerPixel = 1;

            if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(nBitsPerSample)) )
                nBitsPerSample = 1;

            vsi_l_offset nLineSize = (nSamplesPerPixel * (vsi_l_offset)nXSize * nBitsPerSample + 7) / 8;
            int nDefaultStripHeight = (int)(8192 / nLineSize);
            if (nDefaultStripHeight == 0) nDefaultStripHeight = 1;
            vsi_l_offset nStrips = nYSize / nDefaultStripHeight;

            /* There is a risk of DoS due to huge amount of memory allocated in ChopUpSingleUncompressedStrip() */
            /* in libtiff */
            if (nStrips > 128 * 1024 * 1024 &&
                !CSLTestBoolean(CPLGetConfigOption("GTIFF_FORCE_STRIP_CHOP", "NO")))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Potential denial of service detected. Avoid using strip chop. "
                        "Set the GTIFF_FORCE_STRIP_CHOP configuration open to go over this test.");
                bReopenWithStripChop = FALSE;
            }
        }

        if (bReopenWithStripChop)
        {
            CPLDebug("GTiff", "Reopen with strip chop enabled");
            XTIFFClose(hTIFF);
            hTIFF = VSI_TIFFOpen( pszFilename, ( poOpenInfo->eAccess == GA_ReadOnly ) ? "r" : "r+",
                                  poOpenInfo->fpL );
            if( hTIFF == NULL )
                return( NULL );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset 	*poDS;

    poDS = new GTiffDataset();
    poDS->SetDescription( pszFilename );
    poDS->osFilename = pszFilename;
    poDS->poActiveDS = poDS;
    poDS->fpL = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;

    if( poDS->OpenOffset( hTIFF, &(poDS->poActiveDS),
                          TIFFCurrentDirOffset(hTIFF), TRUE,
                          poOpenInfo->eAccess, 
                          bAllowRGBAInterface, TRUE,
                          poOpenInfo->GetSiblingFiles()) != CE_None )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML( poOpenInfo->GetSiblingFiles() );
    poDS->ApplyPamInfo();

    int i;
    for(i=1;i<=poDS->nBands;i++)
    {
        GTiffRasterBand* poBand = (GTiffRasterBand*) poDS->GetRasterBand(i);

        /* Load scale, offset and unittype from PAM if available */
        if (!poBand->bHaveOffsetScale)
        {
            poBand->dfScale = poBand->GDALPamRasterBand::GetScale(&poBand->bHaveOffsetScale);
            poBand->dfOffset = poBand->GDALPamRasterBand::GetOffset();
        }
        if (poBand->osUnitType.size() == 0)
        {
            const char* pszUnitType = poBand->GDALPamRasterBand::GetUnitType();
            if (pszUnitType)
                poBand->osUnitType = pszUnitType;
        }

        GDALColorInterp ePAMColorInterp = poBand->GDALPamRasterBand::GetColorInterpretation();
        if( ePAMColorInterp != GCI_Undefined )
            poBand->eBandInterp = ePAMColorInterp;
    }

    poDS->bColorProfileMetadataChanged = FALSE;
    poDS->bMetadataChanged = FALSE;
    poDS->bGeoTIFFInfoChanged = FALSE;
    poDS->bForceUnsetGT = FALSE;
    poDS->bForceUnsetProjection = FALSE;

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, pszFilename, poOpenInfo->GetSiblingFiles() );
    
    return poDS;
}

/************************************************************************/
/*                         LoadMDAreaOrPoint()                          */
/************************************************************************/

/* This is a light version of LookForProjection(), which saves the */
/* potential costly cost of GTIFGetOGISDefn(), since we just need to */
/* access to a raw GeoTIFF key, and not build the full projection object. */

void GTiffDataset::LoadMDAreaOrPoint()
{
    if( bLookedForProjection || bLookedForMDAreaOrPoint ||
        oGTiffMDMD.GetMetadataItem( GDALMD_AREA_OR_POINT ) != NULL )
        return;

    bLookedForMDAreaOrPoint = TRUE;

    if (!SetDirectory())
        return;

    GTIF* hGTIF = GTIFNew(hTIFF);

    if ( !hGTIF )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "GeoTIFF tags apparently corrupt, they are being ignored." );
    }
    else
    {
        // Is this a pixel-is-point dataset?
        short nRasterType;

        if( GTIFKeyGet(hGTIF, GTRasterTypeGeoKey, &nRasterType,
                       0, 1 ) == 1 )
        {
            if( nRasterType == (short) RasterPixelIsPoint )
                oGTiffMDMD.SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );
            else
                oGTiffMDMD.SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_AREA );
        }

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

    bLookedForProjection = TRUE;
    if (!SetDirectory())
        return;

/* -------------------------------------------------------------------- */
/*      Capture the GeoTIFF projection, if available.                   */
/* -------------------------------------------------------------------- */
    GTIF 	*hGTIF;

    CPLFree( pszProjection );
    pszProjection = NULL;
    
    hGTIF = GTIFNew(hTIFF);

    if ( !hGTIF )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "GeoTIFF tags apparently corrupt, they are being ignored." );
    }
    else
    {
        GTIFDefn      *psGTIFDefn;

#if LIBGEOTIFF_VERSION >= 1410
        psGTIFDefn = GTIFAllocDefn();
#else
        psGTIFDefn = (GTIFDefn *) CPLCalloc(1,sizeof(GTIFDefn));
#endif    

        if( GTIFGetDefn( hGTIF, psGTIFDefn ) )
        {
            pszProjection = GTIFGetOGISDefn( hGTIF, psGTIFDefn );
            
            // Should we simplify away vertical CS stuff?
            if( EQUALN(pszProjection,"COMPD_CS",8)
                && !CSLTestBoolean( CPLGetConfigOption("GTIFF_REPORT_COMPD_CS",
                                                       "NO") ) )
            {
                OGRSpatialReference oSRS;

                CPLDebug( "GTiff", "Got COMPD_CS, but stripping it." );
                char *pszWKT = pszProjection;
                oSRS.importFromWkt( &pszWKT );
                CPLFree( pszProjection );

                oSRS.StripVertical();
                oSRS.exportToWkt( &pszProjection );
            }
        }

        // Is this a pixel-is-point dataset?
        short nRasterType;

        // check the tif linear unit and the CS linear unit 
#ifdef ESRI_BUILD
        AdjustLinearUnit(psGTIFDefn.UOMLength); 
#endif

#if LIBGEOTIFF_VERSION >= 1410
        GTIFFreeDefn(psGTIFDefn);
#else
        CPLFree(psGTIFDefn);
#endif

        if( GTIFKeyGet(hGTIF, GTRasterTypeGeoKey, &nRasterType, 
                       0, 1 ) == 1 )
        {
            if( nRasterType == (short) RasterPixelIsPoint )
                oGTiffMDMD.SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );
            else
                oGTiffMDMD.SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_AREA );
        }

        GTIFFree( hGTIF );
    }

    if( pszProjection == NULL )
    {
        pszProjection = CPLStrdup( "" );
    }

    bGeoTIFFInfoChanged = FALSE;
    bForceUnsetGT = FALSE;
    bForceUnsetProjection = FALSE;
}

/************************************************************************/
/*                          AdjustLinearUnit()                          */
/*                                                                      */
/*      The following code is only used in ESRI Builds and there is     */
/*      outstanding discussion on whether it is even appropriate        */
/*      then.                                                           */
/************************************************************************/
#ifdef ESRI_BUILD

void GTiffDataset::AdjustLinearUnit(short UOMLength)
{
    if (!pszProjection || strlen(pszProjection) == 0)
        return;
    if( UOMLength == 9001)
    {
        char* pstr = strstr(pszProjection, "PARAMETER");
        if (!pstr)
            return;
        pstr = strstr(pstr, "UNIT[");
        if (!pstr)
            return;
        pstr = strchr(pstr, ',') + 1;
        if (!pstr)
            return;
        char* pstr1 = strchr(pstr, ']');
        if (!pstr1 || pstr1 - pstr >= 128)
            return;
        char csUnitStr[128];
        strncpy(csUnitStr, pstr, pstr1-pstr);
        csUnitStr[pstr1-pstr] = '\0';
        double csUnit = CPLAtof(csUnitStr);
        if(fabs(csUnit - 1.0) > 0.000001)
        {
            for(long i=0; i<6; i++)
                adfGeoTransform[i] /= csUnit;
        }
    }
}

#endif /* def ESRI_BUILD */

/************************************************************************/
/*                            ApplyPamInfo()                            */
/*                                                                      */
/*      PAM Information, if available, overrides the GeoTIFF            */
/*      geotransform and projection definition.  Check for them         */
/*      now.                                                            */
/************************************************************************/

void GTiffDataset::ApplyPamInfo()

{
    double adfPamGeoTransform[6];

    if( GDALPamDataset::GetGeoTransform( adfPamGeoTransform ) == CE_None 
        && (adfPamGeoTransform[0] != 0.0 || adfPamGeoTransform[1] != 1.0
            || adfPamGeoTransform[2] != 0.0 || adfPamGeoTransform[3] != 0.0
            || adfPamGeoTransform[4] != 0.0 || adfPamGeoTransform[5] != 1.0 ))
    {
        memcpy( adfGeoTransform, adfPamGeoTransform, sizeof(double)*6 );
        bGeoTransformValid = TRUE;
    }

    const char *pszPamSRS = GDALPamDataset::GetProjectionRef();

    if( pszPamSRS != NULL && strlen(pszPamSRS) > 0 )
    {
        CPLFree( pszProjection );
        pszProjection = CPLStrdup( pszPamSRS );
        bLookedForProjection = TRUE;
    }

    int nPamGCPCount = GDALPamDataset::GetGCPCount();
    if( nPamGCPCount > 0 )
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

        const char *pszPamGCPProjection = GDALPamDataset::GetGCPProjection();
        if( pszPamGCPProjection != NULL && strlen(pszPamGCPProjection) > 0 )
            pszProjection = CPLStrdup(pszPamGCPProjection);

        bLookedForProjection = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Copy any PAM metadata into our GeoTIFF context, and with        */
/*      the PAM info overriding the GeoTIFF context.                    */
/* -------------------------------------------------------------------- */
    char **papszPamDomains = oMDMD.GetDomainList();

    for( int iDomain = 0; papszPamDomains && papszPamDomains[iDomain] != NULL; iDomain++ )
    {
        const char *pszDomain = papszPamDomains[iDomain];
        char **papszGT_MD = CSLDuplicate(oGTiffMDMD.GetMetadata( pszDomain ));
        char **papszPAM_MD = oMDMD.GetMetadata( pszDomain );

        papszGT_MD = CSLMerge( papszGT_MD, papszPAM_MD );

        oGTiffMDMD.SetMetadata( papszGT_MD, pszDomain );
        CSLDestroy( papszGT_MD );
    }

    for( int i = 1; i <= GetRasterCount(); i++)
    {
        GTiffRasterBand* poBand = (GTiffRasterBand *)GetRasterBand(i);
        papszPamDomains = poBand->oMDMD.GetDomainList();

        for( int iDomain = 0; papszPamDomains && papszPamDomains[iDomain] != NULL; iDomain++ )
        {
            const char *pszDomain = papszPamDomains[iDomain];
            char **papszGT_MD = CSLDuplicate(poBand->oGTiffMDMD.GetMetadata( pszDomain ));
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
    int bAllowRGBAInterface = TRUE;
    const char* pszFilename = poOpenInfo->pszFilename;
    if( EQUALN(pszFilename,"GTIFF_RAW:", strlen("GTIFF_RAW:")) )
    {
        bAllowRGBAInterface = FALSE;
        pszFilename += strlen("GTIFF_RAW:");
    }

    if( !EQUALN(pszFilename,"GTIFF_DIR:",strlen("GTIFF_DIR:")) )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Split out filename, and dir#/offset.                            */
/* -------------------------------------------------------------------- */
    pszFilename += strlen("GTIFF_DIR:");
    int        bAbsolute = FALSE;
    toff_t     nOffset;
    
    if( EQUALN(pszFilename,"off:",4) )
    {
        bAbsolute = TRUE;
        pszFilename += 4;
    }

    nOffset = atol(pszFilename);
    pszFilename += 1;

    while( *pszFilename != '\0' && pszFilename[-1] != ':' )
        pszFilename++;

    if( *pszFilename == '\0' || nOffset == 0 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to extract offset or filename, should take the form\n"
                  "GTIFF_DIR:<dir>:filename or GTIFF_DIR:off:<dir_offset>:filename" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    TIFF	*hTIFF;

    if (!GTiffOneTimeInit())
        return NULL;

    VSILFILE* fpL = VSIFOpenL(pszFilename, "r");
    if( fpL == NULL )
        return NULL;
    hTIFF = VSI_TIFFOpen( pszFilename, "r", fpL );
    if( hTIFF == NULL )
    {
        VSIFCloseL(fpL);
        return( NULL );
    }

/* -------------------------------------------------------------------- */
/*      If a directory was requested by index, advance to it now.       */
/* -------------------------------------------------------------------- */
    if( !bAbsolute )
    {
        toff_t nOffsetRequested = nOffset;
        while( nOffset > 1 )
        {
            if( TIFFReadDirectory( hTIFF ) == 0 )
            {
                XTIFFClose( hTIFF );
                CPLError( CE_Failure, CPLE_OpenFailed, 
                          "Requested directory %lu not found.", (long unsigned int)nOffsetRequested );
                VSIFCloseL(fpL);
                return NULL;
            }
            nOffset--;
        }

        nOffset = TIFFCurrentDirOffset( hTIFF );
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset 	*poDS;

    poDS = new GTiffDataset();
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->osFilename = poOpenInfo->pszFilename;
    poDS->poActiveDS = poDS;
    poDS->fpL = fpL;

    if( !EQUAL(pszFilename,poOpenInfo->pszFilename) 
        && !EQUALN(poOpenInfo->pszFilename,"GTIFF_RAW:",10) )
    {
        poDS->SetPhysicalFilename( pszFilename );
        poDS->SetSubdatasetName( poOpenInfo->pszFilename );
        poDS->osFilename = pszFilename;
    }

    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Opening a specific TIFF directory is not supported in update mode. Switching to read-only" );
    }

    if( poDS->OpenOffset( hTIFF, &(poDS->poActiveDS),
                          nOffset, FALSE, GA_ReadOnly,
                          bAllowRGBAInterface, TRUE,
                          poOpenInfo->GetSiblingFiles() ) != CE_None )
    {
        delete poDS;
        return NULL;
    }
    else
    {
        poDS->bCloseTIFFHandle = TRUE;
        return poDS;
    }
}

/************************************************************************/
/*                   ConvertTransferFunctionToString()                  */
/*                                                                      */
/*      Convert a transfer function table into a string.                */
/*      Used by LoadICCProfile().                                       */
/************************************************************************/
static CPLString ConvertTransferFunctionToString( const uint16 *pTable, uint32 nTableEntries )
{
    CPLString sValue;

    for(uint32 i = 0; i < nTableEntries; i++)
    {
        if (i == 0)
            sValue = sValue.Printf("%d", (uint32)pTable[i]);
        else
        sValue = sValue.Printf("%s, %d", (const char*)sValue, (uint32)pTable[i]);
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
    uint32 nEmbedLen;
    uint8* pEmbedBuffer;
    float* pCHR;
    float* pWP;
    uint16 *pTFR, *pTFG, *pTFB;
    uint16 *pTransferRange = NULL;
    const int TIFFTAG_TRANSFERRANGE = 0x0156;

    if (bICCMetadataLoaded)
        return;
    bICCMetadataLoaded = TRUE;

    if (!SetDirectory())
        return;

    if (TIFFGetField(hTIFF, TIFFTAG_ICCPROFILE, &nEmbedLen, &pEmbedBuffer)) 
    {
        char *pszBase64Profile = CPLBase64Encode(nEmbedLen, (const GByte*)pEmbedBuffer);

        oGTiffMDMD.SetMetadataItem( "SOURCE_ICC_PROFILE", pszBase64Profile, "COLOR_PROFILE" );

        CPLFree(pszBase64Profile);

        return;
    }

    /* Check for colorimetric tiff */
    if (TIFFGetField(hTIFF, TIFFTAG_PRIMARYCHROMATICITIES, &pCHR)) 
    {
        if (TIFFGetField(hTIFF, TIFFTAG_WHITEPOINT, &pWP)) 
        {
            if (!TIFFGetFieldDefaulted(hTIFF, TIFFTAG_TRANSFERFUNCTION, &pTFR, &pTFG, &pTFB))
                return;

            TIFFGetFieldDefaulted(hTIFF, TIFFTAG_TRANSFERRANGE, &pTransferRange);

            // Set all the colorimetric metadata.
            oGTiffMDMD.SetMetadataItem( "SOURCE_PRIMARIES_RED", 
                CPLString().Printf( "%.9f, %.9f, 1.0", (double)pCHR[0], (double)pCHR[1] ) , "COLOR_PROFILE" );
            oGTiffMDMD.SetMetadataItem( "SOURCE_PRIMARIES_GREEN", 
                CPLString().Printf( "%.9f, %.9f, 1.0", (double)pCHR[2], (double)pCHR[3] ) , "COLOR_PROFILE" );
            oGTiffMDMD.SetMetadataItem( "SOURCE_PRIMARIES_BLUE", 
                CPLString().Printf( "%.9f, %.9f, 1.0", (double)pCHR[4], (double)pCHR[5] ) , "COLOR_PROFILE" );

            oGTiffMDMD.SetMetadataItem( "SOURCE_WHITEPOINT", 
                CPLString().Printf( "%.9f, %.9f, 1.0", (double)pWP[0], (double)pWP[1] ) , "COLOR_PROFILE" );

            /* Set transfer function metadata */

            /* Get length of table. */
            const uint32 nTransferFunctionLength = 1 << nBitsPerSample;

            oGTiffMDMD.SetMetadataItem( "TIFFTAG_TRANSFERFUNCTION_RED", 
                ConvertTransferFunctionToString( pTFR, nTransferFunctionLength), "COLOR_PROFILE" );

            oGTiffMDMD.SetMetadataItem( "TIFFTAG_TRANSFERFUNCTION_GREEN", 
                ConvertTransferFunctionToString( pTFG, nTransferFunctionLength), "COLOR_PROFILE" );

            oGTiffMDMD.SetMetadataItem( "TIFFTAG_TRANSFERFUNCTION_BLUE", 
                ConvertTransferFunctionToString( pTFB, nTransferFunctionLength), "COLOR_PROFILE" );

            /* Set transfer range */
            if (pTransferRange)
            {
                oGTiffMDMD.SetMetadataItem( "TIFFTAG_TRANSFERRANGE_BLACK",
                    CPLString().Printf( "%d, %d, %d", 
                        (int)pTransferRange[0], (int)pTransferRange[2], (int)pTransferRange[4]), "COLOR_PROFILE" );
                oGTiffMDMD.SetMetadataItem( "TIFFTAG_TRANSFERRANGE_WHITE",
                    CPLString().Printf( "%d, %d, %d", 
                        (int)pTransferRange[1], (int)pTransferRange[3], (int)pTransferRange[5]), "COLOR_PROFILE" );
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

void GTiffDataset::SaveICCProfile(GTiffDataset *pDS, TIFF *hTIFF, char **papszParmList, uint32 nBitsPerSample)
{
    if ((pDS != NULL) && (pDS->eAccess != GA_Update))
        return;

    if (hTIFF == NULL)
    {
        if (pDS == NULL)
            return;

        hTIFF = pDS->hTIFF;
        if (hTIFF == NULL)
            return;
    }

    if ((papszParmList == NULL) && (pDS == NULL))
        return;

    const char *pszValue = NULL;
    if (pDS != NULL)
        pszValue = pDS->GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE");
    else
        pszValue = CSLFetchNameValue(papszParmList, "SOURCE_ICC_PROFILE");
    if( pszValue != NULL )
    {
        int32 nEmbedLen;
        char *pEmbedBuffer = CPLStrdup(pszValue);
        nEmbedLen = CPLBase64DecodeInPlace((GByte*)pEmbedBuffer);

        TIFFSetField(hTIFF, TIFFTAG_ICCPROFILE, nEmbedLen, pEmbedBuffer);

        CPLFree(pEmbedBuffer);        
    }
    else
    {
        /* Output colorimetric data. */
        const int TIFFTAG_TRANSFERRANGE = 0x0156;

        float pCHR[6]; // Primaries
        float pWP[2];  // Whitepoint
        uint16 pTXR[6]; // Transfer range
        const char* pszCHRNames[] = {
            "SOURCE_PRIMARIES_RED",
            "SOURCE_PRIMARIES_GREEN",
            "SOURCE_PRIMARIES_BLUE"
        };
        const char* pszTXRNames[] = {
            "TIFFTAG_TRANSFERRANGE_BLACK",
            "TIFFTAG_TRANSFERRANGE_WHITE"
        };

        /* Output chromacities */
        bool bOutputCHR = true;
        for(int i = 0; ((i < 3) && bOutputCHR); i++)
        {
            if (pDS != NULL)
                pszValue = pDS->GetMetadataItem(pszCHRNames[i], "COLOR_PROFILE");
            else
                pszValue = CSLFetchNameValue(papszParmList, pszCHRNames[i]);
            if (pszValue == NULL)
            {
                bOutputCHR = false;
                break;
            }

            char** papszTokens = CSLTokenizeString2( pszValue, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );

            if (CSLCount( papszTokens ) != 3)
            {
                bOutputCHR = false;
                CSLDestroy( papszTokens );
                break;
            }

            int j;
            for( j = 0; j < 3; j++ )
            {
                float v = (float)atof(papszTokens[j]);

                if (j == 2)
                {
                    /* Last term of xyY color must be 1.0 */
                    if (v != 1.0)
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
        
        if (bOutputCHR)
        {
            TIFFSetField(hTIFF, TIFFTAG_PRIMARYCHROMATICITIES, pCHR);
        }

        /* Output whitepoint */
        bool bOutputWhitepoint = true;
        if (pDS != NULL)
            pszValue = pDS->GetMetadataItem("SOURCE_WHITEPOINT", "COLOR_PROFILE");
        else
            pszValue = CSLFetchNameValue(papszParmList, "SOURCE_WHITEPOINT");
        if (pszValue != NULL)
        {
            char** papszTokens = CSLTokenizeString2( pszValue, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );

            if (CSLCount( papszTokens ) != 3)
            {
                bOutputWhitepoint = false;
            }
            else
            {
                int j;
                for( j = 0; j < 3; j++ )
                {
                    float v = (float)atof(papszTokens[j]);

                    if (j == 2)
                    {
                        /* Last term of xyY color must be 1.0 */
                        if (v != 1.0)
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

            if (bOutputWhitepoint)
            {
                TIFFSetField(hTIFF, TIFFTAG_WHITEPOINT, pWP);
            }
        }
        
        /* Set transfer function metadata */
        char const *pszTFRed = NULL;
        char const *pszTFGreen = NULL;
        char const *pszTFBlue = NULL;
        if (pDS != NULL)
            pszTFRed = pDS->GetMetadataItem("TIFFTAG_TRANSFERFUNCTION_RED", "COLOR_PROFILE");
        else
            pszTFRed = CSLFetchNameValue(papszParmList, "TIFFTAG_TRANSFERFUNCTION_RED");

        if (pDS != NULL)
            pszTFGreen = pDS->GetMetadataItem("TIFFTAG_TRANSFERFUNCTION_GREEN", "COLOR_PROFILE");
        else
            pszTFGreen = CSLFetchNameValue(papszParmList, "TIFFTAG_TRANSFERFUNCTION_GREEN");

        if (pDS != NULL)
            pszTFBlue = pDS->GetMetadataItem("TIFFTAG_TRANSFERFUNCTION_BLUE", "COLOR_PROFILE");
        else
            pszTFBlue = CSLFetchNameValue(papszParmList, "TIFFTAG_TRANSFERFUNCTION_BLUE");

        if ((pszTFRed != NULL) && (pszTFGreen != NULL) && (pszTFBlue != NULL))
        {
            /* Get length of table. */
            const int nTransferFunctionLength = 1 << ((pDS!=NULL)?pDS->nBitsPerSample:nBitsPerSample);

            char** papszTokensRed = CSLTokenizeString2( pszTFRed, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
            char** papszTokensGreen = CSLTokenizeString2( pszTFGreen, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
            char** papszTokensBlue = CSLTokenizeString2( pszTFBlue, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );

            if ((CSLCount( papszTokensRed ) == nTransferFunctionLength) &&
                (CSLCount( papszTokensGreen ) == nTransferFunctionLength) &&
                (CSLCount( papszTokensBlue ) == nTransferFunctionLength))
            {
                uint16 *pTransferFuncRed, *pTransferFuncGreen, *pTransferFuncBlue;
                pTransferFuncRed = (uint16*)CPLMalloc(sizeof(uint16) * nTransferFunctionLength);
                pTransferFuncGreen = (uint16*)CPLMalloc(sizeof(uint16) * nTransferFunctionLength);
                pTransferFuncBlue = (uint16*)CPLMalloc(sizeof(uint16) * nTransferFunctionLength);

                /* Convert our table in string format into int16 format. */
                for(int i = 0; i < nTransferFunctionLength; i++)
                {
                    pTransferFuncRed[i] = (uint16)atoi(papszTokensRed[i]);
                    pTransferFuncGreen[i] = (uint16)atoi(papszTokensGreen[i]);
                    pTransferFuncBlue[i] = (uint16)atoi(papszTokensBlue[i]);
                }

                TIFFSetField(hTIFF, TIFFTAG_TRANSFERFUNCTION, 
                    pTransferFuncRed, pTransferFuncGreen, pTransferFuncBlue);

                CPLFree(pTransferFuncRed);
                CPLFree(pTransferFuncGreen);
                CPLFree(pTransferFuncBlue);
            }

            CSLDestroy( papszTokensRed );
            CSLDestroy( papszTokensGreen );
            CSLDestroy( papszTokensBlue );
        }

        /* Output transfer range */
        bool bOutputTransferRange = true;
        for(int i = 0; ((i < 2) && bOutputTransferRange); i++)
        {
            if (pDS != NULL)
                pszValue = pDS->GetMetadataItem(pszTXRNames[i], "COLOR_PROFILE");
            else
                pszValue = CSLFetchNameValue(papszParmList, pszTXRNames[i]);
            if (pszValue == NULL)
            {
                bOutputTransferRange = false;
                break;
            }

            char** papszTokens = CSLTokenizeString2( pszValue, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );

            if (CSLCount( papszTokens ) != 3)
            {
                bOutputTransferRange = false;
                CSLDestroy( papszTokens );
                break;
            }
            
            int j;
            for( j = 0; j < 3; j++ )
            {
                pTXR[i + j * 2] = (uint16)atoi(papszTokens[j]);
            }

            CSLDestroy( papszTokens );
        }
        
        if (bOutputTransferRange)
        {
            TIFFSetField(hTIFF, TIFFTAG_TRANSFERRANGE, pTXR);
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
                                 GTiffDataset **ppoActiveDSRef,
                                 toff_t nDirOffsetIn, 
				 int bBaseIn, GDALAccess eAccess,
                                 int bAllowRGBAInterface,
                                 int bReadGeoTransform,
                                 char** papszSiblingFiles )

{
    uint32	nXSize, nYSize;
    int		bTreatAsBitmap = FALSE;
    int         bTreatAsOdd = FALSE;

    this->eAccess = eAccess;

    hTIFF = hTIFFIn;
    this->ppoActiveDSRef = ppoActiveDSRef;

    nDirOffset = nDirOffsetIn;

    if (!SetDirectory( nDirOffsetIn ))
        return CE_Failure;

    bBase = bBaseIn;

    this->eAccess = eAccess;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
    TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
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
    
#if defined(TIFFLIB_VERSION) && TIFFLIB_VERSION > 20031007 /* 3.6.0 */
    if (nCompression != COMPRESSION_NONE &&
        !TIFFIsCODECConfigured(nCompression))
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
        && CSLTestBoolean( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                              "YES") ) )
    {
        int nColorMode;

        SetMetadataItem( "SOURCE_COLOR_SPACE", "YCbCr", "IMAGE_STRUCTURE" );
        if ( !TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode ) ||
              nColorMode != JPEGCOLORMODE_RGB )
            TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
    }

/* -------------------------------------------------------------------- */
/*      Get strip/tile layout.                                          */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled(hTIFF) )
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(nBlockYSize) );
    }
    else
    {
        if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(nRowsPerStrip) ) )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "RowsPerStrip not defined ... assuming all one strip." );
            nRowsPerStrip = nYSize; /* dummy value */
        }

        // If the rows per strip is larger than the file we will get
        // confused.  libtiff internally will treat the rowsperstrip as
        // the image height and it is best if we do too. (#4468)
        if (nRowsPerStrip > (uint32)nRasterYSize)
            nRowsPerStrip = nRasterYSize;

        nBlockXSize = nRasterXSize;
        nBlockYSize = nRowsPerStrip;
    }
        
    nBlocksPerBand =
        DIV_ROUND_UP(nYSize, nBlockYSize) * DIV_ROUND_UP(nXSize, nBlockXSize);

/* -------------------------------------------------------------------- */
/*      Should we handle this using the GTiffBitmapBand?                */
/* -------------------------------------------------------------------- */
    if( nBitsPerSample == 1 && nBands == 1 )
    {
        bTreatAsBitmap = TRUE;

        // Lets treat large "one row" bitmaps using the scanline api.
        if( !TIFFIsTiled(hTIFF) 
            && nBlockYSize == nYSize 
            && nYSize > 2000 
            && bAllowRGBAInterface )
            bTreatAsSplitBitmap = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Should we treat this via the RGBA interface?                    */
/* -------------------------------------------------------------------- */
    if( bAllowRGBAInterface &&
        !bTreatAsBitmap && !(nBitsPerSample > 8) 
        && (nPhotometric == PHOTOMETRIC_CIELAB ||
            nPhotometric == PHOTOMETRIC_LOGL ||
            nPhotometric == PHOTOMETRIC_LOGLUV ||
            nPhotometric == PHOTOMETRIC_SEPARATED ||
            ( nPhotometric == PHOTOMETRIC_YCBCR 
              && nCompression != COMPRESSION_JPEG )) )
    {
        char	szMessage[1024];

        if( TIFFRGBAImageOK( hTIFF, szMessage ) == 1 )
        {
            const char* pszSourceColorSpace = NULL;
            switch (nPhotometric)
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
                    break;
            }
            if (pszSourceColorSpace)
                SetMetadataItem( "SOURCE_COLOR_SPACE", pszSourceColorSpace, "IMAGE_STRUCTURE" );
            bTreatAsRGBA = TRUE;
            nBands = 4;
        }
        else
        {
            CPLDebug( "GTiff", "TIFFRGBAImageOK says:\n%s", szMessage );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Should we treat this via the split interface?                   */
/* -------------------------------------------------------------------- */
    if( !TIFFIsTiled(hTIFF) 
        && nBitsPerSample == 8
        && nBlockYSize == nYSize 
        && nYSize > 2000
        && !bTreatAsRGBA 
        && CSLTestBoolean(CPLGetConfigOption("GDAL_ENABLE_TIFF_SPLIT", "YES")))
    {
        /* libtiff 3.9.2 (20091104) and older, libtiff 4.0.0beta5 (also 20091104) */
        /* and older will crash when trying to open a all-in-one-strip */
        /* YCbCr JPEG compressed TIFF (see #3259). */
#if (TIFFLIB_VERSION <= 20091104 && !defined(BIGTIFF_SUPPORT)) || \
    (TIFFLIB_VERSION <= 20091104 && defined(BIGTIFF_SUPPORT))
        if (nPhotometric == PHOTOMETRIC_YCBCR  &&
            nCompression == COMPRESSION_JPEG)
        {
            CPLDebug("GTiff", "Avoid using split band to open all-in-one-strip "
                              "YCbCr JPEG compressed TIFF because of older libtiff");
        }
        else
#endif
            bTreatAsSplit = TRUE;
    }
    
/* -------------------------------------------------------------------- */
/*      Should we treat this via the odd bits interface?                */
/* -------------------------------------------------------------------- */
    if ( nSampleFormat == SAMPLEFORMAT_IEEEFP )
    {
        if ( nBitsPerSample == 16 || nBitsPerSample == 24 )
            bTreatAsOdd = TRUE;
    }
    else if ( !bTreatAsRGBA && !bTreatAsBitmap
              && nBitsPerSample != 8
              && nBitsPerSample != 16
              && nBitsPerSample != 32
              && nBitsPerSample != 64 
              && nBitsPerSample != 128 )
        bTreatAsOdd = TRUE;

    int bMinIsWhite = nPhotometric == PHOTOMETRIC_MINISWHITE;

/* -------------------------------------------------------------------- */
/*      Capture the color table if there is one.                        */
/* -------------------------------------------------------------------- */
    unsigned short	*panRed, *panGreen, *panBlue;

    if( bTreatAsRGBA 
        || TIFFGetField( hTIFF, TIFFTAG_COLORMAP, 
                         &panRed, &panGreen, &panBlue) == 0 )
    {
	// Build inverted palette if we have inverted photometric.
	// Pixel values remains unchanged.  Avoid doing this for *deep*
        // data types (per #1882)
	if( nBitsPerSample <= 16 && nPhotometric == PHOTOMETRIC_MINISWHITE )
	{
	    GDALColorEntry  oEntry;
	    int		    iColor, nColorCount;
	    
	    poColorTable = new GDALColorTable();
	    nColorCount = 1 << nBitsPerSample;

	    for ( iColor = 0; iColor < nColorCount; iColor++ )
	    {
		oEntry.c1 = oEntry.c2 = oEntry.c3 = (short) 
                    ((255 * (nColorCount - 1 - iColor)) / (nColorCount-1));
		oEntry.c4 = 255;
		poColorTable->SetColorEntry( iColor, &oEntry );
	    }

	    nPhotometric = PHOTOMETRIC_PALETTE;
	}
	else
	    poColorTable = NULL;
    }
    else
    {
        int	nColorCount, nMaxColor = 0;
        GDALColorEntry oEntry;

        poColorTable = new GDALColorTable();

        nColorCount = 1 << nBitsPerSample;

        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            oEntry.c1 = panRed[iColor] / 256;
            oEntry.c2 = panGreen[iColor] / 256;
            oEntry.c3 = panBlue[iColor] / 256;
            oEntry.c4 = 255;

            poColorTable->SetColorEntry( iColor, &oEntry );

            nMaxColor = MAX(nMaxColor,panRed[iColor]);
            nMaxColor = MAX(nMaxColor,panGreen[iColor]);
            nMaxColor = MAX(nMaxColor,panBlue[iColor]);
        }

        // Bug 1384 - Some TIFF files are generated with color map entry
        // values in range 0-255 instead of 0-65535 - try to handle these
        // gracefully.
        if( nMaxColor > 0 && nMaxColor < 256 )
        {
            CPLDebug( "GTiff", "TIFF ColorTable seems to be improperly scaled, fixing up." );
            
            for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
            {
                oEntry.c1 = panRed[iColor];
                oEntry.c2 = panGreen[iColor];
                oEntry.c3 = panBlue[iColor];
                oEntry.c4 = 255;
                
                poColorTable->SetColorEntry( iColor, &oEntry );
            }
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        if( bTreatAsRGBA )
            SetBand( iBand+1, new GTiffRGBABand( this, iBand+1 ) );
        else if( bTreatAsSplitBitmap )
            SetBand( iBand+1, new GTiffSplitBitmapBand( this, iBand+1 ) );
        else if( bTreatAsSplit )
            SetBand( iBand+1, new GTiffSplitBand( this, iBand+1 ) );
        else if( bTreatAsBitmap )
            SetBand( iBand+1, new GTiffBitmapBand( this, iBand+1 ) );
        else if( bTreatAsOdd )
            SetBand( iBand+1, new GTiffOddBitsBand( this, iBand+1 ) );
        else
            SetBand( iBand+1, new GTiffRasterBand( this, iBand+1 ) );
    }

    if( GetRasterBand(1)->GetRasterDataType() == GDT_Unknown )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unsupported TIFF configuration." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get the transform or gcps from the GeoTIFF file.                */
/* -------------------------------------------------------------------- */
    if( bReadGeoTransform )
    {
        char    *pszTabWKT = NULL;
        double	*padfTiePoints, *padfScale, *padfMatrix;
        uint16	nCount;
        bool    bPixelIsPoint = false;
        short nRasterType;
        GTIF	*psGTIF;
        int     bPointGeoIgnore = FALSE;

        psGTIF = GTIFNew( hTIFF ); // I wonder how expensive this is?

        if( psGTIF )
        {
            if( GTIFKeyGet(psGTIF, GTRasterTypeGeoKey, &nRasterType,
                        0, 1 ) == 1
                && nRasterType == (short) RasterPixelIsPoint )
            {
                bPixelIsPoint = true;
                bPointGeoIgnore =
                    CSLTestBoolean( CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE",
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
    
        if( TIFFGetField(hTIFF,TIFFTAG_GEOPIXELSCALE,&nCount,&padfScale )
            && nCount >= 2 
            && padfScale[0] != 0.0 && padfScale[1] != 0.0 )
        {
            adfGeoTransform[1] = padfScale[0];
            adfGeoTransform[5] = - ABS(padfScale[1]);

            if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
                && nCount >= 6 )
            {
                adfGeoTransform[0] =
                    padfTiePoints[3] - padfTiePoints[0] * adfGeoTransform[1];
                adfGeoTransform[3] =
                    padfTiePoints[4] - padfTiePoints[1] * adfGeoTransform[5];

                if( bPixelIsPoint && !bPointGeoIgnore )
                {
                    adfGeoTransform[0] -= (adfGeoTransform[1] * 0.5 + adfGeoTransform[2] * 0.5);
                    adfGeoTransform[3] -= (adfGeoTransform[4] * 0.5 + adfGeoTransform[5] * 0.5);
                }

                bGeoTransformValid = TRUE;
            }
        }

        else if( TIFFGetField(hTIFF,TIFFTAG_GEOTRANSMATRIX,&nCount,&padfMatrix ) 
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
                adfGeoTransform[0] -= (adfGeoTransform[1] * 0.5 + adfGeoTransform[2] * 0.5);
                adfGeoTransform[3] -= (adfGeoTransform[4] * 0.5 + adfGeoTransform[5] * 0.5);
            }

            bGeoTransformValid = TRUE;
        }

/* -------------------------------------------------------------------- */
/*      Otherwise try looking for a .tab, .tfw, .tifw or .wld file.     */
/* -------------------------------------------------------------------- */
        else
        {
            char* pszGeorefFilename = NULL;

            /* Begin with .tab since it can also have projection info */
            int bTabFileOK =
                GDALReadTabFile2( osFilename, adfGeoTransform,
                                    &pszTabWKT, &nGCPCount, &pasGCPList,
                                    papszSiblingFiles, &pszGeorefFilename );

            if( bTabFileOK )
            {
                if( nGCPCount == 0 )
                    bGeoTransformValid = TRUE;
            }
            else
            {
                if( !bGeoTransformValid )
                {
                    bGeoTransformValid =
                        GDALReadWorldFile2( osFilename, NULL, adfGeoTransform,
                                            papszSiblingFiles, &pszGeorefFilename);
                }

                if( !bGeoTransformValid )
                {
                    bGeoTransformValid =
                        GDALReadWorldFile2( osFilename, "wld", adfGeoTransform,
                                            papszSiblingFiles, &pszGeorefFilename);
                }
            }

            if (pszGeorefFilename)
            {
                osGeorefFilename = pszGeorefFilename;
                CPLFree(pszGeorefFilename);
            }
        }

/* -------------------------------------------------------------------- */
/*      Check for GCPs.  Note, we will allow there to be GCPs and a     */
/*      transform in some circumstances.                                */
/* -------------------------------------------------------------------- */
        if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
            && !bGeoTransformValid )
        {
            nGCPCount = nCount / 6;
            pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPCount);
        
            for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
            {
                char	szID[32];

                sprintf( szID, "%d", iGCP+1 );
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
        }

/* -------------------------------------------------------------------- */
/*      Did we find a tab file?  If so we will use it's coordinate      */
/*      system and give it precidence.                                  */
/* -------------------------------------------------------------------- */
        if( pszTabWKT != NULL 
            && (pszProjection == NULL || pszProjection[0] == '\0') )
        {
            CPLFree( pszProjection );
            pszProjection = pszTabWKT;
            pszTabWKT = NULL;
            bLookedForProjection = TRUE;
        }
        
        CPLFree( pszTabWKT );
        bGeoTIFFInfoChanged = FALSE;
        bForceUnsetGT = FALSE;
        bForceUnsetProjection = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Capture some other potentially interesting information.         */
/* -------------------------------------------------------------------- */
    char	*pszText, szWorkMDI[200];
    uint16  nShort;

    size_t iTag;
    for(iTag=0;iTag<sizeof(asTIFFTags)/sizeof(asTIFFTags[0]);iTag++)
    {
        if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_STRING )
        {
            if( TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &pszText ) )
                SetMetadataItem( asTIFFTags[iTag].pszTagName,  pszText );
        }
        else if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_FLOAT )
        {
            float   fVal;
            if( TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &fVal ) )
            {
                sprintf( szWorkMDI, "%.8g", fVal );
                SetMetadataItem( asTIFFTags[iTag].pszTagName, szWorkMDI );
            }
        }
        else if( asTIFFTags[iTag].eType == GTIFFTAGTYPE_SHORT &&
                 asTIFFTags[iTag].nTagVal != TIFFTAG_RESOLUTIONUNIT )
        {
            if( TIFFGetField( hTIFF, asTIFFTags[iTag].nTagVal, &nShort ) )
            {
                sprintf( szWorkMDI, "%d", nShort );
                SetMetadataItem( asTIFFTags[iTag].pszTagName, szWorkMDI );
            }
        }
    }

    if( TIFFGetField( hTIFF, TIFFTAG_RESOLUTIONUNIT, &nShort ) )
    {
        if( nShort == RESUNIT_NONE )
            sprintf( szWorkMDI, "%d (unitless)", nShort );
        else if( nShort == RESUNIT_INCH )
            sprintf( szWorkMDI, "%d (pixels/inch)", nShort );
        else if( nShort == RESUNIT_CENTIMETER )
            sprintf( szWorkMDI, "%d (pixels/cm)", nShort );
        else
            sprintf( szWorkMDI, "%d", nShort );
        SetMetadataItem( "TIFFTAG_RESOLUTIONUNIT", szWorkMDI );
    }

    int nTagSize;
    void* pData;
    if( TIFFGetField( hTIFF, TIFFTAG_XMLPACKET, &nTagSize, &pData ) )
    {
        char* pszXMP = (char*)VSIMalloc(nTagSize + 1);
        if (pszXMP)
        {
            memcpy(pszXMP, pData, nTagSize);
            pszXMP[nTagSize] = '\0';

            char *apszMDList[2];
            apszMDList[0] = pszXMP;
            apszMDList[1] = NULL;
            SetMetadata(apszMDList, "xml:XMP");

            CPLFree(pszXMP);
        }
    }

    if( nCompression == COMPRESSION_NONE )
        /* no compression tag */;
    else if( nCompression == COMPRESSION_CCITTRLE )
        SetMetadataItem( "COMPRESSION", "CCITTRLE", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_CCITTFAX3 )
        SetMetadataItem( "COMPRESSION", "CCITTFAX3", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_CCITTFAX4 )
        SetMetadataItem( "COMPRESSION", "CCITTFAX4", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_LZW )
        SetMetadataItem( "COMPRESSION", "LZW", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_OJPEG )
        SetMetadataItem( "COMPRESSION", "OJPEG", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_JPEG )
    {  
        if ( nPhotometric == PHOTOMETRIC_YCBCR )
            SetMetadataItem( "COMPRESSION", "YCbCr JPEG", "IMAGE_STRUCTURE" );
        else
            SetMetadataItem( "COMPRESSION", "JPEG", "IMAGE_STRUCTURE" );
    }
    else if( nCompression == COMPRESSION_NEXT )
        SetMetadataItem( "COMPRESSION", "NEXT", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_CCITTRLEW )
        SetMetadataItem( "COMPRESSION", "CCITTRLEW", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_PACKBITS )
        SetMetadataItem( "COMPRESSION", "PACKBITS", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_THUNDERSCAN )
        SetMetadataItem( "COMPRESSION", "THUNDERSCAN", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_PIXARFILM )
        SetMetadataItem( "COMPRESSION", "PIXARFILM", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_PIXARLOG )
        SetMetadataItem( "COMPRESSION", "PIXARLOG", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_DEFLATE )
        SetMetadataItem( "COMPRESSION", "DEFLATE", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_ADOBE_DEFLATE )
        SetMetadataItem( "COMPRESSION", "DEFLATE", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_DCS )
        SetMetadataItem( "COMPRESSION", "DCS", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_JBIG )
        SetMetadataItem( "COMPRESSION", "JBIG", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_SGILOG )
        SetMetadataItem( "COMPRESSION", "SGILOG", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_SGILOG24 )
        SetMetadataItem( "COMPRESSION", "SGILOG24", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_JP2000 )
        SetMetadataItem( "COMPRESSION", "JP2000", "IMAGE_STRUCTURE" );
    else if( nCompression == COMPRESSION_LZMA )
        SetMetadataItem( "COMPRESSION", "LZMA", "IMAGE_STRUCTURE" );

    else
    {
        CPLString oComp;
        SetMetadataItem( "COMPRESSION", 
                         (const char *) oComp.Printf( "%d", nCompression));
    }

    if( nPlanarConfig == PLANARCONFIG_CONTIG && nBands != 1 )
        SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    else
        SetMetadataItem( "INTERLEAVE", "BAND", "IMAGE_STRUCTURE" );

    if(  (GetRasterBand(1)->GetRasterDataType() == GDT_Byte   && nBitsPerSample != 8 ) ||
         (GetRasterBand(1)->GetRasterDataType() == GDT_UInt16 && nBitsPerSample != 16) ||
         (GetRasterBand(1)->GetRasterDataType() == GDT_UInt32 && nBitsPerSample != 32) )
    {
        for (int i = 0; i < nBands; ++i)
            GetRasterBand(i+1)->SetMetadataItem( "NBITS", 
                                                 CPLString().Printf( "%d", (int)nBitsPerSample ),
                                                 "IMAGE_STRUCTURE" );
    }
        
    if( bMinIsWhite )
        SetMetadataItem( "MINISWHITE", "YES", "IMAGE_STRUCTURE" );

    if( TIFFGetField( hTIFF, TIFFTAG_GDAL_METADATA, &pszText ) )
    {
        CPLXMLNode *psRoot = CPLParseXMLString( pszText );
        CPLXMLNode *psItem = NULL;

        if( psRoot != NULL && psRoot->eType == CXT_Element
            && EQUAL(psRoot->pszValue,"GDALMetadata") )
            psItem = psRoot->psChild;

        for( ; psItem != NULL; psItem = psItem->psNext )
        {
            const char *pszKey, *pszValue, *pszRole, *pszDomain; 
            char *pszUnescapedValue;
            int nBand, bIsXML = FALSE;

            if( psItem->eType != CXT_Element
                || !EQUAL(psItem->pszValue,"Item") )
                continue;

            pszKey = CPLGetXMLValue( psItem, "name", NULL );
            pszValue = CPLGetXMLValue( psItem, NULL, NULL );
            nBand = atoi(CPLGetXMLValue( psItem, "sample", "-1" )) + 1;
            pszRole = CPLGetXMLValue( psItem, "role", "" );
            pszDomain = CPLGetXMLValue( psItem, "domain", "" );
                
            if( pszKey == NULL || pszValue == NULL )
                continue;

            if( EQUALN(pszDomain,"xml:",4) )
                bIsXML = TRUE;

            pszUnescapedValue = CPLUnescapeString( pszValue, NULL, 
                                                   CPLES_XML );
            if( nBand == 0 )
            {
                if( bIsXML )
                {
                    char *apszMD[2] = { pszUnescapedValue, NULL };
                    SetMetadata( apszMD, pszDomain );
                }
                else
                    SetMetadataItem( pszKey, pszUnescapedValue, pszDomain );
            }
            else
            {
                GDALRasterBand *poBand = GetRasterBand(nBand);
                if( poBand != NULL )
                {
                    if( EQUAL(pszRole,"scale") )
                        poBand->SetScale( CPLAtofM(pszUnescapedValue) );
                    else if( EQUAL(pszRole,"offset") )
                        poBand->SetOffset( CPLAtofM(pszUnescapedValue) );
                    else if( EQUAL(pszRole,"unittype") )
                        poBand->SetUnitType( pszUnescapedValue );
                    else if( EQUAL(pszRole,"description") )
                        poBand->SetDescription( pszUnescapedValue );
                    else
                    {
                        if( bIsXML )
                        {
                            char *apszMD[2] = { pszUnescapedValue, NULL };
                            poBand->SetMetadata( apszMD, pszDomain );
                        }
                        else
                            poBand->SetMetadataItem(pszKey,pszUnescapedValue,
                                                    pszDomain );
                    }
                }
            }
            CPLFree( pszUnescapedValue );
        }

        CPLDestroyXMLNode( psRoot );
    }

    bMetadataChanged = FALSE;

/* -------------------------------------------------------------------- */
/*      Check for NODATA                                                */
/* -------------------------------------------------------------------- */
    if( TIFFGetField( hTIFF, TIFFTAG_GDAL_NODATA, &pszText ) )
    {
        bNoDataSet = TRUE;
        dfNoDataValue = CPLAtofM( pszText );
    }

/* -------------------------------------------------------------------- */
/*      If this is a "base" raster, we should scan for any              */
/*      associated overviews, internal mask bands and subdatasets.      */
/* -------------------------------------------------------------------- */
    if( bBase )
    {
        //ScanDirectories();
    }

    return( CE_None );
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

    bScanDeferred = FALSE;

    if( !bBase )
        return;

    if( TIFFLastDirectory( hTIFF ) )
        return;

    CPLDebug( "GTiff", "ScanDirectories()" );

/* ==================================================================== */
/*      Scan all directories.                                           */
/* ==================================================================== */
    char **papszSubdatasets = NULL;
    int  iDirIndex = 0;

    FlushDirectory();  
    while( !TIFFLastDirectory( hTIFF ) 
           && (iDirIndex == 0 || TIFFReadDirectory( hTIFF ) != 0) )
    {
        toff_t	nThisDir = TIFFCurrentDirOffset(hTIFF);
        uint32	nSubType = 0;

        *ppoActiveDSRef = NULL; // our directory no longer matches this ds
            
        iDirIndex++;

        if( !TIFFGetField(hTIFF, TIFFTAG_SUBFILETYPE, &nSubType) )
            nSubType = 0;

        /* Embedded overview of the main image */
        if ((nSubType & FILETYPE_REDUCEDIMAGE) != 0 &&
            (nSubType & FILETYPE_MASK) == 0 &&
            iDirIndex != 1 )
        {
            GTiffDataset	*poODS;
                
            poODS = new GTiffDataset();
            if( poODS->OpenOffset( hTIFF, ppoActiveDSRef, nThisDir, FALSE, 
                                   eAccess ) != CE_None 
                || poODS->GetRasterCount() != GetRasterCount() )
            {
                delete poODS;
            }
            else
            {
                CPLDebug( "GTiff", "Opened %dx%d overview.\n", 
                          poODS->GetRasterXSize(), poODS->GetRasterYSize());
                nOverviewCount++;
                papoOverviewDS = (GTiffDataset **)
                    CPLRealloc(papoOverviewDS, 
                               nOverviewCount * (sizeof(void*)));
                papoOverviewDS[nOverviewCount-1] = poODS;
                poODS->poBaseDS = this;
            }
        }
            
        /* Embedded mask of the main image */
        else if ((nSubType & FILETYPE_MASK) != 0 &&
                 (nSubType & FILETYPE_REDUCEDIMAGE) == 0 &&
                 poMaskDS == NULL )
        {
            poMaskDS = new GTiffDataset();
                
            /* The TIFF6 specification - page 37 - only allows 1 SamplesPerPixel and 1 BitsPerSample
               Here we support either 1 or 8 bit per sample
               and we support either 1 sample per pixel or as many samples as in the main image
               We don't check the value of the PhotometricInterpretation tag, which should be
               set to "Transparency mask" (4) according to the specification (page 36)
               ... But the TIFF6 specification allows image masks to have a higher resolution than
               the main image, what we don't support here
            */

            if( poMaskDS->OpenOffset( hTIFF, ppoActiveDSRef, nThisDir, 
                                      FALSE, eAccess ) != CE_None 
                || poMaskDS->GetRasterCount() == 0
                || !(poMaskDS->GetRasterCount() == 1 || poMaskDS->GetRasterCount() == GetRasterCount())
                || poMaskDS->GetRasterXSize() != GetRasterXSize()
                || poMaskDS->GetRasterYSize() != GetRasterYSize()
                || poMaskDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
            {
                delete poMaskDS;
                poMaskDS = NULL;
            }
            else
            {
                CPLDebug( "GTiff", "Opened band mask.\n");
                poMaskDS->poBaseDS = this;
                    
                poMaskDS->bPromoteTo8Bits = CSLTestBoolean(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
            }
        }
            
        /* Embedded mask of an overview */
        /* The TIFF6 specification allows the combination of the FILETYPE_xxxx masks */
        else if ((nSubType & FILETYPE_REDUCEDIMAGE) != 0 &&
                 (nSubType & FILETYPE_MASK) != 0)
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
                int i;
                for(i=0;i<nOverviewCount;i++)
                {
                    if (((GTiffDataset*)papoOverviewDS[i])->poMaskDS == NULL &&
                        poDS->GetRasterXSize() == papoOverviewDS[i]->GetRasterXSize() &&
                        poDS->GetRasterYSize() == papoOverviewDS[i]->GetRasterYSize() &&
                        (poDS->GetRasterCount() == 1 || poDS->GetRasterCount() == GetRasterCount()))
                    {
                        CPLDebug( "GTiff", "Opened band mask for %dx%d overview.\n",
                                  poDS->GetRasterXSize(), poDS->GetRasterYSize());
                        ((GTiffDataset*)papoOverviewDS[i])->poMaskDS = poDS;
                        poDS->bPromoteTo8Bits = CSLTestBoolean(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
                        poDS->poBaseDS = this;
                        break;
                    }
                }
                if (i == nOverviewCount)
                {
                    delete poDS;
                }
            }
        }
        else if( nSubType == 0 || nSubType == FILETYPE_PAGE ) {
            CPLString osName, osDesc;
            uint32	nXSize, nYSize;
            uint16  nSPP;

            TIFFGetField( hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize );
            TIFFGetField( hTIFF, TIFFTAG_IMAGELENGTH, &nYSize );
            if( !TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSPP ) )
                nSPP = 1;

            osName.Printf( "SUBDATASET_%d_NAME=GTIFF_DIR:%d:%s", 
                           iDirIndex, iDirIndex, osFilename.c_str() );
            osDesc.Printf( "SUBDATASET_%d_DESC=Page %d (%dP x %dL x %dB)", 
                           iDirIndex, iDirIndex, 
                           (int)nXSize, (int)nYSize, nSPP );

            papszSubdatasets = 
                CSLAddString( papszSubdatasets, osName );
            papszSubdatasets = 
                CSLAddString( papszSubdatasets, osDesc );
        }

        // Make sure we are stepping from the expected directory regardless
        // of churn done processing the above.
        if( TIFFCurrentDirOffset(hTIFF) != nThisDir )
            TIFFSetSubDirectory( hTIFF, nThisDir );
        *ppoActiveDSRef = NULL;
    }

    /* If we have a mask for the main image, loop over the overviews, and if they */
    /* have a mask, let's set this mask as an overview of the main mask... */
    if (poMaskDS != NULL)
    {
        int i;
        for(i=0;i<nOverviewCount;i++)
        {
            if (((GTiffDataset*)papoOverviewDS[i])->poMaskDS != NULL)
            {
                poMaskDS->nOverviewCount++;
                poMaskDS->papoOverviewDS = (GTiffDataset **)
                    CPLRealloc(poMaskDS->papoOverviewDS, 
                               poMaskDS->nOverviewCount * (sizeof(void*)));
                poMaskDS->papoOverviewDS[poMaskDS->nOverviewCount-1] =
                    ((GTiffDataset*)papoOverviewDS[i])->poMaskDS;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Only keep track of subdatasets if we have more than one         */
/*      subdataset (pair).                                              */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszSubdatasets) > 2 )
    {
        oGTiffMDMD.SetMetadata( papszSubdatasets, "SUBDATASETS" );
    }
    CSLDestroy( papszSubdatasets );
    
}


static int GTiffGetLZMAPreset(char** papszOptions)
{
    int nLZMAPreset = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "LZMA_PRESET" );
    if( pszValue  != NULL )
    {
        nLZMAPreset =  atoi( pszValue );
        if (!(nLZMAPreset >= 0 && nLZMAPreset <= 9))
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
    if( pszValue  != NULL )
    {
        nZLevel =  atoi( pszValue );
        if (!(nZLevel >= 1 && nZLevel <= 9))
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
    if( pszValue  != NULL )
    {
        nJpegQuality = atoi( pszValue );
        if (!(nJpegQuality >= 1 && nJpegQuality <= 100))
        {
            CPLError( CE_Warning, CPLE_IllegalArg, 
                    "JPEG_QUALITY=%s value not recognised, ignoring.",
                    pszValue );
            nJpegQuality = -1;
        }
    }
    return nJpegQuality;
}

/************************************************************************/
/*                            GTiffCreate()                             */
/*                                                                      */
/*      Shared functionality between GTiffDataset::Create() and         */
/*      GTiffCreateCopy() for creating TIFF file based on a set of      */
/*      options and a configuration.                                    */
/************************************************************************/

TIFF *GTiffDataset::CreateLL( const char * pszFilename,
                              int nXSize, int nYSize, int nBands,
                              GDALDataType eType,
                              double dfExtraSpaceForOverviews,
                              char **papszParmList,
                              VSILFILE** pfpL )

{
    TIFF		*hTIFF;
    int                 nBlockXSize = 0, nBlockYSize = 0;
    int                 bTiled = FALSE;
    int                 nCompression = COMPRESSION_NONE;
    int                 nPredictor = PREDICTOR_NONE, nJpegQuality = -1, nZLevel = -1,
                        nLZMAPreset = -1;
    uint16              nSampleFormat;
    int			nPlanar;
    const char          *pszValue;
    const char          *pszProfile;
    int                 bCreateBigTIFF = FALSE;

    if (!GTiffOneTimeInit())
        return NULL;

/* -------------------------------------------------------------------- */
/*      Blow on a few errors.                                           */
/* -------------------------------------------------------------------- */
    if( nXSize < 1 || nYSize < 1 || nBands < 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to create %dx%dx%d TIFF file, but width, height and bands\n"
                  "must be positive.", 
                  nXSize, nYSize, nBands );

        return NULL;
    }

    if (nBands > 65535)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to create %dx%dx%d TIFF file, but bands\n"
                  "must be lesser or equal to 65535.", 
                  nXSize, nYSize, nBands );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*	Setup values based on options.					*/
/* -------------------------------------------------------------------- */
    pszProfile = CSLFetchNameValue(papszParmList,"PROFILE");
    if( pszProfile == NULL )
        pszProfile = "GDALGeoTIFF";

    if( CSLFetchBoolean( papszParmList, "TILED", FALSE ) )
        bTiled = TRUE;

    pszValue = CSLFetchNameValue(papszParmList,"BLOCKXSIZE");
    if( pszValue != NULL )
        nBlockXSize = atoi( pszValue );

    pszValue = CSLFetchNameValue(papszParmList,"BLOCKYSIZE");
    if( pszValue != NULL )
        nBlockYSize = atoi( pszValue );

    pszValue = CSLFetchNameValue(papszParmList,"INTERLEAVE");
    if( pszValue != NULL )
    {
        if( EQUAL( pszValue, "PIXEL" ) )
            nPlanar = PLANARCONFIG_CONTIG;
        else if( EQUAL( pszValue, "BAND" ) )
            nPlanar = PLANARCONFIG_SEPARATE;
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

    pszValue = CSLFetchNameValue( papszParmList, "COMPRESS" );
    if( pszValue  != NULL )
    {
        nCompression = GTIFFGetCompressionMethod(pszValue, "COMPRESS");
        if (nCompression < 0)
            return NULL;
    }

    pszValue = CSLFetchNameValue( papszParmList, "PREDICTOR" );
    if( pszValue  != NULL )
        nPredictor =  atoi( pszValue );

    nZLevel = GTiffGetZLevel(papszParmList);
    nLZMAPreset = GTiffGetLZMAPreset(papszParmList);
    nJpegQuality = GTiffGetJpegQuality(papszParmList);

/* -------------------------------------------------------------------- */
/*      Compute the uncompressed size.                                  */
/* -------------------------------------------------------------------- */
    double  dfUncompressedImageSize;

    dfUncompressedImageSize = 
        nXSize * ((double)nYSize) * nBands * (GDALGetDataTypeSize(eType)/8);
    dfUncompressedImageSize += dfExtraSpaceForOverviews;

    if( nCompression == COMPRESSION_NONE 
        && dfUncompressedImageSize > 4200000000.0 )
    {
#ifndef BIGTIFF_SUPPORT
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "A %d pixels x %d lines x %d bands %s image would be larger than 4GB\n"
                  "but this is the largest size a TIFF can be, and BigTIFF is unavailable.\n"
                  "Creation failed.",
                  nXSize, nYSize, nBands, GDALGetDataTypeName(eType) );
        return NULL;
#endif
    }

/* -------------------------------------------------------------------- */
/*      Should the file be created as a bigtiff file?                   */
/* -------------------------------------------------------------------- */
    const char *pszBIGTIFF = CSLFetchNameValue(papszParmList, "BIGTIFF");

    if( pszBIGTIFF == NULL )
        pszBIGTIFF = "IF_NEEDED";

    if( EQUAL(pszBIGTIFF,"IF_NEEDED") )
    {
        if( nCompression == COMPRESSION_NONE 
            && dfUncompressedImageSize > 4200000000.0 )
            bCreateBigTIFF = TRUE;
    }
    else if( EQUAL(pszBIGTIFF,"IF_SAFER") )
    {
        if( dfUncompressedImageSize > 2000000000.0 )
            bCreateBigTIFF = TRUE;
    }

    else
    {
        bCreateBigTIFF = CSLTestBoolean( pszBIGTIFF );
        if (!bCreateBigTIFF && nCompression == COMPRESSION_NONE &&
             dfUncompressedImageSize > 4200000000.0 )
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                "The TIFF file will be larger than 4GB, so BigTIFF is necessary.\n"
                "Creation failed.");
            return NULL;
        }
    }

#ifndef BIGTIFF_SUPPORT
    if( bCreateBigTIFF )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "BigTIFF requested, but GDAL built without BigTIFF\n"
                  "enabled libtiff, request ignored." );
        bCreateBigTIFF = FALSE;
    }
#endif

    if( bCreateBigTIFF )
        CPLDebug( "GTiff", "File being created as a BigTIFF." );
    
/* -------------------------------------------------------------------- */
/*      Check if the user wishes a particular endianness                */
/* -------------------------------------------------------------------- */

    int eEndianness = ENDIANNESS_NATIVE;
    pszValue = CSLFetchNameValue(papszParmList, "ENDIANNESS");
    if ( pszValue == NULL )
        pszValue = CPLGetConfigOption( "GDAL_TIFF_ENDIANNESS", NULL );
    if ( pszValue != NULL )
    {
        if (EQUAL(pszValue, "LITTLE"))
            eEndianness = ENDIANNESS_LITTLE;
        else if (EQUAL(pszValue, "BIG"))
            eEndianness = ENDIANNESS_BIG;
        else if (EQUAL(pszValue, "INVERTED"))
        {
#ifdef CPL_LSB
            eEndianness = ENDIANNESS_BIG;
#else
            eEndianness = ENDIANNESS_LITTLE;
#endif
        }
        else if (!EQUAL(pszValue, "NATIVE"))
        {
            CPLError( CE_Warning, CPLE_NotSupported,
                      "ENDIANNESS=%s not supported. Defaulting to NATIVE", pszValue );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */

    char szOpeningFlag[5];
    strcpy(szOpeningFlag, "w+");
    if (bCreateBigTIFF)
        strcat(szOpeningFlag, "8");
    if (eEndianness == ENDIANNESS_BIG)
        strcat(szOpeningFlag, "b");
    else if (eEndianness == ENDIANNESS_LITTLE)
        strcat(szOpeningFlag, "l");

    VSILFILE* fpL = VSIFOpenL( pszFilename, "w+b" );
    if( fpL == NULL )
        return NULL;
    hTIFF = VSI_TIFFOpen( pszFilename, szOpeningFlag, fpL );
    if( hTIFF == NULL )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attempt to create new tiff file `%s'\n"
                      "failed in XTIFFOpen().\n",
                      pszFilename );
        VSIFCloseL(fpL);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      How many bits per sample?  We have a special case if NBITS      */
/*      specified for GDT_Byte, GDT_UInt16, GDT_UInt32.                 */
/* -------------------------------------------------------------------- */
    int nBitsPerSample = GDALGetDataTypeSize(eType);
    if (CSLFetchNameValue(papszParmList, "NBITS") != NULL)
    {
        int nMinBits = 0, nMaxBits = 0;
        nBitsPerSample = atoi(CSLFetchNameValue(papszParmList, "NBITS"));
        if( eType == GDT_Byte  )
        {
            nMinBits = 1;
            nMaxBits = 8;
        }
        else if( eType == GDT_UInt16 )
        {
            nMinBits = 9;
            nMaxBits = 16;
        }
        else if( eType == GDT_UInt32  )
        {
            nMinBits = 17;
            nMaxBits = 32;
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "NBITS is not supported for data type %s",
                     GDALGetDataTypeName(eType));
            nBitsPerSample = GDALGetDataTypeSize(eType);
        }

        if (nMinBits != 0)
        {
            if (nBitsPerSample < nMinBits)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "NBITS=%d is invalid for data type %s. Using NBITS=%d",
                         nBitsPerSample, GDALGetDataTypeName(eType), nMinBits);
                nBitsPerSample = nMinBits;
            }
            else if (nBitsPerSample > nMaxBits)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "NBITS=%d is invalid for data type %s. Using NBITS=%d",
                         nBitsPerSample, GDALGetDataTypeName(eType), nMaxBits);
                nBitsPerSample = nMaxBits;
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
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, nBitsPerSample );

    if( (eType == GDT_Byte && EQUAL(pszPixelType,"SIGNEDBYTE"))
        || eType == GDT_Int16 || eType == GDT_Int32 )
        nSampleFormat = SAMPLEFORMAT_INT;
    else if( eType == GDT_CInt16 || eType == GDT_CInt32 )
        nSampleFormat = SAMPLEFORMAT_COMPLEXINT;
    else if( eType == GDT_Float32 || eType == GDT_Float64 )
        nSampleFormat = SAMPLEFORMAT_IEEEFP;
    else if( eType == GDT_CFloat32 || eType == GDT_CFloat64 )
        nSampleFormat = SAMPLEFORMAT_COMPLEXIEEEFP;
    else
        nSampleFormat = SAMPLEFORMAT_UINT;

    TIFFSetField( hTIFF, TIFFTAG_SAMPLEFORMAT, nSampleFormat );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, nBands );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, nPlanar );

/* -------------------------------------------------------------------- */
/*      Setup Photometric Interpretation. Take this value from the user */
/*      passed option or guess correct value otherwise.                 */
/* -------------------------------------------------------------------- */
    int nSamplesAccountedFor = 1;
    int bForceColorTable = FALSE;

    pszValue = CSLFetchNameValue(papszParmList,"PHOTOMETRIC");
    if( pszValue != NULL )
    {
        if( EQUAL( pszValue, "MINISBLACK" ) )
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
        else if( EQUAL( pszValue, "MINISWHITE" ) )
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE );
        else if( EQUAL( pszValue, "PALETTE" ))
        {
            if( eType == GDT_Byte || eType == GDT_UInt16 )
            {
                TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
                nSamplesAccountedFor = 1;
                bForceColorTable = TRUE;
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "PHOTOMETRIC=PALETTE only compatible with Byte or UInt16");
            }
        }
        else if( EQUAL( pszValue, "RGB" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "CMYK" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED );
            nSamplesAccountedFor = 4;
        }
        else if( EQUAL( pszValue, "YCBCR" ))
        {
            /* Because of subsampling, setting YCBCR without JPEG compression leads */
            /* to a crash currently. Would need to make GTiffRasterBand::IWriteBlock() */
            /* aware of subsampling so that it doesn't overrun buffer size returned */
            /* by libtiff */
            if ( nCompression != COMPRESSION_JPEG )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Currently, PHOTOMETRIC=YCBCR requires COMPRESS=JPEG");
                XTIFFClose(hTIFF);
                VSIFCloseL(fpL);
                return NULL;
            }

            if ( nPlanar == PLANARCONFIG_SEPARATE )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "PHOTOMETRIC=YCBCR requires INTERLEAVE=PIXEL");
                XTIFFClose(hTIFF);
                VSIFCloseL(fpL);
                return NULL;
            }

            /* YCBCR strictly requires 3 bands. Not less, not more */
            /* Issue an explicit error message as libtiff one is a bit cryptic : */
            /* TIFFVStripSize64:Invalid td_samplesperpixel value */
            if ( nBands != 3 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "PHOTOMETRIC=YCBCR requires a source raster with only 3 bands (RGB)");
                XTIFFClose(hTIFF);
                VSIFCloseL(fpL);
                return NULL;
            }

            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_YCBCR );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "CIELAB" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CIELAB );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "ICCLAB" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ICCLAB );
            nSamplesAccountedFor = 3;
        }
        else if( EQUAL( pszValue, "ITULAB" ))
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ITULAB );
            nSamplesAccountedFor = 3;
        }
        else
        {
            CPLError( CE_Warning, CPLE_IllegalArg, 
                      "PHOTOMETRIC=%s value not recognised, ignoring.\n"
                      "Set the Photometric Interpretation as MINISBLACK.", 
                      pszValue );
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
        }

        if ( nBands < nSamplesAccountedFor )
        {
            CPLError( CE_Warning, CPLE_IllegalArg, 
                      "PHOTOMETRIC=%s value does not correspond to number "
                      "of bands (%d), ignoring.\n"
                      "Set the Photometric Interpretation as MINISBLACK.", 
                      pszValue, nBands );
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
        }
    }
    else
    {
        /* 
         * If image contains 3 or 4 bands and datatype is Byte then we will
         * assume it is RGB. In all other cases assume it is MINISBLACK.
         */
        if( nBands == 3 && eType == GDT_Byte )
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
            nSamplesAccountedFor = 3;
        }
        else if( nBands == 4 && eType == GDT_Byte )
        {
            uint16 v[1];

            v[0] = GTiffGetAlphaValue(CSLFetchNameValue(papszParmList,"ALPHA"),
                                      DEFAULT_ALPHA_TYPE);

            TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
            nSamplesAccountedFor = 4;
        }
        else
        {
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
            nSamplesAccountedFor = 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      If there are extra samples, we need to mark them with an        */
/*      appropriate extrasamples definition here.                       */
/* -------------------------------------------------------------------- */
    if( nBands > nSamplesAccountedFor )
    {
        uint16 *v;
        int i;
        int nExtraSamples = nBands - nSamplesAccountedFor;

        v = (uint16 *) CPLMalloc( sizeof(uint16) * nExtraSamples );

        v[0] = GTiffGetAlphaValue(CSLFetchNameValue(papszParmList, "ALPHA"),
                                  EXTRASAMPLE_UNSPECIFIED);

        for( i = 1; i < nExtraSamples; i++ )
            v[i] = EXTRASAMPLE_UNSPECIFIED;

        TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, nExtraSamples, v );
        
        CPLFree(v);
    }
    
    /* Set the ICC color profile. */
    if (!EQUAL(pszProfile,"BASELINE"))
    {
        SaveICCProfile(NULL, hTIFF, papszParmList, nBitsPerSample);
    }

    /* Set the compression method before asking the default strip size */
    /* This is usefull when translating to a JPEG-In-TIFF file where */
    /* the default strip size is 8 or 16 depending on the photometric value */
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompression );

/* -------------------------------------------------------------------- */
/*      Setup tiling/stripping flags.                                   */
/* -------------------------------------------------------------------- */
    if( bTiled )
    {
        if( nBlockXSize == 0 )
            nBlockXSize = 256;
        
        if( nBlockYSize == 0 )
            nBlockYSize = 256;

        if (!TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize ) ||
            !TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize ))
        {
            XTIFFClose(hTIFF);
            VSIFCloseL(fpL);
            return NULL;
        }
    }
    else
    {
        uint32 nRowsPerStrip;

        if( nBlockYSize == 0 )
            nRowsPerStrip = MIN(nYSize, (int)TIFFDefaultStripSize(hTIFF,0));
        else
            nRowsPerStrip = nBlockYSize;
        
        TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, nRowsPerStrip );
    }
    
/* -------------------------------------------------------------------- */
/*      Set compression related tags.                                   */
/* -------------------------------------------------------------------- */
    if ( nCompression == COMPRESSION_LZW ||
         nCompression == COMPRESSION_ADOBE_DEFLATE )
        TIFFSetField( hTIFF, TIFFTAG_PREDICTOR, nPredictor );
    if (nCompression == COMPRESSION_ADOBE_DEFLATE
        && nZLevel != -1)
        TIFFSetField( hTIFF, TIFFTAG_ZIPQUALITY, nZLevel );
    else if( nCompression == COMPRESSION_JPEG 
        && nJpegQuality != -1 )
        TIFFSetField( hTIFF, TIFFTAG_JPEGQUALITY, nJpegQuality );
    else if( nCompression == COMPRESSION_LZMA && nLZMAPreset != -1)
        TIFFSetField( hTIFF, TIFFTAG_LZMAPRESET, nLZMAPreset );

/* -------------------------------------------------------------------- */
/*      If we forced production of a file with photometric=palette,     */
/*      we need to push out a default color table.                      */
/* -------------------------------------------------------------------- */
    if( bForceColorTable )
    {
        int nColors;
        
        if( eType == GDT_Byte )
            nColors = 256;
        else
            nColors = 65536;
        
        unsigned short *panTRed, *panTGreen, *panTBlue;

        panTRed = (unsigned short *) CPLMalloc(sizeof(unsigned short)*nColors);
        panTGreen = (unsigned short *) CPLMalloc(sizeof(unsigned short)*nColors);
        panTBlue = (unsigned short *) CPLMalloc(sizeof(unsigned short)*nColors);

        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            if( eType == GDT_Byte )
            {                
                panTRed[iColor] = (unsigned short) (257 * iColor);
                panTGreen[iColor] = (unsigned short) (257 * iColor);
                panTBlue[iColor] = (unsigned short) (257 * iColor);
            }
            else
            {
                panTRed[iColor] = (unsigned short) iColor;
                panTGreen[iColor] = (unsigned short) iColor;
                panTBlue[iColor] = (unsigned short) iColor;
            }
        }
        
        TIFFSetField( hTIFF, TIFFTAG_COLORMAP,
                      panTRed, panTGreen, panTBlue );
        
        CPLFree( panTRed );
        CPLFree( panTGreen );
        CPLFree( panTBlue );
    }

    *pfpL = fpL;

    return( hTIFF );
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new GeoTIFF or TIFF file.                              */
/************************************************************************/

GDALDataset *GTiffDataset::Create( const char * pszFilename,
                                   int nXSize, int nYSize, int nBands,
                                   GDALDataType eType,
                                   char **papszParmList )

{
    GTiffDataset *	poDS;
    TIFF		*hTIFF;
    VSILFILE* fpL = NULL;

/* -------------------------------------------------------------------- */
/*      Create the underlying TIFF file.                                */
/* -------------------------------------------------------------------- */
    hTIFF = CreateLL( pszFilename, nXSize, nYSize, nBands, 
                      eType, 0, papszParmList, &fpL );

    if( hTIFF == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    poDS = new GTiffDataset();
    poDS->hTIFF = hTIFF;
    poDS->fpL = fpL;
    poDS->poActiveDS = poDS;
    poDS->ppoActiveDSRef = &(poDS->poActiveDS);

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->bCrystalized = FALSE;
    poDS->nSamplesPerPixel = (uint16) nBands;
    poDS->osFilename = pszFilename;

    /* Avoid premature crystalization that will cause directory re-writting */
    /* if GetProjectionRef() or GetGeoTransform() are called on the newly created GeoTIFF */
    poDS->bLookedForProjection = TRUE;

    TIFFGetField( hTIFF, TIFFTAG_SAMPLEFORMAT, &(poDS->nSampleFormat) );
    TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &(poDS->nPlanarConfig) );
    TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &(poDS->nPhotometric) );
    TIFFGetField( hTIFF, TIFFTAG_BITSPERSAMPLE, &(poDS->nBitsPerSample) );
    TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &(poDS->nCompression) );

    if( TIFFIsTiled(hTIFF) )
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(poDS->nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(poDS->nBlockYSize) );
    }
    else
    {
        if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                           &(poDS->nRowsPerStrip) ) )
            poDS->nRowsPerStrip = 1; /* dummy value */

        poDS->nBlockXSize = nXSize;
        poDS->nBlockYSize = MIN((int)poDS->nRowsPerStrip,nYSize);
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
        && CSLTestBoolean( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB",
                                              "YES") ) )
    {
        int nColorMode;

        poDS->SetMetadataItem( "SOURCE_COLOR_SPACE", "YCbCr", "IMAGE_STRUCTURE" );
        if ( !TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode ) ||
             nColorMode != JPEGCOLORMODE_RGB )
            TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
    }

/* -------------------------------------------------------------------- */
/*      Read palette back as a color table if it has one.               */
/* -------------------------------------------------------------------- */
    unsigned short	*panRed, *panGreen, *panBlue;
    
    if( poDS->nPhotometric == PHOTOMETRIC_PALETTE
        && TIFFGetField( hTIFF, TIFFTAG_COLORMAP, 
                         &panRed, &panGreen, &panBlue) )
    {
        int	nColorCount;
        GDALColorEntry oEntry;

        poDS->poColorTable = new GDALColorTable();

        nColorCount = 1 << poDS->nBitsPerSample;

        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            oEntry.c1 = panRed[iColor] / 256;
            oEntry.c2 = panGreen[iColor] / 256;
            oEntry.c3 = panBlue[iColor] / 256;
            oEntry.c4 = 255;

            poDS->poColorTable->SetColorEntry( iColor, &oEntry );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we want to ensure all blocks get written out on close to     */
/*      avoid sparse files?                                             */
/* -------------------------------------------------------------------- */
    if( !CSLFetchBoolean( papszParmList, "SPARSE_OK", FALSE ) )
        poDS->bFillEmptyTiles = TRUE;
        
/* -------------------------------------------------------------------- */
/*      Preserve creation options for consulting later (for instance    */
/*      to decide if a TFW file should be written).                     */
/* -------------------------------------------------------------------- */
    poDS->papszCreationOptions = CSLDuplicate( papszParmList );

    poDS->nZLevel = GTiffGetZLevel(papszParmList);
    poDS->nLZMAPreset = GTiffGetLZMAPreset(papszParmList);
    poDS->nJpegQuality = GTiffGetJpegQuality(papszParmList);

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
        if( TIFFIsTiled( hTIFF ) )
        {
            int cc = TIFFTileSize( hTIFF );
            unsigned char *pabyZeros = (unsigned char *) CPLCalloc(cc,1);
            TIFFWriteEncodedTile(hTIFF, 0, pabyZeros, cc);
            CPLFree( pabyZeros );
        }
        else
        {
            int cc = TIFFStripSize( hTIFF );
            unsigned char *pabyZeros = (unsigned char *) CPLCalloc(cc,1);
            TIFFWriteEncodedStrip(hTIFF, 0, pabyZeros, cc);
            CPLFree( pabyZeros );
        }
        poDS->bDontReloadFirstBlock = TRUE;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        if( poDS->nBitsPerSample == 8 ||
            poDS->nBitsPerSample == 16 ||
            poDS->nBitsPerSample == 32 ||
            poDS->nBitsPerSample == 64 ||
            poDS->nBitsPerSample == 128)
            poDS->SetBand( iBand+1, new GTiffRasterBand( poDS, iBand+1 ) );
        else
        {
            poDS->SetBand( iBand+1, new GTiffOddBitsBand( poDS, iBand+1 ) );
            poDS->GetRasterBand( iBand+1 )->
                SetMetadataItem( "NBITS", 
                                 CPLString().Printf("%d",poDS->nBitsPerSample),
                                 "IMAGE_STRUCTURE" );
        }
    }

    poDS->oOvManager.Initialize( poDS, pszFilename );

    return( poDS );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
GTiffDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                          int bStrict, char ** papszOptions, 
                          GDALProgressFunc pfnProgress, void * pProgressData )

{
    TIFF	*hTIFF;
    int		nXSize = poSrcDS->GetRasterXSize();
    int		nYSize = poSrcDS->GetRasterYSize();
    int		nBands = poSrcDS->GetRasterCount();
    int         iBand;
    CPLErr      eErr = CE_None;
    uint16	nPlanarConfig;
    uint16	nBitsPerSample;
    GDALRasterBand *poPBand;

    if( poSrcDS->GetRasterCount() == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to export GeoTIFF files with zero bands." );
        return NULL;
    }
        
    poPBand = poSrcDS->GetRasterBand(1);
    GDALDataType eType = poPBand->GetRasterDataType();

/* -------------------------------------------------------------------- */
/*      Check, whether all bands in input dataset has the same type.    */
/* -------------------------------------------------------------------- */
    for ( iBand = 2; iBand <= nBands; iBand++ )
    {
        if ( eType != poSrcDS->GetRasterBand(iBand)->GetRasterDataType() )
        {
            if ( bStrict )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to export GeoTIFF file with different datatypes per\n"
                          "different bands. All bands should have the same types in TIFF." );
                return NULL;
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Unable to export GeoTIFF file with different datatypes per\n"
                          "different bands. All bands should have the same types in TIFF." );
            }
        }
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Capture the profile.                                            */
/* -------------------------------------------------------------------- */
    const char          *pszProfile;
    int                 bGeoTIFF;

    pszProfile = CSLFetchNameValue(papszOptions,"PROFILE");
    if( pszProfile == NULL )
        pszProfile = "GDALGeoTIFF";

    if( !EQUAL(pszProfile,"BASELINE") 
        && !EQUAL(pszProfile,"GeoTIFF") 
        && !EQUAL(pszProfile,"GDALGeoTIFF") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "PROFILE=%s not supported in GTIFF driver.", 
                  pszProfile );
        return NULL;
    }
    
    if( EQUAL(pszProfile,"BASELINE") )
        bGeoTIFF = FALSE;
    else
        bGeoTIFF = TRUE;

/* -------------------------------------------------------------------- */
/*      Special handling for NBITS.  Copy from band metadata if found.  */
/* -------------------------------------------------------------------- */
    char     **papszCreateOptions = CSLDuplicate( papszOptions );

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
    if (bGeoTIFF)
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

        /* Copy all the tags. Options will override tags in the source */
        int i = 0;
        while(pszOptionsMD[i] != NULL)
        {
            char const *pszMD = CSLFetchNameValue(papszOptions, pszOptionsMD[i]);
            if (pszMD == NULL)
                pszMD = poSrcDS->GetMetadataItem( pszOptionsMD[i], "COLOR_PROFILE" );

            if ((pszMD != NULL) && !EQUAL(pszMD, "") )
            {
                papszCreateOptions = 
                    CSLSetNameValue( papszCreateOptions, pszOptionsMD[i], pszMD );

                /* If an ICC profile exists, other tags are not needed */
                if (EQUAL(pszOptionsMD[i], "SOURCE_ICC_PROFILE"))
                    break;
            }

            i++;
        }
    }

    int nSrcOverviews = poSrcDS->GetRasterBand(1)->GetOverviewCount();
    double dfExtraSpaceForOverviews = 0;
    if (nSrcOverviews != 0 &&
        CSLFetchBoolean(papszOptions, "COPY_SRC_OVERVIEWS", FALSE))
    {
        for(int j=1;j<=nBands;j++)
        {
            if( poSrcDS->GetRasterBand(j)->GetOverviewCount() != nSrcOverviews )
            {
                CPLError( CE_Failure, CPLE_NotSupported, 
                  "COPY_SRC_OVERVIEWS cannot be used when the bands have not the same number of overview levels." );
                CSLDestroy(papszCreateOptions);
                return NULL;
            }
            for(int i=0;i<nSrcOverviews;i++)
            {
                GDALRasterBand* poOvrBand = poSrcDS->GetRasterBand(j)->GetOverview(i);
                if( poOvrBand == NULL )
                {
                    CPLError( CE_Failure, CPLE_NotSupported, 
                        "COPY_SRC_OVERVIEWS cannot be used when one overview band is NULL." );
                    CSLDestroy(papszCreateOptions);
                    return NULL;
                }
                GDALRasterBand* poOvrFirstBand = poSrcDS->GetRasterBand(1)->GetOverview(i);
                if( poOvrBand->GetXSize() != poOvrFirstBand->GetXSize() ||
                    poOvrBand->GetYSize() != poOvrFirstBand->GetYSize() )
                {
                    CPLError( CE_Failure, CPLE_NotSupported, 
                    "COPY_SRC_OVERVIEWS cannot be used when the overview bands have not the same dimensions among bands." );
                    CSLDestroy(papszCreateOptions);
                    return NULL;
                }
            }
        }

        for(int i=0;i<nSrcOverviews;i++)
        {
            dfExtraSpaceForOverviews += ((double)poSrcDS->GetRasterBand(1)->GetOverview(i)->GetXSize()) *
                                        poSrcDS->GetRasterBand(1)->GetOverview(i)->GetYSize();
        }
        dfExtraSpaceForOverviews *= nBands * (GDALGetDataTypeSize(eType) / 8);
    }

/* -------------------------------------------------------------------- */
/*      Should we use optimized way of copying from an input JPEG       */
/*      dataset ?                                                       */
/* -------------------------------------------------------------------- */
#if defined(HAVE_LIBJPEG)
    int bCopyFromJPEG = FALSE;
#endif
#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
    int bDirectCopyFromJPEG = FALSE;
#endif

    /* Note: JPEG_DIRECT_COPY is not defined by default, because it is mainly */
    /* usefull for debugging purposes */
#ifdef JPEG_DIRECT_COPY
    if (CSLFetchBoolean(papszCreateOptions, "JPEG_DIRECT_COPY", FALSE) &&
        GTIFF_CanDirectCopyFromJPEG(poSrcDS, papszCreateOptions))
    {
        CPLDebug("GTiff", "Using special direct copy mode from a JPEG dataset");

        bDirectCopyFromJPEG = TRUE;
    }
#endif

#ifdef HAVE_LIBJPEG
    /* when CreateCopy'ing() from a JPEG dataset, and asking for COMPRESS=JPEG, */
    /* use DCT coefficients (unless other options are incompatible, like strip/tile dimensions, */
    /* specifying JPEG_QUALITY option, incompatible PHOTOMETRIC with the source colorspace, etc...) */
    /* to avoid the lossy steps involved by uncompression/recompression */
    if (!bDirectCopyFromJPEG && GTIFF_CanCopyFromJPEG(poSrcDS, papszCreateOptions))
    {
        CPLDebug("GTiff", "Using special copy mode from a JPEG dataset");

        bCopyFromJPEG = TRUE;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    VSILFILE* fpL = NULL;
    hTIFF = CreateLL( pszFilename, nXSize, nYSize, nBands,
                      eType, dfExtraSpaceForOverviews, papszCreateOptions, &fpL );

    CSLDestroy( papszCreateOptions );

    if( hTIFF == NULL )
        return NULL;

    TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &nPlanarConfig );
    TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &nBitsPerSample );

    uint16      nCompression;

    if( !TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &(nCompression) ) )
        nCompression = COMPRESSION_NONE;

    int bForcePhotometric = 
        CSLFetchNameValue(papszOptions,"PHOTOMETRIC") != NULL;

/* -------------------------------------------------------------------- */
/*      If the source is RGB, then set the PHOTOMETRIC_RGB value        */
/* -------------------------------------------------------------------- */
    if( nBands == 3 && !bForcePhotometric &&
        nCompression != COMPRESSION_JPEG &&
        poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_RedBand &&
        poSrcDS->GetRasterBand(2)->GetColorInterpretation() == GCI_GreenBand &&
        poSrcDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand )
    {
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    }

/* -------------------------------------------------------------------- */
/*      Are we really producing an RGBA image?  If so, set the          */
/*      associated alpha information.                                   */
/* -------------------------------------------------------------------- */
    else if( nBands == 4 && !bForcePhotometric &&
             nCompression != COMPRESSION_JPEG &&
             poSrcDS->GetRasterBand(4)->GetColorInterpretation()==GCI_AlphaBand)
    {
        uint16 v[1];

        v[0] = GTiffGetAlphaValue(CSLFetchNameValue(papszOptions, "ALPHA"),
                                  DEFAULT_ALPHA_TYPE);

        TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    }

    else if( !bForcePhotometric && nBands == 3 &&
             nCompression != COMPRESSION_JPEG &&
             (poSrcDS->GetRasterBand(1)->GetColorInterpretation() != GCI_Undefined ||
              poSrcDS->GetRasterBand(2)->GetColorInterpretation() != GCI_Undefined ||
              poSrcDS->GetRasterBand(3)->GetColorInterpretation() != GCI_Undefined) )
    {
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    }

/* -------------------------------------------------------------------- */
/*      If the output is jpeg compressed, and the input is RGB make     */
/*      sure we note that.                                              */
/* -------------------------------------------------------------------- */

    if( nCompression == COMPRESSION_JPEG )
    {
        if( nBands >= 3 
            && (poSrcDS->GetRasterBand(1)->GetColorInterpretation() 
                == GCI_YCbCr_YBand)
            && (poSrcDS->GetRasterBand(2)->GetColorInterpretation() 
                == GCI_YCbCr_CbBand)
            && (poSrcDS->GetRasterBand(3)->GetColorInterpretation() 
                == GCI_YCbCr_CrBand) )
        {
            /* do nothing ... */
        }
        else
        {
            /* we assume RGB if it isn't explicitly YCbCr */
            CPLDebug( "GTiff", "Setting JPEGCOLORMODE_RGB" );
            TIFFSetField( hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB );
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Does the source image consist of one band, with a palette?      */
/*      If so, copy over.                                               */
/* -------------------------------------------------------------------- */
    if( (nBands == 1 || nBands == 2) && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL 
        && eType == GDT_Byte )
    {
        unsigned short	anTRed[256], anTGreen[256], anTBlue[256];
        GDALColorTable *poCT;

        poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
        
        for( int iColor = 0; iColor < 256; iColor++ )
        {
            if( iColor < poCT->GetColorEntryCount() )
            {
                GDALColorEntry  sRGB;

                poCT->GetColorEntryAsRGB( iColor, &sRGB );

                anTRed[iColor] = (unsigned short) (257 * sRGB.c1);
                anTGreen[iColor] = (unsigned short) (257 * sRGB.c2);
                anTBlue[iColor] = (unsigned short) (257 * sRGB.c3);
            }
            else
            {
                anTRed[iColor] = anTGreen[iColor] = anTBlue[iColor] = 0;
            }
        }

        if( !bForcePhotometric )
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
        TIFFSetField( hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue );
    }
    else if( (nBands == 1 || nBands == 2) 
             && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL 
             && eType == GDT_UInt16 )
    {
        unsigned short	*panTRed, *panTGreen, *panTBlue;
        GDALColorTable *poCT;

        panTRed   = (unsigned short *) CPLMalloc(65536*sizeof(unsigned short));
        panTGreen = (unsigned short *) CPLMalloc(65536*sizeof(unsigned short));
        panTBlue  = (unsigned short *) CPLMalloc(65536*sizeof(unsigned short));

        poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
        
        for( int iColor = 0; iColor < 65536; iColor++ )
        {
            if( iColor < poCT->GetColorEntryCount() )
            {
                GDALColorEntry  sRGB;

                poCT->GetColorEntryAsRGB( iColor, &sRGB );

                panTRed[iColor] = (unsigned short) (256 * sRGB.c1);
                panTGreen[iColor] = (unsigned short) (256 * sRGB.c2);
                panTBlue[iColor] = (unsigned short) (256 * sRGB.c3);
            }
            else
            {
                panTRed[iColor] = panTGreen[iColor] = panTBlue[iColor] = 0;
            }
        }

        if( !bForcePhotometric )
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
        TIFFSetField( hTIFF, TIFFTAG_COLORMAP, panTRed, panTGreen, panTBlue );

        CPLFree( panTRed );
        CPLFree( panTGreen );
        CPLFree( panTBlue );
    }
    else if( poSrcDS->GetRasterBand(1)->GetColorTable() != NULL )
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unable to export color table to GeoTIFF file.  Color tables\n"
                  "can only be written to 1 band or 2 bands Byte or UInt16 GeoTIFF files." );

    if( nBands == 2
        && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL 
        && (eType == GDT_Byte || eType == GDT_UInt16) )
    {
        uint16 v[1] = { EXTRASAMPLE_UNASSALPHA };

        TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, 1, v );
    }

/* -------------------------------------------------------------------- */
/*      Transfer some TIFF specific metadata, if available.             */
/*      The return value will tell us if we need to try again later with*/
/*      PAM because the profile doesn't allow to write some metadata    */
/*      as TIFF tag                                                     */
/* -------------------------------------------------------------------- */
    int bHasWrittenMDInGeotiffTAG =
            GTiffDataset::WriteMetadata( poSrcDS, hTIFF, FALSE, pszProfile,
                                 pszFilename, papszOptions );

/* -------------------------------------------------------------------- */
/* 	Write NoData value, if exist.                                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszProfile,"GDALGeoTIFF") )
    {
        int		bSuccess;
        double	dfNoData;
    
        dfNoData = poSrcDS->GetRasterBand(1)->GetNoDataValue( &bSuccess );
        if ( bSuccess )
            GTiffDataset::WriteNoDataValue( hTIFF, dfNoData );
    }

/* -------------------------------------------------------------------- */
/*      Are we addressing PixelIsPoint mode?                            */
/* -------------------------------------------------------------------- */
    bool bPixelIsPoint = false;
    int  bPointGeoIgnore = FALSE;

    if( poSrcDS->GetMetadataItem( GDALMD_AREA_OR_POINT ) 
        && EQUAL(poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT),
                 GDALMD_AOP_POINT) )
    {
        bPixelIsPoint = true;
        bPointGeoIgnore = 
            CSLTestBoolean( CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE",
                                               "FALSE") );
    }

/* -------------------------------------------------------------------- */
/*      Write affine transform if it is meaningful.                     */
/* -------------------------------------------------------------------- */
    const char *pszProjection = NULL;
    double      adfGeoTransform[6];
    
    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0 || adfGeoTransform[5] != 1.0 ))
    {
        if( bGeoTIFF )
        {
            if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 
                && adfGeoTransform[5] < 0.0 )
            {

                double	adfPixelScale[3], adfTiePoints[6];

                adfPixelScale[0] = adfGeoTransform[1];
                adfPixelScale[1] = fabs(adfGeoTransform[5]);
                adfPixelScale[2] = 0.0;

                TIFFSetField( hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );
            
                adfTiePoints[0] = 0.0;
                adfTiePoints[1] = 0.0;
                adfTiePoints[2] = 0.0;
                adfTiePoints[3] = adfGeoTransform[0];
                adfTiePoints[4] = adfGeoTransform[3];
                adfTiePoints[5] = 0.0;
                
                if( bPixelIsPoint && !bPointGeoIgnore )
                {
                    adfTiePoints[3] += adfGeoTransform[1] * 0.5 + adfGeoTransform[2] * 0.5;
                    adfTiePoints[4] += adfGeoTransform[4] * 0.5 + adfGeoTransform[5] * 0.5;
                }
	    
                TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
            }
            else
            {
                double	adfMatrix[16];

                memset(adfMatrix,0,sizeof(double) * 16);

                adfMatrix[0] = adfGeoTransform[1];
                adfMatrix[1] = adfGeoTransform[2];
                adfMatrix[3] = adfGeoTransform[0];
                adfMatrix[4] = adfGeoTransform[4];
                adfMatrix[5] = adfGeoTransform[5];
                adfMatrix[7] = adfGeoTransform[3];
                adfMatrix[15] = 1.0;
                
                if( bPixelIsPoint && !bPointGeoIgnore )
                {
                    adfMatrix[3] += adfGeoTransform[1] * 0.5 + adfGeoTransform[2] * 0.5;
                    adfMatrix[7] += adfGeoTransform[4] * 0.5 + adfGeoTransform[5] * 0.5;
                }

                TIFFSetField( hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix );
            }
            
            pszProjection = poSrcDS->GetProjectionRef();
        }

/* -------------------------------------------------------------------- */
/*      Do we need a TFW file?                                          */
/* -------------------------------------------------------------------- */
        if( CSLFetchBoolean( papszOptions, "TFW", FALSE ) )
            GDALWriteWorldFile( pszFilename, "tfw", adfGeoTransform );
        else if( CSLFetchBoolean( papszOptions, "WORLDFILE", FALSE ) )
            GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise write tiepoints if they are available.                */
/* -------------------------------------------------------------------- */
    else if( poSrcDS->GetGCPCount() > 0 && bGeoTIFF )
    {
        const GDAL_GCP *pasGCPs = poSrcDS->GetGCPs();
        double	*padfTiePoints;

        padfTiePoints = (double *) 
            CPLMalloc(6*sizeof(double)*poSrcDS->GetGCPCount());

        for( int iGCP = 0; iGCP < poSrcDS->GetGCPCount(); iGCP++ )
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

        TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 
                      6*poSrcDS->GetGCPCount(), padfTiePoints );
        CPLFree( padfTiePoints );
        
        pszProjection = poSrcDS->GetGCPProjection();
        
        if( CSLFetchBoolean( papszOptions, "TFW", FALSE ) 
            || CSLFetchBoolean( papszOptions, "WORLDFILE", FALSE ) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "TFW=ON or WORLDFILE=ON creation options are ignored when GCPs are available");
        }
    }

    else
        pszProjection = poSrcDS->GetProjectionRef();

/* -------------------------------------------------------------------- */
/*      Write the projection information, if possible.                  */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL && strlen(pszProjection) > 0 && bGeoTIFF )
    {
        GTIF	*psGTIF;

        psGTIF = GTIFNew( hTIFF );
        GTIFSetFromOGISDefn( psGTIF, pszProjection );

        if( poSrcDS->GetMetadataItem( GDALMD_AREA_OR_POINT ) 
            && EQUAL(poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT),
                     GDALMD_AOP_POINT) )
        {
            GTIFKeySet(psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1,
                       RasterPixelIsPoint);
        }

        GTIFWriteKeys( psGTIF );
        GTIFFree( psGTIF );
    }

    int bDontReloadFirstBlock = FALSE;

#ifdef HAVE_LIBJPEG
    if (bCopyFromJPEG)
    {
        GTIFF_CopyFromJPEG_WriteAdditionalTags(hTIFF,
                                               poSrcDS);
    }
#else
    if (0)
    {
    }
#endif

#if !defined(BIGTIFF_SUPPORT)
    /* -------------------------------------------------------------------- */
    /*      If we are writing jpeg compression we need to write some        */
    /*      imagery to force the jpegtables to get created.  This is,       */
    /*      likely only needed with libtiff >= 3.9.3 (#3633)                */
    /* -------------------------------------------------------------------- */
    else if( nCompression == COMPRESSION_JPEG
            && strstr(TIFFLIB_VERSION_STR, "Version 3.9") != NULL )
    {
        CPLDebug( "GDAL",
                  "Writing zero block to force creation of JPEG tables." );
        if( TIFFIsTiled( hTIFF ) )
        {
            int cc = TIFFTileSize( hTIFF );
            unsigned char *pabyZeros = (unsigned char *) CPLCalloc(cc,1);
            TIFFWriteEncodedTile(hTIFF, 0, pabyZeros, cc);
            CPLFree( pabyZeros );
        }
        else
        {
            int cc = TIFFStripSize( hTIFF );
            unsigned char *pabyZeros = (unsigned char *) CPLCalloc(cc,1);
            TIFFWriteEncodedStrip(hTIFF, 0, pabyZeros, cc);
            CPLFree( pabyZeros );
        }
        bDontReloadFirstBlock = TRUE;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    
    TIFFWriteCheck( hTIFF, TIFFIsTiled(hTIFF), "GTiffCreateCopy()");
    TIFFWriteDirectory( hTIFF );
    TIFFFlush( hTIFF );
    XTIFFClose( hTIFF );
    hTIFF = NULL;
    VSIFCloseL(fpL);
    fpL = NULL;

    if( eErr != CE_None )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Re-open as a dataset and copy over missing metadata using       */
/*      PAM facilities.                                                 */
/* -------------------------------------------------------------------- */
    GTiffDataset *poDS;
    CPLString osFileName("GTIFF_RAW:");

    osFileName += pszFilename;

    GDALOpenInfo oOpenInfo( osFileName, GA_Update );
    poDS = (GTiffDataset *) Open(&oOpenInfo);
    if( poDS == NULL )
    {
        oOpenInfo.eAccess = GA_ReadOnly;
        poDS = (GTiffDataset *) Open(&oOpenInfo);
    }

    if ( poDS == NULL )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }

    poDS->osProfile = pszProfile;
    poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT & ~GCIF_MASK );
    poDS->papszCreationOptions = CSLDuplicate( papszOptions );
    poDS->bDontReloadFirstBlock = bDontReloadFirstBlock;

/* -------------------------------------------------------------------- */
/*      CloneInfo() doesn't merge metadata, it just replaces it totally */
/*      So we have to merge it                                          */
/* -------------------------------------------------------------------- */

    char **papszSRC_MD = poSrcDS->GetMetadata();
    char **papszDST_MD = CSLDuplicate(poDS->GetMetadata());

    papszDST_MD = CSLMerge( papszDST_MD, papszSRC_MD );

    poDS->SetMetadata( papszDST_MD );
    CSLDestroy( papszDST_MD );

    /* Depending on the PHOTOMETRIC tag, the TIFF file may not have */
    /* the same band count as the source. Will fail later in GDALDatasetCopyWholeRaster anyway... */
    for( int nBand = 1;
         nBand <= MIN(poDS->GetRasterCount(), poSrcDS->GetRasterCount()) ;
         nBand++ )
    {
        GDALRasterBand* poSrcBand = poSrcDS->GetRasterBand(nBand);
        GDALRasterBand* poDstBand = poDS->GetRasterBand(nBand);
        char **papszSRC_MD = poSrcBand->GetMetadata();
        char **papszDST_MD = CSLDuplicate(poDstBand->GetMetadata());

        papszDST_MD = CSLMerge( papszDST_MD, papszSRC_MD );

        poDstBand->SetMetadata( papszDST_MD );
        CSLDestroy( papszDST_MD );

        char** papszCatNames;
        papszCatNames = poSrcBand->GetCategoryNames();
        if (NULL != papszCatNames)
            poDstBand->SetCategoryNames( papszCatNames );
    }

    hTIFF = (TIFF*) poDS->GetInternalHandle(NULL);

/* -------------------------------------------------------------------- */
/*      Handle forcing xml:ESRI data to be written to PAM.              */
/* -------------------------------------------------------------------- */
    if( CSLTestBoolean(CPLGetConfigOption( "ESRI_XML_PAM", "NO" )) )
    {
        char **papszESRIMD = poSrcDS->GetMetadata("xml:ESRI");
        if( papszESRIMD )
        {
            poDS->SetMetadata( papszESRIMD, "xml:ESRI");
        }
    }

/* -------------------------------------------------------------------- */
/*      Second chance : now that we have a PAM dataset, it is possible  */
/*      to write metadata that we couldn't be writen as TIFF tag        */
/* -------------------------------------------------------------------- */
    if (!bHasWrittenMDInGeotiffTAG)
        GTiffDataset::WriteMetadata( poDS, hTIFF, TRUE, pszProfile,
                                     pszFilename, papszOptions, TRUE /* don't write RPC and IMD file again */);

    /* To avoid unnecessary directory rewriting */
    poDS->bMetadataChanged = FALSE;
    poDS->bGeoTIFFInfoChanged = FALSE;
    poDS->bForceUnsetGT = FALSE;
    poDS->bForceUnsetProjection = FALSE;

    /* We must re-set the compression level at this point, since it has */
    /* been lost a few lines above when closing the newly create TIFF file */
    /* The TIFFTAG_ZIPQUALITY & TIFFTAG_JPEGQUALITY are not store in the TIFF file. */
    /* They are just TIFF session parameters */

    poDS->nZLevel = GTiffGetZLevel(papszOptions);
    poDS->nLZMAPreset = GTiffGetLZMAPreset(papszOptions);
    poDS->nJpegQuality = GTiffGetJpegQuality(papszOptions);

    if (nCompression == COMPRESSION_ADOBE_DEFLATE)
    {
        if (poDS->nZLevel != -1)
        {
            TIFFSetField( hTIFF, TIFFTAG_ZIPQUALITY, poDS->nZLevel );
        }
    }
    else if( nCompression == COMPRESSION_JPEG)
    {
        if (poDS->nJpegQuality != -1)
        {
            TIFFSetField( hTIFF, TIFFTAG_JPEGQUALITY, poDS->nJpegQuality );
        }
    }
    else if( nCompression == COMPRESSION_LZMA)
    {
        if (poDS->nLZMAPreset != -1)
        {
            TIFFSetField( hTIFF, TIFFTAG_LZMAPRESET, poDS->nLZMAPreset );
        }
    }

    /* Precreate (internal) mask, so that the IBuildOverviews() below */
    /* has a chance to create also the overviews of the mask */
    int nMaskFlags = poSrcDS->GetRasterBand(1)->GetMaskFlags();
    if( eErr == CE_None
        && !(nMaskFlags & (GMF_ALL_VALID|GMF_ALPHA|GMF_NODATA) )
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

    /* For scaled progress due to overview copying */
    double dfTotalPixels = ((double)nXSize) * nYSize;
    double dfCurPixels = 0;

    if (eErr == CE_None &&
        nSrcOverviews != 0 &&
        CSLFetchBoolean(papszOptions, "COPY_SRC_OVERVIEWS", FALSE))
    {
        eErr = poDS->CreateOverviewsFromSrcOverviews(poSrcDS);

        if (poDS->nOverviewCount != nSrcOverviews)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Did only manage to instanciate %d overview levels, whereas source contains %d",
                     poDS->nOverviewCount, nSrcOverviews);
            eErr = CE_Failure;
        }

        int i;
        for(i=0;i<nSrcOverviews;i++)
        {
            GDALRasterBand* poOvrBand = poSrcDS->GetRasterBand(1)->GetOverview(i);
            dfTotalPixels += ((double)poOvrBand->GetXSize()) *
                                      poOvrBand->GetYSize();
        }

        char* papszCopyWholeRasterOptions[2] = { NULL, NULL };
        if (nCompression != COMPRESSION_NONE)
            papszCopyWholeRasterOptions[0] = (char*) "COMPRESSED=YES";
        /* Now copy the imagery */
        for(i=0;eErr == CE_None && i<nSrcOverviews;i++)
        {
            /* Begin with the smallest overview */
            int iOvrLevel = nSrcOverviews-1-i;
            
            /* Create a fake dataset with the source overview level so that */
            /* GDALDatasetCopyWholeRaster can cope with it */
            GDALDataset* poSrcOvrDS = new GDALOverviewDS(poSrcDS, iOvrLevel);
            
            GDALRasterBand* poOvrBand =
                    poSrcDS->GetRasterBand(1)->GetOverview(iOvrLevel);
            double dfNextCurPixels = dfCurPixels +
                    ((double)poOvrBand->GetXSize()) * poOvrBand->GetYSize();

            void* pScaledData = GDALCreateScaledProgress( dfCurPixels / dfTotalPixels,
                                      dfNextCurPixels / dfTotalPixels,
                                      pfnProgress, pProgressData);
                                
            eErr = GDALDatasetCopyWholeRaster( (GDALDatasetH) poSrcOvrDS,
                                                (GDALDatasetH) poDS->papoOverviewDS[iOvrLevel],
                                                papszCopyWholeRasterOptions,
                                                GDALScaledProgress, pScaledData );
                                                
            dfCurPixels = dfNextCurPixels;
            GDALDestroyScaledProgress(pScaledData);

            delete poSrcOvrDS;
            poDS->papoOverviewDS[iOvrLevel]->FlushCache();

            /* Copy mask of the overview */
            if (eErr == CE_None && poDS->poMaskDS != NULL)
            {
                eErr = GDALRasterBandCopyWholeRaster( poOvrBand->GetMaskBand(),
                                                    poDS->papoOverviewDS[iOvrLevel]->poMaskDS->GetRasterBand(1),
                                                    papszCopyWholeRasterOptions,
                                                    GDALDummyProgress, NULL);
                poDS->papoOverviewDS[iOvrLevel]->poMaskDS->FlushCache();
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy actual imagery.                                            */
/* -------------------------------------------------------------------- */
    void* pScaledData = GDALCreateScaledProgress( dfCurPixels / dfTotalPixels,
                                                  1.0,
                                                  pfnProgress, pProgressData);

    int bTryCopy = TRUE;

#ifdef HAVE_LIBJPEG
    if (bCopyFromJPEG)
    {
        eErr = GTIFF_CopyFromJPEG(poDS, poSrcDS,
                                  pfnProgress, pProgressData,
                                  bTryCopy);

        /* In case of failure in the decompression step, try normal copy */
        if (bTryCopy)
            eErr = CE_None;
    }
#endif

#ifdef JPEG_DIRECT_COPY
    if (bDirectCopyFromJPEG)
    {
        eErr = GTIFF_DirectCopyFromJPEG(poDS, poSrcDS,
                                        pfnProgress, pProgressData,
                                        bTryCopy);

        /* In case of failure in the reading step, try normal copy */
        if (bTryCopy)
            eErr = CE_None;
    }
#endif

    if (bTryCopy && (poDS->bTreatAsSplit || poDS->bTreatAsSplitBitmap))
    {
        /* For split bands, we use TIFFWriteScanline() interface */
        CPLAssert(poDS->nBitsPerSample == 8 || poDS->nBitsPerSample == 1);
        
        if (poDS->nPlanarConfig == PLANARCONFIG_CONTIG && poDS->nBands > 1)
        {
            int j;
            GByte* pabyScanline = (GByte *) CPLMalloc(TIFFScanlineSize(hTIFF));
            for(j=0;j<nYSize && eErr == CE_None;j++)
            {
                eErr = poSrcDS->RasterIO(GF_Read, 0, j, nXSize, 1,
                                         pabyScanline, nXSize, 1,
                                         GDT_Byte, nBands, NULL, poDS->nBands, 0, 1);
                if (eErr == CE_None &&
                    TIFFWriteScanline( hTIFF, pabyScanline, j, 0) == -1)
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "TIFFWriteScanline() failed." );
                    eErr = CE_Failure;
                }
                if( !GDALScaledProgress( (j+1) * 1.0 / nYSize, NULL, pScaledData ) )
                    eErr = CE_Failure;
            }
            CPLFree(pabyScanline);
        }
        else
        {
            int iBand, j;
            GByte* pabyScanline = (GByte *) CPLMalloc(TIFFScanlineSize(hTIFF));
            eErr = CE_None;
            for(iBand=1;iBand<=nBands && eErr == CE_None;iBand++)
            {
                for(j=0;j<nYSize && eErr == CE_None;j++)
                {
                    eErr = poSrcDS->GetRasterBand(iBand)->RasterIO(
                                                    GF_Read, 0, j, nXSize, 1,
                                                    pabyScanline, nXSize, 1,
                                                    GDT_Byte, 0, 0);
                    if (poDS->bTreatAsSplitBitmap)
                    {
                        for(int i=0;i<nXSize;i++)
                        {
                            GByte byVal = pabyScanline[i];
                            if ((i & 0x7) == 0)
                                pabyScanline[i >> 3] = 0;
                            if (byVal)
                                pabyScanline[i >> 3] |= (0x80 >> (i & 0x7));
                        }
                    }
                    if (eErr == CE_None &&
                        TIFFWriteScanline( hTIFF, pabyScanline, j, (uint16) (iBand-1)) == -1)
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "TIFFWriteScanline() failed." );
                        eErr = CE_Failure;
                    }
                    if( !GDALScaledProgress( (j+1 + (iBand - 1) * nYSize) * 1.0 /
                                      (nBands * nYSize), NULL, pScaledData ) )
                        eErr = CE_Failure;
                }
            }
            CPLFree(pabyScanline);
        }
        
        /* Necessary to be able to read the file without re-opening */
#if defined(HAVE_TIFFGETSIZEPROC)
        TIFFSizeProc pfnSizeProc = TIFFGetSizeProc( hTIFF );

        TIFFFlushData( hTIFF );

        toff_t nNewDirOffset = pfnSizeProc( TIFFClientdata( hTIFF ) );
        if( (nNewDirOffset % 2) == 1 )
            nNewDirOffset++;
#endif

        TIFFFlush( hTIFF );

#if defined(HAVE_TIFFGETSIZEPROC)
        if( poDS->nDirOffset != TIFFCurrentDirOffset( hTIFF ) )
        {
            poDS->nDirOffset = nNewDirOffset;
            CPLDebug( "GTiff", "directory moved during flush." );
        }
#endif
    }
    else if (bTryCopy && eErr == CE_None)
    {
        char* papszCopyWholeRasterOptions[2] = { NULL, NULL };
        if (nCompression != COMPRESSION_NONE)
            papszCopyWholeRasterOptions[0] = (char*) "COMPRESSED=YES";
        eErr = GDALDatasetCopyWholeRaster( (GDALDatasetH) poSrcDS,
                                            (GDALDatasetH) poDS,
                                            papszCopyWholeRasterOptions,
                                            GDALScaledProgress, pScaledData );
    }
    
    GDALDestroyScaledProgress(pScaledData);

    if (eErr == CE_None)
    {
        if (poDS->poMaskDS)
        {
            const char* papszOptions[2] = { "COMPRESSED=YES", NULL };
            eErr = GDALRasterBandCopyWholeRaster(
                                    poSrcDS->GetRasterBand(1)->GetMaskBand(),
                                    poDS->GetRasterBand(1)->GetMaskBand(),
                                    (char**)papszOptions,
                                    GDALDummyProgress, NULL);
        }
        else
            eErr = GDALDriver::DefaultCopyMasks( poSrcDS, poDS, bStrict );
    }

    if( eErr == CE_Failure )
    {
        delete poDS;
        poDS = NULL;

        if (CSLTestBoolean(CPLGetConfigOption("GTIFF_DELETE_ON_ERROR", "YES")))
            VSIUnlink( pszFilename ); // should really delete more carefully.
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
        LookForProjection();

        if( EQUAL(pszProjection,"") )
            return GDALPamDataset::GetProjectionRef();
        else
            return( pszProjection );
    }
    else
        return "";
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr GTiffDataset::SetProjection( const char * pszNewProjection )

{
    LookForProjection();

    if( !EQUALN(pszNewProjection,"GEOGCS",6)
        && !EQUALN(pszNewProjection,"PROJCS",6)
        && !EQUALN(pszNewProjection,"LOCAL_CS",8)
        && !EQUALN(pszNewProjection,"COMPD_CS",8)
        && !EQUALN(pszNewProjection,"GEOCCS",6)
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Only OGC WKT Projections supported for writing to GeoTIFF.\n"
                "%s not supported.",
                  pszNewProjection );
        
        return CE_Failure;
    }

    bForceUnsetProjection = (
         EQUAL(pszNewProjection, "") &&
         pszProjection != NULL &&
         !EQUAL(pszProjection, "") );

    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    bGeoTIFFInfoChanged = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    
    if( !bGeoTransformValid )
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::SetGeoTransform( double * padfTransform )

{
    if( GetAccess() == GA_Update )
    {
        bForceUnsetGT = (
            padfTransform[0] == 0.0 &&
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
            adfGeoTransform[5] == 1.0) );

        memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
        bGeoTransformValid = TRUE;
        bGeoTIFFInfoChanged = TRUE;

        return( CE_None );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Attempt to call SetGeoTransform() on a read-only GeoTIFF file." );
        return CE_Failure;
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GTiffDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *GTiffDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
    {
        LookForProjection();
    }
    if (pszProjection != NULL)
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GTiffDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr GTiffDataset::SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                              const char *pszGCPProjection )
{
    if( GetAccess() == GA_Update )
    {
        LoadMDAreaOrPoint();
        bLookedForProjection = TRUE;

        if( this->nGCPCount > 0 )
        {
            GDALDeinitGCPs( this->nGCPCount, this->pasGCPList );
            CPLFree( this->pasGCPList );
        }

        this->nGCPCount = nGCPCount;
        this->pasGCPList = GDALDuplicateGCPs(nGCPCount, pasGCPList);

        CPLFree( this->pszProjection );
        this->pszProjection = CPLStrdup( pszGCPProjection );
        bGeoTIFFInfoChanged = TRUE;

        return CE_None;
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
            "SetGCPs() is only supported on newly created GeoTIFF files." );
        return CE_Failure;
    }
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GTiffDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(CSLDuplicate(oGTiffMDMD.GetDomainList()),
                                   TRUE,
                                   "", "ProxyOverviewRequest", "RPC", "IMD", "SUBDATASETS", "EXIF",
                                   "xml:XMP", "COLOR_PROFILE", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GTiffDataset::GetMetadata( const char * pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"ProxyOverviewRequest") )
        return GDALPamDataset::GetMetadata( pszDomain );

    else if( pszDomain != NULL && EQUAL(pszDomain,"RPC") )
        LoadRPCRPB();

    else if( pszDomain != NULL && EQUAL(pszDomain,"IMD") )
        LoadIMDPVL();
    
    else if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        ScanDirectories();

    else if( pszDomain != NULL && EQUAL(pszDomain,"EXIF") )
        LoadEXIFMetadata();

    else if( pszDomain != NULL && EQUAL(pszDomain,"COLOR_PROFILE") )
        LoadICCProfile();

    else if( pszDomain == NULL || EQUAL(pszDomain, "") )
        LoadMDAreaOrPoint(); /* to set GDALMD_AREA_OR_POINT */

    return oGTiffMDMD.GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/
CPLErr GTiffDataset::SetMetadata( char ** papszMD, const char *pszDomain )

{
    if ((papszMD != NULL) && (pszDomain != NULL) && EQUAL(pszDomain, "COLOR_PROFILE"))
        bColorProfileMetadataChanged = TRUE;
    else if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
        bMetadataChanged = TRUE;

    if( (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        CSLFetchNameValue(papszMD, GDALMD_AREA_OR_POINT) != NULL )
    {
        const char* pszPrevValue = 
                GetMetadataItem(GDALMD_AREA_OR_POINT);
        const char* pszNewValue = 
                CSLFetchNameValue(papszMD, GDALMD_AREA_OR_POINT);
        if (pszPrevValue == NULL || pszNewValue == NULL ||
            !EQUAL(pszPrevValue, pszNewValue))
        {
            LookForProjection();
            bGeoTIFFInfoChanged = TRUE;
        }
    }

    return oGTiffMDMD.SetMetadata( papszMD, pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GTiffDataset::GetMetadataItem( const char * pszName, 
                                           const char * pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"ProxyOverviewRequest") )
        return GDALPamDataset::GetMetadataItem( pszName, pszDomain );

    else if( pszDomain != NULL && EQUAL(pszDomain,"RPC") )
        LoadRPCRPB();

    else if( pszDomain != NULL && EQUAL(pszDomain,"IMD") )
        LoadIMDPVL();

    else if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        ScanDirectories();

    else if( pszDomain != NULL && EQUAL(pszDomain,"EXIF") )
        LoadEXIFMetadata();

    else if( pszDomain != NULL && EQUAL(pszDomain,"COLOR_PROFILE") )
        LoadICCProfile();

    else if( (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        pszName != NULL && EQUAL(pszName, GDALMD_AREA_OR_POINT) )
    {
        LoadMDAreaOrPoint(); /* to set GDALMD_AREA_OR_POINT */
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
    if ((pszDomain != NULL) && EQUAL(pszDomain, "COLOR_PROFILE"))
        bColorProfileMetadataChanged = TRUE;
    else if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
        bMetadataChanged = TRUE;

    if( (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        pszName != NULL && EQUAL(pszName, GDALMD_AREA_OR_POINT) )
    {
        LookForProjection();
        bGeoTIFFInfoChanged = TRUE;
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
/*                           FindRPBFile()                             */
/************************************************************************/

int GTiffDataset::FindRPBFile()
{
    osRPBFile = GDALFindAssociatedFile( osFilename, "RPB", 
                                        oOvManager.GetSiblingFiles(), 0 );

    return osRPBFile != "";
}

/************************************************************************/
/*                           FindIMDFile()                             */
/************************************************************************/

int GTiffDataset::FindIMDFile()
{
    osIMDFile = GDALFindAssociatedFile( osFilename, "IMD", 
                                        oOvManager.GetSiblingFiles(), 0 );

    return osIMDFile != "";
}

/************************************************************************/
/*                           FindPVLFile()                             */
/************************************************************************/

int GTiffDataset::FindPVLFile()
{
    osPVLFile = GDALFindAssociatedFile( osFilename, "PVL", 
                                        oOvManager.GetSiblingFiles(), 0 );

    return osPVLFile != "";
}

/************************************************************************/
/*                           FindRPCFile()                             */
/************************************************************************/

int GTiffDataset::FindRPCFile()
{
    CPLString osSrcPath = osFilename;
    CPLString soPt(".");
    size_t found = osSrcPath.rfind(soPt);
    if (found == CPLString::npos)
        return FALSE;
    osSrcPath.replace (found, osSrcPath.size() - found, "_rpc.txt");
    CPLString osTarget = osSrcPath; 

    char** papszSiblingFiles = oOvManager.GetSiblingFiles();
    if( papszSiblingFiles == NULL )
    {
        VSIStatBufL sStatBuf;

        if( VSIStatExL( osTarget, &sStatBuf, VSI_STAT_EXISTS_FLAG ) != 0 )
        {
            osSrcPath = osFilename;
            osSrcPath.replace (found, osSrcPath.size() - found, "_RPC.TXT");
            osTarget = osSrcPath; 

            if( VSIStatExL( osTarget, &sStatBuf, VSI_STAT_EXISTS_FLAG ) != 0 )
            {
                osSrcPath = osFilename;
                osSrcPath.replace (found, osSrcPath.size() - found, "_rpc.TXT");
                osTarget = osSrcPath; 

                if( VSIStatExL( osTarget, &sStatBuf, VSI_STAT_EXISTS_FLAG ) != 0 )
                {
                    return FALSE;
                }
            }
        }
    }
    else
    {
        int iSibling = CSLFindString( papszSiblingFiles, 
                                    CPLGetFilename(osTarget) );
        if( iSibling < 0 )
            return FALSE;

        osTarget.resize(osTarget.size() - strlen(papszSiblingFiles[iSibling]));
        osTarget += papszSiblingFiles[iSibling];
    }

    osRPCFile = osTarget;
    return TRUE;
}

/************************************************************************/
/*                            LoadRPCRPB()                              */
/************************************************************************/

void GTiffDataset::LoadRPCRPB()
{
    if (bHasSearchedRPC)
        return;

    bHasSearchedRPC = TRUE;

    char **papszRPCMD = NULL;
    /* Read Digital Globe .RPB file */
    if (FindRPBFile())
        papszRPCMD = GDALLoadRPBFile( osRPBFile.c_str(), NULL );

    /* Read GeoEye _rpc.txt file */
    if(papszRPCMD == NULL && FindRPCFile())
        papszRPCMD = GDALLoadRPCFile( osRPCFile.c_str(), NULL );

    if( papszRPCMD != NULL )
    {
        oGTiffMDMD.SetMetadata( papszRPCMD, "RPC" );
        CSLDestroy( papszRPCMD );
    }
    else
        ReadRPCTag();
}

/************************************************************************/
/*                            LoadIMDPVL()                              */
/************************************************************************/

void GTiffDataset::LoadIMDPVL()
{
    if (!bHasSearchedIMD)
    {
        bHasSearchedIMD = TRUE;

        if (FindIMDFile())
        {
            char **papszIMDMD = GDALLoadIMDFile( osIMDFile.c_str(), NULL );

            if( papszIMDMD != NULL )
            {
                papszIMDMD = CSLSetNameValue( papszIMDMD, 
                                                "md_type", "imd" );
                oGTiffMDMD.SetMetadata( papszIMDMD, "IMD" );
                CSLDestroy( papszIMDMD );
            }
        }
    }
    //the imd has priority
    if (!bHasSearchedPVL && osIMDFile.empty())
    {
        bHasSearchedPVL = TRUE;

        if (FindPVLFile())
        {
            /* -------------------------------------------------------------------- */
            /*      Read file and parse.                                            */
            /* -------------------------------------------------------------------- */
            CPLKeywordParser oParser;

            VSILFILE *fp = VSIFOpenL( osPVLFile.c_str(), "r" );

            if( fp == NULL )
                return;
    
            if( !oParser.Ingest( fp ) )
            {
                VSIFCloseL( fp );
                return;
            }

            VSIFCloseL( fp );

            /* -------------------------------------------------------------------- */
            /*      Consider version changing.                                      */
            /* -------------------------------------------------------------------- */
            char **papszPVLMD = CSLDuplicate( oParser.GetAllKeywords() );
 
            if( papszPVLMD != NULL )
            {
                papszPVLMD = CSLSetNameValue( papszPVLMD, 
                                                "md_type", "pvl" );
                
                oGTiffMDMD.SetMetadata( papszPVLMD, "IMD" );
                CSLDestroy( papszPVLMD );
            }
        }
    }
}

/************************************************************************/
/*                         LoadEXIFMetadata()                           */
/************************************************************************/

void GTiffDataset::LoadEXIFMetadata()
{
    if (bEXIFMetadataLoaded)
        return;
    bEXIFMetadataLoaded = TRUE;

    if (!SetDirectory())
        return;

    VSILFILE* fp = (VSILFILE*) TIFFClientdata( hTIFF );

    GByte          abyHeader[2];
    VSIFSeekL(fp, 0, SEEK_SET);
    VSIFReadL(abyHeader, 1, 2, fp);

    int bLittleEndian = abyHeader[0] == 'I' && abyHeader[1] == 'I';
    int bSwabflag = bLittleEndian ^ CPL_IS_LSB;

    char** papszMetadata = NULL;
    toff_t nOffset;

    if (TIFFGetField(hTIFF, TIFFTAG_EXIFIFD, &nOffset))
    {
        int nExifOffset = (int)nOffset, nInterOffset = 0, nGPSOffset = 0;
        EXIFExtractMetadata(papszMetadata,
                            fp, (int)nOffset,
                            bSwabflag, 0,
                            nExifOffset, nInterOffset, nGPSOffset);
    }

    if (TIFFGetField(hTIFF, TIFFTAG_GPSIFD, &nOffset))
    {
        int nExifOffset = 0, nInterOffset = 0, nGPSOffset = (int)nOffset;
        EXIFExtractMetadata(papszMetadata,
                            fp, (int)nOffset,
                            bSwabflag, 0,
                            nExifOffset, nInterOffset, nGPSOffset);
    }

    oGTiffMDMD.SetMetadata( papszMetadata, "EXIF" );
    CSLDestroy( papszMetadata );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **GTiffDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    LoadRPCRPB();
    LoadIMDPVL();

    if (osIMDFile.size() != 0)
        papszFileList = CSLAddString( papszFileList, osIMDFile );
    if (osPVLFile.size() != 0)
        papszFileList = CSLAddString( papszFileList, osPVLFile );
    if (osRPBFile.size() != 0)
        papszFileList = CSLAddString( papszFileList, osRPBFile );
    if (osRPCFile.size() != 0)
        papszFileList = CSLAddString( papszFileList, osRPCFile );

    if (osGeorefFilename.size() != 0 &&
        CSLFindString(papszFileList, osGeorefFilename) == -1)
    {
        papszFileList = CSLAddString( papszFileList, osGeorefFilename );
    }

    return papszFileList;
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr GTiffDataset::CreateMaskBand(int nFlags)
{
    ScanDirectories();

    if (poMaskDS != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This TIFF dataset has already an internal mask band");
        return CE_Failure;
    }
    else if (CSLTestBoolean(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", "NO")))
    {
        toff_t  nOffset;
        int     bIsTiled;
        int     bIsOverview = FALSE;
        uint32	nSubType;
        int     nCompression;

        if (nFlags != GMF_PER_DATASET)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The only flag value supported for internal mask is GMF_PER_DATASET");
            return CE_Failure;
        }

        if( strstr(GDALGetMetadataItem(GDALGetDriverByName( "GTiff" ),
                                       GDAL_DMD_CREATIONOPTIONLIST, NULL ),
                   "<Value>DEFLATE</Value>") != NULL )
            nCompression = COMPRESSION_ADOBE_DEFLATE;
        else
            nCompression = COMPRESSION_PACKBITS;

    /* -------------------------------------------------------------------- */
    /*      If we don't have read access, then create the mask externally.  */
    /* -------------------------------------------------------------------- */
        if( GetAccess() != GA_Update )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                    "File open for read-only accessing, "
                    "creating mask externally." );

            return GDALPamDataset::CreateMaskBand(nFlags);
        }

        if (poBaseDS)
        {
            if (!poBaseDS->SetDirectory())
                return CE_Failure;
        }
        if (!SetDirectory())
            return CE_Failure;

        if( TIFFGetField(hTIFF, TIFFTAG_SUBFILETYPE, &nSubType))
        {
            bIsOverview = (nSubType & FILETYPE_REDUCEDIMAGE) != 0;

            if ((nSubType & FILETYPE_MASK) != 0)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Cannot create a mask on a TIFF mask IFD !" );
                return CE_Failure;
            }
        }

        bIsTiled = TIFFIsTiled(hTIFF);
        
        FlushDirectory();
        
        nOffset = GTIFFWriteDirectory(hTIFF,
                                      (bIsOverview) ? FILETYPE_REDUCEDIMAGE | FILETYPE_MASK : FILETYPE_MASK,
                                      nRasterXSize, nRasterYSize,
                                      1, PLANARCONFIG_CONTIG, 1,
                                      nBlockXSize, nBlockYSize,
                                      bIsTiled, nCompression, 
                                      PHOTOMETRIC_MASK, PREDICTOR_NONE,
                                      SAMPLEFORMAT_UINT, NULL, NULL, NULL, 0, NULL, "");
        if (nOffset == 0)
            return CE_Failure;

        poMaskDS = new GTiffDataset();
        poMaskDS->bPromoteTo8Bits = CSLTestBoolean(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK_TO_8BIT", "YES"));
        if( poMaskDS->OpenOffset( hTIFF, ppoActiveDSRef, nOffset, 
                                  FALSE, GA_Update ) != CE_None)
        {
            delete poMaskDS;
            poMaskDS = NULL;
            return CE_Failure;
        }

        return CE_None;
    }
    else
    {
        return GDALPamDataset::CreateMaskBand(nFlags);
    }
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr GTiffRasterBand::CreateMaskBand(int nFlags)
{
    poGDS->ScanDirectories();

    if (poGDS->poMaskDS != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This TIFF dataset has already an internal mask band");
        return CE_Failure;
    }
    else if (CSLTestBoolean(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", "NO")))
    {
        return poGDS->CreateMaskBand(nFlags);
    }
    else
    {
        return GDALPamRasterBand::CreateMaskBand(nFlags);
    }
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
    char      *pszModFmt;
    int       iIn, iOut;

    pszModFmt = (char *) CPLMalloc( strlen(module)*2 + strlen(fmt) + 2 );
    for( iOut = 0, iIn = 0; module[iIn] != '\0'; iIn++ )
    {
        if( module[iIn] == '%' )
        {
            pszModFmt[iOut++] = '%';
            pszModFmt[iOut++] = '%';
        }
        else
            pszModFmt[iOut++] = module[iIn];
    }
    pszModFmt[iOut] = '\0';
    strcat( pszModFmt, ":" );
    strcat( pszModFmt, fmt );

    return pszModFmt;
}

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
void
GTiffWarningHandler(const char* module, const char* fmt, va_list ap )
{
    char	*pszModFmt;

    if( strstr(fmt,"nknown field") != NULL )
        return;

    pszModFmt = PrepareTIFFErrorFormat( module, fmt );
    if( strstr(fmt, "does not end in null byte") != NULL )
    {
        CPLString osMsg;
        osMsg.vPrintf(pszModFmt, ap);
        CPLDebug( "GTiff", "%s", osMsg.c_str() );
    }
    else
        CPLErrorV( CE_Warning, CPLE_AppDefined, pszModFmt, ap );
    CPLFree( pszModFmt );
}

/************************************************************************/
/*                         GTiffErrorHandler()                          */
/************************************************************************/
void
GTiffErrorHandler(const char* module, const char* fmt, va_list ap )
{
    char *pszModFmt;

#if SIZEOF_VOIDP == 4
    /* Case of one-strip file where the strip size is > 2GB (#5403) */
    if( strcmp(module, "TIFFStripSize") == 0 &&
        strstr(fmt, "Integer overflow") != NULL )
    {
        bGlobalStripIntegerOverflow = TRUE;
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
        fmt = "Maximum TIFF file size exceeded. Use BIGTIFF=YES creation option.";
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
    static const TIFFFieldInfo xtiffFieldInfo[] = {
        { TIFFTAG_GDAL_METADATA,    -1,-1, TIFF_ASCII,	FIELD_CUSTOM,
          TRUE,	FALSE,	(char*) "GDALMetadata" },
        { TIFFTAG_GDAL_NODATA,	    -1,-1, TIFF_ASCII,	FIELD_CUSTOM,
          TRUE,	FALSE,	(char*) "GDALNoDataValue" },
        { TIFFTAG_RPCCOEFFICIENT,   -1,-1, TIFF_DOUBLE,	FIELD_CUSTOM,
          TRUE,	TRUE,	(char*) "RPCCoefficient" }
    };

    if (_ParentExtender) 
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

static void* hGTiffOneTimeInitMutex = NULL;

int GTiffOneTimeInit()

{
    static int bInitIsOk = TRUE;
    static int bOneTimeInitDone = FALSE;
    CPLMutexHolder oHolder( &hGTiffOneTimeInitMutex);
    if( bOneTimeInitDone )
        return bInitIsOk;

    bOneTimeInitDone = TRUE;

    /* This is a frequent configuration error that is difficult to track down */
    /* for people unaware of the issue : GDAL built against internal libtiff (4.X) */
    /* but used by an application that links with external libtiff (3.X) */
    /* Note: on my conf, the order that cause GDAL to crash - and that is detected */
    /* by the following code - is "-ltiff -lgdal". "-lgdal -ltiff" works for the */
    /* GTiff driver but probably breaks the application that believes it uses libtiff 3.X */
    /* but we cannot detect that... */
#if defined(BIGTIFF_SUPPORT) && !defined(RENAME_INTERNAL_LIBTIFF_SYMBOLS)
#if defined(HAVE_DLFCN_H) && !defined(WIN32)
    const char* (*pfnVersion)(void);
    pfnVersion = (const char* (*)(void)) dlsym(RTLD_DEFAULT, "TIFFGetVersion");
    if (pfnVersion)
    {
        const char* pszVersion = pfnVersion();
        if (pszVersion && strstr(pszVersion, "Version 3.") != NULL)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "libtiff version mismatch : You're linking against libtiff 3.X, but GDAL has been compiled against libtiff >= 4.0.0");
        }
    }
#endif
#endif

    _ParentExtender = TIFFSetTagExtender(GTiffTagExtender);

    TIFFSetWarningHandler( GTiffWarningHandler );
    TIFFSetErrorHandler( GTiffErrorHandler );

    // This only really needed if we are linked to an external libgeotiff
    // with its own (lame) file searching logic. 
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

    if( hGTiffOneTimeInitMutex != NULL )
    {
        CPLDestroyMutex(hGTiffOneTimeInitMutex);
        hGTiffOneTimeInitMutex = NULL;
    }

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

#if defined(TIFFLIB_VERSION) && TIFFLIB_VERSION > 20031007 /* 3.6.0 */
    if (nCompression != COMPRESSION_NONE &&
        !TIFFIsCODECConfigured((uint16) nCompression))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
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
    if( GDALGetDriverByName( "GTiff" ) == NULL )
    {
        GDALDriver	*poDriver;
        char szCreateOptions[4556];
        char szOptionalCompressItems[500];
        int bHasJPEG = FALSE, bHasLZW = FALSE, bHasDEFLATE = FALSE, bHasLZMA = FALSE;

        poDriver = new GDALDriver();
        
/* -------------------------------------------------------------------- */
/*      Determine which compression codecs are available that we        */
/*      want to advertise.  If we are using an old libtiff we won't     */
/*      be able to find out so we just assume all are available.        */
/* -------------------------------------------------------------------- */
        strcpy( szOptionalCompressItems, 
                "       <Value>NONE</Value>" );

#if TIFFLIB_VERSION <= 20040919
        strcat( szOptionalCompressItems, 
                "       <Value>PACKBITS</Value>"
                "       <Value>JPEG</Value>"
                "       <Value>LZW</Value>"
                "       <Value>DEFLATE</Value>" );
        bHasLZW = bHasDEFLATE = TRUE;
#else
        TIFFCodec	*c, *codecs = TIFFGetConfiguredCODECs();

        for( c = codecs; c->name; c++ )
        {
            if( c->scheme == COMPRESSION_PACKBITS )
                strcat( szOptionalCompressItems,
                        "       <Value>PACKBITS</Value>" );
            else if( c->scheme == COMPRESSION_JPEG )
            {
                bHasJPEG = TRUE;
                strcat( szOptionalCompressItems,
                        "       <Value>JPEG</Value>" );
            }
            else if( c->scheme == COMPRESSION_LZW )
            {
                bHasLZW = TRUE;
                strcat( szOptionalCompressItems,
                        "       <Value>LZW</Value>" );
            }
            else if( c->scheme == COMPRESSION_ADOBE_DEFLATE )
            {
                bHasDEFLATE = TRUE;
                strcat( szOptionalCompressItems,
                        "       <Value>DEFLATE</Value>" );
            }
            else if( c->scheme == COMPRESSION_CCITTRLE )
                strcat( szOptionalCompressItems,
                        "       <Value>CCITTRLE</Value>" );
            else if( c->scheme == COMPRESSION_CCITTFAX3 )
                strcat( szOptionalCompressItems,
                        "       <Value>CCITTFAX3</Value>" );
            else if( c->scheme == COMPRESSION_CCITTFAX4 )
                strcat( szOptionalCompressItems,
                        "       <Value>CCITTFAX4</Value>" );
            else if( c->scheme == COMPRESSION_LZMA )
            {
                bHasLZMA = TRUE;
                strcat( szOptionalCompressItems,
                        "       <Value>LZMA</Value>" );
            }
        }
        _TIFFfree( codecs );
#endif        

/* -------------------------------------------------------------------- */
/*      Build full creation option list.                                */
/* -------------------------------------------------------------------- */
        sprintf( szCreateOptions, "%s%s%s", 
"<CreationOptionList>"
"   <Option name='COMPRESS' type='string-select'>",
                 szOptionalCompressItems,
"   </Option>");
        if (bHasLZW || bHasDEFLATE)
            strcat( szCreateOptions, ""        
"   <Option name='PREDICTOR' type='int' description='Predictor Type'/>");
        if (bHasJPEG)
        {
            strcat( szCreateOptions, ""
"   <Option name='JPEG_QUALITY' type='int' description='JPEG quality 1-100' default='75'/>" );
#ifdef JPEG_DIRECT_COPY
            strcat( szCreateOptions, ""
"   <Option name='JPEG_DIRECT_COPY' type='boolean' description='To copy without any decompression/recompression a JPEG source file' default='NO'/>");
#endif
        }
        if (bHasDEFLATE)
            strcat( szCreateOptions, ""
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='6'/>");
        if (bHasLZMA)
            strcat( szCreateOptions, ""
"   <Option name='LZMA_PRESET' type='int' description='LZMA compression level 0(fast)-9(slow)' default='6'/>");
        strcat( szCreateOptions, ""
"   <Option name='NBITS' type='int' description='BITS for sub-byte files (1-7), sub-uint16 (9-15), sub-uint32 (17-31)'/>"
"   <Option name='INTERLEAVE' type='string-select' default='PIXEL'>"
"       <Value>BAND</Value>"
"       <Value>PIXEL</Value>"
"   </Option>"
"   <Option name='TILED' type='boolean' description='Switch to tiled format'/>"
"   <Option name='TFW' type='boolean' description='Write out world file'/>"
"   <Option name='RPB' type='boolean' description='Write out .RPB (RPC) file'/>"
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
"   <Option name='SPARSE_OK' type='boolean' description='Can newly created files have missing blocks?' default='FALSE'/>"
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
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16 Int16 UInt32 Int32 Float32 "
                                   "Float64 CInt16 CInt32 CFloat32 CFloat64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
                                   szCreateOptions );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = GTiffDataset::Open;
        poDriver->pfnCreate = GTiffDataset::Create;
        poDriver->pfnCreateCopy = GTiffDataset::CreateCopy;
        poDriver->pfnUnloadDriver = GDALDeregister_GTiff;
        poDriver->pfnIdentify = GTiffDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
