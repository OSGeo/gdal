/******************************************************************************
 *
 * Project:  BIGGIF Driver
 * Purpose:  Implement GDAL support for reading large GIF files in a
 *           streaming fashion rather than the slurp-into-memory approach
 *           of the normal GIF driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001-2008, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gifabstractdataset.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                          BIGGIFDataset                               */
/* ==================================================================== */
/************************************************************************/

class BIGGifRasterBand;

class BIGGIFDataset : public GIFAbstractDataset
{
    friend class BIGGifRasterBand;

    int         nLastLineRead;

    GDALDataset *poWorkDS;

    CPLErr       ReOpen();

  protected:
    virtual int         CloseDependentDatasets() override;

  public:
                 BIGGIFDataset();
    virtual ~BIGGIFDataset();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            BIGGifRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class BIGGifRasterBand : public GIFAbstractRasterBand
{
    friend class BIGGIFDataset;

  public:
                   BIGGifRasterBand( BIGGIFDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/*                          BIGGifRasterBand()                          */
/************************************************************************/

BIGGifRasterBand::BIGGifRasterBand( BIGGIFDataset *poDSIn, int nBackground ) :
    GIFAbstractRasterBand(poDSIn, 1, poDSIn->hGifFile->SavedImages,
                          nBackground, TRUE)

{}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BIGGifRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                     int nBlockYOff,
                                     void * pImage )
{
    BIGGIFDataset *poGDS = (BIGGIFDataset *) poDS;

    CPLAssert( nBlockXOff == 0 );

    if( panInterlaceMap != NULL )
        nBlockYOff = panInterlaceMap[nBlockYOff];

/* -------------------------------------------------------------------- */
/*      Do we already have this line in the work dataset?               */
/* -------------------------------------------------------------------- */
    if( poGDS->poWorkDS != NULL && nBlockYOff <= poGDS->nLastLineRead )
    {
        return poGDS->poWorkDS->
            RasterIO( GF_Read, 0, nBlockYOff, nBlockXSize, 1,
                      pImage, nBlockXSize, 1, GDT_Byte,
                      1, NULL, 0, 0, 0, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Do we need to restart from the start of the image?              */
/* -------------------------------------------------------------------- */
    if( nBlockYOff <= poGDS->nLastLineRead )
    {
        if( poGDS->ReOpen() == CE_Failure )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read till we get our target line.                               */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    while( poGDS->nLastLineRead < nBlockYOff && eErr == CE_None )
    {
        if( DGifGetLine( poGDS->hGifFile, (GifPixelType*)pImage,
                         nBlockXSize ) == GIF_ERROR )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failure decoding scanline of GIF file." );
            return CE_Failure;
        }

        poGDS->nLastLineRead++;

        if( poGDS->poWorkDS != NULL )
        {
            eErr = poGDS->poWorkDS->RasterIO( GF_Write,
                                       0, poGDS->nLastLineRead, nBlockXSize, 1,
                                       pImage, nBlockXSize, 1, GDT_Byte,
                                       1, NULL, 0, 0, 0, NULL );
        }
    }

    return eErr;
}

/************************************************************************/
/* ==================================================================== */
/*                             BIGGIFDataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            BIGGIFDataset()                            */
/************************************************************************/

BIGGIFDataset::BIGGIFDataset() :
    nLastLineRead(-1),
    poWorkDS(NULL)
{}

/************************************************************************/
/*                           ~BIGGIFDataset()                            */
/************************************************************************/

BIGGIFDataset::~BIGGIFDataset()

{
    FlushCache();

    CloseDependentDatasets();
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int BIGGIFDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if( poWorkDS != NULL )
    {
        bHasDroppedRef = TRUE;

        CPLString osTempFilename = poWorkDS->GetDescription();
        GDALDriver* poDrv = poWorkDS->GetDriver();

        GDALClose( (GDALDatasetH) poWorkDS );
        poWorkDS = NULL;

        if( poDrv != NULL )
            poDrv->Delete( osTempFilename );

        poWorkDS = NULL;
    }

    return bHasDroppedRef;
}

/************************************************************************/
/*                               ReOpen()                               */
/*                                                                      */
/*      (Re)Open the gif file and process past the first image          */
/*      descriptor.                                                     */
/************************************************************************/

CPLErr BIGGIFDataset::ReOpen()

{
/* -------------------------------------------------------------------- */
/*      If the file is already open, close it so we can restart.        */
/* -------------------------------------------------------------------- */
    if( hGifFile != NULL )
        GIFAbstractDataset::myDGifCloseFile( hGifFile );

/* -------------------------------------------------------------------- */
/*      If we are actually reopening, then we assume that access to     */
/*      the image data is not strictly once through sequential, and     */
/*      we will try to create a working database in a temporary         */
/*      directory to hold the image as we read through it the second    */
/*      time.                                                           */
/* -------------------------------------------------------------------- */
    if( hGifFile != NULL )
    {
        GDALDriver *poGTiffDriver = (GDALDriver*) GDALGetDriverByName("GTiff");

        if( poGTiffDriver != NULL )
        {
            /* Create as a sparse file to avoid filling up the whole file */
            /* while closing and then destroying this temporary dataset */
            const char* apszOptions[] = { "COMPRESS=LZW", "SPARSE_OK=YES", NULL };
            CPLString osTempFilename = CPLGenerateTempFilename("biggif");

            osTempFilename += ".tif";

            poWorkDS = poGTiffDriver->Create( osTempFilename,
                                              nRasterXSize, nRasterYSize, 1,
                                              GDT_Byte, const_cast<char**>(apszOptions));
        }
    }

/* -------------------------------------------------------------------- */
/*      Open                                                            */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fp, 0, SEEK_SET );

    nLastLineRead = -1;
    hGifFile = GIFAbstractDataset::myDGifOpen( fp, GIFAbstractDataset::ReadFunc );
    if( hGifFile == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "DGifOpen() failed.  Perhaps the gif file is corrupt?\n" );

        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Find the first image record.                                    */
/* -------------------------------------------------------------------- */
    GifRecordType RecordType = FindFirstImage(hGifFile);
    if( RecordType != IMAGE_DESC_RECORD_TYPE )
    {
        GIFAbstractDataset::myDGifCloseFile( hGifFile );
        hGifFile = NULL;

        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to find image description record in GIF file." );
        return CE_Failure;
    }

    if (DGifGetImageDesc(hGifFile) == GIF_ERROR)
    {
        GIFAbstractDataset::myDGifCloseFile( hGifFile );
        hGifFile = NULL;

        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Image description reading failed in GIF file." );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BIGGIFDataset::Open( GDALOpenInfo * poOpenInfo )

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
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    BIGGIFDataset *poDS = new BIGGIFDataset();

    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;
    poDS->eAccess = GA_ReadOnly;
    if( poDS->ReOpen() == CE_Failure )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */

    poDS->nRasterXSize = poDS->hGifFile->SavedImages[0].ImageDesc.Width;
    poDS->nRasterYSize = poDS->hGifFile->SavedImages[0].ImageDesc.Height;
    if( !GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) )
    {
        delete poDS;
        return NULL;
    }

    if( poDS->hGifFile->SavedImages[0].ImageDesc.ColorMap == NULL &&
        poDS->hGifFile->SColorMap == NULL )
    {
        CPLDebug("GIF", "Skipping image without color table");
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1,
                   new BIGGifRasterBand( poDS,
                                         poDS->hGifFile->SBackGroundColor ));

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
/*                        GDALRegister_BIGGIF()                         */
/************************************************************************/

void GDALRegister_BIGGIF()

{
    if( GDALGetDriverByName( "BIGGIF" ) != NULL )
        return;

     GDALDriver *poDriver = new GDALDriver();

     poDriver->SetDescription( "BIGGIF" );
     poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
     poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                "Graphics Interchange Format (.gif)" );
     poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                "frmt_gif.html" );
     poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gif" );
     poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/gif" );
     poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

     poDriver->pfnOpen = BIGGIFDataset::Open;
     poDriver->pfnIdentify = GIFAbstractDataset::Identify;

     GetGDALDriverManager()->RegisterDriver( poDriver );
}
