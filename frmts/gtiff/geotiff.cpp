/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.44  2001/03/13 19:16:46  warmerda
 * don't try to handle MINISWHITE through RGBA interface, test before using RGBA
 *
 * Revision 1.43  2001/02/12 22:16:22  warmerda
 * added TFW write support
 *
 * Revision 1.42  2001/01/23 15:26:27  warmerda
 * Removed debugging printf.
 *
 * Revision 1.41  2001/01/22 22:33:09  warmerda
 * implement SetColorTable(), Crystalize() ... may be buggy
 *
 * Revision 1.40  2000/12/15 14:46:27  warmerda
 * Added read support for .tfw files.
 * Added read/write support for GEOTRANSMATRIX in GeoTIFF.
 *
 * Revision 1.39  2000/10/14 04:09:26  warmerda
 * Set photometric to RGB for RGBA images in CreateCopy().
 *
 * Revision 1.38  2000/09/25 15:43:21  warmerda
 * Added support for odd TIFF files (such as 1bit, and YCbCr) via the
 * RGBA interface.
 *
 * Revision 1.37  2000/08/14 18:33:49  warmerda
 * added support for writing palettes to overviews
 *
 * Revision 1.36  2000/07/17 17:40:41  warmerda
 * Implemented reading of non-eight bit interleaved files.
 *
 * Revision 1.35  2000/07/17 17:09:11  warmerda
 * added support for complex data
 *
 * Revision 1.34  2000/07/17 14:52:51  warmerda
 * added preliminary complex support
 *
 * Revision 1.33  2000/07/13 21:39:48  warmerda
 * implemented CreateCopy() method which supports GCPs and color tables
 *
 * Revision 1.32  2000/06/26 22:18:33  warmerda
 * added scaled progress support
 *
 * Revision 1.31  2000/06/26 21:09:55  warmerda
 * improved flushing before building overviews
 *
 * Revision 1.30  2000/06/26 19:40:28  warmerda
 * Various fixes for ::Create(), including flushing the directory to disk at
 * end of create so that overviews can be safely added.
 *
 * Revision 1.29  2000/06/19 18:48:29  warmerda
 * added IBuildOverviews() implementation on GTiffDataset
 *
 * Revision 1.28  2000/06/19 14:18:01  warmerda
 * added help link
 *
 * Revision 1.27  2000/06/09 14:21:53  warmerda
 * Don't try to write geotiff info to read-only files.
 *
 * Revision 1.26  2000/05/04 13:56:28  warmerda
 * Avoid having a block ysize larger than the whole file for stripped files.
 *
 * Revision 1.25  2000/05/01 01:53:33  warmerda
 * added various compress options
 *
 * Revision 1.24  2000/04/21 21:53:42  warmerda
 * rewrote metadata support, do flush before changing directories
 *
 * Revision 1.23  2000/03/31 14:12:38  warmerda
 * added CPL based handling of libtiff warnings and errors
 *
 * Revision 1.22  2000/03/31 13:36:07  warmerda
 * added gcp's and metadata
 *
 * Revision 1.21  2000/03/23 18:47:52  warmerda
 * added support for writing tiled TIFF
 *
 * Revision 1.20  2000/03/23 16:54:35  warmerda
 * fixed up geotransform initialization
 *
 * Revision 1.19  2000/03/14 15:16:21  warmerda
 * initialize adfGeoTransform[]
 *
 * Revision 1.18  2000/03/13 14:33:01  warmerda
 * avoid ambiguity with Open
 *
 * Revision 1.17  2000/03/06 02:23:08  warmerda
 * added overviews, and colour tables
 */

#include "tiffiop.h"
#include "xtiffio.h"
#include "geotiff.h"
#include "gdal_priv.h"
#include "geo_normalize.h"
#include "tif_ovrcache.h"
#include "cpl_string.h"

static GDALDriver	*poGTiffDriver = NULL;

CPL_C_START
void	GDALRegister_GTiff(void);
char *  GTIFGetOGISDefn( GTIFDefn * );
int     GTIFSetFromOGISDefn( GTIF *, const char * );
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				GTiffDataset				*/
/* ==================================================================== */
/************************************************************************/

class GTiffRasterBand;
class GTiffRGBABand;

class GTiffDataset : public GDALDataset
{
    friend	GTiffRasterBand;
    friend	GTiffRGBABand;
    
    TIFF	*hTIFF;

    uint32      nDirOffset;
    int		bBase;

    uint16	nPlanarConfig;
    uint16	nSamplesPerPixel;
    uint16	nBitsPerSample;
    uint32	nRowsPerStrip;
    uint16	nPhotometric;
    uint16      nSampleFormat;
    
    int		nBlocksPerBand;

    uint32      nBlockXSize;
    uint32	nBlockYSize;

