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
 ****************************************************************************/

#include "gdal_pam.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
#include "gif_lib.h"
CPL_C_END

CPL_C_START
void	GDALRegister_GIF(void);

// This prototype seems to have been messed up!
GifFileType * EGifOpen(void* userData, OutputFunc writeFunc);
CPL_C_END

static int InterlacedOffset[] = { 0, 4, 2, 1 }; 
static int InterlacedJumps[] = { 8, 8, 4, 2 };  

static int VSIGIFReadFunc( GifFileType *, GifByteType *, int);
static int VSIGIFWriteFunc( GifFileType *, const GifByteType *, int );

/************************************************************************/
/* ==================================================================== */
/*				GIFDataset				*/
/* ==================================================================== */
/************************************************************************/

class GIFRasterBand;

class GIFDataset : public GDALPamDataset
{
    friend class GIFRasterBand;

    FILE *fp;

    GifFileType *hGifFile;

    int	   bGeoTransformValid;
    double adfGeoTransform[6];

  public:
                 GIFDataset();
                 ~GIFDataset();

    virtual CPLErr GetGeoTransform( double * );
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            GIFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GIFRasterBand : public GDALPamRasterBand
{
    friend class GIFDataset;

    SavedImage	*psImage;

    int		*panInterlaceMap;
    
    GDALColorTable *poColorTable;

    int		nTransparentColor;

  public:

                   GIFRasterBand( GIFDataset *, int, SavedImage *, int );
    virtual       ~GIFRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );

    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};

/************************************************************************/
/*                           GIFRasterBand()                            */
/************************************************************************/

GIFRasterBand::GIFRasterBand( GIFDataset *poDS, int nBand, 
                              SavedImage *psSavedImage, int nBackground )

{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;

    psImage = psSavedImage;

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

/* -------------------------------------------------------------------- */
/*      Check for transparency.  We just take the first graphic         */
/*      control extension block we find, if any.                        */
/* -------------------------------------------------------------------- */
    int	iExtBlock;

    nTransparentColor = -1;
    for( iExtBlock = 0; iExtBlock < psImage->ExtensionBlockCount; iExtBlock++ )
    {
        unsigned char *pExtData;

        if( psImage->ExtensionBlocks[iExtBlock].Function != 0xf9 )
            continue;

        pExtData = (unsigned char *) psImage->ExtensionBlocks[iExtBlock].Bytes;

        /* check if transparent color flag is set */
        if( !(pExtData[0] & 0x1) )
            continue;

        nTransparentColor = pExtData[3];
    }

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

        if( iColor == nTransparentColor )
            oEntry.c4 = 0;
        else
            oEntry.c4 = 255;

        poColorTable->SetColorEntry( iColor, &oEntry );
    }

/* -------------------------------------------------------------------- */
/*      If we have a background value, return it here.  Some            */
/*      applications might want to treat this as transparent, but in    */
/*      many uses this is inappropriate so we don't return it as        */
/*      nodata or transparent.                                          */
/* -------------------------------------------------------------------- */
    if( nBackground != 255 )
    {
        char szBackground[10];
        
        sprintf( szBackground, "%d", nBackground );
        SetMetadataItem( "GIF_BACKGROUND", szBackground );
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
/*                           GetNoDataValue()                           */
/************************************************************************/

double GIFRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = nTransparentColor != -1;

    return nTransparentColor;
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
    fp = NULL;
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~GIFDataset()                            */
/************************************************************************/

GIFDataset::~GIFDataset()

{
    FlushCache();
    if( hGifFile )
        DGifCloseFile( hGifFile );
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GIFDataset::GetGeoTransform( double * padfTransform )

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
    FILE                *fp;

    fp = VSIFOpenL( poOpenInfo->pszFilename, "r" );
    if( fp == NULL )
        return NULL;

    hGifFile = DGifOpen( fp, VSIGIFReadFunc );
    if( hGifFile == NULL )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "DGifOpen() failed for %s.\n"
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

    poDS->fp = fp;
    poDS->eAccess = GA_ReadOnly;
    poDS->hGifFile = hGifFile;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = hGifFile->SavedImages[0].ImageDesc.Width;
    poDS->nRasterYSize = hGifFile->SavedImages[0].ImageDesc.Height;

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
                       new GIFRasterBand( poDS, poDS->nBands+1, psImage,
                                          hGifFile->SBackGroundColor ));
    }

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    poDS->bGeoTransformValid = 
        GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                           poDS->adfGeoTransform )
        || GDALReadWorldFile( poOpenInfo->pszFilename, ".gfw", 
                              poDS->adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

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
    FILE *fp;

    fp = VSIFOpenL( pszFilename, "w" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create %s:\n%s", 
                  pszFilename, VSIStrerror( errno ) );
        return NULL;
    }

    hGifFile = EGifOpen( fp, VSIGIFWriteFunc );
    if( hGifFile == NULL )
    {
        VSIFCloseL( fp );
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
            psGifCT->Colors[iColor].Red = (GifByteType) iColor;
            psGifCT->Colors[iColor].Green = (GifByteType) iColor;
            psGifCT->Colors[iColor].Blue = (GifByteType) iColor;
        }
    }
    else
    {
        GDALColorTable	*poCT = poBand->GetColorTable();
        int nFullCount = 1;

        while( nFullCount < poCT->GetColorEntryCount() )
            nFullCount = nFullCount * 2;

        psGifCT = MakeMapObject( nFullCount, NULL );
        for( iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
        {
            GDALColorEntry	sEntry;

            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            psGifCT->Colors[iColor].Red = (GifByteType) sEntry.c1;
            psGifCT->Colors[iColor].Green = (GifByteType) sEntry.c2;
            psGifCT->Colors[iColor].Blue = (GifByteType) sEntry.c3;
        }
        for( ; iColor < nFullCount; iColor++ )
        {
            psGifCT->Colors[iColor].Red = 0;
            psGifCT->Colors[iColor].Green = 0;
            psGifCT->Colors[iColor].Blue = 0;
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
    
    VSIFCloseL( fp );

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
    GDALPamDataset *poDS = (GDALPamDataset *) 
        GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                           VSIGIFReadFunc()                           */
/*                                                                      */
/*      Proxy function for reading from GIF file.                       */
/************************************************************************/

static int VSIGIFReadFunc( GifFileType *psGFile, GifByteType *pabyBuffer, 
                           int nBytesToRead )

{
    return VSIFReadL( pabyBuffer, 1, nBytesToRead, 
                      (FILE *) psGFile->UserData );
}

/************************************************************************/
/*                          VSIGIFWriteFunc()                           */
/*                                                                      */
/*      Proxy write function.                                           */
/************************************************************************/

static int VSIGIFWriteFunc( GifFileType *psGFile, 
                            const GifByteType *pabyBuffer, int nBytesToWrite )

{
    return VSIFWriteL( (void *) pabyBuffer, 1, nBytesToWrite, 
                       (FILE *) psGFile->UserData );
}

/************************************************************************/
/*                          GDALRegister_GIF()                        */
/************************************************************************/

void GDALRegister_GIF()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "GIF" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GIF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Graphics Interchange Format (.gif)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_gif.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gif" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/gif" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>\n"
"   <Option name='INTERLACING' type='boolean'/>\n"
"   <Option name='WORLDFILE' type='boolean'/>\n"
"</CreationOptionList>\n" );

        poDriver->pfnOpen = GIFDataset::Open;
        poDriver->pfnCreateCopy = GIFCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

