/******************************************************************************
 * $Id$
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement GDAL JPEG Support based on IJG libjpeg.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * $Log$
 * Revision 1.17  2003/10/31 00:58:55  warmerda
 * Added support for .jpgw as per request from Markus.
 *
 * Revision 1.16  2003/04/04 13:45:12  warmerda
 * Made casting to JSAMPLE explicit.
 *
 * Revision 1.15  2002/11/23 18:57:06  warmerda
 * added CREATIONOPTIONS metadata on driver
 *
 * Revision 1.14  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.13  2002/07/13 04:16:39  warmerda
 * added WORLDFILE support
 *
 * Revision 1.12  2002/06/20 19:57:04  warmerda
 * ensure GetGeoTransform always sets geotransform.
 *
 * Revision 1.11  2002/06/18 02:50:20  warmerda
 * fixed multiline string constants
 *
 * Revision 1.10  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.9  2001/12/11 16:50:16  warmerda
 * try to push green and blue values into cache when reading red
 *
 * Revision 1.8  2001/11/11 23:51:00  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.7  2001/08/22 17:11:30  warmerda
 * added support for .wld world files
 *
 * Revision 1.6  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.5  2001/06/01 14:13:12  warmerda
 * Improved magic number testing.
 *
 * Revision 1.4  2001/05/01 18:18:28  warmerda
 * added world file support
 *
 * Revision 1.3  2001/01/12 21:19:25  warmerda
 * added progressive support
 *
 * Revision 1.2  2000/07/07 15:11:01  warmerda
 * added QUALITY=n creation option
 *
 * Revision 1.1  2000/04/28 20:57:57  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
#include "jpeglib.h"
CPL_C_END

CPL_C_START
void	GDALRegister_JPEG(void);
CPL_C_END


/************************************************************************/
/* ==================================================================== */
/*				JPGDataset				*/
/* ==================================================================== */
/************************************************************************/

class JPGRasterBand;

class JPGDataset : public GDALDataset
{
    friend class JPGRasterBand;

    struct jpeg_decompress_struct sDInfo;
    struct jpeg_error_mgr sJErr;

    int	   bGeoTransformValid;
    double adfGeoTransform[6];

    FILE   *fpImage;
    int    nLoadedScanline;
    GByte  *pabyScanline;

    CPLErr LoadScanline(int);
    void   Restart();

  public:
                 JPGDataset();
                 ~JPGDataset();

