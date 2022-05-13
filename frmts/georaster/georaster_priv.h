/******************************************************************************
 *
 * Name:     georaster_priv.h
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Define C++/Private declarations
 * Author:   Ivan Lucena [ivan.lucena at oracle.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files ( the "Software" ),
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
 *****************************************************************************/

#ifndef GEORASTER_PRIV_H_INCLUDED
#define GEORASTER_PRIV_H_INCLUDED

#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "gdal_rat.h"
#include "ogr_spatialref.h"
#include "cpl_minixml.h"
#include "cpl_list.h"

//  ---------------------------------------------------------------------------
//  DEFLATE compression support
//  ---------------------------------------------------------------------------

#include <zlib.h>

//  ---------------------------------------------------------------------------
//  JPEG compression support
//  ---------------------------------------------------------------------------

#ifdef JPEG_SUPPORTED
CPL_C_START
#include <jpeglib.h>
CPL_C_END

void jpeg_vsiio_src (j_decompress_ptr cinfo, VSILFILE * infile);
void jpeg_vsiio_dest (j_compress_ptr cinfo, VSILFILE * outfile);
#endif

//  ---------------------------------------------------------------------------
//  JPEG2000 support - Install the Virtual File System handler to OCI LOB
//  ---------------------------------------------------------------------------

CPL_C_START
void CPL_DLL VSIInstallOCILobHandler(void);
CPL_C_END

//  ---------------------------------------------------------------------------
//  System constants
//  ---------------------------------------------------------------------------

//  VAT maximum string len

#define MAXLEN_VATSTR 128

//  Geographic system without EPSG parameters

#define UNKNOWN_CRS     999999
#define NO_CRS          0
#define DEFAULT_CRS     NO_CRS

//  Bitmap Mask for the whole dataset start with -99999

#define DEFAULT_BMP_MASK -99999

//  Default block size

#define DEFAULT_BLOCK_ROWS 512
#define DEFAULT_BLOCK_COLUMNS 512

#define DEFAULT_JP2_TILE_ROWS 512
#define DEFAULT_JP2_TILE_COLUMNS 512

//  Default Model Coordinate Location (internal pixel geo-reference)

#define MCL_CENTER      0
#define MCL_UPPERLEFT   1
#define MCL_DEFAULT     MCL_CENTER

// MAX double string representation

#define MAX_DOUBLE_STR_REP 20

// Pyramid levels details

struct hLevelDetails {
    int             nColumnBlockSize;
    int             nRowBlockSize;
    int             nTotalColumnBlocks;
    int             nTotalRowBlocks;
    unsigned long   nBlockCount;
    unsigned long   nBlockBytes;
    unsigned long   nGDALBlockBytes;
    unsigned long   nOffset;
};

//  ---------------------------------------------------------------------------
//  Support for multi-values NoData support
//  ---------------------------------------------------------------------------

struct hNoDataItem {
    int             nBand;
    double          dfLower;
    double          dfUpper;
};

//  ---------------------------------------------------------------------------
//  GeoRaster wrapper class definitions
//  ---------------------------------------------------------------------------

#include "oci_wrapper.h"

class GeoRasterDataset;
class GeoRasterRasterBand;
class GeoRasterWrapper;

//  ---------------------------------------------------------------------------
//  GeoRasterDataset, extends GDALDataset to support GeoRaster Datasets
//  ---------------------------------------------------------------------------

class GeoRasterDataset final: public GDALDataset
{
    friend class GeoRasterRasterBand;

  public:
    GeoRasterDataset();
    ~GeoRasterDataset() override;

  private:
    GeoRasterWrapper*   poGeoRaster;
    bool                bGeoTransform;
    bool                bForcedSRID;
    char*               pszProjection;
    char**              papszSubdatasets;
    double              adfGeoTransform[6];
    GeoRasterRasterBand*
                        poMaskBand;
    bool                bApplyNoDataArray;
    void                JP2_Open( GDALAccess eAccess );
    void                JP2_CreateCopy( GDALDataset* poJP2DS,
                                        char** papszOptions,
                                        int* pnResolutions,
                                        GDALProgressFunc pfnProgress,
                                        void* pProgressData );
    boolean             JP2_CopyDirect( const char* pszJP2Filename,
                                        int* pnResolutions,
                                        GDALProgressFunc pfnProgress,
                                        void* pProgressData );
    boolean             JPEG_CopyDirect( const char* pszJPGFilename,
                                         GDALProgressFunc pfnProgress,
                                         void* pProgressData );

public:

