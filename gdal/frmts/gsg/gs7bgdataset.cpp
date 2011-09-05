/****************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Implements the Golden Software Surfer 7 Binary Grid Format.
 * Author:   Adam Guernsey, adam@ctech.com
 *           (Based almost entirely on gsbgdataset.cpp by Kevin Locke)
 *
 ****************************************************************************
 * Copyright (c) 2007, Adam Guernsey <adam@ctech.com>
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

#include <float.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "gdal_pam.h"

#ifndef DBL_MAX
# ifdef __DBL_MAX__
#  define DBL_MAX __DBL_MAX__
# else
#  define DBL_MAX 1.7976931348623157E+308
# endif /* __DBL_MAX__ */
#endif /* DBL_MAX */

#ifndef FLT_MAX
# ifdef __FLT_MAX__
#  define FLT_MAX __FLT_MAX__
# else
#  define FLT_MAX 3.40282347E+38F
# endif /* __FLT_MAX__ */
#endif /* FLT_MAX */

#ifndef INT_MAX
# define INT_MAX 2147483647
#endif /* INT_MAX */

#ifndef SHRT_MAX
# define SHRT_MAX 32767
#endif /* SHRT_MAX */

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_GS7BG(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				GS7BGDataset				*/
/* ==================================================================== */
/************************************************************************/

class GS7BGRasterBand;

class GS7BGDataset : public GDALPamDataset
{
    friend class GS7BGRasterBand;

    static double dfNoData_Value;
    static size_t nData_Position;

    VSILFILE	*fp;

  public:
    ~GS7BGDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr GetGeoTransform( double *padfGeoTransform );
};

/* NOTE:  This is not mentioned in the spec, but Surfer 8 uses this value */
/* 0x7effffee (Little Endian: eeffff7e) */
double GS7BGDataset::dfNoData_Value = 1.701410009187828e+38f;

size_t GS7BGDataset::nData_Position = 0;

const long  nHEADER_TAG = 0x42525344;
const long  nGRID_TAG = 0x44495247;
const long  nDATA_TAG = 0x41544144;
const long  nFAULT_TAG = 0x49544c46;

/************************************************************************/
/* ==================================================================== */
/*                            GS7BGRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class GS7BGRasterBand : public GDALPamRasterBand
{
    friend class GS7BGDataset;

    double dfMinX;
    double dfMaxX;
    double dfMinY;
    double dfMaxY;
    double dfMinZ;
    double dfMaxZ;

  public:

    GS7BGRasterBand( GS7BGDataset *, int );

    CPLErr IReadBlock( int, int, void * );

    double GetNoDataValue( int *pbSuccess = NULL );
};

/************************************************************************/
/*                           GS7BGRasterBand()                          */
/************************************************************************/

GS7BGRasterBand::GS7BGRasterBand( GS7BGDataset *poDS, int nBand )
{
    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = GDT_Float64;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GS7BGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
        void * pImage )

{
    if( nBlockYOff < 0 || nBlockYOff > nRasterYSize - 1 || nBlockXOff != 0 )
        return CE_Failure;

    GS7BGDataset *poGDS = (GS7BGDataset *) ( poDS );

    if( VSIFSeekL( poGDS->fp,
        ( GS7BGDataset::nData_Position +
            sizeof(double) * nRasterXSize * (nRasterYSize - nBlockYOff - 1) ),
        SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
        "Unable to seek to beginning of grid row.\n" );
        return CE_Failure;
    }

    if( VSIFReadL( pImage, sizeof(double), nBlockXSize,
        poGDS->fp ) != static_cast<unsigned>(nBlockXSize) )
    {
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read block from grid file.\n" );
        return CE_Failure;
    }

    double *pfImage;
    pfImage = (double *)pImage;
    for( int iPixel=0; iPixel<nBlockXSize; iPixel++ )
        CPL_LSBPTR64( pfImage + iPixel );

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GS7BGRasterBand::GetNoDataValue( int * pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;

    return GS7BGDataset::dfNoData_Value;
}

/************************************************************************/
/* ==================================================================== */
/*				GS7BGDataset				*/
/* ==================================================================== */
/************************************************************************/

GS7BGDataset::~GS7BGDataset()

{
    FlushCache();
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GS7BGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    /* Check for signature */
    if( poOpenInfo->nHeaderBytes < 4
        || !EQUALN((const char *) poOpenInfo->pabyHeader,"DSRB",4) )
    {
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The GS7BG driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    /* ------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                            */
    /* ------------------------------------------------------------------- */
    GS7BGDataset	*poDS = new GS7BGDataset();

    /* ------------------------------------------------------------------- */
    /*      Open file with large file API.                                 */
    /* ------------------------------------------------------------------- */
    poDS->eAccess = poOpenInfo->eAccess;
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "r+b" );

    if( poDS->fp == NULL )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed,
            "VSIFOpenL(%s) failed unexpectedly.",
            poOpenInfo->pszFilename );
        return NULL;
    }

    /* ------------------------------------------------------------------- */
    /*      Read the header. The Header section must be the first section  */
    /*      in the file.                                                   */
    /* ------------------------------------------------------------------- */
    if( VSIFSeekL( poDS->fp, 0, SEEK_SET ) != 0 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
                "Unable to seek to start of grid file header.\n" );
        return NULL;
    }

    GInt32 nTag;
    GInt32 nSize;
    GInt32 nVersion;

    if( VSIFReadL( (void *)&nTag, sizeof(GInt32), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO, "Unable to read Tag.\n" );
        return NULL;
    }

    CPL_LSBPTR32( &nTag );

    if(nTag != nHEADER_TAG)
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO, "Header tag not found.\n" );
        return NULL;
    }

    if( VSIFReadL( (void *)&nSize, sizeof(GInt32), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read file section size.\n" );
        return NULL;
    }

    CPL_LSBPTR32( &nSize );

    if( VSIFReadL( (void *)&nVersion, sizeof(GInt32), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read file version.\n" );
        return NULL;
    }

    CPL_LSBPTR32( &nVersion );

    if(nVersion != 1 && nVersion != 2)
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Incorrect file version (%d).", nVersion );
        return NULL;
    }

    // advance until the grid tag is found
    while(nTag != nGRID_TAG)
    {
        if( VSIFReadL( (void *)&nTag, sizeof(GInt32), 1, poDS->fp ) != 1 )
        {
            delete poDS;
            CPLError( CE_Failure, CPLE_FileIO, "Unable to read Tag.\n" );
            return NULL;
        }

        CPL_LSBPTR32( &nTag );

        if( VSIFReadL( (void *)&nSize, sizeof(GInt32), 1, poDS->fp ) != 1 )
        {
            delete poDS;
            CPLError( CE_Failure, CPLE_FileIO,
                "Unable to read file section size.\n" );
            return NULL;
        }

        CPL_LSBPTR32( &nSize );

        if(nTag != nGRID_TAG)
        {
            if( VSIFSeekL( poDS->fp, nSize, SEEK_SET ) != 0 )
            {
                delete poDS;
                CPLError( CE_Failure, CPLE_FileIO,
                    "Unable to seek to end of file section.\n" );
                return NULL;
            }
        }
    }

    /* --------------------------------------------------------------------*/
    /*      Read the grid.                                                 */
    /* --------------------------------------------------------------------*/
    /* Parse number of Y axis grid rows */
    GInt32 nRows;
    if( VSIFReadL( (void *)&nRows, sizeof(GInt32), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read raster Y size.\n" );
        return NULL;
    }
    CPL_LSBPTR32( &nRows );
    poDS->nRasterYSize = nRows;

    /* Parse number of X axis grid columns */
    GInt32 nCols;
    if( VSIFReadL( (void *)&nCols, sizeof(GInt32), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read raster X size.\n" );
        return NULL;
    }
    CPL_LSBPTR32( &nCols );
    poDS->nRasterXSize = nCols;

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

    /* --------------------------------------------------------------------*/
    /*      Create band information objects.                               */
    /* --------------------------------------------------------------------*/
    GS7BGRasterBand *poBand = new GS7BGRasterBand( poDS, 1 );

    // find the min X Value of the grid
    double dfTemp;
    if( VSIFReadL( (void *)&dfTemp, sizeof(double), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read minimum X value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMinX = dfTemp;

    // find the min Y value of the grid
    if( VSIFReadL( (void *)&dfTemp, sizeof(double), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read minimum X value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMinY = dfTemp;

    // find the spacing between adjacent nodes in the X direction
    // (between columns)
    if( VSIFReadL( (void *)&dfTemp, sizeof(double), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read spacing in X value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMaxX = poBand->dfMinX + (dfTemp * (nCols - 1));

    // find the spacing between adjacent nodes in the Y direction
    // (between rows)
    if( VSIFReadL( (void *)&dfTemp, sizeof(double), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read spacing in Y value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMaxY = poBand->dfMinY + (dfTemp * (nRows - 1));

    // set the z min
    if( VSIFReadL( (void *)&dfTemp, sizeof(double), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read Z min value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMinZ = dfTemp;

    // set the z max
    if( VSIFReadL( (void *)&dfTemp, sizeof(double), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read Z max value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poBand->dfMaxZ = dfTemp;
    poDS->SetBand( 1, poBand );

    // read and ignore the rotation value
    //(This is not used in the current version).
    if( VSIFReadL( (void *)&dfTemp, sizeof(double), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to read rotation value.\n" );
        return NULL;
    }

    // read and set the cell blank value
    if( VSIFReadL( (void *)&dfTemp, sizeof(double), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to Blank value.\n" );
        return NULL;
    }
    CPL_LSBPTR64( &dfTemp );
    poDS->dfNoData_Value = dfTemp;

    /* --------------------------------------------------------------------*/
    /*      Set the current offset of the grid data.                       */
    /* --------------------------------------------------------------------*/
    if( VSIFReadL( (void *)&nTag, sizeof(GInt32), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO, "Unable to read Tag.\n" );
        return NULL;
    }

    CPL_LSBPTR32( &nTag );
    if(nTag != nDATA_TAG)
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO, "Data tag not found.\n" );
        return NULL;
    }

    if( VSIFReadL( (void *)&nSize, sizeof(GInt32), 1, poDS->fp ) != 1 )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_FileIO,
            "Unable to data section size.\n" );
        return NULL;
    }

    poDS->nData_Position =  (size_t) VSIFTellL(poDS->fp);

    /* --------------------------------------------------------------------*/
    /*      Initialize any PAM information.                                */
    /* --------------------------------------------------------------------*/
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->papszSiblingFiles );

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GS7BGDataset::GetGeoTransform( double *padfGeoTransform )
{
    if( padfGeoTransform == NULL )
        return CE_Failure;

    GS7BGRasterBand *poGRB = (GS7BGRasterBand *)GetRasterBand( 1 );

    if( poGRB == NULL )
    {
        padfGeoTransform[0] = 0;
        padfGeoTransform[1] = 1;
        padfGeoTransform[2] = 0;
        padfGeoTransform[3] = 0;
        padfGeoTransform[4] = 0;
        padfGeoTransform[5] = 1;
        return CE_Failure;
    }

    /* check if we have a PAM GeoTransform stored */
    CPLPushErrorHandler( CPLQuietErrorHandler );
    CPLErr eErr = GDALPamDataset::GetGeoTransform( padfGeoTransform );
    CPLPopErrorHandler();

    if( eErr == CE_None )
        return CE_None;

    /* calculate pixel size first */
    padfGeoTransform[1] = (poGRB->dfMaxX - poGRB->dfMinX)/(nRasterXSize - 1);
    padfGeoTransform[5] = (poGRB->dfMinY - poGRB->dfMaxY)/(nRasterYSize - 1);

    /* then calculate image origin */
    padfGeoTransform[0] = poGRB->dfMinX - padfGeoTransform[1] / 2;
    padfGeoTransform[3] = poGRB->dfMaxY - padfGeoTransform[5] / 2;

    /* tilt/rotation does not supported by the GS grids */
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[2] = 0.0;

    return CE_None;
}

/************************************************************************/
/*                          GDALRegister_GS7BG()                        */
/************************************************************************/
void GDALRegister_GS7BG()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "GS7BG" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "GS7BG" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Golden Software 7 Binary Grid (.grd)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#GS7BG" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "grd" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
            "Byte Int16 UInt16 Float32 Float64" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = GS7BGDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

