/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gdal_pam.h"
#define CPL_SERV_H_INCLUDED

#include "tiffio.h"
#include "xtiffio.h"
#include "geotiff.h"
#include "geo_normalize.h"
#include "geovalues.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_minixml.h"
#include "gt_overview.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
char CPL_DLL *  GTIFGetOGISDefn( GTIF *, GTIFDefn * );
int  CPL_DLL   GTIFSetFromOGISDefn( GTIF *, const char * );
const char * GDALDefaultCSVFilename( const char *pszBasename );
GUInt32 HalfToFloat( GUInt16 );
GUInt32 TripleToFloat( GUInt32 );
void    GTiffOneTimeInit();
CPL_C_END

#define TIFFTAG_GDAL_METADATA  42112
#define TIFFTAG_GDAL_NODATA    42113
#define TIFFTAG_RPCCOEFFICIENT 50844

#if TIFFLIB_VERSION >= 20081217 && defined(BIGTIFF_SUPPORT)
#  define HAVE_UNSETFIELD
#endif

TIFF* VSI_TIFFOpen(const char* name, const char* mode);

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

class GTiffDataset : public GDALPamDataset
{
    friend class GTiffRasterBand;
    friend class GTiffSplitBand;
    friend class GTiffRGBABand;
    friend class GTiffBitmapBand;
    friend class GTiffSplitBitmapBand;
    friend class GTiffOddBitsBand;
    
    TIFF	*hTIFF;
    GTiffDataset **ppoActiveDSRef;
    GTiffDataset *poActiveDS; /* only used in actual base */

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

    char	*pszProjection;
    int         bLookedForProjection;

    void        LookForProjection();

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

    int		nGCPCount;
    GDAL_GCP	*pasGCPList;

    int         IsBlockAvailable( int nBlockId );

    int         bGeoTIFFInfoChanged;
    int         bNoDataSet;
    double      dfNoDataValue;

    int	        bMetadataChanged;

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
    int          WriteEncodedTile(uint32 tile, void* data, int bPreserveDataBuffer);
    int          WriteEncodedStrip(uint32 strip, void* data, int bPreserveDataBuffer);

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

    int           bDontReloadFirstBlock; /* Hack for libtiff 3.X and #3633 */

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

    virtual char **GetFileList(void);

    virtual CPLErr IBuildOverviews( const char *, int, int *, int, int *, 
                                    GDALProgressFunc, void * );

    CPLErr	   OpenOffset( TIFF *, GTiffDataset **ppoActiveDSRef, 
                               toff_t nDirOffset, int, GDALAccess, 
                               int bAllowRGBAInterface = TRUE);

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
                              GDALDataType eType, char **papszParmList );

    CPLErr   WriteEncodedTileOrStrip(uint32 tile_or_strip, void* data, int bPreserveDataBuffer);
};

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

protected:
    GTiffDataset       *poGDS;
    GDALMultiDomainMetadata oGTiffMDMD;

    int                bNoDataSet;
    double             dfNoDataValue;

    void NullBlock( void *pData );

