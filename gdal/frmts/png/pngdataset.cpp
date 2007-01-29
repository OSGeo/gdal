/******************************************************************************
 * $Id$
 *
 * Project:  PNG Driver
 * Purpose:  Implement GDAL PNG Support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * ISSUES:
 *  o CollectMetadata() will only capture TEXT chunks before the image 
 *    data as the code is currently structured. 
 *  o Interlaced images are read entirely into memory for use.  This is
 *    bad for large images.
 *  o Image reading is always strictly sequential.  Reading backwards will
 *    cause the file to be rewound, and access started again from the
 *    beginning. 
 *  o 1, 2 and 4 bit data promoted to 8 bit. 
 *  o Transparency values not currently read and applied to palette.
 *  o 16 bit alpha values are not scaled by to eight bit. 
 *  o I should install setjmp()/longjmp() based error trapping for PNG calls.
 *    Currently a failure in png libraries will result in a complete
 *    application termination. 
 * 
 */

#include "gdal_pam.h"
#include "png.h"
#include "cpl_string.h"
#include <setjmp.h>

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_PNG(void);
CPL_C_END

static void
png_vsi_read_data(png_structp png_ptr, png_bytep data, png_size_t length);

static void
png_vsi_write_data(png_structp png_ptr, png_bytep data, png_size_t length);

static void png_vsi_flush(png_structp png_ptr);

static void png_gdal_error( png_structp png_ptr, const char *error_message );
static void png_gdal_warning( png_structp png_ptr, const char *error_message );

/************************************************************************/
/* ==================================================================== */
/*				PNGDataset				*/
/* ==================================================================== */
/************************************************************************/

class PNGRasterBand;

class PNGDataset : public GDALPamDataset
{
    friend class PNGRasterBand;

    FILE        *fpImage;
    png_structp hPNG;
    png_infop   psPNGInfo;
    int         nBitDepth;
    int         nColorType; /* PNG_COLOR_TYPE_* */
    int         bInterlaced;

    int         nBufferStartLine;
    int         nBufferLines;
    int         nLastLineRead;
    GByte      *pabyBuffer;

    GDALColorTable *poColorTable;

    int	   bGeoTransformValid;
    double adfGeoTransform[6];

    int		bHaveNoData;
    double 	dfNoDataValue;

    void        CollectMetadata();
    CPLErr      LoadScanline( int );
    void        Restart();

  public:
                 PNGDataset();
                 ~PNGDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * );
    virtual void FlushCache( void );

    // semi-private.
    jmp_buf     sSetJmpContext;
};

/************************************************************************/
/* ==================================================================== */
/*                            PNGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class PNGRasterBand : public GDALPamRasterBand
{
    friend class PNGDataset;

  public:

                   PNGRasterBand( PNGDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual double GetNoDataValue( int *pbSuccess = NULL );
};


/************************************************************************/
/*                           PNGRasterBand()                            */
/************************************************************************/