    GDALDataset*        poJP2Dataset;

    void                SetSubdatasets( GeoRasterWrapper* poGRW );

    static int          Identify( GDALOpenInfo* poOpenInfo );
    static GDALDataset* Open( GDALOpenInfo* poOpenInfo );
    static CPLErr       Delete( const char *pszFilename );
    static GDALDataset* Create( const char* pszFilename,
                            int nXSize,
                            int nYSize,
                            int nBands,
                            GDALDataType eType,
                            char** papszOptions );
    static GDALDataset* CreateCopy( const char* pszFilename,
                                    GDALDataset* poSrcDS,
                                    int bStrict,
                                    char** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void* pProgressData );
    CPLErr GetGeoTransform( double* padfTransform ) override;
    CPLErr SetGeoTransform( double* padfTransform ) override;
    const char *_GetProjectionRef() override;
    CPLErr _SetProjection( const char* pszProjString ) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }

    char **GetMetadataDomainList() override;
    char **GetMetadata( const char* pszDomain ) override;
    void FlushCache(bool bAtClosing) override;
    CPLErr IRasterIO( GDALRWFlag eRWFlag,
                      int nXOff, int nYOff, int nXSize, int nYSize,
                      void *pData, int nBufXSize, int nBufYSize,
                      GDALDataType eBufType,
                      int nBandCount, int *panBandMap,
                      GSpacing nPixelSpace, GSpacing nLineSpace,
                      GSpacing nBandSpace,
                      GDALRasterIOExtraArg* psExtraArg ) override;
    int GetGCPCount() override;
    const char* _GetGCPProjection() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override {
        return GetGCPSpatialRefFromOldGetGCPProjection();
    }
    const GDAL_GCP*
                GetGCPs() override;
    CPLErr _SetGCPs(
               int nGCPCount,
               const GDAL_GCP *pasGCPList,
               const char *pszGCPProjection ) override;
    using GDALDataset::SetGCPs;
    CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const OGRSpatialReference* poSRS ) override {
        return OldSetGCPsFromNew(nGCPCount, pasGCPList, poSRS);
    }


    CPLErr IBuildOverviews(
               const char* pszResampling,
               int nOverviews,
               int* panOverviewList,
               int nListBandsover,
               int* panBandList,
               GDALProgressFunc pfnProgress,
               void* pProgresoversData ) override;
    CPLErr CreateMaskBand( int nFlags ) override;
    // cppcheck-suppress functionStatic
    OGRErr StartTransaction(int /* bForce */ =FALSE) override
        { return CE_None; }
    OGRErr CommitTransaction() override { return CE_None; }
    OGRErr RollbackTransaction() override { return CE_None; }

    char** GetFileList() override;

    void                AssignGeoRaster( GeoRasterWrapper* poGRW );
};

//  ---------------------------------------------------------------------------
//  GeoRasterRasterBand, extends GDALRasterBand to support GeoRaster Band
//  ---------------------------------------------------------------------------

class GeoRasterRasterBand final: public GDALRasterBand
{
    friend class GeoRasterDataset;

  public:
    GeoRasterRasterBand( GeoRasterDataset* poGDS,
                         int nBand,
                         int nLevel,
                         GDALDataset* poJP2Dataset = nullptr );
    ~GeoRasterRasterBand() override;

private:
    GeoRasterWrapper*   poGeoRaster;
    GDALColorTable*     poColorTable;
    GDALRasterAttributeTable*
                        poDefaultRAT;
    GDALDataset*        poJP2Dataset;
    double              dfMin;
    double              dfMax;
    double              dfMean;
    double              dfMedian;
    double              dfMode;
    double              dfStdDev;
    bool                bValidStats;
    double              dfNoData;
    char*               pszVATName;
    int                 nOverviewLevel;
    GeoRasterRasterBand** papoOverviews;
    int                 nOverviewCount;
    hNoDataItem*        pahNoDataArray;
    int                 nNoDataArraySz;
    bool                bHasNoDataArray;

