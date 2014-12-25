/******************************************************************************
 * $Id$
 *
 * Project:  PNG Driver
 * Purpose:  Implement GDAL PNG Support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

// Define SUPPORT_CREATE if you want Create() call supported.
// Note: callers must provide blocks in increasing Y order.

// Disclaimer (E. Rouault) : this code is NOT production ready at all.
// A lot of issues remains : uninitialized variables, unclosed file,
// inability to handle properly multiband case, inability to read&write
// at the same time. Do NOT use it unless you're ready to fix it
//#define SUPPORT_CREATE

// we believe it is ok to use setjmp() in this situation.
#ifdef _MSC_VER
#  pragma warning(disable:4611)
#endif

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

    VSILFILE        *fpImage;
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


    void        CollectMetadata();

    int         bHasReadXMPMetadata;
    void        CollectXMPMetadata();

    CPLErr      LoadScanline( int );
    CPLErr      LoadInterlacedChunk( int );
    void        Restart();

    int         bHasTriedLoadWorldFile;
    void        LoadWorldFile();
    CPLString   osWldFilename;

    int         bHasReadICCMetadata;
    void        LoadICCProfile();

  public:
                 PNGDataset();
                 ~PNGDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset* CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );

    virtual char **GetFileList(void);

    virtual CPLErr GetGeoTransform( double * );
    virtual void FlushCache( void );

    virtual char      **GetMetadataDomainList();

    virtual char  **GetMetadata( const char * pszDomain = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = NULL );

    virtual CPLErr      IRasterIO( GDALRWFlag, int, int, int, int,
                                   void *, int, int, GDALDataType,
                                   int, int *,
                                   GSpacing, GSpacing,
                                   GSpacing,
                                   GDALRasterIOExtraArg* psExtraArg );

    // semi-private.
    jmp_buf     sSetJmpContext;

#ifdef SUPPORT_CREATE
    int        m_nBitDepth;
    GByte      *m_pabyBuffer;
    png_byte	*m_pabyAlpha;
    png_structp m_hPNG;
    png_infop   m_psPNGInfo;
    png_color	*m_pasPNGColors;
    VSILFILE        *m_fpImage;
    int	   m_bGeoTransformValid;
    double m_adfGeoTransform[6];
    char        *m_pszFilename;
    int         m_nColorType; /* PNG_COLOR_TYPE_* */

    virtual CPLErr SetGeoTransform( double * );
    static GDALDataset  *Create( const char* pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType, char** papszParmList );
  protected:
	CPLErr write_png_header();

#endif
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
    CPLErr SetNoDataValue( double dfNewValue );
    virtual double GetNoDataValue( int *pbSuccess = NULL );

    int		bHaveNoData;
    double 	dfNoDataValue;


#ifdef SUPPORT_CREATE
    virtual CPLErr SetColorTable(GDALColorTable*);
    virtual CPLErr IWriteBlock( int, int, void * );

  protected:
	int m_bBandProvided[5];
	void reset_band_provision_flags()
	{
		PNGDataset& ds = *(PNGDataset*)poDS;

		for(size_t i = 0; i < ds.nBands; i++)
			m_bBandProvided[i] = FALSE;
	}
#endif
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

    bHaveNoData = FALSE;
    dfNoDataValue = -1;

#ifdef SUPPORT_CREATE
	this->reset_band_provision_flags();
#endif
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


    if (poGDS->fpImage == NULL)
    {
        memset( pImage, 0, nPixelSize * nXSize );
        return CE_None;
    }

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

