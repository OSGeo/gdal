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
 * Revision 1.4  2001/01/10 15:36:44  warmerda
 * implement interlacing support
 *
 * Revision 1.3  2001/01/10 05:32:17  warmerda
 * Added GIFCreateCopy() implementation.
 *
 * Revision 1.2  2001/01/10 05:00:32  warmerda
 * Only check against GIF87/GIF89, not GIF87a/GIF89a.
 *
 * Revision 1.1  2001/01/10 04:40:15  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "cpl_string.h"

CPL_C_START
#include "gif_lib.h"
CPL_C_END

static GDALDriver	*poGIFDriver = NULL;

CPL_C_START
void	GDALRegister_GIF(void);
CPL_C_END

static int InterlacedOffset[] = { 0, 4, 2, 1 }; 
static int InterlacedJumps[] = { 8, 8, 4, 2 };  

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

    int		*panInterlaceMap;
    
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

/* -------------------------------------------------------------------- */
/*      Setup colormap.                                                 */
/* -------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------- */
/*      Setup interlacing map if required.                              */
/* -------------------------------------------------------------------- */
    panInterlaceMap = NULL;
    if( psImage->ImageDesc.Interlace )
    {
        int	i, j, iLine = 0;

        panInterlaceMap = (int *) CPLCalloc(poDS->nRasterYSize,sizeof(int));

	for (i = 0; i < 4; i++)
        {
	    for (j = InterlacedOffset[i]; 
                 j < poDS->nRasterYSize;
                 j += InterlacedJumps[i]) 
                panInterlaceMap[j] = iLine++;
        }
    }
}

/************************************************************************/
/*                           ~GIFRasterBand()                           */
/************************************************************************/

GIFRasterBand::~GIFRasterBand()

{
    if( poColorTable != NULL )
        delete poColorTable;

    CPLFree( panInterlaceMap );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GIFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    CPLAssert( nBlockXOff == 0 );

    if( panInterlaceMap != NULL )
        nBlockYOff = panInterlaceMap[nBlockYOff];

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
/*      Open the file and ingest.                                       */
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

        if( psImage->ImageDesc.Width != poDS->nRasterXSize
            || psImage->ImageDesc.Height != poDS->nRasterYSize )
            continue;

        poDS->SetBand( poDS->nBands+1, 
                       new GIFRasterBand( poDS, poDS->nBands+1, psImage ));
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
    int	 bInterlace = FALSE;

/* -------------------------------------------------------------------- */
/*      Check for interlaced option.                                    */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszOptions, "INTERLACING" ) != NULL )
        bInterlace = TRUE;

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "GIF driver only supports one band images.\n" );

        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte 
        && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "GIF driver doesn't support data type %s. "
                  "Only eight bit bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the output file.                                           */
/* -------------------------------------------------------------------- */
    GifFileType *hGifFile;

    hGifFile = EGifOpenFileName( (char *) pszFilename, TRUE );
    if( hGifFile == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "EGifOpenFilename(%s) failed.  Does file already exist?",
                  pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Prepare colortable.                                             */
/* -------------------------------------------------------------------- */
    GDALRasterBand	*poBand = poSrcDS->GetRasterBand(1);
    ColorMapObject	*psGifCT;
    int			iColor;

    if( poBand->GetColorTable() == NULL )
    {
        psGifCT = MakeMapObject( 256, NULL );
        for( iColor = 0; iColor < 256; iColor++ )
        {
            psGifCT->Colors[iColor].Red = iColor;
            psGifCT->Colors[iColor].Green = iColor;
            psGifCT->Colors[iColor].Blue = iColor;
        }
    }
    else
    {
        GDALColorTable	*poCT = poBand->GetColorTable();

        psGifCT = MakeMapObject( poCT->GetColorEntryCount(), NULL );
        for( iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
        {
            GDALColorEntry	sEntry;

            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            psGifCT->Colors[iColor].Red = sEntry.c1;
            psGifCT->Colors[iColor].Green = sEntry.c2;
            psGifCT->Colors[iColor].Blue = sEntry.c3;
        }
    }

/* -------------------------------------------------------------------- */
/*      Setup parameters.                                               */
/* -------------------------------------------------------------------- */
    if (EGifPutScreenDesc(hGifFile, nXSize, nYSize, 
                          psGifCT->ColorCount, 0,
			  psGifCT) == GIF_ERROR ||
	EGifPutImageDesc(hGifFile,
			 0, 0, nXSize, nYSize, bInterlace, NULL) == GIF_ERROR )
    {
        PrintGifError();
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Error writing gif file." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GByte 	*pabyScanline;
    CPLErr      eErr;

    pabyScanline = (GByte *) CPLMalloc( nXSize );

    if( !bInterlace )
    {
        for( int iLine = 0; iLine < nYSize; iLine++ )
        {
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                     pabyScanline, nXSize, 1, GDT_Byte,
                                     nBands, nBands * nXSize );
            
            if( EGifPutLine( hGifFile, pabyScanline, nXSize ) == GIF_ERROR )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Error writing gif file." );
                return NULL;
            }
        }
    }
    else
    {
        int 	i, j;

	/* Need to perform 4 passes on the images: */
	for ( i = 0; i < 4; i++)
        {
	    for (j = InterlacedOffset[i]; j < nYSize; j += InterlacedJumps[i]) 
            {
                poBand->RasterIO( GF_Read, 0, j, nXSize, 1, 
                                  pabyScanline, nXSize, 1, GDT_Byte,
                                  1, nXSize );
            
		if (EGifPutLine(hGifFile, pabyScanline, nXSize)
		    == GIF_ERROR) return GIF_ERROR;
	    }
        }
    }

    CPLFree( pabyScanline );

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    if (EGifCloseFile(hGifFile) == GIF_ERROR)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "EGifCloseFile() failed.\n" );
        return NULL;
    }

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