public:
                   GTiffRasterBand( GTiffDataset *, int );

    // should override RasterIO eventually.
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * ); 

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr          SetColorTable( GDALColorTable * );
    virtual double	    GetNoDataValue( int * );
    virtual CPLErr	    SetNoDataValue( double );

    virtual double GetOffset( int *pbSuccess = NULL );
    virtual CPLErr SetOffset( double dfNewValue );
    virtual double GetScale( int *pbSuccess = NULL );
    virtual CPLErr SetScale( double dfNewValue );
    virtual CPLErr SetColorInterpretation( GDALColorInterp );

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
        {
            uint16 *v;
            uint16 count = 0;

            if( TIFFGetField( poDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v) )
            {
                if( nBand - 3 <= count && v[nBand-4] == EXTRASAMPLE_ASSOCALPHA )
                    eBandInterp = GCI_AlphaBand;
                else
                    eBandInterp = GCI_Undefined;
            }
            else if( nBand == 4 )
                eBandInterp = GCI_AlphaBand;
            else
                eBandInterp = GCI_Undefined;
        }
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
        {
            uint16 *v;
            uint16 count = 0;

            if( TIFFGetField( poDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v) )
            {
                if( nBand - 3 <= count && v[nBand-4] == EXTRASAMPLE_ASSOCALPHA )
                    eBandInterp = GCI_AlphaBand;
                else
                    eBandInterp = GCI_Undefined;
            }
            else if( nBand == 4 )
                eBandInterp = GCI_AlphaBand;
            else
                eBandInterp = GCI_Undefined;
        }
    }
    else if( poDS->nPhotometric == PHOTOMETRIC_SEPARATED )
    {
        if( nBand == 1 )
            eBandInterp = GCI_CyanBand;
        else if( nBand == 2 )
            eBandInterp = GCI_MagentaBand;
        else if( nBand == 3 )
            eBandInterp = GCI_YellowBand;
        else
            eBandInterp = GCI_BlackBand;
    }
    else if( poDS->nPhotometric == PHOTOMETRIC_MINISBLACK && nBand == 1 )
        eBandInterp = GCI_GrayIndex;
    else
    {
        uint16 *v;
        uint16 count = 0;

        if( TIFFGetField( poDS->hTIFF, TIFFTAG_EXTRASAMPLES, &count, &v ) )
        {
            int nBaseSamples;
            nBaseSamples = poDS->nSamplesPerPixel - count;

            if( nBand > nBaseSamples 
                && v[nBand-nBaseSamples-1] == EXTRASAMPLE_ASSOCALPHA )
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
                                     nBlockReqSize ) == -1 )
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
                                      nBlockReqSize ) == -1 )
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
    if( poGDS->nBands != 1 && eErr == CE_None && !poGDS->bLoadingOtherBands &&
        nBlockXSize * nBlockYSize * (GDALGetDataTypeSize(eDataType) / 8) < GDALGetCacheMax() / poGDS->nBands)
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

    if (!poGDS->SetDirectory())
        return CE_Failure;

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

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

    for( iBand = 0; iBand < poGDS->nBands; iBand++ )
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

        for( i = 0; i < nBlockPixels; i++ )
        {
            memcpy( pabyOut, pabyThisImage, nWordBytes );
            
            pabyOut += nWordBytes * poGDS->nBands;
            pabyThisImage += nWordBytes;
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
        if( papszMD != NULL )
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
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr GTiffRasterBand::SetColorInterpretation( GDALColorInterp eInterp )

{
    if( eInterp == eBandInterp )
        return CE_None;
    
    if( poGDS->bCrystalized )
        return GDALPamRasterBand::SetColorInterpretation( eInterp );

    /* greyscale + alpha */
    else if( eInterp == GCI_AlphaBand 
        && nBand == 2 
        && poGDS->nSamplesPerPixel == 2 
        && poGDS->nPhotometric == PHOTOMETRIC_MINISBLACK )
    {
        uint16 v[1] = { EXTRASAMPLE_ASSOCALPHA };

        TIFFSetField(poGDS->hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
        eBandInterp = eInterp;
        return CE_None;
    }

    /* RGB + alpha */
    else if( eInterp == GCI_AlphaBand 
             && nBand == 4 
             && poGDS->nSamplesPerPixel == 4
             && poGDS->nPhotometric == PHOTOMETRIC_RGB )
    {
        uint16 v[1] = { EXTRASAMPLE_ASSOCALPHA };

        TIFFSetField(poGDS->hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
        eBandInterp = eInterp;
        return CE_None;
    }
    
    else
        return GDALPamRasterBand::SetColorInterpretation( eInterp );
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
    if( poGDS->nSamplesPerPixel != 1 )
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
        memset( pData, 0, nWords*nChunkSize );
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
    if( poGDS->nOverviewCount > 0 )
        return poGDS->nOverviewCount;
    else
        return GDALRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetOverview( int i )

{
    if( poGDS->nOverviewCount > 0 )
    {
        if( i < 0 || i >= poGDS->nOverviewCount )
            return NULL;
        else
            return poGDS->papoOverviewDS[i]->GetRasterBand(nBand);
    }
    else
        return GDALRasterBand::GetOverview( i );
}

/************************************************************************/
/*                           GetMaskFlags()                             */
/************************************************************************/

int GTiffRasterBand::GetMaskFlags()
{
    if( poGDS->poMaskDS != NULL )
    {
        int iBand;
        int nMaskFlag = 0;
        if( poGDS->poMaskDS->GetRasterCount() == 1)
        {
            iBand = 1;
            nMaskFlag = GMF_PER_DATASET;
        }
        else
        {
            iBand = nBand;
        }
        if( poGDS->poMaskDS->GetRasterBand(iBand)->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" ) != NULL 
            && atoi(poGDS->poMaskDS->GetRasterBand(iBand)->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" )) == 1)
        {
            return nMaskFlag;
        }
        else
        {
            return nMaskFlag | GMF_ALPHA;
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
            poGDS->pabyBlockBuf = (GByte *) CPLMalloc(TIFFScanlineSize(poGDS->hTIFF));
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
                              (poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE) ? nBand-1 : 0 ) == -1 )
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
                                 (uint32 *) poGDS->pabyBlockBuf) == -1 )
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
                                  (uint32 *) poGDS->pabyBlockBuf) == -1 )
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
    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId += (nBand-1) * poGDS->nBlocksPerBand;

    /* Only read content from disk in the CONTIG case */
    eErr = poGDS->LoadBlockBuf( nBlockId, 
                                poGDS->nPlanarConfig == PLANARCONFIG_CONTIG && poGDS->nBands > 1 );
    if( eErr != CE_None )
        return eErr;

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
                int  nInWord = 0;
                if( eDataType == GDT_Byte )
                    nInWord = ((GByte *) pImage)[iPixel++];
                else if( eDataType == GDT_UInt16 )
                    nInWord = ((GUInt16 *) pImage)[iPixel++];
                else if( eDataType == GDT_UInt32 )
                    nInWord = ((GUInt32 *) pImage)[iPixel++];
                else
                    CPLAssert(0);


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
                int  nInWord = 0;
                if( eDataType == GDT_Byte )
                    nInWord = ((GByte *) pabyThisImage)[iPixel++];
                else if( eDataType == GDT_UInt16 )
                    nInWord = ((GUInt16 *) pabyThisImage)[iPixel++];
                else if( eDataType == GDT_UInt32 )
                    nInWord = ((GUInt32 *) pabyThisImage)[iPixel++];
                else
                    CPLAssert(0);


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

            for( iPixel = 0; iPixel < nBlockXSize; iPixel++, iSrcOffset++ )
            {
                if( pabyBlockBuf[iSrcOffset >>3] & (0x80 >> (iSrcOffset & 0x7)) )
                    ((GByte *) pImage)[iDstOffset++] = 1;
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
                else
                    CPLAssert(0);
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
    return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffBitmapBand::GetColorTable()

{
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
        poGDS->pabyBlockBuf = (GByte *) CPLMalloc(TIFFScanlineSize(poGDS->hTIFF));

/* -------------------------------------------------------------------- */
/*      Read through to target scanline.                                */
/* -------------------------------------------------------------------- */
    if( poGDS->nLastLineRead >= nBlockYOff )
        poGDS->nLastLineRead = -1;

    while( poGDS->nLastLineRead < nBlockYOff )
    {
        if( TIFFReadScanline( poGDS->hTIFF, poGDS->pabyBlockBuf, ++poGDS->nLastLineRead, 0 ) == -1 )
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
    hTIFF = NULL;
    bNeedsRewrite = FALSE;
    bMetadataChanged = FALSE;
    bGeoTIFFInfoChanged = FALSE;
    bCrystalized = TRUE;
    poColorTable = NULL;
    bNoDataSet = FALSE;
    dfNoDataValue = -9999.0;
    pszProjection = CPLStrdup("");
    bLookedForProjection = FALSE;
    bBase = TRUE;
    bCloseTIFFHandle = FALSE;
    bTreatAsRGBA = FALSE;
    nOverviewCount = 0;
    papoOverviewDS = NULL;
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
    bDontReloadFirstBlock = FALSE;
}

/************************************************************************/
/*                           ~GTiffDataset()                            */
/************************************************************************/

GTiffDataset::~GTiffDataset()

{
    Crystalize();

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
        }
    }

    /* If we are a mask dataset, we can have overviews, but we don't */
    /* own them. We can only free the array, not the overviews themselves */
    CPLFree( papoOverviewDS );

    /* poMaskDS is owned by the main image and the overviews */
    /* so because of the latter case, we can delete it even if */
    /* we are not the base image */
    if (poMaskDS)
        delete poMaskDS;

    if( poColorTable != NULL )
        delete poColorTable;

    if( bBase || bCloseTIFFHandle )
    {
        XTIFFClose( hTIFF );
    }

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CPLFree( pszProjection );

    CSLDestroy( papszCreationOptions );

    CPLFree(pabyTempWriteBuffer);

    if( *ppoActiveDSRef == this )
        *ppoActiveDSRef = NULL;
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
            WriteEncodedTileOrStrip( iBlock, pabyData, FALSE );
    }

    CPLFree( pabyData );
}

/************************************************************************/
/*                        WriteEncodedTile()                            */
/************************************************************************/

int GTiffDataset::WriteEncodedTile(uint32 tile, void* data,
                                   int bPreserveDataBuffer)
{
    /* TIFFWriteEncodedTile can alter the passed buffer if byte-swapping is necessary */
    /* so we use a temporary buffer before calling it */
    int cc = TIFFTileSize( hTIFF );
    if (bPreserveDataBuffer && TIFFIsByteSwapped(hTIFF))
    {
        if (cc != nTempWriteBufferSize)
        {
            pabyTempWriteBuffer = CPLRealloc(pabyTempWriteBuffer, cc);
            nTempWriteBufferSize = cc;
        }
        memcpy(pabyTempWriteBuffer, data, cc);
        return TIFFWriteEncodedTile(hTIFF, tile, pabyTempWriteBuffer, cc);
    }
    else
        return TIFFWriteEncodedTile(hTIFF, tile, data, cc);
}

/************************************************************************/
/*                        WriteEncodedStrip()                           */
/************************************************************************/

int  GTiffDataset::WriteEncodedStrip(uint32 strip, void* data,
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
        memcpy(pabyTempWriteBuffer, data, cc);
        return TIFFWriteEncodedStrip(hTIFF, strip, pabyTempWriteBuffer, cc);
    }
    else
        return TIFFWriteEncodedStrip(hTIFF, strip, data, cc);
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
        if( WriteEncodedTile(tile_or_strip, data, bPreserveDataBuffer) == -1 )
        {
            eErr = CE_Failure;
        }
    }
    else
    {
        if( WriteEncodedStrip(tile_or_strip, data, bPreserveDataBuffer) == -1 )
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
    int nBlocksPerRow = (nRasterXSize + nBlockXSize - 1) / nBlockXSize;
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
                                nBlockReqSize) == -1 )
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
                                 nBlockReqSize) == -1 )
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


