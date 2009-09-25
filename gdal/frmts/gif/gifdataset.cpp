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

static const int InterlacedOffset[] = { 0, 4, 2, 1 }; 
static const int InterlacedJumps[] = { 8, 8, 4, 2 };  

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

    FILE        *fp;

    GifFileType *hGifFile;

    char        *pszProjection;
    int	        bGeoTransformValid;
    double      adfGeoTransform[6];

    int         nGCPCount;
    GDAL_GCP	*pasGCPList;

  public:
                 GIFDataset();
                 ~GIFDataset();

    virtual const char *GetProjectionRef();
    virtual CPLErr GetGeoTransform( double * );
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
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

    pszProjection = NULL;
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
/*                           ~GIFDataset()                            */
/************************************************************************/

GIFDataset::~GIFDataset()

{
    FlushCache();

    if ( pszProjection )
        CPLFree( pszProjection );

    if ( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    if( hGifFile )
        DGifCloseFile( hGifFile );

    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                        GetProjectionRef()                            */
/************************************************************************/

const char *GIFDataset::GetProjectionRef()

{
    if ( pszProjection && bGeoTransformValid )
        return pszProjection;
    else
        return GDALPamDataset::GetProjectionRef();
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
/*                            GetGCPCount()                             */
/************************************************************************/

int GIFDataset::GetGCPCount()

{
    if (nGCPCount > 0)
        return nGCPCount;
    else
        return GDALPamDataset::GetGCPCount();
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *GIFDataset::GetGCPProjection()

{
    if ( pszProjection && nGCPCount > 0 )
        return pszProjection;
    else
        return GDALPamDataset::GetGCPProjection();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GIFDataset::GetGCPs()

{
    if (nGCPCount > 0)
        return pasGCPList;
    else
        return GDALPamDataset::GetGCPs();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int GIFDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 8 )
        return FALSE;

    if( strncmp((const char *) poOpenInfo->pabyHeader, "GIF87a",5) != 0
        && strncmp((const char *) poOpenInfo->pabyHeader, "GIF89a",5) != 0 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GIFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
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
    int                  nGifErr;

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

    /* The following code enables us to detect GIF datasets eligible */
    /* for BIGGIF driver even with an unpatched giflib  */

    /* -------------------------------------------------------------------- */
    /*      Find the first image record.                                    */
    /* -------------------------------------------------------------------- */
    GifRecordType RecordType = TERMINATE_RECORD_TYPE;

    while( DGifGetRecordType(hGifFile, &RecordType) != GIF_ERROR
        && RecordType != TERMINATE_RECORD_TYPE
        && RecordType != IMAGE_DESC_RECORD_TYPE ) {}

    if( RecordType == IMAGE_DESC_RECORD_TYPE  &&
        DGifGetImageDesc(hGifFile) != GIF_ERROR)
    {
        int width = hGifFile->SavedImages[0].ImageDesc.Width;
        int height = hGifFile->SavedImages[0].ImageDesc.Height;
        if ((double) width * (double) height > 100000000.0 )
        {
            CPLDebug( "GIF",
                      "Due to limitations of the GDAL GIF driver we deliberately avoid\n"
                      "opening large GIF files (larger than 100 megapixels).");
            DGifCloseFile( hGifFile );
            VSIFCloseL( fp );
            return NULL;
        }
    }

    DGifCloseFile( hGifFile );

    VSIFSeekL( fp, 0, SEEK_SET);
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

    nGifErr = DGifSlurp( hGifFile );

    if( nGifErr != GIF_OK )
    {
        VSIFCloseL( fp );
        DGifCloseFile(hGifFile);

        if( nGifErr == D_GIF_ERR_DATA_TOO_BIG )
        {
             CPLDebug( "GIF",
                       "DGifSlurp() failed for %s because it was too large.\n"
                       "Due to limitations of the GDAL GIF driver we deliberately avoid\n"
                       "opening large GIF files (larger than 100 megapixels).",
                       poOpenInfo->pszFilename );
            return NULL;
         }
         else
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
        GDALReadWorldFile( poOpenInfo->pszFilename, NULL, 
                           poDS->adfGeoTransform );
    if ( !poDS->bGeoTransformValid )
    {
        poDS->bGeoTransformValid =
            GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                               poDS->adfGeoTransform );

        if ( !poDS->bGeoTransformValid )
        {
            int bOziFileOK = 
                GDALReadOziMapFile( poOpenInfo->pszFilename,
                                    poDS->adfGeoTransform, 
                                    &poDS->pszProjection,
                                    &poDS->nGCPCount, &poDS->pasGCPList );

            if ( bOziFileOK && poDS->nGCPCount == 0 )
                 poDS->bGeoTransformValid = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

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
    bInterlace = CSLFetchBoolean(papszOptions, "INTERLACING", FALSE);

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "GIF driver only supports one band images.\n" );

        return NULL;
    }

    if (nXSize > 65535 || nYSize > 65535)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "GIF driver only supports datasets up to 65535x65535 size.\n" );

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
                          psGifCT->ColorCount, 255, psGifCT) == GIF_ERROR)
    {
        FreeMapObject(psGifCT);
        PrintGifError();
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Error writing gif file." );
        EGifCloseFile(hGifFile);
        VSIFCloseL( fp );
        return NULL;
    }
    
    FreeMapObject(psGifCT);
    psGifCT = NULL;

    /* Support for transparency */
    int bNoDataValue;
    double noDataValue = poBand->GetNoDataValue(&bNoDataValue);
    if (bNoDataValue && noDataValue >= 0 && noDataValue <= 255)
    {
        unsigned char extensionData[4];
        extensionData[0] = 1; /*  Transparent Color Flag */
        extensionData[1] = 0;
        extensionData[2] = 0;
        extensionData[3] = (unsigned char)noDataValue;
        EGifPutExtension(hGifFile, 0xf9, 4, extensionData);
    }

    if (EGifPutImageDesc(hGifFile, 0, 0, nXSize, nYSize, bInterlace, NULL) == GIF_ERROR )
    {
        PrintGifError();
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Error writing gif file." );
        EGifCloseFile(hGifFile);
        VSIFCloseL( fp );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    CPLErr      eErr;
    GDALPamDataset *poDS;
    GByte      *pabyScanline;

    pabyScanline = (GByte *) CPLMalloc( nXSize );

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        eErr = CE_Failure;

    if( !bInterlace )
    {
        for( int iLine = 0; iLine < nYSize; iLine++ )
        {
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                     pabyScanline, nXSize, 1, GDT_Byte,
                                     nBands, nBands * nXSize );

            if( eErr != CE_None || EGifPutLine( hGifFile, pabyScanline, nXSize ) == GIF_ERROR )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Error writing gif file." );
                goto error;
            }

            if( !pfnProgress( (iLine + 1) * 1.0 / nYSize, NULL, pProgressData ) )
            {
                goto error;
            }

        }
    }
    else
    {
        int 	i, j;
        int nLinesRead = 0;
        int nLinesToRead = 0;
        for ( i = 0; i < 4; i++)
        {
            for (j = InterlacedOffset[i]; j < nYSize; j += InterlacedJumps[i]) 
            {
                nLinesToRead ++;
            }
        }

        /* Need to perform 4 passes on the images: */
        for ( i = 0; i < 4; i++)
        {
            for (j = InterlacedOffset[i]; j < nYSize; j += InterlacedJumps[i]) 
            {
                eErr= poBand->RasterIO( GF_Read, 0, j, nXSize, 1, 
                                        pabyScanline, nXSize, 1, GDT_Byte,
                                        1, nXSize );

                if (eErr != CE_None || EGifPutLine(hGifFile, pabyScanline, nXSize) == GIF_ERROR)
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                            "Error writing gif file." );
                    goto error;
                }

                nLinesRead ++;
                if( !pfnProgress( nLinesRead * 1.0 / nYSize, NULL, pProgressData ) )
                {
                    goto error;
                }
            }
        }
    }

    CPLFree( pabyScanline );
    pabyScanline = NULL;

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    if (EGifCloseFile(hGifFile) == GIF_ERROR)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "EGifCloseFile() failed.\n" );
        hGifFile = NULL;
        goto error;
    }
    hGifFile = NULL;

    /* This is a hack to write a GIF89a instead of GIF87a */
    /* (we have to, since we are using graphical extension block) */
    /* EGifSpew would write GIF89a when it detects an extension block if we were using it */
    /* As we don't, we could have used EGifSetGifVersion instead, but the version of libungif */
    /* in GDAL has a bug : it writes on read-only memory ! */
    /* (this is a well-known problem. Just google for "EGifSetGifVersion segfault") */
    /* Most readers don't even care if it is GIF87a or GIF89a, but it is */
    /* better to write the right version */

    VSIFSeekL(fp, 0, SEEK_SET);
    if (VSIFWriteL("GIF89a", 1, 6, fp) != 6)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Error writing gif file." );
        goto error;
    }

    VSIFCloseL( fp );
    fp = NULL;

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
    poDS = (GDALPamDataset *) 
        GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;

error:
    if (hGifFile)
        EGifCloseFile(hGifFile);
    if (fp)
        VSIFCloseL( fp );
    if (pabyScanline)
        CPLFree( pabyScanline );
    return NULL;
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

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = GIFDataset::Open;
        poDriver->pfnCreateCopy = GIFCreateCopy;
        poDriver->pfnIdentify = GIFDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