    void                ApplyNoDataArray( void* pBuffer ) const;

public:
    double GetNoDataValue( int *pbSuccess = nullptr ) override;
    CPLErr SetNoDataValue( double dfNoDataValue ) override;
    double GetMinimum( int* pbSuccess = nullptr ) override;
    double GetMaximum( int* pbSuccess = nullptr ) override;
    GDALColorTable *GetColorTable() override;
    CPLErr SetColorTable( GDALColorTable *poInColorTable ) override;
    GDALColorInterp GetColorInterpretation() override;
    CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage ) override;
    CPLErr IWriteBlock( int nBlockXOff, int nBlockYOff, void *pImage ) override;
    CPLErr SetStatistics( double dfMin, double dfMax,
                          double dfMean, double dfStdDev ) override;
    CPLErr GetStatistics( int bApproxOK, int bForce,
                          double* pdfMin, double* pdfMax,
                          double* pdfMean, double* pdfStdDev ) override;
    GDALRasterAttributeTable *GetDefaultRAT() override;
    CPLErr      SetDefaultRAT( const GDALRasterAttributeTable *poRAT ) override;
    int         GetOverviewCount() override;
    GDALRasterBand*
                GetOverview( int ) override;
    CPLErr      CreateMaskBand( int nFlags ) override;
    GDALRasterBand *GetMaskBand() override;
    int GetMaskFlags() override;
    bool            IsMaskBand() const override { return nOverviewLevel == DEFAULT_BMP_MASK; }
};

//  ---------------------------------------------------------------------------
//  GeoRasterWrapper, an interface for Oracle Spatial SDO_GEORASTER objects
//  ---------------------------------------------------------------------------

class GeoRasterWrapper
{
  public:
    GeoRasterWrapper();
    virtual ~GeoRasterWrapper();

  private:
    OCILobLocator**     pahLocator;
    unsigned long       nBlockCount;
    unsigned long       nBlockBytes;
    unsigned long       nGDALBlockBytes;
    GByte*              pabyBlockBuf;
    GByte*              pabyCompressBuf;
    OWStatement*        poBlockStmt;

    int                 nCurrentLevel;
    long                nLevelOffset;

    long                nCacheBlockId;
    bool                bFlushBlock;
    unsigned long       nFlushBlockSize;

    bool                bWriteOnly;

    hLevelDetails*      pahLevels;

    int                 nCellSizeBits;
    int                 nGDALCellBytes;

    bool                bUpdate;
    bool                bInitializeIO;
    bool                bFlushMetadata;

    void                InitializeLayersNode();
    bool                InitializeIO();
    void                InitializeLevel( int nLevel );

    void                LoadNoDataValues();

    void                UnpackNBits( GByte* pabyData );
    void                PackNBits( GByte* pabyData ) const;
#ifdef JPEG_SUPPORTED
    unsigned long       CompressJpeg();
    void                UncompressJpeg( unsigned long nBufferSize );

    struct jpeg_decompress_struct sDInfo;
    struct jpeg_compress_struct sCInfo;
    struct jpeg_error_mgr sJErr;
#endif
    unsigned long       CompressDeflate();
    bool                UncompressDeflate( unsigned long nBufferSize );

    void                GetSpatialReference();

public:

    int                 nGCPCount;
    GDAL_GCP*           pasGCPList;
    bool                bFlushGCP;
    void                FlushGCP();