    virtual CPLErr GetGeoTransform( double * );
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            JPGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class JPGRasterBand : public GDALRasterBand
{
    friend class JPGDataset;

  public:

                   JPGRasterBand( JPGDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                           JPGRasterBand()                            */
/************************************************************************/

JPGRasterBand::JPGRasterBand( JPGDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_Byte;
    nBlockXSize = poDS->nRasterXSize;;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    JPGDataset	*poGDS = (JPGDataset *) poDS;
    CPLErr      eErr;
    int         i, nXSize = GetXSize();
    
    CPLAssert( nBlockXOff == 0 );

/* -------------------------------------------------------------------- */
/*      Load the desired scanline into the working buffer.              */
/* -------------------------------------------------------------------- */
    eErr = poGDS->LoadScanline( nBlockYOff );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Transfer between the working buffer the the callers buffer.     */
/* -------------------------------------------------------------------- */
    if( poGDS->GetRasterCount() == 1 )
        memcpy( pImage, poGDS->pabyScanline, nXSize );
    else
    {
        for( i = 0; i < nXSize; i++ )
            ((GByte *) pImage)[i] = poGDS->pabyScanline[i*3+nBand-1];
    }

/* -------------------------------------------------------------------- */
/*      Forceably load the other bands associated with this scanline.   */
/* -------------------------------------------------------------------- */
    if( poGDS->GetRasterCount() == 3 && nBand == 1 )
    {
        poGDS->GetRasterBand(2)->GetBlockRef( nBlockXOff, nBlockYOff );
        poGDS->GetRasterBand(3)->GetBlockRef( nBlockXOff, nBlockYOff );
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JPGRasterBand::GetColorInterpretation()

{
    JPGDataset	*poGDS = (JPGDataset *) poDS;

    if( poGDS->nBands == 1 )
        return GCI_GrayIndex;

    else if( nBand == 1 )
        return GCI_RedBand;

    else if( nBand == 2 )
        return GCI_GreenBand;

    else 
        return GCI_BlueBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             JPGDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            JPGDataset()                              */
/************************************************************************/

JPGDataset::JPGDataset()

{
    pabyScanline = NULL;
    nLoadedScanline = -1;

    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~JPGDataset()                            */
/************************************************************************/

JPGDataset::~JPGDataset()

{
    FlushCache();

    jpeg_abort_decompress( &sDInfo );
    jpeg_destroy_decompress( &sDInfo );

    if( fpImage != NULL )
        VSIFClose( fpImage );

    if( pabyScanline != NULL )
        CPLFree( pabyScanline );
}

/************************************************************************/
/*                            LoadScanline()                            */
/************************************************************************/

CPLErr JPGDataset::LoadScanline( int iLine )

{
    if( nLoadedScanline == iLine )
        return CE_None;

    if( pabyScanline == NULL )
        pabyScanline = (GByte *)CPLMalloc(GetRasterCount() * GetRasterXSize());

    if( iLine < nLoadedScanline )
        Restart();
        
    while( nLoadedScanline < iLine )
    {
        JSAMPLE	*ppSamples;
            
        ppSamples = (JSAMPLE *) pabyScanline;
        jpeg_read_scanlines( &sDInfo, &ppSamples, 1 );
        nLoadedScanline++;
    }

    return CE_None;
}

/************************************************************************/
/*                              Restart()                               */
/*                                                                      */
/*      Restart compressor at the beginning of the file.                */
/************************************************************************/

void JPGDataset::Restart()

{
    jpeg_abort_decompress( &sDInfo );
    jpeg_destroy_decompress( &sDInfo );
    jpeg_create_decompress( &sDInfo );

    VSIRewind( fpImage );

    jpeg_stdio_src( &sDInfo, fpImage );
    jpeg_read_header( &sDInfo, TRUE );
    
    if( GetRasterCount() == 1 )
        sDInfo.out_color_space = JCS_GRAYSCALE;
    else
        sDInfo.out_color_space = JCS_RGB;
    nLoadedScanline = -1;
    jpeg_start_decompress( &sDInfo );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JPGDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    if( bGeoTransformValid )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JPGDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 10 )
        return NULL;

    if( poOpenInfo->pabyHeader[0] != 0xff
        || poOpenInfo->pabyHeader[1] != 0xd8
        || poOpenInfo->pabyHeader[2] != 0xff )
        return NULL;

    /* Some files lack the JFIF marker, like IMG_0519.JPG. For these we
       require the .jpg extension */
    if( (poOpenInfo->pabyHeader[3] != 0xe0
         || poOpenInfo->pabyHeader[6] != 'J'
         || poOpenInfo->pabyHeader[7] != 'F'
         || poOpenInfo->pabyHeader[8] != 'I'
         || poOpenInfo->pabyHeader[9] != 'F')
        && !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"jpg") )
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The JPEG driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JPGDataset 	*poDS;

    poDS = new JPGDataset();

    poDS->eAccess = GA_ReadOnly;

    poDS->sDInfo.err = jpeg_std_error( &(poDS->sJErr) );

    jpeg_create_decompress( &(poDS->sDInfo) );

/* -------------------------------------------------------------------- */
/*	Read pre-image data after ensuring the file is rewound.         */
/* -------------------------------------------------------------------- */
    VSIRewind( poOpenInfo->fp );

    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

    jpeg_stdio_src( &(poDS->sDInfo), poDS->fpImage );
    jpeg_read_header( &(poDS->sDInfo), TRUE );

    if( poDS->sDInfo.data_precision != 8 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "GDAL JPEG Driver doesn't support files with precision of"
                  " other than 8 bits." );
        delete poDS;
        return NULL;
    }

    jpeg_start_decompress( &(poDS->sDInfo) );

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poDS->sDInfo.image_width;
    poDS->nRasterYSize = poDS->sDInfo.image_height;

    if( poDS->sDInfo.jpeg_color_space == JCS_GRAYSCALE )
    {
        poDS->nBands = 1;
        poDS->sDInfo.out_color_space = JCS_GRAYSCALE;
    }
    else if( poDS->sDInfo.jpeg_color_space == JCS_RGB 
             || poDS->sDInfo.jpeg_color_space == JCS_YCbCr )
    {
        poDS->nBands = 3;
        poDS->sDInfo.out_color_space = JCS_RGB;
    }
    else
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Unrecognised jpeg_color_space value of %d.\n", 
                  poDS->sDInfo.jpeg_color_space );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
        poDS->SetBand( iBand+1, new JPGRasterBand( poDS, iBand+1 ) );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    poDS->bGeoTransformValid = 
        GDALReadWorldFile( poOpenInfo->pszFilename, ".jgw", 
                           poDS->adfGeoTransform )
        || GDALReadWorldFile( poOpenInfo->pszFilename, ".jpgw", 
                              poDS->adfGeoTransform )
        || GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                              poDS->adfGeoTransform );

    return poDS;
}

/************************************************************************/
/*                           JPEGCreateCopy()                           */
/************************************************************************/

static GDALDataset *
JPEGCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                int bStrict, char ** papszOptions, 
                GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();
    int  nQuality = 75;
    int  bProgressive = FALSE;

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "JPEG driver doesn't support %d bands.  Must be 1 (grey) "
                  "or 3 (RGB) bands.\n", nBands );

        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "JPEG driver doesn't support data type %s. "
                  "Only eight bit byte bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      What options has the user selected?                             */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszOptions,"QUALITY") != NULL )
    {
        nQuality = atoi(CSLFetchNameValue(papszOptions,"QUALITY"));
        if( nQuality < 10 || nQuality > 100 )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "QUALITY=%s is not a legal value in the range 10-100.",
                      CSLFetchNameValue(papszOptions,"QUALITY") );
            return NULL;
        }
    }

    bProgressive = CSLFetchBoolean( papszOptions, "PROGRESSIVE", FALSE );

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    FILE	*fpImage;

    fpImage = VSIFOpen( pszFilename, "wb" );
    if( fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create jpeg file %s.\n", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize JPG access to the file.                              */
/* -------------------------------------------------------------------- */
    struct jpeg_compress_struct sCInfo;
    struct jpeg_error_mgr sJErr;
    
    sCInfo.err = jpeg_std_error( &sJErr );
    jpeg_create_compress( &sCInfo );
    
    jpeg_stdio_dest( &sCInfo, fpImage );
    
    sCInfo.image_width = nXSize;
    sCInfo.image_height = nYSize;
    sCInfo.input_components = nBands;

    if( nBands == 1 )
    {
        sCInfo.in_color_space = JCS_GRAYSCALE;
    }
    else
    {
        sCInfo.in_color_space = JCS_RGB;
    }

    jpeg_set_defaults( &sCInfo );
    
    jpeg_set_quality( &sCInfo, nQuality, TRUE );

    if( bProgressive )
        jpeg_simple_progression( &sCInfo );

    jpeg_start_compress( &sCInfo, TRUE );

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GByte 	*pabyScanline;
    CPLErr      eErr;

    pabyScanline = (GByte *) CPLMalloc( nBands * nXSize );

    for( int iLine = 0; iLine < nYSize; iLine++ )
    {
        JSAMPLE      *ppSamples;

        for( int iBand = 0; iBand < nBands; iBand++ )
        {
            GDALRasterBand * poBand = poSrcDS->GetRasterBand( iBand+1 );
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                     pabyScanline + iBand, nXSize, 1, GDT_Byte,
                                     nBands, nBands * nXSize );
        }

        ppSamples = (JSAMPLE *) pabyScanline;
        jpeg_write_scanlines( &sCInfo, &ppSamples, 1 );
    }

    CPLFree( pabyScanline );

    jpeg_finish_compress( &sCInfo );
    jpeg_destroy_compress( &sCInfo );

    VSIFClose( fpImage );

/* -------------------------------------------------------------------- */
/*      Do we need a world file?                                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "WORLDFILE", FALSE ) )
    {
    	double      adfGeoTransform[6];
	
	poSrcDS->GetGeoTransform( adfGeoTransform );
	GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

    return (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
}

/************************************************************************/
/*                         GDALRegister_JPEG()                          */
/************************************************************************/

void GDALRegister_JPEG()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "JPEG" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "JPEG" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "JPEG JFIF" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_jpeg.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jpg" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jpeg" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>\n"
"   <Option name='PROGRESSIVE' type='boolean'/>\n"
"   <Option name='QUALITY' type='int' description='good=100, bad=0, default=75'/>\n"
"   <Option name='WORLDFILE' type='boolean'/>\n"
"</CreationOptionList>\n" );

        poDriver->pfnOpen = JPGDataset::Open;
        poDriver->pfnCreateCopy = JPEGCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

