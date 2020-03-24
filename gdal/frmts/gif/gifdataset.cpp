/******************************************************************************
 *
 * Project:  GIF Driver
 * Purpose:  Implement GDAL GIF Support using libungif code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "gifabstractdataset.h"

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

CPL_CVSID("$Id$")

CPL_C_START
#if !(defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5)

// This prototype seems to have been messed up!
GifFileType * EGifOpen(void* userData, OutputFunc writeFunc);

// Define alias compatible with giflib >= 5.0.0
#define GifMakeMapObject MakeMapObject
#define GifFreeMapObject FreeMapObject

#endif // defined(GIFLIB_MAJOR) && GIFLIB_MAJOR < 5

CPL_C_END

constexpr int InterlacedOffset[] = { 0, 4, 2, 1 };
constexpr int InterlacedJumps[] = { 8, 8, 4, 2 };

/************************************************************************/
/*                          VSIGIFWriteFunc()                           */
/*                                                                      */
/*      Proxy write function.                                           */
/************************************************************************/

static int VSIGIFWriteFunc( GifFileType *psGFile,
                            const GifByteType *pabyBuffer, int nBytesToWrite )

{
    VSILFILE* fp = static_cast<VSILFILE *>(psGFile->UserData);
    if( VSIFTellL(fp) == 0 && nBytesToWrite >= 6 &&
        memcmp(pabyBuffer, "GIF87a", 6) == 0 )
    {
        // This is a hack to write a GIF89a instead of GIF87a (we have to, since
        // we are using graphical extension block).  EGifSpew would write GIF89a
        // when it detects an extension block if we were using it As we don't,
        // we could have used EGifSetGifVersion instead, but the version of
        // libungif in GDAL has a bug: it writes on read-only memory!
        // This is a well-known problem. Just google for "EGifSetGifVersion
        // segfault".
        // Most readers don't even care if it is GIF87a or GIF89a, but it is
        // better to write the right version.

        size_t nRet = VSIFWriteL("GIF89a", 1, 6, fp);
        nRet += VSIFWriteL( reinterpret_cast<const char *>(pabyBuffer) + 6,
                            1, nBytesToWrite - 6, fp );
        return static_cast<int>(nRet);
    }

    return static_cast<int>(VSIFWriteL( pabyBuffer, 1, nBytesToWrite, fp ));
}

/************************************************************************/
/* ==================================================================== */
/*                                  GIFDataset                          */
/* ==================================================================== */
/************************************************************************/

class GIFRasterBand;

class GIFDataset final: public GIFAbstractDataset
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

