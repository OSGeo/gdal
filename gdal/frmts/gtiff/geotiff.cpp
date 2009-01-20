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
#include "tif_ovrcache.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

CPL_C_START
char CPL_DLL *  GTIFGetOGISDefn( GTIF *, GTIFDefn * );
int  CPL_DLL   GTIFSetFromOGISDefn( GTIF *, const char * );
const char * GDALDefaultCSVFilename( const char *pszBasename );
GUInt32 HalfToFloat( GUInt16 );
GUInt32 TripleToFloat( GUInt32 );
void    GTiffOneTimeInit();
CPL_C_END

void GTIFFBuildOverviewMetadata( const char *pszResampling,
                                 GDALDataset *poBaseDS, 
                                 CPLString &osMetadata );

#define TIFFTAG_GDAL_METADATA  42112
#define TIFFTAG_GDAL_NODATA    42113

TIFF* VSI_TIFFOpen(const char* name, const char* mode);

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
    friend class GTiffRGBABand;
    friend class GTiffBitmapBand;
    friend class GTiffSplitBitmapBand;
    friend class GTiffOddBitsBand;
    
    TIFF	*hTIFF;

    toff_t      nDirOffset;
    int		bBase;

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

    CPLErr      LoadBlockBuf( int );
    CPLErr      FlushBlockBuf();

    char	*pszProjection;
    double	adfGeoTransform[6];
    int		bGeoTransformValid;

    char	*pszTFWFilename;

    int		bNewDataset;            /* product of Create() */
    int         bTreatAsRGBA;
    int         bCrystalized;

    void	Crystalize();

    GDALColorTable *poColorTable;

    void	WriteGeoTIFFInfo();
    int		SetDirectory( toff_t nDirOffset = 0 );
    void        SetupTFW(const char *pszBasename);

    int		nOverviewCount;
    GTiffDataset **papoOverviewDS;

    int		nGCPCount;
    GDAL_GCP	*pasGCPList;

    int         IsBlockAvailable( int nBlockId );

    int         bGeoTIFFInfoChanged;
    int         bNoDataSet;
    int         bNoDataChanged;
    double      dfNoDataValue;

    int 	bMetadataChanged;

    void        ApplyPamInfo();

    GDALMultiDomainMetadata oGTiffMDMD;

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

    virtual CPLErr IBuildOverviews( const char *, int, int *, int, int *, 
                                    GDALProgressFunc, void * );

    CPLErr	   OpenOffset( TIFF *, toff_t nDirOffset, int, GDALAccess );

    static GDALDataset *OpenDir( const char *pszFilename );
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    virtual void    FlushCache( void );

    virtual CPLErr  SetMetadata( char **, const char * = "" );
    virtual char  **GetMetadata( const char * pszDomain = "" );
    virtual CPLErr  SetMetadataItem( const char*, const char*, 
                                     const char* = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual void   *GetInternalHandle( const char * );

    // only needed by createcopy and close code.
    static void	    WriteMetadata( GDALDataset *, TIFF *, int );
    static void	    WriteNoDataValue( TIFF *, double );
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
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    int			nBlockBufSize, nBlockId, nBlockIdBand0;
    CPLErr		eErr = CE_None;

    poGDS->SetDirectory();

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
        memset( pImage, 0,
                nBlockXSize * nBlockYSize
                * GDALGetDataTypeSize(eDataType) / 8 );
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
                * GDALGetDataTypeSize(eDataType) / 8 );
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
        
        pabyImage = poGDS->pabyBlockBuf + nBand - 1;

        nBlockPixels = nBlockXSize * nBlockYSize;
        for( i = 0; i < nBlockPixels; i++ )
        {
            ((GByte *) pImage)[i] = *pabyImage;
            pabyImage += poGDS->nBands;
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
/* -------------------------------------------------------------------- */
    if( poGDS->nBitsPerSample == 8 && poGDS->nBands != 1 
        && eErr == CE_None )
    {
        int iOtherBand;

        for( iOtherBand = 1; iOtherBand <= poGDS->nBands; iOtherBand++ )
        {
            if( iOtherBand == nBand )
                continue;

            GDALRasterBlock *poBlock;

            poBlock = poGDS->GetRasterBand(iOtherBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff);
            poBlock->DropLock();
        }
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    int		nBlockId, nBlockBufSize;
    CPLErr      eErr = CE_None;

    poGDS->Crystalize();
    poGDS->SetDirectory();

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
        
        if( TIFFIsTiled(poGDS->hTIFF) )
        {
            nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
            if( TIFFWriteEncodedTile( poGDS->hTIFF, nBlockId, pImage, 
                                      nBlockBufSize ) == -1 )
                eErr = CE_Failure;
        }
        else
        {
            nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
            if( TIFFWriteEncodedStrip( poGDS->hTIFF, nBlockId, pImage, 
                                       nBlockBufSize ) == -1 )
                eErr = CE_Failure;
        }

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
        poGDS->bMetadataChanged = TRUE;

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
    if( poGDS->bCrystalized )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "SetColorTable() not supported for existing TIFF files." );
        return CE_Failure;
    }

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

    poGDS->poColorTable = poCT->Clone();

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GTiffRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( poGDS->bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;

        return poGDS->dfNoDataValue;
    }
    else
        return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr GTiffRasterBand::SetNoDataValue( double dfNoData )