/* -------------------------------------------------------------------- */
/*      Forceably load the other bands associated with this scanline.   */
/* -------------------------------------------------------------------- */
    int iBand;
    for(iBand = 1; iBand < poGDS->GetRasterCount(); iBand++)
    {
        GDALRasterBlock *poBlock;

        poBlock = 
            poGDS->GetRasterBand(iBand+1)->GetLockedBlockRef(nBlockXOff,nBlockYOff);
        if( poBlock != NULL )
            poBlock->DropLock();
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
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr PNGRasterBand::SetNoDataValue( double dfNewValue )

{
   bHaveNoData = TRUE;
   dfNoDataValue = dfNewValue;
      
   return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double PNGRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( bHaveNoData )
    {
        if( pbSuccess != NULL )
            *pbSuccess = bHaveNoData;
        return dfNoDataValue;
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
    fpImage = NULL;
    hPNG = NULL;
    psPNGInfo = NULL;
    pabyBuffer = NULL;
    nBufferStartLine = 0;
    nBufferLines = 0;
    nLastLineRead = -1;
    poColorTable = NULL;
    nBitDepth = 8;

    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    bHasTriedLoadWorldFile = FALSE;
    bHasReadXMPMetadata = FALSE;
    bHasReadICCMetadata = FALSE;
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
/*                            IsFullBandMap()                           */
/************************************************************************/

static int IsFullBandMap(int *panBandMap, int nBands)
{
    for(int i=0;i<nBands;i++)
    {
        if( panBandMap[i] != i + 1 )
            return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr PNGDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg )

{
    if((eRWFlag == GF_Read) &&
       (nBandCount == nBands) &&
       (nXOff == 0) && (nYOff == 0) &&
       (nXSize == nBufXSize) && (nXSize == nRasterXSize) &&
       (nYSize == nBufYSize) && (nYSize == nRasterYSize) &&
       (eBufType == GDT_Byte) &&
       (eBufType == GetRasterBand(1)->GetRasterDataType()) &&
       (pData != NULL) &&
       (panBandMap != NULL) && IsFullBandMap(panBandMap, nBands))
    {
        int y;
        CPLErr tmpError;
        int x;

        // Pixel interleaved case
        if( nBandSpace == 1 )
        {
            for(y = 0; y < nYSize; ++y)
            {
                tmpError = LoadScanline(y);
                if(tmpError != CE_None) return tmpError;
                GByte* pabyScanline = pabyBuffer 
                    + (y - nBufferStartLine) * nBands * nXSize;
                if( nPixelSpace == nBandSpace * nBandCount )
                {
                    memcpy(&(((GByte*)pData)[(y*nLineSpace)]),
                           pabyScanline, nBandCount * nXSize);
                }
                else
                {
                    for(x = 0; x < nXSize; ++x)
                    {
                        memcpy(&(((GByte*)pData)[(y*nLineSpace) + (x*nPixelSpace)]), 
                               (const GByte*)&(pabyScanline[x* nBandCount]), nBandCount);
                    }
                }
            }
        }
        else
        {
            for(y = 0; y < nYSize; ++y)
            {
                tmpError = LoadScanline(y);
                if(tmpError != CE_None) return tmpError;
                GByte* pabyScanline = pabyBuffer 
                    + (y - nBufferStartLine) * nBands * nXSize;
                for(x = 0; x < nXSize; ++x)
                {
                    for(int iBand=0;iBand<nBands;iBand++)
                        ((GByte*)pData)[(y*nLineSpace) + (x*nPixelSpace) + iBand * nBandSpace] = pabyScanline[x*nBands+iBand];
                }
            }
        }

        return CE_None;
    }

    return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType, 
                                     nBandCount, panBandMap, 
                                     nPixelSpace, nLineSpace, nBandSpace,
                                     psExtraArg);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PNGDataset::GetGeoTransform( double * padfTransform )

{
    LoadWorldFile();

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

    png_set_error_fn( hPNG, &sSetJmpContext, png_gdal_error, png_gdal_warning );
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
/*                        LoadInterlacedChunk()                         */
/************************************************************************/

CPLErr PNGDataset::LoadInterlacedChunk( int iLine )

{
    int nPixelOffset;

    if( nBitDepth == 16 )
        nPixelOffset = 2 * GetRasterCount();
    else
        nPixelOffset = 1 * GetRasterCount();

/* -------------------------------------------------------------------- */
/*      Was is the biggest chunk we can safely operate on?              */
/* -------------------------------------------------------------------- */
#define MAX_PNG_CHUNK_BYTES 100000000

    int         nMaxChunkLines = 
        MAX(1,MAX_PNG_CHUNK_BYTES / (nPixelOffset * GetRasterXSize()));
    png_bytep  *png_rows;

    if( nMaxChunkLines > GetRasterYSize() )
        nMaxChunkLines = GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Allocate chunk buffer, if we don't already have it from a       */
/*      previous request.                                               */
/* -------------------------------------------------------------------- */
    nBufferLines = nMaxChunkLines;
    if( nMaxChunkLines + iLine > GetRasterYSize() )
        nBufferStartLine = GetRasterYSize() - nMaxChunkLines;
    else
        nBufferStartLine = iLine;

    if( pabyBuffer == NULL )
    {
        pabyBuffer = (GByte *) 
            VSIMalloc(nPixelOffset*GetRasterXSize()*nMaxChunkLines);
        
        if( pabyBuffer == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, 
                      "Unable to allocate buffer for whole interlaced PNG"
                      "image of size %dx%d.\n", 
                      GetRasterXSize(), GetRasterYSize() );
            return CE_Failure;
        }
#ifdef notdef
        if( nMaxChunkLines < GetRasterYSize() )
            CPLDebug( "PNG", 
                      "Interlaced file being handled in %d line chunks.\n"
                      "Performance is likely to be quite poor.",
                      nMaxChunkLines );
#endif
    }

/* -------------------------------------------------------------------- */
/*      Do we need to restart reading?  We do this if we aren't on      */
/*      the first attempt to read the image.                            */
/* -------------------------------------------------------------------- */
    if( nLastLineRead != -1 )
    {
        Restart();
        if( setjmp( sSetJmpContext ) != 0 )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate and populate rows array.  We create a row for each     */
/*      row in the image, but use our dummy line for rows not in the    */
/*      target window.                                                  */
/* -------------------------------------------------------------------- */
    int        i;
    png_bytep  dummy_row = (png_bytep)CPLMalloc(nPixelOffset*GetRasterXSize());
    png_rows = (png_bytep*)CPLMalloc(sizeof(png_bytep) * GetRasterYSize());

    for( i = 0; i < GetRasterYSize(); i++ )
    {
        if( i >= nBufferStartLine && i < nBufferStartLine + nBufferLines )
            png_rows[i] = pabyBuffer 
                + (i-nBufferStartLine) * nPixelOffset * GetRasterXSize();
        else
            png_rows[i] = dummy_row;
    }

    png_read_image( hPNG, png_rows );

    CPLFree( png_rows );
    CPLFree( dummy_row );

    nLastLineRead = nBufferStartLine + nBufferLines - 1;

    return CE_None;
}

/************************************************************************/
/*                            LoadScanline()                            */
/************************************************************************/

CPLErr PNGDataset::LoadScanline( int nLine )

{
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
        return LoadInterlacedChunk( nLine );

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
            GetRasterBand(iBand+1)->SetMetadataItem( 
                "NBITS", CPLString().Printf( "%d", nBitDepth ),
                "IMAGE_STRUCTURE" );
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
/*                       CollectXMPMetadata()                           */
/************************************************************************/

/* See §2.1.5 of http://wwwimages.adobe.com/www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMPSpecificationPart3.pdf */

void PNGDataset::CollectXMPMetadata()

{
    if (fpImage == NULL || bHasReadXMPMetadata)
        return;

    /* Save current position to avoid disturbing PNG stream decoding */
    vsi_l_offset nCurOffset = VSIFTellL(fpImage);

    vsi_l_offset nOffset = 8;
    VSIFSeekL( fpImage, nOffset, SEEK_SET );

    /* Loop over chunks */
    while(TRUE)
    {
        int nLength;
        char pszChunkType[5];
        int nCRC;

        if (VSIFReadL( &nLength, 4, 1, fpImage ) != 1)
            break;
        nOffset += 4;
        CPL_MSBPTR32(&nLength);
        if (nLength <= 0)
            break;
        if (VSIFReadL( pszChunkType, 4, 1, fpImage ) != 1)
            break;
        nOffset += 4;
        pszChunkType[4] = 0;

        if (strcmp(pszChunkType, "iTXt") == 0 && nLength > 22)
        {
            char* pszContent = (char*)VSIMalloc(nLength + 1);
            if (pszContent == NULL)
                break;
            if (VSIFReadL( pszContent, nLength, 1, fpImage) != 1)
            {
                VSIFree(pszContent);
                break;
            }
            nOffset += nLength;
            pszContent[nLength] = '\0';
            if (memcmp(pszContent, "XML:com.adobe.xmp\0\0\0\0\0", 22) == 0)
            {
                /* Avoid setting the PAM dirty bit just for that */
                int nOldPamFlags = nPamFlags;

                char *apszMDList[2];
                apszMDList[0] = pszContent + 22;
                apszMDList[1] = NULL;
                SetMetadata(apszMDList, "xml:XMP");

                nPamFlags = nOldPamFlags;

                VSIFree(pszContent);

                break;
            }
            else
            {
                VSIFree(pszContent);
            }
        }
        else
        {
            nOffset += nLength;
            VSIFSeekL( fpImage, nOffset, SEEK_SET );
        }

        nOffset += 4;
        if (VSIFReadL( &nCRC, 4, 1, fpImage ) != 1)
            break;
    }

    VSIFSeekL( fpImage, nCurOffset, SEEK_SET );

    bHasReadXMPMetadata = TRUE;
}

/************************************************************************/
/*                           LoadICCProfile()                           */
/************************************************************************/

void PNGDataset::LoadICCProfile()
{
    if (hPNG == NULL || bHasReadICCMetadata)
        return;
    bHasReadICCMetadata = TRUE;

    png_charp pszProfileName;
    png_uint_32 nProfileLength;
#if (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR > 4) || PNG_LIBPNG_VER_MAJOR > 1
    png_bytep pProfileData;
#else
    png_charp pProfileData;
#endif
    int nCompressionType;
    int nsRGBIntent;
    double dfGamma;
    bool bGammaAvailable = false;

    /* Avoid setting the PAM dirty bit just for that */
    int nOldPamFlags = nPamFlags;

    if (png_get_iCCP(hPNG, psPNGInfo, &pszProfileName,
       &nCompressionType, &pProfileData, &nProfileLength) != 0)
    {
        /* Escape the profile */
        char *pszBase64Profile = CPLBase64Encode(nProfileLength, (const GByte*)pProfileData);

        /* Set ICC profile metadata */
        SetMetadataItem( "SOURCE_ICC_PROFILE", pszBase64Profile, "COLOR_PROFILE" );
        SetMetadataItem( "SOURCE_ICC_PROFILE_NAME", pszProfileName, "COLOR_PROFILE" );

        nPamFlags = nOldPamFlags;

        CPLFree(pszBase64Profile);

        return;
    }

    if (png_get_sRGB(hPNG, psPNGInfo, &nsRGBIntent) != 0)
    {
        SetMetadataItem( "SOURCE_ICC_PROFILE_NAME", "sRGB", "COLOR_PROFILE" );

        nPamFlags = nOldPamFlags;

        return;
    }

    if (png_get_valid(hPNG, psPNGInfo, PNG_INFO_gAMA))
    {
        bGammaAvailable = true;

        png_get_gAMA(hPNG,psPNGInfo, &dfGamma);

        SetMetadataItem( "PNG_GAMMA", 
            CPLString().Printf( "%.9f", dfGamma ) , "COLOR_PROFILE" );
    }

    // Check if both cHRM and gAMA available
    if (bGammaAvailable && png_get_valid(hPNG, psPNGInfo, PNG_INFO_cHRM))
    {
        double dfaWhitepoint[2];
        double dfaCHR[6];

        png_get_cHRM(hPNG, psPNGInfo,
                    &dfaWhitepoint[0], &dfaWhitepoint[1],
                    &dfaCHR[0], &dfaCHR[1],
                    &dfaCHR[2], &dfaCHR[3],
                    &dfaCHR[4], &dfaCHR[5]);

        // Set all the colorimetric metadata.
        SetMetadataItem( "SOURCE_PRIMARIES_RED", 
            CPLString().Printf( "%.9f, %.9f, 1.0", dfaCHR[0], dfaCHR[1] ) , "COLOR_PROFILE" );
        SetMetadataItem( "SOURCE_PRIMARIES_GREEN", 
            CPLString().Printf( "%.9f, %.9f, 1.0", dfaCHR[2], dfaCHR[3] ) , "COLOR_PROFILE" );
        SetMetadataItem( "SOURCE_PRIMARIES_BLUE", 
            CPLString().Printf( "%.9f, %.9f, 1.0", dfaCHR[4], dfaCHR[5] ) , "COLOR_PROFILE" );

        SetMetadataItem( "SOURCE_WHITEPOINT", 
            CPLString().Printf( "%.9f, %.9f, 1.0", dfaWhitepoint[0], dfaWhitepoint[1] ) , "COLOR_PROFILE" );

    }

    nPamFlags = nOldPamFlags;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **PNGDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:XMP", "COLOR_PROFILE", NULL);
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char  **PNGDataset::GetMetadata( const char * pszDomain )
{
    if (fpImage == NULL)
        return NULL;
    if (eAccess == GA_ReadOnly && !bHasReadXMPMetadata &&
        pszDomain != NULL && EQUAL(pszDomain, "xml:XMP"))
        CollectXMPMetadata();
    if (eAccess == GA_ReadOnly && !bHasReadICCMetadata &&
        pszDomain != NULL && EQUAL(pszDomain, "COLOR_PROFILE"))
        LoadICCProfile();
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                       GetMetadataItem()                              */
/************************************************************************/
const char *PNGDataset::GetMetadataItem( const char * pszName,
                                         const char * pszDomain )
{
    if (eAccess == GA_ReadOnly && !bHasReadICCMetadata &&
        pszDomain != NULL && EQUAL(pszDomain, "COLOR_PROFILE"))
        LoadICCProfile();
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int PNGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 4 )
        return FALSE;

    if( png_sig_cmp(poOpenInfo->pabyHeader, (png_size_t)0, 
                    poOpenInfo->nHeaderBytes) != 0 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PNGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The PNG driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    PNGDataset 	*poDS;

    poDS = new PNGDataset();

    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;
    poDS->eAccess = poOpenInfo->eAccess;
    
    poDS->hPNG = png_create_read_struct( PNG_LIBPNG_VER_STRING, poDS, 
                                         NULL, NULL );
    if (poDS->hPNG == NULL)
    {
#if (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 2) || PNG_LIBPNG_VER_MAJOR > 1
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
    png_set_error_fn( poDS->hPNG, &poDS->sSetJmpContext, png_gdal_error, png_gdal_warning );

    if( setjmp( poDS->sSetJmpContext ) != 0 )
    {
        delete poDS;
        return NULL;
    }

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
            poDS->GetRasterBand(1)->SetNoDataValue(nNoDataIndex);
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
            poDS->GetRasterBand(1)->SetNoDataValue(trans_values->gray);
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

            poDS->GetRasterBand(1)->SetNoDataValue(trans_values->red);
            poDS->GetRasterBand(2)->SetNoDataValue(trans_values->green);
            poDS->GetRasterBand(3)->SetNoDataValue(trans_values->blue);
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract any text chunks as "metadata".                          */
/* -------------------------------------------------------------------- */
    poDS->CollectMetadata();

/* -------------------------------------------------------------------- */
/*      More metadata.                                                  */
/* -------------------------------------------------------------------- */
    if( poDS->nBands > 1 )
    {
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML( poOpenInfo->GetSiblingFiles() );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename,
                                 poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                        LoadWorldFile()                               */
/************************************************************************/

void PNGDataset::LoadWorldFile()
{
    if (bHasTriedLoadWorldFile)
        return;
    bHasTriedLoadWorldFile = TRUE;

    char* pszWldFilename = NULL;
    bGeoTransformValid =
        GDALReadWorldFile2( GetDescription(), NULL,
                            adfGeoTransform, oOvManager.GetSiblingFiles(),
                            &pszWldFilename);

    if( !bGeoTransformValid )
        bGeoTransformValid =
            GDALReadWorldFile2( GetDescription(), ".wld",
                                adfGeoTransform, oOvManager.GetSiblingFiles(),
                                &pszWldFilename);

    if (pszWldFilename)
    {
        osWldFilename = pszWldFilename;
        CPLFree(pszWldFilename);
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **PNGDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    LoadWorldFile();

    if (osWldFilename.size() != 0 &&
        CSLFindString(papszFileList, osWldFilename) == -1)
    {
        papszFileList = CSLAddString( papszFileList, osWldFilename );
    }

    return papszFileList;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
PNGDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
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
        && poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt16 )
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported, 
                  "PNG driver doesn't support data type %s. "
                  "Only eight bit (Byte) and sixteen bit (UInt16) bands supported. %s\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()),
                  (bStrict) ? "" : "Defaulting to Byte" );

        if (bStrict)
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
    VSILFILE	*fpImage;

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
    
    jmp_buf     sSetJmpContext;
    hPNG = png_create_write_struct( PNG_LIBPNG_VER_STRING, 
                                    &sSetJmpContext, png_gdal_error, png_gdal_warning );
    psPNGInfo = png_create_info_struct( hPNG );

    if( setjmp( sSetJmpContext ) != 0 )
    {
        VSIFCloseL( fpImage );
        png_destroy_write_struct( &hPNG, &psPNGInfo );
        return NULL;
    }

    png_set_write_fn( hPNG, fpImage, png_vsi_write_data, png_vsi_flush );

    png_set_IHDR( hPNG, psPNGInfo, nXSize, nYSize, 
                  nBitDepth, nColorType, PNG_INTERLACE_NONE, 
                  PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );

/* -------------------------------------------------------------------- */
/*      Do we want to control the compression level?                    */
/* -------------------------------------------------------------------- */
    const char *pszLevel = CSLFetchNameValue( papszOptions, "ZLEVEL" );

    if( pszLevel )
    {
        int nLevel = atoi(pszLevel);
        if( nLevel < 1 || nLevel > 9 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal ZLEVEL value '%s', should be 1-9.",
                      pszLevel );
            return NULL;
        }

        png_set_compression_level( hPNG, nLevel );
    }

/* -------------------------------------------------------------------- */
/*      Try to handle nodata values as a tRNS block (note for           */
/*      paletted images, we save the effect to apply as part of         */
/*      palette).                                                       */
/* -------------------------------------------------------------------- */
    png_color_16 sTRNSColor;

    // Gray nodata.
    if( nColorType == PNG_COLOR_TYPE_GRAY )
    {
       int		bHaveNoData = FALSE;
       double	dfNoDataValue = -1;

       dfNoDataValue = poSrcDS->GetRasterBand(1)->GetNoDataValue( &bHaveNoData );

       if ( bHaveNoData && dfNoDataValue >= 0 && dfNoDataValue < 65536 )
       {
          sTRNSColor.gray = (png_uint_16) dfNoDataValue;
          png_set_tRNS( hPNG, psPNGInfo, NULL, 0, &sTRNSColor );
       }
    }

    // RGB nodata.
    if( nColorType == PNG_COLOR_TYPE_RGB )
    {
       // First try to use the NODATA_VALUES metadata item.
       if ( poSrcDS->GetMetadataItem( "NODATA_VALUES" ) != NULL )
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
       // Otherwise, get the nodata value from the bands.
       else
       {
          int	  bHaveNoDataRed = FALSE;
          int	  bHaveNoDataGreen = FALSE;
          int	  bHaveNoDataBlue = FALSE;
          double dfNoDataValueRed = -1;
          double dfNoDataValueGreen = -1;
          double dfNoDataValueBlue = -1;

          dfNoDataValueRed  = poSrcDS->GetRasterBand(1)->GetNoDataValue( &bHaveNoDataRed );
          dfNoDataValueGreen= poSrcDS->GetRasterBand(2)->GetNoDataValue( &bHaveNoDataGreen );
          dfNoDataValueBlue = poSrcDS->GetRasterBand(3)->GetNoDataValue( &bHaveNoDataBlue );

          if ( ( bHaveNoDataRed && dfNoDataValueRed >= 0 && dfNoDataValueRed < 65536 ) &&
               ( bHaveNoDataGreen && dfNoDataValueGreen >= 0 && dfNoDataValueGreen < 65536 ) &&
               ( bHaveNoDataBlue && dfNoDataValueBlue >= 0 && dfNoDataValueBlue < 65536 ) )
          {
             sTRNSColor.red   = (png_uint_16) dfNoDataValueRed;
             sTRNSColor.green = (png_uint_16) dfNoDataValueGreen;
             sTRNSColor.blue  = (png_uint_16) dfNoDataValueBlue;
             png_set_tRNS( hPNG, psPNGInfo, NULL, 0, &sTRNSColor );
          }
       }
    }
    
/* -------------------------------------------------------------------- */
/*      Copy colour profile data                                        */
/* -------------------------------------------------------------------- */
    const char *pszICCProfile = CSLFetchNameValue(papszOptions, "SOURCE_ICC_PROFILE");
    const char *pszICCProfileName = CSLFetchNameValue(papszOptions, "SOURCE_ICC_PROFILE_NAME");
    if (pszICCProfileName == NULL)
        pszICCProfileName = poSrcDS->GetMetadataItem( "SOURCE_ICC_PROFILE_NAME", "COLOR_PROFILE" );

    if (pszICCProfile == NULL)
        pszICCProfile = poSrcDS->GetMetadataItem( "SOURCE_ICC_PROFILE", "COLOR_PROFILE" );

    if ((pszICCProfileName != NULL) && EQUAL(pszICCProfileName, "sRGB"))
    {
        pszICCProfile = NULL;

        png_set_sRGB(hPNG, psPNGInfo, PNG_sRGB_INTENT_PERCEPTUAL);
    }

    if (pszICCProfile != NULL)
    {
        char *pEmbedBuffer = CPLStrdup(pszICCProfile);
        png_uint_32 nEmbedLen = CPLBase64DecodeInPlace((GByte*)pEmbedBuffer);
        const char* pszLocalICCProfileName = (pszICCProfileName!=NULL)?pszICCProfileName:"ICC Profile";

        png_set_iCCP(hPNG, psPNGInfo,
#if (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR > 4) || PNG_LIBPNG_VER_MAJOR > 1
            pszLocalICCProfileName,
#else
            (png_charp)pszLocalICCProfileName,
#endif
            0,
#if (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR > 4) || PNG_LIBPNG_VER_MAJOR > 1
            (png_const_bytep)pEmbedBuffer,
#else
            (png_charp)pEmbedBuffer,
#endif
            nEmbedLen);

        CPLFree(pEmbedBuffer);
    }
    else if ((pszICCProfileName == NULL) || !EQUAL(pszICCProfileName, "sRGB"))
    {
        // Output gamma, primaries and whitepoint
        const char *pszGamma = CSLFetchNameValue(papszOptions, "PNG_GAMMA");
        if (pszGamma == NULL)
            pszGamma = poSrcDS->GetMetadataItem( "PNG_GAMMA", "COLOR_PROFILE" );

        if (pszGamma != NULL)
        {
            double dfGamma = CPLAtof(pszGamma);
            png_set_gAMA(hPNG, psPNGInfo, dfGamma);
        }

        const char *pszPrimariesRed = CSLFetchNameValue(papszOptions, "SOURCE_PRIMARIES_RED");
        if (pszPrimariesRed == NULL)
            pszPrimariesRed = poSrcDS->GetMetadataItem( "SOURCE_PRIMARIES_RED", "COLOR_PROFILE" );
        const char *pszPrimariesGreen = CSLFetchNameValue(papszOptions, "SOURCE_PRIMARIES_GREEN");
        if (pszPrimariesGreen == NULL)
            pszPrimariesGreen = poSrcDS->GetMetadataItem( "SOURCE_PRIMARIES_GREEN", "COLOR_PROFILE" );
        const char *pszPrimariesBlue = CSLFetchNameValue(papszOptions, "SOURCE_PRIMARIES_BLUE");
        if (pszPrimariesBlue == NULL)
            pszPrimariesBlue = poSrcDS->GetMetadataItem( "SOURCE_PRIMARIES_BLUE", "COLOR_PROFILE" );
        const char *pszWhitepoint = CSLFetchNameValue(papszOptions, "SOURCE_WHITEPOINT");
        if (pszWhitepoint == NULL)
            pszWhitepoint = poSrcDS->GetMetadataItem( "SOURCE_WHITEPOINT", "COLOR_PROFILE" );

        if ((pszPrimariesRed != NULL) && (pszPrimariesGreen != NULL) && (pszPrimariesBlue != NULL) &&
            (pszWhitepoint != NULL))
        {
            bool bOk = true;
            double faColour[8];
            char** apapszTokenList[4];

            apapszTokenList[0] = CSLTokenizeString2( pszWhitepoint, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
            apapszTokenList[1] = CSLTokenizeString2( pszPrimariesRed, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
            apapszTokenList[2] = CSLTokenizeString2( pszPrimariesGreen, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
            apapszTokenList[3] = CSLTokenizeString2( pszPrimariesBlue, ",", 
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );

            if ((CSLCount( apapszTokenList[0] ) == 3) &&
                (CSLCount( apapszTokenList[1] ) == 3) &&
                (CSLCount( apapszTokenList[2] ) == 3) &&
                (CSLCount( apapszTokenList[3] ) == 3))
            {
                for( int i = 0; i < 4; i++ )
                {
                    for( int j = 0; j < 3; j++ )
                    {
                        double v = CPLAtof(apapszTokenList[i][j]);

                        if (j == 2)
                        {
                            /* Last term of xyY colour must be 1.0 */
                            if (v != 1.0)
                            {
                                bOk = false;
                                break;
                            }
                        }
                        else
                        {
                            faColour[i*2 + j] = v;
                        }
                    }
                    if (!bOk)
                        break;
                }

                if (bOk)
                {
                    png_set_cHRM(hPNG, psPNGInfo, 
                        faColour[0], faColour[1], 
                        faColour[2], faColour[3], 
                        faColour[4], faColour[5], 
                        faColour[6], faColour[7]);

                }
            }

            CSLDestroy( apapszTokenList[0] );
            CSLDestroy( apapszTokenList[1] );
            CSLDestroy( apapszTokenList[2] );
            CSLDestroy( apapszTokenList[3] );
        }

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
        int		bHaveNoData = FALSE;
        double	dfNoDataValue = -1;

        dfNoDataValue  = poSrcDS->GetRasterBand(1)->GetNoDataValue( &bHaveNoData );
        
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
                                     nBands * nXSize * nWordSize, NULL );
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
	
	if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
            GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */

    /* If outputing to stdout, we can't reopen it, so we'll return */
    /* a fake dataset to make the caller happy */
    if( CSLTestBoolean(CPLGetConfigOption("GDAL_OPEN_AFTER_COPY", "YES")) )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
        PNGDataset *poDS = (PNGDataset*) PNGDataset::Open( &oOpenInfo );
        CPLPopErrorHandler();
        if( poDS )
        {
            poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
            return poDS;
        }
        CPLErrorReset();
    }

    PNGDataset* poPNG_DS = new PNGDataset();
    poPNG_DS->nRasterXSize = nXSize;
    poPNG_DS->nRasterYSize = nYSize;
    poPNG_DS->nBitDepth = nBitDepth;
    for(int i=0;i<nBands;i++)
        poPNG_DS->SetBand( i+1, new PNGRasterBand( poPNG_DS, i+1) );
    return poPNG_DS;
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
                                 (VSILFILE*)png_get_io_ptr(png_ptr));

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

   check = VSIFWriteL(data, 1, length, (VSILFILE*)png_get_io_ptr(png_ptr));

   if (check != length)
      png_error(png_ptr, "Write Error");
}

/************************************************************************/
/*                           png_vsi_flush()                            */
/************************************************************************/
static void png_vsi_flush(png_structp png_ptr)
{
    VSIFFlushL( (VSILFILE*)png_get_io_ptr(png_ptr) );
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

    jmp_buf* psSetJmpContext = (jmp_buf*) png_get_error_ptr(png_ptr);
    if (psSetJmpContext)
    {
        longjmp( *psSetJmpContext, 1 );
    }
}

/************************************************************************/
/*                          png_gdal_warning()                          */
/************************************************************************/

static void png_gdal_warning( CPL_UNUSED png_structp png_ptr, const char *error_message )
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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
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
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='6'/>\n"
"   <Option name='SOURCE_ICC_PROFILE' type='string' description='ICC Profile'/>\n"
"   <Option name='SOURCE_ICC_PROFILE_NAME' type='string' descriptor='ICC Profile name'/>\n"
"   <Option name='SOURCE_PRIMARIES_RED' type='string' description='x,y,1.0 (xyY) red chromaticity'/>\n"
"   <Option name='SOURCE_PRIMARIES_GREEN' type='string' description='x,y,1.0 (xyY) green chromaticity'/>\n"
"   <Option name='SOURCE_PRIMARIES_BLUE' type='string' description='x,y,1.0 (xyY) blue chromaticity'/>\n"
"   <Option name='SOURCE_WHITEPOINT' type='string' description='x,y,1.0 (xyY) whitepoint'/>\n"
"   <Option name='PNG_GAMMA' type='string' description='Gamma'/>\n"
"</CreationOptionList>\n" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = PNGDataset::Open;
        poDriver->pfnCreateCopy = PNGDataset::CreateCopy;
        poDriver->pfnIdentify = PNGDataset::Identify;
#ifdef SUPPORT_CREATE
        poDriver->pfnCreate = PNGDataset::Create;
#endif

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

#ifdef SUPPORT_CREATE
/************************************************************************/
/*                         IWriteBlock()                                */
/************************************************************************/

CPLErr PNGRasterBand::IWriteBlock(int x, int y, void* pvData)
{
    // rcg, added to support Create().

    PNGDataset& ds = *(PNGDataset*)poDS;


    // Write the block (or consolidate into multichannel block)
    // and then write.

    const GDALDataType dt = this->GetRasterDataType();
    const size_t wordsize = ds.m_nBitDepth / 8;
    GDALCopyWords( pvData, dt, wordsize,
                   ds.m_pabyBuffer + (nBand-1) * wordsize,
                   dt, ds.nBands * wordsize,
                   nBlockXSize );

    // See if we got all the bands.
    size_t i;
    m_bBandProvided[nBand - 1] = TRUE;
    for(i = 0; i < ds.nBands; i++)
    {
        if(!m_bBandProvided[i])
            return CE_None;
    }

    // We received all the bands, so
    // reset band flags and write pixels out.
    this->reset_band_provision_flags();


    // If first block, write out file header.
    if(x == 0 && y == 0)
    {
        CPLErr err = ds.write_png_header();
        if(err != CE_None)
            return err;
    }

#ifdef CPL_LSB
    if( ds.m_nBitDepth == 16 )
        GDALSwapWords( ds.m_pabyBuffer, 2, nBlockXSize * ds.nBands, 2 );
#endif
    png_write_rows( ds.m_hPNG, &ds.m_pabyBuffer, 1 );

    return CE_None;
}


/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PNGDataset::SetGeoTransform( double * padfTransform )
{
    // rcg, added to support Create().

    CPLErr eErr = CE_None;

    memcpy( m_adfGeoTransform, padfTransform, sizeof(double) * 6 );

    if ( m_pszFilename )
    {
        if ( GDALWriteWorldFile( m_pszFilename, "wld", m_adfGeoTransform )
             == FALSE )
        {
            CPLError( CE_Failure, CPLE_FileIO, "Can't write world file." );
            eErr = CE_Failure;
        }
    }

    return eErr;
}


/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr PNGRasterBand::SetColorTable(GDALColorTable* poCT)
{
    if( poCT == NULL )
        return CE_Failure;

    // rcg, added to support Create().
    // We get called even for grayscale files, since some 
    // formats need a palette even then. PNG doesn't, so
    // if a gray palette is given, just ignore it.

    GDALColorEntry sEntry;
    for( size_t i = 0; i < poCT->GetColorEntryCount(); i++ )
    {
        poCT->GetColorEntryAsRGB( i, &sEntry );
        if( sEntry.c1 != sEntry.c2 || sEntry.c1 != sEntry.c3)
        {
            CPLErr err = GDALPamRasterBand::SetColorTable(poCT);
            if(err != CE_None)
                return err;

            PNGDataset& ds = *(PNGDataset*)poDS;
            ds.m_nColorType = PNG_COLOR_TYPE_PALETTE;
            break;
            // band::IWriteBlock will emit color table as part of 
            // header preceding first block write.
        }
    }

    return CE_None;
}


/************************************************************************/
/*                  PNGDataset::write_png_header()                      */
/************************************************************************/

CPLErr PNGDataset::write_png_header()
{
/* -------------------------------------------------------------------- */
/*      Initialize PNG access to the file.                              */
/* -------------------------------------------------------------------- */
    
    m_hPNG = png_create_write_struct( 
        PNG_LIBPNG_VER_STRING, NULL, 
        png_gdal_error, png_gdal_warning );

    m_psPNGInfo = png_create_info_struct( m_hPNG );

    png_set_write_fn( m_hPNG, m_fpImage, png_vsi_write_data, png_vsi_flush );

    png_set_IHDR( m_hPNG, m_psPNGInfo, nRasterXSize, nRasterYSize, 
                  m_nBitDepth, m_nColorType, PNG_INTERLACE_NONE, 
                  PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );

    png_set_compression_level(m_hPNG, Z_BEST_COMPRESSION);

    //png_set_swap_alpha(m_hPNG); // Use RGBA order, not ARGB.

/* -------------------------------------------------------------------- */
/*      Try to handle nodata values as a tRNS block (note for           */
/*      paletted images, we save the effect to apply as part of         */
/*      palette).                                                       */
/* -------------------------------------------------------------------- */
    //m_bHaveNoData = FALSE;
    //m_dfNoDataValue = -1;
    png_color_16 sTRNSColor;


    int		bHaveNoData = FALSE;
    double	dfNoDataValue = -1;

    if( m_nColorType == PNG_COLOR_TYPE_GRAY )
    {
        dfNoDataValue = this->GetRasterBand(1)->GetNoDataValue( &bHaveNoData );

        if ( bHaveNoData && dfNoDataValue >= 0 && dfNoDataValue < 65536 )
        {
            sTRNSColor.gray = (png_uint_16) dfNoDataValue;
            png_set_tRNS( m_hPNG, m_psPNGInfo, NULL, 0, &sTRNSColor );
        }
    }

    // RGB nodata.
    if( nColorType == PNG_COLOR_TYPE_RGB )
    {
        // First try to use the NODATA_VALUES metadata item.
        if ( this->GetMetadataItem( "NODATA_VALUES" ) != NULL )
        {
            char **papszValues = CSLTokenizeString(
                this->GetMetadataItem( "NODATA_VALUES" ) );
           
            if( CSLCount(papszValues) >= 3 )
            {
                sTRNSColor.red   = (png_uint_16) atoi(papszValues[0]);
                sTRNSColor.green = (png_uint_16) atoi(papszValues[1]);
                sTRNSColor.blue  = (png_uint_16) atoi(papszValues[2]);
                png_set_tRNS( m_hPNG, m_psPNGInfo, NULL, 0, &sTRNSColor );
            }

            CSLDestroy( papszValues );
        }
        // Otherwise, get the nodata value from the bands.
        else
        {
            int	  bHaveNoDataRed = FALSE;
            int	  bHaveNoDataGreen = FALSE;
            int	  bHaveNoDataBlue = FALSE;
            double dfNoDataValueRed = -1;
            double dfNoDataValueGreen = -1;
            double dfNoDataValueBlue = -1;

            dfNoDataValueRed  = this->GetRasterBand(1)->GetNoDataValue( &bHaveNoDataRed );
            dfNoDataValueGreen= this->GetRasterBand(2)->GetNoDataValue( &bHaveNoDataGreen );
            dfNoDataValueBlue = this->GetRasterBand(3)->GetNoDataValue( &bHaveNoDataBlue );

            if ( ( bHaveNoDataRed && dfNoDataValueRed >= 0 && dfNoDataValueRed < 65536 ) &&
                 ( bHaveNoDataGreen && dfNoDataValueGreen >= 0 && dfNoDataValueGreen < 65536 ) &&
                 ( bHaveNoDataBlue && dfNoDataValueBlue >= 0 && dfNoDataValueBlue < 65536 ) )
            {
                sTRNSColor.red   = (png_uint_16) dfNoDataValueRed;
                sTRNSColor.green = (png_uint_16) dfNoDataValueGreen;
                sTRNSColor.blue  = (png_uint_16) dfNoDataValueBlue;
                png_set_tRNS( m_hPNG, m_psPNGInfo, NULL, 0, &sTRNSColor );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Write palette if there is one.  Technically, I think it is      */
/*      possible to write 16bit palettes for PNG, but we will omit      */
/*      this for now.                                                   */
/* -------------------------------------------------------------------- */

    if( nColorType == PNG_COLOR_TYPE_PALETTE )
    {
        GDALColorTable	*poCT;
        GDALColorEntry  sEntry;
        int		iColor, bFoundTrans = FALSE;
        int		bHaveNoData = FALSE;
        double	dfNoDataValue = -1;

        dfNoDataValue  = this->GetRasterBand(1)->GetNoDataValue( &bHaveNoData );
        
        poCT = this->GetRasterBand(1)->GetColorTable();

        m_pasPNGColors = (png_color *) CPLMalloc(sizeof(png_color) *
                                                 poCT->GetColorEntryCount());

        for( iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
        {
            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            if( sEntry.c4 != 255 )
                bFoundTrans = TRUE;

            m_pasPNGColors[iColor].red = (png_byte) sEntry.c1;
            m_pasPNGColors[iColor].green = (png_byte) sEntry.c2;
            m_pasPNGColors[iColor].blue = (png_byte) sEntry.c3;
        }
        
        png_set_PLTE( m_hPNG, m_psPNGInfo, m_pasPNGColors, 
                      poCT->GetColorEntryCount() );

/* -------------------------------------------------------------------- */
/*      If we have transparent elements in the palette we need to       */
/*      write a transparency block.                                     */
/* -------------------------------------------------------------------- */
        if( bFoundTrans || bHaveNoData )
        {
            m_pabyAlpha = (unsigned char *)CPLMalloc(poCT->GetColorEntryCount());

            for( iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
            {
                poCT->GetColorEntryAsRGB( iColor, &sEntry );
                m_pabyAlpha[iColor] = (unsigned char) sEntry.c4;

                if( bHaveNoData && iColor == (int) dfNoDataValue )
                    m_pabyAlpha[iColor] = 0;
            }

            png_set_tRNS( m_hPNG, m_psPNGInfo, m_pabyAlpha, 
                          poCT->GetColorEntryCount(), NULL );
        }
    }

    png_write_info( m_hPNG, m_psPNGInfo );
    return CE_None;
}


/************************************************************************/
/*                               Create()                               */
/************************************************************************/

// static
GDALDataset *PNGDataset::Create
(
	const char* pszFilename,
    int nXSize, int nYSize, 
	int nBands,
    GDALDataType eType, 
	char **papszOptions 
)
{
    if( eType != GDT_Byte && eType != GDT_UInt16)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create PNG dataset with an illegal\n"
                  "data type (%s), only Byte and UInt16 supported by the format.\n",
                  GDALGetDataTypeName(eType) );

        return NULL;
    }

    if( nBands < 1 || nBands > 4 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "PNG driver doesn't support %d bands. "
                  "Must be 1 (gray/indexed color),\n"
                  "2 (gray+alpha), 3 (rgb) or 4 (rgba) bands.\n", 
                  nBands );

        return NULL;
    }


    // Bands are:
    // 1: grayscale or indexed color
    // 2: gray plus alpha
    // 3: rgb
    // 4: rgb plus alpha

    if(nXSize < 1 || nYSize < 1)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Specified pixel dimensions (% d x %d) are bad.\n",
                  nXSize, nYSize );
    }

/* -------------------------------------------------------------------- */
/*      Setup some parameters.                                          */
/* -------------------------------------------------------------------- */

    PNGDataset* poDS = new PNGDataset();

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->nBands = nBands;


    switch(nBands)
    {
      case 1:
        poDS->m_nColorType = PNG_COLOR_TYPE_GRAY;
        break;// if a non-gray palette is set, we'll change this.

      case 2:
        poDS->m_nColorType = PNG_COLOR_TYPE_GRAY_ALPHA;
        break;

      case 3:
        poDS->m_nColorType = PNG_COLOR_TYPE_RGB;
        break;
	
      case 4:
        poDS->m_nColorType = PNG_COLOR_TYPE_RGB_ALPHA;
        break;
    }

    poDS->m_nBitDepth = (eType == GDT_Byte ? 8 : 16);

    poDS->m_pabyBuffer = (GByte *) CPLMalloc(
        nBands * nXSize * poDS->m_nBitDepth / 8 );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int iBand;

    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
        poDS->SetBand( iBand, new PNGRasterBand( poDS, iBand ) );

/* -------------------------------------------------------------------- */
/*      Do we need a world file?                                        */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "WORLDFILE", FALSE ) )
        poDS->m_bGeoTransformValid = TRUE;

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */

    poDS->m_fpImage = VSIFOpenL( pszFilename, "wb" );
    if( poDS->m_fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create PNG file %s.\n", 
                  pszFilename );
        delete poDS;
        return NULL;
    }

    poDS->m_pszFilename = CPLStrdup(pszFilename);

    return poDS;
}

#endif
