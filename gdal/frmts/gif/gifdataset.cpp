/******************************************************************************
 * $Id$
 *
 * Project:  GIF Driver
 * Purpose:  Implement GDAL GIF Support using libungif code.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gifabstractdataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_GIF(void);

#if !(defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5)

// This prototype seems to have been messed up!
GifFileType * EGifOpen(void* userData, OutputFunc writeFunc);

// Define alias compatible with giflib >= 5.0.0
#define GifMakeMapObject MakeMapObject
#define GifFreeMapObject FreeMapObject

#endif // defined(GIFLIB_MAJOR) && GIFLIB_MAJOR < 5

CPL_C_END

static const int InterlacedOffset[] = { 0, 4, 2, 1 }; 
static const int InterlacedJumps[] = { 8, 8, 4, 2 };  

static int VSIGIFWriteFunc( GifFileType *, const GifByteType *, int );

/************************************************************************/
/* ==================================================================== */
/*	                            GIFDataset                              */
/* ==================================================================== */
/************************************************************************/

class GIFRasterBand;

class GIFDataset : public GIFAbstractDataset
{
    friend class GIFRasterBand;

  public:
                 GIFDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    static GDALDataset* CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );

};

/************************************************************************/
/* ==================================================================== */
/*                            GIFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GIFRasterBand : public GIFAbstractRasterBand
{
  public:

                   GIFRasterBand( GIFDataset *, int, SavedImage *, int );
    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/*                           GIFRasterBand()                            */
/************************************************************************/

GIFRasterBand::GIFRasterBand( GIFDataset *poDS, int nBand, 
                              SavedImage *psSavedImage, int nBackground ) :
                GIFAbstractRasterBand(poDS, nBand, psSavedImage, nBackground, FALSE)

{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GIFRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                  int nBlockYOff,
                                  void * pImage )
{
    CPLAssert( nBlockXOff == 0 );

    if (psImage == NULL)
    {
        memset(pImage, 0, nBlockXSize);
        return CE_None;
    }

    if( panInterlaceMap != NULL )
        nBlockYOff = panInterlaceMap[nBlockYOff];

    memcpy( pImage, psImage->RasterBits + nBlockYOff * nBlockXSize, 
            nBlockXSize );

    return CE_None;
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
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GIFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == NULL )
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The GIF driver does not support update access to existing"
                  " files.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Ingest.                                                         */
/* -------------------------------------------------------------------- */
    GifFileType 	*hGifFile;
    VSILFILE                *fp;
    int                  nGifErr;

    fp = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;

    hGifFile = GIFAbstractDataset::myDGifOpen( fp, GIFAbstractDataset::ReadFunc );
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
        && RecordType != IMAGE_DESC_RECORD_TYPE )
    {
        /* Skip extension records found before IMAGE_DESC_RECORD_TYPE */
        if (RecordType == EXTENSION_RECORD_TYPE)
        {
            int nFunction;
            GifByteType *pExtData;
            if (DGifGetExtension(hGifFile, &nFunction, &pExtData) == GIF_ERROR)
                break;
            while (pExtData != NULL)
            {
                if (DGifGetExtensionNext(hGifFile, &pExtData) == GIF_ERROR)
                    break;
            }
        }
    }

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
            GIFAbstractDataset::myDGifCloseFile( hGifFile );
            /* Reset poOpenInfo->fpL since BIGGIF may need it */
            poOpenInfo->fpL = fp;
            VSIFSeekL(fp, 0, SEEK_SET);
            return NULL;
        }
    }

    GIFAbstractDataset::myDGifCloseFile( hGifFile );

    VSIFSeekL( fp, 0, SEEK_SET);

    hGifFile = GIFAbstractDataset::myDGifOpen( fp, GIFAbstractDataset::ReadFunc );
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

    if( nGifErr != GIF_OK || hGifFile->SavedImages == NULL )
    {
        VSIFCloseL( fp );
        GIFAbstractDataset::myDGifCloseFile(hGifFile);

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

        if( psImage->ImageDesc.ColorMap == NULL &&
            poDS->hGifFile->SColorMap == NULL )
        {
            CPLDebug("GIF", "Skipping image without color table");
            continue;
        }
#if defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5
        /* Since giflib 5, de-interlacing is done by DGifSlurp() */
        psImage->ImageDesc.Interlace = 0;
#endif
        poDS->SetBand( poDS->nBands+1, 
                       new GIFRasterBand( poDS, poDS->nBands+1, psImage,
                                          hGifFile->SBackGroundColor ));
    }
    if( poDS->nBands == 0 )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check for georeferencing.                                       */
/* -------------------------------------------------------------------- */
    poDS->DetectGeoreferencing(poOpenInfo);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML( poOpenInfo->GetSiblingFiles() );

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename,
                                 poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                        GDALPrintGifError()                           */
/************************************************************************/