    int		nLoadedBlock;		/* or tile */
    GByte	*pabyBlockBuf;

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
    int		SetDirectory( uint32 nDirOffset = 0 );
    void        SetupTFW(const char *pszBasename);

    int		nOverviewCount;
    GTiffDataset **papoOverviewDS;

    int		nGCPCount;
    GDAL_GCP	*pasGCPList;

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

    virtual CPLErr IBuildOverviews( const char *, int, int *, int, int *, 
                                    GDALProgressFunc, void * );

    CPLErr	   OpenOffset( TIFF *, uint32 nDirOffset, int, GDALAccess );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    virtual void FlushCache( void );
};

/************************************************************************/
/* ==================================================================== */
/*                            GTiffRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GTiffRasterBand : public GDALRasterBand
{
    friend	GTiffDataset;

  public:

                   GTiffRasterBand( GTiffDataset *, int );

    // should override RasterIO eventually.
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * ); 

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr          SetColorTable( GDALColorTable * );

    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );
};

/************************************************************************/
/*                           GTiffRasterBand()                            */
/************************************************************************/

GTiffRasterBand::GTiffRasterBand( GTiffDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

/* -------------------------------------------------------------------- */
/*      Get the GDAL data type.                                         */
/* -------------------------------------------------------------------- */
    uint16		nSampleFormat = poDS->nSampleFormat;

    eDataType = GDT_Unknown;

    if( poDS->nBitsPerSample <= 8 )
        eDataType = GDT_Byte;
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
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
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
        
    if( poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE )
        nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
                   + (nBand-1) * poGDS->nBlocksPerBand;
    else
        nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow;
        
/* -------------------------------------------------------------------- */
/*	Handle the case of a strip in a writable file that doesn't	*/
/*	exist yet, but that we want to read.  Just set to zeros and	*/
/*	return.								*/
/* -------------------------------------------------------------------- */
    if( poGDS->eAccess == GA_Update
        && (((int) poGDS->hTIFF->tif_dir.td_nstrips) <= nBlockId
            || poGDS->hTIFF->tif_dir.td_stripbytecount[nBlockId] == 0) )
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
        if( TIFFIsTiled( poGDS->hTIFF ) )
        {
            if( TIFFReadEncodedTile( poGDS->hTIFF, nBlockId, pImage,
                                     nBlockBufSize ) == -1 )
            {
                memset( pImage, 0, nBlockBufSize );
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedStrip() failed.\n" );
                
                eErr = CE_Failure;
            }
        }
        else
        {
            if( TIFFReadEncodedStrip( poGDS->hTIFF, nBlockId, pImage,
                                      nBlockBufSize ) == -1 )
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
/*      Allocate a temporary buffer for this strip.                     */
/* -------------------------------------------------------------------- */
    if( poGDS->pabyBlockBuf == NULL )
    {
        poGDS->pabyBlockBuf = (GByte *) VSICalloc( 1, nBlockBufSize );
        if( poGDS->pabyBlockBuf == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                   "Unable to allocate %d bytes for a temporary strip buffer\n"
                      "in GeoTIFF driver.",
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
            if( TIFFReadEncodedTile(poGDS->hTIFF, nBlockId,
                                    poGDS->pabyBlockBuf,
                                    nBlockBufSize) == -1 )
            {
                /* Once TIFFError() is properly hooked, this can go away */
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedStrip() failed." );
                
                memset( poGDS->pabyBlockBuf, 0, nBlockBufSize );
                
                eErr = CE_Failure;
            }
        }
        else
        {
            if( TIFFReadEncodedStrip(poGDS->hTIFF, nBlockId,
                                     poGDS->pabyBlockBuf,
                                     nBlockBufSize) == -1 )
            {
                /* Once TIFFError() is properly hooked, this can go away */
                CPLError( CE_Failure, CPLE_AppDefined,
                          "TIFFReadEncodedStrip() failed." );
                
                memset( poGDS->pabyBlockBuf, 0, nBlockBufSize );
                
                eErr = CE_Failure;
            }
        }
    }

    poGDS->nLoadedBlock = nBlockId;
                              
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

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/*                                                                      */
/*      This is still limited to writing stripped datasets.             */
/************************************************************************/

CPLErr GTiffRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
    int		nBlockId, nBlockBufSize;

    poGDS->Crystalize();
    poGDS->SetDirectory();

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    CPLAssert( poGDS->nBands == 1 
               || poGDS->nPlanarConfig == PLANARCONFIG_SEPARATE );

    nBlockId = nBlockXOff + nBlockYOff * nBlocksPerRow
        + (nBand-1) * poGDS->nBlocksPerBand;
        
    if( TIFFIsTiled(poGDS->hTIFF) )
    {
        nBlockBufSize = TIFFTileSize( poGDS->hTIFF );
        TIFFWriteEncodedTile( poGDS->hTIFF, nBlockId, pImage, nBlockBufSize );
    }
    else
    {
        nBlockBufSize = TIFFStripSize( poGDS->hTIFF );
        TIFFWriteEncodedStrip( poGDS->hTIFF, nBlockId, pImage, nBlockBufSize );
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffRasterBand::GetColorInterpretation()

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    if( poGDS->nPhotometric == PHOTOMETRIC_RGB )
    {
        if( nBand == 1 )
            return GCI_RedBand;
        else if( nBand == 2 )
            return GCI_GreenBand;
        else if( nBand == 3 )
            return GCI_BlueBand;
        else if( nBand == 4 )
            return GCI_AlphaBand;
        else
            return GCI_Undefined;
    }
    else if( poGDS->nPhotometric == PHOTOMETRIC_PALETTE )
    {
        return GCI_PaletteIndex;
    }
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffRasterBand::GetColorTable()

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

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
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Check if this is even a candidate for applying a PCT.           */
/* -------------------------------------------------------------------- */
    if( poGDS->bCrystalized || poGDS->nSamplesPerPixel != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "SetColorTable() not supported for existing TIFF files." );
        return CE_Failure;
    }
        
/* -------------------------------------------------------------------- */
/*      Write out the colortable, and update the configuration.         */
/* -------------------------------------------------------------------- */
    unsigned short	anTRed[256], anTGreen[256], anTBlue[256];

    for( int iColor = 0; iColor < 256; iColor++ )
    {
        if( iColor < poCT->GetColorEntryCount() )
        {
            GDALColorEntry  sRGB;
            
            poCT->GetColorEntryAsRGB( iColor, &sRGB );
            
            anTRed[iColor] = (unsigned short) (256 * sRGB.c1);
            anTGreen[iColor] = (unsigned short) (256 * sRGB.c2);
            anTBlue[iColor] = (unsigned short) (256 * sRGB.c3);
        }
        else
        {
            anTRed[iColor] = anTGreen[iColor] = anTBlue[iColor] = 0;
        }
    }

    TIFFSetField( poGDS->hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
    TIFFSetField( poGDS->hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue );

    if( poGDS->poColorTable )
        delete poGDS->poColorTable;

    poGDS->poColorTable = poCT->Clone();

    return CE_None;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GTiffRasterBand::GetOverviewCount()

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    return poGDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GTiffRasterBand::GetOverview( int i )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;

    if( i < 0 || i >= poGDS->nOverviewCount )
        return NULL;
    else
        return poGDS->papoOverviewDS[i]->GetRasterBand(nBand);
}

/************************************************************************/
/* ==================================================================== */
/*                             GTiffRGBABand                            */
/* ==================================================================== */
/************************************************************************/

class GTiffRGBABand : public GDALRasterBand
{
    friend	GTiffDataset;

  public:

                   GTiffRGBABand( GTiffDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                           GTiffRGBABand()                            */
/************************************************************************/

GTiffRGBABand::GTiffRGBABand( GTiffDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->nBlockXSize;
    nBlockYSize = poDS->nBlockYSize;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTiffRGBABand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    GTiffDataset	*poGDS = (GTiffDataset *) poDS;
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
                   "Unable to allocate %d bytes for a temporary strip buffer\n"
                      "in GeoTIFF driver.",
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
    int   iDestLine;
    int   nThisBlockYSize;

    if( (nBlockYOff+1) * nBlockYSize > GetYSize()
        && !TIFFIsTiled( poGDS->hTIFF ) )
        nThisBlockYSize = GetYSize() - nBlockYOff * nBlockYSize;
    else
        nThisBlockYSize = nBlockYSize;

    for( iDestLine = 0; iDestLine < nThisBlockYSize; iDestLine++ )
    {
        int	nSrcOffset;

        nSrcOffset = (nThisBlockYSize - iDestLine - 1) * nBlockXSize * 4;

        GDALCopyWords( poGDS->pabyBlockBuf + nBand-1 + nSrcOffset, GDT_Byte, 4,
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
/*                            GTiffDataset                              */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            GTiffDataset()                            */
/************************************************************************/

GTiffDataset::GTiffDataset()

{
    nLoadedBlock = -1;
    pabyBlockBuf = NULL;
    hTIFF = NULL;
    bNewDataset = FALSE;
    bCrystalized = TRUE;
    poColorTable = NULL;
    pszProjection = NULL;
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

    if( bNewDataset )
        WriteGeoTIFFInfo();

    if( bBase )
    {
        XTIFFClose( hTIFF );
    }

    if( nGCPCount > 0 )
    {
        for( int i = 0; i < nGCPCount; i++ )
            CPLFree( pasGCPList[i].pszId );

        CPLFree( pasGCPList );
    }

    if( pszTFWFilename != NULL )
        CPLFree( pszTFWFilename );
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
        bCrystalized = TRUE;

        TIFFWriteCheck( hTIFF, TIFFIsTiled(hTIFF), "GTiffDataset::Crystalize");
        TIFFWriteDirectory( hTIFF );

        TIFFSetDirectory( hTIFF, 0 );
        nDirOffset = TIFFCurrentDirOffset( hTIFF );
    }
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we can also flush out local tiff strip      */
/*      cache if need be.                                               */
/************************************************************************/

void GTiffDataset::FlushCache()

{
    GDALDataset::FlushCache();

    CPLFree( pabyBlockBuf );
    pabyBlockBuf = NULL;
    nLoadedBlock = -1;
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
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    unsigned short	anTRed[256], anTGreen[256], anTBlue[256];
    unsigned short      *panRed=NULL, *panGreen=NULL, *panBlue=NULL;

    if( nPhotometric == PHOTOMETRIC_PALETTE && poColorTable != NULL )
    {
        for( int iColor = 0; iColor < 256; iColor++ )
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

        panRed = anTRed;
        panGreen = anTGreen;
        panBlue = anTBlue;
    }
        
/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nOverviews; i++ )
    {
        int   j;

        for( j = 0; j < nOverviewCount; j++ )
        {
            int    nOvFactor;

            poODS = papoOverviewDS[j];

            nOvFactor = (int) 
                (0.5 + GetRasterXSize() / (double) poODS->GetRasterXSize());

            if( nOvFactor == panOverviewList[i] )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
        {
            uint32	nOverviewOffset;
            int         nOXSize, nOYSize;

            nOXSize = (GetRasterXSize() + panOverviewList[i] - 1) 
                / panOverviewList[i];
            nOYSize = (GetRasterYSize() + panOverviewList[i] - 1)
                / panOverviewList[i];

            nOverviewOffset = 
                TIFF_WriteOverview( hTIFF, nOXSize, nOYSize, 
                                    nBitsPerSample, nSamplesPerPixel, 
                                    128, 128, TRUE, COMPRESSION_NONE, 
                                    nPhotometric, nSampleFormat, 
                                    panRed, panGreen, panBlue, FALSE );

            poODS = new GTiffDataset();
            if( poODS->OpenOffset( hTIFF, nOverviewOffset, FALSE, 
                                   GA_Update ) != CE_None )
            {
                delete poODS;
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

                nOvFactor = (int) 
                  (0.5 + poBand->GetXSize() / (double) poOverview->GetXSize());

                if( nOvFactor == panOverviewList[i] )
                {
                    papoOverviewBands[nNewOverviews++] = poOverview;
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
    if( adfGeoTransform[0] == 0.0 && adfGeoTransform[1] == 1.0
        && adfGeoTransform[2] == 0.0 && adfGeoTransform[3] == 0.0
        && adfGeoTransform[4] == 0.0 && ABS(adfGeoTransform[5]) == 1.0 )
        return;

/* -------------------------------------------------------------------- */
/*      Write the transform.  We ignore the rotational coefficients     */
/*      for now.  We will fix this up later. (notdef)                   */
/* -------------------------------------------------------------------- */
    if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 )
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
/*	Write out projection definition.				*/
/* -------------------------------------------------------------------- */
    if( !EQUAL(pszProjection,"") )
    {
        GTIF	*psGTIF;

        psGTIF = GTIFNew( hTIFF );
        GTIFSetFromOGISDefn( psGTIF, pszProjection );
        GTIFWriteKeys( psGTIF );
        GTIFFree( psGTIF );
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

/************************************************************************/
/*                            SetDirectory()                            */
/************************************************************************/

int GTiffDataset::SetDirectory( uint32 nNewOffset )

{
    Crystalize();

    if( nNewOffset == 0 )
        nNewOffset = nDirOffset;

    if( nNewOffset == 0)
        return TRUE;

    if( TIFFCurrentDirOffset(hTIFF) == nNewOffset )
        return TRUE;

    if( GetAccess() == GA_Update )
        TIFFFlush( hTIFF );
    
    return TIFFSetSubDirectory( hTIFF, nNewOffset );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTiffDataset::Open( GDALOpenInfo * poOpenInfo )

{
    TIFF	*hTIFF;

/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 2 )
        return NULL;

    if( (poOpenInfo->pabyHeader[0] != 'I' || poOpenInfo->pabyHeader[1] != 'I')
     && (poOpenInfo->pabyHeader[0] != 'M' || poOpenInfo->pabyHeader[1] != 'M'))
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
	hTIFF = XTIFFOpen( poOpenInfo->pszFilename, "r" );
    else
        hTIFF = XTIFFOpen( poOpenInfo->pszFilename, "r+" );
    
    if( hTIFF == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTiffDataset 	*poDS;

    poDS = new GTiffDataset();
    poDS->SetDescription( poOpenInfo->pszFilename );

    if( poDS->OpenOffset(hTIFF,TIFFCurrentDirOffset(hTIFF), TRUE,
                         poOpenInfo->eAccess ) != CE_None )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                             OpenOffset()                             */
/*                                                                      */
/*      Initialize the GTiffDataset based on a passed in file           */
/*      handle, and directory offset to utilize.  This is called for    */
/*      full res, and overview pages.                                   */
/************************************************************************/

CPLErr GTiffDataset::OpenOffset( TIFF *hTIFFIn, uint32 nDirOffsetIn, 
				 int bBaseIn, GDALAccess eAccess )

{
    uint32	nXSize, nYSize;

    hTIFF = hTIFFIn;

    poDriver = poGTiffDriver;
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
/*      Should we treat this via the RGBA interface?                    */
/* -------------------------------------------------------------------- */
    if( nPhotometric == PHOTOMETRIC_YCBCR
        || nPhotometric == PHOTOMETRIC_CIELAB
        || nPhotometric == PHOTOMETRIC_LOGL
        || nPhotometric == PHOTOMETRIC_LOGLUV
        || nBitsPerSample < 8 )
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
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        if( bTreatAsRGBA )
            SetBand( iBand+1, new GTiffRGBABand( this, iBand+1 ) );
        else
            SetBand( iBand+1, new GTiffRasterBand( this, iBand+1 ) );
    }

/* -------------------------------------------------------------------- */
/*      Get the transform or gcps from the GeoTIFF file.                */
/* -------------------------------------------------------------------- */
    double	*padfTiePoints, *padfScale, *padfMatrix;
    int16	nCount;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    
    if( TIFFGetField(hTIFF,TIFFTAG_GEOPIXELSCALE,&nCount,&padfScale )
        && nCount >= 2 )
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

    else if( TIFFGetField(hTIFF,TIFFTAG_GEOTIEPOINTS,&nCount,&padfTiePoints )
            && nCount >= 6 )
    {
        nGCPCount = nCount / 6;
        pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPCount);
        
        for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
        {
            char	szID[32];

            sprintf( szID, "%d", iGCP+1 );
            pasGCPList[iGCP].pszId = CPLStrdup( szID );
            pasGCPList[iGCP].pszInfo = "";
            pasGCPList[iGCP].dfGCPPixel = padfTiePoints[iGCP*6+0];
            pasGCPList[iGCP].dfGCPLine = padfTiePoints[iGCP*6+1];
            pasGCPList[iGCP].dfGCPX = padfTiePoints[iGCP*6+3];
            pasGCPList[iGCP].dfGCPY = padfTiePoints[iGCP*6+4];
            pasGCPList[iGCP].dfGCPZ = padfTiePoints[iGCP*6+5];
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
/*      Otherwise try looking for a .tfw file.                          */
/* -------------------------------------------------------------------- */
    else
    {
        char	*pszTFW;
        int	i;
        FILE	*fpTFW;

        pszTFW = (char *) CPLMalloc(strlen(GetDescription())+5);
        strcpy( pszTFW, GetDescription());
        for( i = strlen(pszTFW)-1; i > 0; i-- )
        {
            if( pszTFW[i] == '.' )
            {
                pszTFW[i] = '\0';
                break;
            }
        }

        strcat( pszTFW, ".tfw" );
        
        fpTFW = VSIFOpen( pszTFW, "rt" );
        if( fpTFW == NULL )
        {
            strcpy( pszTFW + strlen(pszTFW)-4, ".TFW" );
            fpTFW = VSIFOpen( pszTFW, "rt" );
        }

        if( fpTFW != NULL )
        {
            char	**papszLines;

            VSIFClose( fpTFW );

            papszLines = CSLLoad( pszTFW );
            if( CSLCount(papszLines) >= 6 
                && atof(papszLines[0]) != 0.0
                && atof(papszLines[3]) != 0.0 )
            {
                adfGeoTransform[0] = atof(papszLines[4]);
                adfGeoTransform[1] = atof(papszLines[0]);
                adfGeoTransform[2] = atof(papszLines[2]);
                adfGeoTransform[3] = atof(papszLines[5]);
                adfGeoTransform[4] = atof(papszLines[1]);
                adfGeoTransform[5] = atof(papszLines[3]);

                // correct for center of pixel vs. top left of pixel
                adfGeoTransform[0] -= 0.5 * adfGeoTransform[1];
                adfGeoTransform[0] -= 0.5 * adfGeoTransform[2];
                adfGeoTransform[3] -= 0.5 * adfGeoTransform[4];
                adfGeoTransform[3] -= 0.5 * adfGeoTransform[5];
                                        
                bGeoTransformValid = TRUE;
            }
            CSLDestroy(papszLines);
            pszTFWFilename = CPLStrdup(pszTFW);
        }

        CPLFree( pszTFW );
    }

/* -------------------------------------------------------------------- */
/*      Capture the color table if there is one.                        */
/* -------------------------------------------------------------------- */
    unsigned short	*panRed, *panGreen, *panBlue;

    if( nPhotometric != PHOTOMETRIC_PALETTE 
        || bTreatAsRGBA 
        || TIFFGetField( hTIFF, TIFFTAG_COLORMAP, 
                         &panRed, &panGreen, &panBlue) == 0 )
    {
        poColorTable = NULL;
    }
    else
    {
        int	nColorCount;
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
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Capture the projection.                                         */
/* -------------------------------------------------------------------- */
    GTIF 	*hGTIF;
    GTIFDefn	sGTIFDefn;
    
    hGTIF = GTIFNew(hTIFF);

    if( GTIFGetDefn( hGTIF, &sGTIFDefn ) )
    {
//        pszProjection = GTIFGetProj4Defn( &sGTIFDefn );
        pszProjection = GTIFGetOGISDefn( &sGTIFDefn );
    }
    else
    {
        pszProjection = CPLStrdup( "" );
    }
    
    GTIFFree( hGTIF );

/* -------------------------------------------------------------------- */
/*      Capture some other potentially interesting information.         */
/* -------------------------------------------------------------------- */
    char	*pszText;

    if( TIFFGetField( hTIFF, TIFFTAG_DOCUMENTNAME, &pszText ) )
        SetMetadataItem( "TIFFTAG_DOCUMENTNAME",  pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_IMAGEDESCRIPTION, &pszText ) )
        SetMetadataItem( "TIFFTAG_IMAGEDESCRIPTION", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_SOFTWARE, &pszText ) )
        SetMetadataItem( "TIFFTAG_SOFTWARE", pszText );

    if( TIFFGetField( hTIFF, TIFFTAG_DATETIME, &pszText ) )
        SetMetadataItem(  "TIFFTAG_DATETIME", pszText );

/* -------------------------------------------------------------------- */
/*      If this is a "base" raster, we should scan for any              */
/*      associated overviews.                                           */
/* -------------------------------------------------------------------- */
    if( bBase && !bTreatAsRGBA )
    {
        while( TIFFReadDirectory( hTIFF ) != 0 )
        {
            uint32	nThisDir = TIFFCurrentDirOffset(hTIFF);
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
    uint16              nSampleFormat;
    
/* -------------------------------------------------------------------- */
/*	Setup values based on options.					*/
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszParmList,"TILED") != NULL )
        bTiled = TRUE;

    if( CSLFetchNameValue(papszParmList,"BLOCKXSIZE")  != NULL )
        nBlockXSize = atoi(CSLFetchNameValue(papszParmList,"BLOCKXSIZE"));

    if( CSLFetchNameValue(papszParmList,"BLOCKYSIZE")  != NULL )
        nBlockYSize = atoi(CSLFetchNameValue(papszParmList,"BLOCKYSIZE"));

    if( CSLFetchNameValue(papszParmList,"COMPRESS")  != NULL )
    {
        if( EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"JPEG") )
            nCompression = COMPRESSION_JPEG;
        else if( EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"LZW") )
            nCompression = COMPRESSION_LZW;
        else if( EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"PACKBITS"))
            nCompression = COMPRESSION_PACKBITS;
        else if( EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"DEFLATE")
               || EQUAL(CSLFetchNameValue(papszParmList,"COMPRESS"),"ZIP"))
            nCompression = COMPRESSION_DEFLATE;
        else
            CPLError( CE_Warning, CPLE_IllegalArg, 
                      "COMPRESS=%s value not recognised, ignoring.", 
                      CSLFetchNameValue(papszParmList,"COMPRESS") );
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    hTIFF = XTIFFOpen( pszFilename, "w+" );
    if( hTIFF == NULL )
    {
        if( CPLGetLastErrorNo() == 0 )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attempt to create new tiff file `%s'\n"
                      "failed in XTIFFOpen().\n",
                      pszFilename );
    }

/* -------------------------------------------------------------------- */
/*      Setup some standard flags.                                      */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompression );
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, GDALGetDataTypeSize(eType) );

    if( eType == GDT_Int16 || eType == GDT_Int32 )
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

    if( nBands > 1 )
    {
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_SEPARATE );
    }
    else
    {
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    }

    if( nBands == 3 )
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    else
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    
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
            nRowsPerStrip = MIN(nYSize,
                                      (int)TIFFDefaultStripSize(hTIFF,0));
        else
            nRowsPerStrip = nBlockYSize;
        
        TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, nRowsPerStrip );
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
    hTIFF = GTiffCreate( pszFilename, nXSize, nYSize, nBands, 
                         eType, papszParmList );

    if( hTIFF == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    poDS = new GTiffDataset();
    poDS->hTIFF = hTIFF;
    poDS->poDriver = poGTiffDriver;

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->bNewDataset = TRUE;
    poDS->bCrystalized = FALSE;
    poDS->pszProjection = CPLStrdup("");
    poDS->nSamplesPerPixel = nBands;

    TIFFGetField( hTIFF, TIFFTAG_SAMPLEFORMAT, &(poDS->nSampleFormat) );
    TIFFGetField( hTIFF, TIFFTAG_PLANARCONFIG, &(poDS->nPlanarConfig) );
    TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &(poDS->nPhotometric) );
    TIFFGetField( hTIFF, TIFFTAG_BITSPERSAMPLE, &(poDS->nBitsPerSample) );

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
/*      Do we need a TFW file?                                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszParmList,"TFW")  != NULL
        || CSLFindString( papszParmList, "TFW") != -1 )
        poDS->SetupTFW( pszFilename );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    for( iBand = 0; iBand < nBands; iBand++ )
    {
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
    TIFF *hTIFF;
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    CPLErr      eErr = CE_None;
        
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */
    hTIFF = GTiffCreate( pszFilename, nXSize, nYSize, nBands, 
                         eType, papszOptions );

    if( hTIFF == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Are we really producing an RGBA image?  If so, set the          */
/*      associated alpha information.                                   */
/* -------------------------------------------------------------------- */
    if( nBands == 4 
        && poSrcDS->GetRasterBand(4)->GetColorInterpretation()==GCI_AlphaBand)
    {
        uint16 v[1];

        v[0] = EXTRASAMPLE_ASSOCALPHA;
	TIFFSetField(hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB );
    }

/* -------------------------------------------------------------------- */
/*      Does the source image consist of one band, with a palette?      */
/*      If so, copy over.                                               */
/* -------------------------------------------------------------------- */
    if( nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL )
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

                anTRed[iColor] = (unsigned short) (256 * sRGB.c1);
                anTGreen[iColor] = (unsigned short) (256 * sRGB.c2);
                anTBlue[iColor] = (unsigned short) (256 * sRGB.c3);
            }
            else
            {
                anTRed[iColor] = anTGreen[iColor] = anTBlue[iColor] = 0;
            }
        }

        TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE );
        TIFFSetField( hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue );
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

        if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 )
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

/* -------------------------------------------------------------------- */
/*      Do we need a TFW file?                                          */
/* -------------------------------------------------------------------- */
        if( CSLFetchNameValue(papszOptions,"TFW")  != NULL 
            || CSLFindString( papszOptions, "TFW") != -1 )
        {
            char	*pszPath, *pszTFWFilename;
            char	*pszBasename;
            FILE	*fp;

            pszPath = CPLStrdup( CPLGetPath(pszFilename) );
            pszBasename = CPLStrdup( CPLGetBasename(pszFilename) );
        
            pszTFWFilename = 
                CPLStrdup( CPLFormFilename(pszPath,pszBasename,"tfw") );
        
            CPLFree( pszPath );
            CPLFree( pszBasename );

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

            CPLFree( pszTFWFilename );
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise write tiepoints if they are available.                */
/* -------------------------------------------------------------------- */
    else if( poSrcDS->GetGCPCount() > 0 )
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
    }

/* -------------------------------------------------------------------- */
/*      Write the projection information, if possible.                  */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL && strlen(pszProjection) > 0 )
    {
        GTIF	*psGTIF;

        psGTIF = GTIFNew( hTIFF );
        GTIFSetFromOGISDefn( psGTIF, pszProjection );
        GTIFWriteKeys( psGTIF );
        GTIFFree( psGTIF );
    }

/* -------------------------------------------------------------------- */
/*      Copy image data ... tiled.                                      */
/* -------------------------------------------------------------------- */
    if( TIFFIsTiled( hTIFF ) )
    {
        uint32  nBlockXSize;
        uint32	nBlockYSize;
        int     nTileSize, nTilesAcross, nTilesDown, iTileX, iTileY;
        GByte   *pabyTile;
        int     nTilesDone = 0, nPixelSize;

        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &nBlockXSize );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &nBlockYSize );
        
        nTilesAcross = (nXSize+nBlockXSize-1) / nBlockXSize;
        nTilesDown = (nYSize+nBlockYSize-1) / nBlockYSize;

        nPixelSize = GDALGetDataTypeSize(eType) / 8;
        nTileSize =  nPixelSize * nBlockXSize * nBlockYSize;
        pabyTile = (GByte *) CPLMalloc(nTileSize);

        for( int iBand = 0; 
             eErr == CE_None && iBand < nBands; 
             iBand++ )
        {
            GDALRasterBand *poBand = poSrcDS->GetRasterBand(iBand+1);

            for( iTileY = 0; 
                 eErr == CE_None && iTileY < nTilesDown; 
                 iTileY++ )
            {
                for( iTileX = 0; 
                     eErr == CE_None && iTileX < nTilesAcross; 
                     iTileX++ )
                {
                    int   nThisBlockXSize = nBlockXSize;
                    int   nThisBlockYSize = nBlockYSize;

                    if( (int) ((iTileX+1) * nBlockXSize) > nXSize )
                    {
                        nThisBlockXSize = nXSize - iTileX*nBlockXSize;
                        memset( pabyTile, 0, nTileSize );
                    }

                    if( (int) ((iTileY+1) * nBlockYSize) > nYSize )
                    {
                        nThisBlockYSize = nYSize - iTileY*nBlockYSize;
                        memset( pabyTile, 0, nTileSize );
                    }

                    eErr = poBand->RasterIO( GF_Read, 
                                             iTileX * nBlockXSize, 
                                             iTileY * nBlockYSize, 
                                             nThisBlockXSize, 
                                             nThisBlockYSize,
                                             pabyTile,
                                             nThisBlockXSize, 
                                             nThisBlockYSize, eType,
                                             nPixelSize, 
                                             nBlockXSize * nPixelSize );

                    TIFFWriteEncodedTile( hTIFF, nTilesDone, pabyTile, 
                                          nTileSize );

                    nTilesDone++;

                    if( eErr == CE_None 
                        && !pfnProgress( nTilesDone / 
                                         ((double) nTilesAcross * nTilesDown * nBands),
                                         NULL, pProgressData ) )
                    {
                        eErr = CE_Failure;
                        CPLError( CE_Failure, CPLE_UserInterrupt, 
                                  "User terminated CreateCopy()" );
                    }
                }
                
            }
        }

        CPLFree( pabyTile );
    }
/* -------------------------------------------------------------------- */
/*      Copy image data, one scanline at a time.                        */
/* -------------------------------------------------------------------- */
    else
    {
        int     nLinesDone = 0, nPixelSize, nLineSize;
        GByte   *pabyLine;

        nPixelSize = GDALGetDataTypeSize(eType) / 8;
        nLineSize =  nPixelSize * nXSize;
        pabyLine = (GByte *) CPLMalloc(nLineSize);

        for( int iBand = 0; 
             eErr == CE_None && iBand < nBands; 
             iBand++ )
        {
            GDALRasterBand *poBand = poSrcDS->GetRasterBand(iBand+1);

            for( int iLine = 0; 
                 eErr == CE_None && iLine < nYSize; 
                 iLine++ )
            {
                eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                         pabyLine, nXSize, 1, eType, 
                                         0, 0 );

                if( TIFFWriteScanline( hTIFF, pabyLine, iLine, iBand ) == -1 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "TIFFWriteScanline failed." );
                }

                nLinesDone++;
                if( eErr == CE_None 
                    && !pfnProgress( nLinesDone / 
                                     ((double) nYSize * nBands), 
                                     NULL, pProgressData ) )
                {
                    eErr = CE_Failure;
                    CPLError( CE_Failure, CPLE_UserInterrupt, 
                              "User terminated CreateCopy()" );
                }
            }
        }

        CPLFree( pabyLine );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    TIFFFlush( hTIFF );
    XTIFFClose( hTIFF );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GTiffDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

CPLErr GTiffDataset::SetProjection( const char * pszNewProjection )

{
    if( !EQUALN(pszNewProjection,"GEOGCS",6)
        && !EQUALN(pszNewProjection,"PROJCS",6)
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

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    if( bGeoTransformValid )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTiffDataset::SetGeoTransform( double * padfTransform )

{
    if( bNewDataset )
    {
        memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
        bGeoTransformValid = TRUE;
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
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *GTiffDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
void
GTiffWarningHandler(const char* module, const char* fmt, va_list ap )
{
    char	szModFmt[128];

    if( strstr(fmt,"unknown field") != NULL )
        return;

    sprintf( szModFmt, "%s:%s", module, fmt );
    CPLErrorV( CE_Warning, CPLE_AppDefined, szModFmt, ap );
}

/************************************************************************/
/*                        GTiffWarningHandler()                         */
/************************************************************************/
void
GTiffErrorHandler(const char* module, const char* fmt, va_list ap )
{
    char	szModFmt[128];

    sprintf( szModFmt, "%s:%s", module, fmt );
    CPLErrorV( CE_Failure, CPLE_AppDefined, szModFmt, ap );
}

/************************************************************************/
/*                          GDALRegister_GTiff()                        */
/************************************************************************/

void GDALRegister_GTiff()

{
    GDALDriver	*poDriver;

    if( poGTiffDriver == NULL )
    {
        poGTiffDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "GTiff";
        poDriver->pszLongName = "GeoTIFF";
        poDriver->pszHelpTopic = "frmt_gtiff.html";
        
        poDriver->pfnOpen = GTiffDataset::Open;
        poDriver->pfnCreate = GTiffDataset::Create;
        poDriver->pfnCreateCopy = GTiffCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );

        TIFFSetWarningHandler( GTiffWarningHandler );
        TIFFSetErrorHandler( GTiffErrorHandler );
    }
}
