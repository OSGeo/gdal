/******************************************************************************
 * $Id: landataset.cpp 17117 2009-05-25 19:26:01Z warmerdam $
 *
 * Project:  Vertical Datum Transformation
 * Purpose:  Implementation of NOAA .gtx vertical datum shift file format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam
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
#include "ogr_srs_api.h"

CPL_CVSID("$Id: landataset.cpp 17117 2009-05-25 19:26:01Z warmerdam $");

/**

NOAA .GTX Vertical Datum Grid Shift Format

All values are bigendian

Header
------

float64  latitude_of_origin
float64  longitude_of_origin (0-360)
float64  cell size (x?y?)
float64  cell size (x?y?)
int32    length in pixels
int32    width in pixels

Data
----

float32  * width in pixels * length in pixels 

Values are an offset in meters between two vertical datums.

**/

/************************************************************************/
/* ==================================================================== */
/*				GTXDataset				*/
/* ==================================================================== */
/************************************************************************/

class GTXDataset : public RawDataset
{
  public:
    FILE	*fpImage;	// image data file.
    
    double      adfGeoTransform[6];

  public:
    		GTXDataset();
    	        ~GTXDataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*				GTXDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             GTXDataset()                             */
/************************************************************************/

GTXDataset::GTXDataset()
{
    fpImage = NULL;
}

/************************************************************************/
/*                            ~GTXDataset()                             */
/************************************************************************/

GTXDataset::~GTXDataset()

{
    FlushCache();

    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTXDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We assume the user is pointing to the header (.pcb) file.       */
/*      Does this appear to be a pcb file?                              */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 40 || poOpenInfo->fp == NULL )
        return NULL;

    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"gtx") )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTXDataset 	*poDS;

    poDS = new GTXDataset();

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );

    if( poDS->fpImage == NULL )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[4] = 0.0;

    VSIFReadL( poDS->adfGeoTransform+3, 8, 1, poDS->fpImage );
    VSIFReadL( poDS->adfGeoTransform+0, 8, 1, poDS->fpImage );
    VSIFReadL( poDS->adfGeoTransform+5, 8, 1, poDS->fpImage );
    VSIFReadL( poDS->adfGeoTransform+1, 8, 1, poDS->fpImage );

    VSIFReadL( &(poDS->nRasterYSize), 4, 1, poDS->fpImage );
    VSIFReadL( &(poDS->nRasterXSize), 4, 1, poDS->fpImage );

    CPL_MSBPTR32( &(poDS->nRasterYSize) );
    CPL_MSBPTR32( &(poDS->nRasterXSize) );

    CPL_MSBPTR64( poDS->adfGeoTransform + 0 );
    CPL_MSBPTR64( poDS->adfGeoTransform + 1 );
    CPL_MSBPTR64( poDS->adfGeoTransform + 3 );
    CPL_MSBPTR64( poDS->adfGeoTransform + 5 );

    poDS->adfGeoTransform[3] += 
        poDS->adfGeoTransform[5] * (poDS->nRasterYSize-1);

    poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[1] * 0.5;
    poDS->adfGeoTransform[3] += poDS->adfGeoTransform[5] * 0.5;

    poDS->adfGeoTransform[5] *= -1;

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 
        1, new RawRasterBand( poDS, 1, poDS->fpImage, 
                              (poDS->nRasterYSize-1)*poDS->nRasterXSize*4 + 40,
                              4, poDS->nRasterXSize * -4,
                              GDT_Float32,
                              !CPL_IS_LSB, TRUE, FALSE ) );

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
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTXDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/*                                                                      */
/*      Use PAM coordinate system if available in preference to the     */
/*      generally poor value derived from the file itself.              */
/************************************************************************/

const char *GTXDataset::GetProjectionRef()

{
    return SRS_WKT_WGS84;
}

/************************************************************************/
/*                          GDALRegister_GTX()                          */
/************************************************************************/

void GDALRegister_GTX()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "GTX" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GTX" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "NOAA Vertical Datum .GTX" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gtx" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
//        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
//                                   "frmt_various.html#GTX" );
        
        poDriver->pfnOpen = GTXDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