static void GDALPrintGifError(CPL_UNUSED GifFileType *hGifFile, const char* pszMsg)
{
/* GIFLIB_MAJOR is only defined in libgif >= 4.2.0 */
/* libgif 4.2.0 has retired PrintGifError() and added GifErrorString() */
#if defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && \
        ((GIFLIB_MAJOR == 4 && GIFLIB_MINOR >= 2) || GIFLIB_MAJOR > 4)
    /* Static string actually, hence the const char* cast */

#if GIFLIB_MAJOR >= 5
    const char* pszGIFLIBError = (const char*) GifErrorString(hGifFile->Error);
#else
    const char* pszGIFLIBError = (const char*) GifErrorString();
#endif
    if (pszGIFLIBError == NULL)
        pszGIFLIBError = "Unknown error";
    CPLError( CE_Failure, CPLE_AppDefined,
              "%s. GIFLib Error : %s", pszMsg, pszGIFLIBError );
#else
    PrintGifError();
    CPLError( CE_Failure, CPLE_AppDefined, "%s", pszMsg );
#endif
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
GIFDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
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
    VSILFILE *fp;

    fp = VSIFOpenL( pszFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create %s:\n%s", 
                  pszFilename, VSIStrerror( errno ) );
        return NULL;
    }

#if defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5
    int nError;
    hGifFile = EGifOpen( fp, VSIGIFWriteFunc, &nError );
#else
    hGifFile = EGifOpen( fp, VSIGIFWriteFunc );
#endif
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
        psGifCT = GifMakeMapObject( 256, NULL );
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

        psGifCT = GifMakeMapObject( nFullCount, NULL );
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
                          8, /* ColorRes */
                          255, /* Background */
                          psGifCT) == GIF_ERROR)
    {
        GifFreeMapObject(psGifCT);
        GDALPrintGifError(hGifFile, "Error writing gif file.");
        GIFAbstractDataset::myEGifCloseFile(hGifFile);
        VSIFCloseL( fp );
        return NULL;
    }
    
    GifFreeMapObject(psGifCT);
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
        GDALPrintGifError(hGifFile, "Error writing gif file.");
        GIFAbstractDataset::myEGifCloseFile(hGifFile);
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
                                     nBands, nBands * nXSize, NULL );

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
                                        1, nXSize, NULL );

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
    if (GIFAbstractDataset::myEGifCloseFile(hGifFile) == GIF_ERROR)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "EGifCloseFile() failed.\n" );
        hGifFile = NULL;
        goto error;
    }
    hGifFile = NULL;

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
/*      Re-open dataset, and copy any auxiliary pam information.         */
/* -------------------------------------------------------------------- */

    /* If outputing to stdout, we can't reopen it, so we'll return */
    /* a fake dataset to make the caller happy */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poDS = (GDALPamDataset*) GDALOpen(pszFilename, GA_ReadOnly);
    CPLPopErrorHandler();
    if (poDS)
    {
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
        return poDS;
    }
    else
    {
        CPLErrorReset();

        GIFDataset* poGIF_DS = new GIFDataset();
        poGIF_DS->nRasterXSize = nXSize;
        poGIF_DS->nRasterYSize = nYSize;
        for(int i=0;i<nBands;i++)
            poGIF_DS->SetBand( i+1, new GIFRasterBand( poGIF_DS, i+1, NULL, 0 ) );
        return poGIF_DS;
    }

error:
    if (hGifFile)
        GIFAbstractDataset::myEGifCloseFile(hGifFile);
    if (fp)
        VSIFCloseL( fp );
    if (pabyScanline)
        CPLFree( pabyScanline );
    return NULL;
}

/************************************************************************/
/*                          VSIGIFWriteFunc()                           */
/*                                                                      */
/*      Proxy write function.                                           */
/************************************************************************/

static int VSIGIFWriteFunc( GifFileType *psGFile, 
                            const GifByteType *pabyBuffer, int nBytesToWrite )

{
    VSILFILE* fp = (VSILFILE *) psGFile->UserData;
    if ( VSIFTellL(fp) == 0 && nBytesToWrite >= 6 &&
         memcmp(pabyBuffer, "GIF87a", 6) == 0 )
    {
        /* This is a hack to write a GIF89a instead of GIF87a */
        /* (we have to, since we are using graphical extension block) */
        /* EGifSpew would write GIF89a when it detects an extension block if we were using it */
        /* As we don't, we could have used EGifSetGifVersion instead, but the version of libungif */
        /* in GDAL has a bug : it writes on read-only memory ! */
        /* (this is a well-known problem. Just google for "EGifSetGifVersion segfault") */
        /* Most readers don't even care if it is GIF87a or GIF89a, but it is */
        /* better to write the right version */

        int nRet = VSIFWriteL("GIF89a", 1, 6, fp);
        nRet += VSIFWriteL( (char *) pabyBuffer + 6, 1, nBytesToWrite - 6, fp );
        return nRet;
    }
    else
        return VSIFWriteL( (void *) pabyBuffer, 1, nBytesToWrite, fp );
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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
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
        poDriver->pfnCreateCopy = GIFDataset::CreateCopy;
        poDriver->pfnIdentify = GIFAbstractDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