class GIFRasterBand final: public GIFAbstractRasterBand
{
  public:
    GIFRasterBand( GIFDataset *, int, SavedImage *, int );
    CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           GIFRasterBand()                            */
/************************************************************************/

GIFRasterBand::GIFRasterBand( GIFDataset *poDSIn, int nBandIn,
                              SavedImage *psSavedImage, int nBackground ) :
    GIFAbstractRasterBand(poDSIn, nBandIn, psSavedImage, nBackground, FALSE)
{}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GIFRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                  int nBlockYOff,
                                  void * pImage )
{
    CPLAssert( nBlockXOff == 0 );

    if( psImage == nullptr )
    {
        memset(pImage, 0, nBlockXSize);
        return CE_None;
    }

    if( panInterlaceMap != nullptr )
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

GIFDataset::GIFDataset() {}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GIFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == nullptr )
        return nullptr;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The GIF driver does not support update access to existing"
                  " files." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Ingest.                                                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    GifFileType *hGifFile
        = GIFAbstractDataset::myDGifOpen( fp, GIFAbstractDataset::ReadFunc );
    if( hGifFile == nullptr )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "DGifOpen() failed for %s.  "
                  "Perhaps the gif file is corrupt?",
                  poOpenInfo->pszFilename );

        return nullptr;
    }

    // The following code enables us to detect GIF datasets eligible
    // for BIGGIF driver even with an unpatched giflib.

    /* -------------------------------------------------------------------- */
    /*      Find the first image record.                                    */
    /* -------------------------------------------------------------------- */
    GifRecordType RecordType = FindFirstImage(hGifFile);
    if( RecordType == IMAGE_DESC_RECORD_TYPE  &&
        DGifGetImageDesc(hGifFile) != GIF_ERROR)
    {
        const int width = hGifFile->SavedImages[0].ImageDesc.Width;
        const int height = hGifFile->SavedImages[0].ImageDesc.Height;
        if( static_cast<double>(width) * height > 100000000.0 )
        {
            CPLDebug( "GIF",
                      "Due to limitations of the GDAL GIF driver we "
                      "deliberately avoid opening large GIF files "
                      "(larger than 100 megapixels).");
            GIFAbstractDataset::myDGifCloseFile( hGifFile );
            // Reset poOpenInfo->fpL since BIGGIF may need it.
            poOpenInfo->fpL = fp;
            VSIFSeekL(fp, 0, SEEK_SET);
            return nullptr;
        }
    }

    GIFAbstractDataset::myDGifCloseFile( hGifFile );

    VSIFSeekL( fp, 0, SEEK_SET);

    hGifFile =
        GIFAbstractDataset::myDGifOpen( fp, GIFAbstractDataset::ReadFunc );
    if( hGifFile == nullptr )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "DGifOpen() failed for %s.  "
                  "Perhaps the gif file is corrupt?",
                  poOpenInfo->pszFilename );

        return nullptr;
    }

    const int nGifErr = DGifSlurp( hGifFile );

    if( nGifErr != GIF_OK || hGifFile->SavedImages == nullptr )
    {
        VSIFCloseL( fp );
        GIFAbstractDataset::myDGifCloseFile(hGifFile);

        if( nGifErr == D_GIF_ERR_DATA_TOO_BIG )
        {
             CPLDebug( "GIF",
                       "DGifSlurp() failed for %s because it was too large.  "
                       "Due to limitations of the GDAL GIF driver we "
                       "deliberately avoid opening large GIF files "
                       "(larger than 100 megapixels).",
                       poOpenInfo->pszFilename );
            return nullptr;
        }

        CPLError( CE_Failure, CPLE_OpenFailed,
                  "DGifSlurp() failed for %s.  "
                  "Perhaps the gif file is corrupt?",
                  poOpenInfo->pszFilename );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GIFDataset *poDS = new GIFDataset();

    poDS->fp = fp;
    poDS->eAccess = GA_ReadOnly;
    poDS->hGifFile = hGifFile;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = hGifFile->SavedImages[0].ImageDesc.Width;
    poDS->nRasterYSize = hGifFile->SavedImages[0].ImageDesc.Height;
    if( !GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) )
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iImage = 0; iImage < hGifFile->ImageCount; iImage++ )
    {
        SavedImage *psImage = hGifFile->SavedImages + iImage;

        if( psImage->ImageDesc.Width != poDS->nRasterXSize
            || psImage->ImageDesc.Height != poDS->nRasterYSize )
            continue;

        if( psImage->ImageDesc.ColorMap == nullptr &&
            poDS->hGifFile->SColorMap == nullptr )
        {
            CPLDebug("GIF", "Skipping image without color table");
            continue;
        }
#if defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5
        // Since giflib 5, de-interlacing is done by DGifSlurp().
        psImage->ImageDesc.Interlace = false;
#endif
        poDS->SetBand( poDS->nBands+1,
                       new GIFRasterBand( poDS, poDS->nBands+1, psImage,
                                          hGifFile->SBackGroundColor ));
    }
    if( poDS->nBands == 0 )
    {
        delete poDS;
        return nullptr;
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

static void GDALPrintGifError( CPL_UNUSED GifFileType *hGifFile,
                               const char* pszMsg )
{
    // GIFLIB_MAJOR is only defined in libgif >= 4.2.0.
    // libgif 4.2.0 has retired PrintGifError() and added GifErrorString().
#if defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && \
        ((GIFLIB_MAJOR == 4 && GIFLIB_MINOR >= 2) || GIFLIB_MAJOR > 4)
    // Static string actually, hence the const char* cast.

#if GIFLIB_MAJOR >= 5
    const char* pszGIFLIBError = GifErrorString(hGifFile->Error);
#else
    // TODO(schwehr): Can we remove the cast for older libgif?
    const char* pszGIFLIBError = (const char*) GifErrorString();
#endif
    if( pszGIFLIBError == nullptr )
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
GIFDataset::CreateCopy(
    const char * pszFilename, GDALDataset *poSrcDS,
    int bStrict, char ** papszOptions,
    GDALProgressFunc pfnProgress, void * pProgressData )

{
/* -------------------------------------------------------------------- */
/*      Check for interlaced option.                                    */
/* -------------------------------------------------------------------- */
    const bool bInterlace =
        CPLFetchBool(papszOptions, "INTERLACING", false);

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    const int nBands = poSrcDS->GetRasterCount();
    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GIF driver only supports one band images." );

        return nullptr;
    }

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    if( nXSize > 65535 || nYSize > 65535 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GIF driver only supports datasets up to 65535x65535 size.");

        return nullptr;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte
        && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GIF driver doesn't support data type %s. "
                  "Only eight bit bands supported.",
                  GDALGetDataTypeName(
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Open the output file.                                           */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "wb" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create %s:\n%s",
                  pszFilename, VSIStrerror( errno ) );
        return nullptr;
    }

#if defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5
    int nError = 0;
    GifFileType *hGifFile = EGifOpen( fp, VSIGIFWriteFunc, &nError );
#else
    GifFileType *hGifFile = EGifOpen( fp, VSIGIFWriteFunc );
#endif
    if( hGifFile == nullptr )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "EGifOpenFilename(%s) failed.  Does file already exist?",
                  pszFilename );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Prepare colortable.                                             */
/* -------------------------------------------------------------------- */
    GDALRasterBand *poBand = poSrcDS->GetRasterBand(1);
    ColorMapObject *psGifCT = nullptr;

    if( poBand->GetColorTable() == nullptr )
    {
        psGifCT = GifMakeMapObject( 256, nullptr );
        if( psGifCT == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot allocate color table");
            GIFAbstractDataset::myEGifCloseFile(hGifFile);
            VSIFCloseL( fp );
            return nullptr;
        }
        for( int iColor = 0; iColor < 256; iColor++ )
        {
            psGifCT->Colors[iColor].Red = static_cast<GifByteType>(iColor);
            psGifCT->Colors[iColor].Green = static_cast<GifByteType>(iColor);
            psGifCT->Colors[iColor].Blue = static_cast<GifByteType>(iColor);
        }
    }
    else
    {
        GDALColorTable *poCT = poBand->GetColorTable();
        int nFullCount = 2;

        while( nFullCount < poCT->GetColorEntryCount() )
            nFullCount = nFullCount * 2;

        psGifCT = GifMakeMapObject( nFullCount, nullptr );
        if( psGifCT == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot allocate color table");
            GIFAbstractDataset::myEGifCloseFile(hGifFile);
            VSIFCloseL( fp );
            return nullptr;
        }
        int iColor = 0;
        for( ; iColor < poCT->GetColorEntryCount(); iColor++ )
        {
            GDALColorEntry sEntry;

            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            psGifCT->Colors[iColor].Red = static_cast<GifByteType>(sEntry.c1);
            psGifCT->Colors[iColor].Green = static_cast<GifByteType>(sEntry.c2);
            psGifCT->Colors[iColor].Blue = static_cast<GifByteType>(sEntry.c3);
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
    if( EGifPutScreenDesc(hGifFile, nXSize, nYSize,
                          8, /* ColorRes */
                          255, /* Background */
                          psGifCT) == GIF_ERROR )
    {
        GifFreeMapObject(psGifCT);
        GDALPrintGifError(hGifFile, "Error writing gif file.");
        GIFAbstractDataset::myEGifCloseFile(hGifFile);
        VSIFCloseL( fp );
        return nullptr;
    }

    GifFreeMapObject(psGifCT);
    psGifCT = nullptr;

    // Support for transparency.
    int bNoDataValue = 0;
    double noDataValue = poBand->GetNoDataValue(&bNoDataValue);
    if( bNoDataValue && noDataValue >= 0 && noDataValue <= 255 )
    {
      unsigned char extensionData[4] = {
        1,  // Transparent Color Flag.
        0,
        0,
        static_cast<unsigned char>(noDataValue) };
        EGifPutExtension(hGifFile, 0xf9, 4, extensionData);
    }

    if( EGifPutImageDesc(hGifFile, 0, 0, nXSize, nYSize,
                         bInterlace, nullptr) == GIF_ERROR )
    {
        GDALPrintGifError(hGifFile, "Error writing gif file.");
        GIFAbstractDataset::myEGifCloseFile(hGifFile);
        VSIFCloseL( fp );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = nullptr;
    GByte *pabyScanline = static_cast<GByte *>(CPLMalloc( nXSize ));

    if( !pfnProgress( 0.0, nullptr, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to setup progress." );
    }

    if( !bInterlace )
    {
        for( int iLine = 0; iLine < nYSize; iLine++ )
        {
            const CPLErr eErr =
                poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1,
                                  pabyScanline, nXSize, 1, GDT_Byte,
                                  nBands, nBands * nXSize, nullptr );

            if( eErr != CE_None ||
                EGifPutLine( hGifFile, pabyScanline, nXSize ) == GIF_ERROR )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Error writing gif file." );
                goto error;
            }

            if( !pfnProgress( (iLine + 1) * 1.0 / nYSize,
                              nullptr, pProgressData ) )
            {
                goto error;
            }
        }
    }
    else
    {
        int nLinesToRead = 0;
        for( int i = 0; i < 4; i++)
        {
            for( int j = InterlacedOffset[i];
                 j < nYSize;
                 j += InterlacedJumps[i] )
            {
                nLinesToRead++;
            }
        }

        int nLinesRead = 0;
        // Need to perform 4 passes on the images:
        for( int i = 0; i < 4; i++)
        {
            for( int j = InterlacedOffset[i];
                 j < nYSize;
                 j += InterlacedJumps[i] )
            {
                const CPLErr eErr =
                    poBand->RasterIO( GF_Read, 0, j, nXSize, 1,
                                      pabyScanline, nXSize, 1, GDT_Byte,
                                      1, nXSize, nullptr );

                if( eErr != CE_None ||
                    EGifPutLine(hGifFile, pabyScanline, nXSize) == GIF_ERROR )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                            "Error writing gif file." );
                    goto error;
                }

                nLinesRead++;
                if( !pfnProgress( nLinesRead * 1.0 / nYSize,
                                  nullptr, pProgressData ) )
                {
                    goto error;
                }
            }
        }
    }

    CPLFree( pabyScanline );
    pabyScanline = nullptr;

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( GIFAbstractDataset::myEGifCloseFile(hGifFile) == GIF_ERROR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "EGifCloseFile() failed." );
        hGifFile = nullptr;
        goto error;
    }
    hGifFile = nullptr;

    VSIFCloseL( fp );
    fp = nullptr;