/************************************************************************/
/*                          IsBlockAvailable()                          */
/*                                                                      */
/*      Return TRUE if the indicated strip/tile is available.  We       */
/*      establish this by testing if the stripbytecount is zero.  If    */
/*      zero then the block has never been committed to disk.           */
/************************************************************************/

int GTiffDataset::IsBlockAvailable( int nBlockId )

{
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
/* We need at least TIFF 3.7.0 for TIFFGetSizeProc and TIFFClientdata */
#if  TIFFLIB_VERSION > 20041016
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
    if( TIFFCurrentDirOffset(hTIFF) == nDirOffset )
        TIFFFlush( hTIFF );
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
                anOvDirIndexes.push_back( iThisOffset );
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

/* -------------------------------------------------------------------- */
/*      If we don't have read access, then create the overviews         */
/*      externally.                                                     */
/* -------------------------------------------------------------------- */
    if( GetAccess() != GA_Update )
    {
        CPLDebug( "GTiff",
                  "File open for read-only accessing, "
                  "creating overviews externally." );

        return GDALDataset::IBuildOverviews( 
            pszResampling, nOverviews, panOverviewList, 
            nBands, panBandList, pfnProgress, pProgressData );
    }

/* -------------------------------------------------------------------- */
/*      If RRD overviews requested, then invoke generic handling.       */
/* -------------------------------------------------------------------- */
    if( CSLTestBoolean(CPLGetConfigOption( "USE_RRD", "NO" )) )
    {
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
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
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
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nOverviews && eErr == CE_None; i++ )
    {
        int   j;

        for( j = 0; j < nOverviewCount; j++ )
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
                                    nSamplesPerPixel, 128, 128, TRUE,
                                    nCompression, nPhotometric, nSampleFormat, 
                                    panRed, panGreen, panBlue,
                                    nExtraSamples, panExtraSampleValues,
                                    osMetadata );

            if( nOverviewOffset == 0 )
            {
                eErr = CE_Failure;
                continue;
            }

            poODS = new GTiffDataset();
            if( poODS->OpenOffset( hTIFF, ppoActiveDSRef, nOverviewOffset, FALSE, 
                                   GA_Update ) != CE_None )
            {
                delete poODS;
                eErr = CE_Failure;
            }
            else
            {
                nOverviewCount++;
                papoOverviewDS = (GTiffDataset **)
                    CPLRealloc(papoOverviewDS, 
                               nOverviewCount * (sizeof(void*)));
                papoOverviewDS[nOverviewCount-1] = poODS;
                poODS->poBaseDS = this;
            }
        }
        else
            panOverviewList[i] *= -1;
    }

    CPLFree(panExtraSampleValues);
    panExtraSampleValues = NULL;