{
    poGDS->bNoDataSet = TRUE;
    poGDS->bNoDataChanged = TRUE;
    poGDS->dfNoDataValue = dfNoData;

    return CE_None;
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

    poGDS->SetDirectory();

    nBlockBufSize = 4 * nBlockXSize * nBlockYSize;
    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( poGDS->pabyBlockBuf == NULL )
    {
        poGDS->pabyBlockBuf = (GByte *) VSICalloc( 1, nBlockBufSize );
        if( poGDS->pabyBlockBuf == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "Unable to allocate %d bytes for a temporary strip "
                      "buffer in GTIFF driver.",
                      nBlockBufSize );
            
            return( CE_Failure );
        }
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
/*                             GTiffBitmapBand                          */
/* ==================================================================== */
/************************************************************************/

class GTiffBitmapBand : public GTiffRasterBand
{
    friend class GTiffDataset;

    GDALColorTable *poColorTable;

  public:

                   GTiffBitmapBand( GTiffDataset *, int );
    virtual       ~GTiffBitmapBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};


/************************************************************************/
/*                           GTiffBitmapBand()                          */
/************************************************************************/

GTiffBitmapBand::GTiffBitmapBand( GTiffDataset *poDS, int nBand )
        : GTiffRasterBand( poDS, nBand )

{

    if( nBand != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "One bit deep TIFF files only supported with one sample per pixel (band)." );
    }

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
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffBitmapBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    int		nBlockId, nBlockBufSize;
    CPLErr      eErr = CE_None;

    poGDS->Crystalize();
    poGDS->SetDirectory();

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    // First downsample to the particular number of bits in
    // a temporary buffer.
    int nLineOffset, iLine;
    GByte *pabyOddImg = (GByte *) CPLCalloc(nBlockXSize,nBlockYSize);
    
    nLineOffset = (nBlockXSize * poGDS->nBitsPerSample + 7) / 8;
    
    for( iLine = 0; iLine < nBlockYSize; iLine++ )
    {
        GDALCopyBits( (GByte *) pImage, 
                      iLine*nBlockXSize*8 + 8 - poGDS->nBitsPerSample, 8, 
                      pabyOddImg, 
                      iLine * nLineOffset * 8, poGDS->nBitsPerSample,
                      poGDS->nBitsPerSample, nBlockXSize );
    }
    
    // Then write as appropriate.
    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
        + (nBand-1) * poGDS->nBlocksPerBand;
    
    if( TIFFIsTiled(poGDS->hTIFF) )
    {
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
        if( TIFFWriteEncodedTile( poGDS->hTIFF, nBlockId, pabyOddImg, 
                                  nBlockBufSize ) == -1 )
            eErr = CE_Failure;
    }
    else
    {
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
        if( TIFFWriteEncodedStrip( poGDS->hTIFF, nBlockId, pabyOddImg, 
                                   nBlockBufSize ) == -1 )
            eErr = CE_Failure;
    }
    
    CPLFree( pabyOddImg );
    
    return eErr;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffBitmapBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    int			nBlockBufSize, nBlockId;
    CPLErr		eErr = CE_None;

    poGDS->SetDirectory();

    if( TIFFIsTiled(poGDS->hTIFF) )
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
    else
    {
        CPLAssert( nBlockXOff == 0 );
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
    }

    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadBlockBuf( nBlockId );
    if( eErr != CE_None )
        return eErr;

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
    
    return CE_None;
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
    int		nBlockId, nBlockBufSize;
    CPLErr      eErr = CE_None;

    if (eDataType != GDT_Byte)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Updating data with data type = %s and nBitsPerSample = %d is unsupported",
                 GDALGetDataTypeName(eDataType), poGDS->nBitsPerSample);
        return CE_Failure;
    }

    poGDS->Crystalize();
    poGDS->SetDirectory();

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