/* -------------------------------------------------------------------- */
/*      Do we need a world file?                                        */
/* -------------------------------------------------------------------- */
    if( CPLFetchBool( papszOptions, "WORLDFILE", false ) )
    {
        double adfGeoTransform[6] = {};

        if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
            GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxiliary pam information.         */
/* -------------------------------------------------------------------- */

    // If writing to stdout, we can't reopen it, so return
    // a fake dataset to make the caller happy.
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poDS = static_cast<GDALPamDataset *>(GDALOpen(pszFilename, GA_ReadOnly));
    CPLPopErrorHandler();
    if( poDS )
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
        for( int i = 0; i < nBands; i++ )
            poGIF_DS->SetBand( i+1,
                               new GIFRasterBand( poGIF_DS, i+1, nullptr, 0 ) );
        return poGIF_DS;
    }

error:
    if( hGifFile )
        GIFAbstractDataset::myEGifCloseFile(hGifFile);
    if( fp )
        VSIFCloseL( fp );
    if( pabyScanline )
        CPLFree( pabyScanline );
    return nullptr;
}

/************************************************************************/
/*                          GDALRegister_GIF()                          */
/************************************************************************/

void GDALRegister_GIF()

{
    if( GDALGetDriverByName( "GIF" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "GIF" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Graphics Interchange Format (.gif)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/gif.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gif" );
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/gif" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
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
