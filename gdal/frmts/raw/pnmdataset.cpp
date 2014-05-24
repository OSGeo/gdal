/******************************************************************************
 * $Id$
 *
 * Project:  PNM Driver
 * Purpose:  Portable anymap file format imlementation
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "rawdataset.h"
#include "cpl_string.h"
#include <ctype.h>

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_PNM(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                              PNMDataset                              */
/* ==================================================================== */
/************************************************************************/

class PNMDataset : public RawDataset
{
    VSILFILE        *fpImage;       // image data file.

    int         bGeoTransformValid;
    double      adfGeoTransform[6];

  public:
                PNMDataset();
                ~PNMDataset();

    virtual CPLErr GetGeoTransform( double * );

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/*                            PNMDataset()                             */
/************************************************************************/

PNMDataset::PNMDataset()
{
    fpImage = NULL;
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~PNMDataset()                            */
/************************************************************************/

PNMDataset::~PNMDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PNMDataset::GetGeoTransform( double * padfTransform )

{

    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }
    else
        return CE_Failure;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int PNMDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Verify that this is a _raw_ ppm or pgm file.  Note, we don't    */
/*      support ascii files, or pbm (1bit) files.                       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 10 )
        return FALSE;

    if( poOpenInfo->pabyHeader[0] != 'P'  ||
        (poOpenInfo->pabyHeader[2] != ' '  &&    // XXX: Magick number
         poOpenInfo->pabyHeader[2] != '\t' &&    // may be followed
         poOpenInfo->pabyHeader[2] != '\n' &&    // any of the blank
         poOpenInfo->pabyHeader[2] != '\r') )    // characters
        return FALSE;

    if( poOpenInfo->pabyHeader[1] != '5'
        && poOpenInfo->pabyHeader[1] != '6' )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PNMDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Verify that this is a _raw_ ppm or pgm file.  Note, we don't    */
/*      support ascii files, or pbm (1bit) files.                       */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Parse out the tokens from the header.                           */
/* -------------------------------------------------------------------- */
    const char  *pszSrc = (const char *) poOpenInfo->pabyHeader;
    char        szToken[512];
    int         iIn, iToken = 0, nWidth =-1, nHeight=-1, nMaxValue=-1;
    unsigned int iOut;

    iIn = 2;
    while( iIn < poOpenInfo->nHeaderBytes && iToken < 3 )
    {
        iOut = 0;
        szToken[0] = '\0';
        while( iOut < sizeof(szToken) && iIn < poOpenInfo->nHeaderBytes )
        {
            if( pszSrc[iIn] == '#' )
            {
                while( pszSrc[iIn] != 10 && pszSrc[iIn] != 13
                       && iIn < poOpenInfo->nHeaderBytes - 1 )
                    iIn++;
            }

            if( iOut != 0 && isspace((unsigned char)pszSrc[iIn]) )
            {
                szToken[iOut] = '\0';

                if( iToken == 0 )
                    nWidth = atoi(szToken);
                else if( iToken == 1 )
                    nHeight = atoi(szToken);
                else if( iToken == 2 )
                    nMaxValue = atoi(szToken);

                iToken++;
                iIn++;
                break;
            }

            else if( !isspace((unsigned char)pszSrc[iIn]) )
            {
                szToken[iOut++] = pszSrc[iIn];
            }

            iIn++;
        }
    }

    CPLDebug( "PNM", "PNM header contains: width=%d, height=%d, maxval=%d",
              nWidth, nHeight, nMaxValue );

    if( iToken != 3 || nWidth < 1 || nHeight < 1 || nMaxValue < 1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    PNMDataset  *poDS;

    poDS = new PNMDataset();

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;

/* -------------------------------------------------------------------- */
/*      Open file                                                       */
/* -------------------------------------------------------------------- */

    if( poOpenInfo->eAccess == GA_Update )
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );
    else
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to re-open %s within PNM driver.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int         bMSBFirst = TRUE, iPixelSize;
    GDALDataType eDataType;

#ifdef CPL_LSB
    bMSBFirst = FALSE;
#endif

    if ( nMaxValue < 256 )
        eDataType = GDT_Byte;
    else
        eDataType = GDT_UInt16;

    iPixelSize = GDALGetDataTypeSize( eDataType ) / 8;

    if( poOpenInfo->pabyHeader[1] == '5' )
    {
        if (nWidth > INT_MAX / iPixelSize)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                  "Int overflow occured.");
            delete poDS;
            return NULL;
        }
        poDS->SetBand(
            1, new RawRasterBand( poDS, 1, poDS->fpImage, iIn, iPixelSize,
                                  nWidth*iPixelSize, eDataType, bMSBFirst, TRUE ));
        poDS->GetRasterBand(1)->SetColorInterpretation( GCI_GrayIndex );
    }
    else
    {
        if (nWidth > INT_MAX / (3 * iPixelSize))
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                  "Int overflow occured.");
            delete poDS;
            return NULL;
        }
        poDS->SetBand(
            1, new RawRasterBand( poDS, 1, poDS->fpImage, iIn, 3*iPixelSize,
                                  nWidth*3*iPixelSize, eDataType, bMSBFirst, TRUE ));
        poDS->SetBand(
            2, new RawRasterBand( poDS, 2, poDS->fpImage, iIn+iPixelSize,
                                  3*iPixelSize, nWidth*3*iPixelSize,
                                  eDataType, bMSBFirst, TRUE ));
        poDS->SetBand(
            3, new RawRasterBand( poDS, 3, poDS->fpImage, iIn+2*iPixelSize,
                                  3*iPixelSize, nWidth*3*iPixelSize,
                                  eDataType, bMSBFirst, TRUE ));

        poDS->GetRasterBand(1)->SetColorInterpretation( GCI_RedBand );
        poDS->GetRasterBand(2)->SetColorInterpretation( GCI_GreenBand );
        poDS->GetRasterBand(3)->SetColorInterpretation( GCI_BlueBand );
    }

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    poDS->bGeoTransformValid = 
        GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                           poDS->adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *PNMDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte && eType != GDT_UInt16 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create PNM dataset with an illegal\n"
              "data type (%s), only Byte and UInt16 supported.\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

    if( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create PNM dataset with an illegal number\n"
                  "of bands (%d).  Must be 1 (greyscale) or 3 (RGB).\n",
                  nBands );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE        *fp;

    fp = VSIFOpenL( pszFilename, "wb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write out the header.                                           */
/* -------------------------------------------------------------------- */
    char        szHeader[500];
    const char  *pszMaxValue = NULL;
    int         nMaxValue = 0;

    pszMaxValue = CSLFetchNameValue( papszOptions, "MAXVAL" );
    if ( pszMaxValue )
    {
        nMaxValue = atoi( pszMaxValue );
        if ( eType == GDT_Byte && (nMaxValue > 255 || nMaxValue < 0) )
            nMaxValue = 255;
        else if ( nMaxValue > 65535 || nMaxValue < 0 )
            nMaxValue = 65535;
    }
    else
    {
        if ( eType == GDT_Byte )
            nMaxValue = 255;
        else
            nMaxValue = 65535;
    }


    memset( szHeader, 0, sizeof(szHeader) );

    if( nBands == 3 )
        sprintf( szHeader, "P6\n%d %d\n%d\n", nXSize, nYSize, nMaxValue );
    else
        sprintf( szHeader, "P5\n%d %d\n%d\n", nXSize, nYSize, nMaxValue );

    VSIFWriteL( (void *) szHeader, strlen(szHeader) + 2, 1, fp );
    VSIFCloseL( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_PNM()                          */
/************************************************************************/

void GDALRegister_PNM()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "PNM" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "PNM" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Portable Pixmap Format (netpbm)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#PNM" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "pnm" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE,
                                   "image/x-portable-anymap" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte UInt16" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='MAXVAL' type='unsigned int' description='Maximum color value'/>"
"</CreationOptionList>" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = PNMDataset::Open;
        poDriver->pfnCreate = PNMDataset::Create;
        poDriver->pfnIdentify = PNMDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