/* -------------------------------------------------------------------- */
/*      Handle case of "separate" images or single band images where    */
/*      no interleaving with other data is required.                    */
/* -------------------------------------------------------------------- */
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE
        || poGDS->nBands == 1 )
    {
        // First downsample to the particular number of bits in
        // a temporary buffer.
        int nLineOffset, iLine;
        GByte *pabyOddImg = (GByte *) CPLCalloc(nBlockXSize,nBlockYSize);

        nLineOffset = (nBlockXSize * poGDS->nBitsPerSample + 7) / 8;

        for( iLine = 0; iLine < nBlockYSize; iLine++ )
        {
            GDALCopyBits( (GByte *) pImage, 
                          iLine*nBlockXSize*8 + 8 - poGDS->nBitsPerSample, 8, 
                          pabyOddImg, 
                          iLine * nLineOffset * 8, poGDS->nBitsPerSample,
                          poGDS->nBitsPerSample, nBlockXSize );
        }

        // Then write as appropriate.
        nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
            + (nBand-1) * poGDS->nBlocksPerBand;
        
        if( TIFFIsTiled(poGDS->hTIFF) )
        {
            nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
            if( TIFFWriteEncodedTile( poGDS->hTIFF, nBlockId, pabyOddImg, 
                                      nBlockBufSize ) == -1 )
                eErr = CE_Failure;
        }
        else
        {
            nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
            if( TIFFWriteEncodedStrip( poGDS->hTIFF, nBlockId, pabyOddImg, 
                                       nBlockBufSize ) == -1 )
                eErr = CE_Failure;
        }

        CPLFree( pabyOddImg );

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

    for( iBand = 0; iBand < poGDS->nBands; iBand++ )
    {
        const GByte *pabyThisImage = NULL;
        GDALRasterBlock *poBlock = NULL;
        int nLineOffset, iLine;

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

        // lines always end on byte boundaries.
        nLineOffset = (poGDS->nBands*poGDS->nBitsPerSample*nBlockXSize+7) / 8;

        for( iLine = 0; iLine < nBlockYSize; iLine++ )
        {
            GDALCopyBits( pabyThisImage, 
                          iLine*nBlockXSize*8 + 8 - poGDS->nBitsPerSample, 8, 
                          poGDS->pabyBlockBuf, 
                          iLine*nLineOffset*8 + iBand * poGDS->nBitsPerSample, 
                          poGDS->nBitsPerSample * poGDS->nBands,
                          poGDS->nBitsPerSample, nBlockXSize );
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
    int			nBlockBufSize, nBlockId;
    CPLErr		eErr = CE_None;

    poGDS->SetDirectory();

    if( TIFFIsTiled(poGDS->hTIFF) )
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
    else
    {
        CPLAssert( nBlockXOff == 0 );
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
    }

    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;

    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId += (nBand-1) * poGDS->nBlocksPerBand;

/* -------------------------------------------------------------------- */
/*	Handle the case of a strip in a writable file that doesn't	*/
/*	exist yet, but that we want to read.  Just set to zeros and	*/
/*	return.								*/
/* -------------------------------------------------------------------- */
    if( poGDS->eAccess == GA_Update && !poGDS->IsBlockAvailable(nBlockId) )
    {
        memset( pImage, 0,
                nBlockXSize * nBlockYSize
                * GDALGetDataTypeSize(eDataType) / 8 );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Load the block buffer.                                          */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadBlockBuf( nBlockId );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Handle the case of 16- and 24-bit floating point data as per    */
/*      TIFF Technical Note 3.                                          */
/* -------------------------------------------------------------------- */
    if( eDataType == GDT_Float32 && poGDS->nBitsPerSample < 32 )
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
                ((GUInt32 *) pImage)[i] =
                    TripleToFloat( ((GUInt32)*(pabyImage + 2) << 16)
                                   | ((GUInt32)*(pabyImage + 1) << 8)
                                   | (GUInt32)*pabyImage );
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

        iPixel = 0;
        for( iY = 0; iY < nBlockYSize; iY++ )
        {
            iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for( iX = 0; iX < nBlockXSize; iX++ )
            {
                int  nOutWord = 0;

                for( iBit = 0; iBit < poGDS->nBitsPerSample; iBit++ )
                {
                    if( poGDS->pabyBlockBuf[iBitOffset>>3] 
                        & (0x80 >>(iBitOffset & 7)) )
                        nOutWord |= (1 << (poGDS->nBitsPerSample - 1 - iBit));
                    iBitOffset++;
                } 

                iBitOffset= iBitOffset + iPixelBitSkip - poGDS->nBitsPerSample;
                
                if( eDataType == GDT_Byte )
                    ((GByte *) pImage)[iPixel++] = nOutWord;
                else if( eDataType == GDT_UInt16 )
                    ((GUInt16 *) pImage)[iPixel++] = nOutWord;
                else if( eDataType == GDT_UInt32 )
                    ((GUInt32 *) pImage)[iPixel++] = nOutWord;
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                          GTiffSplitBitmapBand                        */
/* ==================================================================== */
/************************************************************************/

class GTiffSplitBitmapBand : public GTiffBitmapBand
{
    friend class GTiffDataset;

    int            nLastLineRead;

  public:

                   GTiffSplitBitmapBand( GTiffDataset *, int );
    virtual       ~GTiffSplitBitmapBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
};


/************************************************************************/
/*                           GTiffBitmapBand()                          */
/************************************************************************/

GTiffSplitBitmapBand::GTiffSplitBitmapBand( GTiffDataset *poDS, int nBand )
        : GTiffBitmapBand( poDS, nBand )

{
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    nLastLineRead = -1;
}

/************************************************************************/
/*                           GTiffBitmapBand()                          */
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
    GByte              *pabyLineBuf;

    poGDS->SetDirectory();

    pabyLineBuf = (GByte *) CPLMalloc(TIFFScanlineSize(poGDS->hTIFF));

/* -------------------------------------------------------------------- */
/*      Read through to target scanline.                                */
/* -------------------------------------------------------------------- */
    if( nLastLineRead >= nBlockYOff )
        nLastLineRead = -1;

    while( nLastLineRead < nBlockYOff )
    {
        if( TIFFReadScanline( poGDS->hTIFF, pabyLineBuf, ++nLastLineRead, 0 ) == -1 )
        {
            CPLFree( pabyLineBuf );
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
        if( pabyLineBuf[iSrcOffset >>3] & (0x80 >> (iSrcOffset & 0x7)) )
            ((GByte *) pImage)[iDstOffset++] = 1;
        else
            ((GByte *) pImage)[iDstOffset++] = 0;
    }
    
    CPLFree( pabyLineBuf );

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffSplitBitmapBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                          void * pImage )

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

GTiffDataset::GTiffDataset()

{
    nLoadedBlock = -1;
    bLoadedBlockDirty = FALSE;
    pabyBlockBuf = NULL;
    hTIFF = NULL;
    bNewDataset = FALSE;
    bCrystalized = TRUE;
    poColorTable = NULL;
    bNoDataSet = FALSE;
    bNoDataChanged = FALSE;
    dfNoDataValue = -9999.0;
    pszProjection = CPLStrdup("");
    bBase = TRUE;
    bTreatAsRGBA = FALSE;
    nOverviewCount = 0;
    papoOverviewDS = NULL;
    nDirOffset = 0;

    pszTFWFilename = NULL;

    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;
}

/************************************************************************/
/*                           ~GTiffDataset()                            */
/************************************************************************/

GTiffDataset::~GTiffDataset()

{
    Crystalize();

    FlushCache();

    if( bBase )
    {
        for( int i = 0; i < nOverviewCount; i++ )
        {
            delete papoOverviewDS[i];
        }
        CPLFree( papoOverviewDS );
    }

    SetDirectory();

    if( poColorTable != NULL )
        delete poColorTable;

    if( GetAccess() == GA_Update && bBase )
    {
        if( bNewDataset || bMetadataChanged )
            WriteMetadata( this, hTIFF, TRUE );
        
        if( bNewDataset || bGeoTIFFInfoChanged )
            WriteGeoTIFFInfo();

	if( bNoDataChanged )
            WriteNoDataValue( hTIFF, dfNoDataValue );
        
        if( bNewDataset || bMetadataChanged || bGeoTIFFInfoChanged || bNoDataChanged )
        {
#if defined(TIFFLIB_VERSION)
#if  TIFFLIB_VERSION > 20010925 && TIFFLIB_VERSION != 20011807
            TIFFRewriteDirectory( hTIFF );
#endif
#endif
        }
    }

    if( bBase )
    {
        XTIFFClose( hTIFF );
    }

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    if( pszTFWFilename != NULL )
        CPLFree( pszTFWFilename );
    CPLFree( pszProjection );
}

/************************************************************************/
/*                           FlushBlockBuf()                            */
/************************************************************************/

CPLErr GTiffDataset::FlushBlockBuf()

{
    int		nBlockBufSize;
    CPLErr      eErr = CE_None;

    if( nLoadedBlock < 0 || !bLoadedBlockDirty )
        return CE_None;

    bLoadedBlockDirty = FALSE;

    SetDirectory();

    if( TIFFIsTiled(hTIFF) )
        nBlockBufSize = TIFFTileSize( hTIFF );
    else
        nBlockBufSize = TIFFStripSize( hTIFF );

    if( TIFFIsTiled( hTIFF ) )
    {
        if( TIFFWriteEncodedTile(hTIFF, nLoadedBlock, pabyBlockBuf,
                                 nBlockBufSize) == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFWriteEncodedTile() failed." );
            eErr = CE_Failure;
        }
    }
    else
    {
        if( TIFFWriteEncodedStrip(hTIFF, nLoadedBlock, pabyBlockBuf,
                                 nBlockBufSize) == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TIFFWriteEncodedStrip() failed." );
            eErr = CE_Failure;
        }
    }

    return eErr;;
}

/************************************************************************/
/*                            LoadBlockBuf()                            */
/*                                                                      */
/*      Load working block buffer with request block (tile/strip).      */
/************************************************************************/

CPLErr GTiffDataset::LoadBlockBuf( int nBlockId )

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
    if( eAccess == GA_Update && !IsBlockAvailable( nBlockId ) )
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
        if( bNewDataset || bMetadataChanged )
            WriteMetadata( this, hTIFF, TRUE );

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

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    Crystalize();

    TIFFFlush( hTIFF );

/* -------------------------------------------------------------------- */
/*      If we don't have read access, then create the overviews externally.*/
/* -------------------------------------------------------------------- */
    if( GetAccess() != GA_Update )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
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
                TIFF_WriteOverview( hTIFF, nOXSize, nOYSize, 
                                    nOvBitsPerSample, nPlanarConfig,
                                    nSamplesPerPixel, 128, 128, TRUE,
                                    nCompression, nPhotometric, nSampleFormat, 
                                    panRed, panGreen, panBlue, FALSE, 
                                    osMetadata );

            if( nOverviewOffset == 0 )
            {
                eErr = CE_Failure;
                continue;
            }

            poODS = new GTiffDataset();
            if( poODS->OpenOffset( hTIFF, nOverviewOffset, FALSE, 
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
            }
        }
        else
            panOverviewList[i] *= -1;
    }

/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
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

        eErr = GDALRegenerateOverviews( poBand,
                                        nNewOverviews, papoOverviewBands,
                                        pszResampling, 
                                        GDALScaledProgress, 
                                        pScaledProgressData);

        GDALDestroyScaledProgress( pScaledProgressData );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( papoOverviewBands );

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
/* -------------------------------------------------------------------- */
/*      Are we maintaining a .tfw file?                                 */
/* -------------------------------------------------------------------- */
	if( pszTFWFilename != NULL )
	{
	    FILE	*fp;

	    fp = VSIFOpen( pszTFWFilename, "wt" );
	    
	    fprintf( fp, "%.10f\n", adfGeoTransform[1] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[4] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[2] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[5] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[0] 
		     + 0.5 * adfGeoTransform[1]
		     + 0.5 * adfGeoTransform[2] );
	    fprintf( fp, "%.10f\n", adfGeoTransform[3]
		     + 0.5 * adfGeoTransform[4]
		     + 0.5 * adfGeoTransform[5] );
	    VSIFClose( fp );
	}
    }
    else if( GetGCPCount() > 0 )
    {
	double	*padfTiePoints;
	int		iGCP;
	
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

	TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 
		      6 * GetGCPCount(), padfTiePoints );
	CPLFree( padfTiePoints );
    }

/* -------------------------------------------------------------------- */
/*	Write out projection definition.				*/
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL && !EQUAL( pszProjection, "" ) )
    {
        GTIF	*psGTIF;

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
                             int nBand )

{
    int iDomain;
    char **papszDomainList;

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

void GTiffDataset::WriteMetadata( GDALDataset *poSrcDS, TIFF *hTIFF,
                                  int bSrcIsGeoTIFF )

{
/* -------------------------------------------------------------------- */
/*      Convert all the remaining metadata into a simple XML            */
/*      format.                                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot = NULL, *psTail = NULL;

    if( bSrcIsGeoTIFF )
    {
        WriteMDMetadata( &(((GTiffDataset *)poSrcDS)->oGTiffMDMD), 
                         hTIFF, &psRoot, &psTail, 0 );
    }
    else
    {
        char **papszMD = poSrcDS->GetMetadata();

        if( CSLCount(papszMD) > 0 )
        {
            GDALMultiDomainMetadata oMDMD;
            oMDMD.SetMetadata( papszMD );

            WriteMDMetadata( &oMDMD, hTIFF, &psRoot, &psTail, 0 );
        }
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
                             hTIFF, &psRoot, &psTail, nBand );
        }
        else
        {
            char **papszMD = poBand->GetMetadata();
            
            if( CSLCount(papszMD) > 0 )
            {
                GDALMultiDomainMetadata oMDMD;
                oMDMD.SetMetadata( papszMD );
                
                WriteMDMetadata( &oMDMD, hTIFF, &psRoot, &psTail, nBand );
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
        char *pszXML_MD = CPLSerializeXMLTree( psRoot );
        if( strlen(pszXML_MD) > 32000 )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Lost metadata writing to GeoTIFF ... too large to fit in tag." );
        }
        else
        {
            TIFFSetField( hTIFF, TIFFTAG_GDAL_METADATA, pszXML_MD );
        }
        CPLFree( pszXML_MD );
        CPLDestroyXMLNode( psRoot );
    }
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

    if( nNewOffset == 0)
        return TRUE;

    if( TIFFCurrentDirOffset(hTIFF) == nNewOffset )
        return TRUE;

    CPLDebug( "GTiff", "SetDirectory(%15.0f)",(double) nNewOffset );

    if( GetAccess() == GA_Update )
        TIFFFlush( hTIFF );
    
    int nSetDirResult = TIFFSetSubDirectory( hTIFF, nNewOffset );

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

    return nSetDirResult;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int GTiffDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We have a special hook for handling opening a specific          */
/*      directory of a TIFF file.                                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(poOpenInfo->pszFilename,"GTIFF_DIR:",10) )
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
    if( poOpenInfo->pabyHeader[2] == 43 && poOpenInfo->pabyHeader[3] == 0 )
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

/* -------------------------------------------------------------------- */
/*      We have a special hook for handling opening a specific          */
/*      directory of a TIFF file.                                       */
/* -------------------------------------------------------------------- */
    if( EQUALN(poOpenInfo->pszFilename,"GTIFF_DIR:",10) )
        return OpenDir( poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 2 )
        return NULL;

    if( (poOpenInfo->pabyHeader[0] != 'I' || poOpenInfo->pabyHeader[1] != 'I')
        && (poOpenInfo->pabyHeader[0] != 'M' || poOpenInfo->pabyHeader[1] != 'M'))
        return NULL;

#ifndef BIGTIFF_SUPPORT
    if( poOpenInfo->pabyHeader[2] == 43 && poOpenInfo->pabyHeader[3] == 0 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "This is a BigTIFF file.  BigTIFF is not supported by this\n"
                  "version of GDAL and libtiff." );
        return NULL;
    }
#endif

    if( (poOpenInfo->pabyHeader[2] != 0x2A || poOpenInfo->pabyHeader[3] != 0)
        && (poOpenInfo->pabyHeader[3] != 0x2A || poOpenInfo->pabyHeader[2] != 0)
        && (poOpenInfo->pabyHeader[2] != 0x2B || poOpenInfo->pabyHeader[3] != 0)
        && (poOpenInfo->pabyHeader[3] != 0x2B || poOpenInfo->pabyHeader[2] != 0))
        return NULL;

    GTiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
	hTIFF = VSI_TIFFOpen( poOpenInfo->pszFilename, "r" );
    else
        hTIFF = VSI_TIFFOpen( poOpenInfo->pszFilename, "r+" );
    
    if( hTIFF == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset 	*poDS;

    poDS = new GTiffDataset();
    poDS->SetDescription( poOpenInfo->pszFilename );

    if( poDS->OpenOffset( hTIFF,TIFFCurrentDirOffset(hTIFF), TRUE,
                          poOpenInfo->eAccess ) != CE_None )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
    
/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();
    poDS->ApplyPamInfo();

    return poDS;
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

    if( GDALPamDataset::GetGeoTransform( adfPamGeoTransform ) == CE_None )
    {
        memcpy( adfGeoTransform, adfPamGeoTransform, sizeof(double)*6 );
        bGeoTransformValid = TRUE;
    }

    const char *pszPamSRS = GDALPamDataset::GetProjectionRef();

    if( pszPamSRS != NULL && strlen(pszPamSRS) > 0 )
    {
        CPLFree( pszProjection );
        pszProjection = CPLStrdup( pszPamSRS );
    }

/* -------------------------------------------------------------------- */
/*      Copy any PAM metadata into our GeoTIFF context, but with the    */
/*      GeoTIFF context overriding the PAM info.                        */
/* -------------------------------------------------------------------- */
    char **papszPamDomains = oMDMD.GetDomainList();
    int i;

    for( i = 0; papszPamDomains && papszPamDomains[i] != NULL; i++ )
    {
        const char *pszDomain = papszPamDomains[i];
        char **papszGT_MD = oGTiffMDMD.GetMetadata( pszDomain );
        char **papszPAM_MD = CSLDuplicate(oMDMD.GetMetadata( pszDomain ));

        papszPAM_MD = CSLMerge( papszPAM_MD, papszGT_MD );

        oGTiffMDMD.SetMetadata( papszPAM_MD, pszDomain );
        CSLDestroy( papszPAM_MD );
    }
}

/************************************************************************/
/*                              OpenDir()                               */
/*                                                                      */
/*      Open a specific directory as encoded into a filename.           */
/************************************************************************/

GDALDataset *GTiffDataset::OpenDir( const char *pszCompositeName )

{
    if( !EQUALN(pszCompositeName,"GTIFF_DIR:",10) )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Split out filename, and dir#/offset.                            */
/* -------------------------------------------------------------------- */
    const char *pszFilename = pszCompositeName + 10;
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
                          "Requested directory %d not found." );
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
    poDS->SetDescription( pszFilename );

    if( poDS->OpenOffset( hTIFF, nOffset, FALSE, GA_ReadOnly ) != CE_None )
    {
        delete poDS;
        return NULL;
    }
    else
    {
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

CPLErr GTiffDataset::OpenOffset( TIFF *hTIFFIn, toff_t nDirOffsetIn, 
				 int bBaseIn, GDALAccess eAccess )

{
    uint32	nXSize, nYSize;
    int		bTreatAsBitmap = FALSE;
    int         bTreatAsOdd = FALSE;
    int         bTreatAsSplit = FALSE;

    hTIFF = hTIFFIn;

    nDirOffset = nDirOffsetIn;

    SetDirectory( nDirOffsetIn );

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

        TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode );
        if( nColorMode != JPEGCOLORMODE_RGB )
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
            && nYSize > 2000 )
            bTreatAsSplit = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Should we treat this via the RGBA interface?                    */
/* -------------------------------------------------------------------- */
    if( !bTreatAsBitmap && !(nBitsPerSample > 8) 
        && nPhotometric == PHOTOMETRIC_CIELAB ||
        nPhotometric == PHOTOMETRIC_LOGL ||
        nPhotometric == PHOTOMETRIC_LOGLUV ||
        ( nPhotometric == PHOTOMETRIC_YCBCR 
          && nCompression != COMPRESSION_JPEG ) )
    {
        char	szMessage[1024];

        if( TIFFRGBAImageOK( hTIFF, szMessage ) == 1 )
        {
            bTreatAsRGBA = TRUE;
            nBands = 4;
        }
        else
        {
            CPLDebug( "GTiff", "TIFFRGBAImageOK says:\n%s", szMessage );
        }
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
		oEntry.c1 = oEntry.c2 = oEntry.c3 = 
                    (255 * (nColorCount - 1 - iColor)) / (nColorCount-1);
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
        else if( bTreatAsSplit )
            SetBand( iBand+1, new GTiffSplitBitmapBand( this, iBand+1 ) );
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
                GDALReadWorldFile( GetDescription(), "tfw", adfGeoTransform );

            if( !bGeoTransformValid )
            {
                bGeoTransformValid = 
                    GDALReadWorldFile( GetDescription(), "tifw", adfGeoTransform );
            }
            if( !bGeoTransformValid )
            {
                bGeoTransformValid = 
                    GDALReadWorldFile( GetDescription(), "wld", adfGeoTransform );
            }
            if( !bGeoTransformValid )
            {
                int bTabFileOK = 
                    GDALReadTabFile( GetDescription(), adfGeoTransform, 
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
            }

            // Is this a pixel-is-point dataset?
            short nRasterType;
            

            if( GTIFKeyGet(hGTIF, GTRasterTypeGeoKey, &nRasterType, 
                           0, 1 ) == 1 )
            {
                if( nRasterType == (short) RasterPixelIsPoint )
                    SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );
                else
                    SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_AREA );
            }

            GTIFFree( hGTIF );
        }
        
/* -------------------------------------------------------------------- */
/*      If we didn't get a geotiff projection definition, but we did    */
/*      get one from the .tab file, use that instead.                   */
/* -------------------------------------------------------------------- */
        if( pszTabWKT != NULL 
            && (pszProjection == NULL || pszProjection[0] == '\0') )
        {
            CPLFree( pszProjection );
            pszProjection = pszTabWKT;
            pszTabWKT = NULL;
        }
        
        if( pszProjection == NULL )
        {
            pszProjection = CPLStrdup( "" );
        }
        
        if( pszTabWKT != NULL )
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
        SetMetadataItem( "COMPRESSION", "JPEG", "IMAGE_STRUCTURE" );
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

    if( nBitsPerSample < 8 )
    {
        for (int i = 0; i < nBands; ++i)
            GetRasterBand(i+1)->SetMetadataItem( "NBITS", 
                                                 CPLString().Printf( "%ld", nBitsPerSample ),
                                                 "IMAGE_STRUCTURE" );
    }
        
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
	
    if( TIFFGetField( hTIFF, TIFFTAG_GDAL_NODATA, &pszText ) )
    {
        bNoDataSet = TRUE;
        dfNoDataValue = atof( pszText );
    }

    bNoDataChanged = FALSE;

/* -------------------------------------------------------------------- */
/*      If this is a "base" raster, we should scan for any              */
/*      associated overviews.                                           */
/* -------------------------------------------------------------------- */
    if( bBase )
    {
        while( !TIFFLastDirectory( hTIFF ) 
               && TIFFReadDirectory( hTIFF ) != 0 )
        {
            toff_t	nThisDir = TIFFCurrentDirOffset(hTIFF);
            uint32	nSubType;

            if( TIFFGetField(hTIFF, TIFFTAG_SUBFILETYPE, &nSubType)
                && (nSubType & FILETYPE_REDUCEDIMAGE) )
            {
                GTiffDataset	*poODS;

                poODS = new GTiffDataset();
                if( poODS->OpenOffset( hTIFF, nThisDir, FALSE, 
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
                }
            }

            SetDirectory( nThisDir );
        }
    }

    return( CE_None );
}

/************************************************************************/
/*                              SetupTFW()                              */
/************************************************************************/

void GTiffDataset::SetupTFW( const char *pszTIFFilename )

{
    char	*pszPath;
    char	*pszBasename;
    
    pszPath = CPLStrdup( CPLGetPath(pszTIFFilename) );
    pszBasename = CPLStrdup( CPLGetBasename(pszTIFFilename) );

    pszTFWFilename = CPLStrdup( CPLFormFilename(pszPath,pszBasename,"tfw") );
    
    CPLFree( pszPath );
    CPLFree( pszBasename );
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

TIFF *GTiffCreate( const char * pszFilename,
                   int nXSize, int nYSize, int nBands,
                   GDALDataType eType,
                   char **papszParmList )

{
    TIFF		*hTIFF;
    int                 nBlockXSize = 0, nBlockYSize = 0;
    int                 bTiled = FALSE;
    int                 nCompression = COMPRESSION_NONE;
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
        pszValue = CSLFetchNameValue(papszParmList,"INTERLEAVE");
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
    }

    pszValue = CSLFetchNameValue( papszParmList, "PREDICTOR" );
    if( pszValue  != NULL )
        nPredictor =  atoi( pszValue );

    nZLevel = GTiffGetZLevel(papszParmList);

    nJpegQuality = GTiffGetJpegQuality(papszParmList);

/* -------------------------------------------------------------------- */
/*      Check if we are producing an uncompressed file and it is        */
/*      certain to be larger than 4GB.                                  */
/* -------------------------------------------------------------------- */
    if( nCompression == COMPRESSION_NONE )
    {
        if( nXSize * ((double)nYSize) * nBands * (GDALGetDataTypeSize(eType)/8)
            > 4200000000.0 )
        {
#ifndef BIGTIFF_SUPPORT
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "A %d pixels x %d lines x %d bands %s image would be larger than 4GB\n"
                      "but this is the largest size a TIFF can be.  Creation failed.",
                      nXSize, nYSize, nBands, GDALGetDataTypeName(eType) );
            return NULL;
#else
            CPLDebug( "GTiff", 
                 "Automatically flipping to Bigtiff due to large file size" );
            bCreateBigTIFF = TRUE;
#endif
        }
    }

/* -------------------------------------------------------------------- */
/*      Create BigTIFF file on user's request, even if total image      */
/*      size is enough for ClassicTIFF.                                 */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszParmList, "BigTIFF") != NULL )
    {
        bCreateBigTIFF = CSLFetchBoolean( papszParmList, "BIGTIFF", 
                                          bCreateBigTIFF );
#ifndef BIGTIFF_SUPPORT
        if( bCreateBigTIFF )
            CPLError( CE_Warning, CPLE_NotSupported,
                      "BIGTIFF requested, but GDAL built without BigTIFF\n"
                      "enabled libtiff, request ignored." );
        bCreateBigTIFF = FALSE;
#endif
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    hTIFF = VSI_TIFFOpen( pszFilename, (bCreateBigTIFF) ? "w+8" : "w+" );
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
/*      specified for GDT_Byte.                                         */
/* -------------------------------------------------------------------- */
    int nBitsPerSample = GDALGetDataTypeSize(eType);
    if( eType == GDT_Byte 
        && CSLFetchNameValue(papszParmList, "NBITS") != NULL )
    {
        nBitsPerSample = 
            MIN(8,MAX(1,atoi(CSLFetchNameValue(papszParmList, "NBITS"))));
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

    pszValue = CSLFetchNameValue(papszParmList,"PHOTOMETRIC");
    if( pszValue != NULL )
    {
        if( EQUAL( pszValue, "MINISBLACK" ) )
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
        else if( EQUAL( pszValue, "MINISWHITE" ) )
            TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE );
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
    
/* -------------------------------------------------------------------- */
/*      Setup tiling/stripping flags.                                   */
/* -------------------------------------------------------------------- */
    if( bTiled )
    {
        if( nBlockXSize == 0 )
            nBlockXSize = 256;
        
        if( nBlockYSize == 0 )
            nBlockYSize = 256;

        TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize );
        TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize );
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
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompression );
    if ( nCompression == COMPRESSION_LZW ||
         nCompression == COMPRESSION_ADOBE_DEFLATE )
        TIFFSetField( hTIFF, TIFFTAG_PREDICTOR, nPredictor );
    if (nCompression == COMPRESSION_ADOBE_DEFLATE
        && nZLevel != -1)
        TIFFSetField( hTIFF, TIFFTAG_ZIPQUALITY, nZLevel );
    if( nCompression == COMPRESSION_JPEG 
        && nJpegQuality != -1 )
        TIFFSetField( hTIFF, TIFFTAG_JPEGQUALITY, nJpegQuality );
        
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
    hTIFF = GTiffCreate( pszFilename, nXSize, nYSize, nBands, 
                         eType, papszParmList );

    if( hTIFF == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    poDS = new GTiffDataset();
    poDS->hTIFF = hTIFF;

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->bNewDataset = TRUE;
    poDS->bCrystalized = FALSE;
    poDS->nSamplesPerPixel = (uint16) nBands;

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

        if ( !TIFFGetField( hTIFF, TIFFTAG_JPEGCOLORMODE, &nColorMode ) ||
              nColorMode != JPEGCOLORMODE_RGB )
            TIFFSetField(hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
    }


/* -------------------------------------------------------------------- */
/*      Do we need a TFW file?                                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszParmList, "TFW", FALSE ) 
        || CSLFetchBoolean( papszParmList, "WORLDFILE", FALSE ) )
        poDS->SetupTFW( pszFilename );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        if( poDS->nBitsPerSample < 8 )
            poDS->SetBand( iBand+1, new GTiffOddBitsBand( poDS, iBand+1 ) );
        else
            poDS->SetBand( iBand+1, new GTiffRasterBand( poDS, iBand+1 ) );
    }

    return( poDS );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

static GDALDataset *
GTiffCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
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
        && atoi(poPBand->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" )) < 8 
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
    hTIFF = GTiffCreate( pszFilename, nXSize, nYSize, nBands, 
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
/*      Transfer some TIFF specific metadata, if available.  Should     */
/*      we push this into .pam if we are avoiding GDAL tags?            */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszProfile,"GDALGeoTIFF") )
        GTiffDataset::WriteMetadata( poSrcDS, hTIFF, FALSE );

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

    poDS = (GTiffDataset *) GDALOpen( pszFilename, GA_Update );
    if( poDS == NULL )
        poDS = (GTiffDataset *) GDALOpen( pszFilename, GA_ReadOnly );

    if ( poDS == NULL )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }

    if( EQUAL(pszProfile,"GDALGeoTIFF") )
    {
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
    }
    
    hTIFF = (TIFF*) poDS->GetInternalHandle(NULL);

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
    eErr = GDALDatasetCopyWholeRaster( (GDALDatasetH) poSrcDS, 
                                        (GDALDatasetH) poDS,
                                        NULL, pfnProgress, pProgressData );

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
    return oGTiffMDMD.GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/
