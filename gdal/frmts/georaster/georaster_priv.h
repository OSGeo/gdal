/******************************************************************************
 * $Id: $
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

#ifndef _GEORASTER_PRIV_H_INCLUDED
#define _GEORASTER_PRIV_H_INCLUDED

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

CPL_C_START
#include <jpeglib.h>
CPL_C_END

void jpeg_vsiio_src (j_decompress_ptr cinfo, VSILFILE * infile);
void jpeg_vsiio_dest (j_compress_ptr cinfo, VSILFILE * outfile);

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

#define DEFAULT_BLOCK_ROWS 256
#define DEFAULT_BLOCK_COLUMNS 256

//  Default Model Coordinate Location (internal pixel geo-reference)

#define MCL_CENTER      0
#define MCL_UPPERLEFT   1
#define MCL_DEFAULT     MCL_CENTER

// MAX double string representation

#define MAX_DOUBLE_STR_REP 20

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
//  GeoRaster wrapper classe definitions
//  ---------------------------------------------------------------------------

#include "oci_wrapper.h"

class GeoRasterDataset;
class GeoRasterRasterBand;
class GeoRasterWrapper;

//  ---------------------------------------------------------------------------
//  GeoRasterDataset, extends GDALDataset to support GeoRaster Datasets
//  ---------------------------------------------------------------------------

class GeoRasterDataset : public GDALDataset
{
    friend class GeoRasterRasterBand;

public:
                        GeoRasterDataset();
    virtual            ~GeoRasterDataset();

private:

    GeoRasterWrapper*   poGeoRaster;
    bool                bGeoTransform;
    bool                bForcedSRID;
    char*               pszProjection;
    char**              papszSubdatasets;
    double              adfGeoTransform[6];
    int                 nGCPCount;
    GDAL_GCP*           pasGCPList;
    GeoRasterRasterBand*
                        poMaskBand;
    bool                bApplyNoDataArray;

public:

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
    virtual CPLErr      GetGeoTransform( double* padfTransform );
    virtual CPLErr      SetGeoTransform( double* padfTransform );
    virtual const char* GetProjectionRef( void );
    virtual CPLErr      SetProjection( const char* pszProjString );
    virtual char      **GetMetadataDomainList();
    virtual char**      GetMetadata( const char* pszDomain );
    virtual void        FlushCache( void );
    virtual CPLErr      IRasterIO( GDALRWFlag eRWFlag, 
                            int nXOff, int nYOff, int nXSize, int nYSize,
                            void *pData, int nBufXSize, int nBufYSize, 
                            GDALDataType eBufType,
                            int nBandCount, int *panBandMap, 
                            GSpacing nPixelSpace, GSpacing nLineSpace,
                            GSpacing nBandSpace,
                            GDALRasterIOExtraArg* psExtraArg );
    virtual int         GetGCPCount() { return nGCPCount; }
    virtual const char* GetGCPProjection();
    virtual const GDAL_GCP*
                        GetGCPs() { return pasGCPList; }
    virtual CPLErr      SetGCPs(
                            int nGCPCount,
                            const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );
    virtual CPLErr      IBuildOverviews(
                            const char* pszResampling,
                            int nOverviews,
                            int* panOverviewList,
                            int nListBandsover,
                            int* panBandList,
                            GDALProgressFunc pfnProgress,
                            void* pProgresoversData );
    virtual CPLErr      CreateMaskBand( int nFlags );
    virtual OGRErr      StartTransaction(int bForce=FALSE) {return CE_None;};
    virtual OGRErr      CommitTransaction() {return CE_None;};
    virtual OGRErr      RollbackTransaction() {return CE_None;};
    
    void                AssignGeoRaster( GeoRasterWrapper* poGRW );
};

//  ---------------------------------------------------------------------------
//  GeoRasterRasterBand, extends GDALRasterBand to support GeoRaster Band
//  ---------------------------------------------------------------------------

class GeoRasterRasterBand : public GDALRasterBand
{
    friend class GeoRasterDataset;

public:
                        GeoRasterRasterBand( GeoRasterDataset* poGDS, 
                            int nBand,
                            int nLevel );
    virtual            ~GeoRasterRasterBand();

private:

    GeoRasterWrapper*   poGeoRaster;
    GDALColorTable*     poColorTable;
    GDALRasterAttributeTable*
                        poDefaultRAT;
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
    
    void                ApplyNoDataArry( void* pBuffer );

public:

    virtual double      GetNoDataValue( int *pbSuccess = NULL );
    virtual CPLErr      SetNoDataValue( double dfNoDataValue );
    virtual double      GetMinimum( int* pbSuccess = NULL );
    virtual double      GetMaximum( int* pbSuccess = NULL );
    virtual GDALColorTable* 
                        GetColorTable();
    virtual CPLErr      SetColorTable( GDALColorTable *poInColorTable ); 
    virtual GDALColorInterp   
                        GetColorInterpretation();
    virtual CPLErr      IReadBlock( int nBlockXOff, int nBlockYOff, 
                            void *pImage );
    virtual CPLErr      IWriteBlock( int nBlockXOff, int nBlockYOff, 
                            void *pImage );
    virtual CPLErr      SetStatistics( double dfMin, double dfMax, 
                            double dfMean, double dfStdDev );
    virtual CPLErr      GetStatistics( int bApproxOK, int bForce,
                            double* pdfMin, double* pdfMax, 
                            double* pdfMean, double* pdfStdDev );
    virtual             GDALRasterAttributeTable *GetDefaultRAT();
    virtual CPLErr      SetDefaultRAT( const GDALRasterAttributeTable *poRAT );
    virtual int         GetOverviewCount();
    virtual GDALRasterBand*
                        GetOverview( int );
    virtual CPLErr      CreateMaskBand( int nFlags );
    virtual GDALRasterBand*
                        GetMaskBand();
    virtual int         GetMaskFlags();
};

//  ---------------------------------------------------------------------------
//  GeoRasterWrapper, an interface for Oracle Spatial SDO_GEORASTER objects
//  ---------------------------------------------------------------------------

class GeoRasterWrapper
{

public:

                        GeoRasterWrapper();
    virtual            ~GeoRasterWrapper();

private:

    OCILobLocator**     pahLocator;
    unsigned long       nBlockCount;
    unsigned long       nBlockBytes;
    unsigned long       nGDALBlockBytes;
    GByte*              pabyBlockBuf;
    GByte*              pabyCompressBuf;
    OWStatement*        poBlockStmt;
    OWStatement*        poStmtWrite;

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

    void                InitializeLayersNode( void );
    bool                InitializeIO( void );
    void                InitializeLevel( int nLevel );
    bool                FlushMetadata( void );

    void                LoadNoDataValues( void );

    void                UnpackNBits( GByte* pabyData );
    void                PackNBits( GByte* pabyData );
    unsigned long       CompressJpeg( void );
    unsigned long       CompressDeflate( void );
    void                UncompressJpeg( unsigned long nBufferSize );
    bool                UncompressDeflate( unsigned long nBufferSize );

    struct jpeg_decompress_struct sDInfo;
    struct jpeg_compress_struct sCInfo;
    struct jpeg_error_mgr sJErr;

public:

    static char**       ParseIdentificator( const char* pszStringID );
    static GeoRasterWrapper*
                        Open( 
                            const char* pszStringID,
                            bool bUpdate );
    bool                Create(
                            char* pszDescription,
                            char* pszInsert,
                            bool bUpdate );
    bool                Delete( void );
    void                GetRasterInfo( void );
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
    void                SetGeoReference( int nSRIDIn );
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
    long                GetBlockNumber( int nB, int nX, int nY )
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
    CPLXMLNode*         GetMetadata() { return phMetadata; };
    bool                SetVAT( int nBand, const char* pszName );
    char*               GetVAT( int nBand );
    bool                GeneratePyramid(
                            int nLevels,
                            const char* pszResampling,
                            bool bInternal = false );
    bool                DeletePyramid();
    void                PrepareToOverwrite( void );
    bool                InitializeMask( int nLevel,
                                                int nBlockColumns,
                                                int nBlockRows,
                                                int nColumnBlocks,
                                                int nRowBlocks,
                                                int nBandBlocks );
    void                SetWriteOnly( bool value ) { bWriteOnly = value; };
    void                SetRPC();
    void                GetRPC();

public:

    OWConnection*       poConnection;

    CPLString           sTable;
    CPLString           sSchema;
    CPLString           sOwner;
    CPLString           sColumn;
    CPLString           sDataTable;
    int                 nRasterId;
    CPLString           sWhere;
    CPLString           sValueAttributeTab;

    int                 nSRID;
    int                 nExtentSRID;
    bool                bGenSpatialIndex;
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

    GDALRPCInfo*        phRPC;
};

#endif /* ifndef _GEORASTER_PRIV_H_INCLUDED */