PNGRasterBand::PNGRasterBand( PNGDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    if( poDS->nBitDepth == 16 )
        eDataType = GDT_UInt16;
    else
        eDataType = GDT_Byte;

    nBlockXSize = poDS->nRasterXSize;;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PNGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    PNGDataset	*poGDS = (PNGDataset *) poDS;
    CPLErr      eErr;
    GByte       *pabyScanline;
    int         i, nPixelSize, nPixelOffset, nXSize = GetXSize();
    
    CPLAssert( nBlockXOff == 0 );

    if( poGDS->nBitDepth == 16 )
        nPixelSize = 2;
    else
        nPixelSize = 1;
    nPixelOffset = poGDS->nBands * nPixelSize;

/* -------------------------------------------------------------------- */
/*      Load the desired scanline into the working buffer.              */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadScanline( nBlockYOff );
    if( eErr != CE_None )
        return eErr;

    pabyScanline = poGDS->pabyBuffer 
        + (nBlockYOff - poGDS->nBufferStartLine) * nPixelOffset * nXSize
        + nPixelSize * (nBand - 1);

/* -------------------------------------------------------------------- */
/*      Transfer between the working buffer the the callers buffer.     */
/* -------------------------------------------------------------------- */
    if( nPixelSize == nPixelOffset )
        memcpy( pImage, pabyScanline, nPixelSize * nXSize );
    else if( nPixelSize == 1 )
    {
        for( i = 0; i < nXSize; i++ )
            ((GByte *) pImage)[i] = pabyScanline[i*nPixelOffset];
    }
    else 
    {
        CPLAssert( nPixelSize == 2 );
        for( i = 0; i < nXSize; i++ )
        {
            ((GUInt16 *) pImage)[i] = 
                *((GUInt16 *) (pabyScanline+i*nPixelOffset));
        }
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp PNGRasterBand::GetColorInterpretation()

{
    PNGDataset	*poGDS = (PNGDataset *) poDS;

    if( poGDS->nColorType == PNG_COLOR_TYPE_GRAY )
        return GCI_GrayIndex;

    else if( poGDS->nColorType == PNG_COLOR_TYPE_GRAY_ALPHA )
    {
        if( nBand == 1 )
            return GCI_GrayIndex;
        else
            return GCI_AlphaBand;
    }

    else  if( poGDS->nColorType == PNG_COLOR_TYPE_PALETTE )
        return GCI_PaletteIndex;

    else  if( poGDS->nColorType == PNG_COLOR_TYPE_RGB
              || poGDS->nColorType == PNG_COLOR_TYPE_RGB_ALPHA )
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
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *PNGRasterBand::GetColorTable()

{
    PNGDataset	*poGDS = (PNGDataset *) poDS;

    if( nBand == 1 )
        return poGDS->poColorTable;
    else
        return NULL;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double PNGRasterBand::GetNoDataValue( int *pbSuccess )

{
    PNGDataset *poPDS = (PNGDataset *) poDS;

    if( poPDS->bHaveNoData )
    {
        if( pbSuccess != NULL )
            *pbSuccess = poPDS->bHaveNoData;
        return poPDS->dfNoDataValue;
    }
    else
    {
        return GDALPamRasterBand::GetNoDataValue( pbSuccess );
    }
}

/************************************************************************/
/* ==================================================================== */
/*                             PNGDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            PNGDataset()                            */
/************************************************************************/

PNGDataset::PNGDataset()

{
    hPNG = NULL;
    psPNGInfo = NULL;
    pabyBuffer = NULL;
    nBufferStartLine = 0;
    nBufferLines = 0;
    nLastLineRead = -1;
    poColorTable = NULL;

    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    bHaveNoData = FALSE;
    dfNoDataValue = -1;
}

/************************************************************************/
/*                           ~PNGDataset()                            */
/************************************************************************/

PNGDataset::~PNGDataset()

{
    FlushCache();

    if( hPNG != NULL )
        png_destroy_read_struct( &hPNG, &psPNGInfo, NULL );

    if( fpImage )
        VSIFCloseL( fpImage );

    if( poColorTable != NULL )
        delete poColorTable;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PNGDataset::GetGeoTransform( double * padfTransform )

{

    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }
    else
        return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we can also flush out local tiff strip      */
/*      cache if need be.                                               */
/************************************************************************/

void PNGDataset::FlushCache()

{
    GDALPamDataset::FlushCache();

    if( pabyBuffer != NULL )
    {
        CPLFree( pabyBuffer );
        pabyBuffer = NULL;
        nBufferStartLine = 0;
        nBufferLines = 0;
    }
}

/************************************************************************/
/*                              Restart()                               */
/*                                                                      */
/*      Restart reading from the beginning of the file.                 */
/************************************************************************/

void PNGDataset::Restart()

{
    png_destroy_read_struct( &hPNG, &psPNGInfo, NULL );

    hPNG = png_create_read_struct( PNG_LIBPNG_VER_STRING, this, NULL, NULL );

    png_set_error_fn( hPNG, this, png_gdal_error, png_gdal_warning );
    if( setjmp( sSetJmpContext ) != 0 )
        return;

    psPNGInfo = png_create_info_struct( hPNG );

    VSIFSeekL( fpImage, 0, SEEK_SET );
    png_set_read_fn( hPNG, fpImage, png_vsi_read_data );
    png_read_info( hPNG, psPNGInfo );

    if( nBitDepth < 8 )
        png_set_packing( hPNG );

    nLastLineRead = -1;
}


/************************************************************************/
/*                            LoadScanline()                            */
/************************************************************************/

CPLErr PNGDataset::LoadScanline( int nLine )

{
    int   i;
    int   nPixelOffset;

    CPLAssert( nLine >= 0 && nLine < GetRasterYSize() );

    if( nLine >= nBufferStartLine && nLine < nBufferStartLine + nBufferLines)
        return CE_None;

    if( nBitDepth == 16 )
        nPixelOffset = 2 * GetRasterCount();
    else
        nPixelOffset = 1 * GetRasterCount();

    if( setjmp( sSetJmpContext ) != 0 )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      If the file is interlaced, we will load the entire image        */
/*      into memory using the high level API.                           */
/* -------------------------------------------------------------------- */
    if( bInterlaced )
    {
        png_bytep	*png_rows;
        
        CPLAssert( pabyBuffer == NULL );

        if( nLastLineRead != -1 )
        {
            Restart();
            if( setjmp( sSetJmpContext ) != 0 )
                return CE_Failure;
        }

        nBufferStartLine = 0;
        nBufferLines = GetRasterYSize();
        pabyBuffer = (GByte *) 
            VSIMalloc(nPixelOffset*GetRasterXSize()*GetRasterYSize());
        
        if( pabyBuffer == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, 
                      "Unable to allocate buffer for whole interlaced PNG"
                      "image of size %dx%d.\n", 
                      GetRasterXSize(), GetRasterYSize() );
            return CE_Failure;
        }

        png_rows = (png_bytep*)CPLMalloc(sizeof(png_bytep) * GetRasterYSize());
        for( i = 0; i < GetRasterYSize(); i++ )
            png_rows[i] = pabyBuffer + i * nPixelOffset * GetRasterXSize();

        png_read_image( hPNG, png_rows );

        CPLFree( png_rows );

        nLastLineRead = GetRasterYSize() - 1;

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Ensure we have space allocated for one scanline                 */
/* -------------------------------------------------------------------- */
    if( pabyBuffer == NULL )
        pabyBuffer = (GByte *) CPLMalloc(nPixelOffset * GetRasterXSize());

/* -------------------------------------------------------------------- */
/*      Otherwise we just try to read the requested row.  Do we need    */
/*      to rewind and start over?                                       */
/* -------------------------------------------------------------------- */
    if( nLine <= nLastLineRead )
    {
        Restart();
        if( setjmp( sSetJmpContext ) != 0 )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read till we get the desired row.                               */
/* -------------------------------------------------------------------- */
    png_bytep      row;

    row = pabyBuffer;
    while( nLine > nLastLineRead )
    {
        png_read_rows( hPNG, &row, NULL, 1 );
        nLastLineRead++;
    }

    nBufferStartLine = nLine;
    nBufferLines = 1;

/* -------------------------------------------------------------------- */
/*      Do swap on LSB machines.  16bit PNG data is stored in MSB       */
/*      format.                                                         */
/* -------------------------------------------------------------------- */
#ifdef CPL_LSB
    if( nBitDepth == 16 )
        GDALSwapWords( row, 2, GetRasterXSize() * GetRasterCount(), 2 );
#endif

    return CE_None;
}

/************************************************************************/
/*                          CollectMetadata()                           */
/*                                                                      */
/*      We normally do this after reading up to the image, but be       */
/*      forwarned ... we can missing text chunks this way.              */
/*                                                                      */
/*      We turn each PNG text chunk into one metadata item.  It         */
/*      might be nice to preserve language information though we        */
/*      don't try to now.                                               */
/************************************************************************/

void PNGDataset::CollectMetadata()

{
    int   nTextCount;
    png_textp text_ptr;

    if( nBitDepth < 8 )
    {
        for( int iBand = 0; iBand < nBands; iBand++ )
        {
            GetRasterBand(iBand+1)->SetMetadataItem( "NBITS", 
                         CPLString().Printf( "%ld", nBitDepth ) );
        }
    }

    if( png_get_text( hPNG, psPNGInfo, &text_ptr, &nTextCount ) == 0 )
        return;

    for( int iText = 0; iText < nTextCount; iText++ )
    {
        char	*pszTag = CPLStrdup(text_ptr[iText].key);

        for( int i = 0; pszTag[i] != '\0'; i++ )
        {
            if( pszTag[i] == ' ' || pszTag[i] == '=' || pszTag[i] == ':' )
                pszTag[i] = '_';
        }

        SetMetadataItem( pszTag, text_ptr[iText].text );
        CPLFree( pszTag );
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PNGDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 4 )
        return NULL;

    if( png_sig_cmp(poOpenInfo->pabyHeader, (png_size_t)0, 
                    poOpenInfo->nHeaderBytes) != 0 )
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The PNG driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open a file handle using large file API.                        */
/* -------------------------------------------------------------------- */
    FILE *fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unexpected failure of VSIFOpenL(%s) in PNG Open()", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    PNGDataset 	*poDS;

    poDS = new PNGDataset();

    poDS->fpImage = fp;
    poDS->eAccess = poOpenInfo->eAccess;
    
    poDS->hPNG = png_create_read_struct( PNG_LIBPNG_VER_STRING, poDS, 
                                         NULL, NULL );
    if (poDS->hPNG == NULL)
    {
#if LIBPNG_VER_MINOR >= 2 || LIBPNG_VER_MAJOR > 1
        int version = png_access_version_number();
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The PNG driver failed to access libpng with version '%s',"
                  " library is actually version '%d'.\n",
                  PNG_LIBPNG_VER_STRING, version);
#else
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The PNG driver failed to in png_create_read_struct().\n"
                  "This may be due to version compatibility problems." );
#endif
        delete poDS;
        return NULL;
    }

    poDS->psPNGInfo = png_create_info_struct( poDS->hPNG );

/* -------------------------------------------------------------------- */
/*      Setup error handling.                                           */
/* -------------------------------------------------------------------- */
    png_set_error_fn( poDS->hPNG, poDS, png_gdal_error, png_gdal_warning );

    if( setjmp( poDS->sSetJmpContext ) != 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*	Read pre-image data after ensuring the file is rewound.         */
/* -------------------------------------------------------------------- */
    /* we should likely do a setjmp() here */

    png_set_read_fn( poDS->hPNG, poDS->fpImage, png_vsi_read_data );
    png_read_info( poDS->hPNG, poDS->psPNGInfo );

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = png_get_image_width( poDS->hPNG, poDS->psPNGInfo);
    poDS->nRasterYSize = png_get_image_height( poDS->hPNG,poDS->psPNGInfo);

    poDS->nBands = png_get_channels( poDS->hPNG, poDS->psPNGInfo );
    poDS->nBitDepth = png_get_bit_depth( poDS->hPNG, poDS->psPNGInfo );
    poDS->bInterlaced = png_get_interlace_type( poDS->hPNG, poDS->psPNGInfo ) 
        != PNG_INTERLACE_NONE;

    poDS->nColorType = png_get_color_type( poDS->hPNG, poDS->psPNGInfo );

    if( poDS->nColorType == PNG_COLOR_TYPE_PALETTE 
        && poDS->nBands > 1 )
    {
        CPLDebug( "GDAL", "PNG Driver got %d from png_get_channels(),\n"
                  "but this kind of image (paletted) can only have one band.\n"
                  "Correcting and continuing, but this may indicate a bug!",
                  poDS->nBands );
        poDS->nBands = 1;
    }
    
/* -------------------------------------------------------------------- */
/*      We want to treat 1,2,4 bit images as eight bit.  This call      */
/*      causes libpng to unpack the image.                              */
/* -------------------------------------------------------------------- */
    if( poDS->nBitDepth < 8 )
        png_set_packing( poDS->hPNG );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
        poDS->SetBand( iBand+1, new PNGRasterBand( poDS, iBand+1 ) );

/* -------------------------------------------------------------------- */
/*      Is there a palette?  Note: we should also read back and         */
/*      apply transparency values if available.                         */
/* -------------------------------------------------------------------- */
    if( poDS->nColorType == PNG_COLOR_TYPE_PALETTE )
    {
        png_color *pasPNGPalette;
        int	nColorCount;
        GDALColorEntry oEntry;
        unsigned char *trans = NULL;
        png_color_16 *trans_values = NULL;
        int	num_trans = 0;
        int	nNoDataIndex = -1;

        if( png_get_PLTE( poDS->hPNG, poDS->psPNGInfo, 
                          &pasPNGPalette, &nColorCount ) == 0 )
            nColorCount = 0;

        png_get_tRNS( poDS->hPNG, poDS->psPNGInfo, 
                      &trans, &num_trans, &trans_values );

        poDS->poColorTable = new GDALColorTable();

        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            oEntry.c1 = pasPNGPalette[iColor].red;
            oEntry.c2 = pasPNGPalette[iColor].green;
            oEntry.c3 = pasPNGPalette[iColor].blue;

            if( iColor < num_trans )
            {
                oEntry.c4 = trans[iColor];
                if( oEntry.c4 == 0 )
                {
                    if( nNoDataIndex == -1 )
                        nNoDataIndex = iColor;
                    else
                        nNoDataIndex = -2;
                }
            }
            else
                oEntry.c4 = 255;

            poDS->poColorTable->SetColorEntry( iColor, &oEntry );
        }

        /*
        ** Special hack to an index as the no data value, as long as it
        ** is the _only_ transparent color in the palette.
        */
        if( nNoDataIndex > -1 )
        {
            poDS->bHaveNoData = TRUE;
            poDS->dfNoDataValue = nNoDataIndex;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for transparency values in greyscale images.              */
/* -------------------------------------------------------------------- */
    if( poDS->nColorType == PNG_COLOR_TYPE_GRAY )
    {
        png_color_16 *trans_values = NULL;
        unsigned char *trans;
        int num_trans;

        if( png_get_tRNS( poDS->hPNG, poDS->psPNGInfo, 
                          &trans, &num_trans, &trans_values ) != 0 
            && trans_values != NULL )
        {
            poDS->bHaveNoData = TRUE;
            poDS->dfNoDataValue = trans_values->gray;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for nodata color for RGB images.                          */
/* -------------------------------------------------------------------- */
    if( poDS->nColorType == PNG_COLOR_TYPE_RGB ) 
    {
        png_color_16 *trans_values = NULL;
        unsigned char *trans;
        int num_trans;

        if( png_get_tRNS( poDS->hPNG, poDS->psPNGInfo, 
                          &trans, &num_trans, &trans_values ) != 0 
            && trans_values != NULL )
        {
            CPLString oNDValue;

            oNDValue.Printf( "%d %d %d", 
                             trans_values->red, 
                             trans_values->green, 
                             trans_values->blue );
            poDS->SetMetadataItem( "NODATA_VALUES", oNDValue.c_str() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract any text chunks as "metadata".                          */
/* -------------------------------------------------------------------- */
    poDS->CollectMetadata();

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    poDS->bGeoTransformValid = 
        GDALReadWorldFile( poOpenInfo->pszFilename, NULL, 
                           poDS->adfGeoTransform );

    if( !poDS->bGeoTransformValid )
        poDS->bGeoTransformValid = 
            GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                               poDS->adfGeoTransform );

    return poDS;
}

/************************************************************************/
/*                           PNGCreateCopy()                            */
/************************************************************************/

static GDALDataset *
PNGCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
               int bStrict, char ** papszOptions, 
               GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 && nBands != 2 && nBands != 3 && nBands != 4 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "PNG driver doesn't support %d bands.  Must be 1 (grey),\n"
                  "2 (grey+alpha), 3 (rgb) or 4 (rgba) bands.\n", 
                  nBands );

        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte 
        && poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt16
        && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "PNG driver doesn't support data type %s. "
                  "Only eight bit (Byte) and sixteen bit (UInt16) bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup some parameters.                                          */
/* -------------------------------------------------------------------- */
    int  nColorType=0, nBitDepth;
    GDALDataType eType;

    if( nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() == NULL )
        nColorType = PNG_COLOR_TYPE_GRAY;
    else if( nBands == 1 )
        nColorType = PNG_COLOR_TYPE_PALETTE;
    else if( nBands == 2 )
        nColorType = PNG_COLOR_TYPE_GRAY_ALPHA;
    else if( nBands == 3 )
        nColorType = PNG_COLOR_TYPE_RGB;
    else if( nBands == 4 )
        nColorType = PNG_COLOR_TYPE_RGB_ALPHA;

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt16 )
    {
        eType = GDT_Byte;
        nBitDepth = 8;
    }
    else 
    {
        eType = GDT_UInt16;
        nBitDepth = 16;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    FILE	*fpImage;

    fpImage = VSIFOpenL( pszFilename, "wb" );
    if( fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create png file %s.\n", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize PNG access to the file.                              */
/* -------------------------------------------------------------------- */
    png_structp hPNG;
    png_infop   psPNGInfo;
    
    hPNG = png_create_write_struct( PNG_LIBPNG_VER_STRING, 
                                    NULL, NULL, NULL );
    psPNGInfo = png_create_info_struct( hPNG );
    
    png_set_write_fn( hPNG, fpImage, png_vsi_write_data, png_vsi_flush );

    png_set_IHDR( hPNG, psPNGInfo, nXSize, nYSize, 
                  nBitDepth, nColorType, PNG_INTERLACE_NONE, 
                  PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );

/* -------------------------------------------------------------------- */
/*      Try to handle nodata values as a tRNS block (note for           */
/*      paletted images, we save the effect to apply as part of         */
/*      palette).                                                       */
/* -------------------------------------------------------------------- */
    int		bHaveNoData = FALSE;
    double	dfNoDataValue = -1;
    png_color_16 sTRNSColor;

    dfNoDataValue = poSrcDS->GetRasterBand(1)->GetNoDataValue( &bHaveNoData );

    if( (nColorType == PNG_COLOR_TYPE_GRAY )
        && dfNoDataValue > 0 && dfNoDataValue < 65536 )
    {
        sTRNSColor.gray = (png_uint_16) dfNoDataValue;
        png_set_tRNS( hPNG, psPNGInfo, NULL, 0, &sTRNSColor );
    }

    // RGB case.
    if( nColorType == PNG_COLOR_TYPE_RGB 
        && poSrcDS->GetMetadataItem( "NODATA_VALUES" ) != NULL )
    {
        char **papszValues = CSLTokenizeString(
            poSrcDS->GetMetadataItem( "NODATA_VALUES" ) );
        
        if( CSLCount(papszValues) >= 3 )
        {
            sTRNSColor.red   = (png_uint_16) atoi(papszValues[0]);
            sTRNSColor.green = (png_uint_16) atoi(papszValues[1]);
            sTRNSColor.blue  = (png_uint_16) atoi(papszValues[2]);
            png_set_tRNS( hPNG, psPNGInfo, NULL, 0, &sTRNSColor );
        }

        CSLDestroy( papszValues );
    }

/* -------------------------------------------------------------------- */
/*      Write palette if there is one.  Technically, I think it is      */
/*      possible to write 16bit palettes for PNG, but we will omit      */
/*      this for now.                                                   */
/* -------------------------------------------------------------------- */
    png_color	*pasPNGColors = NULL;
    unsigned char	*pabyAlpha = NULL;

    if( nColorType == PNG_COLOR_TYPE_PALETTE )
    {
        GDALColorTable	*poCT;
        GDALColorEntry  sEntry;
        int		iColor, bFoundTrans = FALSE;

        poCT = poSrcDS->GetRasterBand(1)->GetColorTable();

        pasPNGColors = (png_color *) CPLMalloc(sizeof(png_color) *
                                               poCT->GetColorEntryCount());

        for( iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
        {
            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            if( sEntry.c4 != 255 )
                bFoundTrans = TRUE;

            pasPNGColors[iColor].red = (png_byte) sEntry.c1;
            pasPNGColors[iColor].green = (png_byte) sEntry.c2;
            pasPNGColors[iColor].blue = (png_byte) sEntry.c3;
        }
        
        png_set_PLTE( hPNG, psPNGInfo, pasPNGColors, 
                      poCT->GetColorEntryCount() );

/* -------------------------------------------------------------------- */
/*      If we have transparent elements in the palette we need to       */
/*      write a transparency block.                                     */
/* -------------------------------------------------------------------- */
        if( bFoundTrans || bHaveNoData )
        {

            pabyAlpha = (unsigned char *)CPLMalloc(poCT->GetColorEntryCount());

            for( iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
            {
                poCT->GetColorEntryAsRGB( iColor, &sEntry );
                pabyAlpha[iColor] = (unsigned char) sEntry.c4;

                if( bHaveNoData && iColor == (int) dfNoDataValue )
                    pabyAlpha[iColor] = 0;
            }

            png_set_tRNS( hPNG, psPNGInfo, pabyAlpha, 
                          poCT->GetColorEntryCount(), NULL );
        }
    }

    png_write_info( hPNG, psPNGInfo );

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GByte 	*pabyScanline;
    CPLErr      eErr = CE_None;
    int         nWordSize = nBitDepth/8;

    pabyScanline = (GByte *) CPLMalloc( nBands * nXSize * nWordSize );

    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
    {
        png_bytep       row = pabyScanline;

        for( int iBand = 0; iBand < nBands; iBand++ )
        {
            GDALRasterBand * poBand = poSrcDS->GetRasterBand( iBand+1 );
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                     pabyScanline + iBand*nWordSize, 
                                     nXSize, 1, eType,
                                     nBands * nWordSize, 
                                     nBands * nXSize * nWordSize );
        }

#ifdef CPL_LSB
        if( nBitDepth == 16 )
            GDALSwapWords( row, 2, nXSize * nBands, 2 );
#endif
        if( eErr == CE_None )
            png_write_rows( hPNG, &row, 1 );

        if( eErr == CE_None
            && !pfnProgress( (iLine+1) / (double) nYSize,
                             NULL, pProgressData ) )
        {
            eErr = CE_Failure;
            CPLError( CE_Failure, CPLE_UserInterrupt,
                      "User terminated CreateCopy()" );
        }
    }

    CPLFree( pabyScanline );

    png_write_end( hPNG, psPNGInfo );
    png_destroy_write_struct( &hPNG, &psPNGInfo );

    VSIFCloseL( fpImage );

    CPLFree( pabyAlpha );
    CPLFree( pasPNGColors );

    if( eErr != CE_None )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Do we need a world file?                                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "WORLDFILE", FALSE ) )
    {
    	double      adfGeoTransform[6];
	
	poSrcDS->GetGeoTransform( adfGeoTransform );
	GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    PNGDataset *poDS = (PNGDataset *) GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                         png_vsi_read_data()                          */
/*                                                                      */
/*      Read data callback through VSI.                                 */
/************************************************************************/
static void
png_vsi_read_data(png_structp png_ptr, png_bytep data, png_size_t length)

{
   png_size_t check;

   /* fread() returns 0 on error, so it is OK to store this in a png_size_t
    * instead of an int, which is what fread() actually returns.
    */
   check = (png_size_t)VSIFReadL(data, (png_size_t)1, length,
                                 (png_FILE_p)png_ptr->io_ptr);

   if (check != length)
      png_error(png_ptr, "Read Error");
}

/************************************************************************/
/*                         png_vsi_write_data()                         */
/************************************************************************/

static void
png_vsi_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
   png_uint_32 check;

   check = VSIFWriteL(data, 1, length, (png_FILE_p)(png_ptr->io_ptr));

   if (check != length)
      png_error(png_ptr, "Write Error");
}

/************************************************************************/
/*                           png_vsi_flush()                            */
/************************************************************************/
static void png_vsi_flush(png_structp png_ptr)
{
    VSIFFlushL( (png_FILE_p)(png_ptr->io_ptr) );
}

/************************************************************************/
/*                           png_gdal_error()                           */
/************************************************************************/

static void png_gdal_error( png_structp png_ptr, const char *error_message )
{
    CPLError( CE_Failure, CPLE_AppDefined, 
              "libpng: %s", error_message );

    // We have to use longjmp instead of a C++ exception because 
    // libpng is generally not built as C++ and so won't honour unwind
    // semantics.  Ugg. 

    PNGDataset *poDS = (PNGDataset *) png_ptr->error_ptr;

    longjmp( poDS->sSetJmpContext, 1 );
}

/************************************************************************/
/*                          png_gdal_warning()                          */
/************************************************************************/

static void png_gdal_warning( png_structp png_ptr, const char *error_message )
{
    CPLError( CE_Warning, CPLE_AppDefined, 
              "libpng: %s", error_message );
}

/************************************************************************/
/*                          GDALRegister_PNG()                        */
/************************************************************************/

void GDALRegister_PNG()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "PNG" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "PNG" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Portable Network Graphics" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#PNG" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "png" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/png" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>\n"
"   <Option name='WORLDFILE' type='boolean' description='Create world file'/>\n"
"</CreationOptionList>\n" );

        poDriver->pfnOpen = PNGDataset::Open;
        poDriver->pfnCreateCopy = PNGCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

