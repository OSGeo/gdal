/******************************************************************************
 * $Id$
 *
 * Project:  GIF Driver
 * Purpose:  Implement GDAL GIF Support using libungif code.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 * Revision 1.2  2001/01/10 05:00:32  warmerda
 * Only check against GIF87/GIF89, not GIF87a/GIF89a.
 *
 * Revision 1.1  2001/01/10 04:40:15  warmerda
 * New
 *
 */

#include "gdal_priv.h"

CPL_C_START
#include "gif_lib.h"
CPL_C_END

static GDALDriver	*poGIFDriver = NULL;

CPL_C_START
void	GDALRegister_GIF(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				GIFDataset				*/
/* ==================================================================== */
/************************************************************************/

class GIFRasterBand;

class GIFDataset : public GDALDataset
{
    friend	GIFRasterBand;

    GifFileType *hGifFile;

  public:
                 GIFDataset();
                 ~GIFDataset();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            GIFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GIFRasterBand : public GDALRasterBand
{
    friend	GIFDataset;

    SavedImage	*psImage;
    
    GDALColorTable *poColorTable;

  public:

                   GIFRasterBand( GIFDataset *, int, SavedImage * );
    virtual       ~GIFRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};

/************************************************************************/
/*                           GIFRasterBand()                            */
/************************************************************************/

GIFRasterBand::GIFRasterBand( GIFDataset *poDS, int nBand, 
                              SavedImage *psSavedImage )

{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;

    psImage = psSavedImage;

    ColorMapObject 	*psGifCT = psImage->ImageDesc.ColorMap;
    if( psGifCT == NULL )
        psGifCT = poDS->hGifFile->SColorMap;

    poColorTable = new GDALColorTable();
    for( int iColor = 0; iColor < psGifCT->ColorCount; iColor++ )
    {
        GDALColorEntry	oEntry;
        
        oEntry.c1 = psGifCT->Colors[iColor].Red;
        oEntry.c2 = psGifCT->Colors[iColor].Green;
        oEntry.c3 = psGifCT->Colors[iColor].Blue;
        oEntry.c4 = 255;
        poColorTable->SetColorEntry( iColor, &oEntry );
    }
}

/************************************************************************/
/*                           ~GIFRasterBand()                           */
/************************************************************************/

GIFRasterBand::~GIFRasterBand()

{
    if( poColorTable != NULL )
        delete poColorTable;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GIFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    CPLAssert( nBlockXOff == 0 );

    memcpy( pImage, psImage->RasterBits + nBlockYOff * nBlockXSize, 
            nBlockXSize );

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GIFRasterBand::GetColorInterpretation()

{
    return GCI_PaletteIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GIFRasterBand::GetColorTable()

{
    return poColorTable;
}

/************************************************************************/
/* ==================================================================== */
/*                             GIFDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            GIFDataset()                            */
/************************************************************************/

GIFDataset::GIFDataset()

{
    hGifFile = NULL;
}

/************************************************************************/
/*                           ~GIFDataset()                            */
/************************************************************************/

GIFDataset::~GIFDataset()

{
    DGifCloseFile( hGifFile );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GIFDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 8 )
        return NULL;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader, "GIF87a",5)
        && !EQUALN((const char *) poOpenInfo->pabyHeader, "GIF89a",5) )
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The GIF driver does not support update access to existing"
                  " files.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    GifFileType 	*hGifFile;

    hGifFile = DGifOpenFileName( poOpenInfo->pszFilename );
    if( hGifFile == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "DGifOpenFileName() failed for %s.\n"
                  "Perhaps the gif file is corrupt?\n",
                  poOpenInfo->pszFilename );

        return NULL;
    }

    if( DGifSlurp( hGifFile ) != GIF_OK )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "DGifSlurp() failed for %s.\n"
                  "Perhaps the gif file is corrupt?\n",
                  poOpenInfo->pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GIFDataset 	*poDS;

    poDS = new GIFDataset();

    poDS->eAccess = GA_ReadOnly;
    poDS->hGifFile = hGifFile;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = hGifFile->SWidth;
    poDS->nRasterYSize = hGifFile->SHeight;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iImage = 0; iImage < hGifFile->ImageCount; iImage++ )
    {
        SavedImage	*psImage = hGifFile->SavedImages + iImage;

        if( psImage->ImageDesc.Width == poDS->nRasterXSize
            && psImage->ImageDesc.Height == poDS->nRasterYSize )
        {
            poDS->SetBand( poDS->nBands+1, 
                           new GIFRasterBand( poDS, poDS->nBands+1, psImage ));
        }
    }

    return poDS;
}