CPLErr GTiffDataset::SetMetadata( char ** papszMD, const char *pszDomain )

{
    if( pszDomain == NULL || !EQUAL(pszDomain,"_temporary_") )
        bMetadataChanged = TRUE;

    return oGTiffMDMD.SetMetadata( papszMD, pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GTiffDataset::GetMetadataItem( const char * pszName, 
                                           const char * pszDomain )

{
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
          TRUE,	FALSE,	"GDALMetadata" },
        { TIFFTAG_GDAL_NODATA,	    -1,-1, TIFF_ASCII,	FIELD_CUSTOM,
          TRUE,	FALSE,	"GDALNoDataValue" }
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
        char szCreateOptions[2048];
        char szOptionalCompressItems[500];

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
#else
        TIFFCodec	*c, *codecs = TIFFGetConfiguredCODECs();

        for( c = codecs; c->name; c++ )
        {
            if( c->scheme == COMPRESSION_PACKBITS )
                strcat( szOptionalCompressItems,
                        "       <Value>PACKBITS</Value>" );
            else if( c->scheme == COMPRESSION_JPEG )
                strcat( szOptionalCompressItems,
                        "       <Value>JPEG</Value>" );
            else if( c->scheme == COMPRESSION_LZW )
                strcat( szOptionalCompressItems,
                        "       <Value>LZW</Value>" );
            else if( c->scheme == COMPRESSION_ADOBE_DEFLATE )
                strcat( szOptionalCompressItems,
                        "       <Value>DEFLATE</Value>" );
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
"   </Option>"
"   <Option name='PREDICTOR' type='int' description='Predictor Type'/>"
"   <Option name='JPEG_QUALITY' type='int' description='JPEG quality 1-100, default 75.'/>"
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9, default 6.'/>"
"   <Option name='NBITS' type='int' description='BITS for sub-byte files (1-7)'/>"
"   <Option name='INTERLEAVE' type='string-select' default='PIXEL'>"
"       <Value>BAND</Value>"
"       <Value>PIXEL</Value>"
"   </Option>"
"   <Option name='TILED' type='boolean' description='Switch to tiled format'/>"
"   <Option name='TFW' type='boolean' description='Write out world file'/>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile/Strip Height'/>"
"   <Option name='PHOTOMETRIC' type='string-select'>"
"       <Value>MINISBLACK</Value>"
"       <Value>MINISWHITE</Value>"
"       <Value>RGB</Value>"
"       <Value>CMYK</Value>"
"       <Value>YCBCR</Value>"
"       <Value>CIELAB</Value>"
"       <Value>ICCLAB</Value>"
"       <Value>ITULAB</Value>"
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
"   <Option name='BIGTIFF' type='boolean' description='Force creation of BigTIFF file'/>"
#endif
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
        poDriver->pfnCreateCopy = GTiffCreateCopy;
        poDriver->pfnUnloadDriver = GDALDeregister_GTiff;
        poDriver->pfnIdentify = GTiffDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