    bool                FlushMetadata();
    static char**       ParseIdentificator( const char* pszStringID );
    static GeoRasterWrapper*
                        Open(
                            const char* pszStringID,
                            bool bUpdate );
    bool                Create(
                            char* pszDescription,
                            char* pszInsert,
                            bool bUpdate );
    bool                Delete();
    void                GetRasterInfo();
    bool                GetStatistics( int nBand,
                                       char* pszMin,
                                       char* pszMax,
                                       char* pszMean,
                                       char* pszMedian,
                                       char* pszMode,
                                       char* pszStdDev,
                                       char* pszSampling );
    bool                SetStatistics( int nBand,
                                       const char* pszMin,
                                       const char* pszMax,
                                       const char* pszMean,
                                       const char* pszMedian,
                                       const char* pszMode,
                                       const char* pszStdDev,
                                       const char* pszSampling );
    bool                HasColorMap( int nBand );
    void                GetColorMap( int nBand, GDALColorTable* poCT );
    void                SetColorMap( int nBand, GDALColorTable* poCT );
    void                SetGeoReference( long long nSRIDIn );
    bool                GetDataBlock(
                            int nBand,
                            int nLevel,
                            int nXOffset,
                            int nYOffset,
                            void* pData );
    bool                SetDataBlock(
                            int nBand,
                            int nLevel,
                            int nXOffset,
                            int nYOffset,
                            void* pData );
    long                GetBlockNumber( int nB, int nX, int nY ) const
                        {
                            return nLevelOffset +
                                   (long) ( ( ceil( (double)
                                   ( ( nB - 1 ) / nBandBlockSize ) ) *
                                   nTotalColumnBlocks * nTotalRowBlocks ) +
                                   ( nY * nTotalColumnBlocks ) + nX );
                        }

    bool                FlushBlock( long nCacheBlock );
    bool                GetNoData( int nLayer, double* pdfNoDataValue );
    bool                SetNoData( int nLayer, const char* pszValue );
    CPLXMLNode*         GetMetadata() { return phMetadata; }
    bool                SetVAT( int nBand, const char* pszName );
    char*               GetVAT( int nBand );
    bool                GeneratePyramid(
                            int nLevels,
                            const char* pszResampling,
                            bool bInternal = false );
    void                DeletePyramid();
    void                PrepareToOverwrite();
    bool                InitializeMask( int nLevel,
                                                int nBlockColumns,
                                                int nBlockRows,
                                                int nColumnBlocks,
                                                int nRowBlocks,
                                                int nBandBlocks );
    void                SetWriteOnly( bool value ) { bWriteOnly = value; }
    void                SetRPC();
    void                SetMaxLevel( int nMaxLevel );
    void                GetRPC();
    void                GetGCP();
    void                SetGCP( int nGCPCountIn, const GDAL_GCP *pasGCPListIn );
    void                QueryWKText();

public:

    OWConnection*       poConnection;

    CPLString           sTable;
    CPLString           sSchema;
    CPLString           sOwner;
    CPLString           sColumn;
    CPLString           sDataTable;
    long long           nRasterId;
    CPLString           sWhere;
    CPLString           sValueAttributeTab;

    long long           nSRID;
    long long           nExtentSRID;
    bool                bGenSpatialExtent;
    bool                bCreateObjectTable;
    CPLXMLNode*         phMetadata;
    CPLString           sCellDepth;

    bool                bGenPyramid;
    CPLString           sPyramidResampling;
    int                 nPyramidLevels;

    CPLString           sCompressionType;
    int                 nCompressQuality;
    CPLString           sWKText;
    CPLString           sAuthority;
    CPLList*            psNoDataList;

    int                 nRasterColumns;
    int                 nRasterRows;
    int                 nRasterBands;

    CPLString           sInterleaving;
    bool                bIsReferenced;

    bool                bBlocking;
    bool                bAutoBlocking;

    double              dfXCoefficient[3];
    double              dfYCoefficient[3];

    int                 nColumnBlockSize;
    int                 nRowBlockSize;
    int                 nBandBlockSize;

    int                 nTotalColumnBlocks;
    int                 nTotalRowBlocks;
    int                 nTotalBandBlocks;

    int                 iDefaultRedBand;
    int                 iDefaultGreenBand;
    int                 iDefaultBlueBand;

    int                 nPyramidMaxLevel;

    bool                bHasBitmapMask;
    bool                bUniqueFound;

    int                 eModelCoordLocation;
    unsigned int        anULTCoordinate[3];

    GDALRPCInfoV2*      phRPC;
};

#endif /* ifndef GEORASTER_PRIV_H_INCLUDED */