/************************************************************************/
/*                           GIFCreateCopy()                            */
/************************************************************************/

static GDALDataset *
GIFCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
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
                  "GIF driver doesn't support %d bands.  Must be 1 (grey),\n"
                  "2 (grey+alpha), 3 (rgb) or 4 (rgba) bands.\n", 
                  nBands );

        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte 
        && poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt16
        && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "GIF driver doesn't support data type %s. "
                  "Only eight and sixteen bit bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup some parameters.                                          */
/* -------------------------------------------------------------------- */
    int  nColorType=0, nBitDepth;
    GDALDataType eType;

    if( nBands == 1 )
        nColorType = GIF_COLOR_TYPE_GRAY;
    else if( nBands == 2 )
        nColorType = GIF_COLOR_TYPE_GRAY_ALPHA;
    else if( nBands == 3 )
        nColorType = GIF_COLOR_TYPE_RGB;
    else if( nBands == 4 )
        nColorType = GIF_COLOR_TYPE_RGB_ALPHA;

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

    fpImage = VSIFOpen( pszFilename, "wb" );
    if( fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create gif file %s.\n", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize GIF access to the file.                              */
/* -------------------------------------------------------------------- */
    gif_structp hGIF;
    gif_infop   psGIFInfo;
    
    hGIF = gif_create_write_struct( GIF_LIBGIF_VER_STRING, 
                                    NULL, NULL, NULL );
    psGIFInfo = gif_create_info_struct( hGIF );
    
    gif_init_io( hGIF, fpImage );

    gif_set_IHDR( hGIF, psGIFInfo, nXSize, nYSize, 
                  nBitDepth, nColorType, GIF_INTERLACE_NONE, 
                  GIF_COMPRESSION_TYPE_BASE, GIF_FILTER_TYPE_BASE );
    
    gif_write_info( hGIF, psGIFInfo );

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GByte 	*pabyScanline;
    CPLErr      eErr;

    pabyScanline = (GByte *) CPLMalloc( nBands * nXSize * 2 );

    for( int iLine = 0; iLine < nYSize; iLine++ )
    {
        gif_bytep       row = pabyScanline;

        for( int iBand = 0; iBand < nBands; iBand++ )
        {
            GDALRasterBand * poBand = poSrcDS->GetRasterBand( iBand+1 );
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                     pabyScanline + iBand, nXSize, 1, GDT_Byte,
                                     nBands, nBands * nXSize );
        }

        gif_write_rows( hGIF, &row, 1 );
    }

    CPLFree( pabyScanline );

    gif_write_end( hGIF, psGIFInfo );
    gif_destroy_write_struct( &hGIF, &psGIFInfo );

    VSIFClose( fpImage );

    return (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
}

/************************************************************************/
/*                          GDALRegister_GIF()                        */
/************************************************************************/

void GDALRegister_GIF()

{
    GDALDriver	*poDriver;

    if( poGIFDriver == NULL )
    {
        poGIFDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "GIF";
        poDriver->pszLongName = "Graphics Interchange Format (.gif)";
        poDriver->pszHelpTopic = "frmt_various.html#GIF";
        
        poDriver->pfnOpen = GIFDataset::Open;
        poDriver->pfnCreateCopy = GIFCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