/* -------------------------------------------------------------------- */
/*      Create overviews for the mask.                                  */
/* -------------------------------------------------------------------- */

    if (poMaskDS != NULL &&
        poMaskDS->GetRasterCount() == 1 &&
        CSLTestBoolean(CPLGetConfigOption("GDAL_TIFF_INTERNAL_MASK", "NO")))
    {
        for( i = 0; i < nOverviewCount; i++ )
        {
            if (papoOverviewDS[i]->poMaskDS == NULL)
            {
                toff_t	nOverviewOffset;

                nOverviewOffset = 
                    GTIFFWriteDirectory(hTIFF, FILETYPE_REDUCEDIMAGE | FILETYPE_MASK,
                                        papoOverviewDS[i]->nRasterXSize, papoOverviewDS[i]->nRasterYSize, 
                                        1, PLANARCONFIG_CONTIG,
                                        1, 128, 128, TRUE,
                                        COMPRESSION_NONE, PHOTOMETRIC_MASK, SAMPLEFORMAT_UINT, 
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
/*                          WriteGeoTIFFInfo()                          */
/************************************************************************/

void GTiffDataset::WriteGeoTIFFInfo()

{
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
        // by writing a dummy goekey directory. (#2546)
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

        psGTIF = GTIFNew( hTIFF );  

        // set according to coordinate system.
        GTIFSetFromOGISDefn( psGTIF, pszProjection );

        if( GetMetadataItem( GDALMD_AREA_OR_POINT ) 
            && EQUAL(GetMetadataItem(GDALMD_AREA_OR_POINT),
                     GDALMD_AOP_POINT) )
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
/*                         WriteMDMDMetadata()                          */
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
        if( EQUAL(papszDomainList[iDomain], "RPC") )
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
            }
            
/* -------------------------------------------------------------------- */
/*      Convert into XML item or handle as a special TIFF tag.          */
/* -------------------------------------------------------------------- */
            if( strlen(papszDomainList[iDomain]) == 0 
                && nBand == 0 && EQUALN(pszItemName,"TIFFTAG_",8) )
            {
                if( EQUAL(pszItemName,"TIFFTAG_DOCUMENTNAME") )
                    TIFFSetField( hTIFF, TIFFTAG_DOCUMENTNAME, pszItemValue );
                else if( EQUAL(pszItemName,"TIFFTAG_IMAGEDESCRIPTION") )
                    TIFFSetField( hTIFF, TIFFTAG_IMAGEDESCRIPTION, pszItemValue );
                else if( EQUAL(pszItemName,"TIFFTAG_SOFTWARE") )
                    TIFFSetField( hTIFF, TIFFTAG_SOFTWARE, pszItemValue );
                else if( EQUAL(pszItemName,"TIFFTAG_DATETIME") )
                    TIFFSetField( hTIFF, TIFFTAG_DATETIME, pszItemValue );
                else if( EQUAL(pszItemName,"TIFFTAG_ARTIST") )
                    TIFFSetField( hTIFF, TIFFTAG_ARTIST, pszItemValue );
                else if( EQUAL(pszItemName,"TIFFTAG_HOSTCOMPUTER") )
                    TIFFSetField( hTIFF, TIFFTAG_HOSTCOMPUTER, pszItemValue );
                else if( EQUAL(pszItemName,"TIFFTAG_COPYRIGHT") )
                    TIFFSetField( hTIFF, TIFFTAG_COPYRIGHT, pszItemValue );
                else if( EQUAL(pszItemName,"TIFFTAG_XRESOLUTION") )
                    TIFFSetField( hTIFF, TIFFTAG_XRESOLUTION, atof(pszItemValue) );
                else if( EQUAL(pszItemName,"TIFFTAG_YRESOLUTION") )
                    TIFFSetField( hTIFF, TIFFTAG_YRESOLUTION, atof(pszItemValue) );
                else if( EQUAL(pszItemName,"TIFFTAG_RESOLUTIONUNIT") )
                    TIFFSetField( hTIFF, TIFFTAG_RESOLUTIONUNIT, atoi(pszItemValue) );
            }
            else if( nBand == 0 && EQUAL(pszItemName,GDALMD_AREA_OR_POINT) )
                /* do nothing, handled elsewhere */;
            else
                AppendMetadataItem( ppsRoot, ppsTail, 
                                    pszItemName, pszItemValue, 
                                    nBand, NULL, papszDomainList[iDomain] );

            CPLFree( pszItemName );
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

        int bSuccess;
        double dfOffset = poBand->GetOffset( &bSuccess );
        double dfScale = poBand->GetScale();

        if( bSuccess && (dfOffset != 0.0 || dfScale != 1.0) )
        {
            char szValue[128];

            sprintf( szValue, "%.18g", dfOffset );
            AppendMetadataItem( &psRoot, &psTail, "OFFSET", szValue, nBand, 
                                "offset", "" );
            sprintf( szValue, "%.18g", dfScale );
            AppendMetadataItem( &psRoot, &psTail, "SCALE", szValue, nBand, 
                                "scale", "" );
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
                || EQUAL(papszDomainList[iDomain],"IMAGE_STRUCTURE") )
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
            int bSuccess;
            double dfOffset = poBand->GetOffset( &bSuccess );
            double dfScale = poBand->GetScale();

            if( bSuccess && (dfOffset != 0.0 || dfScale != 1.0) )
            {
                poBand->GDALPamRasterBand::SetScale( dfScale );
                poBand->GDALPamRasterBand::SetOffset( dfOffset );
            }
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
    char **papszMD = NULL;
    CPLString osField;
    CPLString osMultiField;
    int i;
    uint16 nCount;

    if( !TIFFGetField( hTIFF, TIFFTAG_RPCCOEFFICIENT, &nCount, &padfRPCTag ) 
        || nCount != 92 )
        return;

    osField.Printf( "%.15g", padfRPCTag[2] );
    papszMD = CSLSetNameValue( papszMD, "LINE_OFF", osField );

    osField.Printf( "%.15g", padfRPCTag[3] );
    papszMD = CSLSetNameValue( papszMD, "SAMP_OFF", osField );

    osField.Printf( "%.15g", padfRPCTag[4] );
    papszMD = CSLSetNameValue( papszMD, "LAT_OFF", osField );

    osField.Printf( "%.15g", padfRPCTag[5] );
    papszMD = CSLSetNameValue( papszMD, "LONG_OFF", osField );

    osField.Printf( "%.15g", padfRPCTag[6] );
    papszMD = CSLSetNameValue( papszMD, "HEIGHT_OFF", osField );

    osField.Printf( "%.15g", padfRPCTag[7] );
    papszMD = CSLSetNameValue( papszMD, "LINE_SCALE", osField );

    osField.Printf( "%.15g", padfRPCTag[8] );
    papszMD = CSLSetNameValue( papszMD, "SAMP_SCALE", osField );

    osField.Printf( "%.15g", padfRPCTag[9] );
    papszMD = CSLSetNameValue( papszMD, "LAT_SCALE", osField );

    osField.Printf( "%.15g", padfRPCTag[10] );
    papszMD = CSLSetNameValue( papszMD, "LONG_SCALE", osField );

    osField.Printf( "%.15g", padfRPCTag[11] );
    papszMD = CSLSetNameValue( papszMD, "HEIGHT_SCALE", osField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", padfRPCTag[12+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    papszMD = CSLSetNameValue( papszMD, "LINE_NUM_COEFF", osMultiField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", padfRPCTag[32+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    papszMD = CSLSetNameValue( papszMD, "LINE_DEN_COEFF", osMultiField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", padfRPCTag[52+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    papszMD = CSLSetNameValue( papszMD, "SAMP_NUM_COEFF", osMultiField );

    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", padfRPCTag[72+i] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    papszMD = CSLSetNameValue( papszMD, "SAMP_DEN_COEFF", osMultiField );

    oGTiffMDMD.SetMetadata( papszMD, "RPC" );
    CSLDestroy( papszMD );
}

/************************************************************************/
/*                         WriteNoDataValue()                           */
/************************************************************************/

void GTiffDataset::WriteNoDataValue( TIFF *hTIFF, double dfNoData )

{
    TIFFSetField( hTIFF, TIFFTAG_GDAL_NODATA, 
                  CPLString().Printf( "%.18g", dfNoData ).c_str() );
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

    int jquality = -1, zquality = -1; 

    if( GetAccess() == GA_Update )
    {
        TIFFGetField(hTIFF, TIFFTAG_JPEGQUALITY, &jquality); 
        TIFFGetField(hTIFF, TIFFTAG_ZIPQUALITY, &zquality); 

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
        if(jquality > 0) 
        {
            CPLDebug( "GTiff", "Propgate JPEG_QUALITY(%d) in SetDirectory()",
                      jquality );
            TIFFSetField(hTIFF, TIFFTAG_JPEGQUALITY, jquality); 
        }
        if(zquality > 0) 
            TIFFSetField(hTIFF, TIFFTAG_ZIPQUALITY, zquality);
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
    if( poOpenInfo->nHeaderBytes < 2 )
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

    GTiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
	hTIFF = VSI_TIFFOpen( pszFilename, "r" );
    else
        hTIFF = VSI_TIFFOpen( pszFilename, "r+" );
    
    if( hTIFF == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset 	*poDS;

    poDS = new GTiffDataset();
    poDS->SetDescription( pszFilename );
    poDS->osFilename = pszFilename;
    poDS->poActiveDS = poDS;

    if( poDS->OpenOffset( hTIFF, &(poDS->poActiveDS),
                          TIFFCurrentDirOffset(hTIFF), TRUE,
                          poOpenInfo->eAccess, 
                          bAllowRGBAInterface ) != CE_None )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();
    poDS->ApplyPamInfo();

    poDS->bMetadataChanged = FALSE;
    poDS->bGeoTIFFInfoChanged = FALSE;

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, pszFilename );
    
    return poDS;
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
    GTIFDefn	sGTIFDefn;

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
        if( GTIFGetDefn( hGTIF, &sGTIFDefn ) )
        {
            pszProjection = GTIFGetOGISDefn( hGTIF, &sGTIFDefn );
            
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
}

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
        bLookedForProjection= TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Copy any PAM metadata into our GeoTIFF context, but with the    */
/*      GeoTIFF context overriding the PAM info.                        */
/* -------------------------------------------------------------------- */
    char **papszPamDomains = oMDMD.GetDomainList();

    for( int iDomain = 0; papszPamDomains && papszPamDomains[iDomain] != NULL; iDomain++ )
    {
        const char *pszDomain = papszPamDomains[iDomain];
        char **papszGT_MD = oGTiffMDMD.GetMetadata( pszDomain );
        char **papszPAM_MD = CSLDuplicate(oMDMD.GetMetadata( pszDomain ));

        papszPAM_MD = CSLMerge( papszPAM_MD, papszGT_MD );

        oGTiffMDMD.SetMetadata( papszPAM_MD, pszDomain );
        CSLDestroy( papszPAM_MD );
    }

    for( int i = 1; i <= GetRasterCount(); i++)
    {
        GTiffRasterBand* poBand = (GTiffRasterBand *)GetRasterBand(i);
        papszPamDomains = poBand->oMDMD.GetDomainList();

        for( int iDomain = 0; papszPamDomains && papszPamDomains[iDomain] != NULL; iDomain++ )
        {
            const char *pszDomain = papszPamDomains[iDomain];
            char **papszGT_MD = poBand->oGTiffMDMD.GetMetadata( pszDomain );
            char **papszPAM_MD = CSLDuplicate(poBand->oMDMD.GetMetadata( pszDomain ));

            papszPAM_MD = CSLMerge( papszPAM_MD, papszGT_MD );

            poBand->oGTiffMDMD.SetMetadata( papszPAM_MD, pszDomain );
            CSLDestroy( papszPAM_MD );
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

    GTiffOneTimeInit();

    hTIFF = VSI_TIFFOpen( pszFilename, "r" );
    if( hTIFF == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      If a directory was requested by index, advance to it now.       */
/* -------------------------------------------------------------------- */
    if( !bAbsolute )
    {
        while( nOffset > 1 )
        {
            if( TIFFReadDirectory( hTIFF ) == 0 )
            {
                XTIFFClose( hTIFF );
                CPLError( CE_Failure, CPLE_OpenFailed, 
                          "Requested directory %lu not found.", (long unsigned int)nOffset );
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
                          nOffset, FALSE, GA_ReadOnly, bAllowRGBAInterface ) != CE_None )
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
                                 int bAllowRGBAInterface)

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

        nBlockXSize = nRasterXSize;
        nBlockYSize = MIN(nRowsPerStrip,nYSize);
    }
        
    nBlocksPerBand =
        ((nYSize + nBlockYSize - 1) / nBlockYSize)
        * ((nXSize + nBlockXSize  - 1) / nBlockXSize);

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
        /* YCbCr JPEG compressed TIFF (see #3259). BUG_3259_FIXED is defined */
        /* in internal libtiff tif_config.h until a 4.0.0beta6 is released */
#if (TIFFLIB_VERSION <= 20091104 && !defined(BIGTIFF_SUPPORT)) || \
    (TIFFLIB_VERSION <= 20091104 && defined(BIGTIFF_SUPPORT) && !defined(BUG_3259_FIXED))
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
    if( bBaseIn )
    {
        char    *pszTabWKT = NULL;
        double	*padfTiePoints, *padfScale, *padfMatrix;
        uint16	nCount;

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
            bGeoTransformValid = TRUE;
        }

/* -------------------------------------------------------------------- */
/*      Otherwise try looking for a .tfw, .tifw or .wld file.           */
/* -------------------------------------------------------------------- */
        else
        {
            bGeoTransformValid = 
                GDALReadWorldFile( osFilename, NULL, adfGeoTransform );

            if( !bGeoTransformValid )
            {
                bGeoTransformValid = 
                    GDALReadWorldFile( osFilename, "wld", adfGeoTransform );
            }

            if( !bGeoTransformValid )
            {
                int bTabFileOK = 
                    GDALReadTabFile( osFilename, adfGeoTransform, 
                                     &pszTabWKT, &nGCPCount, &pasGCPList );

                if( bTabFileOK && nGCPCount == 0 )
                    bGeoTransformValid = TRUE;
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
    }

/* -------------------------------------------------------------------- */
/*      Capture some other potentially interesting information.         */
/* -------------------------------------------------------------------- */
    char	*pszText, szWorkMDI[200];
    float   fResolution;
    uint16  nShort;

    if( TIFFGetField( hTIFF, TIFFTAG_DOCUMENTNAME, &pszText ) )
        SetMetadataItem( "TIFFTAG_DOCUMENTNAME",  pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_IMAGEDESCRIPTION, &pszText ) )
        SetMetadataItem( "TIFFTAG_IMAGEDESCRIPTION", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_SOFTWARE, &pszText ) )
        SetMetadataItem( "TIFFTAG_SOFTWARE", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_DATETIME, &pszText ) )
        SetMetadataItem( "TIFFTAG_DATETIME", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_ARTIST, &pszText ) )
        SetMetadataItem( "TIFFTAG_ARTIST", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_HOSTCOMPUTER, &pszText ) )
        SetMetadataItem( "TIFFTAG_HOSTCOMPUTER", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_COPYRIGHT, &pszText ) )
        SetMetadataItem( "TIFFTAG_COPYRIGHT", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_XRESOLUTION, &fResolution ) )
    {
        sprintf( szWorkMDI, "%.8g", fResolution );
        SetMetadataItem( "TIFFTAG_XRESOLUTION", szWorkMDI );
    }

    if( TIFFGetField( hTIFF, TIFFTAG_YRESOLUTION, &fResolution ) )
    {
        sprintf( szWorkMDI, "%.8g", fResolution );
        SetMetadataItem( "TIFFTAG_YRESOLUTION", szWorkMDI );
    }

    if( TIFFGetField( hTIFF, TIFFTAG_MINSAMPLEVALUE, &nShort ) )
    {
        sprintf( szWorkMDI, "%d", nShort );
        SetMetadataItem( "TIFFTAG_MINSAMPLEVALUE", szWorkMDI );
    }

    if( TIFFGetField( hTIFF, TIFFTAG_MAXSAMPLEVALUE, &nShort ) )
    {
        sprintf( szWorkMDI, "%d", nShort );
        SetMetadataItem( "TIFFTAG_MAXSAMPLEVALUE", szWorkMDI );
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
                        poBand->SetScale( atof(pszUnescapedValue) );
                    else if( EQUAL(pszRole,"offset") )
                        poBand->SetOffset( atof(pszUnescapedValue) );
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
/*      Check for RPC metadata in an RPB file.                          */
/* -------------------------------------------------------------------- */
    if( bBaseIn )
    {
        char **papszRPCMD = GDALLoadRPBFile( osFilename, NULL );
        
        if( papszRPCMD != NULL )
        {
            oGTiffMDMD.SetMetadata( papszRPCMD, "RPC" );
            CSLDestroy( papszRPCMD );
            bMetadataChanged = FALSE;
        }
        else
            ReadRPCTag();
    }

/* -------------------------------------------------------------------- */
/*      Check for RPC metadata in an RPB file.                          */
/* -------------------------------------------------------------------- */
    if( bBaseIn )
    {
        char **papszIMDMD = GDALLoadIMDFile( osFilename, NULL );

        if( papszIMDMD != NULL )
        {
            oGTiffMDMD.SetMetadata( papszIMDMD, "IMD" );
            CSLDestroy( papszIMDMD );
            bMetadataChanged = FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for NODATA                                                */
/* -------------------------------------------------------------------- */
    if( TIFFGetField( hTIFF, TIFFTAG_GDAL_NODATA, &pszText ) )
    {
        bNoDataSet = TRUE;
        dfNoDataValue = atof( pszText );
    }

/* -------------------------------------------------------------------- */
/*      If this is a "base" raster, we should scan for any              */
/*      associated overviews, internal mask bands and subdatasets.      */
/* -------------------------------------------------------------------- */
    if( bBase )
    {
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
                }
            }
            
            /* Embedded mask of an overview */
            /* The TIFF6 specification allows the combination of the FILETYPE_xxxx masks */
            else if (nSubType & (FILETYPE_REDUCEDIMAGE | FILETYPE_MASK))
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
            else if( nSubType == 0 ) {
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

    return( CE_None );
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
                              char **papszParmList )

{
    TIFF		*hTIFF;
    int                 nBlockXSize = 0, nBlockYSize = 0;
    int                 bTiled = FALSE;
    uint16              nCompression = COMPRESSION_NONE;
    int                 nPredictor = 1, nJpegQuality = -1, nZLevel = -1;
    uint16              nSampleFormat;
    int			nPlanar;
    const char          *pszValue;
    const char          *pszProfile;
    int                 bCreateBigTIFF = FALSE;

    GTiffOneTimeInit();

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
        else
            CPLError( CE_Warning, CPLE_IllegalArg, 
                      "COMPRESS=%s value not recognised, ignoring.",
                      pszValue );

#if defined(TIFFLIB_VERSION) && TIFFLIB_VERSION > 20031007 /* 3.6.0 */
        if (nCompression != COMPRESSION_NONE &&
            !TIFFIsCODECConfigured(nCompression))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Cannot create TIFF file due to missing codec for %s.", pszValue );
            return NULL;
        }
#endif
    }

    pszValue = CSLFetchNameValue( papszParmList, "PREDICTOR" );
    if( pszValue  != NULL )
        nPredictor =  atoi( pszValue );

    nZLevel = GTiffGetZLevel(papszParmList);

    nJpegQuality = GTiffGetJpegQuality(papszParmList);

/* -------------------------------------------------------------------- */
/*      Compute the uncompressed size.                                  */
/* -------------------------------------------------------------------- */
    double  dfUncompressedImageSize;

    dfUncompressedImageSize = 
        nXSize * ((double)nYSize) * nBands * (GDALGetDataTypeSize(eType)/8);

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
    hTIFF = VSI_TIFFOpen( pszFilename, szOpeningFlag );
    if( hTIFF == NULL )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attempt to create new tiff file `%s'\n"
                      "failed in XTIFFOpen().\n",
                      pszFilename );
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
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
            nSamplesAccountedFor = 1;
            bForceColorTable = TRUE;
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
                return NULL;
            }

            if ( nPlanar == PLANARCONFIG_SEPARATE )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "PHOTOMETRIC=YCBCR requires INTERLEAVE=PIXEL");
                XTIFFClose(hTIFF);
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

            v[0] = EXTRASAMPLE_ASSOCALPHA;
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

        if( CSLFetchBoolean(papszParmList,"ALPHA",FALSE) )
            v[0] = EXTRASAMPLE_ASSOCALPHA;
        else
            v[0] = EXTRASAMPLE_UNSPECIFIED;
            
        for( i = 1; i < nExtraSamples; i++ )
            v[i] = EXTRASAMPLE_UNSPECIFIED;

        TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, nExtraSamples, v );
        
        CPLFree(v);
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
    if( nCompression == COMPRESSION_JPEG 
        && nJpegQuality != -1 )
        TIFFSetField( hTIFF, TIFFTAG_JPEGQUALITY, nJpegQuality );

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

/* -------------------------------------------------------------------- */
/*      Create the underlying TIFF file.                                */
/* -------------------------------------------------------------------- */
    hTIFF = CreateLL( pszFilename, nXSize, nYSize, nBands, 
                      eType, papszParmList );

    if( hTIFF == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    poDS = new GTiffDataset();
    poDS->hTIFF = hTIFF;
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
        ((nYSize + poDS->nBlockYSize - 1) / poDS->nBlockYSize)
        * ((nXSize + poDS->nBlockXSize - 1) / poDS->nBlockXSize);

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
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    hTIFF = CreateLL( pszFilename, nXSize, nYSize, nBands, 
                      eType, papszCreateOptions );

    CSLDestroy( papszCreateOptions );

    if( hTIFF == NULL )
        return NULL;

    TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &nPlanarConfig );
    TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &nBitsPerSample );

/* -------------------------------------------------------------------- */
/*      Are we really producing an RGBA image?  If so, set the          */
/*      associated alpha information.                                   */
/* -------------------------------------------------------------------- */
    int bForcePhotometric = 
        CSLFetchNameValue(papszOptions,"PHOTOMETRIC") != NULL;

    if( nBands == 4 && !bForcePhotometric
        && poSrcDS->GetRasterBand(4)->GetColorInterpretation()==GCI_AlphaBand)
    {
        uint16 v[1];

        v[0] = EXTRASAMPLE_ASSOCALPHA;
	TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    }

/* -------------------------------------------------------------------- */
/*      If the output is jpeg compressed, and the input is RGB make     */
/*      sure we note that.                                              */
/* -------------------------------------------------------------------- */
    uint16      nCompression;

    if( !TIFFGetField( hTIFF, TIFFTAG_COMPRESSION, &(nCompression) ) )
        nCompression = COMPRESSION_NONE;

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
    if( nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL 
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
    else if( nBands == 1 
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
                  "can only be written to 1 band Byte or UInt16 GeoTIFF files." );

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
/*      Write affine transform if it is meaningful.                     */
/* -------------------------------------------------------------------- */
    const char *pszProjection = NULL;
    double      adfGeoTransform[6];
    
    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0 || ABS(adfGeoTransform[5]) != 1.0 ))
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

/* -------------------------------------------------------------------- */
/*      If we are writing jpeg compression we need to write some        */
/*      imagery to force the jpegtables to get created.  This is,       */
/*      likely only needed with libtiff 3.9.x.                          */
/* -------------------------------------------------------------------- */
    int bDontReloadFirstBlock = FALSE;
    if( nCompression == COMPRESSION_JPEG
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

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    
    TIFFWriteCheck( hTIFF, TIFFIsTiled(hTIFF), "GTiffCreateCopy()");
    TIFFWriteDirectory( hTIFF );
    TIFFFlush( hTIFF );
    XTIFFClose( hTIFF );
    hTIFF = NULL;

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

    poDS = (GTiffDataset *) GDALOpen( osFileName, GA_Update );
    if( poDS == NULL )
        poDS = (GTiffDataset *) GDALOpen( osFileName, GA_ReadOnly );

    if ( poDS == NULL )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }

    poDS->osProfile = pszProfile;
    poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
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
        char **papszSRC_MD = poSrcDS->GetRasterBand(nBand)->GetMetadata();
        char **papszDST_MD = CSLDuplicate(poDS->GetRasterBand(nBand)->GetMetadata());

        papszDST_MD = CSLMerge( papszDST_MD, papszSRC_MD );

        poDS->GetRasterBand(nBand)->SetMetadata( papszDST_MD );
        CSLDestroy( papszDST_MD );
    }

    hTIFF = (TIFF*) poDS->GetInternalHandle(NULL);

/* -------------------------------------------------------------------- */
/*      Second chance : now that we have a PAM dataset, it is possible  */
/*      to write metadata that we couldn't be writen as TIFF tag        */
/* -------------------------------------------------------------------- */
    if (!bHasWrittenMDInGeotiffTAG)
        GTiffDataset::WriteMetadata( poDS, hTIFF, TRUE, pszProfile,
                                     pszFilename, papszOptions, TRUE /* don't write RPC and IMG file again */);

    /* To avoid unnecessary directory rewriting */
    poDS->bMetadataChanged = FALSE;
    poDS->bGeoTIFFInfoChanged = FALSE;

    /* We must re-set the compression level at this point, since it has */
    /* been lost a few lines above when closing the newly create TIFF file */
    /* The TIFFTAG_ZIPQUALITY & TIFFTAG_JPEGQUALITY are not store in the TIFF file. */
    /* They are just TIFF session parameters */
    if (nCompression == COMPRESSION_ADOBE_DEFLATE)
    {
        int nZLevel = GTiffGetZLevel(papszOptions);
        if (nZLevel != -1)
        {
            TIFFSetField( hTIFF, TIFFTAG_ZIPQUALITY, nZLevel );
        }
    }
    else if( nCompression == COMPRESSION_JPEG)
    {
        int nJpegQuality = GTiffGetJpegQuality(papszOptions);
        if (nJpegQuality != -1)
        {
            TIFFSetField( hTIFF, TIFFTAG_JPEGQUALITY, nJpegQuality );
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy actual imagery.                                            */
/* -------------------------------------------------------------------- */

    if (poDS->bTreatAsSplit || poDS->bTreatAsSplitBitmap)
    {
        /* For split bands, we use TIFFWriteScanline() interface */
        CPLAssert(poDS->nBitsPerSample == 8 || poDS->nBitsPerSample == 1);
        
        if (poDS->nPlanarConfig == PLANARCONFIG_CONTIG && poDS->nBands > 1)
        {
            int j;
            GByte* pabyScanline = (GByte *) CPLMalloc(TIFFScanlineSize(hTIFF));
            eErr = CE_None;
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
                if( !pfnProgress( (j+1) * 1.0 / nYSize, NULL, pProgressData ) )
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
                        TIFFWriteScanline( hTIFF, pabyScanline, j, iBand - 1) == -1)
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "TIFFWriteScanline() failed." );
                        eErr = CE_Failure;
                    }
                    if( !pfnProgress( (j+1 + (iBand - 1) * nYSize) * 1.0 /
                                      (nBands * nYSize), NULL, pProgressData ) )
                        eErr = CE_Failure;
                }
            }
            CPLFree(pabyScanline);
        }
        
        /* Necessary to be able to read the file without re-opening */
        TIFFFlush( hTIFF );
    }
    else
    {
        char* papszCopyWholeRasterOptions[2] = { NULL, NULL };
        if (nCompression != COMPRESSION_NONE)
            papszCopyWholeRasterOptions[0] = (char*) "COMPRESSED=YES";
        eErr = GDALDatasetCopyWholeRaster( (GDALDatasetH) poSrcDS, 
                                            (GDALDatasetH) poDS,
                                            papszCopyWholeRasterOptions,
                                            pfnProgress, pProgressData );
    }

    if( eErr == CE_Failure )
    {
        delete poDS;
        poDS = NULL;

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
        && !EQUALN(pszNewProjection,"LOCAL_CS",6)
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Only OGC WKT Projections supported for writing to GeoTIFF.\n"
                "%s not supported.",
                  pszNewProjection );
        
        return CE_Failure;
    }
    
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
        memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
        bGeoTransformValid = TRUE;
        bGeoTIFFInfoChanged = TRUE;

        return( CE_None );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
      "SetGeoTransform() is only supported on newly created GeoTIFF files." );
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
        return pszProjection;
    }
    else
        return GDALPamDataset::GetGCPProjection();
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
/*                            GetMetadata()                             */
/************************************************************************/

char **GTiffDataset::GetMetadata( const char * pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"ProxyOverviewRequest") )
        return GDALPamDataset::GetMetadata( pszDomain );

    /* FIXME ? Should we call LookForProjection() to load GDALMD_AREA_OR_POINT ? */
    /* This can impact performances */

    return oGTiffMDMD.GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/
CPLErr GTiffDataset::SetMetadata( char ** papszMD, const char *pszDomain )

{
    if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
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

    if( (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        pszName != NULL && EQUAL(pszName, GDALMD_AREA_OR_POINT) )
    {
        LookForProjection();
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
    if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
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
/*                            GetFileList()                             */
/************************************************************************/

char **GTiffDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();
    VSIStatBufL sStatBuf;

/* -------------------------------------------------------------------- */
/*      Check for .imd file.                                            */
/* -------------------------------------------------------------------- */
    CPLString osTarget = CPLResetExtension( osFilename, "IMD" );
    if( VSIStatL( osTarget, &sStatBuf ) == 0 )
        papszFileList = CSLAddString( papszFileList, osTarget );
    else
    {
        osTarget = CPLResetExtension( osFilename, "imd" );
        if( VSIStatL( osTarget, &sStatBuf ) == 0 )
            papszFileList = CSLAddString( papszFileList, osTarget );
    }
    
/* -------------------------------------------------------------------- */
/*      Check for .rpb file.                                            */
/* -------------------------------------------------------------------- */
    osTarget = CPLResetExtension( osFilename, "RPB" );
    if( VSIStatL( osTarget, &sStatBuf ) == 0 )
        papszFileList = CSLAddString( papszFileList, osTarget );
    else
    {
        osTarget = CPLResetExtension( osFilename, "rpb" );
        if( VSIStatL( osTarget, &sStatBuf ) == 0 )
            papszFileList = CSLAddString( papszFileList, osTarget );
    }

    return papszFileList;
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr GTiffDataset::CreateMaskBand(int nFlags)
{
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

        if (nFlags != GMF_PER_DATASET)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The only flag value supported for internal mask is GMF_PER_DATASET");
            return CE_Failure;
        }

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

        TIFFFlush( hTIFF );

        bIsTiled = TIFFIsTiled(hTIFF);

        nOffset = GTIFFWriteDirectory(hTIFF,
                                      (bIsOverview) ? FILETYPE_REDUCEDIMAGE | FILETYPE_MASK : FILETYPE_MASK,
                                       nRasterXSize, nRasterYSize,
                                       1, PLANARCONFIG_CONTIG, 1,
                                       nBlockXSize, nBlockYSize,
                                       bIsTiled, COMPRESSION_NONE, PHOTOMETRIC_MASK,
                                       SAMPLEFORMAT_UINT, NULL, NULL, NULL, 0, NULL, "");
        if (nOffset == 0)
            return CE_Failure;

        poMaskDS = new GTiffDataset();
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

    if( strstr(fmt,"unknown field") != NULL )
        return;

    pszModFmt = PrepareTIFFErrorFormat( module, fmt );
    CPLErrorV( CE_Warning, CPLE_AppDefined, pszModFmt, ap );
    CPLFree( pszModFmt );
}

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
void
GTiffErrorHandler(const char* module, const char* fmt, va_list ap )
{
    char *pszModFmt;

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

void GTiffOneTimeInit()

{
    static int bOneTimeInitDone = FALSE;
    
    if( bOneTimeInitDone )
        return;

    bOneTimeInitDone = TRUE;

    _ParentExtender = TIFFSetTagExtender(GTiffTagExtender);

    TIFFSetWarningHandler( GTiffWarningHandler );
    TIFFSetErrorHandler( GTiffErrorHandler );

    // This only really needed if we are linked to an external libgeotiff
    // with its own (lame) file searching logic. 
    SetCSVFilenameHook( GDALDefaultCSVFilename );
}

/************************************************************************/
/*                        GDALDeregister_GTiff()                        */
/************************************************************************/

void GDALDeregister_GTiff( GDALDriver * )

{
    CPLDebug( "GDAL", "GDALDeregister_GTiff() called." );
    CSVDeaccess( NULL );

#if defined(LIBGEOTIFF_VERSION) && LIBGEOTIFF_VERSION > 1150
    GTIFDeaccessCSV();
#endif
}

/************************************************************************/
/*                          GDALRegister_GTiff()                        */
/************************************************************************/

void GDALRegister_GTiff()

{
    if( GDALGetDriverByName( "GTiff" ) == NULL )
    {
        GDALDriver	*poDriver;
        char szCreateOptions[3072];
        char szOptionalCompressItems[500];
        int bHasJPEG = FALSE, bHasLZW = FALSE, bHasDEFLATE = FALSE;

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
            strcat( szCreateOptions, ""
"   <Option name='JPEG_QUALITY' type='int' description='JPEG quality 1-100' default='75'/>");
        if (bHasDEFLATE)
            strcat( szCreateOptions, ""
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='6'/>");
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
"   <Option name='ALPHA' type='boolean' description='Mark first extrasample as being alpha'/>"
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
"</CreationOptionList>" );
                 
/* -------------------------------------------------------------------- */
/*      Set the driver details.                                         */
/* -------------------------------------------------------------------- */
        poDriver->SetDescription( "GTiff" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GeoTIFF" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_gtiff.html" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/tiff" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "tif" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16 Int16 UInt32 Int32 Float32 "
                                   "Float64 CInt16 CInt32 CFloat32 CFloat64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
                                   szCreateOptions );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = GTiffDataset::Open;
        poDriver->pfnCreate = GTiffDataset::Create;
        poDriver->pfnCreateCopy = GTiffDataset::CreateCopy;
        poDriver->pfnUnloadDriver = GDALDeregister_GTiff;
        poDriver->pfnIdentify = GTiffDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
